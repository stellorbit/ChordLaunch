#pragma once
#include "music_theory.hpp"
#include <string>
#include <vector>
#include <span>
#include <cstdint>
#include <string_view>

namespace clp {

struct ProgressionTemplate {
    std::string id;
    std::string majorCategory; // 例: "🇯🇵 ポップス・アニソン"
    std::string subCategory;   // 例: "🔥 人気・定番"
    std::string name;          // 例: "王道進行 (4536)"
    std::string progression;   // 例: "IVmaj7 V7 iiim7 vim7"
    std::string description;   // 説明文
    bool isUserCustom = false;
};

class MusicEngine {
public:
    static PitchClass NoteToPitch(std::string_view noteName);
    static std::string PitchToNote(PitchClass pitch, bool preferFlat = false);
    static std::string PitchToNoteForKey(PitchClass pitch, std::string_view keyRoot, MusicalMode mode);

    static std::string QualityToSuffix(ChordQuality quality);
    static ChordToken ParseChordSymbol(std::string_view symbol, std::string_view currentKey = "C", MusicalMode currentMode = MusicalMode::Major);

    static std::string CalculateDegree(
        PitchClass chordRoot,
        ChordQuality quality,
        PitchClass keyRoot,
        MusicalMode mode,
        PitchClass bassPitch = PitchClass::Unknown
    );

    static std::vector<ChordToken> GetDiatonicChords(std::string_view keyRoot, MusicalMode mode, bool with7th);
    static std::vector<ChordToken> GetDiatonicChordsByTab(std::string_view keyRoot, MusicalMode mode, int tabIndex);
    static std::vector<ChordToken> GetModalInterchangeChords(std::string_view keyRoot, MusicalMode mode, bool with7th = true);
    static std::vector<ChordToken> GetSecondaryDominants(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ChordToken> GetTritoneSubstitutions(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ChordToken> GetSuggestions(const std::vector<ChordToken>& chords, std::string_view keyRoot, MusicalMode mode, bool with7th = true);

    // 後方互換用テンプレート定義
    struct TemplateInfo {
        std::string id;
        std::string name;
        std::string progression;
        std::string description;
    };
    static std::vector<TemplateInfo> GetBuiltinTemplates();

    // Ver 2026.9.2 準拠のジャンル別大分類テンプレート
    static const std::vector<ProgressionTemplate>& GetAllGenreTemplates();
    static std::vector<std::string> GetTemplateMajorCategories();
    static std::vector<ProgressionTemplate> GetTemplatesByCategory(std::string_view category);
    static bool SaveUserTemplate(const ProgressionTemplate& tmpl);
    static bool DeleteUserTemplate(std::string_view id);

    static std::vector<int> GetChordMidiNotes(
        const ChordToken& chord,
        int baseOctave = 4,
        VoicingStyle voicing = VoicingStyle::RootPosition,
        int inversion = 0,
        int bassOctaveMode = 0
    );

    static std::vector<std::string> GetCircleOfFifths(bool counterClockwise = false);

private:
    static std::vector<int> GetQualityIntervals(ChordQuality quality);
    static bool IsFlatKey(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ProgressionTemplate> s_userTemplates;
    static bool s_userTemplatesLoaded;
    static void LoadUserTemplates();
};

} // namespace clp
