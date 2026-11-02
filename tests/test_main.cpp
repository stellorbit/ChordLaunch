#include "music_theory.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "part_manager.hpp"
#include "midi_builder.hpp"
#include "i18n.hpp"
#include "wasapi_synth.hpp"
#include <windows.h>
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

    std::cout << "========================================\n";
    std::cout << "Tests Passed: " << g_passCount << ", Failed: " << g_failCount << "\n";
    std::cout << "========================================\n";

    return (g_failCount == 0) ? 0 : 1;
}
