#include "i18n.hpp"
#include <vector>

namespace clp {

Language I18n::s_currentLang = Language::Japanese;

struct LocPair {
    const wchar_t* ja;
    const wchar_t* en;
    const char* ja_u8;
    const char* en_u8;
};

#define LOC(ja_str, en_str) { L##ja_str, L##en_str, ja_str, en_str }

static const LocPair kStrings[] = {
    // AppTitle
    LOC("Chord Launch", "Chord Launch"),
    // Play
    LOC("再生 (Space)", "Play (Space)"),
    // Stop
    LOC("停止", "Stop"),
    // Loop
    LOC("ループ", "Loop"),
    // Bpm
    LOC("BPM", "BPM"),
    // TapTempo
    LOC("TAP", "TAP"),
    // TimeSig
    LOC("拍子", "Meter"),
    // Voicing
    LOC("ボイシング", "Voicing"),
    // SoundPreset
    LOC("音色", "Instrument"),
    // MasterVolume
    LOC("音量", "Volume"),
    // ExportMidiDaw
    LOC("DAWへドラッグ", "Drag to DAW"),
    // DragToDawTooltip
    LOC("DAWのトラックへ直接ドラッグ＆ドロップしてMIDIを取り込めます", "Drag & Drop directly to your DAW track to import MIDI"),
    // Undo
    LOC("元に戻す (Ctrl+Z)", "Undo (Ctrl+Z)"),
    // Redo
    LOC("やり直す (Ctrl+Y)", "Redo (Ctrl+Y)"),
    // Save
    LOC("保存", "Save"),
    // Load
    LOC("開く", "Open"),
    // LangSwitch
    LOC("English", "Japanese"),

    // Left Pane
    LOC("コード進行テキスト入力", "Chord Progression Text"),
    LOC("| C | G | Am | F | のように入力", "Enter chords e.g. | C | G | Am | F |"),
    LOC("ダイアトニックコード", "Diatonic Chords"),
    LOC("3和音", "Triads"),
    LOC("7th和音", "7th Chords"),
    LOC("休符 (N.C.)", "Rest (N.C.)"),
    LOC("スケール外・カスタムコード入力", "Custom / Non-Diatonic Builder"),
    LOC("ルート音", "Root"),
    LOC("クオリティ", "Quality"),
    LOC("テンション", "Tensions"),
    LOC("ベース (オンコード)", "Bass / Slash"),
    LOC("このコードを追加", "Add Chord"),

    // Center Pane & Timeline
    LOC("★ 全体マージ (Full Song)", "★ Merged (Full Song)"),
    LOC("+ パート追加", "+ Add Part"),
    LOC("パート複製", "Duplicate Part"),
    LOC("パート削除", "Delete Part"),
    LOC("名前の変更", "Rename Part"),
    LOC("キー", "Key"),
    LOC("スケール", "Scale"),
    LOC("ルーラー", "Ruler"),
    LOC("再生位置", "Playhead"),
    LOC("ループ開始", "Loop Start"),
    LOC("ループ終了", "Loop End"),
    LOC("ループ解除", "Clear Loop"),
    LOC("個選択中", "selected"),
    LOC("コピー (Ctrl+C)", "Copy (Ctrl+C)"),
    LOC("切り取り (Ctrl+X)", "Cut (Ctrl+X)"),
    LOC("貼り付け (Ctrl+V)", "Paste (Ctrl+V)"),
    LOC("複製 (Ctrl+D)", "Duplicate (Ctrl+D)"),
    LOC("削除 (Del)", "Delete (Del)"),
    LOC("転調", "Key Change"),

    // Right Pane
    LOC("音楽理論・提案パレット", "Music Theory Palette"),
    LOC("同主調借用和音 (Modal Interchange)", "Modal Interchange (Parallel Key)"),
    LOC("セカンダリードミナント (Secondary Dominant)", "Secondary Dominants"),
    LOC("裏コード (Tritone Substitution)", "Tritone Substitutions"),
    LOC("五度圏 (Circle of Fifths)", "Circle of Fifths"),
    LOC("定番コード進行テンプレート", "Chord Progression Templates"),

    // Voicings
    LOC("基本形 (Root Position)", "Root Position"),
    LOC("ドロップ2 (Drop 2)", "Drop 2"),
    LOC("ドロップ3 (Drop 3)", "Drop 3"),
    LOC("オープン (Open Spread)", "Open Spread"),

    // Instruments
    LOC("グランドピアノ (Grand Piano)", "Grand Piano"),
    LOC("ウォームパッド (Warm Pad)", "Warm Pad"),
    LOC("エレピ (Electric Piano)", "Electric Piano"),
    LOC("ストリングス (Strings)", "Strings"),

    // Modes
    LOC("メジャー (Ionian)", "Major (Ionian)"),
    LOC("ナチュラルマイナー (Aeolian)", "Natural Minor (Aeolian)"),
    LOC("ハーモニックマイナー", "Harmonic Minor"),
    LOC("メロディックマイナー", "Melodic Minor"),
    LOC("ドリアン (Dorian)", "Dorian"),
    LOC("フリジアン (Phrygian)", "Phrygian"),
    LOC("リディアン (Lydian)", "Lydian"),
    LOC("ミクソリディアン (Mixolydian)", "Mixolydian"),
    LOC("ロクリアン (Locrian)", "Locrian"),

    // Templates
    LOC("王道進行 (IV - V - IIIm - VIm)", "Royal Road (IV - V - IIIm - VIm)"),
    LOC("小室進行 (VIm - IV - V - I)", "Komuro (VIm - IV - V - I)"),
    LOC("カノン進行 (I - V - VIm - IIIm - IV - I - IV - V)", "Canon (I - V - VIm - IIIm - IV - I - IV - V)"),
    LOC("2-5-1進行 (IIm7 - V7 - Imaj7)", "Two-Five-One (IIm7 - V7 - Imaj7)"),
    LOC("丸サ進行 (IVmaj7 - III7 - VIm7 - I7)", "Just the Two of Us (IVmaj7 - III7 - VIm7 - I7)"),
    LOC("ポップ定番 (IV - V - IIIm - VIm)", "Standard Pop (IV - V - IIIm - VIm)"),

    // Additional UI Strings
    LOC("新規", "New"),
    LOC("開く", "Open"),
    LOC("保存", "Save"),
    LOC("設定", "Settings"),
    LOC("反映 (Ctrl+Enter)", "Apply (Ctrl+Enter)"),
    LOC("スケール外・カスタムコード", "Custom / Non-Diatonic Builder"),
    LOC("SUGGESTIONS (次候補)", "SUGGESTIONS"),
    LOC("このコードを追加", "Add Chord"),
    LOC("全クリア", "Clear All"),
    LOC("ズーム:", "Zoom:"),
    LOC("コード: ", "Chord: "),
    LOC("長さ: ", "Length: "),
    LOC("構成音: ", "Notes: "),
    LOC("INSPECTOR:", "INSPECTOR:"),
    LOC("★ 全体マージ (Full)", "★ Merged (Full)"),
    LOC("+ パート", "+ Part"),
    LOC("グリッド: ", "Grid: "),
    LOC("拍", " beats"),
    LOC("小節", " bars"),

    // Status
    LOC("準備完了", "Ready"),
    LOC("再生中", "Playing"),
    LOC("MIDIファイルをDAWへドラッグ可能です", "MIDI file ready to drag into DAW"),
    LOC("コードをクリップボードにコピーしました", "Chords copied to clipboard"),
    LOC("コードをタイムラインに貼り付けました", "Chords pasted to timeline")
};

void I18n::SetLanguage(Language lang) {
    s_currentLang = lang;
}

Language I18n::GetLanguage() {
    return s_currentLang;
}

std::wstring I18n::Get(StrId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= sizeof(kStrings) / sizeof(kStrings[0])) return L"";

    const auto& p = kStrings[idx];
    return (s_currentLang == Language::Japanese) ? p.ja : p.en;
}

std::string I18n::GetUtf8(StrId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= sizeof(kStrings) / sizeof(kStrings[0])) return "";

    const auto& p = kStrings[idx];
    return (s_currentLang == Language::Japanese) ? p.ja_u8 : p.en_u8;
}

} // namespace clp
