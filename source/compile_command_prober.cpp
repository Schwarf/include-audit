//
// Created by andreas on 12.01.26.
//
#include "compile_command_prober.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <optional>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

CompileCommandProber::CompileCommandProber(Options opt) : options(opt)
{
}


// Minimal shell-ish splitter for compile_commands "command" string.
// Handles spaces, single/double quotes, and backslash escapes.
std::vector<std::string> CompileCommandProber::splitCommand(std::string_view command)
{
    std::vector<std::string> output;
    std::string current;
    enum class Mode { Normal, SingleQuote, DoubleQuote } mode = Mode::Normal;

    auto push = [&]()
    {
        if (!current.empty())
        {
            output.push_back(current);
            current.clear();
        }
    };

    for (size_t i = 0; i < command.size(); ++i)
    {
        const char character = command[i];

        if (mode == Mode::Normal)
        {
            if (std::isspace(static_cast<unsigned char>(character)))
            {
                push();
                continue;
            }
            if (character == '\'')
            {
                mode = Mode::SingleQuote;
                continue;
            }
            if (character == '"')
            {
                mode = Mode::DoubleQuote;
                continue;
            }
            if (character == '\\')
            {
                if (i + 1 < command.size())
                    current.push_back(command[++i]);
                continue;
            }
            current.push_back(character);
        }
        else if (mode == Mode::SingleQuote)
        {
            if (character == '\'')
            {
                mode = Mode::Normal;
                continue;
            }
            current.push_back(character);
        }
        else
        {
            // DoubleQ
            if (character == '"')
            {
                mode = Mode::Normal;
                continue;
            }
            if (character == '\\')
            {
                if (i + 1 < command.size())
                    current.push_back(command[++i]);
                continue;
            }
            current.push_back(character);
        }
    }
    push();
    return output;
}

std::vector<std::string> CompileCommandProber::buildBaseArgv(const CompileCommand& compileCommand) const
{
    if (!compileCommand.arguments.empty())
        return compileCommand.arguments;
    if (compileCommand.command.has_value())
        return splitCommand(compileCommand.command.value());

    // Should not happen given your loader checks, but keep it safe.
    return {};
}

std::vector<std::string> CompileCommandProber::makeProbeArgv(const CompileCommand& cc) const
{
    auto base = buildBaseArgv(cc);
    if (base.empty())
        return {};

    std::vector<std::string> ouput;
    ouput.reserve(base.size() + 4);

    // compiler binary
    ouput.push_back(base[0]);

    auto hasPrefix = [&](const std::string& s, const char* p)
    {
        return s.rfind(p, 0) == 0;
    };


    for (size_t i = 1; i < base.size(); ++i)
    {
        const std::string& a = base[i];

        // Drop compile-to-object / outputs / depfile generation.
        if (a == "-c")
            continue;

        // Drop dependency-generation switches that create files.
        if (a == "-MMD" || a == "-MD" || a == "-MM" || a == "-M" || a == "-MP")
            continue;

        // Drop -o <file> or -oFILE
        if (a == "-o")
        {
            if (i + 1 < base.size())
                ++i;
            continue;
        }
        if (hasPrefix(a, "-o") && a.size() > 2)
            continue;

        // Drop -MF <file> / -MFfile, -MT/-MQ variants
        if (a == "-MF" || a == "-MT" || a == "-MQ")
        {
            if (i + 1 < base.size()) ++i;
            continue;
        }
        if (hasPrefix(a, "-MF") && a.size() > 3)
            continue;
        if (hasPrefix(a, "-MT") && a.size() > 3)
            continue;
        if (hasPrefix(a, "-MQ") && a.size() > 3)
            continue;

        // Optionally drop -Werror (often breaks “probe” runs for harmless warnings)
        if (options.dropWerror)
        {
            if (a == "-Werror" || hasPrefix(a, "-Werror=")) continue;
        }

        // -Xclang <arg> appears in clang-based builds; dropping it can break correctness.
        if (a == "-Xclang")
        {
            if (options.keepXclang)
            {
                ouput.push_back(a);
                if (i + 1 < base.size()) ouput.push_back(base[++i]);
            }
            else
            {
                if (i + 1 < base.size()) ++i;
            }
            continue;
        }

        ouput.push_back(a);
    }

    // Make it a syntax-only probe (fast, no output files).
    ouput.push_back("-fsyntax-only");

    // Ensure the source file appears in argv (compdb typically includes it, but be safe).
    bool hasSource = false;
    for (const auto& a : ouput)
    {
        if (a == cc.file)
        {
            hasSource = true;
            break;
        }
    }
    if (!hasSource) ouput.push_back(cc.file);

    return ouput;
}


int CompileCommandProber::runInDirectory(const std::vector<std::string>& argv, const std::string& directory)
{
    if (argv.empty()) return 127;

    // Convert to char* array
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
    cargv.push_back(nullptr);

    // Save cwd
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
    {
        std::perror("getcwd");
        return 127;
    }

    // chdir to TU directory
    if (chdir(directory.c_str()) != 0)
    {
        std::cerr << "chdir failed: " << directory << " : " << std::strerror(errno) << "\n";
        return 127;
    }

    pid_t pid = -1;
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    // Spawn (PATH lookup)
    int rc = posix_spawnp(&pid, cargv[0], nullptr, &attr, cargv.data(), environ);

    posix_spawnattr_destroy(&attr);

    // Restore cwd even if spawn fails
    (void)chdir(cwd);

    if (rc != 0)
    {
        std::cerr << "posix_spawnp failed: " << std::strerror(rc) << "\n";
        return 127;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        std::perror("waitpid");
        return 127;
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
}

ProbeResult CompileCommandProber::probeOne(const CompileCommand& cc) const
{
    ProbeResult result;
    result.file = cc.file;
    result.directory = cc.directory;
    auto debugPrint = [](const std::vector<std::string>& argv)
    {
        for (const auto& a : argv)
            std::cerr << a << " ";
        std::cerr << "\n";

    };

    result.argv = makeProbeArgv(cc);
    if (result.argv.empty())
    {
        result.exitCode = 127;
        return result;
    }

    result.invokedBinary = result.argv[0];

    if (options.printCommand)
    {
        std::cerr << "[probe] ";
        debugPrint(result.argv);
    }

    result.exitCode = runInDirectory(result.argv, cc.directory);
    return result;
}

std::vector<ProbeResult> CompileCommandProber::probeAll(const std::vector<CompileCommand>& cmds) const
{
    std::vector<ProbeResult> results;
    results.reserve(cmds.size());

    for (const auto& cc : cmds)
    {
        auto r = probeOne(cc);
        results.push_back(std::move(r));
        if (options.stopOnFirstFailure && results.back().exitCode != 0) break;
    }
    return results;
}
