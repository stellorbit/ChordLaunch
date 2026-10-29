#include "app_window.hpp"
#include "ole_dnd.hpp"
#include <commctrl.h>
#include <algorithm>
#include <format>
#include <iostream>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")

namespace clp {

AppWindow::AppWindow() = default;

AppWindow::~AppWindow() {
    DiscardDeviceResources();
    if (m_pFontTitle) m_pFontTitle->Release();
    if (m_pFontHeader) m_pFontHeader->Release();
    if (m_pFontNormal) m_pFontNormal->Release();
    if (m_pFontSmall) m_pFontSmall->Release();
    if (m_pFontCardTitle) m_pFontCardTitle->Release();
    if (m_pFontCardSub) m_pFontCardSub->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pD2DFactory) m_pD2DFactory->Release();
}

bool AppWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return false;

    const wchar_t* fontName = Theme::FontName;

    // フォント作成
    hr = m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"ja-jp", &m_pFontTitle
    );
    if (FAILED(hr)) {
        // フォールバックフォント
        fontName = Theme::FallbackFontName;
        m_pDWriteFactory->CreateTextFormat(
            fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"ja-jp", &m_pFontTitle
        );
    }

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"ja-jp", &m_pFontHeader
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ja-jp", &m_pFontNormal
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"ja-jp", &m_pFontSmall
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"ja-jp", &m_pFontCardTitle
    );
    if (m_pFontCardTitle) {
        m_pFontCardTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontCardTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"ja-jp", &m_pFontCardSub
    );
    if (m_pFontCardSub) {
        m_pFontCardSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontCardSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    return true;
}

bool AppWindow::CreateDeviceResources() {
    if (m_pRenderTarget) return true;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pRenderTarget
    );
    if (FAILED(hr)) return false;

    m_pRenderTarget->CreateSolidColorBrush(Theme::Background, &m_pBrushBackground);
    m_pRenderTarget->CreateSolidColorBrush(Theme::PaneBackground, &m_pBrushPaneBg);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardBackground, &m_pBrushCardBg);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardHover, &m_pBrushCardHover);
    m_pRenderTarget->CreateSolidColorBrush(Theme::CardSelected, &m_pBrushCardSelected);
    m_pRenderTarget->CreateSolidColorBrush(Theme::Border, &m_pBrushBorder);
    m_pRenderTarget->CreateSolidColorBrush(Theme::TextPrimary, &m_pBrushTextPrimary);
    m_pRenderTarget->CreateSolidColorBrush(Theme::TextSecondary, &m_pBrushTextSecondary);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentWarm, &m_pBrushAccent);
    m_pRenderTarget->CreateSolidColorBrush(Theme::AccentPlay, &m_pBrushPlay);
    m_pRenderTarget->CreateSolidColorBrush(Theme::Playhead, &m_pBrushPlayhead);
    m_pRenderTarget->CreateSolidColorBrush(Theme::LoopRangeFill, &m_pBrushLoopRange);
    m_pRenderTarget->CreateSolidColorBrush(Theme::RestBlockFill, &m_pBrushRest);

    return true;
}

void AppWindow::DiscardDeviceResources() {
    if (m_pBrushRest) { m_pBrushRest->Release(); m_pBrushRest = nullptr; }
    if (m_pBrushLoopRange) { m_pBrushLoopRange->Release(); m_pBrushLoopRange = nullptr; }
    if (m_pBrushPlayhead) { m_pBrushPlayhead->Release(); m_pBrushPlayhead = nullptr; }
    if (m_pBrushPlay) { m_pBrushPlay->Release(); m_pBrushPlay = nullptr; }
    if (m_pBrushAccent) { m_pBrushAccent->Release(); m_pBrushAccent = nullptr; }
    if (m_pBrushTextSecondary) { m_pBrushTextSecondary->Release(); m_pBrushTextSecondary = nullptr; }
    if (m_pBrushTextPrimary) { m_pBrushTextPrimary->Release(); m_pBrushTextPrimary = nullptr; }
    if (m_pBrushBorder) { m_pBrushBorder->Release(); m_pBrushBorder = nullptr; }
    if (m_pBrushCardSelected) { m_pBrushCardSelected->Release(); m_pBrushCardSelected = nullptr; }
    if (m_pBrushCardHover) { m_pBrushCardHover->Release(); m_pBrushCardHover = nullptr; }
    if (m_pBrushCardBg) { m_pBrushCardBg->Release(); m_pBrushCardBg = nullptr; }
    if (m_pBrushPaneBg) { m_pBrushPaneBg->Release(); m_pBrushPaneBg = nullptr; }
    if (m_pBrushBackground) { m_pBrushBackground->Release(); m_pBrushBackground = nullptr; }
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

bool AppWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = L"ChordLaunchpadMainWindowClass";

    RegisterClassExW(&wcex);

    if (!CreateDeviceIndependentResources()) return false;

    // 初期ウィンドウサイズ 1360 x 850
    m_hwnd = CreateWindowExW(
        0,
        wcex.lpszClassName,
        L"Chord Launchpad",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1360, 850,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    // WASAPI シンセ初期化
    m_synth.Initialize();

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    // 60FPS タイマー設定 (ID: 1, 16ms)
    SetTimer(m_hwnd, 1, 16, nullptr);

    return true;
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<AppWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            OnRender();
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE: {
            if (m_pRenderTarget) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                m_pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1 && m_isPlaying) {
                auto now = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(now - m_lastPlaybackTime).count();
                m_lastPlaybackTime = now;
                AdvancePlayback(dt);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            OnLButtonDown(LOWORD(lParam), HIWORD(lParam), wParam);
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove(LOWORD(lParam), HIWORD(lParam), wParam);
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(LOWORD(lParam), HIWORD(lParam), wParam);
            return 0;
        case WM_LBUTTONDBLCLK:
            OnLButtonDblClk(LOWORD(lParam), HIWORD(lParam), wParam);
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(LOWORD(lParam), HIWORD(lParam), GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_KEYDOWN:
            OnKeyDown(wParam, lParam);
            return 0;
        case WM_DESTROY:
            m_synth.Shutdown();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void AppWindow::RunMessageLoop() {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void AppWindow::DrawCard(
    const D2D1_RECT_F& rect,
    const std::wstring& title,
    const std::wstring& subtitle,
    bool isHovered,
    bool isSelected,
    bool isRest
) {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 6.0f, 6.0f);

    ID2D1SolidColorBrush* fillBrush = m_pBrushCardBg;
    if (isSelected) fillBrush = m_pBrushCardSelected;
    else if (isHovered) fillBrush = m_pBrushCardHover;
    else if (isRest) fillBrush = m_pBrushRest;

    m_pRenderTarget->FillRoundedRectangle(rounded, fillBrush);
    m_pRenderTarget->DrawRoundedRectangle(
        rounded,
        isSelected ? m_pBrushAccent : (isHovered ? m_pBrushBorder : m_pBrushBorder),
        isSelected ? 2.0f : 1.0f
    );

    // タイトル (コード名)
    D2D1_RECT_F textRectTitle = D2D1::RectF(rect.left, rect.top + 4.0f, rect.right, rect.top + 28.0f);
    m_pRenderTarget->DrawText(
        title.c_str(), static_cast<UINT32>(title.size()),
        m_pFontCardTitle, textRectTitle, m_pBrushTextPrimary
    );

    // サブタイトル (ディグリー)
    D2D1_RECT_F textRectSub = D2D1::RectF(rect.left, rect.top + 28.0f, rect.right, rect.bottom - 4.0f);
    m_pRenderTarget->DrawText(
        subtitle.c_str(), static_cast<UINT32>(subtitle.size()),
        m_pFontCardSub, textRectSub, m_pBrushAccent
    );
}

void AppWindow::DrawButton(
    const D2D1_RECT_F& rect,
    const std::wstring& label,
    bool isHovered,
    [[maybe_unused]] bool isPressed,
    bool isAccent
) {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 4.0f, 4.0f);
    m_pRenderTarget->FillRoundedRectangle(rounded, isAccent ? m_pBrushAccent : (isHovered ? m_pBrushCardHover : m_pBrushCardBg));
    m_pRenderTarget->DrawRoundedRectangle(rounded, m_pBrushBorder, 1.0f);

    D2D1_RECT_F textRect = rect;
    m_pRenderTarget->DrawText(
        label.c_str(), static_cast<UINT32>(label.size()),
        m_pFontNormal, textRect, isAccent ? m_pBrushCardBg : m_pBrushTextPrimary
    );
}

void AppWindow::OnRender() {
    if (!CreateDeviceResources()) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(Theme::Background);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);

    float topBarHeight = 44.0f;
    float leftPaneWidth = 330.0f;
    float rightPaneWidth = 270.0f;
    float centerPaneWidth = width - leftPaneWidth - rightPaneWidth;
    float contentHeight = height - topBarHeight;

    // 1. 上部ツールバー
    RenderTopBar(width, topBarHeight);

    // 2. 左ペイン (テキスト入力 + ダイアトニック + 休符 + スケール外ビルダー)
    RenderLeftPane(0.0f, topBarHeight, leftPaneWidth, contentHeight);

    // 3. 中央ペイン (パートタブ + ルーラー/プレイヘッド + タイムライン + ピアノロール/鍵盤)
    RenderCenterPane(leftPaneWidth, topBarHeight, centerPaneWidth, contentHeight);

    // 4. 右ペイン (音楽理論パレット - 単一スクロール)
    RenderRightPane(width - rightPaneWidth, topBarHeight, rightPaneWidth, contentHeight);

    // ドラッグ中のカードゴースト描画
    if (m_isDraggingCard) {
        D2D1_RECT_F ghostRect = D2D1::RectF(
            static_cast<float>(m_dragCurrentPos.x) - 40.0f,
            static_cast<float>(m_dragCurrentPos.y) - 25.0f,
            static_cast<float>(m_dragCurrentPos.x) + 40.0f,
            static_cast<float>(m_dragCurrentPos.y) + 25.0f
        );
        std::wstring nameW = std::wstring(m_draggedChord.rootName.begin(), m_draggedChord.rootName.end());
        std::wstring degW = std::wstring(m_draggedChord.degreeRoman.begin(), m_draggedChord.degreeRoman.end());
        DrawCard(ghostRect, nameW, degW, true, true, m_draggedChord.isRest);
    }

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void AppWindow::RenderTopBar(float width, float height) {
    D2D1_RECT_F barRect = D2D1::RectF(0.0f, 0.0f, width, height);
    m_pRenderTarget->FillRectangle(barRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(0.0f, height),
        D2D1::Point2F(width, height),
        m_pBrushBorder, 1.0f
    );

    float curX = 12.0f;
    float btnY = 6.0f;
    float btnH = 32.0f;

    // 再生 / 停止ボタン
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 70.0f, btnY + btnH),
        m_isPlaying ? I18n::Get(StrId::Stop) : I18n::Get(StrId::Play),
        false, false, m_isPlaying
    );
    curX += 78.0f;

    // ループ切替
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 60.0f, btnY + btnH),
        I18n::Get(StrId::Loop),
        false, false, m_isLooping
    );
    curX += 68.0f;

    // BPM表示
    Part* activePart = m_partManager.GetActivePart();
    int bpm = activePart ? activePart->bpm : 120;
    std::wstring bpmStr = L"BPM " + std::to_wstring(bpm);
    DrawButton(D2D1::RectF(curX, btnY, curX + 76.0f, btnY + btnH), bpmStr, false, false, false);
    curX += 84.0f;

    // TAPボタン
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"TAP", false, false, false);
    curX += 54.0f;

    // 音色 (Instrument)
    static const wchar_t* kInstNames[] = { L"Piano", L"Warm Pad", L"E-Piano", L"Strings" };
    DrawButton(D2D1::RectF(curX, btnY, curX + 85.0f, btnY + btnH), kInstNames[m_selectedSoundIndex % 4], false, false, false);
    curX += 93.0f;

    // ボイシング
    static const wchar_t* kVoiceNames[] = { L"Root Pos", L"Drop 2", L"Drop 3", L"Open" };
    DrawButton(D2D1::RectF(curX, btnY, curX + 80.0f, btnY + btnH), kVoiceNames[m_selectedVoicingIndex % 4], false, false, false);
    curX += 88.0f;

    // DAWへドラッグボタン (CF_HDROP)
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 110.0f, btnY + btnH),
        I18n::Get(StrId::ExportMidiDaw),
        false, false, true
    );
    curX += 118.0f;

    // Undo / Redo
    DrawButton(D2D1::RectF(curX, btnY, curX + 46.0f, btnY + btnH), L"↶", false, false, false);
    curX += 50.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 46.0f, btnY + btnH), L"↷", false, false, false);

    // 言語切替 (右端)
    DrawButton(
        D2D1::RectF(width - 80.0f, btnY, width - 12.0f, btnY + btnH),
        I18n::Get(StrId::LangSwitch),
        false, false, false
    );
}

void AppWindow::RenderLeftPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(x + width, y),
        D2D1::Point2F(x + width, y + height),
        m_pBrushBorder, 1.0f
    );

    float curY = y + 10.0f;
    float padding = 12.0f;

    // 1. ダイアトニックコードセクション
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::DiatonicTitle).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::DiatonicTitle).size()),
        m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 20.0f),
        m_pBrushTextPrimary
    );
    curY += 26.0f;

    // 3和音 / 7th 切替スイッチ
    DrawButton(
        D2D1::RectF(x + padding, curY, x + padding + 70.0f, curY + 24.0f),
        I18n::Get(StrId::ShowTriads), false, false, !m_show7thDiatonic
    );
    DrawButton(
        D2D1::RectF(x + padding + 76.0f, curY, x + padding + 146.0f, curY + 24.0f),
        I18n::Get(StrId::Show7ths), false, false, m_show7thDiatonic
    );
    curY += 32.0f;

    // ダイアトニックコードカード一覧 (7枚) + 休符カード (1枚)
    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    auto diatonics = MusicEngine::GetDiatonicChords(key, mode, m_show7thDiatonic);

    float cardW = 70.0f;
    float cardH = 48.0f;
    float spacing = 6.0f;

    for (size_t i = 0; i < diatonics.size(); ++i) {
        float cx = x + padding + (i % 4) * (cardW + spacing);
        float cy = curY + (i / 4) * (cardH + spacing);

        std::wstring nameW = std::wstring(diatonics[i].rootName.begin(), diatonics[i].rootName.end());
        std::wstring degW = std::wstring(diatonics[i].degreeRoman.begin(), diatonics[i].degreeRoman.end());
        DrawCard(D2D1::RectF(cx, cy, cx + cardW, cy + cardH), nameW, degW, false, false, false);
    }

    // 休符 (Rest / N.C.) カード
    float restX = x + padding + 3 * (cardW + spacing);
    float restY = curY + 1 * (cardH + spacing);
    DrawCard(D2D1::RectF(restX, restY, restX + cardW, restY + cardH), L"Rest", L"N.C.", false, false, true);

    curY += (cardH + spacing) * 2 + 16.0f;

    // 2. スケール外・カスタムコードビルダー
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::QuickBuilderTitle).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::QuickBuilderTitle).size()),
        m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 20.0f),
        m_pBrushTextPrimary
    );
    curY += 26.0f;

    // 12音ルートピッカーボタン
    static const wchar_t* kRoots[12] = { L"C", L"C#", L"D", L"Eb", L"E", L"F", L"F#", L"G", L"Ab", L"A", L"Bb", L"B" };
    float btnW = (width - padding * 2 - 5 * 4.0f) / 6.0f;
    for (int i = 0; i < 12; ++i) {
        float bx = x + padding + (i % 6) * (btnW + 4.0f);
        float by = curY + (i / 6) * 26.0f;
        DrawButton(D2D1::RectF(bx, by, bx + btnW, by + 22.0f), kRoots[i], false, false, m_builderRoot == i);
    }
    curY += 56.0f;

    // クオリティピッカー
    static const wchar_t* kQualities[8] = { L"Maj", L"m", L"7", L"maj7", L"m7", L"m7b5", L"dim7", L"sus4" };
    float qBtnW = (width - padding * 2 - 3 * 4.0f) / 4.0f;
    for (int i = 0; i < 8; ++i) {
        float bx = x + padding + (i % 4) * (qBtnW + 4.0f);
        float by = curY + (i / 4) * 26.0f;
        DrawButton(D2D1::RectF(bx, by, bx + qBtnW, by + 22.0f), kQualities[i], false, false, m_builderQuality == i);
    }
    curY += 58.0f;

    // 生成コードのプレビューカード & 追加ボタン
    PitchClass pRoot = static_cast<PitchClass>(m_builderRoot);
    static const ChordQuality kQEnums[8] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Diminished7, ChordQuality::Sus4
    };
    ChordQuality qEnum = kQEnums[m_builderQuality % 8];
    std::string bChordName = MusicEngine::PitchToNote(pRoot, false) + ((qEnum == ChordQuality::Minor) ? "m" : (qEnum == ChordQuality::Minor7 ? "m7" : (qEnum == ChordQuality::Major7 ? "maj7" : (qEnum == ChordQuality::Dominant7 ? "7" : ""))));
    std::string bDeg = MusicEngine::CalculateDegree(pRoot, qEnum, MusicEngine::NoteToPitch(key), mode);

    DrawCard(
        D2D1::RectF(x + padding, curY, x + padding + 90.0f, curY + 50.0f),
        std::wstring(bChordName.begin(), bChordName.end()),
        std::wstring(bDeg.begin(), bDeg.end()),
        false, false, false
    );

    DrawButton(
        D2D1::RectF(x + padding + 98.0f, curY + 10.0f, x + width - padding, curY + 42.0f),
        I18n::Get(StrId::AddCurrentChord),
        false, false, true
    );
}

void AppWindow::RenderCenterPane(float x, float y, float width, float height) {
    // 中央ペイン上部: パートタブバー (高さ 36px)
    float tabH = 36.0f;
    D2D1_RECT_F tabRect = D2D1::RectF(x, y, x + width, y + tabH);
    m_pRenderTarget->FillRectangle(tabRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + tabH), D2D1::Point2F(x + width, y + tabH), m_pBrushBorder, 1.0f);

    float tx = x + 8.0f;
    // [★ 全体マージ] タブ
    DrawButton(
        D2D1::RectF(tx, y + 4.0f, tx + 130.0f, y + tabH - 4.0f),
        I18n::Get(StrId::FullSongTab), false, false, m_isMergedView
    );
    tx += 136.0f;

    // パートタブ列
    const auto& parts = m_partManager.GetParts();
    for (size_t i = 0; i < parts.size(); ++i) {
        std::wstring pTitle = std::wstring(parts[i].name.begin(), parts[i].name.end()) + L" (" + std::wstring(parts[i].keyRoot.begin(), parts[i].keyRoot.end()) + L")";
        bool isAct = (!m_isMergedView && m_partManager.GetActiveIndex() == static_cast<int>(i));
        DrawButton(D2D1::RectF(tx, y + 4.0f, tx + 96.0f, y + tabH - 4.0f), pTitle, false, false, isAct);
        tx += 100.0f;
    }

    // [+ パート追加]
    DrawButton(D2D1::RectF(tx, y + 4.0f, tx + 84.0f, y + tabH - 4.0f), I18n::Get(StrId::AddPart), false, false, false);

    // DAW風タイムルーラー (高さ 28px)
    float rulerY = y + tabH;
    float rulerH = 28.0f;
    D2D1_RECT_F rulerRect = D2D1::RectF(x, rulerY, x + width, rulerY + rulerH);
    m_pRenderTarget->FillRectangle(rulerRect, m_pBrushBackground);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, rulerY + rulerH), D2D1::Point2F(x + width, rulerY + rulerH), m_pBrushBorder, 1.0f);

    // 小節・拍ルーラーの目盛り描画
    float barW = 140.0f; // 1小節の幅 (px)
    int totalBars = 32;

    for (int b = 0; b < totalBars; ++b) {
        float bx = x + b * barW - m_timelineScrollX;
        if (bx + barW < x || bx > x + width) continue;

        // 小節線
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(bx, rulerY),
            D2D1::Point2F(bx, rulerY + rulerH),
            m_pBrushBorder, 1.5f
        );

        // 小節番号テキスト
        std::wstring barNumStr = std::to_wstring(b + 1);
        m_pRenderTarget->DrawText(
            barNumStr.c_str(), static_cast<UINT32>(barNumStr.size()),
            m_pFontSmall,
            D2D1::RectF(bx + 4.0f, rulerY + 4.0f, bx + 40.0f, rulerY + rulerH),
            m_pBrushTextSecondary
        );

        // 4拍のサブ目盛り
        for (int beat = 1; beat < 4; ++beat) {
            float subX = bx + (barW / 4.0f) * beat;
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(subX, rulerY + rulerH - 6.0f),
                D2D1::Point2F(subX, rulerY + rulerH),
                m_pBrushBorder, 0.8f
            );
        }
    }

    // タイムライン描画領域 (高さ: 320px)
    float tlY = rulerY + rulerH;
    float tlH = 320.0f;

    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});

    for (size_t bi = 0; bi < currentBars.size(); ++bi) {
        float bx = x + bi * barW - m_timelineScrollX;
        if (bx + barW < x || bx > x + width) continue;

        // 小節枠
        D2D1_RECT_F bRect = D2D1::RectF(bx, tlY, bx + barW, tlY + tlH);
        m_pRenderTarget->DrawRectangle(bRect, m_pBrushBorder, 1.0f);

        // 転調マーカー
        if (!currentBars[bi].keyChange.empty()) {
            std::wstring kcStr = L"🔑 " + std::wstring(currentBars[bi].keyChange.begin(), currentBars[bi].keyChange.end());
            m_pRenderTarget->DrawText(
                kcStr.c_str(), static_cast<UINT32>(kcStr.size()),
                m_pFontSmall,
                D2D1::RectF(bx + 4.0f, tlY + 4.0f, bx + barW, tlY + 20.0f),
                m_pBrushAccent
            );
        }

        // コードカード描画
        float curCardX = bx + 4.0f;
        for (size_t ci = 0; ci < currentBars[bi].chords.size(); ++ci) {
            const auto& chord = currentBars[bi].chords[ci];
            float cardSlotW = ((barW - 8.0f) / 4.0f) * chord.durationBeats;

            std::wstring cTitle = chord.isRest ? L"Rest" : (std::wstring(chord.rootName.begin(), chord.rootName.end()) + std::wstring(chord.qualitySuffix.begin(), chord.qualitySuffix.end()));
            std::wstring cDeg = std::wstring(chord.degreeRoman.begin(), chord.degreeRoman.end());

            bool isSel = (m_selection.startBar <= static_cast<int>(bi) && static_cast<int>(bi) <= m_selection.endBar);
            DrawCard(
                D2D1::RectF(curCardX, tlY + 26.0f, curCardX + cardSlotW - 4.0f, tlY + 90.0f),
                cTitle, cDeg, false, isSel, chord.isRest
            );

            curCardX += cardSlotW;
        }
    }

    // プレイヘッド描画 (赤い縦棒)
    float playheadX = x + (m_playbackBeat / 4.0f) * barW - m_timelineScrollX;
    if (playheadX >= x && playheadX <= x + width) {
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(playheadX, rulerY),
            D2D1::Point2F(playheadX, y + height),
            m_pBrushPlayhead, 2.0f
        );
    }

    // ピアノロール & 61鍵盤描画 (下部)
    RenderPianoRollAndKeyboard(x, tlY + tlH, width, height - (tlY + tlH - y));
}

void AppWindow::RenderPianoRollAndKeyboard(float x, float y, float width, float height) {
    D2D1_RECT_F pRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(pRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    // 鍵盤 (61鍵: C2=36 から C7=96)
    float keyW = width / 36.0f; // 白鍵数 36
    float keyH = height * 0.4f;
    float kY = y + height - keyH;

    for (int i = 0; i < 36; ++i) {
        float kx = x + i * keyW;
        D2D1_RECT_F wk = D2D1::RectF(kx, kY, kx + keyW, kY + keyH);
        m_pRenderTarget->FillRectangle(wk, m_pBrushCardBg);
        m_pRenderTarget->DrawRectangle(wk, m_pBrushBorder, 1.0f);
    }
}

void AppWindow::RenderRightPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x, y + height), m_pBrushBorder, 1.0f);

    float curY = y + 10.0f - m_rightPaneScrollY;
    float padding = 10.0f;

    // タイトル
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::TheoryPaletteTitle).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::TheoryPaletteTitle).size()),
        m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 20.0f),
        m_pBrushTextPrimary
    );
    curY += 26.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    // 1. 同主調借用和音 (Modal Interchange)
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::ModalInterchange).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::ModalInterchange).size()),
        m_pFontSmall,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 16.0f),
        m_pBrushAccent
    );
    curY += 20.0f;

    auto miChords = MusicEngine::GetModalInterchangeChords(key, mode);
    float cardW = 74.0f;
    float cardH = 46.0f;
    for (size_t i = 0; i < miChords.size(); ++i) {
        float cx = x + padding + (i % 3) * (cardW + 8.0f);
        float cy = curY + (i / 3) * (cardH + 6.0f);
        std::wstring nW = std::wstring(miChords[i].rootName.begin(), miChords[i].rootName.end()) + std::wstring(miChords[i].qualitySuffix.begin(), miChords[i].qualitySuffix.end());
        std::wstring dW = std::wstring(miChords[i].degreeRoman.begin(), miChords[i].degreeRoman.end());
        DrawCard(D2D1::RectF(cx, cy, cx + cardW, cy + cardH), nW, dW, false, false, false);
    }
    curY += (cardH + 6.0f) * 3 + 12.0f;

    // 2. セカンダリードミナント
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::SecondaryDominants).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::SecondaryDominants).size()),
        m_pFontSmall,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 16.0f),
        m_pBrushAccent
    );
    curY += 20.0f;

    auto secDoms = MusicEngine::GetSecondaryDominants(key, mode);
    for (size_t i = 0; i < secDoms.size(); ++i) {
        float cx = x + padding + (i % 3) * (cardW + 8.0f);
        float cy = curY + (i / 3) * (cardH + 6.0f);
        std::wstring nW = std::wstring(secDoms[i].rootName.begin(), secDoms[i].rootName.end()) + L"7";
        std::wstring dW = std::wstring(secDoms[i].degreeRoman.begin(), secDoms[i].degreeRoman.end());
        DrawCard(D2D1::RectF(cx, cy, cx + cardW, cy + cardH), nW, dW, false, false, false);
    }
    curY += (cardH + 6.0f) * 2 + 12.0f;

    // 3. 定番コード進行テンプレート
    m_pRenderTarget->DrawText(
        I18n::Get(StrId::Templates).c_str(),
        static_cast<UINT32>(I18n::Get(StrId::Templates).size()),
        m_pFontSmall,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 16.0f),
        m_pBrushAccent
    );
    curY += 20.0f;

    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 28.0f), I18n::Get(StrId::TmplJPopRoyal), false, false, false);
    curY += 34.0f;
    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 28.0f), I18n::Get(StrId::TmplJPopKomuro), false, false, false);
    curY += 34.0f;
    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 28.0f), I18n::Get(StrId::TmplCanon), false, false, false);
    curY += 34.0f;
    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 28.0f), I18n::Get(StrId::TmplPop251), false, false, false);
    curY += 34.0f;
    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 28.0f), I18n::Get(StrId::TmplJPopOwari), false, false, false);

    // 右ペイン全体の単一スリムスクロールバー描画
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(x + width - 3.0f, y),
        D2D1::Point2F(x + width - 3.0f, y + height),
        m_pBrushBorder, 4.0f
    );
}

void AppWindow::StartPlayback() {
    m_isPlaying = true;
    m_lastPlaybackTime = std::chrono::high_resolution_clock::now();
    m_lastSoundedBeat = -1;
}

void AppWindow::StopPlayback() {
    m_isPlaying = false;
    m_synth.AllNotesOff();
    m_lastSoundedBeat = -1;
}

void AppWindow::TogglePlayback() {
    if (m_isPlaying) StopPlayback();
    else StartPlayback();
}

void AppWindow::SeekToBeat(int totalBeats) {
    m_playbackBeat = static_cast<float>(totalBeats);
    m_lastSoundedBeat = totalBeats - 1;
}

void AppWindow::AdvancePlayback(float deltaSeconds) {
    Part* pActive = m_partManager.GetActivePart();
    int bpm = pActive ? pActive->bpm : 120;
    float beatsPerSec = static_cast<float>(bpm) / 60.0f;

    m_playbackBeat += deltaSeconds * beatsPerSec;

    int currentBeatInt = static_cast<int>(m_playbackBeat);

    // 小節・拍からコードを特定して発音
    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (pActive ? pActive->bars : std::vector<Bar>{});

    int totalBeats = 0;
    for (const auto& b : currentBars) {
        totalBeats += b.timeNumerator;
    }
    if (totalBeats <= 0) totalBeats = 16;

    if (m_playbackBeat >= totalBeats) {
        if (m_isLooping) {
            m_playbackBeat = 0.0f;
            currentBeatInt = 0;
            m_lastSoundedBeat = -1;
        } else {
            StopPlayback();
            return;
        }
    }

    if (currentBeatInt != m_lastSoundedBeat) {
        m_lastSoundedBeat = currentBeatInt;

        // 該当ビートのコードを検出
        int accBeats = 0;
        for (const auto& b : currentBars) {
            for (const auto& c : b.chords) {
                if (currentBeatInt >= accBeats && currentBeatInt < accBeats + c.durationBeats) {
                    if (!c.isRest && c.rootPitch != PitchClass::Unknown) {
                        auto notes = MusicEngine::GetChordMidiNotes(c, 4, static_cast<VoicingStyle>(m_selectedVoicingIndex));
                        m_synth.PlayChord(notes, 0.8f);
                    } else {
                        m_synth.AllNotesOff();
                    }
                    return;
                }
                accBeats += c.durationBeats;
            }
        }
    }
}

void AppWindow::EnsureVisible(int barIndex) {
    float barW = 140.0f;
    float barX = barIndex * barW;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float centerW = static_cast<float>(rc.right - rc.left) - 600.0f;

    if (barX < m_timelineScrollX) {
        m_timelineScrollX = barX - 20.0f;
    } else if (barX + barW > m_timelineScrollX + centerW) {
        m_timelineScrollX = barX + barW - centerW + 40.0f;
    }
    if (m_timelineScrollX < 0.0f) m_timelineScrollX = 0.0f;
}

void AppWindow::OnLButtonDown(int x, int y, [[maybe_unused]] WPARAM wParam) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);

    // 上部ツールバークリック
    if (y < 44) {
        if (x >= 12 && x <= 82) {
            TogglePlayback();
        } else if (x >= 90 && x <= 150) {
            m_isLooping = !m_isLooping;
        } else if (x >= 320 && x <= 405) {
            m_selectedSoundIndex = (m_selectedSoundIndex + 1) % 4;
            m_synth.SetPreset(static_cast<SynthPreset>(m_selectedSoundIndex));
        } else if (x >= 415 && x <= 495) {
            m_selectedVoicingIndex = (m_selectedVoicingIndex + 1) % 4;
        } else if (x >= 505 && x <= 615) {
            // DAWドラッグ開始
            std::vector<Bar> currentBars = m_isMergedView ?
                m_partManager.GenerateMergedProgression() :
                (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});
            std::wstring tempMidi = MidiBuilder::ExportToTempFile(currentBars, 120);
            if (!tempMidi.empty()) {
                OleFileDataObject::StartFileDrag(m_hwnd, tempMidi);
            }
        } else if (x >= width - 80 && x <= width - 12) {
            // 言語切替
            I18n::SetLanguage(I18n::GetLanguage() == Language::Japanese ? Language::English : Language::Japanese);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // ルーラー領域クリック (シーク)
    if (x >= 330 && x <= width - 270 && y >= 80 && y <= 108) {
        float relX = x - 330.0f + m_timelineScrollX;
        int clickedBeat = static_cast<int>(relX / (140.0f / 4.0f));
        SeekToBeat(clickedBeat);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // 左ペインのクイックビルダー追加ボタンクリック
    if (x >= 110 && x <= 318 && y >= 370 && y <= 402) {
        PitchClass pRoot = static_cast<PitchClass>(m_builderRoot);
        static const ChordQuality kQEnums[8] = {
            ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7, ChordQuality::Major7,
            ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Diminished7, ChordQuality::Sus4
        };
        ChordToken t;
        t.rootPitch = pRoot;
        t.quality = kQEnums[m_builderQuality % 8];
        t.rootName = MusicEngine::PitchToNote(pRoot, false);
        t.durationBeats = 4;

        Part* pActive = m_partManager.GetActivePart();
        if (pActive) {
            Bar newBar;
            newBar.barNumber = static_cast<int>(pActive->bars.size()) + 1;
            newBar.chords.push_back(t);
            pActive->bars.push_back(newBar);
            EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
}

void AppWindow::OnMouseMove(int x, int y, [[maybe_unused]] WPARAM wParam) {
    if (m_isDraggingCard) {
        m_dragCurrentPos = {x, y};
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::OnLButtonUp([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] WPARAM wParam) {
    if (m_isDraggingCard) {
        m_isDraggingCard = false;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::OnLButtonDblClk(int x, int y, [[maybe_unused]] WPARAM wParam) {
    // 左ペインのダイアトニックカードのダブルクリック -> タイムライン末尾に追加
    if (x >= 12 && x <= 320 && y >= 112 && y <= 220) {
        int col = (x - 12) / 76;
        int row = (y - 112) / 54;
        int idx = row * 4 + col;

        Part* pActive = m_partManager.GetActivePart();
        if (!pActive) return;

        if (idx == 7) {
            // 休符追加
            ChordToken r;
            r.isRest = true;
            r.quality = ChordQuality::Rest;
            r.rootName = "R";
            r.degreeRoman = "R";
            r.durationBeats = 4;
            Bar b;
            b.barNumber = static_cast<int>(pActive->bars.size()) + 1;
            b.chords.push_back(r);
            pActive->bars.push_back(b);
            EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
        } else if (idx >= 0 && idx < 7) {
            auto dt = MusicEngine::GetDiatonicChords(pActive->keyRoot, pActive->mode, m_show7thDiatonic);
            if (idx < static_cast<int>(dt.size())) {
                ChordToken c = dt[idx];
                c.durationBeats = 4;
                Bar b;
                b.barNumber = static_cast<int>(pActive->bars.size()) + 1;
                b.chords.push_back(c);
                pActive->bars.push_back(b);
                EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
}

void AppWindow::OnMouseWheel([[maybe_unused]] int x, [[maybe_unused]] int y, short delta) {
    m_timelineScrollX -= delta * 0.5f;
    if (m_timelineScrollX < 0.0f) m_timelineScrollX = 0.0f;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::OnKeyDown(WPARAM wParam, [[maybe_unused]] LPARAM lParam) {
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (wParam == VK_SPACE) {
        TogglePlayback();
    } else if (isCtrl && wParam == 'C') {
        CopySelection();
    } else if (isCtrl && wParam == 'X') {
        CutSelection();
    } else if (isCtrl && wParam == 'V') {
        PasteAtCursor();
    } else if (isCtrl && wParam == 'D') {
        DuplicateSelection();
    } else if (isCtrl && wParam == 'A') {
        SelectAll();
    } else if (wParam == VK_DELETE) {
        DeleteSelection();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::CopySelection() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || !m_selection.HasSelection()) return;

    m_selection.clipboard.clear();
    for (int bi = m_selection.startBar; bi <= m_selection.endBar && bi < static_cast<int>(pActive->bars.size()); ++bi) {
        for (const auto& c : pActive->bars[bi].chords) {
            m_selection.clipboard.push_back(c);
        }
    }
}

void AppWindow::CutSelection() {
    CopySelection();
    DeleteSelection();
}

void AppWindow::PasteAtCursor() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selection.clipboard.empty()) return;

    for (const auto& c : m_selection.clipboard) {
        Bar b;
        b.barNumber = static_cast<int>(pActive->bars.size()) + 1;
        b.chords.push_back(c);
        pActive->bars.push_back(b);
    }
    EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
}

void AppWindow::DuplicateSelection() {
    CopySelection();
    PasteAtCursor();
}

void AppWindow::DeleteSelection() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || !m_selection.HasSelection()) return;

    int start = std::max(0, m_selection.startBar);
    int end = std::min(static_cast<int>(pActive->bars.size()) - 1, m_selection.endBar);

    if (start <= end && pActive->bars.size() > 1) {
        pActive->bars.erase(pActive->bars.begin() + start, pActive->bars.begin() + end + 1);
    }
    m_selection.Clear();
}

void AppWindow::SelectAll() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || pActive->bars.empty()) return;

    m_selection.startBar = 0;
    m_selection.endBar = static_cast<int>(pActive->bars.size()) - 1;
}

} // namespace clp
