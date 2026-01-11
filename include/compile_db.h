//
// Created by andreas on 11.01.26.
//

#ifndef INCLUDE_AUDIT_COMPILD_DB_H
#define INCLUDE_AUDIT_COMPILD_DB_H

#include <optional>
#include <string>
#include <vector>

struct CompileCommand {
    std::string directory;
    std::string file;

    // compile_commands.json can contain either "command" (string) or "arguments" (array)
    // see: https://clang.llvm.org/docs/JSONCompilationDatabase.html?utm_source=chatgpt.com
    std::optional<std::string> command;
    std::vector<std::string> arguments;

    std::optional<std::string> output;
};

std::vector<CompileCommand> loadCompileCommands(const std::string& path);
#endif //INCLUDE_AUDIT_COMPILD_DB_H