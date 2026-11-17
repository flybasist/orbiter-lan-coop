// olc-doccheck: checks that the English and Russian documentation stay in sync (decision 29).
//
// Pairs: README.md and README.ru.md, CHANGELOG.md and CHANGELOG.ru.md, docs/<name>.md and
// docs/ru/<name>.md. Every document must have its pair, and the two files must have the same
// structure: the same sequence of headings (level and number, e.g. "## 3."), tables (number of
// rows) and code blocks (number of lines). The text itself is not compared - only the skeleton,
// so that a change made in one language and forgotten in the other is caught.
//
// Usage: olc-doccheck <repository root>
// Exit code 0 - all pairs match, 1 - something differs (details are printed), 2 - usage error.

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// One element of a document skeleton and the line where it starts.
struct Element {
    std::string kind;  // e.g. "heading ## 3.", "table 5 rows", "code 12 lines"
    int line = 0;
};

// Whether the line opens or closes a code block.
bool isFence(const std::string& line) {
    size_t start = line.find_first_not_of(' ');
    return start != std::string::npos && line.compare(start, 3, "```") == 0;
}

// Heading level and number: "## 3. Text" → "## 3.", "### Text" → "###".
std::string headingKey(const std::string& line) {
    size_t level = line.find_first_not_of('#');
    if (level == std::string::npos || level == 0 || line[level] != ' ') {
        return "";
    }
    std::string key = line.substr(0, level);
    // A number right after the hashes ("3.", "8.1.") is part of the structure.
    size_t pos = level + 1;
    size_t end = pos;
    while (end < line.size() && (std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '.')) {
        end++;
    }
    if (end > pos) {
        key += " " + line.substr(pos, end - pos);
    }
    return key;
}

// Reads a file and builds its skeleton. false - the file cannot be read.
bool readSkeleton(const fs::path& path, std::vector<Element>& skeleton) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::string line;
    int number = 0;
    bool inCode = false;
    int codeStart = 0;
    int codeLines = 0;
    int tableStart = 0;
    int tableRows = 0;

    auto closeTable = [&]() {
        if (tableRows > 0) {
            skeleton.push_back({"table " + std::to_string(tableRows) + " rows", tableStart});
            tableRows = 0;
        }
    };

    while (std::getline(file, line)) {
        number++;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (inCode) {
            if (isFence(line)) {
                skeleton.push_back({"code " + std::to_string(codeLines) + " lines", codeStart});
                inCode = false;
            } else {
                codeLines++;
            }
            continue;
        }
        if (isFence(line)) {
            closeTable();
            inCode = true;
            codeStart = number;
            codeLines = 0;
            continue;
        }
        if (!line.empty() && line[0] == '|') {
            if (tableRows == 0) {
                tableStart = number;
            }
            tableRows++;
            continue;
        }
        closeTable();
        std::string key = headingKey(line);
        if (!key.empty()) {
            skeleton.push_back({"heading " + key, number});
        }
    }
    closeTable();
    if (inCode) {
        skeleton.push_back({"code block not closed", codeStart});
    }
    return true;
}

// Compares the skeletons of a pair. Prints the first difference. true - they match.
bool comparePair(const fs::path& root, const fs::path& english, const fs::path& russian) {
    std::vector<Element> en;
    std::vector<Element> ru;
    if (!readSkeleton(root / english, en) || !readSkeleton(root / russian, ru)) {
        std::printf("FAIL %s / %s: cannot read\n", english.generic_string().c_str(),
                    russian.generic_string().c_str());
        return false;
    }
    size_t count = en.size() < ru.size() ? en.size() : ru.size();
    for (size_t i = 0; i < count; i++) {
        if (en[i].kind != ru[i].kind) {
            std::printf("FAIL %s:%d '%s' vs %s:%d '%s'\n", english.generic_string().c_str(),
                        en[i].line, en[i].kind.c_str(), russian.generic_string().c_str(),
                        ru[i].line, ru[i].kind.c_str());
            return false;
        }
    }
    if (en.size() != ru.size()) {
        const bool englishLonger = en.size() > ru.size();
        const Element& extra = englishLonger ? en[count] : ru[count];
        const fs::path& file = englishLonger ? english : russian;
        std::printf("FAIL %s:%d '%s' has no counterpart in the other language\n",
                    file.generic_string().c_str(), extra.line, extra.kind.c_str());
        return false;
    }
    return true;
}

// Names of the .md files in a folder (not recursive).
std::set<std::string> markdownFiles(const fs::path& folder) {
    std::set<std::string> names;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            names.insert(entry.path().filename().string());
        }
    }
    return names;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::printf("usage: olc-doccheck <repository root>\n");
        return 2;
    }
    const fs::path root = argv[1];
    if (!fs::exists(root / "README.md") || !fs::is_directory(root / "docs")) {
        std::printf("olc-doccheck: %s does not look like the repository root\n", argv[1]);
        return 2;
    }

    std::vector<std::pair<fs::path, fs::path>> pairs = {
        {"README.md", "README.ru.md"},
        {"CHANGELOG.md", "CHANGELOG.ru.md"},
    };
    bool ok = true;

    const std::set<std::string> english = markdownFiles(root / "docs");
    const std::set<std::string> russian = markdownFiles(root / "docs" / "ru");
    for (const std::string& name : english) {
        if (russian.count(name) == 0) {
            std::printf("FAIL docs/%s has no Russian version docs/ru/%s\n", name.c_str(),
                        name.c_str());
            ok = false;
        } else {
            pairs.push_back({fs::path("docs") / name, fs::path("docs") / "ru" / name});
        }
    }
    for (const std::string& name : russian) {
        if (english.count(name) == 0) {
            std::printf("FAIL docs/ru/%s has no English version docs/%s\n", name.c_str(),
                        name.c_str());
            ok = false;
        }
    }

    for (const auto& pair : pairs) {
        if (!comparePair(root, pair.first, pair.second)) {
            ok = false;
        }
    }
    std::printf("olc-doccheck: %zu pairs checked, %s\n", pairs.size(), ok ? "OK" : "differences found");
    return ok ? 0 : 1;
}
