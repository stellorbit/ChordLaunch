#include "part_manager.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "timeline_edit.hpp"
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
    // 選択中より前のパートを削除した場合は、同じパートを選択し続けるよう番号を補正する
    if (index < m_activePartIndex) {
        --m_activePartIndex;
    }
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
    int gridTicks,
    int inputGridTicks
) const {
    if (inputGridTicks <= 0) inputGridTicks = gridTicks;
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"ChordLaunchProject\",\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"title\": \"" << EscapeJsonString(projectTitle) << "\",\n";
    ss << "  \"activePartIndex\": " << m_activePartIndex << ",\n";
    ss << "  \"voicing\": " << static_cast<int>(voicing) << ",\n";
    ss << "  \"bassMode\": " << bassMode << ",\n";
    ss << "  \"gridTicks\": " << gridTicks << ",\n";
    ss << "  \"inputGridTicks\": " << inputGridTicks << ",\n";
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
                std::string rawToSave = c.rawText;
                if (rawToSave.empty() && !c.isRest && !c.rootName.empty()) {
                    rawToSave = c.rootName + c.qualitySuffix;
                    if (!c.bassName.empty()) rawToSave += "/" + c.bassName;
                }
                ss << "            {\n";
                ss << "              \"rawText\": \"" << EscapeJsonString(rawToSave) << "\",\n";
                ss << "              \"rootName\": \"" << EscapeJsonString(c.rootName) << "\",\n";
                ss << "              \"quality\": " << static_cast<int>(c.quality) << ",\n";
                ss << "              \"qualitySuffix\": \"" << EscapeJsonString(c.qualitySuffix) << "\",\n";
                ss << "              \"durationTicks\": " << c.durationTicks << ",\n";
                ss << "              \"durationBeats\": " << c.durationBeats << ",\n";
                ss << "              \"isRest\": " << (c.isRest ? "true" : "false") << ",\n";
                ss << "              \"octaveOffset\": " << c.octaveOffset << ",\n";
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
            else if (next == 'b') result += '\b';
            else if (next == 'f') result += '\f';
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

// 指定位置の '[' から、同じ深さの要素オブジェクト（{...}）を列挙する
static std::vector<std::string> ExtractArrayObjects(const std::string& src, size_t arrayOpenPos) {
    std::vector<std::string> result;
    if (arrayOpenPos >= src.size() || src[arrayOpenPos] != '[') return result;

    size_t pos = arrayOpenPos + 1;
    int depth = 0;
    bool inQuotes = false;
    size_t objStart = std::string::npos;
    while (pos < src.size()) {
        char c = src[pos];
        if (inQuotes) {
            if (c == '\\') { pos += 2; continue; }
            if (c == '"') inQuotes = false;
            ++pos;
            continue;
        }
        if (c == '"') {
            inQuotes = true;
        } else if (c == '{') {
            if (depth == 0) objStart = pos;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && objStart != std::string::npos) {
                result.push_back(src.substr(objStart, pos - objStart + 1));
                objStart = std::string::npos;
            }
        } else if (c == '[') {
            ++depth;
        } else if (c == ']') {
            if (depth == 0) break;
            --depth;
        }
        ++pos;
    }
    return result;
}

static bool LooksLikeJsonTrue(const std::string& src, const char* key) {
    std::string needle = std::string("\"") + key + "\":";
    size_t pos = src.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) ++pos;
    return src.compare(pos, 4, "true") == 0;
}

// 保存された chords 配列の1要素から ChordToken を復元する。
// rawText を再パースして quality / rootPitch / テンション等を復元し、
// 音価は durationTicks（sub-beat シンコペーション）を優先する。
static ChordToken ParseChordJson(const std::string& obj, const Part& part) {
    std::string raw = FindJsonString(obj, "rawText");
    std::string root = FindJsonString(obj, "rootName");
    std::string suffix = FindJsonString(obj, "qualitySuffix");
    std::string degree = FindJsonString(obj, "degreeRoman");
    int quality = FindJsonInt(obj, "quality", -1);
    int ticks = FindJsonInt(obj, "durationTicks", 0);
    int beats = FindJsonInt(obj, "durationBeats", 1);
    int octaveOffset = FindJsonInt(obj, "octaveOffset", 0);
    bool isRest = LooksLikeJsonTrue(obj, "isRest");

    ChordToken tok;
    if (!isRest) {
        std::string symbol = raw;
        if (symbol.empty() || symbol == "_") {
            if (!suffix.empty() && root.size() >= suffix.size() && root.rfind(suffix) == (root.size() - suffix.size())) {
                symbol = root;
            } else {
                symbol = root + suffix;
            }
        }
        if (!symbol.empty()) {
            tok = MusicEngine::ParseChordSymbol(symbol, part.keyRoot, part.mode);
        }
        if (tok.rootPitch == PitchClass::Unknown && !root.empty()) {
            tok.rootPitch = MusicEngine::NoteToPitch(root);
            tok.rootName = root;
            if (quality >= 0) tok.quality = static_cast<ChordQuality>(quality);
            tok.qualitySuffix = suffix;
        }
        if (tok.rawText.empty()) tok.rawText = symbol;
        if (tok.rootName.empty()) tok.rootName = root;
        if (tok.qualitySuffix.empty()) tok.qualitySuffix = suffix;
        if (quality >= 0 && tok.quality == ChordQuality::Major && static_cast<ChordQuality>(quality) != ChordQuality::Major) {
            tok.quality = static_cast<ChordQuality>(quality);
        }
    } else {
        tok = ChordToken{};
        tok.isRest = true;
        tok.quality = ChordQuality::Rest;
        tok.rootName = "R";
        tok.degreeRoman = "R";
        tok.rawText = raw.empty() ? "_" : raw;
    }

    if (!degree.empty()) tok.degreeRoman = degree;
    if (!tok.isRest) tok.octaveOffset = std::clamp(octaveOffset, -3, 3);
    if (ticks <= 0) ticks = std::max(1, beats) * TICKS_PER_BEAT;
    tok.SetTicks(ticks);
    return tok;
}

// 保存された bars 配列を復元する。bars が無い（旧形式）場合は false。
static bool ParseBarsJson(const std::string& partBlock, const Part& part, std::vector<Bar>& outBars) {
    size_t barsKey = partBlock.find("\"bars\"");
    if (barsKey == std::string::npos) return false;
    size_t barsArr = partBlock.find('[', barsKey);
    if (barsArr == std::string::npos) return false;

    std::vector<Bar> bars;
    for (const auto& barObj : ExtractArrayObjects(partBlock, barsArr)) {
        size_t chordsKey = barObj.find("\"chords\"");
        if (chordsKey == std::string::npos) continue;
        size_t chordsArr = barObj.find('[', chordsKey);
        if (chordsArr == std::string::npos) continue;

        Bar bar;
        bar.timeNumerator = part.timeNumerator;
        bar.timeDenominator = part.timeDenominator;
        bar.barNumber = FindJsonInt(barObj, "barNumber", static_cast<int>(bars.size()) + 1);

        for (const auto& chordObj : ExtractArrayObjects(barObj, chordsArr)) {
            bar.chords.push_back(ParseChordJson(chordObj, part));
        }
        if (bar.chords.empty()) {
            ChordToken fullRest;
            fullRest.isRest = true;
            fullRest.quality = ChordQuality::Rest;
            fullRest.rootName = "R";
            fullRest.degreeRoman = "R";
            fullRest.rawText = "_";
            fullRest.SetTicks(bar.GetTotalTicks());
            bar.chords.push_back(fullRest);
        }
        TimelineEdit::NormalizeBar(bar, nullptr, true);
        bars.push_back(std::move(bar));
    }

    if (bars.empty()) return false;
    outBars = std::move(bars);
    return true;
}

bool PartManager::DeserializeProject(
    const std::string& data,
    std::string& outTitle,
    VoicingStyle& outVoicing,
    int& outBassMode,
    int& outGridTicks,
    int* outInputGridTicks
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
    if (outInputGridTicks) {
        *outInputGridTicks = FindJsonInt(data, "inputGridTicks", outGridTicks);
    }
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

        // bars 配列が保存されていれば、テキスト再パースより優先する。
        // テキストは拍単位に丸められるため、sub-beat のシンコペーションは bars にしか残らない。
        std::vector<Bar> savedBars;
        if (ParseBarsJson(partBlock, p, savedBars)) {
            p.bars = std::move(savedBars);
            if (p.rawProgressionText.empty()) {
                p.rawProgressionText = ProgressionParser::Format(p.bars);
            }
        }

        newParts.push_back(std::move(p));
        curPos = objEnd;
        size_t nextComma = data.find(',', curPos);
        size_t nextBracket = data.find(']', curPos);
        if (nextBracket < nextComma) break;
    }

    if (newParts.empty()) return false;

    // 既存・旧形式データの救済: 全小節の全コードで rootPitch / rawText / degreeRoman の健全性を保証する
    for (auto& part : newParts) {
        PitchClass pKey = MusicEngine::NoteToPitch(part.keyRoot);
        if (pKey == PitchClass::Unknown) pKey = PitchClass::C;
        for (auto& bar : part.bars) {
            for (auto& chord : bar.chords) {
                if (chord.isRest) continue;
                if (chord.rootPitch == PitchClass::Unknown) {
                    if (!chord.rootName.empty()) {
                        chord.rootPitch = MusicEngine::NoteToPitch(chord.rootName);
                    } else if (!chord.rawText.empty()) {
                        auto rep = MusicEngine::ParseChordSymbol(chord.rawText, part.keyRoot, part.mode);
                        chord.rootPitch = rep.rootPitch;
                        if (chord.rootName.empty()) chord.rootName = rep.rootName;
                        if (chord.qualitySuffix.empty()) chord.qualitySuffix = rep.qualitySuffix;
                    }
                }
                if (chord.rawText.empty()) {
                    chord.rawText = chord.rootName + chord.qualitySuffix;
                    if (!chord.bassName.empty()) chord.rawText += "/" + chord.bassName;
                }
                if (chord.degreeRoman.empty() && chord.rootPitch != PitchClass::Unknown) {
                    chord.degreeRoman = MusicEngine::CalculateDegree(
                        chord.rootPitch, chord.quality, pKey, part.mode, chord.bassPitch
                    );
                }
            }
        }
    }

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
    int gridTicks,
    int inputGridTicks
) const {
    if (filePath.empty()) return false;
    std::string json = SerializeProject(projectTitle, voicing, bassMode, gridTicks, inputGridTicks);

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
    int& outGridTicks,
    int* outInputGridTicks
) {
    if (filePath.empty()) return false;
    try {
        std::filesystem::path p(filePath);
        if (!std::filesystem::exists(p)) return false;

        std::ifstream ifs(p, std::ios::binary);
        if (!ifs.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        return DeserializeProject(content, outTitle, outVoicing, outBassMode, outGridTicks, outInputGridTicks);
    } catch (...) {
        return false;
    }
}

bool PartManager::PerformAutoBackup(
    const std::wstring& currentProjectFilePath,
    const std::string& projectTitle,
    const std::string& projectData,
    const std::wstring& customBackupDir
) {
    if (projectData.empty()) return false;

    try {
        std::filesystem::path backupDir;
        if (!customBackupDir.empty()) {
            backupDir = customBackupDir;
        } else if (!currentProjectFilePath.empty()) {
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

        // タイトルは UTF-8。std::string のまま path に渡すと ACP 解釈で文字化けするため UTF-16 へ変換する
        std::string safeTitle = projectTitle.empty() ? "Untitled" : projectTitle;
        std::wstring wTitle;
        {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, safeTitle.data(), static_cast<int>(safeTitle.size()), nullptr, 0);
            if (wlen > 0) {
                wTitle.resize(static_cast<size_t>(wlen));
                MultiByteToWideChar(CP_UTF8, 0, safeTitle.data(), static_cast<int>(safeTitle.size()), wTitle.data(), wlen);
            }
        }
        for (wchar_t& c : wTitle) {
            if (c < 0x20 || c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
                c = L'_';
            }
        }
        if (wTitle.size() > 100) wTitle.resize(100);
        while (!wTitle.empty() && (wTitle.back() == L'.' || wTitle.back() == L' ')) wTitle.pop_back();
        if (wTitle.empty()) wTitle = L"Untitled";

        std::wstring wTime(timeBuf, timeBuf + std::char_traits<char>::length(timeBuf));
        std::wstring fileName = L"Backup_" + wTitle + L"_" + wTime + L".chord";
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
