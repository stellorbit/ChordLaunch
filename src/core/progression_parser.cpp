#include "progression_parser.hpp"
#include "music_engine.hpp"
#include <sstream>
#include <regex>
#include <numeric>
#include <algorithm>

namespace clp {

bool ProgressionParser::IsRepeatToken(std::string_view token) {
    return token == "%" || token == "=" || token == "%%";
}

bool ProgressionParser::IsRestToken(std::string_view token) {
    return token == "R" || token == "r" || token == "N.C." || token == "NC" || token == "rest" ||
           token == "_" || token == "." || token == "__" || token == "___" || token == "____";
}

std::vector<std::string> ProgressionParser::NormalizeTokens(const std::vector<std::string>& tokens) {
    std::vector<std::string> normalized;

    for (const auto& raw : tokens) {
        if (raw.empty()) continue;

        // すべてハイフンのみの場合 (例: "-", "--", "---")
        bool allHyphens = true;
        for (char c : raw) {
            if (c != '-') { allHyphens = false; break; }
        }
        if (allHyphens) {
            for (size_t i = 0; i < raw.size(); ++i) {
                normalized.push_back("-");
            }
            continue;
        }

        // すべてアンダースコアのみの場合 (例: "_", "__", "___")
        bool allUnderscores = true;
        for (char c : raw) {
            if (c != '_') { allUnderscores = false; break; }
        }
        if (allUnderscores) {
            for (size_t i = 0; i < raw.size(); ++i) {
                normalized.push_back("_");
            }
            continue;
        }

        // 末尾の長音ハイフンを切り離す (例: "C---" -> "C", "-", "-", "-")
        // 注意: "m7-5" や "7-9" のように '-' の直後に数字が続く場合はコード修飾子
        size_t trailingHyphens = 0;
        size_t len = raw.size();
        while (len > 0 && raw[len - 1] == '-') {
            trailingHyphens++;
            len--;
        }

        std::string baseChord = raw.substr(0, len);
        if (!baseChord.empty()) {
            normalized.push_back(baseChord);
        }
        for (size_t i = 0; i < trailingHyphens; ++i) {
            normalized.push_back("-");
        }
    }

    return normalized;
}

std::vector<std::string> ProgressionParser::TokenizeBar(std::string_view barContent) {
    std::vector<std::string> rawTokens;
    std::string current;

    for (size_t i = 0; i < barContent.size(); ++i) {
        char c = barContent[i];
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
            if (!current.empty()) {
                rawTokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        rawTokens.push_back(current);
    }

    return NormalizeTokens(rawTokens);
}

std::vector<Bar> ProgressionParser::Parse(
    std::string_view text,
    std::string_view defaultKey,
    MusicalMode defaultMode,
    int defaultNumerator,
    int defaultDenominator
) {
    std::vector<Bar> bars;
    std::string currentKey = std::string(defaultKey);
    MusicalMode currentMode = defaultMode;
    std::string pendingSectionMarker;
    std::string pendingKeyChange;

    Bar lastValidBar;
    ChordToken lastValidChord;
    lastValidChord.rootName = "C";
    lastValidChord.rootPitch = PitchClass::C;
    lastValidChord.quality = ChordQuality::Major;
    lastValidChord.durationBeats = defaultNumerator;
    lastValidChord.durationTicks = defaultNumerator * TICKS_PER_BEAT;

    // 行ごとに分割
    std::istringstream stream((std::string(text)));
    std::string line;
    int barCounter = 1;

    while (std::getline(stream, line)) {
        // 空白トリム
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        // コメント行スキップ (#, //)
        if (line.starts_with("#") || line.starts_with("//")) continue;

        // セクションマーカー / キーチェンジ行判定
        // 例: [Intro], [Chorus], [Key: G], [Key: F# Minor]
        if (line.starts_with("[") && line.ends_with("]")) {
            std::string content = line.substr(1, line.size() - 2);
            if (content.starts_with("Key:") || content.starts_with("key:")) {
                std::string kStr = content.substr(4);
                size_t ks = kStr.find_first_not_of(" \t");
                if (ks != std::string::npos) {
                    kStr = kStr.substr(ks);
                    if (kStr.find("minor") != std::string::npos || kStr.find("Minor") != std::string::npos) {
                        currentMode = MusicalMode::NaturalMinor;
                    } else {
                        currentMode = MusicalMode::Major;
                    }
                    size_t sp = kStr.find(' ');
                    currentKey = (sp != std::string::npos) ? kStr.substr(0, sp) : kStr;
                    pendingKeyChange = currentKey;
                }
            } else {
                pendingSectionMarker = content;
            }
            continue;
        }

        // 小節線 '|' で分割
        std::vector<std::string> barSegments;
        if (line.find('|') != std::string::npos) {
            std::string seg;
            for (char c : line) {
                if (c == '|') {
                    size_t s = seg.find_first_not_of(" \t");
                    if (s != std::string::npos) {
                        barSegments.push_back(seg.substr(s, seg.find_last_not_of(" \t") - s + 1));
                    }
                    seg.clear();
                } else {
                    seg += c;
                }
            }
            size_t s = seg.find_first_not_of(" \t");
            if (s != std::string::npos) {
                barSegments.push_back(seg.substr(s, seg.find_last_not_of(" \t") - s + 1));
            }
        } else {
            // 小節線がない場合: トークン数が defaultNumerator (4) を超える場合は4拍ごとに小節を自動分割する
            auto lineTokens = TokenizeBar(line);
            if (static_cast<int>(lineTokens.size()) > defaultNumerator) {
                std::string chunk;
                int countInChunk = 0;
                for (const auto& t : lineTokens) {
                    if (!chunk.empty()) chunk += " ";
                    chunk += t;
                    countInChunk++;
                    if (countInChunk >= defaultNumerator) {
                        barSegments.push_back(chunk);
                        chunk.clear();
                        countInChunk = 0;
                    }
                }
                if (!chunk.empty()) {
                    barSegments.push_back(chunk);
                }
            } else {
                barSegments.push_back(line);
            }
        }

        for (const auto& seg : barSegments) {
            if (seg.empty()) continue;

            Bar bar;
            bar.barNumber = barCounter++;
            bar.timeNumerator = defaultNumerator;
            bar.timeDenominator = defaultDenominator;
            if (!pendingSectionMarker.empty()) {
                bar.sectionMarker = pendingSectionMarker;
                pendingSectionMarker.clear();
            }
            if (!pendingKeyChange.empty()) {
                bar.keyChange = pendingKeyChange;
                pendingKeyChange.clear();
            }

            auto tokens = TokenizeBar(seg);
            if (tokens.empty()) continue;

            // 小節全体が繰り返し記号（"%" または "="）の場合
            if (tokens.size() == 1 && IsRepeatToken(tokens[0])) {
                if (!lastValidBar.chords.empty()) {
                    bar.chords = lastValidBar.chords;
                } else {
                    bar.chords.push_back(lastValidChord);
                }
                bars.push_back(bar);
                lastValidBar = bar;
                continue;
            }

            // トークン列の解析と音価配分
            struct ParsedToken {
                ChordToken chord;
                bool isHyphen = false;
            };
            std::vector<ParsedToken> parsedList;

            for (const auto& tok : tokens) {
                if (tok == "-") {
                    ParsedToken pt;
                    pt.isHyphen = true;
                    parsedList.push_back(pt);
                } else if (IsRepeatToken(tok)) {
                    ParsedToken pt;
                    pt.chord = lastValidChord;
                    pt.chord.rawText = tok;
                    parsedList.push_back(pt);
                } else if (IsRestToken(tok)) {
                    ParsedToken pt;
                    pt.chord.isRest = true;
                    pt.chord.quality = ChordQuality::Rest;
                    pt.chord.rootName = "R";
                    pt.chord.degreeRoman = "R";
                    pt.chord.rawText = tok;
                    parsedList.push_back(pt);
                    lastValidChord = pt.chord;
                } else {
                    ParsedToken pt;
                    pt.chord = MusicEngine::ParseChordSymbol(tok, currentKey, currentMode);
                    parsedList.push_back(pt);
                    if (!pt.chord.isRest && pt.chord.rootPitch != PitchClass::Unknown) {
                        lastValidChord = pt.chord;
                    }
                }
            }

            // 音価の計算
            std::vector<ChordToken> finalChords;
            for (const auto& pt : parsedList) {
                if (pt.isHyphen) {
                    if (!finalChords.empty()) {
                        finalChords.back().durationBeats++;
                        finalChords.back().durationTicks = finalChords.back().durationBeats * TICKS_PER_BEAT;
                    }
                } else {
                    finalChords.push_back(pt.chord);
                    finalChords.back().durationBeats = 1;
                    finalChords.back().durationTicks = TICKS_PER_BEAT;
                }
            }

            int totalBeatsSpecified = 0;
            for (const auto& c : finalChords) {
                totalBeatsSpecified += c.durationBeats;
            }

            const int barTotalTicks = bar.GetTotalTicks();
            if (totalBeatsSpecified < defaultNumerator && !finalChords.empty()) {
                size_t numChords = finalChords.size();
                int baseBeats = defaultNumerator / static_cast<int>(numChords);
                int remainder = defaultNumerator % static_cast<int>(numChords);

                for (size_t i = 0; i < numChords; ++i) {
                    finalChords[i].durationBeats = baseBeats + (i < static_cast<size_t>(remainder) ? 1 : 0);
                    finalChords[i].durationTicks = finalChords[i].durationBeats * TICKS_PER_BEAT;
                }
            } else if (totalBeatsSpecified > defaultNumerator && !finalChords.empty()) {
                // 1小節に5個以上のコードが明示された場合も 0 tick にならないよう barTotalTicks を配分
                int tickPerUnit = std::max(60, barTotalTicks / totalBeatsSpecified);
                int assignedTicks = 0;
                for (size_t i = 0; i < finalChords.size(); ++i) {
                    if (i + 1 == finalChords.size()) {
                        finalChords[i].SetTicks(std::max(60, barTotalTicks - assignedTicks));
                    } else {
                        int t = tickPerUnit * finalChords[i].durationBeats;
                        finalChords[i].SetTicks(t);
                        assignedTicks += t;
                    }
                }
            } else {
                for (auto& c : finalChords) {
                    if (c.durationTicks <= 0) {
                        c.durationTicks = std::max(60, c.durationBeats * TICKS_PER_BEAT);
                    }
                }
            }

            bar.chords = std::move(finalChords);
            bars.push_back(bar);
            lastValidBar = bar;
        }
    }

    return bars;
}

std::string ProgressionParser::Format(const std::vector<Bar>& bars, int barsPerLine) {
    if (bars.empty()) return "";

    std::ostringstream oss;
    int countInLine = 0;

    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& bar = bars[i];

        if (!bar.sectionMarker.empty()) {
            if (countInLine > 0) {
                oss << "|\n";
                countInLine = 0;
            }
            oss << "[" << bar.sectionMarker << "]\n";
        }
        if (!bar.keyChange.empty()) {
            if (countInLine > 0) {
                oss << "|\n";
                countInLine = 0;
            }
            oss << "[Key: " << bar.keyChange << "]\n";
        }

        oss << "| ";
        if (bar.chords.empty()) {
            oss << "- ";
        } else {
            int usedBeats = 0;
            for (size_t ci = 0; ci < bar.chords.size(); ++ci) {
                const auto& chord = bar.chords[ci];
                if (chord.isRest) {
                    oss << ((chord.rawText == "_" || chord.rawText.empty()) ? "_" : "R");
                } else {
                    oss << chord.rootName << chord.qualitySuffix;
                    if (!chord.bassName.empty()) {
                        oss << "/" << chord.bassName;
                    }
                }
                usedBeats += chord.durationBeats;
                if (bar.chords.size() == 1 && chord.durationBeats >= bar.timeNumerator) {
                    // 1コード1小節の場合は余計なハイフン不要
                } else {
                    for (int h = 1; h < chord.durationBeats; ++h) {
                        oss << " -";
                    }
                }
                if (ci + 1 < bar.chords.size()) {
                    oss << " ";
                }
            }
            if (bar.chords.size() == 1 && bar.chords[0].durationBeats < bar.timeNumerator) {
                for (int b = usedBeats; b < bar.timeNumerator; ++b) {
                    oss << " _";
                }
            }
            oss << " ";
        }

        countInLine++;
        if (countInLine >= barsPerLine) {
            oss << "|\n";
            countInLine = 0;
        }
    }

    if (countInLine > 0) {
        oss << "|";
    }

    return oss.str();
}

} // namespace clp
