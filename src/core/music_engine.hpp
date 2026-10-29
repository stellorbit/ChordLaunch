#pragma once
#include "music_theory.hpp"
#include <string>
#include <vector>
#include <span>
#include <cstdint>

namespace clp {

class MusicEngine {
public:
    static PitchClass NoteToPitch(std::string_view noteName);
    static std::string PitchToNote(PitchClass pitch, bool preferFlat = false);
    static std::string PitchToNoteForKey(PitchClass pitch, std::string_view keyRoot, MusicalMode mode);

    static ChordToken ParseChordSymbol(std::string_view symbol, std::string_view currentKey = "C", MusicalMode currentMode = MusicalMode::Major);

    static std::string CalculateDegree(
        PitchClass chordRoot,
        ChordQuality quality,
        PitchClass keyRoot,
        MusicalMode mode,
        PitchClass bassPitch = PitchClass::Unknown
    );

    static std::vector<ChordToken> GetDiatonicChords(std::string_view keyRoot, MusicalMode mode, bool with7th);
    static std::vector<ChordToken> GetModalInterchangeChords(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ChordToken> GetSecondaryDominants(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ChordToken> GetTritoneSubstitutions(std::string_view keyRoot, MusicalMode mode);
    static std::vector<ChordToken> GetSuggestions(const std::vector<ChordToken>& chords, std::string_view keyRoot, MusicalMode mode);

    struct TemplateInfo {
        std::string id;
        std::string name;
        std::string progression;
        std::string description;
    };
    static std::vector<TemplateInfo> GetBuiltinTemplates();

    static std::vector<int> GetChordMidiNotes(
        const ChordToken& chord,
        int baseOctave = 4,
        VoicingStyle voicing = VoicingStyle::RootPosition
    );

    static std::vector<std::string> GetCircleOfFifths(bool counterClockwise = false);

private:
    static std::vector<int> GetQualityIntervals(ChordQuality quality);
    static bool IsFlatKey(std::string_view keyRoot, MusicalMode mode);
};

} // namespace clp
