#include "part_manager.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>

namespace clp {

PartManager::PartManager() {
    ResetToDefaultSong();
}

void PartManager::SetActiveIndex(int index) {
    if (index >= 0 && index < static_cast<int>(m_parts.size())) {
        m_activePartIndex = index;
    }
}

Part* PartManager::GetActivePart() {
    if (m_parts.empty()) return nullptr;
    if (m_activePartIndex < 0 || m_activePartIndex >= static_cast<int>(m_parts.size())) {
        m_activePartIndex = 0;
    }
    return &m_parts[m_activePartIndex];
}

const Part* PartManager::GetActivePart() const {
    if (m_parts.empty()) return nullptr;
    if (m_activePartIndex < 0 || m_activePartIndex >= static_cast<int>(m_parts.size())) {
        return &m_parts[0];
    }
    return &m_parts[m_activePartIndex];
}

Part& PartManager::AddPart(std::string_view name, std::string_view key, MusicalMode mode) {
    Part p;
    p.id = "part_" + std::to_string(m_nextPartId++);
    p.name = std::string(name);
    p.keyRoot = std::string(key);
    p.mode = mode;
    p.bpm = 120;
    p.timeNumerator = 4;
    p.timeDenominator = 4;
    p.rawProgressionText = "| C | G | Am | F |";
    p.bars = ProgressionParser::Parse(p.rawProgressionText, p.keyRoot, p.mode, p.timeNumerator, p.timeDenominator);

    m_parts.push_back(std::move(p));
    m_activePartIndex = static_cast<int>(m_parts.size()) - 1;
    return m_parts.back();
}

bool PartManager::RemovePart(int index) {
    if (m_parts.size() <= 1) return false; // 最低1パートは保持
    if (index < 0 || index >= static_cast<int>(m_parts.size())) return false;

    m_parts.erase(m_parts.begin() + index);
    if (m_activePartIndex >= static_cast<int>(m_parts.size())) {
        m_activePartIndex = static_cast<int>(m_parts.size()) - 1;
    }
    return true;
}

bool PartManager::DuplicatePart(int index) {
    if (index < 0 || index >= static_cast<int>(m_parts.size())) return false;

    Part copy = m_parts[index];
    copy.id = "part_" + std::to_string(m_nextPartId++);
    copy.name += " (Copy)";
    m_parts.insert(m_parts.begin() + index + 1, std::move(copy));
    m_activePartIndex = index + 1;
    return true;
}

void PartManager::MovePart(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<int>(m_parts.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<int>(m_parts.size())) return;
    if (fromIndex == toIndex) return;

    Part p = std::move(m_parts[fromIndex]);
    m_parts.erase(m_parts.begin() + fromIndex);
    m_parts.insert(m_parts.begin() + toIndex, std::move(p));
    m_activePartIndex = toIndex;
}

std::vector<Bar> PartManager::GenerateMergedProgression() const {
    std::vector<Bar> merged;
    int currentBarNum = 1;

    std::string lastKey = "";
    MusicalMode lastMode = MusicalMode::Major;
    bool isFirstPart = true;

    for (const auto& part : m_parts) {
        if (part.bars.empty()) continue;

        for (size_t i = 0; i < part.bars.size(); ++i) {
            Bar b = part.bars[i];
            b.barNumber = currentBarNum++;

            if (i == 0) {
                b.sectionMarker = part.name;

                if (isFirstPart || part.keyRoot != lastKey || part.mode != lastMode) {
                    b.keyChange = part.keyRoot + ((part.mode == MusicalMode::NaturalMinor) ? " Minor" : " Major");
                    lastKey = part.keyRoot;
                    lastMode = part.mode;
                    isFirstPart = false;
                }
            }

            PitchClass pKey = MusicEngine::NoteToPitch(part.keyRoot);
            for (auto& chord : b.chords) {
                if (!chord.isRest && chord.rootPitch != PitchClass::Unknown) {
                    chord.degreeRoman = MusicEngine::CalculateDegree(
                        chord.rootPitch, chord.quality, pKey, part.mode, chord.bassPitch
                    );
                }
            }

            merged.push_back(std::move(b));
        }
    }

    return merged;
}

void PartManager::ResetToDefaultSong() {
    m_parts.clear();
    m_nextPartId = 5;

    // Intro (Key: C)
    Part intro;
    intro.id = "intro";
    intro.name = "Intro";
    intro.keyRoot = "C";
    intro.mode = MusicalMode::Major;
    intro.bpm = 120;
    intro.rawProgressionText = "| C | G | Am | F |";
    intro.bars = ProgressionParser::Parse(intro.rawProgressionText, intro.keyRoot, intro.mode);
    m_parts.push_back(std::move(intro));

    // Aメロ (Verse) (Key: C)
    Part verse;
    verse.id = "verse";
    verse.name = "Verse";
    verse.keyRoot = "C";
    verse.mode = MusicalMode::Major;
    verse.bpm = 120;
    verse.rawProgressionText = "| C | Em | F | G |";
    verse.bars = ProgressionParser::Parse(verse.rawProgressionText, verse.keyRoot, verse.mode);
    m_parts.push_back(std::move(verse));

    // Bメロ (Pre-Chorus) (Key: C)
    Part pre;
    pre.id = "pre_chorus";
    pre.name = "Pre-Chorus";
    pre.keyRoot = "C";
    pre.mode = MusicalMode::Major;
    pre.bpm = 120;
    pre.rawProgressionText = "| Dm7 | G7 | Em7 | Am7 |";
    pre.bars = ProgressionParser::Parse(pre.rawProgressionText, pre.keyRoot, pre.mode);
    m_parts.push_back(std::move(pre));

    // サビ (Chorus) (Key: Eb - 転調例)
    Part chorus;
    chorus.id = "chorus";
    chorus.name = "Chorus";
    chorus.keyRoot = "Eb";
    chorus.mode = MusicalMode::Major;
    chorus.bpm = 120;
    chorus.rawProgressionText = "| Ab | Bb | Gm7 | Cm7 |";
    chorus.bars = ProgressionParser::Parse(chorus.rawProgressionText, chorus.keyRoot, chorus.mode);
    m_parts.push_back(std::move(chorus));

    m_activePartIndex = 0;
}

static std::string EscapeJsonString(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string PartManager::SerializeProject(
    std::string_view projectTitle,
    VoicingStyle voicing,
    int bassMode,
    int gridTicks
) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"ChordLaunchProject\",\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"title\": \"" << EscapeJsonString(projectTitle) << "\",\n";
    ss << "  \"activePartIndex\": " << m_activePartIndex << ",\n";
    ss << "  \"voicing\": " << static_cast<int>(voicing) << ",\n";
    ss << "  \"bassMode\": " << bassMode << ",\n";
    ss << "  \"gridTicks\": " << gridTicks << ",\n";
    ss << "  \"parts\": [\n";

    for (size_t pi = 0; pi < m_parts.size(); ++pi) {
        const auto& part = m_parts[pi];
        ss << "    {\n";
        ss << "      \"id\": \"" << EscapeJsonString(part.id) << "\",\n";
        ss << "      \"name\": \"" << EscapeJsonString(part.name) << "\",\n";
        ss << "      \"keyRoot\": \"" << EscapeJsonString(part.keyRoot) << "\",\n";
        ss << "      \"mode\": " << static_cast<int>(part.mode) << ",\n";
        ss << "      \"bpm\": " << part.bpm << ",\n";
        ss << "      \"timeNumerator\": " << part.timeNumerator << ",\n";
        ss << "      \"timeDenominator\": " << part.timeDenominator << ",\n";
        ss << "      \"rawProgressionText\": \"" << EscapeJsonString(part.rawProgressionText) << "\",\n";
        ss << "      \"bars\": [\n";

        for (size_t bi = 0; bi < part.bars.size(); ++bi) {
            const auto& bar = part.bars[bi];
            ss << "        {\n";
            ss << "          \"barNumber\": " << bar.barNumber << ",\n";
            ss << "          \"chords\": [\n";
            for (size_t ci = 0; ci < bar.chords.size(); ++ci) {
                const auto& c = bar.chords[ci];
                ss << "            {\n";
                ss << "              \"rawText\": \"" << EscapeJsonString(c.rawText) << "\",\n";
                ss << "              \"rootName\": \"" << EscapeJsonString(c.rootName) << "\",\n";
                ss << "              \"durationTicks\": " << c.durationTicks << ",\n";
                ss << "              \"durationBeats\": " << c.durationBeats << ",\n";
                ss << "              \"isRest\": " << (c.isRest ? "true" : "false") << ",\n";
                ss << "              \"degreeRoman\": \"" << EscapeJsonString(c.degreeRoman) << "\"\n";
                ss << "            }" << (ci + 1 < bar.chords.size() ? "," : "") << "\n";
            }
            ss << "          ]\n";
            ss << "        }" << (bi + 1 < part.bars.size() ? "," : "") << "\n";
        }

        ss << "      ]\n";
        ss << "    }" << (pi + 1 < m_parts.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

static std::string FindJsonString(const std::string& src, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\":";
    size_t pos = src.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) ++pos;
    if (pos >= src.size() || src[pos] != '"') return "";
    ++pos;
    std::string result;
    while (pos < src.size()) {
        if (src[pos] == '\\' && pos + 1 < src.size()) {
            char next = src[pos + 1];
            if (next == '"') result += '"';
            else if (next == '\\') result += '\\';
            else if (next == 'n') result += '\n';
            else if (next == 'r') result += '\r';
            else if (next == 't') result += '\t';
            else result += next;
            pos += 2;
        } else if (src[pos] == '"') {
            break;
        } else {
            result += src[pos++];
        }
    }
    return result;
}

static int FindJsonInt(const std::string& src, std::string_view key, int defaultVal = 0) {
    std::string needle = "\"" + std::string(key) + "\":";
    size_t pos = src.find(needle);
    if (pos == std::string::npos) return defaultVal;
    pos += needle.size();
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) ++pos;
    size_t endPos = pos;
    if (endPos < src.size() && (src[endPos] == '-' || src[endPos] == '+')) ++endPos;
    while (endPos < src.size() && std::isdigit(static_cast<unsigned char>(src[endPos]))) ++endPos;
    if (endPos > pos) {
        try {
            return std::stoi(src.substr(pos, endPos - pos));
        } catch (...) {}
    }
    return defaultVal;
}

bool PartManager::DeserializeProject(
    const std::string& data,
    std::string& outTitle,
    VoicingStyle& outVoicing,
    int& outBassMode,
    int& outGridTicks
) {
    if (data.empty()) return false;
    if (data.find("ChordLaunchProject") == std::string::npos && data.find("ChordLaunchpadProject") == std::string::npos && data.find("\"parts\"") == std::string::npos) {
        return false;
    }

    outTitle = FindJsonString(data, "title");
    if (outTitle.empty()) outTitle = "MyProgression";

    outVoicing = static_cast<VoicingStyle>(FindJsonInt(data, "voicing", 0));
    outBassMode = FindJsonInt(data, "bassMode", 0);
    outGridTicks = FindJsonInt(data, "gridTicks", TICKS_PER_BEAT);
    int activeIdx = FindJsonInt(data, "activePartIndex", 0);

    size_t partsPos = data.find("\"parts\":");
    if (partsPos == std::string::npos) return false;
    partsPos = data.find('[', partsPos);
    if (partsPos == std::string::npos) return false;

    std::vector<Part> newParts;
    size_t curPos = partsPos + 1;

    while (curPos < data.size()) {
        size_t objStart = data.find('{', curPos);
        if (objStart == std::string::npos) break;

        int braceDepth = 1;
        size_t objEnd = objStart + 1;
        bool inQuotes = false;
        while (objEnd < data.size() && braceDepth > 0) {
            char c = data[objEnd];
            if (inQuotes && c == '\\') {
                objEnd += 2;
                continue;
            }
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (!inQuotes) {
                if (c == '{') ++braceDepth;
                else if (c == '}') --braceDepth;
            }
            ++objEnd;
        }
        if (braceDepth != 0) break;

        std::string partBlock = data.substr(objStart, objEnd - objStart);

        Part p;
        p.id = FindJsonString(partBlock, "id");
        if (p.id.empty()) p.id = "part_" + std::to_string(newParts.size() + 1);
        p.name = FindJsonString(partBlock, "name");
        if (p.name.empty()) p.name = "Part " + std::to_string(newParts.size() + 1);
        p.keyRoot = FindJsonString(partBlock, "keyRoot");
        if (p.keyRoot.empty()) p.keyRoot = "C";
        p.mode = static_cast<MusicalMode>(FindJsonInt(partBlock, "mode", 0));
        p.bpm = std::clamp(FindJsonInt(partBlock, "bpm", 120), 40, 300);
        p.timeNumerator = std::clamp(FindJsonInt(partBlock, "timeNumerator", 4), 1, 16);
        p.timeDenominator = std::clamp(FindJsonInt(partBlock, "timeDenominator", 4), 1, 16);
        p.rawProgressionText = FindJsonString(partBlock, "rawProgressionText");

        if (!p.rawProgressionText.empty()) {
            p.bars = ProgressionParser::Parse(p.rawProgressionText, p.keyRoot, p.mode, p.timeNumerator, p.timeDenominator);
        } else {
            p.rawProgressionText = "| C | G | Am | F |";
            p.bars = ProgressionParser::Parse(p.rawProgressionText, p.keyRoot, p.mode, p.timeNumerator, p.timeDenominator);
        }

        newParts.push_back(std::move(p));
        curPos = objEnd;
        size_t nextComma = data.find(',', curPos);
        size_t nextBracket = data.find(']', curPos);
        if (nextBracket < nextComma) break;
    }

    if (newParts.empty()) return false;

    m_parts = std::move(newParts);
    m_activePartIndex = std::clamp(activeIdx, 0, static_cast<int>(m_parts.size()) - 1);
    m_nextPartId = static_cast<int>(m_parts.size()) + 1;
    return true;
}

bool PartManager::SaveToFile(
    const std::wstring& filePath,
    std::string_view projectTitle,
    VoicingStyle voicing,
    int bassMode,
    int gridTicks
) const {
    if (filePath.empty()) return false;
    std::string json = SerializeProject(projectTitle, voicing, bassMode, gridTicks);

    try {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs.write(json.data(), json.size());
        ofs.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool PartManager::LoadFromFile(
    const std::wstring& filePath,
    std::string& outTitle,
    VoicingStyle& outVoicing,
    int& outBassMode,
    int& outGridTicks
) {
    if (filePath.empty()) return false;
    try {
        std::filesystem::path p(filePath);
        if (!std::filesystem::exists(p)) return false;

        std::ifstream ifs(p, std::ios::binary);
        if (!ifs.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        return DeserializeProject(content, outTitle, outVoicing, outBassMode, outGridTicks);
    } catch (...) {
        return false;
    }
}

bool PartManager::PerformAutoBackup(
    const std::wstring& currentProjectFilePath,
    const std::string& projectTitle,
    const std::string& projectData
) {
    if (projectData.empty()) return false;

    try {
        std::filesystem::path backupDir;
        if (!currentProjectFilePath.empty()) {
            std::filesystem::path projPath(currentProjectFilePath);
            backupDir = projPath.parent_path() / L"Project Backup";
        } else {
            wchar_t appData[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
                backupDir = std::filesystem::path(appData) / L"ChordLaunch" / L"AutoBackup";
            } else {
                return false;
            }
        }

        std::filesystem::create_directories(backupDir);

        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow;
        localtime_s(&tmNow, &nowTime);

        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tmNow);

        std::string safeTitle = projectTitle.empty() ? "Untitled" : projectTitle;
        for (char& c : safeTitle) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }

        std::string fileName = "Backup_" + safeTitle + "_" + std::string(timeBuf) + ".chord";
        std::filesystem::path backupFile = backupDir / fileName;

        std::ofstream ofs(backupFile, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs.write(projectData.data(), projectData.size());
        ofs.close();

        std::vector<std::filesystem::path> existingBackups;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".chord") {
                existingBackups.push_back(entry.path());
            }
        }

        if (existingBackups.size() > 5) {
            std::sort(existingBackups.begin(), existingBackups.end(), [](const auto& a, const auto& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });
            size_t toDelete = existingBackups.size() - 5;
            for (size_t i = 0; i < toDelete; ++i) {
                std::error_code ec;
                std::filesystem::remove(existingBackups[i], ec);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace clp
