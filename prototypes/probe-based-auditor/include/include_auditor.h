//
// Created by andreas on 15.01.26.
//

#ifndef INCLUDE_AUDIT_INCLUDE_AUDITOR_H
#define INCLUDE_AUDIT_INCLUDE_AUDITOR_H

#include "compile_command_prober.h"
#include "compile_db.h"

#include <string>
#include <vector>

struct IncludeFinding {
    std::string includeLine;     // exact line text (trimmed)
    size_t lineIndex = 0;        // 0-based line index in file
    bool unused = false;
    int exitCodeAfterRemoval = 0;
};

struct AuditResult {
    std::string file;
    std::string directory;
    int baselineExitCode = 0;
    std::vector<IncludeFinding> findings;
};

class IncludeAuditor {
public:
    struct Options {
        bool onlyQuotedIncludes = false; // if true, ignore <...> system includes
        bool keepEmptyLines = true;      // replace removed include with blank line (preserve line numbers)
        bool verbose = false;            // print progress
    };

    IncludeAuditor(const CompileCommandProber& prober, Options opt);

    AuditResult auditOneTU(const CompileCommand& cc) const;

private:
    const CompileCommandProber& prober_;
    Options opt_;

    static std::vector<std::string> readAllLines(const std::string& path);
    static void writeAllLines(const std::string& path, const std::vector<std::string>& lines);

    static bool isIncludeLine(const std::string& line, std::string* trimmedOut);
    static bool isSystemInclude(const std::string& trimmedLine);  // #include <...>
    static std::string makeTempPath(const std::string& originalPath, size_t counter);

    static std::vector<std::string> makeModifiedLinesRemoveOne(
        const std::vector<std::string>& lines,
        size_t lineIndex,
        bool keepEmptyLines
    );

    static std::vector<std::string> replaceSourcePathInArgv(
        const std::vector<std::string>& argv,
        const std::string& originalPath,
        const std::string& newPath
    );
};

#endif //INCLUDE_AUDIT_INCLUDE_AUDITOR_H