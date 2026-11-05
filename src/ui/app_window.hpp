#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <memory>
#include <optional>
#include "music_theory.hpp"
#include "music_engine.hpp"
#include "part_manager.hpp"
#include "timeline_edit.hpp"
#include "wasapi_synth.hpp"

namespace clp {

enum class IconType {
    None = 0,
    Play,
    Pause,
    Stop,
    Loop,
    Settings,
    Music,
    Close,
    Plus,
    ChevronUp,
    ChevronDown,
    Template,
    Help,
    SaveDisk,
    DawDrag,
    Keyboard,
    Zoom
};

enum class ModalType {
    None = 0,
    Dashboard,
    Settings,
    Templates,
    Help,
    TapTempo,
    RenamePart,
    About
};

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    int Run();
    int RunMessageLoop() { return Run(); }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Direct2D / DirectWrite リソース管理
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    // 描画ルーチン
    void OnRender();
    void RenderToolbar(float x, float y, float width, float height);
    void RenderLeftPane(float x, float y, float width, float height);
    void RenderCenterPane(float x, float y, float width, float height);
    void RenderPartTabs(float x, float y, float width, float height);
    void RenderTimelineCards(float x, float y, float width, float height);
    void RenderPianoRoll(float x, float y, float width, float height);
    void RenderBottomDock(float x, float y, float width, float height);
    void RenderRightPane(float x, float y, float width, float height);
    void RenderTimelineScrollBar(float x, float y, float width, float height);

    // モーダルダイアログ描画ルーチン
    void RenderModal(float width, float height);
    void RenderDashboardModal(const D2D1_RECT_F& rc);
    void RenderSettingsModal(const D2D1_RECT_F& rc);
    void RenderTemplatesModal(const D2D1_RECT_F& rc);
    void RenderHelpModal(const D2D1_RECT_F& rc);
    void RenderAboutModal(const D2D1_RECT_F& rc);
    void RenderTapTempoModal(const D2D1_RECT_F& rc);
    void RenderRenamePartModal(const D2D1_RECT_F& rc);

    // UIヘルパー描画
    void DrawButton(
        const D2D1_RECT_F& rect,
        const std::wstring& text,
        IconType icon = IconType::None,
        bool isHovered = false,
        bool isPressed = false,
        bool isPrimary = false,
        D2D1_COLOR_F primaryColor = {0,0,0,0}
    );
    void DrawCard(
        const D2D1_RECT_F& rect,
        const std::wstring& chordName,
        const std::wstring& degree,
        bool isHovered,
        bool isSelected,
        bool isRest = false,
        bool showCloseButton = false,
        bool isCloseHovered = false
    );
    void DrawSectionHeader(
        const D2D1_RECT_F& rect,
        const std::wstring& title,
        bool isOpen,
        bool isHovered = false
    );
    void DrawTextSafe(
        const std::wstring& text,
        IDWriteTextFormat* pFormat,
        const D2D1_RECT_F& rect,
        ID2D1Brush* pBrush
    );

    // ベクター図形アイコン描画ルーチン (絵文字不使用)
    void DrawIcon(IconType type, const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconPlay(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconPause(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconStop(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconLoop(const D2D1_RECT_F& rect, ID2D1Brush* pBrush, bool active);
    void DrawIconSettings(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconMusic(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconClose(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconPlus(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconChevron(const D2D1_RECT_F& rect, ID2D1Brush* pBrush, bool open);
    void DrawIconTemplate(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconHelp(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconSaveDisk(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconDawDrag(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconKeyboard(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);
    void DrawIconZoom(const D2D1_RECT_F& rect, ID2D1Brush* pBrush);

    // イベント・入力ハンドラ
    void OnLButtonDown(float x, float y);
    void OnLButtonDblClk(float x, float y);
    void OnLButtonUp(float x, float y);
    void OnRButtonUp(float x, float y);
    void OnMouseMove(float x, float y);
    void OnMouseWheel(short delta, float x, float y);
    void OnKeyDown(WPARAM key);
    void OnChar(WCHAR ch);
    void OnSize(UINT width, UINT height);
    bool HitTestPaletteCard(float x, float y, ChordToken& outChord);
    void InsertChordAtTimelineX(const ChordToken& chord, float x, bool pushUndo = true, int snapTicks = 0);
    void MoveChordToTimelineX(int srcBar, int srcChord, float x);
    // x座標 → グリッドにスナップした絶対tick（小節内の上限・前小節のカバー範囲による制限なし。曲末より後ろも可）
    int  TimelineTickFromX(float x, int gridTicks, int* outBar = nullptr, int* outTickInBar = nullptr, int* outBarTicks = nullptr) const;
    // 曲末が endTick に満たない場合、休符のみの小節を末尾に足す
    void EnsureBarsCoverTick(std::vector<Bar>& bars, int endTick) const;
    void InsertChordAtTimelinePosition(const ChordToken& chord, int targetBarIdx, int targetBeat);
    void InsertChordAtTimelineTick(const ChordToken& chord, int targetBarIdx, int targetTickInBar, bool pushUndo = true);
    void SplitSelectedChord();
    void DuplicateSelectedChord();
    void ShiftSelectedChordOctave(int delta);
    void ResetSelectedChordOctave();
    void AddRecentFile(const std::wstring& path);
    void LoadAppSettings();
    void SaveAppSettings();
    void OpenRecentFile(int index);
    void ChooseDirectory(std::wstring& outDir, const wchar_t* title);
    void DoMergeAllToNewPart();
    void OpenRenamePartModal(int partIndex);
    // 自由位置マーカー（tick 単位）。編集には RenamePart モーダルを流用する。
    // パート先頭（tick 0 付近）はパート名変更へ振り替える。
    void OpenMarkerModal(int tick);
    void AddMarkerAtPlayhead();      // M キー: 再生ヘッド位置（グリッドへスナップ）にマーカーを追加/編集
    void ClearMarker(int tick);
    // ルーラー上の x 座標を、アクティブパート内の tick に変換する（パート外・全体表示は -1）
    int HitTestRulerTick(float x) const;
    // 既存マーカーを指しているとみなす tick 幅（約 8px 相当）
    int MarkerHitToleranceTicks() const;
    // 表示中のビュー（パート / 全体）のマーカー一覧
    std::vector<PartMarker> CurrentViewMarkers() const;
    // RenamePart モーダルの確定（パート名 / マーカー）。空のパート名は無視。
    void CommitRenameModal();
    void OpenDashboardModal();

    // プルダウンメニュー表示・メニューバー言語更新
    void RebuildMainMenu();
    void ShowKeyMenu(float screenX, float screenY);
    void ShowScaleMenu(float screenX, float screenY);
    void ShowPresetMenu(float screenX, float screenY);
    void ShowVoicingMenu(float screenX, float screenY);
    void ShowBassMenu(float screenX, float screenY);
    void ShowGridMenu(float screenX, float screenY);
    void ShowInputGridMenu(float screenX, float screenY);
    void ShowDiatonicVariantMenu(float screenX, float screenY);

    const wchar_t* TR(const wchar_t* ja, const wchar_t* en) const {
        return (m_settingLanguage == 1) ? en : ja;
    }

    // タップテンポ処理
    void HandleTapTempo();
    void ConfirmTapTempo();

    // モーダルイベントハンドラ
    bool OnModalClick(float x, float y);

    // タイムライン・コード操作
    void SelectChord(int barIndex, int chordIndex);
    void AuditionChord(const ChordToken& chord);
    void AppendChordToTimeline(const ChordToken& chord);
    void SyncTextToTimeline();
    void SyncTimelineToText();
    void PushUndo();
    void Undo();
    void Redo();
    void DuplicateSelection();
    void DeleteSelection();
    void SelectAll();
    void EnsureVisible(int barIndex);

    // パートのキー/スケール変更後に、全コードの度数表記をそのパートのキーで計算し直す
    void RefreshPartDegrees(Part& part);

    // 選択解除・複数選択・コード単位のクリップボード
    void ClearSelection();
    int  PrimaryChordStartTick() const;               // 主選択コードの絶対開始tick（なし/休符は -1）
    std::vector<int> GetSelectedStartTicks() const;   // 複数選択があればそれ、なければ主選択のみ
    void ToggleChordSelection(int barIndex, int chordIndex); // Ctrl+クリック
    void ExtendSelectionTo(int barIndex, int chordIndex);    // Shift+クリック（アンカーから範囲）
    void DoEditCut();
    void DoEditCopy();
    void DoEditPaste();

    // コード提案の基準: 直前パート（空パートはさかのぼる）の最終コード（接続先 = 表示中パート）。
    // 直前パートに最終コードがなければ、表示中パート自身の最終コード（接続先なし）。選択コードには依存しない。
    bool FindSuggestionBase(ChordToken& outChord, const Part*& outBasePart, const Part*& outTargetPart) const;
    // 繋ぎコード（セカンダリードミナント）: 接続先（表示中パート）のキーで生成する
    std::vector<ChordToken> BuildSecondaryList() const;
    std::vector<ChordToken> BuildSuggestionList(bool use7th) const;
    std::vector<ChordToken> BuildBorrowedList(bool use7th) const;

    // 再生エンジン (tick単位精密制御)
    void TogglePlayback();
    void StopPlayback();
    void AdvancePlayback(float deltaSeconds);
    void SeekToTick(double tick);

    // クリップボード・ファイル・MIDIドラッグ
    void CopyProgressionToClipboard();
    void PasteProgressionFromClipboard();
    bool ConfirmDiscardChanges();
    void DoFileNew();
    void DoFileOpen();
    void DoFileSave();
    void DoFileSaveAs();
    void DoMidiSaveAs();
    void StartDawDrag();
    void UpdateWindowTitle();
    void SetStatusMessage(const std::wstring& msg);
    void TriggerAutoBackup();

    // テンプレート管理・試聴
    void OpenTemplateEditor(bool isEdit);
    void CloseTemplateEditor();
    void SaveTemplateFromEditor();
    void DeleteSelectedUserTemplate();
    void AuditionTemplate(const ProgressionTemplate& tmpl);

    // ヘルプ・ドキュメント
    void OpenOnlineDocumentation();

    // モーダル開閉
    void OpenModal(ModalType modal);
    void CloseModal();

private:
    HWND m_hwnd = nullptr;
    HWND m_hwndEdit = nullptr;
    HFONT m_hFontEdit = nullptr;
    WNDPROC m_origEditWndProc = nullptr;
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HMENU m_hMainMenu = nullptr;

    // Direct2D / DirectWrite インターフェース
    ID2D1Factory*          m_pD2DFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    IDWriteFactory*        m_pDWriteFactory = nullptr;

    // DirectWrite テキストフォーマット
    IDWriteTextFormat*     m_pFontMain = nullptr;
    IDWriteTextFormat*     m_pFontButton = nullptr;
    IDWriteTextFormat*     m_pFontCardTitle = nullptr;
    IDWriteTextFormat*     m_pFontCardDegree = nullptr;
    IDWriteTextFormat*     m_pFontRuler = nullptr;
    IDWriteTextFormat*     m_pFontInspector = nullptr;
    IDWriteTextFormat*     m_pFontTab = nullptr;
    IDWriteTextFormat*     m_pFontModalTitle = nullptr;
    IDWriteTextFormat*     m_pFontModalBody = nullptr;
    IDWriteTextFormat*     m_pFontBigNumber = nullptr;

    // ソリッドブラシ
    ID2D1SolidColorBrush*  m_pBrushBg = nullptr;
    ID2D1SolidColorBrush*  m_pBrushText = nullptr;
    ID2D1SolidColorBrush*  m_pBrushTextMuted = nullptr;
    ID2D1SolidColorBrush*  m_pBrushBorder = nullptr;
    ID2D1SolidColorBrush*  m_pBrushBorderLight = nullptr;
    ID2D1SolidColorBrush*  m_pBrushCardBg = nullptr;
    ID2D1SolidColorBrush*  m_pBrushCardActive = nullptr;
    ID2D1SolidColorBrush*  m_pBrushCardSelected = nullptr;
    ID2D1SolidColorBrush*  m_pBrushAccent = nullptr;
    ID2D1SolidColorBrush*  m_pBrushPlay = nullptr;
    ID2D1SolidColorBrush*  m_pBrushStop = nullptr;
    ID2D1SolidColorBrush*  m_pBrushPlayhead = nullptr;
    ID2D1SolidColorBrush*  m_pBrushPianoRollNote = nullptr;
    ID2D1SolidColorBrush*  m_pBrushPianoRollRest = nullptr;
    ID2D1SolidColorBrush*  m_pBrushModalBackdrop = nullptr;

    // アプリケーション状態
    PartManager m_partManager;
    WasapiSynth m_synth;

    // UIレイアウト寸法 (ツールバー2段化・コンパクト縦幅)
    float m_width = 1280.0f;
    float m_height = 670.0f;
    float m_toolbarHeight = 74.0f; // 2段ツールバー (上段38px + 下段36px)
    float m_leftPaneWidth = 280.0f;
    float m_rightPaneWidth = 270.0f;
    float m_partTabHeight = 28.0f;
    float m_rulerHeight = 22.0f;
    float m_cardTrackHeight = 76.0f;
    float m_bottomDockHeight = 68.0f;
    float m_scrollBarHeight = 12.0f;

    // タイムライン表示パラメータ (tickベース)
    float m_timelineScrollX = 0.0f;
    float m_pixelsPerBeat = 64.0f;
    int   m_selectedBarIndex = -1;
    int   m_selectedChordIndex = -1;
    std::vector<int> m_multiSelTicks;                    // 複数選択中コードの絶対開始tick（単一選択時は空）
    int   m_selAnchorTick = -1;                          // Shift範囲選択のアンカー
    std::vector<TimelineEdit::RangeItem> m_chordClipboard; // アプリ内コードクリップボード
    DWORD m_chordClipSeq = 0;                            // コピー時のOSクリップボード連番（変化したら無効）

    // リサイズ・ドラッグ＆ドロップ状態
    enum class DragMode { None, CardMove, CardResizeLeft, CardResizeRight, ScrollBarThumb, PaletteCardDrag, CardTimelineDrag, RulerSeek };
    DragMode m_dragMode = DragMode::None;
    int   m_dragBarIndex = -1;
    int   m_dragChordIndex = -1;
    float m_dragStartX = 0.0f;
    float m_dragStartY = 0.0f;
    float m_dragCurrentX = 0.0f;
    float m_dragCurrentY = 0.0f;
    bool  m_isPaletteDragging = false;
    ChordToken m_dragPaletteChord;
    int   m_dragChordStartTick = 0;
    int   m_dragChordEndTick = 0;
    int   m_dragChordStartGlobal = 0;
    int   m_dragChordEndGlobal = 0;
    std::vector<Bar> m_dragBaseBars;     // リサイズ開始時点のパートスナップショット
    float m_dragCardGrabOffsetX = 0.0f; // カード左端からのクリック位置オフセット

    // 再生状態 (tick時間管理)
    bool   m_isPlaying = false;
    bool   m_isLooping = false;
    bool   m_isMergedView = false;
    double m_playbackTick = 0.0;
    int    m_lastSoundedChordUid = -1;
    std::chrono::steady_clock::time_point m_lastPlaybackTime;

    // 音声・ボイシング設定
    VoicingStyle m_selectedVoicing = VoicingStyle::RootPosition;
    int m_baseOctave = 4;
    int m_bassOctaveMode = 0;
    int m_inputGridTicks = TICKS_PER_BEAT;      // コード入力時の音価グリッド
    int m_timelineGridTicks = TICKS_PER_BEAT;   // タイムラインのスナップ/表示グリッド

    // 右ペインのアコーディオン開閉状態
    bool m_accSuggestionsOpen = true;
    bool m_accBorrowOpen = true;
    bool m_accSecondaryOpen = true;
    bool m_accTemplatesOpen = true;

    // 左ペイン・カスタムコードビルダー状態
    PitchClass m_customRoot = PitchClass::C;
    std::string m_customSuffix = "";
    int8_t m_customTension = 0;
    PitchClass m_customBass = PitchClass::Unknown;
    int m_diatonicChordTypeTab = 0; // 0=3和音 / 1=7th / 2以上=「その他」バリエーション (MusicEngine::GetDiatonicChordsByTab の tabIndex)
    bool m_accCustomBuilderOpen = true;

    // 表示切り替え
    bool m_showPianoRoll = true;
    bool m_showKeyboard = true;
    bool m_showRightPane = true;

    // モーダルダイアログ状態
    ModalType m_activeModal = ModalType::None;
    int m_settingLanguage = 0;
    int m_settingZoomStyle = 0;
    int m_selectedTmplCategoryIndex = 0;
    int m_selectedTmplIndex = 0;
    int m_tmplListScroll = 0;
    int m_helpActiveTab = 0;
    int m_contextPartIndex = -1;
    int m_renamePartIndex = -1;
    int m_renameMarkerTick = -1;  // >0: RenamePart モーダルをマーカー編集（その tick）として使用中
    int m_contextMarkerTick = -1; // ルーラー右クリックメニューの対象 tick
    bool m_swallowMarkerChar = false; // M キーでモーダルを開いた直後の WM_CHAR を1回無視する
    std::wstring m_renamePartName;
    bool m_isCardResizingHover = false;

    // テンプレート追加・編集サブダイアログ状態
    bool m_tmplEditorOpen = false;
    bool m_tmplIsEditingExisting = false;
    std::string m_tmplEditId;
    std::wstring m_tmplEditName;
    std::wstring m_tmplEditProg;
    std::wstring m_tmplEditDesc;
    int m_tmplEditFocusField = 0;

    // プロジェクト保存・自動バックアップ・ダッシュボード設定
    std::wstring m_currentProjectPath;
    std::string m_projectTitle = "MyProgression";
    bool m_isDirty = false;
    std::vector<std::wstring> m_recentFiles;
    std::wstring m_projectDirectory;
    std::wstring m_autoBackupDirectory;
    bool m_autoBackupInProjectDir = true;
    bool m_showDashboardOnStartup = true;
    int m_dashboardSelectedRecentIndex = -1;
    std::wstring m_statusMessage;
    std::chrono::steady_clock::time_point m_statusMessageTime;

    // タップテンポ測定状態
    std::vector<std::chrono::steady_clock::time_point> m_tapTimes;
    int m_measuredBpm = 120;
    bool m_tapBlinked = false;
    std::chrono::steady_clock::time_point m_lastTapBlinkTime;

    // Undo / Redo スタック
    struct UndoSnapshot {
        std::vector<Part> parts;
        int activePartIndex;
    };
    std::vector<UndoSnapshot> m_undoStack;
    std::vector<UndoSnapshot> m_redoStack;
};

} // namespace clp
