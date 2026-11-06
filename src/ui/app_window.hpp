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
#include "icon_sheet.hpp"

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
    Zoom,
    New,
    Open,
    SaveProject,
    SaveMidi,
    Link
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
    // ダッシュボード各要素の矩形（描画・クリック・ダブルクリックで共通使用）
    struct DashboardLayout {
        D2D1_RECT_F newBox, nameBox, dirBox, browseBtn, createBtn;
        D2D1_RECT_F recentTitle, openFileBtn, listBox;
        D2D1_RECT_F checkbox, openBtn, closeBtn;
        float itemH = 48.0f;
        float itemGap = 2.0f;
        size_t maxItems = 0;
    };
    DashboardLayout GetDashboardLayout(const D2D1_RECT_F& bodyRc) const;
    D2D1_RECT_F GetDashboardBodyRc() const;
    int HitTestDashboardRecent(float x, float y) const;
    bool CreateProjectFromDashboard();
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
        D2D1_COLOR_F primaryColor = {0,0,0,0},
        bool dropdown = false   // 右端に小型のV字（プルダウン表示）を描く
    );
    // 小型のV字（上向き/下向き）。プルダウン表示・スピンボタン用
    void DrawChevronV(const D2D1_RECT_F& rect, ID2D1Brush* pBrush, bool up);
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
    void DrawTextLeft(const std::wstring& text, IDWriteTextFormat* pFont, const D2D1_RECT_F& rc, ID2D1SolidColorBrush* pBrush);
    // 入力欄の文字（左寄せ・上下中央・枠内クリップ）
    void DrawTextInBox(const std::wstring& text, IDWriteTextFormat* pFont, const D2D1_RECT_F& box, ID2D1Brush* pBrush, float padX);
    // ヘルプ用: キーキャップ（kbd 風）。左端 x から描画し、描画後の右端 x を返す
    float DrawKeyCap(const std::wstring& label, float x, float cy, float maxRight, bool large = false);
    // ヘルプ用: "Ctrl + Shift + ホイール" のようなキー列を kbd 風に描画する（large: 大きめの文字・キーキャップ）
    void DrawKeyCombo(const std::wstring& combo, float x, float cy, float maxRight, bool large = false);

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
    void ShiftSelectedChordInversion(int delta);
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
    // テンプレート進行を現在のパートへ展開する（各コードの音価は入力グリッド）
    void ApplyTemplateProgression(const std::string& progression);
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
    void RewindPlayback();
    void AdvancePlayback(float deltaSeconds);
    void SeekToTick(double tick);

    // クリップボード・ファイル・MIDIドラッグ
    void CopyProgressionToClipboard();
    void PasteProgressionFromClipboard();
    bool ConfirmDiscardChanges();
    bool DoFileNew();
    void DoFileOpen();
    void DoFileSave();
    void DoFileSaveAs();
    void DoMidiSaveAs();
    void StartDawDrag();
    void UpdateWindowTitle();
    void SetStatusMessage(const std::wstring& msg);
    void UpdateToolbarTip(float x, float y);
    void TriggerAutoBackup();

    // テンプレート管理・試聴
    void OpenTemplateEditor(bool isEdit);
    void CloseTemplateEditor();
    void SaveTemplateFromEditor();
    void DeleteSelectedUserTemplate();
    void AuditionTemplate(const ProgressionTemplate& tmpl);

    // タイムライン横ズーム（anchorX の下の拍位置を保つ）/ ピアノロール縦ズーム・縦スクロール
    void ZoomTimelineAt(int steps, float anchorX);
    void ZoomPianoRollV(int steps);
    void ScrollPianoRollV(int rows);
    // コードカード領域とピアノロールの境界（分割バー）
    float TimelineAvailHeight() const;
    float SplitBarY() const;
    bool HitTestSplitBar(float x, float y) const;
    // マウス位置の説明（上段Tipsエリアに表示）。該当なしは空
    std::wstring BuildHoverTip(float x, float y);
    // タイムラインのカード（休符を除く）の位置判定。outEdge は左右端の掴み範囲
    bool HitTestTimelineCard(float x, float y, bool& outEdge);

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
    IDWriteTextFormat*     m_pFontHelp = nullptr;      // ヘルプ（キーボードタブ）の説明文 13px
    IDWriteTextFormat*     m_pFontHelpKey = nullptr;   // ヘルプのキーキャップ内文字 12px（中央揃え）
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
    IconSheet              m_iconSheet;
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
    // タイムライン左端の余白(=ピアノロール音名ガイド幅)。スクロール下限を負にして1小節目をガイドの右へ出す
    static constexpr float kTimelineGutter = 28.0f;
    float m_timelineScrollX = -kTimelineGutter;
    float m_pixelsPerBeat = 64.0f;
    int   m_selectedBarIndex = -1;
    int   m_selectedChordIndex = -1;
    std::vector<int> m_multiSelTicks;                    // 複数選択中コードの絶対開始tick（単一選択時は空）
    int   m_selAnchorTick = -1;                          // Shift範囲選択のアンカー
    std::vector<TimelineEdit::RangeItem> m_chordClipboard; // アプリ内コードクリップボード
    DWORD m_chordClipSeq = 0;                            // コピー時のOSクリップボード連番（変化したら無効）

    // リサイズ・ドラッグ＆ドロップ状態
    enum class DragMode { None, CardMove, CardResizeLeft, CardResizeRight, ScrollBarThumb, PaletteCardDrag, CardTimelineDrag, RulerSeek, PianoNoteDrag, SplitDrag };
    DragMode m_dragMode = DragMode::None;

    // パート連動リンク（キー/モード/BPM の変更を同じ値のパートへ一括反映）。Ctrl 押下中は一時的に反転
    bool m_linkEnabled = true;
    bool IsPartLinkActive() const { return m_linkEnabled != ((GetKeyState(VK_CONTROL) & 0x8000) != 0); }

    // ピアノロールのレイアウト（描画時に更新）とノートドラッグ状態
    D2D1_RECT_F m_prRect = {};
    int   m_prMinMidi = 0;
    float m_prNoteRowH = 0.0f;
    bool  m_prValid = false;
    float m_prZoomV = 1.0f;          // ピアノロール縦ズーム倍率 (1.0〜4.0)
    int   m_prScrollRows = 0;        // 縦ズーム時の縦スクロール量（行。正で上方向）
    int   m_selNotePc = -1;          // フォーカス中のノートの音名 (0〜11)。-1 は未選択
    float m_cardSplitRatio = 0.0f;   // コードカード領域の高さ比率。0 以下は自動
    bool  m_splitHover = false;      // 分割バー上にカーソルがある
    int   m_prDragPc = 0;            // ドラッグ中の音名 (0〜11)
    int   m_prDragStartPitch = 0;    // ドラッグ開始時のノート高さ (MIDI)
    int   m_prDragStartShift = 0;    // ドラッグ開始時のオクターブ移動量
    bool  m_prDragUndoPushed = false;
    int   m_prDragMin = 0;           // ドラッグ中に固定する表示音域（音域の自動調整による揺れ防止）
    int   m_prDragMax = 0;
    bool HitTestPianoRollNote(float x, float y, int& outBar, int& outChord, int& outPitch) const;
    void OnPianoNoteDrag(float y);
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
    int m_helpKbCategory = 0;   // キーボードタブの表示カテゴリ（再生/選択/編集/ファイル/表示）
    int m_helpMouseCategory = 0; // マウスタブの表示カテゴリ（カード/ピアノロール/ホイール/画面ツール/リンク）
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
    std::wstring m_dashNewName = L"MyProgression"; // ダッシュボード: 新規プロジェクト名
    bool m_dashNameFocused = false;                // 名前欄の入力フォーカス
    std::wstring m_tipText;          // 上段Tipsエリアに表示するアイコンボタンの説明
    bool m_mouseTracking = false;    // WM_MOUSELEAVE 追跡中
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
