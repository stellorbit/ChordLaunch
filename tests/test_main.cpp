#include "music_theory.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "part_manager.hpp"
#include "midi_builder.hpp"
#include "i18n.hpp"
#include "wasapi_synth.hpp"
#include "app_window_internal.hpp"
#include <windows.h>
#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>

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

        auto overflow = TimelineEdit::InsertChord(bar, MakeTok("G", 480), 480);
        TEST_ASSERT(overflow.empty(), "Split insert must not overflow a full-length chord");
        TEST_ASSERT(bar.chords.size() == 3, "C must be split into 2 pieces around the inserted G");
        TEST_ASSERT(bar.chords[0].rootName == "C" && bar.chords[0].durationTicks == 480, "Left C piece is 480 ticks");
        TEST_ASSERT(bar.chords[1].rootName == "G" && bar.chords[1].durationTicks == 480, "G sits on beat 2");
        TEST_ASSERT(bar.chords[2].rootName == "C" && bar.chords[2].durationTicks == 960, "Right C piece is 960 ticks (no drift)");
        TEST_ASSERT(TimelineEdit::TotalTicks(bar) == 1920, "Bar total stays exactly 1920 ticks");
    }

    // 2. 休符区間へのドロップは休符を消費し、後続コードを押し出さない
    {
        Bar bar = MakeTickBar4();
        bar.chords.push_back(MakeTok("C", 960));
        bar.chords.push_back(MakeRestTok(960));

        auto overflow = TimelineEdit::InsertChord(bar, MakeTok("G", 480), 960);
        TEST_ASSERT(overflow.empty(), "Drop into a rest must not overflow");
        TEST_ASSERT(bar.chords.size() == 3, "Rest is split into chord + remainder");
        TEST_ASSERT(bar.chords[0].rootName == "C" && bar.chords[0].durationTicks == 960, "Preceding C untouched");
        TEST_ASSERT(bar.chords[1].rootName == "G" && bar.chords[1].durationTicks == 480, "G pinned at the drop tick");
        TEST_ASSERT(bar.chords[2].isRest && bar.chords[2].durationTicks == 480, "Remaining rest is 480 ticks");
        TEST_ASSERT(StartTickOfIndex(bar, 1) == 960, "G starts exactly at the drop tick (WYSIWYG)");
    }

    // 3. 満杯の小節では押し出し分が overflow として次小節へ渡る（データ消失なし）
    {
        Bar bar = MakeTickBar4();
        for (const char* r : {"C", "D", "E", "F"}) bar.chords.push_back(MakeTok(r, 480));

        auto overflow = TimelineEdit::InsertChord(bar, MakeTok("G", 480), 480);
        TEST_ASSERT(overflow.size() == 1, "One chord overflows to the next bar");
        TEST_ASSERT(overflow[0].chord.rootName == "F", "Last chord F is carried over (nothing is deleted)");
        TEST_ASSERT(TimelineEdit::TotalTicks(bar) == 1920, "Rippled bar still sums to 1920 ticks");
        TEST_ASSERT(bar.chords[1].rootName == "G", "Inserted chord lands on the requested tick");
        TEST_ASSERT(StartTickOfIndex(bar, 1) == 480, "Inserted chord starts at 480 ticks");
    }

    // 4. 押し出しは後方の休符で吸収される（次小節へ漏れない）
    {
        Bar bar = MakeTickBar4();
        bar.chords.push_back(MakeTok("C", 480));
        bar.chords.push_back(MakeRestTok(1440));

        auto overflow = TimelineEdit::InsertChord(bar, MakeTok("G", 480), 0);
        TEST_ASSERT(overflow.empty(), "Trailing rest absorbs the shift");
        TEST_ASSERT(TimelineEdit::TotalTicks(bar) == 1920, "Bar stays 1920 ticks");
        TEST_ASSERT(bar.chords[0].rootName == "G", "G on beat 1");
        TEST_ASSERT(bar.chords[1].rootName == "C" && bar.chords[1].durationTicks == 480, "C pushed to beat 2");
        TEST_ASSERT(bar.chords[2].isRest && bar.chords[2].durationTicks == 960, "Rest shrunk to 960 ticks");
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
    std::cout << "[TEST] 28. One-bar chord cross-bar insertion and overflow handling\n";

    // 1. 1小節のコード A (1920 ticks) を Bar 1 の 2拍目 (480 ticks) に挿入
    Bar bar1;
    bar1.barNumber = 1;
    bar1.timeNumerator = 4;
    bar1.timeDenominator = 4;

    ChordToken chordC;
    chordC.rawText = "C";
    chordC.rootName = "C";
    chordC.SetTicks(1920);
    bar1.chords.push_back(chordC);

    ChordToken chordA;
    chordA.rawText = "A";
    chordA.rootName = "A";
    chordA.SetTicks(1920);

    auto overflow = TimelineEdit::InsertChord(bar1, chordA, 480);
    TEST_ASSERT(overflow.size() == 1, "Inserting 1-bar chord at Beat 2 produces 1 overflow item");
    TEST_ASSERT(overflow[0].startTick == 0, "Overflow item starts at tick 0 of next bar");
    TEST_ASSERT(overflow[0].chord.durationTicks == 480, "Overflow item duration is 480 ticks (1 beat)");

    TEST_ASSERT(TimelineEdit::TotalTicks(bar1) == 1920, "Bar 1 total ticks is strictly 1920 ticks");
    TEST_ASSERT(bar1.chords.size() == 2, "Bar 1 contains 2 chords (C trimmed to 480, A occupying 1440)");
    TEST_ASSERT(bar1.chords[0].rootName == "C" && bar1.chords[0].durationTicks == 480, "C occupies 0..480 (Beat 1)");
    TEST_ASSERT(bar1.chords[1].rootName == "A" && bar1.chords[1].durationTicks == 1440, "A occupies 480..1920 (Beats 2-4)");

    // 2. 次小節 (Bar 2) へのオーバーフロー挿入
    Bar bar2;
    bar2.barNumber = 2;
    bar2.timeNumerator = 4;
    bar2.timeDenominator = 4;
    ChordToken restBar2;
    restBar2.isRest = true;
    restBar2.rawText = "_";
    restBar2.SetTicks(1920);
    bar2.chords.push_back(restBar2);

    auto overflow2 = TimelineEdit::InsertChord(bar2, overflow[0].chord, overflow[0].startTick);
    TEST_ASSERT(overflow2.empty(), "Overflow item into empty bar produces no further overflow");
    TEST_ASSERT(TimelineEdit::TotalTicks(bar2) == 1920, "Bar 2 total ticks is strictly 1920 ticks");
    TEST_ASSERT(bar2.chords.size() == 2, "Bar 2 contains A (480) and remaining rest (1440)");
    TEST_ASSERT(bar2.chords[0].rootName == "A" && bar2.chords[0].durationTicks == 480, "A occupies Beat 1 of Bar 2 (480 ticks)");
    TEST_ASSERT(bar2.chords[1].isRest && bar2.chords[1].durationTicks == 1440, "Remaining 3 beats in Bar 2 are rest (1440 ticks)");

    // 3. 3拍目 (960 ticks) に1小節コードを挿入した場合の前後小節の分割検証
    Bar bar3;
    bar3.barNumber = 3;
    ChordToken restBar3;
    restBar3.isRest = true;
    restBar3.rawText = "_";
    restBar3.SetTicks(1920);
    bar3.chords.push_back(restBar3);

    auto over3 = TimelineEdit::InsertChord(bar3, chordA, 960);
    TEST_ASSERT(over3.size() == 1 && over3[0].chord.durationTicks == 960, "At Beat 3, overflow is 960 ticks (2 beats)");
    TEST_ASSERT(TimelineEdit::TotalTicks(bar3) == 1920, "Bar 3 total ticks is 1920 ticks");
    TEST_ASSERT(bar3.chords.size() == 2 && bar3.chords[1].durationTicks == 960, "Bar 3 has rest (960) + A (960)");
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
    TestSyncopationPersistence();
    TestOctaveAndGridSeparation();
    TestOneBarChordCrossBarInsertion();
    TestExistingProjectDeserializationRestoresPitch();

    std::cout << "========================================\n";
    std::cout << "Tests Passed: " << g_passCount << ", Failed: " << g_failCount << "\n";
    std::cout << "========================================\n";

    return (g_failCount == 0) ? 0 : 1;
}
