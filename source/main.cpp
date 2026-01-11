#include "compile_db.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

static void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " --compdb <path> [--limit N]\n";
}

static std::string getArgValue(int& i, int argc, char** argv, const char* opt) {
    if (i + 1 >= argc) throw std::runtime_error(std::string("Missing value after ") + opt);
    return argv[++i];
}

int main(int argc, char** argv) {
    try {
        std::string compdbPath;
        int limit = 10;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--compdb") {
                compdbPath = getArgValue(i, argc, argv, "--compdb");
            } else if (a == "--limit") {
                limit = std::stoi(getArgValue(i, argc, argv, "--limit"));
                if (limit < 0) limit = 0;
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
        std::cout << "Showing first " << std::min<int>(limit, (int)commands.size()) << ":\n";

        for (int i = 0; i < limit && i < (int)commands.size(); ++i) {
            const auto& cc = commands[i];
            std::cout << "\n[" << i << "]\n";
            std::cout << "  file:      " << cc.file << "\n";
            std::cout << "  directory: " << cc.directory << "\n";

            // Print "compiler" in a simplistic way:
            // - if "arguments" exists, argv[0] is the compiler
            // - else if "command" exists, print the first token up to whitespace
            if (!cc.arguments.empty()) {
                std::cout << "  compiler:  " << cc.arguments.front() << "\n";
            } else if (cc.command.has_value()) {
                const std::string& cmd = *cc.command;
                auto pos = cmd.find_first_of(" \t");
                std::cout << "  compiler:  " << cmd.substr(0, pos) << "\n";
            } else {
                std::cout << "  compiler:  (unknown)\n";
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
