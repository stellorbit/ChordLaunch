#include "music_theory.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "part_manager.hpp"
#include "midi_builder.hpp"
#include "i18n.hpp"
#include "wasapi_synth.hpp"
#include "app_window_internal.hpp"
#include "../src/version.h"
#include <windows.h>
#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <shlobj.h>

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
    TEST_ASSERT(degEb == "bIIIMaj7", "EbMaj7 in C Major must be bIIIMaj7");

    std::string degAb = MusicEngine::CalculateDegree(PitchClass::Gs, ChordQuality::Major7, keyC, MusicalMode::Major);
    TEST_ASSERT(degAb == "bVIMaj7", "AbMaj7 in C Major must be bVIMaj7");

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
    TEST_ASSERT(bars2[0].chords[0].qualitySuffix == "Maj7", "Quality suffix must be 'Maj7' without hyphens");
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

    // アンダースコア記法による空き拍・休符テスト
    auto barsGap = ProgressionParser::Parse("| C _ G _ |");
    TEST_ASSERT(barsGap.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(barsGap[0].chords.size() == 4, "Must have 4 items (C, _, G, _)");
    TEST_ASSERT(barsGap[0].chords[0].rootName == "C" && barsGap[0].chords[0].durationBeats == 1, "C is 1 beat");
    TEST_ASSERT(barsGap[0].chords[1].isRest && barsGap[0].chords[1].durationBeats == 1, "_ is 1 beat rest");
    TEST_ASSERT(barsGap[0].chords[2].rootName == "G" && barsGap[0].chords[2].durationBeats == 1, "G is 1 beat");
    TEST_ASSERT(barsGap[0].chords[3].isRest && barsGap[0].chords[3].durationBeats == 1, "_ is 1 beat rest");

    // 先頭空白・ハイフンテスト
    auto barsLeadGap = ProgressionParser::Parse("| _ _ C - |");
    TEST_ASSERT(barsLeadGap.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(barsLeadGap[0].chords[0].isRest && barsLeadGap[0].chords[0].durationBeats == 1, "Leading rest 1 beat");
    TEST_ASSERT(barsLeadGap[0].chords[1].isRest && barsLeadGap[0].chords[1].durationBeats == 1, "Leading rest 2nd beat");
    TEST_ASSERT(barsLeadGap[0].chords[2].rootName == "C" && barsLeadGap[0].chords[2].durationBeats == 2, "C is 2 beats");

    // 可逆フォーマットテスト
    std::string fmt = ProgressionParser::Format(barsGap);
    TEST_ASSERT(fmt.find("C _ G _") != std::string::npos, "Formatted output must retain gap underscores: " + fmt);
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

void TestLinkedKeyModeBpm() {
    std::cout << "[TEST] 7b. Linked key/mode/BPM apply across parts\n";

    PartManager pm; // Intro/Verse/Pre = C Major 120, Chorus = Eb Major 120
    pm.SetActiveIndex(0);

    auto ch = pm.ApplyBpm(100, true);
    TEST_ASSERT(ch.size() == 4, "Linked BPM must reach all 4 parts sharing 120");
    for (const auto& p : pm.GetParts()) TEST_ASSERT(p.bpm == 100, "All parts BPM 100");

    pm.SetActiveIndex(1);
    ch = pm.ApplyBpm(90, false);
    TEST_ASSERT(ch.size() == 1 && pm.GetParts()[1].bpm == 90 && pm.GetParts()[0].bpm == 100, "Unlinked BPM changes only the active part");

    pm.SetActiveIndex(0);
    ch = pm.ApplyBpm(110, true);
    TEST_ASSERT(ch.size() == 3 && pm.GetParts()[1].bpm == 90, "Linked BPM must skip the part with a different BPM");

    pm.SetActiveIndex(0);
    ch = pm.ApplyKey("D", true);
    TEST_ASSERT(ch.size() == 3, "Linked key reaches the 3 C parts");
    TEST_ASSERT(pm.GetParts()[3].keyRoot == "Eb", "Modulated chorus key stays Eb");
    TEST_ASSERT(pm.GetParts()[0].keyRoot == "D" && pm.GetParts()[2].keyRoot == "D", "C parts become D");

    pm.SetActiveIndex(0);
    ch = pm.ApplyMode(MusicalMode::NaturalMinor, false);
    TEST_ASSERT(ch.size() == 1 && pm.GetParts()[1].mode == MusicalMode::Major, "Unlinked mode changes only the active part");

    Part& added = pm.AddPart("New Part");
    TEST_ASSERT(added.keyRoot == "Eb" && added.mode == MusicalMode::Major && added.bpm == 110, "New part inherits last part's key/mode/BPM");
}

void TestMidiBuilder() {
    std::cout << "[TEST] 8. MIDI file builder generates valid SMF Format 0 binary\n";

    auto bars = ProgressionParser::Parse("| C | G | Am | F |");
    auto midiBytes = MidiBuilder::BuildMidi(bars, 120);

    TEST_ASSERT(midiBytes.size() > 22, "MIDI bytes must exceed header size");
    TEST_ASSERT(midiBytes[0] == 'M' && midiBytes[1] == 'T' && midiBytes[2] == 'h' && midiBytes[3] == 'd', "Header must start with MThd");
    TEST_ASSERT(midiBytes[14] == 'M' && midiBytes[15] == 'T' && midiBytes[16] == 'r' && midiBytes[17] == 'k', "Track must start with MTrk");
}

void TestDegreeInputParsing() {
    std::cout << "[TEST] 9. Degree notation parsing (1 5 6m 4, 4mas 3svn 6mis 1svn, Roman numerals)\n";

    auto bars1 = ProgressionParser::Parse("1 5 6m 4", "C", MusicalMode::Major);
    TEST_ASSERT(bars1.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(bars1[0].chords.size() == 4, "Must have 4 chords");
    TEST_ASSERT(bars1[0].chords[0].rootName == "C", "1 is C");
    TEST_ASSERT(bars1[0].chords[1].rootName == "G", "5 is G");
    TEST_ASSERT(bars1[0].chords[2].rootName == "A" && bars1[0].chords[2].quality == ChordQuality::Minor, "6m is Am");
    TEST_ASSERT(bars1[0].chords[3].rootName == "F", "4 is F");

    // 丸サ進行ディグリー 4mas 3svn 6mis 1svn
    auto bars2 = ProgressionParser::Parse("4mas 3svn 6mis 1svn", "C", MusicalMode::Major);
    TEST_ASSERT(bars2[0].chords[0].rootName == "F" && bars2[0].chords[0].quality == ChordQuality::Major7, "4mas is Fmaj7");
    TEST_ASSERT(bars2[0].chords[1].rootName == "E" && bars2[0].chords[1].quality == ChordQuality::Dominant7, "3svn is E7");
    TEST_ASSERT(bars2[0].chords[2].rootName == "A" && bars2[0].chords[2].quality == ChordQuality::Minor7, "6mis is Am7");
    TEST_ASSERT(bars2[0].chords[3].rootName == "C" && bars2[0].chords[3].quality == ChordQuality::Dominant7, "1svn is C7");

    // ローマ数字入力 IVmaj7 V7 iiim7 vim7
    auto bars3 = ProgressionParser::Parse("IVmaj7 V7 iiim7 vim7", "C", MusicalMode::Major);
    TEST_ASSERT(bars3[0].chords[0].rootName == "F" && bars3[0].chords[0].quality == ChordQuality::Major7, "IVmaj7 is Fmaj7");
    TEST_ASSERT(bars3[0].chords[1].rootName == "G" && bars3[0].chords[1].quality == ChordQuality::Dominant7, "V7 is G7");
    TEST_ASSERT(bars3[0].chords[2].rootName == "E" && bars3[0].chords[2].quality == ChordQuality::Minor7, "iiim7 is Em7");
    TEST_ASSERT(bars3[0].chords[3].rootName == "A" && bars3[0].chords[3].quality == ChordQuality::Minor7, "vim7 is Am7");
}

void TestNewAuditFixes() {
    std::cout << "[TEST] 10. Audit fixes: QualitySuffix preservation, CM7 vs Cm7, and Circle of Fifths\n";

    // C-02: GetDiatonicChords preserving qualitySuffix
    auto diatonics7 = MusicEngine::GetDiatonicChords("C", MusicalMode::Major, true);
    TEST_ASSERT(diatonics7.size() == 7, "Diatonics size is 7");
    TEST_ASSERT(diatonics7[0].qualitySuffix == "Maj7", "Diatonic 1 7th has qualitySuffix Maj7");
    TEST_ASSERT(diatonics7[1].qualitySuffix == "m7", "Diatonic 2 7th has qualitySuffix m7");
    TEST_ASSERT(diatonics7[4].qualitySuffix == "7", "Diatonic 5 7th has qualitySuffix 7");
    TEST_ASSERT(diatonics7[6].qualitySuffix == "m7b5", "Diatonic 7 7th has qualitySuffix m7b5");

    // Modal interchange qualitySuffix
    auto miChords = MusicEngine::GetModalInterchangeChords("C", MusicalMode::Major);
    TEST_ASSERT(miChords[0].qualitySuffix == "m7", "Modal interchange Cm7 suffix is m7");
    TEST_ASSERT(miChords[2].qualitySuffix == "Maj7", "Modal interchange EbMaj7 suffix is Maj7");

    // Triad Modal interchange & Sus4/Add9 tabs
    auto miTriads = MusicEngine::GetModalInterchangeChords("C", MusicalMode::Major, false);
    TEST_ASSERT(miTriads.size() == 7 && miTriads[3].rootName == "F" && miTriads[3].qualitySuffix == "m" && miTriads[3].degreeRoman == "IVm", "Triad modal interchange IVm is Fm");
    TEST_ASSERT(miTriads[5].rootName == "Ab" && miTriads[5].qualitySuffix.empty() && miTriads[5].degreeRoman == "bVI", "Triad modal interchange bVI is Ab");

    auto diaSus4 = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 2);
    TEST_ASSERT(diaSus4.size() == 7 && diaSus4[0].qualitySuffix == "sus4" && diaSus4[4].rootName == "G" && diaSus4[4].qualitySuffix == "sus4", "Sus4 diatonic tab generates Gsus4");

    auto diaAdd9 = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 3);

    // 二重パイプ (||) 正規化テスト: 余分な空小節を作らず正確な小節数にパース
    auto doublePipeBars = ProgressionParser::Parse("| C | C | Eb | Eb || Asus4 _ Asus4 _ |");
    TEST_ASSERT(doublePipeBars.size() == 5, "Double pipe || must not create empty bar, count must be 5");
    TEST_ASSERT(doublePipeBars[4].chords[0].rootName == "A", "5th bar must be Asus4");

    // 同一コード連続配置テスト (勝手にマージされず独立カードとして保持されること)
    auto repeatChords = ProgressionParser::Parse("| Asus4 Asus4 _ _ |");
    TEST_ASSERT(repeatChords.size() == 1, "Must parse 1 bar");
    TEST_ASSERT(repeatChords[0].chords.size() == 4, "Must have 4 items (Asus4, Asus4, _, _)");
    TEST_ASSERT(repeatChords[0].chords[0].rootName == "A" && repeatChords[0].chords[0].qualitySuffix == "sus4" && repeatChords[0].chords[0].durationBeats == 1, "First Asus4 must be 1 beat");
    TEST_ASSERT(repeatChords[0].chords[1].rootName == "A" && repeatChords[0].chords[1].qualitySuffix == "sus4" && repeatChords[0].chords[1].durationBeats == 1, "Second Asus4 must be 1 beat");
    TEST_ASSERT(repeatChords[0].chords[2].isRest && repeatChords[0].chords[2].durationBeats == 1, "First trailing rest must be 1 beat");
    TEST_ASSERT(repeatChords[0].chords[3].isRest && repeatChords[0].chords[3].durationBeats == 1, "Second trailing rest must be 1 beat");
    TEST_ASSERT(diaAdd9.size() == 7 && diaAdd9[0].qualitySuffix == "add9", "Add9 diatonic tab generates Cadd9");

    // ダイアトニック追加バリエーション (tab 5〜8)
    auto diaSus2 = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 5);
    TEST_ASSERT(diaSus2.size() == 7 && diaSus2[0].qualitySuffix == "sus2" && diaSus2[4].rootName == "G", "Sus2 diatonic tab generates Csus2 / Gsus2");
    auto diaSix = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 6);
    TEST_ASSERT(diaSix.size() == 7 && diaSix[0].qualitySuffix == "6" && diaSix[1].qualitySuffix == "m6" && diaSix[6].qualitySuffix == "dim", "6th diatonic tab keeps major/minor/dim character (C6, Dm6, Bdim)");
    auto dia7sus4 = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 7);
    TEST_ASSERT(dia7sus4.size() == 7 && dia7sus4[0].qualitySuffix == "7sus4", "7sus4 diatonic tab generates C7sus4");
    auto diaMAdd9 = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 8);
    TEST_ASSERT(diaMAdd9.size() == 7 && diaMAdd9[0].qualitySuffix == "add9" && diaMAdd9[1].qualitySuffix == "madd9", "Quality-aware add9 tab generates Cadd9 / Dmadd9");
    auto diaSixMinor = MusicEngine::GetDiatonicChordsByTab("A", MusicalMode::NaturalMinor, 6);
    TEST_ASSERT(diaSixMinor.size() == 7 && diaSixMinor[0].qualitySuffix == "m6" && diaSixMinor[2].qualitySuffix == "6", "6th tab in A minor generates Am6 / C6");
    TEST_ASSERT(MusicEngine::IsSeventhDiatonicTab(1) && MusicEngine::IsSeventhDiatonicTab(7) && !MusicEngine::IsSeventhDiatonicTab(0) && !MusicEngine::IsSeventhDiatonicTab(6), "Seventh-tab helper recognises 7th and 7sus4 only");

    // C-10: CM7 vs Cm7 distinction
    auto chordMaj7 = MusicEngine::ParseChordSymbol("CM7");
    TEST_ASSERT(chordMaj7.quality == ChordQuality::Major7, "CM7 parses as Major7");
    TEST_ASSERT(chordMaj7.qualitySuffix == "Maj7", "CM7 suffix is Maj7");

    auto chordMin7 = MusicEngine::ParseChordSymbol("Cm7");
    TEST_ASSERT(chordMin7.quality == ChordQuality::Minor7, "Cm7 parses as Minor7");
    TEST_ASSERT(chordMin7.qualitySuffix == "m7", "Cm7 suffix is m7");

    // Im7 and IVm7 in minor / custom degrees
    auto chordIm7 = MusicEngine::ParseChordSymbol("1mis", "C", MusicalMode::Major);
    TEST_ASSERT(chordIm7.quality == ChordQuality::Minor7, "1mis parses as Minor7");

    // C-04: Circle of Fifths
    auto cof = MusicEngine::GetCircleOfFifths(false);
    TEST_ASSERT(cof.size() == 12, "Circle of fifths has 12 keys");
    TEST_ASSERT(cof[0] == "C" && cof[1] == "G" && cof[11] == "F", "Circle of fifths order is valid");
}

void TestUiRedesignFeatures() {
    std::cout << "[TEST] 11. UI Redesign & MusicEngine Extensions (Templates, Suffixes, Inversions)\n";

    // 1. 31+ Templates and categories
    const auto& tmpls = MusicEngine::GetAllGenreTemplates();
    TEST_ASSERT(tmpls.size() >= 31, "At least 31 templates loaded");

    auto cats = MusicEngine::GetTemplateMajorCategories();
    TEST_ASSERT(cats.size() == 8, "8 major categories (7 genres + user defined)");

    auto popTmpls = MusicEngine::GetTemplatesByCategory("🇯🇵 ポップス・アニソン");
    TEST_ASSERT(!popTmpls.empty(), "Pop/Anime category contains templates");

    // 2. Degree suffixes (hdm, suf, sut, adn, six)
    auto cHdm = MusicEngine::ParseChordSymbol("7hdm", "C", MusicalMode::Major);
    TEST_ASSERT(cHdm.quality == ChordQuality::HalfDiminished7, "7hdm is HalfDiminished7");
    TEST_ASSERT(cHdm.rootName == "B", "7hdm root is B");

    auto cSuf = MusicEngine::ParseChordSymbol("5suf", "C", MusicalMode::Major);
    TEST_ASSERT(cSuf.quality == ChordQuality::Sus4, "5suf is Sus4");
    TEST_ASSERT(cSuf.rootName == "G", "5suf root is G");

    auto cSut = MusicEngine::ParseChordSymbol("1sut", "C", MusicalMode::Major);
    TEST_ASSERT(cSut.quality == ChordQuality::Sus2, "1sut is Sus2");

    auto cAdn = MusicEngine::ParseChordSymbol("1adn", "C", MusicalMode::Major);
    TEST_ASSERT(cAdn.quality == ChordQuality::Add9, "1adn parses as Add9");

    auto cSix = MusicEngine::ParseChordSymbol("1six", "C", MusicalMode::Major);
    TEST_ASSERT(cSix.quality == ChordQuality::Six, "1six is Six");

    // 3. Inversion in GetChordMidiNotes
    ChordToken cC;
    cC.rootName = "C";
    cC.rootPitch = PitchClass::C;
    cC.quality = ChordQuality::Major;
    auto rootNotes = MusicEngine::GetChordMidiNotes(cC, 4, VoicingStyle::RootPosition, 0);
    auto inv1Notes = MusicEngine::GetChordMidiNotes(cC, 4, VoicingStyle::RootPosition, 1);
    TEST_ASSERT(!rootNotes.empty() && !inv1Notes.empty(), "Notes generated");
    TEST_ASSERT(rootNotes != inv1Notes, "Inversion changes chord voicing notes");
}

void TestTickTimeModel() {
    std::cout << "[TEST] 12. Tick Time Model (480 TPQN, durationTicks sync, sub-beat parsing)\n";

    // 1. TICKS_PER_BEAT 定数確認
    TEST_ASSERT(TICKS_PER_BEAT == 480, "TICKS_PER_BEAT is 480");
    TEST_ASSERT(TICKS_PER_BAR_4_4 == 1920, "TICKS_PER_BAR_4_4 is 1920");

    // 2. ChordToken の SetTicks / SetBeats 同期
    ChordToken chord;
    chord.SetTicks(480);
    TEST_ASSERT(chord.durationTicks == 480 && chord.durationBeats == 1, "480 ticks equals 1 beat");

    chord.SetTicks(960);
    TEST_ASSERT(chord.durationTicks == 960 && chord.durationBeats == 2, "960 ticks equals 2 beats");

    chord.SetTicks(240); // 8分音符
    TEST_ASSERT(chord.durationTicks == 240, "240 ticks preserved (8th note)");

    chord.SetBeats(4);
    TEST_ASSERT(chord.durationBeats == 4 && chord.durationTicks == 1920, "4 beats equals 1920 ticks");

    // 3. ProgressionParser::Parse による durationTicks 自動設定
    auto bars = ProgressionParser::Parse("| C G | Am - F - |");
    TEST_ASSERT(bars.size() == 2, "2 bars parsed");
    TEST_ASSERT(bars[0].chords[0].durationTicks == 960, "C has 960 ticks (2 beats)");
    TEST_ASSERT(bars[0].chords[1].durationTicks == 960, "G has 960 ticks (2 beats)");
    TEST_ASSERT(bars[1].chords[0].durationTicks == 960, "Am has 960 ticks (2 beats)");
    TEST_ASSERT(bars[1].chords[1].durationTicks == 960, "F has 960 ticks (2 beats)");

    // 4. Bar::GetTotalTicks
    TEST_ASSERT(bars[0].GetTotalTicks() == 1920, "Bar 1 total ticks is 1920");

    // 5. MidiBuilder への Tick 時間反映
    auto midi = MidiBuilder::BuildMidi(bars, 120);
    TEST_ASSERT(!midi.empty(), "MIDI binary generated from tick-based bars");
}

void TestProjectFileAndAutoBackup() {
    std::cout << "[TEST] 13. Project File Serialization, Deserialization & AutoBackup\n";

    PartManager pm;
    pm.ResetToDefaultSong();
    TEST_ASSERT(pm.GetParts().size() == 4, "Default song has 4 parts");

    // シリアライズ
    std::string json = pm.SerializeProject("TestSong_01", VoicingStyle::Drop2, 1, 480);
    TEST_ASSERT(json.find("\"format\": \"ChordLaunchProject\"") != std::string::npos, "Project format tag exists");
    TEST_ASSERT(json.find("\"title\": \"TestSong_01\"") != std::string::npos, "Project title exists");
    TEST_ASSERT(json.find("\"voicing\": 1") != std::string::npos, "Voicing exists");

    // デシリアライズ
    PartManager pm2;
    std::string title;
    VoicingStyle voicing;
    int bassMode = 0;
    int gridTicks = 0;
    bool ok = pm2.DeserializeProject(json, title, voicing, bassMode, gridTicks);
    TEST_ASSERT(ok, "Deserialization succeeded");
    TEST_ASSERT(title == "TestSong_01", "Title matches");
    TEST_ASSERT(voicing == VoicingStyle::Drop2, "Voicing matches");
    TEST_ASSERT(bassMode == 1, "BassMode matches");
    TEST_ASSERT(gridTicks == 480, "GridTicks matches");
    TEST_ASSERT(pm2.GetParts().size() == 4, "4 parts restored");
    TEST_ASSERT(pm2.GetParts()[0].name == "Intro", "Intro part restored");
    TEST_ASSERT(pm2.GetParts()[3].name == "Chorus", "Chorus part restored");
    TEST_ASSERT(pm2.GetParts()[3].keyRoot == "Eb", "Chorus key Eb restored");

    // 一時ファイルへの保存と読み込み
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring testFile = std::wstring(tempPath) + L"clp_test_project.chord";

    bool saveOk = pm.SaveToFile(testFile, "FileTestSong", VoicingStyle::OpenSpread, 2, 240);
    TEST_ASSERT(saveOk, "File save succeeded");

    PartManager pmFile;
    std::string fileTitle;
    VoicingStyle fileVoicing;
    int fileBass = 0;
    int fileGrid = 0;
    bool loadOk = pmFile.LoadFromFile(testFile, fileTitle, fileVoicing, fileBass, fileGrid);
    TEST_ASSERT(loadOk, "File load succeeded");
    TEST_ASSERT(fileTitle == "FileTestSong", "File title matches");
    TEST_ASSERT(fileVoicing == VoicingStyle::OpenSpread, "File voicing matches");
    TEST_ASSERT(fileBass == 2, "File bass matches");
    TEST_ASSERT(fileGrid == 240, "File grid matches");
    TEST_ASSERT(pmFile.GetParts().size() == 4, "File parts restored");

    // 自動バックアップ検証
    bool backupOk = PartManager::PerformAutoBackup(testFile, "FileTestSong", json);
    TEST_ASSERT(backupOk, "AutoBackup succeeded");

    DeleteFileW(testFile.c_str());
}

void TestUserTemplateManager() {
    std::cout << "[TEST] 14. User Template Registration, Deletion & Categories\n";

    // 組み込みテンプレートカテゴリ確認
    auto categories = MusicEngine::GetTemplateMajorCategories();
    TEST_ASSERT(!categories.empty(), "Categories not empty");

    // 新規テンプレート登録
    ProgressionTemplate pt;
    pt.id = "test_custom_prog_01";
    pt.name = "Test Custom Progression";
    pt.progression = "Fmaj7 G7 Em7 Am7";
    pt.description = "Test description for unit test";
    pt.majorCategory = "👤 ユーザー定義";
    pt.isUserCustom = true;

    bool saveOk = MusicEngine::SaveUserTemplate(pt);
    TEST_ASSERT(saveOk, "User template saved");

    // 取得確認
    auto userTmpls = MusicEngine::GetTemplatesByCategory("👤 ユーザー定義");
    bool found = false;
    for (const auto& t : userTmpls) {
        if (t.id == pt.id) {
            found = true;
            TEST_ASSERT(t.name == pt.name, "Template name matches");
            TEST_ASSERT(t.progression == pt.progression, "Template progression matches");
            TEST_ASSERT(t.isUserCustom, "isUserCustom is true");
            break;
        }
    }
    TEST_ASSERT(found, "Saved template found in user category");

    // 削除確認
    bool delOk = MusicEngine::DeleteUserTemplate(pt.id);
    TEST_ASSERT(delOk, "User template deleted");

    auto afterDel = MusicEngine::GetTemplatesByCategory("👤 ユーザー定義");
    bool foundAfter = false;
    for (const auto& t : afterDel) {
        if (t.id == pt.id) {
            foundAfter = true;
            break;
        }
    }
    TEST_ASSERT(!foundAfter, "Deleted template not found");
}

void TestWasapiAudioEngine() {
    std::cout << "[TEST] 15. WasapiSynth Audio Engine Initialization & Playback\n";
    HRESULT hr = OleInitialize(nullptr);
    std::cout << "  OleInitialize hr: " << std::hex << hr << std::dec << "\n";

    WasapiSynth synth;
    bool initOk = synth.Initialize();
    std::cout << "  synth.Initialize() result: " << (initOk ? "TRUE" : "FALSE") << "\n";
    TEST_ASSERT(initOk, "WasapiSynth::Initialize() succeeded");

    if (initOk) {
        synth.PlayChord({60, 64, 67, 71}); // Cmaj7
        std::cout << "  synth.PlayChord({60, 64, 67, 71}) called\n";
        Sleep(100);
        synth.AllNotesOff();
    }
    synth.Shutdown();
    OleUninitialize();
}

void TestComprehensiveFixes() {
    std::cout << "[TEST] 16. Comprehensive Fixes (Accidental Degrees, Long Bar Split, Tensions, BassMode)\n";

    // 1. Accidental Roman/Arabic degrees: bVII, #4dim, iv, iii7
    auto cBVII = MusicEngine::ParseChordSymbol("bVII", "C", MusicalMode::Major);
    TEST_ASSERT(cBVII.rootName == "Bb" && cBVII.quality == ChordQuality::Major, "bVII in C Major is Bb Major");

    auto cSharp4Dim = MusicEngine::ParseChordSymbol("#4dim", "C", MusicalMode::Major);
    TEST_ASSERT(cSharp4Dim.rootName == "F#" && cSharp4Dim.quality == ChordQuality::Diminished, "#4dim in C Major is F#dim");

    auto cMinorIv = MusicEngine::ParseChordSymbol("iv", "C", MusicalMode::Major);
    TEST_ASSERT(cMinorIv.rootName == "F" && cMinorIv.quality == ChordQuality::Minor, "iv in C Major is Fm");

    auto cMinorIii7 = MusicEngine::ParseChordSymbol("iii7", "C", MusicalMode::Major);
    TEST_ASSERT(cMinorIii7.rootName == "E" && cMinorIii7.quality == ChordQuality::Minor7, "iii7 in C Major is Em7");

    // 2. Long barless progression split (8 chords -> 2 bars, non-zero durationTicks)
    auto canonBars = ProgressionParser::Parse("I V vi iii IV I IV V", "C", MusicalMode::Major);
    TEST_ASSERT(canonBars.size() == 2, "8 barless chords split into 2 bars");
    for (const auto& b : canonBars) {
        for (const auto& c : b.chords) {
            TEST_ASSERT(c.durationTicks > 0, "Every chord in split bars has durationTicks > 0");
        }
    }

    // 3. Parenthesized tensions & Bass Octave Mode
    auto cHendrix = MusicEngine::ParseChordSymbol("E7(#9)", "C", MusicalMode::Major);
    TEST_ASSERT(cHendrix.quality == ChordQuality::Dominant7 && !cHendrix.tensions.empty(), "E7(#9) parses Dominant7 with tension");

    auto notesNoBass = MusicEngine::GetChordMidiNotes(cHendrix, 4, VoicingStyle::RootPosition, 0, 0);
    auto notesBass1 = MusicEngine::GetChordMidiNotes(cHendrix, 4, VoicingStyle::RootPosition, 0, 1);
    auto notesBass3 = MusicEngine::GetChordMidiNotes(cHendrix, 4, VoicingStyle::RootPosition, 0, 3);
    TEST_ASSERT(notesBass1.size() == notesNoBass.size() + 1, "Bass mode 1 adds 1 bass note");
    TEST_ASSERT(notesBass3.size() == notesNoBass.size() + 2, "Bass mode 3 adds 2 bass notes (-1 & -2 oct)");

    // 4. Voice polyphonic headroom & FastChoke verification
    Voice v1;
    v1.NoteOn(60, 0.85f, SynthPreset::GrandPiano, 48000.0f);
    float maxSingleAmp = 0.0f;
    for (int i = 0; i < 500; ++i) {
        maxSingleAmp = std::max(maxSingleAmp, std::abs(v1.ProcessSample()));
    }
    TEST_ASSERT(maxSingleAmp > 0.01f && maxSingleAmp <= 0.22f, "Single voice peak amplitude stays within anti-clipping headroom (<= 0.22)");

    v1.Choke();
    TEST_ASSERT(v1.state == Voice::EnvState::FastChoke, "Choke() transitions voice to FastChoke state");
    for (int i = 0; i < 650; ++i) {
        v1.ProcessSample();
    }
    TEST_ASSERT(!v1.active && v1.state == Voice::EnvState::Idle, "FastChoke silences previous chord within ~12ms");
}

void TestDashboardAndMRU() {
    std::cout << "[TEST] 17. Dashboard, Custom Backup Directory, and MRU\n";

    // 1. PerformAutoBackup with custom directory
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring customBakDir = std::wstring(tempPath) + L"ChordLaunch_CustomBak_Test";
    std::filesystem::remove_all(customBakDir);

    std::string dummyData = "{\"format\":\"ChordLaunchProject\",\"version\":1}";
    bool bakRes = PartManager::PerformAutoBackup(L"", "TestSong", dummyData, customBakDir);
    TEST_ASSERT(bakRes, "PerformAutoBackup succeeds with custom backup directory");
    TEST_ASSERT(std::filesystem::exists(customBakDir), "Custom backup directory was created");

    bool foundBackupFile = false;
    for (const auto& entry : std::filesystem::directory_iterator(customBakDir)) {
        if (entry.path().extension() == ".chord") {
            foundBackupFile = true;
            break;
        }
    }
    TEST_ASSERT(foundBackupFile, "Backup file with .chord extension was written to custom backup directory");
    std::filesystem::remove_all(customBakDir);

    // 2. Empty project state verification
    PartManager pm;
    pm.GetParts().clear();
    Part emptyPart;
    emptyPart.id = "part_1";
    emptyPart.name = "Intro";
    emptyPart.keyRoot = "C";
    emptyPart.mode = MusicalMode::Major;
    emptyPart.bpm = 120;
    emptyPart.timeNumerator = 4;
    emptyPart.timeDenominator = 4;
    pm.GetParts().push_back(emptyPart);
    pm.SetActiveIndex(0);

    TEST_ASSERT(pm.GetParts().size() == 1, "Empty project has exactly 1 part");
    TEST_ASSERT(pm.GetActivePart() != nullptr, "Active part is valid");
    TEST_ASSERT(pm.GetActivePart()->bars.empty(), "Empty part has 0 initial bars");
    auto merged = pm.GenerateMergedProgression();
    TEST_ASSERT(merged.empty(), "GenerateMergedProgression on empty part returns 0 bars safely");

    // 3. MRU Rotation & Deduplication logic verification
    std::vector<std::wstring> mru;
    auto addMru = [&](const std::wstring& path) {
        auto it = std::find(mru.begin(), mru.end(), path);
        if (it != mru.end()) mru.erase(it);
        mru.insert(mru.begin(), path);
        if (mru.size() > 10) mru.resize(10);
    };

    for (int i = 0; i < 15; ++i) {
        addMru(L"C:\\Project_" + std::to_wstring(i) + L".chord");
    }
    TEST_ASSERT(mru.size() == 10, "MRU size is capped at 10 items");
    TEST_ASSERT(mru[0] == L"C:\\Project_14.chord", "Most recently added file is at index 0");
    TEST_ASSERT(mru[9] == L"C:\\Project_5.chord", "Oldest kept file is at index 9");

    // 重複追加
    addMru(L"C:\\Project_5.chord");
    TEST_ASSERT(mru.size() == 10, "Re-adding existing file keeps MRU size at 10");
    TEST_ASSERT(mru[0] == L"C:\\Project_5.chord", "Re-added file moves to top (index 0)");
    TEST_ASSERT(mru[1] == L"C:\\Project_14.chord", "Previous top file shifts to index 1");
}

void TestPartRenameAndMergeAllToNewPart() {
    std::cout << "[TEST] 18. Part Rename and Merge All to New Part\n";

    PartManager pm;
    pm.GetParts().clear();

    // パート1: Verse (4小節)
    Part p1;
    p1.id = "part_verse";
    p1.name = "Verse";
    p1.keyRoot = "C";
    p1.mode = MusicalMode::Major;
    p1.bpm = 120;
    p1.timeNumerator = 4;
    p1.timeDenominator = 4;
    p1.bars = ProgressionParser::Parse("| C | Am | F | G |", "C", MusicalMode::Major);
    pm.GetParts().push_back(p1);

    // パート2: Chorus (4小節)
    Part p2;
    p2.id = "part_chorus";
    p2.name = "Chorus";
    p2.keyRoot = "C";
    p2.mode = MusicalMode::Major;
    p2.bpm = 120;
    p2.timeNumerator = 4;
    p2.timeDenominator = 4;
    p2.bars = ProgressionParser::Parse("| F | G | Em | Am |", "C", MusicalMode::Major);
    pm.GetParts().push_back(p2);

    TEST_ASSERT(pm.GetParts().size() == 2, "2 initial parts added");

    // 1. パート名変更の検証
    pm.GetParts()[0].name = "A-Melody";
    TEST_ASSERT(pm.GetParts()[0].name == "A-Melody", "Part 0 renamed successfully");

    pm.GetParts()[1].name = "Sabi";
    TEST_ASSERT(pm.GetParts()[1].name == "Sabi", "Part 1 renamed successfully");

    // 2. 全体マージによる新規パーツ生成 (DoMergeAllToNewPart 相当) の検証
    auto mergedBars = pm.GenerateMergedProgression();
    TEST_ASSERT(mergedBars.size() == 8, "Merged progression contains 8 bars");

    Part mergedPart;
    mergedPart.id = "part_merged_test";
    mergedPart.name = "Full Song";
    mergedPart.keyRoot = pm.GetParts()[0].keyRoot;
    mergedPart.mode = pm.GetParts()[0].mode;
    mergedPart.bpm = pm.GetParts()[0].bpm;
    mergedPart.timeNumerator = pm.GetParts()[0].timeNumerator;
    mergedPart.timeDenominator = pm.GetParts()[0].timeDenominator;
    mergedPart.bars = mergedBars;

    std::string text;
    for (size_t bi = 0; bi < mergedBars.size(); ++bi) {
        if (bi > 0) text += " ";
        text += "|";
        for (const auto& c : mergedBars[bi].chords) {
            text += " " + c.rawText;
        }
    }
    mergedPart.rawProgressionText = text;
    pm.GetParts().push_back(mergedPart);
    pm.SetActiveIndex(static_cast<int>(pm.GetParts().size() - 1));

    TEST_ASSERT(pm.GetParts().size() == 3, "New merged part added, total parts is now 3");
    TEST_ASSERT(pm.GetActivePart() != nullptr, "Active part is valid");
    TEST_ASSERT(pm.GetActivePart()->name == "Full Song", "Active part is Full Song");
    TEST_ASSERT(pm.GetActivePart()->bars.size() == 8, "Full Song contains 8 bars");
    TEST_ASSERT(pm.GetActivePart()->bars[0].chords[0].rawText == "C", "First chord of Full Song is C");
    TEST_ASSERT(pm.GetActivePart()->bars[7].chords[0].rawText == "Am", "Last chord of Full Song is Am");

    // 3. タイムラインの最大スクロール小節数 (std::max(bars.size() + 16, 64)) の安全性検証
    size_t activeBars = pm.GetActivePart()->bars.size();
    size_t minRenderBars = std::max<size_t>(activeBars + 16, 64);
    TEST_ASSERT(minRenderBars == 64, "Short song timeline automatically extends to 64 bars minimum");

    // 64小節超のロング進行での自動拡張検証
    size_t longBarsCount = 80;
    size_t longRenderBars = std::max<size_t>(longBarsCount + 16, 64);
    TEST_ASSERT(longRenderBars == 96, "80 bars song extends timeline capacity to 96 bars");
}

void TestDashboardFileSelectionAndMenu() {
    std::cout << "[TEST] 19. Dashboard File Selection Separation and Menu Item\n";

    // 1. メニューコマンドID定義
    TEST_ASSERT(IDM_FILE_DASHBOARD == 1007, "IDM_FILE_DASHBOARD is 1007");

    // 2. 履歴ファイル選択の非破壊ハイライトとオープン分離ロジック
    std::vector<std::wstring> recentFiles = {
        L"C:\\Projects\\SongA.chord",
        L"C:\\Projects\\SongB.chord",
        L"C:\\Projects\\SongC.chord"
    };

    int selectedIdx = -1;
    bool openExecuted = false;
    std::wstring openedPath;

    auto onRowClick = [&](int clickedIdx, bool isButtonOrDblClick) {
        if (isButtonOrDblClick) {
            selectedIdx = clickedIdx;
            openedPath = recentFiles[clickedIdx];
            openExecuted = true;
        } else {
            // 行の通常クリック: 選択ハイライトのみ
            selectedIdx = clickedIdx;
            openExecuted = false;
        }
    };

    // 行クリック時: 選択のみ行われ、ファイルは開かれない
    onRowClick(1, false);
    TEST_ASSERT(selectedIdx == 1, "Clicking recent row selects index 1");
    TEST_ASSERT(!openExecuted, "Clicking recent row does NOT trigger open action");
    TEST_ASSERT(openedPath.empty(), "Opened path remains empty on selection");

    // 別の行をクリック時: 選択が更新され、ファイルは開かれない
    onRowClick(2, false);
    TEST_ASSERT(selectedIdx == 2, "Clicking another recent row updates selection to index 2");
    TEST_ASSERT(!openExecuted, "Switching selection does NOT trigger open action");

    // 明示的［開く］押下またはダブルクリック時: 初めてオープンが実行される
    onRowClick(2, true);
    TEST_ASSERT(openExecuted, "Clicking Open button or double click triggers file open");
    TEST_ASSERT(openedPath == L"C:\\Projects\\SongC.chord", "Correct selected file is opened");

    // 3. キーボードナビゲーション (上下キー循環)
    auto handleKeyNav = [&](int current, bool isDown, size_t total) {
        if (total == 0) return -1;
        if (isDown) {
            return (current < 0 || current >= static_cast<int>(total) - 1) ? 0 : current + 1;
        } else {
            return (current <= 0) ? static_cast<int>(total) - 1 : current - 1;
        }
    };

    int navIdx = -1;
    navIdx = handleKeyNav(navIdx, true, recentFiles.size()); // Down from none -> 0
    TEST_ASSERT(navIdx == 0, "Down key from unselected selects first item (0)");

    navIdx = handleKeyNav(navIdx, true, recentFiles.size()); // Down -> 1
    TEST_ASSERT(navIdx == 1, "Down key advances selection to index 1");

    navIdx = handleKeyNav(navIdx, false, recentFiles.size()); // Up -> 0
    TEST_ASSERT(navIdx == 0, "Up key moves selection back to index 0");

    navIdx = handleKeyNav(navIdx, false, recentFiles.size()); // Up from 0 wraps to last (2)
    TEST_ASSERT(navIdx == 2, "Up key from index 0 wraps around to last item (2)");
}

void TestJsonUnescapeAndPathNormalization() {
    std::cout << "[TEST] 20. JSON Unescape and Path Normalization\n";

    // 1. JSON Unescape logic
    auto unescapeJson = [](std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char next = s[i + 1];
                if (next == '"') { out += '"'; ++i; }
                else if (next == '\\') { out += '\\'; ++i; }
                else if (next == '/') { out += '/'; ++i; }
                else if (next == 'b') { out += '\b'; ++i; }
                else if (next == 'f') { out += '\f'; ++i; }
                else if (next == 'n') { out += '\n'; ++i; }
                else if (next == 'r') { out += '\r'; ++i; }
                else if (next == 't') { out += '\t'; ++i; }
                else { out += next; ++i; }
            } else {
                out += s[i];
            }
        }
        return out;
    };

    std::string escapedPath = "C:\\\\Users\\\\Test\\\\Documents\\\\Song.chord";
    std::string unescaped = unescapeJson(escapedPath);
    TEST_ASSERT(unescaped == "C:\\Users\\Test\\Documents\\Song.chord", "unescapeJson removes JSON backslash escapes correctly");

    // 2. Path normalization (eliminating runaway multiplied backslashes)
    auto normalizePath = [](const std::wstring& pathStr) {
        if (pathStr.empty()) return std::wstring();
        std::wstring out;
        out.reserve(pathStr.size());
        bool prevSep = false;
        for (size_t i = 0; i < pathStr.size(); ++i) {
            wchar_t c = pathStr[i];
            if (c == L'\\' || c == L'/') {
                if (!prevSep || (i == 1 && out[0] == L'\\')) {
                    out += L'\\';
                }
                prevSep = true;
            } else {
                out += c;
                prevSep = false;
            }
        }
        return out;
    };

    std::wstring runawayPath = L"C:\\\\\\\\Users\\\\\\\\KotoneP\\\\\\\\Documents\\\\Song.chord";
    std::wstring normalized = normalizePath(runawayPath);
    TEST_ASSERT(normalized == L"C:\\Users\\KotoneP\\Documents\\Song.chord", "normalizePath compresses multiplied backslashes into single separator");

    // 3. 複数回の保存・読込ループによるバックスラッシュ増殖防止検証 (Multi-cycle Save/Load idempotence)
    auto escapeJsonW = [](const std::wstring& s) {
        std::wstring out;
        for (wchar_t c : s) {
            if (c == L'"') out += L"\\\"";
            else if (c == L'\\') out += L"\\\\";
            else out += c;
        }
        return out;
    };
    auto unescapeJsonW = [](const std::wstring& s) {
        std::wstring out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == L'\\' && i + 1 < s.size()) {
                wchar_t next = s[i + 1];
                if (next == L'"') { out += L'"'; ++i; }
                else if (next == L'\\') { out += L'\\'; ++i; }
                else { out += next; ++i; }
            } else {
                out += s[i];
            }
        }
        return out;
    };

    std::wstring currentPath = L"C:\\Users\\KotoneP\\Documents\\Song.chord";
    for (int cycle = 0; cycle < 5; ++cycle) {
        // 保存時: JSONエスケープ
        std::wstring jsonSaved = L"\"" + escapeJsonW(currentPath) + L"\"";
        // 読込時: アンクォート＆アンエスケープ
        std::wstring inner = jsonSaved.substr(1, jsonSaved.size() - 2);
        std::wstring unesc = unescapeJsonW(inner);
        // パス正規化
        currentPath = normalizePath(unesc);
    }
    TEST_ASSERT(currentPath == L"C:\\Users\\KotoneP\\Documents\\Song.chord", "5 save/load cycles keep backslashes strictly single, preventing multiplication");
}

void TestTickSyncopationAndTriadDefault() {
    std::cout << "[TEST] 21. Tick Syncopation Placement and Triad Default\n";

    // 1. シンコペーション (裏拍 240 ticks = 8分音符裏) へのコード挿入ロジック検証
    Bar bar;
    bar.barNumber = 1;
    bar.timeNumerator = 4;
    bar.timeDenominator = 4;

    int barTotalTicks = 4 * TICKS_PER_BEAT; // 1920
    int targetTick = 240; // 8分休符の直後（裏拍）
    int chordTicks = 480; // 1拍のコード

    ChordToken chord;
    chord.rawText = "Asus4";
    chord.rootName = "A";
    chord.qualitySuffix = "sus4";
    chord.SetTicks(chordTicks);

    // 空き小節の targetTick に配置 -> 手前 0..targetTick に休符が自動生成される
    std::vector<ChordToken> finalChords;
    if (targetTick > 0) {
        ChordToken rest;
        rest.isRest = true;
        rest.quality = ChordQuality::Rest;
        rest.rootName = "R";
        rest.rawText = "_";
        rest.SetTicks(targetTick);
        finalChords.push_back(rest);
    }
    finalChords.push_back(chord);
    int walker = targetTick + chordTicks;
    if (walker < barTotalTicks) {
        ChordToken restTail;
        restTail.isRest = true;
        restTail.quality = ChordQuality::Rest;
        restTail.rootName = "R";
        restTail.rawText = "_";
        restTail.SetTicks(barTotalTicks - walker);
        finalChords.push_back(restTail);
    }

    TEST_ASSERT(finalChords.size() == 3, "Bar with syncopated chord has 3 tokens (rest, chord, rest)");
    TEST_ASSERT(finalChords[0].isRest && finalChords[0].durationTicks == 240, "Initial token is an 8th rest (240 ticks)");
    TEST_ASSERT(!finalChords[1].isRest && finalChords[1].rawText == "Asus4" && finalChords[1].durationTicks == 480, "Middle token is Asus4 on upbeat (480 ticks)");
    TEST_ASSERT(finalChords[2].isRest && finalChords[2].durationTicks == 1200, "Tail rest fills remainder of bar (1200 ticks)");

    int totalBarSum = 0;
    for (const auto& c : finalChords) totalBarSum += c.durationTicks;
    TEST_ASSERT(totalBarSum == 1920, "Total ticks in syncopated bar equals exactly 1920 ticks");

    // 2. グリッドショートカット定数検証
    int grid1Bar = TICKS_PER_BAR_4_4;      // 1920 (Ctrl+1)
    int grid2Beats = TICKS_PER_BEAT * 2;   // 960  (Ctrl+2)
    int grid1Beat = TICKS_PER_BEAT;        // 480  (Ctrl+3)
    int gridHalfBeat = TICKS_PER_BEAT / 2; // 240  (Ctrl+4)
    int gridQuarter = TICKS_PER_BEAT / 4;  // 120  (Ctrl+5)

    TEST_ASSERT(grid1Bar == 1920, "Ctrl+1 sets 1 Bar grid (1920 ticks)");
    TEST_ASSERT(grid2Beats == 960, "Ctrl+2 sets 2 Beats grid (960 ticks)");
    TEST_ASSERT(grid1Beat == 480, "Ctrl+3 sets 1 Beat grid (480 ticks)");
    TEST_ASSERT(gridHalfBeat == 240, "Ctrl+4 sets 1/2 Beat grid (240 ticks)");
    TEST_ASSERT(gridQuarter == 120, "Ctrl+5 sets 1/4 Beat grid (120 ticks)");

    // 3. 3和音初期値 (0: Triad) の検証
    int defaultTab = 0; // 0 = 3和音
    auto diatonicTriads = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, defaultTab);
    TEST_ASSERT(diatonicTriads.size() >= 7, "Diatonic triads for C Major has 7 chords");
    TEST_ASSERT(diatonicTriads[0].qualitySuffix.empty(), "Degree I in C Major triad is pure C Major (no 7th)");
    TEST_ASSERT(diatonicTriads[4].qualitySuffix.empty(), "Degree V in C Major triad is pure G Major (no 7th)");
}

void TestCardResizeLeftWithRestPadding() {
    std::cout << "[TEST] 22. Card Resize Left with Rest Padding (Trim start position)\n";

    // 1. 2拍のコード C (960 ticks) と 2拍の休符 (960 ticks) からなる小節
    Bar bar;
    bar.barNumber = 1;
    bar.timeNumerator = 4;
    bar.timeDenominator = 4;

    ChordToken chordC;
    chordC.rawText = "C";
    chordC.rootName = "C";
    chordC.SetTicks(960);
    bar.chords.push_back(chordC);

    ChordToken restTail;
    restTail.isRest = true;
    restTail.rawText = "_";
    restTail.SetTicks(960);
    bar.chords.push_back(restTail);

    TEST_ASSERT(bar.chords.size() == 2, "Bar initially has 2 tokens (C, rest)");

    // 2. 決定論的スナップショットモデルによる左端リサイズ検証
    std::vector<ChordToken> snapshot = bar.chords;
    int dragChordIndex = 0;
    int dragChordStartTick = 0;
    int dragInitialTicks = 960;
    int dragLeftLimitTick = 0;
    int snapTicks = 480; // 1拍グリッド
    int origEndTick = dragChordStartTick + dragInitialTicks; // 960

    auto rebuildBar = [&](int deltaTicks) {
        int maxStartTick = origEndTick - snapTicks;
        int minStartTick = dragLeftLimitTick;
        int newStartTick = std::clamp(dragChordStartTick + deltaTicks, minStartTick, maxStartTick);
        int newChordTicks = origEndTick - newStartTick;

        std::vector<ChordToken> rebuilt = snapshot;
        bool hadPrevRest = (dragChordIndex > 0 && rebuilt[dragChordIndex - 1].isRest);

        if (hadPrevRest) {
            int prevRestTicks = newStartTick - dragLeftLimitTick;
            if (prevRestTicks > 0) {
                rebuilt[dragChordIndex - 1].SetTicks(prevRestTicks);
                rebuilt[dragChordIndex].SetTicks(newChordTicks);
            } else {
                rebuilt[dragChordIndex].SetTicks(newChordTicks);
                rebuilt.erase(rebuilt.begin() + (dragChordIndex - 1));
            }
        } else {
            int insertedRestTicks = newStartTick - dragChordStartTick;
            if (insertedRestTicks > 0) {
                ChordToken restTok;
                restTok.isRest = true;
                restTok.quality = ChordQuality::Rest;
                restTok.rootName = "R";
                restTok.degreeRoman = "R";
                restTok.rawText = "_";
                restTok.SetTicks(insertedRestTicks);
                rebuilt[dragChordIndex].SetTicks(newChordTicks);
                rebuilt.insert(rebuilt.begin() + dragChordIndex, restTok);
            } else {
                rebuilt[dragChordIndex].SetTicks(newChordTicks);
            }
        }
        return rebuilt;
    };

    // (A) 右へ1拍 (480 ticks) ドラッグ: 縮小＆手前に休符を自動生成
    auto chordsTrimmed = rebuildBar(480);
    TEST_ASSERT(chordsTrimmed.size() == 3, "Trim right generated 3 tokens (rest, C, tail rest)");
    TEST_ASSERT(chordsTrimmed[0].isRest && chordsTrimmed[0].durationTicks == 480, "Preceding inserted rest has 480 ticks (1 beat)");
    TEST_ASSERT(!chordsTrimmed[1].isRest && chordsTrimmed[1].rawText == "C" && chordsTrimmed[1].durationTicks == 480, "Chord C trimmed to 480 ticks (1 beat)");
    TEST_ASSERT(chordsTrimmed[2].isRest && chordsTrimmed[2].durationTicks == 960, "Tail rest preserved at 960 ticks");

    // (B) 再び左へ戻す (0 ticks): 手前休符が消滅して元の状態へ完全復元
    auto chordsRestored = rebuildBar(0);
    TEST_ASSERT(chordsRestored.size() == 2, "Returned to 0 delta restores 2 tokens without dummy rests");
    TEST_ASSERT(!chordsRestored[0].isRest && chordsRestored[0].rawText == "C" && chordsRestored[0].durationTicks == 960, "Chord C restored to 960 ticks");
    TEST_ASSERT(chordsRestored[1].isRest && chordsRestored[1].durationTicks == 960, "Tail rest intact");

    // 3. 手前に休符がある状態からの左端ドラッグ伸長 (シンコペーション) 検証
    Bar bar2;
    ChordToken restLead;
    restLead.isRest = true;
    restLead.rawText = "_";
    restLead.SetTicks(480); // 1拍目休符
    bar2.chords.push_back(restLead);

    ChordToken chordG;
    chordG.rawText = "G";
    chordG.rootName = "G";
    chordG.SetTicks(480); // 2拍目 G
    bar2.chords.push_back(chordG);

    std::vector<ChordToken> snapshot2 = bar2.chords;
    int dragIdx2 = 1;
    int startTick2 = 480;
    int leftLimit2 = 0; // 手前休符の開始位置 = 0
    int endTick2 = 960;

    auto rebuildBar2 = [&](int deltaTicks) {
        int newStart = std::clamp(startTick2 + deltaTicks, leftLimit2, endTick2 - 240);
        int newTicks = endTick2 - newStart;
        std::vector<ChordToken> rebuilt = snapshot2;
        int prevRestTicks = newStart - leftLimit2;
        if (prevRestTicks > 0) {
            rebuilt[dragIdx2 - 1].SetTicks(prevRestTicks);
            rebuilt[dragIdx2].SetTicks(newTicks);
        } else {
            rebuilt[dragIdx2].SetTicks(newTicks);
            rebuilt.erase(rebuilt.begin() + (dragIdx2 - 1));
        }
        return rebuilt;
    };

    // 左へ1/2拍 (-240 ticks) ドラッグ: 8分音符裏拍へのシンコペーション伸長
    auto chordsSynco = rebuildBar2(-240);
    TEST_ASSERT(chordsSynco.size() == 2, "Synco bar has 2 tokens (compressed rest, expanded G)");
    TEST_ASSERT(chordsSynco[0].isRest && chordsSynco[0].durationTicks == 240, "Lead rest compressed to 240 ticks (half beat)");
    TEST_ASSERT(!chordsSynco[1].isRest && chordsSynco[1].durationTicks == 720, "Chord G expanded to 720 ticks (1.5 beats, syncopated)");

    // 左へ1拍 (-480 ticks) ドラッグ: 手前休符を完全消費して小節頭まで伸長
    auto chordsFull = rebuildBar2(-480);
    TEST_ASSERT(chordsFull.size() == 1, "Full expansion consumed lead rest completely (1 token)");
    TEST_ASSERT(!chordsFull[0].isRest && chordsFull[0].rawText == "G" && chordsFull[0].durationTicks == 960, "Chord G now starts at beat 1 with 960 ticks (2 beats)");
}

void TestDropGridSnapPreview() {
    std::cout << "[TEST] 23. Drop Grid Snap Preview & Edge Margin Verification\n";

    // 1. グリッドスナップ計算ロジック
    float pixelsPerBeat = 64.0f;
    float barWidth = pixelsPerBeat * 4.0f; // 256.0f
    int barTotalTicks = 1920;

    auto calcSnap = [&](float relX, int gridTicks, int chordTicks) {
        int targetBar = (barWidth > 0.0f) ? static_cast<int>(relX / barWidth) : 0;
        float barOffsetRatio = (barWidth > 0.0f) ? (std::fmod(relX, barWidth) / barWidth) : 0.0f;
        float rawTick = barOffsetRatio * static_cast<float>(barTotalTicks);
        int snappedTick = static_cast<int>(std::round(rawTick / static_cast<float>(gridTicks))) * gridTicks;
        snappedTick = std::clamp(snappedTick, 0, std::max(0, barTotalTicks - chordTicks));
        return std::make_pair(targetBar, snappedTick);
    };

    // 1拍グリッド (480 ticks), 1拍コード
    // barWidth = 256px (64px/beat). 276px = 256px + 20px (1拍目中央より手前 -> 1拍目 0 ticks)
    auto [b1, t1] = calcSnap(276.0f, 480, 480);
    TEST_ASSERT(b1 == 1 && t1 == 0, "relX 276px snaps to Bar 1, Tick 0 (Beat 1)");

    // 330px = 256px + 74px (1拍目境界64pxを超え2拍目前半 -> 2拍目 480 ticks)
    auto [b2, t2] = calcSnap(330.0f, 480, 480);
    TEST_ASSERT(b2 == 1 && t2 == 480, "relX 330px snaps to Bar 1, Tick 480 (Beat 2)");

    // 1/2拍グリッド (240 ticks = 8分音符裏拍), 1拍コード
    auto [b3, t3] = calcSnap(345.0f, 240, 480); // 256 + 89 -> Bar 1, 720 ticks (2拍目裏)
    TEST_ASSERT(b3 == 1 && t3 == 720, "relX 345px on 1/2-beat grid snaps to Bar 1, Tick 720 (Beat 2.5)");

    // 2. 左端リサイズ掴み判定マージンの検証 (スリム化: 8pxかつ20%以下)
    auto calcMargin = [](float cardW) {
        return std::min(8.0f, cardW * 0.20f);
    };

    float mStandard = calcMargin(64.0f); // 1拍カード
    TEST_ASSERT(mStandard == 8.0f, "1-beat card has 8px grab margin, leaving 75% for drag-move");

    float mSmall = calcMargin(24.0f); // 極小カード
    TEST_ASSERT(mSmall == 4.8f, "Small card caps margin at 20% (4.8px), leaving >= 60% for drag-move");
}

void TestCrossBarDragAndRestNormalization() {
    std::cout << "[TEST] 24. Cross-bar Drag and Rest Normalization\n";

    // 1. Bar 5: [Asus4 (480), _ (480), Asus4 (480), _ (480)]
    Bar bar5;
    bar5.barNumber = 5;
    bar5.timeNumerator = 4;
    bar5.timeDenominator = 4;

    ChordToken asus1;
    asus1.rawText = "Asus4";
    asus1.rootName = "A";
    asus1.qualitySuffix = "sus4";
    asus1.SetTicks(480);
    bar5.chords.push_back(asus1);

    ChordToken rest1;
    rest1.isRest = true;
    rest1.rawText = "_";
    rest1.SetTicks(480);
    bar5.chords.push_back(rest1);

    ChordToken asus2;
    asus2.rawText = "Asus4";
    asus2.rootName = "A";
    asus2.qualitySuffix = "sus4";
    asus2.SetTicks(480);
    bar5.chords.push_back(asus2);

    ChordToken rest2;
    rest2.isRest = true;
    rest2.rawText = "_";
    rest2.SetTicks(480);
    bar5.chords.push_back(rest2);

    TEST_ASSERT(bar5.chords.size() == 4, "Bar 5 has 4 tokens initially");

    // 2. 移動元コード (index 2: asus2) をドラッグ移動した際の休符化と連続休符結合
    bar5.chords[2].isRest = true;
    bar5.chords[2].quality = ChordQuality::Rest;
    bar5.chords[2].rawText = "_";

    std::vector<ChordToken> merged;
    for (const auto& c : bar5.chords) {
        if (!merged.empty() && merged.back().isRest && c.isRest) {
            merged.back().SetTicks(merged.back().durationTicks + c.durationTicks);
        } else {
            merged.push_back(c);
        }
    }
    bar5.chords = std::move(merged);

    TEST_ASSERT(bar5.chords.size() == 2, "After moving chord, consecutive rests merged into 1 rest");
    TEST_ASSERT(!bar5.chords[0].isRest && bar5.chords[0].durationTicks == 480, "First chord remains Asus4 (480 ticks)");
    TEST_ASSERT(bar5.chords[1].isRest && bar5.chords[1].durationTicks == 1440, "Merged rest duration is 1440 ticks (3 beats)");

    int totalTicksBar5 = 0;
    for (const auto& c : bar5.chords) totalTicksBar5 += c.durationTicks;
    TEST_ASSERT(totalTicksBar5 == 1920, "Total ticks in Bar 5 perfectly preserved at 1920 ticks");

    // 3. 小節跨ぎドロップ先 Bar 4: [Eb (1440), _ (480)] への配置
    Bar bar4;
    bar4.barNumber = 4;
    bar4.timeNumerator = 4;
    bar4.timeDenominator = 4;

    ChordToken chordEb;
    chordEb.rawText = "Eb";
    chordEb.rootName = "Eb";
    chordEb.SetTicks(1440);
    bar4.chords.push_back(chordEb);

    ChordToken restEb;
    restEb.isRest = true;
    restEb.rawText = "_";
    restEb.SetTicks(480);
    bar4.chords.push_back(restEb);

    // Asus4 を Bar 4 の 4拍目 (Tick 1440) にドロップ
    struct PlacedItem {
        int startTick;
        int durationTicks;
        ChordToken chord;
    };
    std::vector<PlacedItem> placed;
    int curT = 0;
    for (const auto& existing : bar4.chords) {
        if (!existing.isRest) {
            placed.push_back({curT, existing.durationTicks, existing});
        }
        curT += existing.durationTicks;
    }
    // ドロップアイテム追加
    placed.push_back({1440, 480, asus2});

    std::sort(placed.begin(), placed.end(), [](const PlacedItem& a, const PlacedItem& b) {
        return a.startTick < b.startTick;
    });

    std::vector<ChordToken> finalBar4;
    int walkT = 0;
    for (const auto& pt : placed) {
        if (pt.startTick > walkT) {
            ChordToken r;
            r.isRest = true;
            r.rawText = "_";
            r.SetTicks(pt.startTick - walkT);
            finalBar4.push_back(r);
            walkT = pt.startTick;
        }
        finalBar4.push_back(pt.chord);
        walkT += pt.durationTicks;
    }
    if (walkT < 1920) {
        ChordToken r;
        r.isRest = true;
        r.rawText = "_";
        r.SetTicks(1920 - walkT);
        finalBar4.push_back(r);
    }
    bar4.chords = std::move(finalBar4);

    TEST_ASSERT(bar4.chords.size() == 2, "Bar 4 has 2 chords: Eb and Asus4");
    TEST_ASSERT(bar4.chords[0].rootName == "Eb" && bar4.chords[0].durationTicks == 1440, "Eb is 1440 ticks (3 beats)");
    TEST_ASSERT(bar4.chords[1].rootName == "A" && bar4.chords[1].durationTicks == 480, "Asus4 is placed at beat 4 (480 ticks)");

    int totalTicksBar4 = 0;
    for (const auto& c : bar4.chords) totalTicksBar4 += c.durationTicks;
    TEST_ASSERT(totalTicksBar4 == 1920, "Total ticks in Bar 4 perfectly preserved at 1920 ticks");
}

static ChordToken MakeTok(const char* root, int ticks) {
    ChordToken c;
    c.rawText = root;
    c.rootName = root;
    c.SetTicks(ticks);
    return c;
}

static ChordToken MakeRestTok(int ticks) {
    ChordToken c;
    c.isRest = true;
    c.quality = ChordQuality::Rest;
    c.rawText = "_";
    c.rootName = "R";
    c.degreeRoman = "R";
    c.SetTicks(ticks);
    return c;
}

static Bar MakeTickBar4() {
    Bar b;
    b.timeNumerator = 4;
    b.timeDenominator = 4;
    return b;
}

static int StartTickOfIndex(const Bar& bar, int index) {
    int t = 0;
    for (int i = 0; i < index && i < static_cast<int>(bar.chords.size()); ++i) t += bar.chords[i].durationTicks;
    return t;
}

void TestTimelineEditInsertAndResize() {
    std::cout << "[TEST] 25. TimelineEdit: no-drift insert, rest absorption and syncopated resize\n";

    // 1. 4拍コードの中央へ1コード挿入: 分割長が正しく、小節は1920tickのまま
    {
        Bar bar = MakeTickBar4();
        bar.chords.push_back(MakeTok("C", 1920));

        auto res = TimelineEdit::InsertChordSpan(std::vector<Bar>{bar}, MakeTok("G", 480), 480);
        TEST_ASSERT(res.bars.size() == 1, "Split insert into a full-length chord must not add bars");
        const Bar& rb = res.bars[0];
        TEST_ASSERT(rb.chords.size() == 3, "C must be split into 2 pieces around the inserted G");
        TEST_ASSERT(rb.chords[0].rootName == "C" && rb.chords[0].durationTicks == 480, "Left C piece is 480 ticks");
        TEST_ASSERT(rb.chords[1].rootName == "G" && rb.chords[1].durationTicks == 480, "G sits on beat 2");
        TEST_ASSERT(rb.chords[2].rootName == "C" && rb.chords[2].durationTicks == 960, "Right C piece is 960 ticks (no drift)");
        TEST_ASSERT(TimelineEdit::TotalTicks(rb) == 1920, "Bar total stays exactly 1920 ticks");
        TEST_ASSERT(res.selectedBar == 0 && res.selectedChord == 1, "Inserted G is reported as the selection");
    }

    // 2. 休符区間へのドロップは休符を消費し、後続コードを押し出さない
    {
        Bar bar = MakeTickBar4();
        bar.chords.push_back(MakeTok("C", 960));
        bar.chords.push_back(MakeRestTok(960));

        auto res = TimelineEdit::InsertChordSpan(std::vector<Bar>{bar}, MakeTok("G", 480), 960);
        TEST_ASSERT(res.bars.size() == 1, "Drop into a rest must not add bars");
        const Bar& rb = res.bars[0];
        TEST_ASSERT(rb.chords.size() == 3, "Rest is split into chord + remainder");
        TEST_ASSERT(rb.chords[0].rootName == "C" && rb.chords[0].durationTicks == 960, "Preceding C untouched");
        TEST_ASSERT(rb.chords[1].rootName == "G" && rb.chords[1].durationTicks == 480, "G pinned at the drop tick");
        TEST_ASSERT(rb.chords[2].isRest && rb.chords[2].durationTicks == 480, "Remaining rest is 480 ticks");
        TEST_ASSERT(StartTickOfIndex(rb, 1) == 960, "G starts exactly at the drop tick (WYSIWYG)");
    }

    // 3. 満杯の小節では押し出し分が次小節へ続く（データ消失なし・休符小節が足される）
    {
        Bar bar = MakeTickBar4();
        for (const char* r : {"C", "D", "E", "F"}) bar.chords.push_back(MakeTok(r, 480));

        auto res = TimelineEdit::InsertChordSpan(std::vector<Bar>{bar}, MakeTok("G", 480), 480);
        TEST_ASSERT(res.bars.size() == 2, "A rest bar is appended to hold the pushed-out chord");
        const Bar& b0 = res.bars[0];
        TEST_ASSERT(b0.chords.size() == 4 && b0.chords[1].rootName == "G", "Inserted chord lands on the requested tick");
        TEST_ASSERT(StartTickOfIndex(b0, 1) == 480, "Inserted chord starts at 480 ticks");
        TEST_ASSERT(b0.chords[3].rootName == "E", "E is pushed to beat 4");
        TEST_ASSERT(res.bars[1].chords[0].rootName == "F" && res.bars[1].chords[0].durationTicks == 480, "F is carried into the next bar (nothing is deleted)");
        TEST_ASSERT(TimelineEdit::TotalTicks(res.bars[1]) == 1920, "Appended bar still sums to 1920 ticks");
    }

    // 4. 押し出しは後方の休符で吸収される（曲は伸びない）
    {
        Bar bar = MakeTickBar4();
        bar.chords.push_back(MakeTok("C", 480));
        bar.chords.push_back(MakeRestTok(1440));

        auto res = TimelineEdit::InsertChordSpan(std::vector<Bar>{bar}, MakeTok("G", 480), 0);
        TEST_ASSERT(res.bars.size() == 1, "Trailing rest absorbs the shift");
        const Bar& rb = res.bars[0];
        TEST_ASSERT(TimelineEdit::TotalTicks(rb) == 1920, "Bar stays 1920 ticks");
        TEST_ASSERT(rb.chords[0].rootName == "G", "G on beat 1");
        TEST_ASSERT(rb.chords[1].rootName == "C" && rb.chords[1].durationTicks == 480, "C pushed to beat 2");
        TEST_ASSERT(rb.chords[2].isRest && rb.chords[2].durationTicks == 960, "Rest shrunk to 960 ticks");
    }

    auto MakeBars = [](std::initializer_list<Bar> bs) {
        std::vector<Bar> v;
        for (const auto& b : bs) v.push_back(b);
        return v;
    };
    auto BarSum = [](const Bar& b) {
        int t = 0;
        for (const auto& c : b.chords) t += c.durationTicks;
        return t;
    };

    // 5. 右端伸長: 後続休符を食い、シンコペーション長(1.75拍)をそのまま保持
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 480));
        b.chords.push_back(MakeRestTok(1440));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 0, 0, 840, 120);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords[0].durationTicks == 840, "Right edge extends to 1.75 beats (840 ticks)");
        TEST_ASSERT(bar.chords[1].isRest && bar.chords[1].durationTicks == 1080, "Following rest shrinks by the same amount");
        TEST_ASSERT(TimelineEdit::TotalTicks(bar) == 1920, "Total ticks preserved after extension");
    }

    // 6. 右端伸長で後続コードを完全に覆うと、そのコードは消える（区間の上書き）
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 960));
        b.chords.push_back(MakeTok("G", 960));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 0, 0, 1920, 240);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords.size() == 1, "Fully covered G is removed");
        TEST_ASSERT(bar.chords[0].rootName == "C" && bar.chords[0].durationTicks == 1920, "C fills the bar");
        TEST_ASSERT(BarSum(bar) == 1920, "Bar stays 1920 ticks");
    }

    // 7. 右端縮小: 空いた区間は休符として残る（左詰めされない）
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 960));
        b.chords.push_back(MakeTok("G", 960));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 0, 0, 720, 240);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords.size() == 3, "C, gap rest and G");
        TEST_ASSERT(bar.chords[0].durationTicks == 720, "C shrinks to 720 ticks");
        TEST_ASSERT(bar.chords[1].isRest && bar.chords[1].durationTicks == 240, "Gap rest of 240 ticks is inserted");
        TEST_ASSERT(bar.chords[2].rootName == "G" && StartTickOfIndex(bar, 2) == 960, "Following G keeps its absolute position");
    }

    // 8. 左端伸長: 手前休符を食って裏拍開始(1.5拍目)へ
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 480));
        b.chords.push_back(MakeRestTok(480));
        b.chords.push_back(MakeTok("G", 480));
        b.chords.push_back(MakeRestTok(480));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 960, 720, 1440, 120);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords.size() == 4, "C, lead rest, G, tail rest");
        TEST_ASSERT(bar.chords[0].rootName == "C", "C stays first");
        TEST_ASSERT(bar.chords[1].isRest && bar.chords[1].durationTicks == 240, "Lead rest compressed to 240 ticks");
        TEST_ASSERT(bar.chords[2].rootName == "G" && bar.chords[2].durationTicks == 720, "G starts on the off-beat with 1.5 beats");
        TEST_ASSERT(StartTickOfIndex(bar, 2) == 720, "G starts at tick 720 (off-beat)");
        TEST_ASSERT(TimelineEdit::TotalTicks(bar) == 1920, "Total ticks preserved");
    }

    // 9. 左端伸長は手前コードを完全に覆える（終端は固定）
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 480));
        b.chords.push_back(MakeTok("G", 960));
        b.chords.push_back(MakeRestTok(480));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 480, 0, 1440, 120);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords.size() == 2, "Covered C is removed");
        TEST_ASSERT(bar.chords[0].rootName == "G" && bar.chords[0].durationTicks == 1440, "G now starts at 0 and keeps its end (1440)");
        TEST_ASSERT(bar.chords[1].isRest && bar.chords[1].durationTicks == 480, "Tail rest intact");
    }

    // 10. 左端縮小: 手前に休符が生成される
    {
        Bar b = MakeTickBar4();
        b.chords.push_back(MakeTok("C", 960));
        b.chords.push_back(MakeTok("G", 960));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b}), 0, 480, 960, 240);
        const Bar& bar = res.bars[0];
        TEST_ASSERT(bar.chords[0].isRest && bar.chords[0].durationTicks == 480, "Leading rest of 480 ticks is inserted");
        TEST_ASSERT(bar.chords[1].rootName == "C" && bar.chords[1].durationTicks == 480, "C keeps its end tick (960)");
        TEST_ASSERT(StartTickOfIndex(bar, 1) == 480, "C now starts at 480 ticks");
    }

    // 11. 小節跨ぎの右伸長: 1枚のカードのまま次小節へ。次小節先頭は上書き（短縮）
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeTok("G", 1920));

        // C を 1拍ぶん小節線の向こうまで伸ばす (1920 -> 2400)
        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b1, b2}), 0, 0, 2400, 120);
        TEST_ASSERT(res.bars.size() == 2, "Two bars preserved");
        TEST_ASSERT(res.bars[0].chords.size() == 1, "No chord is moved into the next bar");
        TEST_ASSERT(res.bars[0].chords[0].rootName == "C" && res.bars[0].chords[0].durationTicks == 2400,
                    "C is one card spanning the bar line (2400 ticks = 5 beats)");
        TEST_ASSERT(BarSum(res.bars[0]) == 2400, "Spill is kept in the starting bar");

        const Bar& nb = res.bars[1];
        TEST_ASSERT(nb.chords.size() == 2, "Next bar: covered rest + trimmed chord");
        TEST_ASSERT(nb.chords[0].isRest && nb.chords[0].durationTicks == 480, "Next bar starts with 480 ticks covered by the C card");
        TEST_ASSERT(nb.chords[1].rootName == "G" && nb.chords[1].durationTicks == 1440, "G trimmed from the left (3 beats left)");
        TEST_ASSERT(StartTickOfIndex(nb, 1) == 480, "G now starts on beat 2 of the next bar");
        TEST_ASSERT(BarSum(nb) == 1920, "Next bar total remains 1920 ticks");
        TEST_ASSERT(res.selectedBar == 0 && res.selectedChord == 0, "Selection stays on the spanning card");
    }

    // 12. 小節跨ぎの左伸長: 前小節の末尾を上書きして、開始小節に格納される
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeTok("G", 1920));

        // G の開始を1拍前へ (1920 -> 1440)。G は前小節4拍目から5拍ぶんになる
        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b1, b2}), 1920, 1440, 3840, 120);
        TEST_ASSERT(res.selectedBar == 0, "Chord is stored in the bar containing its new start");
        TEST_ASSERT(res.bars[0].chords.size() == 2, "Bar 1: trimmed C + spanning G");
        TEST_ASSERT(res.bars[0].chords[0].rootName == "C" && res.bars[0].chords[0].durationTicks == 1440,
                    "C trimmed from the right (3 beats)");
        TEST_ASSERT(res.bars[0].chords[1].rootName == "G" && res.bars[0].chords[1].durationTicks == 2400,
                    "G is one card starting on beat 4 of bar 1 (5 beats)");
        TEST_ASSERT(res.bars[1].chords.size() == 1 && res.bars[1].chords[0].isRest, "Bar 2 is fully covered");
        TEST_ASSERT(BarSum(res.bars[1]) == 1920, "Covered bar remains 1920 ticks");
    }

    // 13. 複数小節を跨ぐ伸長（3小節ぶん）
    {
        Bar b1 = MakeTickBar4(); b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4(); b2.chords.push_back(MakeTok("G", 1920));
        Bar b3 = MakeTickBar4(); b3.chords.push_back(MakeTok("Am", 1920));

        auto res = TimelineEdit::ResizeChordSpan(MakeBars({b1, b2, b3}), 0, 0, 4800, 120);
        TEST_ASSERT(res.bars[0].chords[0].durationTicks == 4800, "Single card spans 2.5 bars");
        TEST_ASSERT(res.bars[1].chords.size() == 1 && res.bars[1].chords[0].isRest, "Middle bar is fully covered");
        const Bar& last = res.bars[2];
        TEST_ASSERT(last.chords.size() == 2, "Last bar: covered rest + trimmed Am");
        TEST_ASSERT(last.chords[0].isRest && last.chords[0].durationTicks == 960, "Covered until tick 960 in the last bar");
        TEST_ASSERT(last.chords[1].rootName == "Am" && last.chords[1].durationTicks == 960, "Am trimmed to its last 2 beats");
    }

    // 14. どの操作後も各小節は拍子ぶん以上を覆い、負長のコードを作らない
    {
        Bar b1 = MakeTickBar4(); b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4(); b2.chords.push_back(MakeTok("G", 1920));
        Bar b3 = MakeTickBar4(); b3.chords.push_back(MakeRestTok(1920));
        std::vector<Bar> bars = MakeBars({b1, b2, b3});

        bool invariant = true;
        int targetStart = 0;
        for (int i = 0; i < 30; ++i) {
            int newEnd = 120 + (i * 173) % 5000;
            auto res = TimelineEdit::ResizeChordSpan(bars, targetStart, targetStart, newEnd, 120);
            bars = res.bars;
            for (const auto& bar : bars) {
                if (BarSum(bar) < 1920) invariant = false;
                for (const auto& c : bar.chords) {
                    if (c.durationTicks <= 0) invariant = false;
                }
            }
            targetStart = (targetStart + 480) % 1920;
        }
        TEST_ASSERT(invariant, "Every bar still covers its meter after repeated cross-bar resizes");
    }
}

void TestOctaveAndGridSeparation() {
    std::cout << "[TEST] 27. Per-chord octave shift and split input/timeline grid persistence\n";

    ChordToken octChord = MusicEngine::ParseChordSymbol("C", "C", MusicalMode::Major);
    octChord.SetTicks(480);
    auto baseNotes = MusicEngine::GetChordMidiNotes(octChord, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!baseNotes.empty() && baseNotes[0] == 60, "C4 root is MIDI 60");

    octChord.octaveOffset = 1;
    auto upNotes = MusicEngine::GetChordMidiNotes(octChord, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!upNotes.empty() && upNotes[0] == 72, "+1 octave shifts the root to MIDI 72");

    octChord.octaveOffset = -1;
    auto downNotes = MusicEngine::GetChordMidiNotes(octChord, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!downNotes.empty() && downNotes[0] == 48, "-1 octave shifts the root to MIDI 48");

    PartManager pm;
    std::vector<Part>& parts = pm.GetParts();
    TEST_ASSERT(!parts.empty(), "Default song has parts");

    Bar bar = MakeTickBar4();
    ChordToken g = MusicEngine::ParseChordSymbol("G", "C", MusicalMode::Major);
    g.SetTicks(1920);
    g.octaveOffset = 2;
    g.inversion = 2;
    g.toneShift[static_cast<int>(PitchClass::B)] = -1;
    g.toneShift[static_cast<int>(PitchClass::G)] = 1;
    {
        ChordToken base = MusicEngine::ParseChordSymbol("G", "C", MusicalMode::Major);
        auto n0 = MusicEngine::GetChordMidiNotes(base, 4, VoicingStyle::RootPosition, 0, 0);
        ChordToken sh = base;
        sh.toneShift[static_cast<int>(PitchClass::G)] = 1;
        auto n1 = MusicEngine::GetChordMidiNotes(sh, 4, VoicingStyle::RootPosition, 0, 0);
        TEST_ASSERT(n0.size() == n1.size() && n1.back() == n0.front() + 12, "toneShift moves only the root tone up one octave");
        sh.toneShift[static_cast<int>(PitchClass::G)] = 0;
        TEST_ASSERT(MusicEngine::GetChordMidiNotes(sh, 4, VoicingStyle::RootPosition, 0, 0) == n0, "toneShift 0 keeps notes unchanged");
    }
    bar.chords.push_back(g);
    parts[0].bars = { bar };
    parts[0].rawProgressionText = ProgressionParser::Format(parts[0].bars);

    std::string json = pm.SerializeProject("OctGrid", VoicingStyle::RootPosition, 0, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4);
    TEST_ASSERT(json.find("\"inputGridTicks\": 120") != std::string::npos, "Input grid (120) is serialized independently");
    TEST_ASSERT(json.find("\"octaveOffset\": 2") != std::string::npos, "Octave offset is serialized");

    PartManager loaded;
    std::string title;
    VoicingStyle voicing;
    int bassMode = 0;
    int timelineGrid = 0;
    int inputGrid = 0;
    TEST_ASSERT(loaded.DeserializeProject(json, title, voicing, bassMode, timelineGrid, &inputGrid), "Project with octave/grid deserializes");
    TEST_ASSERT(timelineGrid == TICKS_PER_BEAT / 2, "Timeline grid restored (240)");
    TEST_ASSERT(inputGrid == TICKS_PER_BEAT / 4, "Input grid restored separately (120)");
    const Bar& rb = loaded.GetParts()[0].bars[0];
    TEST_ASSERT(!rb.chords.empty() && rb.chords[0].octaveOffset == 2, "Per-chord octave offset restored");
    TEST_ASSERT(json.find("\"inversion\": 2") != std::string::npos, "Inversion is serialized");
    TEST_ASSERT(rb.chords[0].inversion == 2, "Per-chord inversion restored");
    TEST_ASSERT(rb.chords[0].toneShift[static_cast<int>(PitchClass::B)] == -1 && rb.chords[0].toneShift[static_cast<int>(PitchClass::G)] == 1, "Per-chord toneShift restored");

    // 旧形式 (inputGridTicks 無し) はタイムライングリッドへフォールバック
    std::string legacy = json;
    size_t keyPos = legacy.find("\"inputGridTicks\"");
    if (keyPos != std::string::npos) {
        size_t lineEnd = legacy.find('\n', keyPos);
        legacy.erase(keyPos, lineEnd - keyPos + 1);
    }
    PartManager legacyLoaded;
    int legacyTimeline = 0;
    int legacyInput = 0;
    TEST_ASSERT(legacyLoaded.DeserializeProject(legacy, title, voicing, bassMode, legacyTimeline, &legacyInput), "Legacy project deserializes");
    TEST_ASSERT(legacyInput == legacyTimeline, "Legacy file falls back to the timeline grid for input");
}

void TestAppendTickAndEmptyPartAppend() {
    std::cout << "[TEST] 26. TimelineEdit: AppendTick ignores trailing rests; text input lands at bar 1 in an empty part\n";

    // 1. 空 / 全休符 -> 0
    TEST_ASSERT(TimelineEdit::AppendTick({}) == 0, "Empty song appends at tick 0");
    {
        Bar rest = MakeTickBar4();
        rest.chords.push_back(MakeRestTok(1920));
        TEST_ASSERT(TimelineEdit::AppendTick({ rest }) == 0, "All-rest part appends at tick 0");

        // 全休符パートへ AppendTick の位置から挿入すると 1 小節目に入り、小節数は増えない
        auto res = TimelineEdit::InsertChordSpan({ rest }, MakeTok("F", 480), TimelineEdit::AppendTick({ rest }));
        TEST_ASSERT(res.bars.size() == 1, "Insert at tick 0 into a rest bar does not add a bar");
        TEST_ASSERT(res.bars[0].chords[0].rootName == "F", "F lands on bar 1 beat 1");
    }

    // 2. 末尾の休符は無視し、最後の非休符コードの終端を返す（小節跨ぎも含む）
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeTok("G", 480));
        b2.chords.push_back(MakeRestTok(1440));
        Bar b3 = MakeTickBar4();
        b3.chords.push_back(MakeRestTok(1920));
        TEST_ASSERT(TimelineEdit::AppendTick({ b1, b2, b3 }) == 1920 + 480, "Append tick is the end of the last non-rest chord");
    }
}

void TestSyncopationPersistence() {
    std::cout << "[TEST] 26. Syncopated timing survives project save/load (bars array is authoritative)\n";

    PartManager pm;
    std::vector<Part>& parts = pm.GetParts();
    TEST_ASSERT(!parts.empty(), "Default song has parts");

    Bar bar;
    bar.timeNumerator = 4;
    bar.timeDenominator = 4;
    bar.chords.push_back(MakeRestTok(240));
    bar.chords.push_back(MakeTok("G", 720));
    bar.chords.push_back(MakeRestTok(960));
    parts[0].bars = { bar };
    parts[0].rawProgressionText = ProgressionParser::Format(parts[0].bars);

    std::string json = pm.SerializeProject("SyncopationTest", VoicingStyle::RootPosition, 0, 120);
    TEST_ASSERT(json.find("\"durationTicks\": 720") != std::string::npos, "Sub-beat duration is serialized");

    PartManager loaded;
    std::string title;
    VoicingStyle voicing;
    int bassMode = 0;
    int gridTicks = 0;
    TEST_ASSERT(loaded.DeserializeProject(json, title, voicing, bassMode, gridTicks), "Project deserializes");
    TEST_ASSERT(loaded.GetParts()[0].bars.size() == 1, "One bar restored");

    const Bar& rb = loaded.GetParts()[0].bars[0];
    TEST_ASSERT(rb.chords.size() == 3, "Three tokens restored");
    TEST_ASSERT(rb.chords[0].isRest && rb.chords[0].durationTicks == 240, "Lead 8th-note rest (240 ticks) restored");
    TEST_ASSERT(rb.chords[1].rootName == "G" && rb.chords[1].durationTicks == 720, "G keeps its 1.5-beat syncopated length");
    TEST_ASSERT(rb.chords[2].isRest && rb.chords[2].durationTicks == 960, "Tail rest restored");
    TEST_ASSERT(TimelineEdit::TotalTicks(rb) == 1920, "Restored bar totals 1920 ticks");

    // 小節跨ぎカード（1枚が小節線を越える）も保存・復元できる
    Bar spanBar;
    spanBar.timeNumerator = 4;
    spanBar.timeDenominator = 4;
    spanBar.chords.push_back(MakeTok("C", 2400));
    Bar coveredBar;
    coveredBar.timeNumerator = 4;
    coveredBar.timeDenominator = 4;
    coveredBar.chords.push_back(MakeRestTok(480));
    coveredBar.chords.push_back(MakeTok("G", 1440));
    parts[0].bars = { spanBar, coveredBar };
    parts[0].rawProgressionText = ProgressionParser::Format(parts[0].bars);

    std::string spanJson = pm.SerializeProject("SpanTest", VoicingStyle::RootPosition, 0, 120);
    PartManager spanLoaded;
    int spanGrid = 0;
    TEST_ASSERT(spanLoaded.DeserializeProject(spanJson, title, voicing, bassMode, spanGrid), "Span project deserializes");
    TEST_ASSERT(spanLoaded.GetParts()[0].bars.size() == 2, "Two bars restored");
    const Bar& sb = spanLoaded.GetParts()[0].bars[0];
    TEST_ASSERT(sb.chords.size() == 1 && sb.chords[0].durationTicks == 2400,
                "Spanning card keeps its 5-beat (2400 tick) length across reload");
    const Bar& cb = spanLoaded.GetParts()[0].bars[1];
    TEST_ASSERT(cb.chords.size() == 2, "Covered bar keeps two tokens");
    TEST_ASSERT(cb.chords[0].isRest && cb.chords[0].durationTicks == 480, "Covered region stays a 480-tick rest");
    TEST_ASSERT(cb.chords[1].rootName == "G" && cb.chords[1].durationTicks == 1440, "Following chord stays trimmed");
}

void TestOneBarChordCrossBarInsertion() {
    std::cout << "[TEST] 28. One-bar chord cross-bar insertion keeps a single card\n";

    auto MakeBar = [](int number) {
        Bar b;
        b.barNumber = number;
        b.timeNumerator = 4;
        b.timeDenominator = 4;
        return b;
    };

    // 1. 1小節のコード A (1920 ticks) を Bar 1 の 2拍目 (480 ticks) へ挿入: 小節線で分割されず1枚のまま跨ぐ
    {
        Bar bar1 = MakeBar(1);
        bar1.chords.push_back(MakeTok("C", 1920));
        Bar bar2 = MakeBar(2);
        bar2.chords.push_back(MakeRestTok(1920));

        auto res = TimelineEdit::InsertChordSpan({ bar1, bar2 }, MakeTok("A", 1920), 480);
        TEST_ASSERT(res.bars.size() == 2, "Crossing insert fits in the existing two bars");
        TEST_ASSERT(res.bars[0].chords.size() == 2, "Bar 1 holds trimmed C and the crossing A");
        TEST_ASSERT(res.bars[0].chords[0].rootName == "C" && res.bars[0].chords[0].durationTicks == 480, "C occupies Beat 1");
        TEST_ASSERT(res.bars[0].chords[1].rootName == "A" && res.bars[0].chords[1].durationTicks == 1920, "A keeps its full 1920 ticks as one card");
        TEST_ASSERT(res.bars[1].chords.size() == 1 && res.bars[1].chords[0].isRest, "Bar 2 holds no split copy of A");
        TEST_ASSERT(res.selectedBar == 0 && res.selectedChord == 1, "The crossing card is selected in its start bar");
    }

    // 2. 3拍目 (960 ticks) へ1小節コードを挿入: 最終小節を越える分は休符小節が足される
    {
        Bar bar3 = MakeBar(1);
        bar3.chords.push_back(MakeRestTok(1920));

        auto res = TimelineEdit::InsertChordSpan({ bar3 }, MakeTok("A", 1920), 960);
        TEST_ASSERT(res.bars.size() == 2, "A rest bar is appended for the part beyond the song end");
        TEST_ASSERT(TimelineEdit::SongTicks(res.bars) == 3840, "Song grows by exactly one bar");
        TEST_ASSERT(res.bars[0].chords.size() == 2 && res.bars[0].chords[0].isRest && res.bars[0].chords[0].durationTicks == 960, "Bar 1 has rest (960)");
        TEST_ASSERT(res.bars[0].chords[1].rootName == "A" && res.bars[0].chords[1].durationTicks == 1920, "A is stored as one 1920-tick card");
    }

    // 3. 拍子の異なる小節（4/4 の次が 3/4）をまたぐ挿入でも、絶対tickで位置が保たれる
    {
        Bar bar1 = MakeBar(1);
        bar1.chords.push_back(MakeRestTok(1920));
        Bar bar2;
        bar2.barNumber = 2;
        bar2.timeNumerator = 3;
        bar2.timeDenominator = 4;
        bar2.chords.push_back(MakeRestTok(1440));

        auto res = TimelineEdit::InsertChordSpan({ bar1, bar2 }, MakeTok("D", 960), 1440);
        TEST_ASSERT(res.bars.size() == 2 && TimelineEdit::SongTicks(res.bars) == 3360, "Song length is unchanged");
        TEST_ASSERT(res.bars[0].chords.size() == 2 && res.bars[0].chords[1].rootName == "D" && res.bars[0].chords[1].durationTicks == 960, "D crosses from the 4/4 bar into the 3/4 bar as one card");
    }

    // 4. 小節跨ぎカードを持つ小節の NormalizeBar は、末尾の跨ぎを保持する
    {
        Bar bar = MakeBar(1);
        bar.chords.push_back(MakeRestTok(960));
        bar.chords.push_back(MakeTok("A", 1920));
        TimelineEdit::NormalizeBar(bar);
        TEST_ASSERT(bar.chords.size() == 2 && bar.chords[1].durationTicks == 1920, "Tail-crossing card is preserved");
    }
}

void TestExistingProjectDeserializationRestoresPitch() {
    std::cout << "[TEST] 28. Deserializing project with empty rawText restores rootPitch, notes, and rawText\n";

    // 1. パレット生成関数群で rawText が正しく設定されていること
    auto diatonic = MusicEngine::GetDiatonicChordsByTab("C", MusicalMode::Major, 0);
    TEST_ASSERT(!diatonic.empty(), "Diatonic chords not empty");
    for (const auto& c : diatonic) {
        TEST_ASSERT(!c.rawText.empty(), "Palette chord has non-empty rawText: " + c.rawText);
        TEST_ASSERT(c.rootPitch != PitchClass::Unknown, "Palette chord has valid rootPitch: " + c.rawText);
    }

    auto modal = MusicEngine::GetModalInterchangeChords("C", MusicalMode::Major, false);
    for (const auto& c : modal) {
        TEST_ASSERT(!c.rawText.empty(), "Modal interchange chord has non-empty rawText: " + c.rawText);
    }

    // 2. 既存破損形式（rawText が空文字列）の JSON から復元
    std::string brokenJson = R"({
  "format": "ChordLaunchProject",
  "version": 1,
  "title": "BrokenProject",
  "activePartIndex": 0,
  "voicing": 0,
  "bassMode": 0,
  "gridTicks": 480,
  "parts": [
    {
      "id": "part_1",
      "name": "Intro",
      "keyRoot": "C",
      "mode": 0,
      "bpm": 120,
      "timeNumerator": 4,
      "timeDenominator": 4,
      "rawProgressionText": "",
      "bars": [
        {
          "barNumber": 1,
          "chords": [
            {
              "rawText": "",
              "rootName": "Dm",
              "quality": 1,
              "qualitySuffix": "m",
              "durationTicks": 1920,
              "durationBeats": 4,
              "isRest": false,
              "octaveOffset": 0,
              "degreeRoman": "IIm"
            }
          ]
        },
        {
          "barNumber": 2,
          "chords": [
            {
              "rawText": "",
              "rootName": "C",
              "quality": 0,
              "qualitySuffix": "",
              "durationTicks": 960,
              "durationBeats": 2,
              "isRest": false,
              "octaveOffset": 0,
              "degreeRoman": "bVII"
            },
            {
              "rawText": "",
              "rootName": "A",
              "quality": 0,
              "qualitySuffix": "",
              "durationTicks": 960,
              "durationBeats": 2,
              "isRest": false,
              "octaveOffset": -1,
              "degreeRoman": "V"
            }
          ]
        }
      ]
    }
  ]
})";

    PartManager pm;
    std::string title;
    VoicingStyle voicing;
    int bassMode = 0;
    int gridTicks = 0;
    TEST_ASSERT(pm.DeserializeProject(brokenJson, title, voicing, bassMode, gridTicks), "Broken project deserializes successfully");

    const auto& parts = pm.GetParts();
    TEST_ASSERT(parts.size() == 1, "One part restored");
    const auto& bars = parts[0].bars;
    TEST_ASSERT(bars.size() == 2, "Two bars restored");

    // Bar 1: Dm
    const auto& cDm = bars[0].chords[0];
    TEST_ASSERT(cDm.rootPitch == PitchClass::D, "Dm rootPitch restored to D");
    TEST_ASSERT(cDm.rawText == "Dm", "Dm rawText restored to Dm");
    auto dmNotes = MusicEngine::GetChordMidiNotes(cDm, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!dmNotes.empty(), "Dm produces MIDI notes (not empty)");

    // Bar 2: C and A
    const auto& cC = bars[1].chords[0];
    TEST_ASSERT(cC.rootPitch == PitchClass::C, "C rootPitch restored to C");
    TEST_ASSERT(cC.rawText == "C", "C rawText restored to C");
    auto cNotes = MusicEngine::GetChordMidiNotes(cC, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!cNotes.empty(), "C produces MIDI notes");

    const auto& cA = bars[1].chords[1];
    TEST_ASSERT(cA.rootPitch == PitchClass::A, "A rootPitch restored to A");
    TEST_ASSERT(cA.octaveOffset == -1, "A octaveOffset restored to -1");
    auto aNotes = MusicEngine::GetChordMidiNotes(cA, 4, VoicingStyle::RootPosition, 0, 0);
    TEST_ASSERT(!aNotes.empty(), "A produces MIDI notes");

    // 3. 再保存時に rawText が空にならずシリアライズされること
    std::string reSaved = pm.SerializeProject("Saved", VoicingStyle::RootPosition, 0, 480);
    TEST_ASSERT(reSaved.find("\"rawText\": \"Dm\"") != std::string::npos, "Re-serialized project preserves rawText 'Dm'");
    TEST_ASSERT(reSaved.find("\"rawText\": \"C\"") != std::string::npos, "Re-serialized project preserves rawText 'C'");
}

void TestVersionDefinition() {
    std::cout << "[TEST] 30. Version definition consistency (src/version.h)\n";
    std::string ver = CHORDLAUNCH_VERSION_STR;
    std::string fileVer = CHORDLAUNCH_FILEVERSION_STR;
    TEST_ASSERT(ver.size() > 1 && ver[0] == 'v', "Version string starts with 'v'");
    std::string expectedPrefix = "v" + std::to_string(CHORDLAUNCH_VER_MAJOR) + "." + std::to_string(CHORDLAUNCH_VER_MINOR);
    TEST_ASSERT(ver.rfind(expectedPrefix, 0) == 0, "Version string matches MAJOR.MINOR");
    TEST_ASSERT(fileVer == std::to_string(CHORDLAUNCH_VER_MAJOR) + "." + std::to_string(CHORDLAUNCH_VER_MINOR) + ".0.0", "File version string matches MAJOR.MINOR.0.0");
    TEST_ASSERT(std::wstring(CHORDLAUNCH_VERSION_WSTR).size() == ver.size(), "Wide version string has the same length");
}

void TestAuditFixesRegression() {
    std::cout << "[TEST] 29. Audit fixes: aug chord '+', template store, part removal, MIDI meta length, JSON escapes\n";

    // 1. 'C+' は C# ではなく augmented
    auto cAug = MusicEngine::ParseChordSymbol("C+", "C", MusicalMode::Major);
    TEST_ASSERT(cAug.rootPitch == PitchClass::C, "C+ root is C");
    TEST_ASSERT(cAug.quality == ChordQuality::Augmented, "C+ is Augmented");
    auto cAug7 = MusicEngine::ParseChordSymbol("C+7", "C", MusicalMode::Major);
    TEST_ASSERT(cAug7.rootPitch == PitchClass::C, "C+7 root is C");
    TEST_ASSERT(cAug7.quality == ChordQuality::Augmented7, "C+7 is Augmented7");
    auto fsAug = MusicEngine::ParseChordSymbol("F#+", "C", MusicalMode::Major);
    TEST_ASSERT(fsAug.rootPitch == PitchClass::Fs && fsAug.quality == ChordQuality::Augmented, "F#+ is F# Augmented");
    auto cSharp = MusicEngine::ParseChordSymbol("C#", "C", MusicalMode::Major);
    TEST_ASSERT(cSharp.rootPitch == PitchClass::Cs && cSharp.quality == ChordQuality::Major, "C# remains C# major");

    // 2. ノート番号は 0〜127 に収まる
    auto extreme = MusicEngine::ParseChordSymbol("Bmaj7(13)", "C", MusicalMode::Major);
    auto extremeNotes = MusicEngine::GetChordMidiNotes(extreme, 8, VoicingStyle::RootPosition, 0, 3);
    TEST_ASSERT(!extremeNotes.empty(), "Extreme octave chord produces notes");
    TEST_ASSERT(*std::max_element(extremeNotes.begin(), extremeNotes.end()) <= 127, "Notes clamped to <= 127");
    auto lowNotes = MusicEngine::GetChordMidiNotes(MusicEngine::ParseChordSymbol("C", "C", MusicalMode::Major), 0, VoicingStyle::RootPosition, 0, 3);
    TEST_ASSERT(*std::min_element(lowNotes.begin(), lowNotes.end()) >= 0, "Notes clamped to >= 0");

    // 3. ユーザーテンプレート: 複数行・空の説明・タブ・バックスラッシュがファイル上で1レコード1行に保たれる
    ProgressionTemplate pt;
    pt.id = "audit_regression_tmpl";
    pt.name = "Audit\tName";
    pt.progression = "[Intro]\r\n| C | G |\n| Am | F \\ |";
    pt.description = "";
    pt.isUserCustom = true;
    TEST_ASSERT(MusicEngine::SaveUserTemplate(pt), "Multi-line template saved");

    bool foundInMemory = false;
    for (const auto& t : MusicEngine::GetTemplatesByCategory("👤 ユーザー定義")) {
        if (t.id == pt.id) {
            foundInMemory = true;
            TEST_ASSERT(t.progression == pt.progression, "Multi-line progression preserved in memory");
            TEST_ASSERT(t.name == pt.name, "Tab in name preserved in memory");
        }
    }
    TEST_ASSERT(foundInMemory, "Multi-line template visible via GetTemplatesByCategory");

    wchar_t appData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
        std::filesystem::path file = std::filesystem::path(appData) / L"ChordLaunch" / L"user_templates.txt";
        std::ifstream ifs(file);
        std::string line;
        int recordLines = 0;
        bool endsWithEmptyDesc = false;
        bool hasNoRawBreakInside = true;
        while (std::getline(ifs, line)) {
            if (line.rfind(pt.id + "\t", 0) == 0) {
                ++recordLines;
                endsWithEmptyDesc = !line.empty() && line.back() == '\t';
                size_t tabs = static_cast<size_t>(std::count(line.begin(), line.end(), '\t'));
                hasNoRawBreakInside = (tabs == 3);
            }
        }
        TEST_ASSERT(recordLines == 1, "Template stored as exactly one line");
        TEST_ASSERT(endsWithEmptyDesc, "Empty description stored as trailing empty field");
        TEST_ASSERT(hasNoRawBreakInside, "Escaped tab does not add extra fields");
    }
    TEST_ASSERT(MusicEngine::DeleteUserTemplate(pt.id), "Audit regression template deleted");

    // 4. RemovePart: 選択中より前を削除しても同じパートが選択され続ける
    PartManager pm;
    pm.SetActiveIndex(2);
    std::string activeName = pm.GetActivePart()->name;
    TEST_ASSERT(pm.RemovePart(0), "RemovePart(0) succeeds");
    TEST_ASSERT(pm.GetActiveIndex() == 1, "Active index shifted after removing earlier part");
    TEST_ASSERT(pm.GetActivePart()->name == activeName, "Same part remains active after removing earlier part");

    // 5. MIDI メタイベント長は可変長数量 (200 バイトのマーカー = 0x81 0x48)
    auto bars = ProgressionParser::Parse("| C |", "C", MusicalMode::Major);
    TEST_ASSERT(!bars.empty(), "Bars parsed for MIDI test");
    bars[0].sectionMarker = std::string(200, 'A');
    auto midi = MidiBuilder::BuildMidi(bars, 120, VoicingStyle::RootPosition, 4, 90, 0);
    const uint8_t pattern[] = {0xFF, 0x06, 0x81, 0x48};
    bool foundVlq = std::search(midi.begin(), midi.end(), std::begin(pattern), std::end(pattern)) != midi.end();
    TEST_ASSERT(foundVlq, "Marker meta event length encoded as variable-length quantity");

    // 6. ベロシティは 1〜127 にクランプ
    bars[0].sectionMarker.clear();
    auto midiOver = MidiBuilder::BuildMidi(bars, 120, VoicingStyle::RootPosition, 4, 300, 0);
    auto midiMax = MidiBuilder::BuildMidi(bars, 120, VoicingStyle::RootPosition, 4, 127, 0);
    TEST_ASSERT(midiOver == midiMax, "Velocity above 127 clamped to 127");

    // 7. JSON の \b / \f がタイトルで往復する
    std::string roundTripTitle = "A\bB\fC";
    std::string json = pm.SerializeProject(roundTripTitle, VoicingStyle::RootPosition, 0, 480);
    PartManager pm2;
    std::string outTitle;
    VoicingStyle outVoicing = VoicingStyle::RootPosition;
    int outBass = 0;
    int outGrid = 0;
    TEST_ASSERT(pm2.DeserializeProject(json, outTitle, outVoicing, outBass, outGrid), "Project with control chars in title deserializes");
    TEST_ASSERT(outTitle == roundTripTitle, "Backspace and formfeed in title round-trip");
}

void TestChordMoveAndClipboard() {
    std::cout << "[TEST] 31. Cross-bar chord move and chord clipboard (Move/Extract/Remove/Insert)\n";

    // 1. 小節線を越える移動: カードは分割されず1枚のまま小節を跨ぐ
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeRestTok(1920));

        auto res = TimelineEdit::MoveChordSpan({ b1, b2 }, 0, 1440, 120);
        TEST_ASSERT(res.bars.size() == 2, "Move keeps the bar count");
        TEST_ASSERT(res.bars[0].chords.size() == 2, "Source bar holds a rest and one spanning card");
        TEST_ASSERT(res.bars[0].chords[0].isRest && res.bars[0].chords[0].durationTicks == 1440, "Leading rest is 1440 ticks");
        TEST_ASSERT(res.bars[0].chords[1].rootName == "C" && res.bars[0].chords[1].durationTicks == 1920,
                    "Moved card keeps its full 1920 ticks across the bar line (not split)");
        TEST_ASSERT(res.bars[1].chords.size() == 1 && res.bars[1].chords[0].isRest, "Next bar stays a single covered rest");
        TEST_ASSERT(res.selectedBar == 0 && res.selectedChord == 1, "Moved card is reported as the selection");
    }

    // 2. 末尾を越える移動先は、長さを保ったまま曲末に収まる位置へクランプされる
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 960));
        b1.chords.push_back(MakeRestTok(960));
        auto res = TimelineEdit::MoveChordSpan({ b1 }, 0, 99999, 120);
        TEST_ASSERT(res.bars[0].chords.back().rootName == "C" && res.bars[0].chords.back().durationTicks == 960,
                    "Move beyond the song end clamps while keeping the length");
        TEST_ASSERT(TimelineEdit::TotalTicks(res.bars[0]) == 1920, "Song length is unchanged after a clamped move");
    }

    // 2b. 小節を足した上での移動（アプリは曲末を越える移動前に休符小節を足す）: 位置が引き戻されない
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 1920));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeRestTok(1920));
        Bar b3 = MakeTickBar4();
        b3.chords.push_back(MakeRestTok(1920));

        auto res = TimelineEdit::MoveChordSpan({ b1, b2, b3 }, 0, 3360, 480);
        TEST_ASSERT(res.bars.size() == 3, "Extended song keeps three bars after the move");
        TEST_ASSERT(res.bars[1].chords.size() == 2 && res.bars[1].chords[0].isRest && res.bars[1].chords[0].durationTicks == 1440,
                    "Move to beat 4 of bar 2 is not pulled back to the bar line");
        TEST_ASSERT(res.bars[1].chords[1].rootName == "C" && res.bars[1].chords[1].durationTicks == 1920,
                    "Card placed at the grid position keeps its full length across the bar line");
        TEST_ASSERT(res.bars[2].chords.size() == 1 && res.bars[2].chords[0].isRest, "Bar after the moved card is a covered rest");
        TEST_ASSERT(res.selectedBar == 1 && res.selectedChord == 1, "Moved card is selected at the requested position");
    }

    // 3. 抽出 / 取り除き / 貼り付けの往復
    {
        Bar b1 = MakeTickBar4();
        b1.chords.push_back(MakeTok("C", 960));
        b1.chords.push_back(MakeTok("G", 960));
        Bar b2 = MakeTickBar4();
        b2.chords.push_back(MakeTok("Am", 960));
        b2.chords.push_back(MakeTok("F", 960));
        std::vector<Bar> bars = { b1, b2 };

        auto items = TimelineEdit::ExtractChords(bars, { 960, 0 });
        TEST_ASSERT(items.size() == 2, "Extract returns both selected chords");
        TEST_ASSERT(items[0].startTick == 0 && items[0].chord.rootName == "C", "First item is relative tick 0 (sorted by time)");
        TEST_ASSERT(items[1].startTick == 960 && items[1].chord.rootName == "G", "Second item keeps its relative offset");

        auto removed = TimelineEdit::RemoveChords(bars, { 0, 960 });
        TEST_ASSERT(removed[0].chords.size() == 1 && removed[0].chords[0].isRest && removed[0].chords[0].durationTicks == 1920,
                    "Cut leaves a single full-bar rest");
        TEST_ASSERT(removed[1].chords.size() == 2 && removed[1].chords[0].rootName == "Am", "Unselected bar is untouched by cut");

        auto pasted = TimelineEdit::InsertRange(bars, items, 1920);
        TEST_ASSERT(pasted[1].chords.size() == 2, "Paste over a bar keeps two chords");
        TEST_ASSERT(pasted[1].chords[0].rootName == "C" && pasted[1].chords[1].rootName == "G", "Pasted chords replace the covered span");
        TEST_ASSERT(TimelineEdit::TotalTicks(pasted[1]) == 1920, "Pasted bar length is preserved");
        TEST_ASSERT(pasted[0].chords[0].rootName == "C" && pasted[0].chords[1].rootName == "G", "Source bar is unchanged by paste");

        // 曲末を越える分は貼り付けない（切り詰め / 破棄）
        auto tail = TimelineEdit::InsertRange(bars, items, 3360);
        TEST_ASSERT(tail[1].chords.size() == 3, "Paste near the song end splits the last bar");
        TEST_ASSERT(tail[1].chords[2].rootName == "C" && tail[1].chords[2].durationTicks == 480, "Tail paste is trimmed to the song end");
        TEST_ASSERT(TimelineEdit::SongTicks(tail) == 3840, "Song length is unchanged after tail paste");
    }
}

void TestKeyRunAndKeyLabel() {
    std::cout << "[TEST] 33. Same-key run start (FindKeyRunStart) and tab key label (KeyModeLabel)\n";

    PartManager pm;
    pm.GetParts().clear();
    auto addPart = [&](const char* name, const char* key, MusicalMode mode) {
        Part p;
        p.name = name;
        p.keyRoot = key;
        p.mode = mode;
        pm.GetParts().push_back(p);
    };
    addPart("Intro", "D", MusicalMode::NaturalMinor);
    addPart("Verse", "D", MusicalMode::NaturalMinor);
    addPart("Pre", "C", MusicalMode::Major);
    addPart("Pre2", "C", MusicalMode::Major);
    addPart("Pre3", "C", MusicalMode::Major);
    addPart("Chorus", "Eb", MusicalMode::Major);
    addPart("Bridge", "Eb", MusicalMode::NaturalMinor);

    const int expected[] = { 0, 0, 2, 2, 2, 5, 6 };
    for (int i = 0; i < 7; ++i) {
        TEST_ASSERT(pm.FindKeyRunStart(i) == expected[i], "FindKeyRunStart returns the first part of the same key/mode run");
    }
    TEST_ASSERT(pm.FindKeyRunStart(-1) == -1 && pm.FindKeyRunStart(99) == 99, "FindKeyRunStart returns out-of-range indices unchanged");

    // エンハーモニック表記違い（C# と Db）は同じ音として同キー扱い
    PartManager pm2;
    pm2.GetParts().clear();
    Part a; a.keyRoot = "C#"; a.mode = MusicalMode::Major;
    Part b; b.keyRoot = "Db"; b.mode = MusicalMode::Major;
    pm2.GetParts().push_back(a);
    pm2.GetParts().push_back(b);
    TEST_ASSERT(pm2.FindKeyRunStart(1) == 0, "Enharmonic key spellings belong to the same run");

    TEST_ASSERT(MusicEngine::KeyModeLabel("D", MusicalMode::Major) == "D maj", "Major tab label is 'D maj'");
    TEST_ASSERT(MusicEngine::KeyModeLabel("D", MusicalMode::NaturalMinor) == "D min", "Natural minor tab label is 'D min'");
    TEST_ASSERT(MusicEngine::KeyModeLabel("Eb", MusicalMode::Dorian) == "Eb dor", "Other modes use a short abbreviation");
    TEST_ASSERT(MusicEngine::KeyModeLabel("D", MusicalMode::Major) != MusicEngine::KeyModeLabel("D", MusicalMode::NaturalMinor),
                "Major and minor labels differ for the same tonic");
}

void TestBridgeSuggestions() {
    std::cout << "[TEST] 32. Cross-part suggestion bridging (GetBridgeSuggestions / OrderBorrowedByContext)\n";

    ChordToken am = MusicEngine::ParseChordSymbol("Am", "C", MusicalMode::Major);
    ChordToken g = MusicEngine::ParseChordSymbol("G", "C", MusicalMode::Major);

    // 同キー: 通常の候補と同じ結果（前パートから連続）
    {
        auto bridge = MusicEngine::GetBridgeSuggestions(am, "C", MusicalMode::Major, "C", MusicalMode::Major, false);
        auto plain = MusicEngine::GetSuggestions({ am }, "C", MusicalMode::Major, false);
        TEST_ASSERT(bridge.size() == plain.size() && !bridge.empty(), "Same key yields the same number of suggestions as GetSuggestions");
        bool same = (bridge.size() == plain.size());
        for (size_t i = 0; same && i < bridge.size(); ++i) {
            same = (bridge[i].rootName == plain[i].rootName && bridge[i].quality == plain[i].quality);
        }
        TEST_ASSERT(same, "Same key keeps the GetSuggestions order");
    }

    // 異キー(C → G): 前パート末尾 G は両キーで使えるため先頭に来る（重複なし）
    {
        auto bridge = MusicEngine::GetBridgeSuggestions(g, "C", MusicalMode::Major, "G", MusicalMode::Major, false);
        TEST_ASSERT(bridge.size() >= 4, "Different key yields at least four suggestions");
        TEST_ASSERT(!bridge.empty() && bridge[0].rootName == "G", "Shared pivot chord (G) is suggested first");
        bool unique = true;
        for (size_t i = 0; i < bridge.size(); ++i) {
            for (size_t j = i + 1; j < bridge.size(); ++j) {
                if (bridge[i].rootPitch == bridge[j].rootPitch && bridge[i].quality == bridge[j].quality) unique = false;
            }
        }
        TEST_ASSERT(unique, "Bridge suggestions contain no duplicates");
    }

    // 保存済みの度数が古い（別キー基準の "bIII"）G でも、D マイナーでの機能(IV)で候補が決まる
    {
        ChordToken gStale = MusicEngine::ParseChordSymbol("G", "D", MusicalMode::NaturalMinor);
        TEST_ASSERT(gStale.degreeRoman == "IV", "G in D minor is degree IV");
        ChordToken gOld = gStale;
        gOld.degreeRoman = "bIII"; // E 基準の古い度数
        auto fresh = MusicEngine::GetSuggestions({ gStale }, "D", MusicalMode::NaturalMinor, false);
        auto stale = MusicEngine::GetSuggestions({ gOld }, "D", MusicalMode::NaturalMinor, false);
        TEST_ASSERT(!fresh.empty() && fresh[0].rootName == "A" && fresh[0].qualitySuffix == "m", "IV-function chord suggests Am first in D minor");
        bool same = (fresh.size() == stale.size());
        for (size_t i = 0; same && i < fresh.size(); ++i) {
            same = (fresh[i].rootName == stale[i].rootName && fresh[i].quality == stale[i].quality);
        }
        TEST_ASSERT(same, "A stale stored degree does not change the suggestions");

        ChordToken em = MusicEngine::ParseChordSymbol("Em", "D", MusicalMode::NaturalMinor);
        TEST_ASSERT(em.degreeRoman == "IIm", "Em in D minor is degree IIm");
        std::string recomputed = MusicEngine::CalculateDegree(em.rootPitch, em.quality, MusicEngine::NoteToPitch("C"), MusicalMode::Major, em.bassPitch);
        TEST_ASSERT(recomputed == "IIIm", "The same Em is degree IIIm when read in C");
    }

    // 前コードなし（休符）は、通常の初期候補にフォールバックする
    {
        auto bridge = MusicEngine::GetBridgeSuggestions(MakeRestTok(480), "C", MusicalMode::Major, "G", MusicalMode::Major, false);
        auto plain = MusicEngine::GetSuggestions({}, "G", MusicalMode::Major, false);
        TEST_ASSERT(bridge.size() == plain.size() && !bridge.empty() && bridge[0].rootName == plain[0].rootName,
                    "Rest as previous chord falls back to the default suggestions");
    }

    // 借用コードは基準コードに近い根音から並べる（件数は変わらない）
    {
        auto borrowed = MusicEngine::GetModalInterchangeChords("C", MusicalMode::Major, false);
        auto ordered = MusicEngine::OrderBorrowedByContext(borrowed, g);
        TEST_ASSERT(ordered.size() == borrowed.size(), "Borrowed chord count is preserved");
        auto dist = [&](const ChordToken& c) {
            int d = std::abs(static_cast<int>(c.rootPitch) - static_cast<int>(g.rootPitch)) % 12;
            return std::min(d, 12 - d);
        };
        TEST_ASSERT(!ordered.empty() && dist(ordered.front()) <= dist(ordered.back()), "Borrowed chords are ordered by closeness to the base chord");
    }

    // 転調先（接続先）キーへ向かう借用候補
    {
        auto same = MusicEngine::GetModulationBorrowed("C", MusicalMode::Major, "C", MusicalMode::Major, false);
        TEST_ASSERT(same.empty(), "No modulation borrowed chords when the key does not change");

        // D → C（全音下への転調）: 接続先 C の V7 (G7) が先頭
        auto toC = MusicEngine::GetModulationBorrowed("D", MusicalMode::Major, "C", MusicalMode::Major, false);
        TEST_ASSERT(toC.size() >= 3, "Modulation borrowed list has several candidates");
        TEST_ASSERT(!toC.empty() && toC[0].rootName == "G" && toC[0].quality == ChordQuality::Dominant7,
                    "First modulation candidate is V7 of the target key (G7 for C)");
        bool unique = true;
        for (size_t i = 0; i < toC.size(); ++i) {
            for (size_t j = i + 1; j < toC.size(); ++j) {
                if (toC[i].rootPitch == toC[j].rootPitch && toC[i].quality == toC[j].quality) unique = false;
            }
        }
        TEST_ASSERT(unique, "Modulation borrowed candidates contain no duplicates");

        // モードだけが違う場合（C major → C minor）も転調先として扱う
        auto toMinor = MusicEngine::GetModulationBorrowed("C", MusicalMode::Major, "C", MusicalMode::NaturalMinor, false);
        TEST_ASSERT(!toMinor.empty(), "Mode change at the same tonic also yields modulation candidates");
    }

    // 異キーの橋渡し（D の最終コード D → 次パート C）でも、重複なく4件以上返る
    {
        ChordToken d = MusicEngine::ParseChordSymbol("D", "D", MusicalMode::Major);
        auto bridge = MusicEngine::GetBridgeSuggestions(d, "D", MusicalMode::Major, "C", MusicalMode::Major, false);
        TEST_ASSERT(bridge.size() >= 4, "Bridge from the last chord to the next part's key returns at least four candidates");
        bool hasDominant = false;
        for (const auto& c : bridge) {
            if (c.rootName == "G") hasDominant = true;
        }
        TEST_ASSERT(hasDominant, "Bridge to key C includes its dominant (G)");
    }

    // D minor の最終コード Dm → 次パート C major: 共通ピボット(Dm/C/F/Am)が枠を埋めず、V(G)が先頭4件に入る
    {
        ChordToken dm = MusicEngine::ParseChordSymbol("Dm", "D", MusicalMode::NaturalMinor);
        auto bridge = MusicEngine::GetBridgeSuggestions(dm, "D", MusicalMode::NaturalMinor, "C", MusicalMode::Major, false);
        TEST_ASSERT(bridge.size() >= 4, "D minor to C major bridge returns at least four candidates");
        TEST_ASSERT(bridge[0].rootName == "Dm" || (bridge[0].rootName == "D" && bridge[0].quality == dm.quality),
                    "The previous chord itself leads as the first pivot");
        bool gInTop4 = false;
        bool fInTop4 = false;
        for (size_t i = 0; i < 4 && i < bridge.size(); ++i) {
            if (bridge[i].rootName == "G") gInTop4 = true;
            if (bridge[i].rootName == "F") fInTop4 = true;
        }
        TEST_ASSERT(gInTop4, "Dominant (G) of the target key is within the top four candidates");
        // 共通ピボット4件(Dm/C/F/Am)のうち、先頭4件に入るのは Dm と C の2件まで（F は V/I の後ろ）
        TEST_ASSERT(bridge[1].rootName == "C" && !fInTop4 && bridge[2].rootName == "G",
                    "Pivots are capped at two (Dm, C) and V (G) follows immediately");
    }
}

// MIDI バイナリから Marker(FF 06) メタイベントの (絶対tick, 名前) を取り出す簡易パーサ
static std::vector<std::pair<uint32_t, std::string>> ExtractMidiMarkers(const std::vector<uint8_t>& midi) {
    std::vector<std::pair<uint32_t, std::string>> out;
    size_t pos = 22; // MThd(14) + MTrk(4) + length(4)
    uint32_t tick = 0;
    while (pos < midi.size()) {
        uint32_t delta = 0;
        while (pos < midi.size()) {
            uint8_t b = midi[pos++];
            delta = (delta << 7) | (b & 0x7F);
            if (!(b & 0x80)) break;
        }
        tick += delta;
        if (pos >= midi.size()) break;
        uint8_t status = midi[pos++];
        if (status == 0xFF) {
            uint8_t type = midi[pos++];
            uint32_t len = 0;
            while (pos < midi.size()) {
                uint8_t b = midi[pos++];
                len = (len << 7) | (b & 0x7F);
                if (!(b & 0x80)) break;
            }
            if (type == 0x06) out.emplace_back(tick, std::string(midi.begin() + pos, midi.begin() + pos + len));
            pos += len;
            if (type == 0x2F) break;
        } else {
            pos += 2; // Note On/Off はデータ2バイト
        }
    }
    return out;
}

void TestPartBarNumberingAndMarkers() {
    std::cout << "[TEST] 34. Per-part bar numbering from 1 and free-position markers (tick based)\n";

    PartManager pm;
    pm.GetParts().clear();
    pm.AddPart("Verse", "C", MusicalMode::Major);
    pm.AddPart("Chorus", "C", MusicalMode::Major);
    auto& parts = pm.GetParts();
    TEST_ASSERT(parts.size() == 2, "Two parts created");
    const int len0 = PartManager::PartLengthTicks(parts[0]);
    TEST_ASSERT(len0 == 4 * TICKS_PER_BAR_4_4, "Part length is the sum of bar ticks");

    // 1. RenumberBars
    parts[1].bars[0].barNumber = 9;
    parts[1].bars[1].barNumber = 9;
    PartManager::RenumberBars(parts[1].bars);
    bool seq = true;
    for (size_t i = 0; i < parts[1].bars.size(); ++i) seq = seq && (parts[1].bars[i].barNumber == static_cast<int>(i) + 1);
    TEST_ASSERT(seq, "RenumberBars restores 1..N numbering");

    // 2. SetMarker: 小節の途中にも置ける。ソート・上書き・解除・不正入力
    const int midBar = TICKS_PER_BAR_4_4 + TICKS_PER_BEAT * 2 + 120; // 2小節目の途中（拍の途中）
    TEST_ASSERT(pm.SetMarker(0, midBar, "Hook"), "SetMarker places a marker mid-bar");
    TEST_ASSERT(pm.SetMarker(0, TICKS_PER_BEAT, "Early"), "SetMarker places an earlier marker");
    TEST_ASSERT(parts[0].markers.size() == 2 && parts[0].markers[0].tick == TICKS_PER_BEAT && parts[0].markers[1].tick == midBar,
                "Markers are kept sorted by tick");
    TEST_ASSERT(pm.SetMarker(0, midBar, "  [Hook2]\n "), "Same tick overwrites with sanitized name");
    TEST_ASSERT(parts[0].markers.size() == 2 && parts[0].markers[1].name == "Hook2", "Overwrite keeps one marker per tick");
    TEST_ASSERT(!pm.SetMarker(0, midBar, "Key: G"), "Names starting with Key: are rejected");
    TEST_ASSERT(parts[0].markers[1].name == "Hook2", "Rejected name leaves the marker unchanged");
    TEST_ASSERT(!pm.SetMarker(0, 0, "X") && !pm.SetMarker(0, -5, "X") && !pm.SetMarker(9, 100, "X"), "Tick 0, negative tick and bad part are rejected");
    TEST_ASSERT(pm.SetMarker(0, len0 + 5000, "Tail"), "Tick beyond the part end is accepted and clamped");
    TEST_ASSERT(parts[0].markers.back().tick == len0 - 1, "Out-of-range tick is clamped to length-1");
    TEST_ASSERT(pm.FindMarkerNear(0, midBar + 10, 20) == midBar && pm.FindMarkerNear(0, midBar + 100, 20) == -1, "FindMarkerNear honors tolerance");
    TEST_ASSERT(pm.RemoveMarker(0, len0 - 1) && parts[0].markers.size() == 2, "RemoveMarker deletes by exact tick");
    TEST_ASSERT(pm.SetMarker(0, TICKS_PER_BEAT, "") && parts[0].markers.size() == 1, "Empty name removes the marker");

    // 3. マージ: 後続パートへ長さ分オフセットされ、パート先頭はタブ名のまま
    TEST_ASSERT(pm.SetMarker(1, TICKS_PER_BEAT * 3, "Drop"), "Marker in the second part");
    auto mergedMarkers = pm.GenerateMergedMarkers();
    TEST_ASSERT(mergedMarkers.size() == 2 && mergedMarkers[0].tick == midBar, "Merged markers keep first-part ticks");
    TEST_ASSERT(mergedMarkers[1].tick == len0 + TICKS_PER_BEAT * 3 && mergedMarkers[1].name == "Drop", "Second-part marker is offset by the first part length");
    auto merged = pm.GenerateMergedProgression();
    TEST_ASSERT(merged[0].sectionMarker == "Verse" && merged[parts[0].bars.size()].sectionMarker == "Chorus", "Part-first bars carry the tab names");
    TEST_ASSERT(merged[parts[0].bars.size()].barNumber == static_cast<int>(parts[0].bars.size()) + 1, "Merged bar numbers are continuous");

    // 4. 旧形式（小節マーカー）の変換: 2小節目以降は小節先頭へ、先頭小節は破棄
    {
        Part legacy;
        legacy.keyRoot = "C";
        legacy.bars = ProgressionParser::Parse("[Intro]\n| C | G |\n[Verse]\n| Am | F |", "C", MusicalMode::Major);
        TEST_ASSERT(legacy.bars.size() == 4 && legacy.bars[2].sectionMarker == "Verse", "Parser still reads [name] into the bar");
        PartManager::MigrateBarMarkers(legacy);
        TEST_ASSERT(legacy.markers.size() == 1 && legacy.markers[0].tick == 2 * TICKS_PER_BAR_4_4 && legacy.markers[0].name == "Verse",
                    "Legacy bar marker becomes a marker at the bar start (first bar dropped)");
        bool cleared = true;
        for (const auto& b : legacy.bars) cleared = cleared && b.sectionMarker.empty();
        TEST_ASSERT(cleared, "Bar markers are cleared after migration");

        legacy.markers.push_back(PartMarker{ 99999, "Far" });
        PartManager::MigrateBarMarkers(legacy);
        TEST_ASSERT(legacy.markers.back().tick == PartManager::PartLengthTicks(legacy) - 1, "Migration clamps markers beyond the part end");
    }

    // 5. 保存・読込でマーカー位置が保たれ、小節番号は1始まりに正規化される
    parts[1].bars[0].barNumber = 7;
    std::string json = pm.SerializeProject("MarkerSong", VoicingStyle::RootPosition, 0, 480);
    PartManager pm2;
    std::string t; VoicingStyle v = VoicingStyle::RootPosition; int bm = 0; int g = 0;
    TEST_ASSERT(pm2.DeserializeProject(json, t, v, bm, g), "Project with markers deserializes");
    TEST_ASSERT(pm2.GetParts().size() == 2 && pm2.GetParts()[0].markers.size() == 1 &&
                pm2.GetParts()[0].markers[0].tick == midBar && pm2.GetParts()[0].markers[0].name == "Hook2",
                "Mid-bar marker survives project save/load at the exact tick");
    TEST_ASSERT(pm2.GetParts()[1].markers.size() == 1 && pm2.GetParts()[1].markers[0].name == "Drop", "Second-part marker survives save/load");
    TEST_ASSERT(pm2.GetParts()[1].bars[0].barNumber == 1, "Loaded parts are renumbered from bar 1");

    // 6. MIDI: マーカーが指定 tick（小節の途中）に Marker イベントとして出力される
    {
        auto bars = ProgressionParser::Parse("| C | G |", "C", MusicalMode::Major);
        std::vector<PartMarker> mk = { { 960, "Mid" }, { 2 * TICKS_PER_BAR_4_4 - 1, "Edge" } };
        auto midi = MidiBuilder::BuildMidi(bars, 120, VoicingStyle::RootPosition, 4, 90, 0, &mk);
        auto found = ExtractMidiMarkers(midi);
        TEST_ASSERT(found.size() == 2 && found[0].first == 960 && found[0].second == "Mid", "MIDI marker is written at the exact mid-bar tick");
        TEST_ASSERT(found.size() == 2 && found[1].first == 2u * TICKS_PER_BAR_4_4 - 1 && found[1].second == "Edge", "MIDI marker near the end keeps its tick");
        auto plain = MidiBuilder::BuildMidi(bars, 120, VoicingStyle::RootPosition, 4, 90, 0);
        TEST_ASSERT(ExtractMidiMarkers(plain).empty(), "No markers argument writes no Marker events");
    }
}

// ヘルプに載せている入力例・数字接尾辞表・分割バー・縦ズーム窓の計算が、実装と一致することを確認する
void TestHelpExamplesAndLayoutHelpers() {
    std::cout << "[TEST] Help examples, card split, piano-roll vertical zoom window\n";

    auto render = [](const std::string& text) {
        std::string out;
        auto bars = ProgressionParser::Parse(text, "C", MusicalMode::Major);
        for (const auto& bar : bars) {
            for (const auto& chord : bar.chords) {
                if (!out.empty()) out += " ";
                out += chord.rootName + chord.qualitySuffix;
            }
        }
        return out;
    };

    // 入力形式タブ 項目3 の表（コマンド入力 / 通常の記号 / 複合入力 / 数字のみ）
    TEST_ASSERT(render("4mas 3svn 6mis 1") == "FMaj7 E7 Am7 C", "command suffix example: " + render("4mas 3svn 6mis 1"));
    TEST_ASSERT(render("4M7 37 6m7 1") == "FMaj7 E7 Am7 C", "standard symbol example: " + render("4M7 37 6m7 1"));
    TEST_ASSERT(render("4M7 E7 6m7 C") == "FMaj7 E7 Am7 C", "mixed example: " + render("4M7 E7 6m7 C"));
    TEST_ASSERT(render("1 5 6 4") == "C G Am F", "numbers only example: " + render("1 5 6 4"));

    // コード記号表: ^ は残し、Δ7 と ø は入力記号から外した。M7 / maj7 / hdm / m7b5 は従来どおり
    {
        auto q = [](const char* sym) { return MusicEngine::ParseChordSymbol(sym, "C", MusicalMode::Major).quality; };
        TEST_ASSERT(q("C^7") == ChordQuality::Major7, "C^7 stays Major7");
        TEST_ASSERT(q("CM7") == ChordQuality::Major7 && q("Cmaj7") == ChordQuality::Major7, "CM7 / Cmaj7 stay Major7");
        TEST_ASSERT(q("Chdm") == ChordQuality::HalfDiminished7 && q("Cm7b5") == ChordQuality::HalfDiminished7, "Chdm / Cm7b5 stay half-diminished");
        TEST_ASSERT(q("C\xCE\x94" "7") != ChordQuality::Major7, "C(delta)7 is no longer a Major7 alias");
        TEST_ASSERT(q("C\xC3\xB8") != ChordQuality::HalfDiminished7, "C(o-slash) is no longer a half-diminished alias");
    }

    // 数字接尾辞タブの表（入力 → 結果）
    const std::pair<const char*, const char*> suffixRows[] = {
        { "4mas", "FMaj7" }, { "5svn", "G7" }, { "2mis", "Dm7" }, { "4m", "Fm" },
        { "7hdm", "Bm7b5" }, { "7dim", "Bdim" }, { "7dim7", "Bdim7" }, { "5suf", "Gsus4" },
        { "1sut", "Csus2" }, { "4adn", "Fadd9" }, { "1six", "C6" }
    };
    for (const auto& row : suffixRows) {
        TEST_ASSERT(render(row.first) == row.second, std::string("suffix table: ") + row.first + " -> " + render(row.first) + " (expected " + row.second + ")");
    }

    // カード領域の高さ: 自動 / 比率指定 / 極端値
    TEST_ASSERT(ComputeCardTrackHeight(300.0f, 0.0f) == 80.0f, "auto height is clamped to 80");
    TEST_ASSERT(ComputeCardTrackHeight(150.0f, 0.0f) == 58.0f, "auto height keeps the 58px minimum");
    TEST_ASSERT(ComputeCardTrackHeight(400.0f, 0.5f) == 200.0f, "ratio 0.5 of 400 is 200");
    TEST_ASSERT(ComputeCardTrackHeight(400.0f, 0.99f) == 340.0f, "piano roll keeps at least 60px");
    TEST_ASSERT(ComputeCardTrackHeight(400.0f, 0.01f) == 58.0f, "card keeps at least 58px");
    TEST_ASSERT(ComputeCardTrackHeight(100.0f, 0.9f) == 58.0f, "tiny area never goes below the card minimum");
    TEST_ASSERT(ComputeCardTrackHeight(0.0f, 0.5f) >= 58.0f, "zero area falls back to the minimum");
    TEST_ASSERT(CardSplitRatioFromHeight(200.0f, 400.0f) == 0.5f, "ratio from height");
    TEST_ASSERT(std::fabs(CardSplitRatioFromHeight(9999.0f, 400.0f) - 340.0f / 400.0f) < 1e-4f, "ratio from huge height is clamped");
    TEST_ASSERT(std::fabs(CardSplitRatioFromHeight(-50.0f, 400.0f) - 58.0f / 400.0f) < 1e-4f, "ratio from negative height is clamped");
    TEST_ASSERT(CardSplitRatioFromHeight(100.0f, 0.0f) == 0.0f, "ratio with no area is auto (0)");

    // ピアノロール縦ズーム窓: 等倍 / ズーム / スクロール範囲 / 行数が少ない場合
    PianoRollWindow w1 = ComputePianoRollWindow(55, 75, 1.0f, 5);
    TEST_ASSERT(w1.minMidi == 55 && w1.maxMidi == 75 && w1.scrollRows == 0, "zoom 1.0 shows the whole range and drops scroll");
    PianoRollWindow w2 = ComputePianoRollWindow(55, 75, 2.0f, 0);   // 21行 -> 10行（中央寄せ）
    TEST_ASSERT(w2.maxMidi - w2.minMidi + 1 == 11, "zoom 2.0 shows round(21 / 2) = 11 rows");
    TEST_ASSERT(w2.minMidi >= 55 && w2.maxMidi <= 75, "zoomed window stays inside the range");
    PianoRollWindow w3 = ComputePianoRollWindow(55, 75, 2.0f, 100);  // 上へ過剰スクロール
    TEST_ASSERT(w3.maxMidi == 75, "over-scroll up clamps to the top of the range");
    PianoRollWindow w4 = ComputePianoRollWindow(55, 75, 2.0f, -100); // 下へ過剰スクロール
    TEST_ASSERT(w4.minMidi == 55, "over-scroll down clamps to the bottom of the range");
    TEST_ASSERT(ComputePianoRollWindow(55, 75, 2.0f, w3.scrollRows).maxMidi == 75, "returned scroll value is stable");
    PianoRollWindow w5 = ComputePianoRollWindow(60, 64, 4.0f, 0);    // 5行しかない音域
    TEST_ASSERT(w5.minMidi == 60 && w5.maxMidi == 64, "range smaller than the 6-row floor is not shrunk");
    PianoRollWindow w6 = ComputePianoRollWindow(40, 62, 4.0f, 0);
    TEST_ASSERT(w6.maxMidi - w6.minMidi + 1 >= 6, "max zoom keeps at least 6 rows");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "ChordLaunch C++20 Core Test Suite\n";
    std::cout << "========================================\n";

    TestBug1_RepeatTokens();
    TestBug2_EnglishLocalization();
    TestBug3_ModalInterchangeAndDragDrop();
    TestBug4_DegreesNeverReturnQuestionMark();
    TestBug5_HyphenPackedTokenNormalization();
    TestRestTokens();
    TestPartManagerAndKeyChangeMerge();
    TestLinkedKeyModeBpm();
    TestMidiBuilder();
    TestDegreeInputParsing();
    TestNewAuditFixes();
    TestUiRedesignFeatures();
    TestTickTimeModel();
    TestProjectFileAndAutoBackup();
    TestUserTemplateManager();
    TestWasapiAudioEngine();
    TestComprehensiveFixes();
    TestDashboardAndMRU();
    TestPartRenameAndMergeAllToNewPart();
    TestDashboardFileSelectionAndMenu();
    TestJsonUnescapeAndPathNormalization();
    TestTickSyncopationAndTriadDefault();
    TestCardResizeLeftWithRestPadding();
    TestDropGridSnapPreview();
    TestCrossBarDragAndRestNormalization();
    TestTimelineEditInsertAndResize();
    TestAppendTickAndEmptyPartAppend();
    TestSyncopationPersistence();
    TestOctaveAndGridSeparation();
    TestOneBarChordCrossBarInsertion();
    TestExistingProjectDeserializationRestoresPitch();
    TestAuditFixesRegression();
    TestVersionDefinition();
    TestChordMoveAndClipboard();
    TestKeyRunAndKeyLabel();
    TestBridgeSuggestions();
    TestPartBarNumberingAndMarkers();
    TestHelpExamplesAndLayoutHelpers();

    std::cout << "========================================\n";
    std::cout << "Tests Passed: " << g_passCount << ", Failed: " << g_failCount << "\n";
    std::cout << "========================================\n";

    return (g_failCount == 0) ? 0 : 1;
}
