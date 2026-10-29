#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

#include "music_theory.hpp"
#include "music_engine.hpp"
#include "progression_parser.hpp"
#include "part_manager.hpp"
#include "midi_builder.hpp"
#include "wasapi_synth.hpp"
#include "theme.hpp"
#include "i18n.hpp"

namespace clp {

struct TimelineSelection {
    int startBar = -1;
    int startBeat = -1;
    int endBar = -1;
    int endBeat = -1;
    std::vector<ChordToken> clipboard;

    bool HasSelection() const {
        return startBar >= 0 && endBar >= 0;
    }

    void Clear() {
        startBar = -1;
        startBeat = -1;
        endBar = -1;
        endBeat = -1;
    }
};

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    void RunMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Direct2D / DirectWrite 初期化・リソース作成
    bool CreateDeviceIndependentResources();
    bool CreateDeviceResources();
    void DiscardDeviceResources();

    // 描画ルーチン
    void OnRender();
    void RenderTopToolbarRow1(float width, float height);
    void RenderTopToolbarRow2(float width, float y, float height);
    void RenderLeftPane(float x, float y, float width, float height);
    void RenderCenterPane(float x, float y, float width, float height);
    void RenderRightPane(float x, float y, float width, float height);
    void RenderPianoRoll(float x, float y, float width, float height, float barWidth);
    void RenderPianoKeyboard(float x, float y, float width, float height);
    void RenderInspectorBar(float x, float y, float width, float height);
    void RenderStatusBar(float x, float y, float width, float height);

    // 再生・エンジン制御
    void StartPlayback();
    void StopPlayback();
    void TogglePlayback();
    void SeekToBeat(int totalBeats);
    void AdvancePlayback(float deltaSeconds);
    void ProcessTapTempo();

    // UI操作・イベント
    void OnLButtonDown(int x, int y, WPARAM wParam);
    void OnMouseMove(int x, int y, WPARAM wParam);
    void OnLButtonUp(int x, int y, WPARAM wParam);
    void OnLButtonDblClk(int x, int y, WPARAM wParam);
    void OnMouseWheel(int x, int y, short delta);
    void OnKeyDown(WPARAM wParam, LPARAM lParam);

    // ドロップダウンメニュー
    void ShowKeyMenu(int x, int y);
    void ShowModeMenu(int x, int y);
    void ShowToneMenu(int x, int y);
    void ShowBassMenu(int x, int y);
    void ShowVoicingMenu(int x, int y);
    void ShowGridMenu(int x, int y);
    void ShowSettingsMenu(int x, int y);

    // コピペ・編集
    void CopySelection();
    void CutSelection();
    void PasteAtCursor();
    void DuplicateSelection();
    void DeleteSelection();
    void SelectAll();
    void ApplyTextToTimeline();
    void SyncTimelineToText();
    void AddChordToTimeline(const ChordToken& chord);
    void ApplyTemplate(const MusicEngine::TemplateInfo& tmpl);

    // カード追加時のスクロール追従
    void EnsureVisible(int barIndex);

    // ヘルパー描画
    void DrawCard(
        const D2D1_RECT_F& rect,
        const std::wstring& title,
        const std::wstring& subtitle,
        bool isHovered,
        bool isSelected,
        bool isRest = false
    );

    void DrawRoundBadge(
        const D2D1_RECT_F& rect,
        const std::wstring& title,
        const std::wstring& subtitle,
        bool isHovered,
        bool isSelected
    );

    void DrawButton(
        const D2D1_RECT_F& rect,
        const std::wstring& label,
        bool isHovered,
        bool isPressed,
        bool isAccent = false,
        D2D1_COLOR_F customBg = {0,0,0,0}
    );

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // 左ペインの子ウィンドウ (テキスト入力ボックス)
    HWND m_hwndInputEdit = nullptr;
    HFONT m_hEditFont = nullptr;

    // Direct2D / DirectWrite
    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

    // DirectWrite テキストフォーマット
    IDWriteTextFormat* m_pFontTitle = nullptr;
    IDWriteTextFormat* m_pFontHeader = nullptr;
    IDWriteTextFormat* m_pFontNormal = nullptr;
    IDWriteTextFormat* m_pFontSmall = nullptr;
    IDWriteTextFormat* m_pFontTiny = nullptr;
    IDWriteTextFormat* m_pFontCardTitle = nullptr;
    IDWriteTextFormat* m_pFontCardSub = nullptr;

    // ブラシ
    ID2D1SolidColorBrush* m_pBrushBackground = nullptr;
    ID2D1SolidColorBrush* m_pBrushPaneBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushCardBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushCardHover = nullptr;
    ID2D1SolidColorBrush* m_pBrushCardSelected = nullptr;
    ID2D1SolidColorBrush* m_pBrushBorder = nullptr;
    ID2D1SolidColorBrush* m_pBrushTextPrimary = nullptr;
    ID2D1SolidColorBrush* m_pBrushTextSecondary = nullptr;
    ID2D1SolidColorBrush* m_pBrushAccent = nullptr;
    ID2D1SolidColorBrush* m_pBrushPlay = nullptr;
    ID2D1SolidColorBrush* m_pBrushStop = nullptr;
    ID2D1SolidColorBrush* m_pBrushPlayhead = nullptr;
    ID2D1SolidColorBrush* m_pBrushLoopRange = nullptr;
    ID2D1SolidColorBrush* m_pBrushRest = nullptr;
    ID2D1SolidColorBrush* m_pBrushNoteBar = nullptr;
    ID2D1SolidColorBrush* m_pBrushNoteActive = nullptr;
    ID2D1SolidColorBrush* m_pBrushBlackKey = nullptr;

    // 音楽エンジン & パートマネージャー
    PartManager m_partManager;
    WasapiSynth m_synth;

    // 再生状態
    bool m_isPlaying = false;
    bool m_isLooping = true;
    float m_playbackBeat = 0.0f; // 現在の再生ビート位置 (0.0 から)
    int m_lastSoundedBeat = -1;
    std::chrono::high_resolution_clock::time_point m_lastPlaybackTime;
    std::vector<int> m_currentlySoundingNotes;

    // タップテンポ
    std::vector<std::chrono::high_resolution_clock::time_point> m_tapTimes;

    // ループ / 再生範囲選択
    int m_loopStartBeat = 0;
    int m_loopEndBeat = -1; // -1 の場合は全曲
    bool m_isSelectingLoopRange = false;

    // タイムライン複数選択
    TimelineSelection m_selection;
    int m_selectedCardIndex = -1;

    // ズーム & スクロール
    float m_pixelsPerBeat = 35.0f; // ズーム倍率 (1拍あたりのpx)
    float m_timelineScrollX = 0.0f;
    float m_rightPaneScrollY = 0.0f;

    // ドラッグ＆ドロップ状態
    bool m_isDraggingCard = false;
    ChordToken m_draggedChord;
    POINT m_dragStartPos{0, 0};
    POINT m_dragCurrentPos{0, 0};

    // UI状態
    bool m_isMergedView = false;
    bool m_show7thDiatonic = true;
    int m_selectedBassIndex = 0;   // 0: なし, 1: 1, 2: 2, 3: 両方
    int m_selectedVoicingIndex = 0;
    int m_selectedSoundIndex = 0;
    float m_selectedGridBeats = 1.0f;

    // クイックビルダー状態
    int m_builderRoot = 0;     // 0: C, 1: C#, ...
    int m_builderQuality = 0;  // 0: Maj, 1: min, 2: 7, 3: maj7, ...
    int m_builderTension = 0;  // 0: None, 1: 9, 2: b9, ...
    int m_builderBass = -1;    // -1: None, 0: /C, 1: /C#, ...

    // ステータス文言
    std::wstring m_statusMessage = L"準備完了";
};

} // namespace clp
