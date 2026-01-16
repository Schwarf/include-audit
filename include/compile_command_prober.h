//
// Created by andreas on 12.01.26.
//

#ifndef INCLUDE_AUDIT_COMPILE_COMMAND_PROBER_H
#define INCLUDE_AUDIT_COMPILE_COMMAND_PROBER_H

#include "compile_db.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct ProbeResult {
    std::string file;
    std::string directory;
    int exitCode = 0;
    std::string invokedBinary;
    std::vector<std::string> argv;
};

class CompileCommandProber {
public:
    struct Options {
        bool dropWerror = true;      // remove -Werror / -Werror=...
        bool keepXclang = true;      // keep -Xclang <arg> pairs
        bool printCommand = false;   // print probe command line
        bool stopOnFirstFailure = false;
    };

    explicit CompileCommandProber(Options opt);

    // Probe a single compile command. Returns result with exitCode.
    ProbeResult probeOne(const CompileCommand& cc) const;

    // Probe many compile commands. Returns vector of results.
    std::vector<ProbeResult> probeAll(const std::vector<CompileCommand>& cmds) const;

    // Build the probe argv for this TU (what probeOne would run)
    std::vector<std::string> buildProbeArgv(const CompileCommand& cc) const;

    // Run an argv in the TU directory (same behavior as probing)
    int runArgvInDirectory(const std::vector<std::string>& argv, const std::string& directory) const;

private:
    Options options;

    static std::vector<std::string> splitCommand(std::string_view command);

    std::vector<std::string> buildBaseArgv(const CompileCommand& compileCommand) const;
    std::vector<std::string> makeProbeArgv(const CompileCommand& cc) const;

    static int runInDirectory(const std::vector<std::string>& argv, const std::string& directory);
    static void debugPrint(const std::vector<std::string>& argv);
};

#endif //INCLUDE_AUDIT_COMPILE_COMMAND_PROBER_H