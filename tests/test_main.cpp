#include "music_theory.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "part_manager.hpp"
#include "midi_builder.hpp"
#include "i18n.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace clp;

int g_passCount = 0;
int g_failCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_failCount++; \
        } else { \
            g_passCount++; \
        } \
    } while(0)

void TestBug1_RepeatTokens() {
    std::cout << "[TEST] 1. Repeat tokens (% and =) across beats and bars\n";

    // 小節全体の繰り返し | C | % |
    auto bars1 = ProgressionParser::Parse("| C | % |");
    TEST_ASSERT(bars1.size() == 2, "Must parse 2 bars");
    TEST_ASSERT(!bars1[0].chords.empty() && bars1[0].chords[0].rootName == "C", "Bar 1 must be C");
    TEST_ASSERT(!bars1[1].chords.empty() && bars1[1].chords[0].rootName == "C", "Bar 2 (%) must repeat C");
    TEST_ASSERT(bars1[1].chords[0].durationBeats == 4, "Bar 2 must have 4 beats");

    // 複数コードの小節繰り返し | C G | % |
    auto bars2 = ProgressionParser::Parse("| C G | % |");
    TEST_ASSERT(bars2.size() == 2, "Must parse 2 bars");
    TEST_ASSERT(bars2[1].chords.size() == 2, "Bar 2 must repeat both chords");
    TEST_ASSERT(bars2[1].chords[0].rootName == "C" && bars2[1].chords[1].rootName == "G", "Bar 2 must be C and G");

    // 拍単位の繰り返し | C % Am = |
    auto bars3 = ProgressionParser::Parse("| C % Am = |");
    TEST_ASSERT(bars3.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(bars3[0].chords.size() == 4, "Bar must have 4 chord tokens");
    TEST_ASSERT(bars3[0].chords[0].rootName == "C", "Beat 1 is C");
    TEST_ASSERT(bars3[0].chords[1].rootName == "C", "Beat 2 (%) repeats C");
    TEST_ASSERT(bars3[0].chords[2].rootName == "A", "Beat 3 is Am");
    TEST_ASSERT(bars3[0].chords[3].rootName == "A", "Beat 4 (=) repeats Am");
}

void TestBug2_EnglishLocalization() {
    std::cout << "[TEST] 2. English localization contains 0 Japanese characters\n";

    I18n::SetLanguage(Language::English);
    bool allEnClean = true;

    for (int i = 0; i <= static_cast<int>(StrId::StatusPasted); ++i) {
        StrId id = static_cast<StrId>(i);
        const auto& wstr = I18n::Get(id);
        TEST_ASSERT(!wstr.empty(), "String ID must not be empty");
        for (wchar_t c : wstr) {
            // 日本語の範囲 (ひらがな、カタカナ、CJK統合漢字、全角記号: 0x3000以上) を検知
            if (c >= 0x3000 && c <= 0x9FFF) {
                std::cerr << "Found Japanese char in English mode: U+" << std::hex << (int)c << " in StrId=" << i << "\n";
                allEnClean = false;
            }
        }
    }
    TEST_ASSERT(allEnClean, "All English localized strings must contain 0 Japanese characters");
}

void TestBug3_ModalInterchangeAndDragDrop() {
    std::cout << "[TEST] 3. Modal interchange chords generated properly for Major and Minor\n";

    auto miMajor = MusicEngine::GetModalInterchangeChords("C", MusicalMode::Major);
    TEST_ASSERT(miMajor.size() == 7, "Parallel minor borrowed chords must have 7 chords");
    TEST_ASSERT(miMajor[0].rootName == "C" && miMajor[0].quality == ChordQuality::Minor7, "Im7 is Cm7");
    TEST_ASSERT(miMajor[2].rootName == "Eb" && miMajor[2].quality == ChordQuality::Major7, "bIIImaj7 is Ebmaj7");
    TEST_ASSERT(miMajor[5].rootName == "Ab" && miMajor[5].quality == ChordQuality::Major7, "bVImaj7 is Abmaj7");
    TEST_ASSERT(miMajor[6].rootName == "Bb" && miMajor[6].quality == ChordQuality::Dominant7, "bVII7 is Bb7");

    auto miMinor = MusicEngine::GetModalInterchangeChords("C", MusicalMode::NaturalMinor);
    TEST_ASSERT(miMinor.size() == 7, "Parallel major borrowed chords must have 7 chords");
    TEST_ASSERT(miMinor[0].rootName == "C" && miMinor[0].quality == ChordQuality::Major7, "Imaj7 is Cmaj7");
    TEST_ASSERT(miMinor[3].rootName == "F" && miMinor[3].quality == ChordQuality::Major7, "IVmaj7 is Fmaj7");
}

void TestBug4_DegreesNeverReturnQuestionMark() {
    std::cout << "[TEST] 4. Degrees never return '?' for all 12 chromatic semitones and qualities\n";

    PitchClass keyC = PitchClass::C;
    ChordQuality testQualities[] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7,
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::HalfDiminished7,
        ChordQuality::Diminished7, ChordQuality::Augmented, ChordQuality::Sus4
    };

    for (int semitone = 0; semitone < 12; ++semitone) {
        PitchClass root = static_cast<PitchClass>(semitone);
        for (auto q : testQualities) {
            std::string deg = MusicEngine::CalculateDegree(root, q, keyC, MusicalMode::Major);
            TEST_ASSERT(deg.find('?') == std::string::npos, "Degree must NEVER contain '?' for any semitone");
            TEST_ASSERT(!deg.empty(), "Degree must not be empty");
        }
    }

    // 同主短調の借用和音ディグリー検証
    std::string degEb = MusicEngine::CalculateDegree(PitchClass::Ds, ChordQuality::Major7, keyC, MusicalMode::Major);
    TEST_ASSERT(degEb == "bIIImaj7", "Ebmaj7 in C Major must be bIIImaj7");

    std::string degAb = MusicEngine::CalculateDegree(PitchClass::Gs, ChordQuality::Major7, keyC, MusicalMode::Major);
    TEST_ASSERT(degAb == "bVImaj7", "Abmaj7 in C Major must be bVImaj7");

    std::string degBb = MusicEngine::CalculateDegree(PitchClass::As, ChordQuality::Dominant7, keyC, MusicalMode::Major);
    TEST_ASSERT(degBb == "bVII7", "Bb7 in C Major must be bVII7");

    std::string degFm = MusicEngine::CalculateDegree(PitchClass::F, ChordQuality::Minor7, keyC, MusicalMode::Major);
    TEST_ASSERT(degFm == "IVm7", "Fm7 in C Major must be IVm7");
}

void TestBug5_HyphenPackedTokenNormalization() {
    std::cout << "[TEST] 5. Packed hyphens (e.g. C---, Fmaj7--) separated without trailing hyphens in chord name\n";

    auto bars1 = ProgressionParser::Parse("| C--- |");
    TEST_ASSERT(bars1.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(bars1[0].chords.size() == 1, "Must have 1 chord");
    TEST_ASSERT(bars1[0].chords[0].rootName == "C", "Root name must be C");
    TEST_ASSERT(bars1[0].chords[0].qualitySuffix.empty(), "Quality suffix must be empty (no hyphens)");
    TEST_ASSERT(bars1[0].chords[0].durationBeats == 4, "Duration must be 4 beats");

    auto bars2 = ProgressionParser::Parse("| Fmaj7--- |");
    TEST_ASSERT(bars2[0].chords[0].rootName == "F", "Root name must be F");
    TEST_ASSERT(bars2[0].chords[0].qualitySuffix == "maj7", "Quality suffix must be 'maj7' without hyphens");
    TEST_ASSERT(bars2[0].chords[0].durationBeats == 4, "Duration must be 4 beats");

    // ハーフディミニッシュ m7-5 の保護
    auto bars3 = ProgressionParser::Parse("| Dm7-5 - G7 - |");
    TEST_ASSERT(bars3[0].chords.size() == 2, "Must parse 2 chords");
    TEST_ASSERT(bars3[0].chords[0].quality == ChordQuality::HalfDiminished7, "Dm7-5 must be HalfDiminished7");
    TEST_ASSERT(bars3[0].chords[0].durationBeats == 2, "Dm7-5 has 2 beats");
    TEST_ASSERT(bars3[0].chords[1].durationBeats == 2, "G7 has 2 beats");
}

void TestRestTokens() {
    std::cout << "[TEST] 6. Rest token (R, N.C.) input and parsing\n";

    auto bars = ProgressionParser::Parse("| C - R - |");
    TEST_ASSERT(bars.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(bars[0].chords.size() == 2, "Must have 2 items (C and Rest)");
    TEST_ASSERT(bars[0].chords[0].rootName == "C" && bars[0].chords[0].durationBeats == 2, "C has 2 beats");
    TEST_ASSERT(bars[0].chords[1].isRest && bars[0].chords[1].durationBeats == 2, "Rest has 2 beats");
    TEST_ASSERT(bars[0].chords[1].degreeRoman == "R", "Rest degree is R");
}

void TestPartManagerAndKeyChangeMerge() {
    std::cout << "[TEST] 7. Part manager and full song merge with key attribution\n";

    PartManager pm;
    auto merged = pm.GenerateMergedProgression();
    TEST_ASSERT(!merged.empty(), "Merged progression must not be empty");

    // Intro (C), Verse (C), Pre (C), Chorus (Eb)
    // 最初の小節は Key: C Major
    TEST_ASSERT(merged[0].keyChange == "C Major", "First bar has initial key C Major");
    TEST_ASSERT(merged[0].sectionMarker == "Intro", "First bar has section Intro");

    // Chorus は 13小節目 (4 bars * 3 = 12 bars preceding)
    TEST_ASSERT(merged[12].sectionMarker == "Chorus", "Bar 13 must be Chorus");
    TEST_ASSERT(merged[12].keyChange == "Eb Major", "Bar 13 must have key change to Eb Major");

    // Chorusの第1コード Ab のディグリーが Eb 基準で IV になっていること
    TEST_ASSERT(merged[12].chords[0].rootName == "Ab", "Chorus bar 1 chord is Ab");
    TEST_ASSERT(merged[12].chords[0].degreeRoman == "IV", "Ab in Eb Major must be degree IV");
}

void TestMidiBuilder() {
    std::cout << "[TEST] 8. MIDI file builder generates valid SMF Format 0 binary\n";

    auto bars = ProgressionParser::Parse("| C | G | Am | F |");
    auto midiBytes = MidiBuilder::BuildMidi(bars, 120);

    TEST_ASSERT(midiBytes.size() > 22, "MIDI bytes must exceed header size");
    TEST_ASSERT(midiBytes[0] == 'M' && midiBytes[1] == 'T' && midiBytes[2] == 'h' && midiBytes[3] == 'd', "Header must start with MThd");
    TEST_ASSERT(midiBytes[14] == 'M' && midiBytes[15] == 'T' && midiBytes[16] == 'r' && midiBytes[17] == 'k', "Track must start with MTrk");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "ChordLaunchpad C++20 Core Test Suite\n";
    std::cout << "========================================\n";

    TestBug1_RepeatTokens();
    TestBug2_EnglishLocalization();
    TestBug3_ModalInterchangeAndDragDrop();
    TestBug4_DegreesNeverReturnQuestionMark();
    TestBug5_HyphenPackedTokenNormalization();
    TestRestTokens();
    TestPartManagerAndKeyChangeMerge();
    TestMidiBuilder();

    std::cout << "========================================\n";
    std::cout << "Tests Passed: " << g_passCount << ", Failed: " << g_failCount << "\n";
    std::cout << "========================================\n";

    return (g_failCount == 0) ? 0 : 1;
}
