#include "scenario_filter.h"

#include <algorithm>
#include <cctype>

namespace olc {

namespace {

// The line without spaces, tabs and \r at the ends.
std::string trimmed(const std::string& line) {
    size_t begin = 0;
    size_t end = line.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(line[begin]))) {
        begin++;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
        end--;
    }
    return line.substr(begin, end - begin);
}

// Orbiter compares keywords without regard to case (_stricmp).
bool sameWord(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

// For "KEY value" returns true if the first word is key, and puts the rest into value.
bool keyValue(const std::string& line, const char* key, std::string& value) {
    const std::string text = trimmed(line);
    const size_t space = text.find_first_of(" \t");
    if (space == std::string::npos || !sameWord(text.substr(0, space), key)) {
        return false;
    }
    value = trimmed(text.substr(space));
    return true;
}

// Splits the text into lines; each line keeps its '\n' (the last one may have none).
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        end = end == std::string::npos ? text.size() : end + 1;
        lines.push_back(text.substr(start, end - start));
        start = end;
    }
    return lines;
}

// The line with the value after the key replaced, the indent and line ending kept.
std::string replaceValue(const std::string& line, const std::string& oldValue,
                         const std::string& newValue) {
    const size_t at = line.rfind(oldValue);
    if (at == std::string::npos) {
        return line;
    }
    return line.substr(0, at) + newValue + line.substr(at + oldValue.size());
}

}  // namespace

ScenarioFilterResult filterScenario(const std::string& text,
                                    const std::function<bool(const std::string&)>& classInstalled) {
    ScenarioFilterResult result;
    const std::vector<std::string> lines = splitLines(text);
    std::vector<std::string> kept;
    std::string firstKeptVessel;

    // Pass 1: vessels.
    bool inShips = false;
    bool skipping = false;  // inside the block of a removed vessel
    bool inBlock = false;   // inside the block of any vessel
    for (const std::string& line : lines) {
        const std::string word = trimmed(line);
        if (!inShips) {
            inShips = sameWord(word, "BEGIN_SHIPS");
            kept.push_back(line);
            continue;
        }
        if (inBlock) {
            if (sameWord(word, "END")) {
                inBlock = false;
            }
            if (!skipping) {
                kept.push_back(line);
            }
            if (!inBlock) {
                skipping = false;
            }
            continue;
        }
        if (sameWord(word, "END_SHIPS")) {
            inShips = false;
            kept.push_back(line);
            continue;
        }
        if (word.empty()) {
            kept.push_back(line);
            continue;
        }
        // A vessel header "name:class" or "name".
        const size_t colon = word.find(':');
        const std::string name = word.substr(0, colon);
        const std::string className = colon == std::string::npos ? name : word.substr(colon + 1);
        inBlock = true;
        if (classInstalled(className)) {
            kept.push_back(line);
            if (firstKeptVessel.empty()) {
                firstKeptVessel = name;
            }
        } else {
            skipping = true;
            result.removedVessels.push_back(name);
            if (std::find(result.missingClasses.begin(), result.missingClasses.end(), className) ==
                result.missingClasses.end()) {
                result.missingClasses.push_back(className);
            }
        }
    }
    result.hasVessels = !firstKeptVessel.empty();

    // Pass 2: focus and camera target must not point to a removed vessel.
    auto removed = [&](const std::string& name) {
        return std::find(result.removedVessels.begin(), result.removedVessels.end(), name) !=
               result.removedVessels.end();
    };
    std::string section;
    for (const std::string& line : kept) {
        const std::string word = trimmed(line);
        std::string value;
        if (word.rfind("BEGIN_", 0) == 0) {
            section = word;
        } else if (word.rfind("END_", 0) == 0) {
            section.clear();
        } else if (result.hasVessels && ((section == "BEGIN_FOCUS" && keyValue(line, "Ship", value)) ||
                                         (section == "BEGIN_CAMERA" && keyValue(line, "TARGET", value))) &&
                   removed(value)) {
            result.text += replaceValue(line, value, firstKeptVessel);
            continue;
        }
        result.text += line;
    }
    return result;
}

}  // namespace olc
