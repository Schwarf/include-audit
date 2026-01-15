#include "compile_db.h"
#include "compile_command_prober.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

static void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --compdb <path> [--limit N] [--probe] [--print-probe-cmd]\n";
}

static std::string getArgValue(int& i, int argc, char** argv, const char* opt) {
    if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value after ") + opt);
    return argv[++i];
}

int main(int argc, char** argv) {
    try {
        std::string compdbPath;
        int limit = 10;
        bool doProbe = false;
        bool printProbeCmd = false;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--compdb") {
                compdbPath = getArgValue(i, argc, argv, "--compdb");
            } else if (a == "--limit") {
                limit = std::stoi(getArgValue(i, argc, argv, "--limit"));
                if (limit < 0) limit = 0;
            } else if (a == "--probe") {
                doProbe = true;
            } else if (a == "--print-probe-cmd") {
                printProbeCmd = true;
            } else if (a == "-h" || a == "--help") {
                usage(argv[0]);
                return 0;
            } else {
                std::cerr << "Unknown option: " << a << "\n";
                usage(argv[0]);
                return 2;
            }
        }

        if (compdbPath.empty()) {
            usage(argv[0]);
            return 2;
        }

        auto commands = loadCompileCommands(compdbPath);

        std::cout << "Compilation database entries: " << commands.size() << "\n";
        const int showN = std::min<int>(limit, (int)commands.size());
        std::cout << "Showing first " << showN << ":\n";

        for (int i = 0; i < showN; ++i) {
            const auto& cc = commands[i];
            std::cout << "\n[" << i << "]\n";
            std::cout << "  file:      " << cc.file << "\n";
            std::cout << "  directory: " << cc.directory << "\n";

            if (!cc.arguments.empty()) {
                std::cout << "  compiler:  " << cc.arguments.front() << "\n";
            } else if (cc.command.has_value()) {
                const std::string& cmd = *cc.command;
                auto pos = cmd.find_first_of(" \t");
                std::cout << "  compiler:  " << cmd.substr(0, pos) << "\n";
            } else {
                std::cout << "  compiler:  (unknown)\n";
            }

            if (doProbe) {
                CompileCommandProber::Options opt;
                opt.printCommand = printProbeCmd;
                opt.dropWerror = true;
                opt.keepXclang = true;

                CompileCommandProber prober(opt);
                auto r = prober.probeOne(cc);
                if (r.exitCode == 0) {
                    std::cout << "  probe:     OK\n";
                } else {
                    std::cout << "  probe:     FAIL (exit " << r.exitCode << ")\n";
                    // Optional: stop early on failure
                    // break;
                }
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
