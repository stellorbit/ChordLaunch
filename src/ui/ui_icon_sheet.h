#pragma once
// scripts/Generate-Icons.py が自動生成。手で編集しない。
namespace clp {

enum class UiIcon : int {
    Play,
    Pause,
    Stop,
    Loop,
    New,
    Open,
    Save,
    Midi,
    Link,
    Settings,
    Help,
    Template,
    Dawdrag,
    Keyboard,
    Zoom,
    Count
};

// スプライトシートの1アイコンの一辺(px)。resource.h の IDR_UIICONS_* と同じ並び
inline constexpr int kUiIconCellSizes[] = { 24, 48, 96 };

} // namespace clp
