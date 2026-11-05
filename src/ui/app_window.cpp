#include "app_window_internal.hpp"
#include "../resource.h"
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
namespace clp {
// ---------------------------------------------------------------------------
// AppWindow ライフサイクル
// ---------------------------------------------------------------------------
AppWindow::AppWindow() {
    m_undoStack.reserve(32);
    m_redoStack.reserve(32);
    m_tapTimes.reserve(16);
}

AppWindow::~AppWindow() {
    DiscardDeviceResources();

    if (m_pFontMain) { m_pFontMain->Release(); m_pFontMain = nullptr; }
    if (m_pFontButton) { m_pFontButton->Release(); m_pFontButton = nullptr; }
    if (m_pFontCardTitle) { m_pFontCardTitle->Release(); m_pFontCardTitle = nullptr; }
    if (m_pFontCardDegree) { m_pFontCardDegree->Release(); m_pFontCardDegree = nullptr; }
    if (m_pFontRuler) { m_pFontRuler->Release(); m_pFontRuler = nullptr; }
    if (m_pFontInspector) { m_pFontInspector->Release(); m_pFontInspector = nullptr; }
    if (m_pFontTab) { m_pFontTab->Release(); m_pFontTab = nullptr; }
    if (m_pFontModalTitle) { m_pFontModalTitle->Release(); m_pFontModalTitle = nullptr; }
    if (m_pFontModalBody) { m_pFontModalBody->Release(); m_pFontModalBody = nullptr; }
    if (m_pFontBigNumber) { m_pFontBigNumber->Release(); m_pFontBigNumber = nullptr; }

    if (m_hFontEdit) { DeleteObject(m_hFontEdit); m_hFontEdit = nullptr; }
    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

HRESULT AppWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return hr;

    const WCHAR* chosenFamily = Theme::FontName;

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.0f, L"ja-jp", &m_pFontMain
    );

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"ja-jp", &m_pFontButton
    );
    if (m_pFontButton) {
        m_pFontButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontButton->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_pFontButton->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"ja-jp", &m_pFontCardTitle
    );
    if (m_pFontCardTitle) {
        m_pFontCardTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontCardTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_pFontCardTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.0f, L"ja-jp", &m_pFontCardDegree
    );
    if (m_pFontCardDegree) {
        m_pFontCardDegree->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontCardDegree->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_pFontCardDegree->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.0f, L"ja-jp", &m_pFontRuler
    );

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"ja-jp", &m_pFontInspector
    );

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"ja-jp", &m_pFontTab
    );
    if (m_pFontTab) {
        m_pFontTab->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontTab->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_pFontTab->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        15.0f, L"ja-jp", &m_pFontModalTitle
    );

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"ja-jp", &m_pFontModalBody
    );

    m_pDWriteFactory->CreateTextFormat(
        chosenFamily, nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        28.0f, L"ja-jp", &m_pFontBigNumber
    );
    if (m_pFontBigNumber) {
        m_pFontBigNumber->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontBigNumber->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    return S_OK;
}

HRESULT AppWindow::CreateDeviceResources() {
    if (m_pRenderTarget) return S_OK;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pRenderTarget
    );
    if (FAILED(hr)) return hr;

    m_pRenderTarget->CreateSolidColorBrush(Theme::WindowBackground, &m_pBrushBg);
    m_pRenderTarget->CreateSolidColorBrush(Theme::TextPrimary, &m_pBrushText);
    m_pRenderTarget->CreateSolidColorBrush(Theme::TextMuted, &m_pBrushTextMuted);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardBorder, &m_pBrushBorder);
    m_pRenderTarget->CreateSolidColorBrush(Theme::PillBorder, &m_pBrushBorderLight);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardBackground, &m_pBrushCardBg);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardActive, &m_pBrushCardActive);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardSelected, &m_pBrushCardSelected);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentAction, &m_pBrushAccent);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentPlay, &m_pBrushPlay);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentStop, &m_pBrushStop);
    m_pRenderTarget->CreateSolidColorBrush(Theme::Playhead, &m_pBrushPlayhead);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentAction, &m_pBrushPianoRollNote);
    m_pRenderTarget->CreateSolidColorBrush(Theme::RestBlockFill, &m_pBrushPianoRollRest);
    m_pRenderTarget->CreateSolidColorBrush(Theme::ModalBackdrop, &m_pBrushModalBackdrop);

    return S_OK;
}

void AppWindow::DiscardDeviceResources() {
    if (m_pBrushModalBackdrop) { m_pBrushModalBackdrop->Release(); m_pBrushModalBackdrop = nullptr; }
    if (m_pBrushPianoRollRest) { m_pBrushPianoRollRest->Release(); m_pBrushPianoRollRest = nullptr; }
    if (m_pBrushPianoRollNote) { m_pBrushPianoRollNote->Release(); m_pBrushPianoRollNote = nullptr; }
    if (m_pBrushPlayhead) { m_pBrushPlayhead->Release(); m_pBrushPlayhead = nullptr; }
    if (m_pBrushStop) { m_pBrushStop->Release(); m_pBrushStop = nullptr; }
    if (m_pBrushPlay) { m_pBrushPlay->Release(); m_pBrushPlay = nullptr; }
    if (m_pBrushAccent) { m_pBrushAccent->Release(); m_pBrushAccent = nullptr; }
    if (m_pBrushCardSelected) { m_pBrushCardSelected->Release(); m_pBrushCardSelected = nullptr; }
    if (m_pBrushCardActive) { m_pBrushCardActive->Release(); m_pBrushCardActive = nullptr; }
    if (m_pBrushCardBg) { m_pBrushCardBg->Release(); m_pBrushCardBg = nullptr; }
    if (m_pBrushBorderLight) { m_pBrushBorderLight->Release(); m_pBrushBorderLight = nullptr; }
    if (m_pBrushBorder) { m_pBrushBorder->Release(); m_pBrushBorder = nullptr; }
    if (m_pBrushTextMuted) { m_pBrushTextMuted->Release(); m_pBrushTextMuted = nullptr; }
    if (m_pBrushText) { m_pBrushText->Release(); m_pBrushText = nullptr; }
    if (m_pBrushBg) { m_pBrushBg->Release(); m_pBrushBg = nullptr; }

    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

// ---------------------------------------------------------------------------
// 文字列安全描画ヘルパー (文字化け・バッファ越え完全防止)
// ---------------------------------------------------------------------------
void AppWindow::DrawTextSafe(
    const std::wstring& text,
    IDWriteTextFormat* pFormat,
    const D2D1_RECT_F& rect,
    ID2D1Brush* pBrush
) {
    if (text.empty() || !m_pRenderTarget || !pFormat || !pBrush) return;
    m_pRenderTarget->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), pFormat, rect, pBrush);
}

// ---------------------------------------------------------------------------
// ウィンドウ作成とメッセージループ
// ---------------------------------------------------------------------------
void AppWindow::RebuildMainMenu() {
    HMENU hNewMenu = CreateMenu();

    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, TR(L"新規(&N)\tCtrl+N", L"New(&N)\tCtrl+N"));
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, TR(L"開く(&O)...\tCtrl+O", L"Open(&O)...\tCtrl+O"));
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_DASHBOARD, TR(L"ダッシュボード(&D)...", L"Dashboard(&D)..."));

    // 最近使ったファイル (MRU) サブメニュー
    HMENU hRecentMenu = CreatePopupMenu();
    if (m_recentFiles.empty()) {
        AppendMenuW(hRecentMenu, MF_STRING | MF_GRAYED, 0, TR(L"(履歴なし)", L"(No Recent Files)"));
    } else {
        for (size_t ri = 0; ri < std::min<size_t>(m_recentFiles.size(), 10); ++ri) {
            std::filesystem::path fp(m_recentFiles[ri]);
            std::wstring itemText = std::to_wstring(ri + 1) + L": " + fp.filename().wstring();
            AppendMenuW(hRecentMenu, MF_STRING, IDM_FILE_MRU_BASE + ri, itemText.c_str());
        }
    }
    AppendMenuW(hFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hRecentMenu), TR(L"最近使ったファイル(&R)", L"Recent Files(&R)"));

    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, TR(L"保存(&S)\tCtrl+S", L"Save(&S)\tCtrl+S"));
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, TR(L"名前を付けて保存(&A)...", L"Save As(&A)..."));
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXPORT_MIDI, TR(L"MIDIエクスポート(&M)...", L"Export MIDI(&M)..."));
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, TR(L"終了(&X)", L"Exit(&X)"));
    AppendMenuW(hNewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), TR(L"ファイル(&F)", L"File(&F)"));

    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_UNDO, TR(L"元に戻す(&U)\tCtrl+Z", L"Undo(&U)\tCtrl+Z"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REDO, TR(L"やり直し(&R)\tCtrl+Y", L"Redo(&R)\tCtrl+Y"));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CUT, TR(L"切り取り(&T)\tCtrl+X", L"Cut(&T)\tCtrl+X"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY, TR(L"コピー(&C)\tCtrl+C", L"Copy(&C)\tCtrl+C"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE, TR(L"貼り付け(&P)\tCtrl+V", L"Paste(&P)\tCtrl+V"));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_SPLIT, TR(L"選択コードの分割(&S)\tB", L"Split Chord(&S)\tB"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_DUPLICATE, TR(L"選択コードの複製(&D)\tCtrl+D", L"Duplicate Chord(&D)\tCtrl+D"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_DELETE, TR(L"選択コードの削除(&D)\tDel", L"Delete Chord(&D)\tDel"));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_OCTAVE_UP, TR(L"選択コードをオクターブ上へ(&U)\tCtrl+↑", L"Octave Up(&U)\tCtrl+Up"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_OCTAVE_DOWN, TR(L"選択コードをオクターブ下へ(&L)\tCtrl+↓", L"Octave Down(&L)\tCtrl+Down"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_OCTAVE_RESET, TR(L"選択コードのオクターブをリセット", L"Reset Chord Octave"));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_ADD_MARKER, TR(L"再生ヘッド位置にマーカーを追加\tM", L"Add Marker at Playhead\tM"));
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_SELECTALL, TR(L"すべて選択(&A)\tCtrl+A", L"Select All(&A)\tCtrl+A"));
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CLEARALL, TR(L"全クリア(&C)", L"Clear All(&C)"));
    AppendMenuW(hNewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hEditMenu), TR(L"編集(&E)", L"Edit(&E)"));

    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_PIANOROLL, TR(L"鍵盤プレビュー表示切替", L"Toggle Keyboard Preview"));
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_RIGHTPANE, TR(L"コード提案パネル表示切替", L"Toggle Chord Suggestions"));
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMIN, TR(L"ズームイン\tCtrl++", L"Zoom In\tCtrl++"));
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMOUT, TR(L"ズームアウト\tCtrl+-", L"Zoom Out\tCtrl+-"));
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ZOOMRESET, TR(L"ズームリセット\tCtrl+0", L"Reset Zoom\tCtrl+0"));
    AppendMenuW(hNewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hViewMenu), TR(L"表示(&V)", L"View(&V)"));

    HMENU hPlayMenu = CreatePopupMenu();
    AppendMenuW(hPlayMenu, MF_STRING, IDM_PLAY_PLAYPAUSE, TR(L"再生/一時停止(&P)\tSpace", L"Play/Pause(&P)\tSpace"));
    AppendMenuW(hPlayMenu, MF_STRING, IDM_PLAY_STOP, TR(L"停止(&S)\tEsc", L"Stop(&S)\tEsc"));
    AppendMenuW(hPlayMenu, MF_STRING, IDM_PLAY_LOOP, TR(L"ループ切替(&L)\tL", L"Toggle Loop(&L)\tL"));
    AppendMenuW(hNewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPlayMenu), TR(L"再生(&P)", L"Playback(&P)"));

    HMENU hToolMenu = CreatePopupMenu();
    AppendMenuW(hToolMenu, MF_STRING, IDM_TOOL_TEMPLATES, TR(L"定番テンプレート集(&T)...", L"Progression Templates(&T)..."));
    AppendMenuW(hToolMenu, MF_STRING, IDM_TOOL_SETTINGS, TR(L"環境設定(&S)...", L"Settings(&S)..."));
    AppendMenuW(hToolMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolMenu, MF_STRING, IDM_TOOL_HELP, TR(L"記法ヘルプ・リファレンス(&H)...", L"Notation Help & Reference(&H)..."));
    AppendMenuW(hToolMenu, MF_STRING, IDM_TOOL_ABOUT, TR(L"バージョン情報(&A)...", L"About ChordLaunch(&A)..."));
    AppendMenuW(hNewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hToolMenu), TR(L"ツール(&T)", L"Tools(&T)"));

    HMENU hOldMenu = m_hMainMenu;
    m_hMainMenu = hNewMenu;
    if (m_hwnd) {
        SetMenu(m_hwnd, m_hMainMenu);
        DrawMenuBar(m_hwnd);
    }
    if (hOldMenu) {
        DestroyMenu(hOldMenu);
    }
}

bool AppWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    if (FAILED(CreateDeviceIndependentResources())) return false;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = AppWindow::WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    // アプリアイコン (exe のリソースから読込。テスト実行ファイル等でリソースが無い場合は既定アイコン)
    wcex.hIcon = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    wcex.hIconSm = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    wcex.lpszClassName = L"ChordLaunchMainWindowClass";
    RegisterClassExW(&wcex);

    RebuildMainMenu();
    m_hwnd = CreateWindowExW(
        0, L"ChordLaunchMainWindowClass",
        L"Chord Launch",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 670,
        nullptr, m_hMainMenu, hInstance, this
    );

    if (!m_hwnd) return false;

    // 左ペインのコードテキスト入力欄（入力専用。位置は共通レイアウト ComputeLeftPaneLayout に従う）
    const LeftPaneLayout editLayout = ComputeLeftPaneLayout(m_toolbarHeight, m_leftPaneWidth);
    m_hwndEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        static_cast<int>(editLayout.pad), static_cast<int>(editLayout.editY), static_cast<int>(editLayout.innerW), static_cast<int>(editLayout.editH),
        m_hwnd, reinterpret_cast<HMENU>(IDC_TEXT_EDIT), hInstance, nullptr
    );

    SetWindowLongPtrW(m_hwndEdit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_origEditWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(m_hwndEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc))
    );

    m_hFontEdit = CreateFontW(
        15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, Theme::FontName
    );
    SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontEdit), TRUE);

    m_synth.Initialize();
    LoadAppSettings();

    // 起動時は Empty でスタート
    m_partManager.GetParts().clear();
    Part emptyPart;
    emptyPart.id = "part_1";
    emptyPart.name = "Intro";
    emptyPart.keyRoot = "C";
    emptyPart.mode = MusicalMode::Major;
    emptyPart.bpm = 120;
    emptyPart.timeNumerator = 4;
    emptyPart.timeDenominator = 4;
    m_partManager.GetParts().push_back(emptyPart);
    m_partManager.SetActiveIndex(0);
    m_projectTitle = "MyProgression";
    m_currentProjectPath.clear();
    m_isDirty = false;

    SyncTimelineToText();
    UpdateWindowTitle();

    if (m_showDashboardOnStartup) {
        m_activeModal = ModalType::Dashboard;
    }

    SetTimer(m_hwnd, 1, 16, nullptr); // 60FPS
    SetTimer(m_hwnd, TIMER_ID_AUTOBACKUP, 60000, nullptr); // 1分間隔自動バックアップ

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    return true;
}

int AppWindow::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (m_activeModal == ModalType::Templates && m_tmplEditorOpen) {
                if (msg.wParam == VK_ESCAPE) {
                    CloseTemplateEditor();
                    continue;
                }
                if (msg.wParam == VK_RETURN) {
                    SaveTemplateFromEditor();
                    continue;
                }
                if (msg.wParam == VK_TAB) {
                    m_tmplEditFocusField = isShift ? ((m_tmplEditFocusField + 2) % 3) : ((m_tmplEditFocusField + 1) % 3);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    continue;
                }
                if (msg.wParam == VK_BACK) {
                    std::wstring* pField = (m_tmplEditFocusField == 0) ? &m_tmplEditName : ((m_tmplEditFocusField == 1) ? &m_tmplEditProg : &m_tmplEditDesc);
                    if (!pField->empty()) {
                        pField->pop_back();
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    continue;
                }
                if (isCtrl && msg.wParam == 'V') {
                    if (OpenClipboard(m_hwnd)) {
                        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                        if (hData) {
                            const wchar_t* pszText = static_cast<const wchar_t*>(GlobalLock(hData));
                            if (pszText) {
                                std::wstring* pField = (m_tmplEditFocusField == 0) ? &m_tmplEditName : ((m_tmplEditFocusField == 1) ? &m_tmplEditProg : &m_tmplEditDesc);
                                for (const wchar_t* p = pszText; *p; ++p) {
                                    if (*p != L'\r' && *p != L'\n' && *p != L'\t') *pField += *p;
                                }
                                GlobalUnlock(hData);
                                InvalidateRect(m_hwnd, nullptr, FALSE);
                            }
                        }
                        CloseClipboard();
                    }
                    continue;
                }
            } else if (m_activeModal == ModalType::TapTempo) {
                if (msg.wParam == 'T' || msg.wParam == VK_SPACE) {
                    HandleTapTempo();
                    continue;
                }
                if (msg.wParam == VK_RETURN) {
                    ConfirmTapTempo();
                    continue;
                }
                if (msg.wParam == VK_ESCAPE) {
                    CloseModal();
                    continue;
                }
            } else if (m_activeModal != ModalType::None) {
                if (msg.wParam == VK_ESCAPE) {
                    CloseModal();
                    continue;
                }
            } else {
                if (isCtrl && msg.wParam == 'S') {
                    if (isShift) DoFileSaveAs();
                    else DoFileSave();
                    continue;
                }
                if (isCtrl && msg.wParam == 'O') {
                    DoFileOpen();
                    continue;
                }
                if (isCtrl && msg.wParam == 'N') {
                    DoFileNew();
                    continue;
                }
                if (isCtrl && msg.wParam == 'E') {
                    DoMidiSaveAs();
                    continue;
                }
                if (isCtrl && msg.wParam == 'T') {
                    OpenModal(ModalType::Templates);
                    continue;
                }
                if (isCtrl && msg.wParam == 'H') {
                    OpenModal(ModalType::Help);
                    continue;
                }
                if (msg.wParam == VK_SPACE && msg.hwnd != m_hwndEdit) {
                    TogglePlayback();
                    continue;
                }
                if (msg.wParam == VK_ESCAPE) {
                    // 再生中、または選択がなければ停止。停止中に選択があれば選択解除
                    if (m_isPlaying || (m_selectedChordIndex < 0 && m_multiSelTicks.empty())) StopPlayback();
                    else ClearSelection();
                    continue;
                }
                if (!isCtrl && msg.wParam == 'L' && msg.hwnd != m_hwndEdit) {
                    m_isLooping = !m_isLooping;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    continue;
                }
                if (isCtrl && msg.wParam == 'Z') {
                    Undo();
                    continue;
                }
                if (isCtrl && msg.wParam == 'Y') {
                    Redo();
                    continue;
                }
                if (isCtrl && msg.wParam == 'D') {
                    DuplicateSelection();
                    continue;
                }
                if (!isCtrl && (msg.wParam == 'B' || msg.wParam == 'b') && msg.hwnd != m_hwndEdit) {
                    SplitSelectedChord();
                    continue;
                }
                if (isCtrl && msg.wParam == 'C' && msg.hwnd != m_hwndEdit) {
                    DoEditCopy(); // 選択コードがあればコード単位、なければ進行全体をテキストでコピー
                    continue;
                }
                if (isCtrl && msg.wParam == 'X' && msg.hwnd != m_hwndEdit) {
                    DoEditCut();
                    continue;
                }
                if (isCtrl && msg.wParam == 'V' && msg.hwnd != m_hwndEdit) {
                    DoEditPaste();
                    continue;
                }
                if (isCtrl && msg.wParam == 'A' && msg.hwnd != m_hwndEdit) {
                    SelectAll();
                    continue;
                }
                if ((msg.wParam == VK_DELETE || msg.wParam == VK_BACK) && msg.hwnd != m_hwndEdit) {
                    DeleteSelection();
                    continue;
                }
                if (isCtrl && (msg.wParam == VK_OEM_PLUS || msg.wParam == VK_ADD)) {
                    m_pixelsPerBeat = std::min(140.0f, m_pixelsPerBeat + 8.0f);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    continue;
                }
                if (isCtrl && (msg.wParam == VK_OEM_MINUS || msg.wParam == VK_SUBTRACT)) {
                    m_pixelsPerBeat = std::max(24.0f, m_pixelsPerBeat - 8.0f);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    continue;
                }
                if (isCtrl && (msg.wParam == '0' || msg.wParam == VK_NUMPAD0)) {
                    m_pixelsPerBeat = 64.0f;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    continue;
                }
                if ((isCtrl || isShift) && msg.wParam == VK_UP && msg.hwnd != m_hwndEdit) {
                    ShiftSelectedChordOctave(1);
                    continue;
                }
                if ((isCtrl || isShift) && msg.wParam == VK_DOWN && msg.hwnd != m_hwndEdit) {
                    ShiftSelectedChordOctave(-1);
                    continue;
                }
                if (!isCtrl && msg.hwnd != m_hwndEdit) {
                    if ((m_settingZoomStyle == 0 && msg.wParam == 'R') ||
                        (m_settingZoomStyle == 1 && msg.wParam == 'G') ||
                        (m_settingZoomStyle == 2 && msg.wParam == 'W')) {
                        m_pixelsPerBeat = std::max(24.0f, m_pixelsPerBeat - 8.0f);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                        continue;
                    }
                    if ((m_settingZoomStyle == 0 && msg.wParam == 'T') ||
                        (m_settingZoomStyle == 1 && msg.wParam == 'H') ||
                        (m_settingZoomStyle == 2 && msg.wParam == 'E')) {
                        m_pixelsPerBeat = std::min(140.0f, m_pixelsPerBeat + 8.0f);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                        continue;
                    }
                    if (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT) {
                        Part* pActive = m_partManager.GetActivePart();
                        std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
                        if (!bars.empty()) {
                            int b = m_selectedBarIndex;
                            int c = m_selectedChordIndex;
                            if (b < 0 || b >= static_cast<int>(bars.size())) {
                                b = 0; c = 0;
                            } else if (msg.wParam == VK_RIGHT) {
                                if (c + 1 < static_cast<int>(bars[b].chords.size())) c++;
                                else if (b + 1 < static_cast<int>(bars.size())) { b++; c = 0; }
                            } else {
                                if (c > 0) c--;
                                else if (b > 0) { b--; c = std::max(0, static_cast<int>(bars[b].chords.size()) - 1); }
                            }
                            SelectChord(b, c);
                            if (b >= 0 && b < static_cast<int>(bars.size()) && c >= 0 && c < static_cast<int>(bars[b].chords.size())) {
                                AuditionChord(bars[b].chords[c]);
                            }
                        }
                        continue;
                    }
                }
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<AppWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK AppWindow::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis && msg == WM_KEYDOWN) {
        bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (isCtrl && wParam == VK_RETURN) {
            pThis->SyncTextToTimeline();
            return 0;
        }
        if (isCtrl && wParam == 'Z') {
            pThis->Undo();
            return 0;
        }
        if (isCtrl && wParam == 'Y') {
            pThis->Redo();
            return 0;
        }
    }
    return CallWindowProcW(pThis->m_origEditWndProc, hwnd, msg, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            OnSize(w, h);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 1000;
            mmi->ptMinTrackSize.y = 580;
            return 0;
        }
        case WM_PAINT: {
            OnRender();
            ValidateRect(hwnd, nullptr);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == TIMER_ID_AUTOBACKUP) {
                TriggerAutoBackup();
                return 0;
            }
            if (m_isPlaying) {
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - m_lastPlaybackTime).count();
                m_lastPlaybackTime = now;
                AdvancePlayback(dt);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (m_tapBlinked) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTapBlinkTime).count() > 150) {
                    m_tapBlinked = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            // プルダウンメニュー選択の処理
            if (wmId >= IDM_KEY_BASE && wmId < IDM_KEY_BASE + 12) {
                const char* keys[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
                if (m_partManager.GetActivePart()) {
                    const bool linked = (GetKeyState(VK_CONTROL) & 0x8000) == 0;
                    PushUndo();
                    auto changed = m_partManager.ApplyKey(keys[wmId - IDM_KEY_BASE], linked);
                    for (int idx : changed) RefreshPartDegrees(m_partManager.GetParts()[static_cast<size_t>(idx)]);
                    SyncTimelineToText();
                    SetStatusMessage(std::wstring(TR(L"キーを変更したパート数: ", L"Parts with key changed: ")) + std::to_wstring(changed.size()) +
                                     TR(L"（Ctrl 併用でこのパートのみ）", L" (hold Ctrl for this part only)"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wmId >= IDM_SCALE_BASE && wmId < IDM_SCALE_BASE + 10) {
                if (m_partManager.GetActivePart()) {
                    const bool linked = (GetKeyState(VK_CONTROL) & 0x8000) == 0;
                    PushUndo();
                    auto changed = m_partManager.ApplyMode(static_cast<MusicalMode>(wmId - IDM_SCALE_BASE), linked);
                    for (int idx : changed) RefreshPartDegrees(m_partManager.GetParts()[static_cast<size_t>(idx)]);
                    SyncTimelineToText();
                    SetStatusMessage(std::wstring(TR(L"スケールを変更したパート数: ", L"Parts with scale changed: ")) + std::to_wstring(changed.size()) +
                                     TR(L"（Ctrl 併用でこのパートのみ）", L" (hold Ctrl for this part only)"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wmId >= IDM_VOICING_BASE && wmId < IDM_VOICING_BASE + 5) {
                m_selectedVoicing = static_cast<VoicingStyle>(wmId - IDM_VOICING_BASE);
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_BASS_BASE && wmId < IDM_BASS_BASE + 4) {
                m_bassOctaveMode = wmId - IDM_BASS_BASE;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_GRID_BASE && wmId < IDM_GRID_BASE + 5) {
                int grids[] = { TICKS_PER_BAR_4_4, TICKS_PER_BEAT * 2, TICKS_PER_BEAT, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4 };
                m_timelineGridTicks = grids[wmId - IDM_GRID_BASE];
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_INPUT_GRID_BASE && wmId < IDM_INPUT_GRID_BASE + 5) {
                int grids[] = { TICKS_PER_BAR_4_4, TICKS_PER_BEAT * 2, TICKS_PER_BEAT, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4 };
                m_inputGridTicks = grids[wmId - IDM_INPUT_GRID_BASE];
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_DIA_VARIANT_BASE && wmId < IDM_DIA_VARIANT_BASE + 7) {
                // 「その他」バリエーション: メニュー順 = ダイアトニックタブ 2,3,4,5,6,7,8
                m_diatonicChordTypeTab = 2 + (wmId - IDM_DIA_VARIANT_BASE);
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_PRESET_BASE && wmId < IDM_PRESET_BASE + 4) {
                m_synth.SetPreset(static_cast<WasapiSynth::Preset>(wmId - IDM_PRESET_BASE));
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId == IDM_CTX_PART_RENAME && m_contextPartIndex >= 0) {
                OpenRenamePartModal(m_contextPartIndex);
                return 0;
            }
            if (wmId == IDM_CTX_MARKER_EDIT && m_contextMarkerTick >= 0) {
                OpenMarkerModal(m_contextMarkerTick);
                return 0;
            }
            if (wmId == IDM_CTX_MARKER_DEL && m_contextMarkerTick >= 0) {
                ClearMarker(m_contextMarkerTick);
                return 0;
            }
            if (wmId == IDM_EDIT_ADD_MARKER) {
                AddMarkerAtPlayhead();
                return 0;
            }
            if (wmId == IDM_CTX_PART_DUP && m_contextPartIndex >= 0) {
                PushUndo();
                m_partManager.DuplicatePart(m_contextPartIndex);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId == IDM_CTX_PART_DEL && m_contextPartIndex >= 0) {
                PushUndo();
                m_partManager.RemovePart(m_contextPartIndex);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId == IDM_CTX_PART_LEFT && m_contextPartIndex > 0) {
                PushUndo();
                m_partManager.MovePart(m_contextPartIndex, m_contextPartIndex - 1);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId == IDM_CTX_PART_RIGHT && m_contextPartIndex >= 0 && m_contextPartIndex + 1 < static_cast<int>(m_partManager.GetParts().size())) {
                PushUndo();
                m_partManager.MovePart(m_contextPartIndex, m_contextPartIndex + 1);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return 0;
            }
            if (wmId >= IDM_CTX_CARD_INV0 && wmId <= IDM_CTX_CARD_INV3) {
                Part* pActive = m_partManager.GetActivePart();
                if (pActive && m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(pActive->bars.size())) {
                    auto& bar = pActive->bars[m_selectedBarIndex];
                    if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
                        PushUndo();
                        bar.chords[m_selectedChordIndex].inversion = wmId - IDM_CTX_CARD_INV0;
                        AuditionChord(bar.chords[m_selectedChordIndex]);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            if (wmId >= IDM_CTX_CARD_OCT_UP && wmId <= IDM_CTX_CARD_OCT_RESET) {
                if (wmId == IDM_CTX_CARD_OCT_UP) ShiftSelectedChordOctave(1);
                else if (wmId == IDM_CTX_CARD_OCT_DOWN) ShiftSelectedChordOctave(-1);
                else ResetSelectedChordOctave();
                return 0;
            }
            if ((wmId >= IDM_CTX_CARD_BASS_NONE && wmId <= IDM_CTX_CARD_BASS_7TH) ||
                (wmId >= IDM_CTX_CARD_BASS_BASE && wmId < IDM_CTX_CARD_BASS_BASE + 12)) {
                Part* pActive = m_partManager.GetActivePart();
                if (pActive && m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(pActive->bars.size())) {
                    auto& bar = pActive->bars[m_selectedBarIndex];
                    if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
                        auto& selC = bar.chords[m_selectedChordIndex];
                        if (!selC.isRest && selC.rootPitch != PitchClass::Unknown) {
                            PushUndo();
                            std::string newBassName;
                            if (wmId == IDM_CTX_CARD_BASS_3RD) {
                                bool isMin = (selC.quality == ChordQuality::Minor || selC.quality == ChordQuality::Minor7 ||
                                              selC.quality == ChordQuality::MinorMajor7 || selC.quality == ChordQuality::Diminished ||
                                              selC.quality == ChordQuality::Diminished7 || selC.quality == ChordQuality::HalfDiminished7 ||
                                              selC.quality == ChordQuality::MinorAdd9 || selC.quality == ChordQuality::MinorSix);
                                PitchClass bp = static_cast<PitchClass>((static_cast<int>(selC.rootPitch) + (isMin ? 3 : 4)) % 12);
                                newBassName = MusicEngine::PitchToNoteForKey(bp, pActive->keyRoot, pActive->mode);
                            } else if (wmId == IDM_CTX_CARD_BASS_5TH) {
                                bool isDim = (selC.quality == ChordQuality::Diminished || selC.quality == ChordQuality::Diminished7 || selC.quality == ChordQuality::HalfDiminished7);
                                PitchClass bp = static_cast<PitchClass>((static_cast<int>(selC.rootPitch) + (isDim ? 6 : 7)) % 12);
                                newBassName = MusicEngine::PitchToNoteForKey(bp, pActive->keyRoot, pActive->mode);
                            } else if (wmId == IDM_CTX_CARD_BASS_7TH) {
                                bool isMaj7 = (selC.quality == ChordQuality::Major7 || selC.quality == ChordQuality::MinorMajor7);
                                PitchClass bp = static_cast<PitchClass>((static_cast<int>(selC.rootPitch) + (isMaj7 ? 11 : 10)) % 12);
                                newBassName = MusicEngine::PitchToNoteForKey(bp, pActive->keyRoot, pActive->mode);
                            } else if (wmId >= IDM_CTX_CARD_BASS_BASE && wmId < IDM_CTX_CARD_BASS_BASE + 12) {
                                PitchClass bp = static_cast<PitchClass>(wmId - IDM_CTX_CARD_BASS_BASE);
                                if (bp != selC.rootPitch) {
                                    newBassName = MusicEngine::PitchToNoteForKey(bp, pActive->keyRoot, pActive->mode);
                                }
                            }
                            std::string newSym = selC.rootName + selC.qualitySuffix;
                            if (!newBassName.empty()) newSym += "/" + newBassName;
                            ChordToken updated = MusicEngine::ParseChordSymbol(newSym, pActive->keyRoot, pActive->mode);
                            updated.durationTicks = selC.durationTicks;
                            updated.durationBeats = selC.durationBeats;
                            updated.inversion = selC.inversion;
                            updated.octaveOffset = selC.octaveOffset;
                            selC = updated;
                            SyncTimelineToText();
                            AuditionChord(selC);
                            InvalidateRect(m_hwnd, nullptr, FALSE);
                        }
                    }
                }
                return 0;
            }

            switch (wmId) {
                case IDM_FILE_NEW: DoFileNew(); break;
                case IDM_FILE_OPEN: DoFileOpen(); break;
                case IDM_FILE_DASHBOARD: OpenDashboardModal(); break;
                default:
                    if (wmId >= IDM_FILE_MRU_BASE && wmId < IDM_FILE_MRU_BASE + 10) {
                        OpenRecentFile(wmId - IDM_FILE_MRU_BASE);
                        break;
                    }
                    break;
                case IDM_FILE_SAVE: DoFileSave(); break;
                case IDM_FILE_SAVEAS: DoFileSaveAs(); break;
                case IDM_FILE_EXPORT_MIDI: DoMidiSaveAs(); break;
                case IDM_FILE_EXIT: PostQuitMessage(0); break;

                case IDM_EDIT_UNDO: Undo(); break;
                case IDM_EDIT_REDO: Redo(); break;
                case IDM_EDIT_CUT: DoEditCut(); break;
                case IDM_EDIT_COPY: DoEditCopy(); break;
                case IDM_EDIT_PASTE: DoEditPaste(); break;
                case IDM_EDIT_SPLIT: SplitSelectedChord(); break;
                case IDM_EDIT_OCTAVE_UP: ShiftSelectedChordOctave(1); break;
                case IDM_EDIT_OCTAVE_DOWN: ShiftSelectedChordOctave(-1); break;
                case IDM_EDIT_OCTAVE_RESET: ResetSelectedChordOctave(); break;
                case IDM_EDIT_DUPLICATE: DuplicateSelection(); break;
                case IDM_EDIT_DELETE: DeleteSelection(); break;
                case IDM_EDIT_SELECTALL: SelectAll(); break;
                case IDM_EDIT_CLEARALL:
                    PushUndo();
                    if (m_partManager.GetActivePart()) {
                        m_partManager.GetActivePart()->bars.clear();
                    }
                    SyncTimelineToText();
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;

                case IDM_VIEW_PIANOROLL:
                    m_showKeyboard = !m_showKeyboard;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;
                case IDM_VIEW_RIGHTPANE:
                    m_showRightPane = !m_showRightPane;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;
                case IDM_VIEW_ZOOMIN:
                    m_pixelsPerBeat = std::min(140.0f, m_pixelsPerBeat + 8.0f);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;
                case IDM_VIEW_ZOOMOUT:
                    m_pixelsPerBeat = std::max(24.0f, m_pixelsPerBeat - 8.0f);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;
                case IDM_VIEW_ZOOMRESET:
                    m_pixelsPerBeat = 64.0f;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;

                case IDM_PLAY_PLAYPAUSE: TogglePlayback(); break;
                case IDM_PLAY_STOP: StopPlayback(); break;
                case IDM_PLAY_LOOP:
                    m_isLooping = !m_isLooping;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    break;

                case IDM_TOOL_TEMPLATES: OpenModal(ModalType::Templates); break;
                case IDM_TOOL_SETTINGS:  OpenModal(ModalType::Settings); break;
                case IDM_TOOL_HELP:      OpenModal(ModalType::Help); break;
                case IDM_TOOL_ABOUT:     OpenModal(ModalType::About); break;
            }
            return 0;
        }
        case WM_KEYDOWN:
            OnKeyDown(wParam);
            return 0;
        case WM_CHAR:
            OnChar(static_cast<WCHAR>(wParam));
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_LBUTTONDBLCLK:
            OnLButtonDblClk(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_RBUTTONUP:
            OnRButtonUp(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            OnMouseWheel(delta, static_cast<float>(pt.x), static_cast<float>(pt.y));
            return 0;
        }
        case WM_CLOSE:
            if (!ConfirmDiscardChanges()) {
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AppWindow::OnSize(UINT width, UINT height) {
    m_width = static_cast<float>(width);
    m_height = static_cast<float>(height);

    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }

    if (m_hwndEdit) {
        const LeftPaneLayout editLayout = ComputeLeftPaneLayout(m_toolbarHeight, m_leftPaneWidth);
        SetWindowPos(m_hwndEdit, nullptr, static_cast<int>(editLayout.pad), static_cast<int>(editLayout.editY), static_cast<int>(editLayout.innerW), static_cast<int>(editLayout.editH), SWP_NOZORDER);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// モーダル開閉制御
// ---------------------------------------------------------------------------
void AppWindow::OpenModal(ModalType modal) {
    m_activeModal = modal;
    if (m_hwndEdit) {
        ShowWindow(m_hwndEdit, SW_HIDE);
    }
    if (modal == ModalType::TapTempo) {
        m_tapTimes.clear();
        Part* pActive = m_partManager.GetActivePart();
        m_measuredBpm = pActive ? pActive->bpm : 120;
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::CloseModal() {
    m_activeModal = ModalType::None;
    if (m_hwndEdit) {
        ShowWindow(m_hwndEdit, SW_SHOW);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// ベクター図形アイコン描画ルーチン (Direct2D ジオメトリ / 絵文字フリー)
// ---------------------------------------------------------------------------
void AppWindow::DrawIcon(IconType type, const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    if (!pBrush || type == IconType::None) return;
    switch (type) {
        case IconType::Play: DrawIconPlay(rect, pBrush); break;
        case IconType::Pause: DrawIconPause(rect, pBrush); break;
        case IconType::Stop: DrawIconStop(rect, pBrush); break;
        case IconType::Loop: DrawIconLoop(rect, pBrush, true); break;
        case IconType::Settings: DrawIconSettings(rect, pBrush); break;
        case IconType::Music: DrawIconMusic(rect, pBrush); break;
        case IconType::Close: DrawIconClose(rect, pBrush); break;
        case IconType::Plus: DrawIconPlus(rect, pBrush); break;
        case IconType::ChevronUp: DrawIconChevron(rect, pBrush, true); break;
        case IconType::ChevronDown: DrawIconChevron(rect, pBrush, false); break;
        case IconType::Template: DrawIconTemplate(rect, pBrush); break;
        case IconType::Help: DrawIconHelp(rect, pBrush); break;
        case IconType::SaveDisk: DrawIconSaveDisk(rect, pBrush); break;
        case IconType::DawDrag: DrawIconDawDrag(rect, pBrush); break;
        case IconType::Keyboard: DrawIconKeyboard(rect, pBrush); break;
        case IconType::Zoom: DrawIconZoom(rect, pBrush); break;
        default: break;
    }
}

void AppWindow::DrawIconPlay(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.40f;

    ID2D1PathGeometry* pPath = nullptr;
    if (SUCCEEDED(m_pD2DFactory->CreatePathGeometry(&pPath))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pPath->Open(&pSink))) {
            pSink->BeginFigure(D2D1::Point2F(cx - sz * 0.6f, cy - sz), D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddLine(D2D1::Point2F(cx + sz * 0.8f, cy));
            pSink->AddLine(D2D1::Point2F(cx - sz * 0.6f, cy + sz));
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            pSink->Release();
            m_pRenderTarget->FillGeometry(pPath, pBrush);
        }
        pPath->Release();
    }
}

void AppWindow::DrawIconPause(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.35f;
    float barW = sz * 0.45f;
    float gap = sz * 0.35f;

    D2D1_RECT_F r1 = D2D1::RectF(cx - gap - barW, cy - sz, cx - gap, cy + sz);
    D2D1_RECT_F r2 = D2D1::RectF(cx + gap, cy - sz, cx + gap + barW, cy + sz);
    m_pRenderTarget->FillRectangle(r1, pBrush);
    m_pRenderTarget->FillRectangle(r2, pBrush);
}

void AppWindow::DrawIconStop(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.32f;
    D2D1_RECT_F r = D2D1::RectF(cx - sz, cy - sz, cx + sz, cy + sz);
    m_pRenderTarget->FillRectangle(r, pBrush);
}

void AppWindow::DrawIconLoop(const D2D1_RECT_F& rect, ID2D1Brush* pBrush, bool /*active*/) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float r = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.35f;

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(cx - r, cy - r * 0.75f, cx + r, cy + r * 0.75f), 3.0f, 3.0f);
    m_pRenderTarget->DrawRoundedRectangle(rr, pBrush, 1.5f);

    ID2D1PathGeometry* pPath = nullptr;
    if (SUCCEEDED(m_pD2DFactory->CreatePathGeometry(&pPath))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pPath->Open(&pSink))) {
            pSink->BeginFigure(D2D1::Point2F(cx + r - 3.0f, cy - r * 0.75f - 3.0f), D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddLine(D2D1::Point2F(cx + r + 3.0f, cy - r * 0.75f));
            pSink->AddLine(D2D1::Point2F(cx + r - 3.0f, cy - r * 0.75f + 3.0f));
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            pSink->Release();
            m_pRenderTarget->FillGeometry(pPath, pBrush);
        }
        pPath->Release();
    }
}

void AppWindow::DrawIconSettings(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    // 音楽制作・DAW標準の「3段アジャスタースライダー」アイコン
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float w = (rect.right - rect.left) * 0.65f;
    float h = (rect.bottom - rect.top) * 0.55f;

    float left = cx - w * 0.5f;
    float right = cx + w * 0.5f;
    float top = cy - h * 0.5f;
    float rowH = h * 0.5f;

    float y1 = top;
    float y2 = top + rowH;
    float y3 = top + rowH * 2.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(left, y1), D2D1::Point2F(right, y1), pBrush, 1.2f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(left, y2), D2D1::Point2F(right, y2), pBrush, 1.2f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(left, y3), D2D1::Point2F(right, y3), pBrush, 1.2f);

    float knobW = 4.0f;
    float knobH = 6.0f;

    float kx1 = left + w * 0.28f;
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(kx1 - knobW * 0.5f, y1 - knobH * 0.5f, kx1 + knobW * 0.5f, y1 + knobH * 0.5f), 1.5f, 1.5f), pBrush);

    float kx2 = left + w * 0.72f;
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(kx2 - knobW * 0.5f, y2 - knobH * 0.5f, kx2 + knobW * 0.5f, y2 + knobH * 0.5f), 1.5f, 1.5f), pBrush);

    float kx3 = left + w * 0.45f;
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(kx3 - knobW * 0.5f, y3 - knobH * 0.5f, kx3 + knobW * 0.5f, y3 + knobH * 0.5f), 1.5f, 1.5f), pBrush);
}

void AppWindow::DrawIconSaveDisk(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    // フロッピーディスクの精密ベクター描画
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.35f;

    D2D1_RECT_F body = D2D1::RectF(cx - sz, cy - sz, cx + sz, cy + sz);
    m_pRenderTarget->DrawRectangle(body, pBrush, 1.4f);

    // シャッター窓 (上部)
    D2D1_RECT_F shutter = D2D1::RectF(cx - sz * 0.6f, cy - sz, cx + sz * 0.4f, cy - sz * 0.2f);
    m_pRenderTarget->FillRectangle(shutter, pBrush);

    // ラベル領域 (下部)
    D2D1_RECT_F label = D2D1::RectF(cx - sz * 0.7f, cy + sz * 0.1f, cx + sz * 0.7f, cy + sz);
    m_pRenderTarget->DrawRectangle(label, pBrush, 1.0f);
}

void AppWindow::DrawIconDawDrag(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    // 音符 ＋ DAWドラッグ矢印
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.35f;

    // 8分音符
    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - sz * 0.4f, cy + sz * 0.5f), sz * 0.35f, sz * 0.25f), pBrush);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz * 0.1f, cy + sz * 0.5f), D2D1::Point2F(cx - sz * 0.1f, cy - sz * 0.6f), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz * 0.1f, cy - sz * 0.6f), D2D1::Point2F(cx + sz * 0.4f, cy - sz * 0.4f), pBrush, 2.0f);

    // 右向きドラッグ矢印 (下部)
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx + sz * 0.2f, cy + sz * 0.5f), D2D1::Point2F(cx + sz * 0.8f, cy + sz * 0.5f), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx + sz * 0.55f, cy + sz * 0.25f), D2D1::Point2F(cx + sz * 0.8f, cy + sz * 0.5f), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx + sz * 0.55f, cy + sz * 0.75f), D2D1::Point2F(cx + sz * 0.8f, cy + sz * 0.5f), pBrush, 1.5f);
}

void AppWindow::DrawIconMusic(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    DrawIconDawDrag(rect, pBrush);
}

void AppWindow::DrawIconClose(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.25f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz, cy - sz), D2D1::Point2F(cx + sz, cy + sz), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx + sz, cy - sz), D2D1::Point2F(cx - sz, cy + sz), pBrush, 1.5f);
}

void AppWindow::DrawIconPlus(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.30f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz, cy), D2D1::Point2F(cx + sz, cy), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx, cy - sz), D2D1::Point2F(cx, cy + sz), pBrush, 1.5f);
}

void AppWindow::DrawIconChevron(const D2D1_RECT_F& rect, ID2D1Brush* pBrush, bool open) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.20f;

    ID2D1PathGeometry* pPath = nullptr;
    if (SUCCEEDED(m_pD2DFactory->CreatePathGeometry(&pPath))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pPath->Open(&pSink))) {
            if (open) {
                pSink->BeginFigure(D2D1::Point2F(cx - sz, cy - sz * 0.5f), D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(D2D1::Point2F(cx + sz, cy - sz * 0.5f));
                pSink->AddLine(D2D1::Point2F(cx, cy + sz * 0.5f));
            } else {
                pSink->BeginFigure(D2D1::Point2F(cx - sz * 0.5f, cy - sz), D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(D2D1::Point2F(cx + sz * 0.5f, cy));
                pSink->AddLine(D2D1::Point2F(cx - sz * 0.5f, cy + sz));
            }
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            pSink->Release();
            m_pRenderTarget->FillGeometry(pPath, pBrush);
        }
        pPath->Release();
    }
}

void AppWindow::DrawIconTemplate(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float sz = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.32f;

    m_pRenderTarget->DrawRectangle(D2D1::RectF(cx - sz, cy - sz * 0.7f, cx + sz, cy + sz), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz * 0.6f, cy - sz * 0.1f), D2D1::Point2F(cx + sz * 0.6f, cy - sz * 0.1f), pBrush, 1.2f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx - sz * 0.6f, cy + sz * 0.4f), D2D1::Point2F(cx + sz * 0.3f, cy + sz * 0.4f), pBrush, 1.2f);
}

void AppWindow::DrawIconHelp(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float r = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.35f;

    m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush, 1.5f);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx, cy + r * 0.2f), D2D1::Point2F(cx, cy + r * 0.5f), pBrush, 1.8f);
    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + r * 0.7f), 1.2f, 1.2f), pBrush);
}

void AppWindow::DrawIconKeyboard(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float w = std::min(rect.right - rect.left, 24.0f) * 0.72f;
    float h = std::min(rect.bottom - rect.top, 20.0f) * 0.62f;
    D2D1_RECT_F box = D2D1::RectF(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f);

    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(box, 1.5f, 1.5f), pBrush, 1.3f);
    float kw = w / 4.0f;
    for (int i = 1; i <= 3; ++i) {
        float kx = box.left + kw * i;
        m_pRenderTarget->DrawLine(D2D1::Point2F(kx, box.top), D2D1::Point2F(kx, box.bottom), pBrush, 1.0f);
    }
    float bh = h * 0.55f;
    float bw = kw * 0.55f;
    for (int i : { 1, 2 }) {
        float bkx = box.left + kw * i;
        m_pRenderTarget->FillRectangle(D2D1::RectF(bkx - bw * 0.5f, box.top, bkx + bw * 0.5f, box.top + bh), pBrush);
    }
}

void AppWindow::DrawIconZoom(const D2D1_RECT_F& rect, ID2D1Brush* pBrush) {
    float cx = (rect.left + rect.right) * 0.5f - 1.5f;
    float cy = (rect.top + rect.bottom) * 0.5f - 1.5f;
    float r = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.24f;

    m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush, 1.5f);
    float hx1 = cx + r * 0.707f;
    float hy1 = cy + r * 0.707f;
    float hx2 = cx + r * 1.65f;
    float hy2 = cy + r * 1.65f;
    m_pRenderTarget->DrawLine(D2D1::Point2F(hx1, hy1), D2D1::Point2F(hx2, hy2), pBrush, 1.8f);
}

// ---------------------------------------------------------------------------
// 共通UI描画 (ボタン・カード・アコーディオン見出し)
// ---------------------------------------------------------------------------
void AppWindow::DrawButton(
    const D2D1_RECT_F& rect,
    const std::wstring& text,
    IconType icon,
    bool isHovered,
    bool isPressed,
    bool isPrimary,
    D2D1_COLOR_F primaryColor
) {
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 4.0f, 4.0f);

    D2D1_COLOR_F bgColor = Theme::CardBackground;
    if (isPrimary) {
        bgColor = (primaryColor.a > 0.0f) ? primaryColor : Theme::AccentAction;
        if (isHovered) bgColor = D2D1::ColorF(bgColor.r * 1.08f, bgColor.g * 1.08f, bgColor.b * 1.08f, bgColor.a);
    } else {
        if (isPressed) bgColor = Theme::CardActive;
        else if (isHovered) bgColor = Theme::CardHover;
    }

    ID2D1SolidColorBrush* pBtnBgBrush = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(bgColor, &pBtnBgBrush);
    m_pRenderTarget->FillRoundedRectangle(rr, pBtnBgBrush);
    pBtnBgBrush->Release();

    m_pRenderTarget->DrawRoundedRectangle(rr, isPrimary ? m_pBrushAccent : m_pBrushBorder, 1.0f);

    m_pRenderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    ID2D1SolidColorBrush* pTextBrush = isPrimary ? m_pBrushBg : m_pBrushText;

    if (icon != IconType::None && !text.empty()) {
        float iconBoxSize = std::min(rect.bottom - rect.top, 20.0f);
        D2D1_RECT_F iconRect = D2D1::RectF(rect.left + 4.0f, rect.top, rect.left + 4.0f + iconBoxSize, rect.bottom);
        DrawIcon(icon, iconRect, pTextBrush);

        D2D1_RECT_F textRect = D2D1::RectF(rect.left + iconBoxSize + 4.0f, rect.top, rect.right - 2.0f, rect.bottom);
        DrawTextSafe(text, m_pFontButton, textRect, pTextBrush);
    } else if (icon != IconType::None) {
        DrawIcon(icon, rect, pTextBrush);
    } else {
        DrawTextSafe(text, m_pFontButton, rect, pTextBrush);
    }

    m_pRenderTarget->PopAxisAlignedClip();
}

void AppWindow::DrawCard(
    const D2D1_RECT_F& rect,
    const std::wstring& chordName,
    const std::wstring& degree,
    bool isHovered,
    bool isSelected,
    bool isRest,
    bool showCloseButton,
    bool isCloseHovered
) {
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 6.0f, 6.0f);

    D2D1_COLOR_F bgColor = isRest ? Theme::RestBlockFill : (isSelected ? Theme::CardActive : (isHovered ? Theme::CardHover : Theme::CardBackground));
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(bgColor, &pBgBrush);
    m_pRenderTarget->FillRoundedRectangle(rr, pBgBrush);
    pBgBrush->Release();

    ID2D1SolidColorBrush* pBorderBrush = isSelected ? m_pBrushAccent : (isHovered ? m_pBrushBorder : m_pBrushBorderLight);
    m_pRenderTarget->DrawRoundedRectangle(rr, pBorderBrush, isSelected ? 2.0f : 1.0f);

    m_pRenderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float w = rect.right - rect.left;
    float h = rect.bottom - rect.top;
    bool useCompactFont = (w < 84.0f && chordName.size() >= 5) || (chordName.size() >= 8);
    IDWriteTextFormat* pTitleFont = useCompactFont ? m_pFontButton : m_pFontCardTitle;

    // 閉じるボタンがないカードは左右の余白を対称にして、文字が左へ寄るズレを防ぐ
    const float textRightPad = showCloseButton ? 14.0f : 8.0f;
    D2D1_RECT_F titleRect = D2D1::RectF(rect.left + 8.0f, rect.top + 3.0f, rect.right - textRightPad, degree.empty() ? (rect.bottom - 3.0f) : (rect.top + h * 0.56f));
    DrawTextSafe(chordName, pTitleFont, titleRect, isRest ? m_pBrushTextMuted : m_pBrushText);

    if (!degree.empty()) {
        D2D1_RECT_F degRect = D2D1::RectF(rect.left + 8.0f, rect.top + h * 0.55f, rect.right - textRightPad, rect.bottom - 2.0f);
        DrawTextSafe(degree, m_pFontCardDegree, degRect, m_pBrushAccent);
    }

    if (showCloseButton) {
        D2D1_RECT_F closeRc = D2D1::RectF(rect.right - 16.0f, rect.top + 2.0f, rect.right - 2.0f, rect.top + 16.0f);
        DrawIconClose(closeRc, isCloseHovered ? m_pBrushStop : m_pBrushTextMuted);
    }

    if (showCloseButton && !isRest) {
        float hTop = rect.top + h * 0.35f;
        float hBottom = rect.bottom - h * 0.25f;

        // 左端リサイズハンドル (縦線: 文字に被らないよう端から3.5px)
        float leftHandleX = rect.left + 3.5f;
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(leftHandleX, hTop),
            D2D1::Point2F(leftHandleX, hBottom),
            isSelected ? m_pBrushAccent : m_pBrushBorder,
            1.5f
        );

        // 右端リサイズハンドル (縦線)
        float handleX = rect.right - 5.0f;
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(handleX, hTop),
            D2D1::Point2F(handleX, hBottom),
            isSelected ? m_pBrushAccent : m_pBrushBorder,
            1.5f
        );
    }

    m_pRenderTarget->PopAxisAlignedClip();
}

void AppWindow::DrawSectionHeader(
    const D2D1_RECT_F& rect,
    const std::wstring& title,
    bool isOpen,
    bool isHovered
) {
    if (isHovered) {
        m_pRenderTarget->FillRectangle(rect, m_pBrushBorderLight);
    }
    m_pRenderTarget->DrawLine(D2D1::Point2F(rect.left, rect.bottom), D2D1::Point2F(rect.right, rect.bottom), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F chevronRc = D2D1::RectF(rect.left + 4.0f, rect.top + 2.0f, rect.left + 20.0f, rect.bottom - 2.0f);
    DrawIconChevron(chevronRc, m_pBrushTextMuted, isOpen);

    D2D1_RECT_F textRc = D2D1::RectF(rect.left + 24.0f, rect.top, rect.right - 4.0f, rect.bottom);
    DrawTextSafe(title, m_pFontInspector, textRc, m_pBrushText);
}
} // namespace clp
