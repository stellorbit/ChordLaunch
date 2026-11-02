#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace clp {

enum class Language {
    Japanese = 0,
    English
};

enum class StrId {
    AppTitle,
    // Top Bar
    Play,
    Stop,
    Loop,
    Bpm,
    TapTempo,
    TimeSig,
    Voicing,
    SoundPreset,
    MasterVolume,
    ExportMidiDaw,
    DragToDawTooltip,
    Undo,
    Redo,
    Save,
    Load,
    LangSwitch,

    // Left Pane
    TextEditorTitle,
    TextEditorHint,
    DiatonicTitle,
    ShowTriads,
    Show7ths,
    RestChord,
    QuickBuilderTitle,
    RootLabel,
    QualityLabel,
    TensionLabel,
    BassLabel,
    AddCurrentChord,

    // Center Pane & Timeline
    FullSongTab,
    AddPart,
    DuplicatePart,
    DeletePart,
    RenamePart,
    KeyLabel,
    ScaleLabel,
    BarRuler,
    Playhead,
    LoopRangeStart,
    LoopRangeEnd,
    ClearLoopRange,
    MultiSelectCount,
    Copy,
    Cut,
    Paste,
    DuplicateSelection,
    DeleteSelection,
    KeyChangeMarker,

    // Right Pane
    TheoryPaletteTitle,
    ModalInterchange,
    SecondaryDominants,
    TritoneSubstitutions,
    CircleOfFifths,
    Templates,

    // Voicings
    VoicingRoot,
    VoicingDrop2,
    VoicingDrop3,
    VoicingOpen,

    // Instruments
    InstPiano,
    InstPad,
    InstEPiano,
    InstStrings,

    // Modes
    ModeMajor,
    ModeNaturalMinor,
    ModeHarmonicMinor,
    ModeMelodicMinor,
    ModeDorian,
    ModePhrygian,
    ModeLydian,
    ModeMixolydian,
    ModeLocrian,

    // Templates
    TmplJPopRoyal,     // 王道進行 (IV - V - IIIm - VIm)
    TmplJPopKomuro,    // 小室進行 (VIm - IV - V - I)
    TmplCanon,         // カノン進行 (I - V - VIm - IIIm - IV - I - IV - V)
    TmplPop251,        // Two-Five-One (IIm7 - V7 - Imaj7)
    TmplJPopOwari,     // 丸サ進行 (IVmaj7 - III7 - VIm7 - I7)
    TmplStandardPop,   // 4536 (IV - V - IIIm - VIm)

    // Additional UI Strings
    FileNew,
    FileOpen,
    FileSave,
    Settings,
    ApplyCtrlEnter,
    CustomBuilder,
    SuggestionsTitle,
    AddThisChord,
    ClearAll,
    ZoomLabel,
    InspectorChord,
    InspectorLength,
    InspectorNotes,
    InspectorTitle,
    PartFullSong,
    PartAddBtn,
    GridBeats,
    BeatUnit,
    BarUnit,

    // Status
    StatusReady,
    StatusPlaying,
    StatusMidiExported,
    StatusCopied,
    StatusPasted
};

class I18n {
public:
    static void SetLanguage(Language lang);
    static Language GetLanguage();
    static std::wstring Get(StrId id);
    static std::string GetUtf8(StrId id);

private:
    static Language s_currentLang;
};

} // namespace clp
