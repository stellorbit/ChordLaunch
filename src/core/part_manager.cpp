#include "part_manager.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"

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
    p.id = "part_" + std::to_string(m_parts.size() + 1);
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
    copy.id = "part_" + std::to_string(m_parts.size() + 1);
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

            // パートの先頭小節にセクション名マーカーを付与
            if (i == 0) {
                b.sectionMarker = part.name;

                // キーが直前パートと異なる、または楽曲先頭の場合は転調マーカーを付与
                if (isFirstPart || part.keyRoot != lastKey || part.mode != lastMode) {
                    b.keyChange = part.keyRoot + ((part.mode == MusicalMode::NaturalMinor) ? " Minor" : " Major");
                    lastKey = part.keyRoot;
                    lastMode = part.mode;
                    isFirstPart = false;
                }
            }

            // マージ後の各コードのディグリーを、このパートのKeyに基づいて再計算
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

} // namespace clp
