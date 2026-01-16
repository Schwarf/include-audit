//
// Created by andreas on 15.01.26.
//#include "include_auditor.h"
#include "include_auditor.h"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>     // getpid
#include <sys/stat.h>   // mkdir
#include <algorithm>

IncludeAuditor::IncludeAuditor(const CompileCommandProber& prober, Options opt)
    : prober_(prober), opt_(opt) {}

static std::string trim_copy(const std::string& s) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    size_t b = 0;
    while (b < s.size() && is_space(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && is_space(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> IncludeAuditor::readAllLines(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open source file: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

void IncludeAuditor::writeAllLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to write temp file: " + path);
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i] << "\n";
    }
}

bool IncludeAuditor::isIncludeLine(const std::string& line, std::string* trimmedOut) {
    std::string t = trim_copy(line);

    // Ignore commented-out lines quickly
    if (t.rfind("//", 0) == 0) return false;

    // Very small MVP parser:
    // Accept: #include "x" or # include <x> etc.
    if (t.rfind("#include", 0) == 0) {
        if (trimmedOut) *trimmedOut = t;
        return true;
    }
    if (t.rfind("#", 0) == 0) {
        // allow "# include"
        // collapse "#   include" pattern:
        if (t.size() >= 2 && t[0] == '#') {
            // find next non-space
            size_t i = 1;
            while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i;
            if (t.compare(i, 7, "include") == 0) {
                if (trimmedOut) *trimmedOut = t;
                return true;
            }
        }
    }
    return false;
}

bool IncludeAuditor::isSystemInclude(const std::string& trimmedLine) {
    // naive: if it contains '<' after include
    return trimmedLine.find("<") != std::string::npos && trimmedLine.find(">") != std::string::npos;
}

std::string IncludeAuditor::makeTempPath(const std::string& originalPath, size_t counter) {
    // Write temp file in /tmp. Keep basename-ish for readability.
    // (No need for mkstemp; we include pid+counter.)
    const auto pid = static_cast<long>(::getpid());

    // crude basename:
    std::string base = originalPath;
    auto slash = base.find_last_of('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);

    std::ostringstream oss;
    oss << "/tmp/include-audit_" << pid << "_" << counter << "_" << base;
    return oss.str();
}

std::vector<std::string> IncludeAuditor::makeModifiedLinesRemoveOne(
    const std::vector<std::string>& lines,
    size_t lineIndex,
    bool keepEmptyLines
) {
    std::vector<std::string> modified = lines;
    if (lineIndex >= modified.size()) return modified;

    if (keepEmptyLines) {
        // Keep line count stable (helps diagnostics)
        modified[lineIndex] = "";
    } else {
        modified.erase(modified.begin() + static_cast<long>(lineIndex));
    }
    return modified;
}

std::vector<std::string> IncludeAuditor::replaceSourcePathInArgv(
    const std::vector<std::string>& argv,
    const std::string& originalPath,
    const std::string& newPath
) {
    std::vector<std::string> out = argv;

    // Replace exact match only.
    // (Your prober ensures the source path appears as its own argv token.)
    for (auto& a : out) {
        if (a == originalPath) {
            a = newPath;
            break;
        }
    }
    return out;
}

AuditResult IncludeAuditor::auditOneTU(const CompileCommand& cc) const {
    AuditResult result;
    result.file = cc.file;
    result.directory = cc.directory;

    // Baseline probe (should be OK; otherwise include auditing makes no sense)
    auto baseProbeArgv = prober_.buildProbeArgv(cc);
    if (baseProbeArgv.empty()) {
        result.baselineExitCode = 127;
        return result;
    }

    const int baseline = prober_.runArgvInDirectory(baseProbeArgv, cc.directory);
    result.baselineExitCode = baseline;

    if (baseline != 0) {
        // If baseline fails, we can’t evaluate “remove include” reliably.
        return result;
    }

    // Read file and collect direct includes
    auto lines = readAllLines(cc.file);

    struct IncLine {
        size_t idx;
        std::string trimmed;
    };
    std::vector<IncLine> includes;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmed;
        if (!isIncludeLine(lines[i], &trimmed)) continue;

        if (opt_.onlyQuotedIncludes && isSystemInclude(trimmed)) continue;

        includes.push_back({i, trimmed});
    }

    result.findings.reserve(includes.size());

    size_t tempCounter = 0;
    for (const auto& inc : includes) {
        IncludeFinding f;
        f.includeLine = inc.trimmed;
        f.lineIndex = inc.idx;

        if (opt_.verbose) {
            std::cerr << "[audit] " << cc.file << ":" << (inc.idx + 1)
                      << " try remove: " << inc.trimmed << "\n";
        }

        // Create modified temp source
        const auto tempPath = makeTempPath(cc.file, tempCounter++);
        auto modifiedLines = makeModifiedLinesRemoveOne(lines, inc.idx, opt_.keepEmptyLines);
        writeAllLines(tempPath, modifiedLines);

        // Build argv with replaced source file
        auto probeArgv = replaceSourcePathInArgv(baseProbeArgv, cc.file, tempPath);

        const int code = prober_.runArgvInDirectory(probeArgv, cc.directory);
        f.exitCodeAfterRemoval = code;
        f.unused = (code == 0);

        result.findings.push_back(std::move(f));

        // Cleanup temp file (best effort)
        (void)::unlink(tempPath.c_str());
    }

    return result;
}
