//
// Created by andreas on 11.01.26.
//
#include "compile_db.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>



static std::string requireString(const nlohmann::json& json, const char* key) {
    if (!json.contains(key) || !json[key].is_string()) {
        throw std::runtime_error(std::string("compile_commands.json: missing or non-string key: ") + key);
    }
    return json[key].get<std::string>();
}

std::vector<CompileCommand> loadCompileCommands(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open: " + path);
    }

    nlohmann::json root;

    try {
        in >> root;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Invalid JSON in " + path + ": " + std::string(e.what()));
    }

    if (!root.is_array()) {
        throw std::runtime_error("compile_commands.json: top-level JSON is not an array");
    }

    std::vector<CompileCommand> output;
    output.reserve(root.size());

    for (size_t idx = 0; idx < root.size(); ++idx) {
        const auto& item = root[idx];
        if (!item.is_object()) {
            throw std::runtime_error("Invalid compile_commands.json: entry " + std::to_string(idx) +
                                     " is not an object (" + path + ")");
        }

        if (!item.contains("directory") || !item["directory"].is_string()) {
            throw std::runtime_error("Invalid compile_commands.json: entry " + std::to_string(idx) +
                                     " missing string 'directory' (" + path + ")");
        }

        if (!item.contains("file") || !item["file"].is_string()) {
            throw std::runtime_error("Invalid compile_commands.json: entry " + std::to_string(idx) +
                                     " missing string 'file' (" + path + ")");
        }
        const bool hasCommand = item.contains("command") && item["command"].is_string();
        const bool hasArgs = item.contains("arguments") && item["arguments"].is_array();

        if (!hasCommand && !hasArgs) {
            throw std::runtime_error("Invalid compile_commands.json: entry " + std::to_string(idx) +
                                     " must contain either string 'command' or array 'arguments' (" + path + ")");
        }

        CompileCommand compileCommand;

        compileCommand.directory = item["directory"].get<std::string>();
        compileCommand.file      = item["file"].get<std::string>();

        if (hasCommand) {
            compileCommand.command = item["command"].get<std::string>();
        }

        if (hasArgs) {
            for (const auto& a : item["arguments"]) {
                if (!a.is_string()) {
                    throw std::runtime_error("Invalid compile_commands.json: entry " + std::to_string(idx) +
                                             " has non-string in 'arguments' (" + path + ")");
                }
                compileCommand.arguments.push_back(a.get<std::string>());
            }
        }
        if (item.contains("output") && item["output"].is_string()) {
            compileCommand.output = item["output"].get<std::string>();
        }

        output.push_back(std::move(compileCommand));
    }

    return output;
}
