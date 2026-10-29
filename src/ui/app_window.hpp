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
    void RenderTopBar(float width, float height);
    void RenderLeftPane(float x, float y, float width, float height);
    void RenderCenterPane(float x, float y, float width, float height);
    void RenderRightPane(float x, float y, float width, float height);
    void RenderPianoRollAndKeyboard(float x, float y, float width, float height);

    // 再生・エンジン制御
    void StartPlayback();
    void StopPlayback();
    void TogglePlayback();
    void SeekToBeat(int totalBeats);
    void AdvancePlayback(float deltaSeconds);

    // UI操作・イベント
    void OnLButtonDown(int x, int y, WPARAM wParam);
    void OnMouseMove(int x, int y, WPARAM wParam);
    void OnLButtonUp(int x, int y, WPARAM wParam);
    void OnLButtonDblClk(int x, int y, WPARAM wParam);
    void OnMouseWheel(int x, int y, short delta);
    void OnKeyDown(WPARAM wParam, LPARAM lParam);

    // コピペ・編集
    void CopySelection();
    void CutSelection();
    void PasteAtCursor();
    void DuplicateSelection();
    void DeleteSelection();
    void SelectAll();

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

    void DrawButton(
        const D2D1_RECT_F& rect,
        const std::wstring& label,
        bool isHovered,
        bool isPressed,
        bool isAccent = false
    );

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // Direct2D / DirectWrite
    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

    // DirectWrite テキストフォーマット
    IDWriteTextFormat* m_pFontTitle = nullptr;
    IDWriteTextFormat* m_pFontHeader = nullptr;
    IDWriteTextFormat* m_pFontNormal = nullptr;
    IDWriteTextFormat* m_pFontSmall = nullptr;
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
    ID2D1SolidColorBrush* m_pBrushPlayhead = nullptr;
    ID2D1SolidColorBrush* m_pBrushLoopRange = nullptr;
    ID2D1SolidColorBrush* m_pBrushRest = nullptr;

    // 音楽エンジン & パートマネージャー
    PartManager m_partManager;
    WasapiSynth m_synth;

    // 再生状態
    bool m_isPlaying = false;
    bool m_isLooping = true;
    float m_playbackBeat = 0.0f; // 現在の再生ビート位置 (0.0 から)
    int m_lastSoundedBeat = -1;
    std::chrono::high_resolution_clock::time_point m_lastPlaybackTime;

    // ループ / 再生範囲選択
    int m_loopStartBeat = 0;
    int m_loopEndBeat = -1; // -1 の場合は全曲
    bool m_isSelectingLoopRange = false;

    // タイムライン複数選択
    TimelineSelection m_selection;
    bool m_isRubberbanding = false;
    POINT m_rubberbandStart{0, 0};
    POINT m_rubberbandEnd{0, 0};

    // スクロールオフセット
    float m_timelineScrollX = 0.0f;
    float m_rightPaneScrollY = 0.0f;
    float m_leftPaneScrollY = 0.0f;

    // ドラッグ＆ドロップ状態
    bool m_isDraggingCard = false;
    ChordToken m_draggedChord;
    POINT m_dragStartPos{0, 0};
    POINT m_dragCurrentPos{0, 0};

    // UI状態
    bool m_isMergedView = false;
    bool m_show7thDiatonic = true;
    int m_selectedKeyIndex = 0;   // 0: C, 1: C#, ...
    int m_selectedModeIndex = 0;  // 0: Major, 1: Minor
    int m_selectedVoicingIndex = 0;
    int m_selectedSoundIndex = 0;

    // クイックビルダー状態
    int m_builderRoot = 0;     // 0: C, 1: C#, ...
    int m_builderQuality = 0;  // 0: Maj, 1: min, 2: 7, 3: maj7, ...
    int m_builderTension = 0;  // 0: None, 1: 9, 2: b9, ...
    int m_builderBass = -1;    // -1: None, 0: /C, 1: /C#, ...

    // テキスト入力同期用エディットコントロール
    HWND m_hwndTextEdit = nullptr;
};

} // namespace clp
