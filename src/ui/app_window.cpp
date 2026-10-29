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

static std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, nullptr, nullptr);
    return strTo;
}

static std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

AppWindow::AppWindow() = default;

AppWindow::~AppWindow() {
    DiscardDeviceResources();
    if (m_hEditFont) DeleteObject(m_hEditFont);
    if (m_pFontTitle) m_pFontTitle->Release();
    if (m_pFontHeader) m_pFontHeader->Release();
    if (m_pFontNormal) m_pFontNormal->Release();
    if (m_pFontSmall) m_pFontSmall->Release();
    if (m_pFontTiny) m_pFontTiny->Release();
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

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"ja-jp", &m_pFontTitle
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ja-jp", &m_pFontHeader
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"ja-jp", &m_pFontNormal
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"ja-jp", &m_pFontSmall
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 8.5f, L"ja-jp", &m_pFontTiny
    );

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"ja-jp", &m_pFontCardTitle
    );
    if (m_pFontCardTitle) {
        m_pFontCardTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFontCardTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_pDWriteFactory->CreateTextFormat(
        fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"ja-jp", &m_pFontCardSub
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
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.25f, 0.22f, 1.0f), &m_pBrushStop);
    m_pRenderTarget->CreateSolidColorBrush(Theme::Playhead, &m_pBrushPlayhead);
    m_pRenderTarget->CreateSolidColorBrush(Theme::LoopRangeFill, &m_pBrushLoopRange);
    m_pRenderTarget->CreateSolidColorBrush(Theme::RestBlockFill, &m_pBrushRest);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.65f, 0.90f, 0.75f), &m_pBrushNoteBar);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.55f, 0.20f, 1.0f), &m_pBrushNoteActive);
    m_pRenderTarget->CreateSolidColorBrush(Theme::BlackKey, &m_pBrushBlackKey);

    return true;
}

void AppWindow::DiscardDeviceResources() {
    if (m_pBrushBlackKey) { m_pBrushBlackKey->Release(); m_pBrushBlackKey = nullptr; }
    if (m_pBrushNoteActive) { m_pBrushNoteActive->Release(); m_pBrushNoteActive = nullptr; }
    if (m_pBrushNoteBar) { m_pBrushNoteBar->Release(); m_pBrushNoteBar = nullptr; }
    if (m_pBrushRest) { m_pBrushRest->Release(); m_pBrushRest = nullptr; }
    if (m_pBrushLoopRange) { m_pBrushLoopRange->Release(); m_pBrushLoopRange = nullptr; }
    if (m_pBrushPlayhead) { m_pBrushPlayhead->Release(); m_pBrushPlayhead = nullptr; }
    if (m_pBrushStop) { m_pBrushStop->Release(); m_pBrushStop = nullptr; }
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

    m_hwnd = CreateWindowExW(
        0,
        wcex.lpszClassName,
        L"Chord Launchpad",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1380, 880,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    // 左ペインの複数行テキストエディットボックスを作成
    m_hwndInputEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        12, 108, 256, 76,
        m_hwnd,
        reinterpret_cast<HMENU>(101),
        hInstance,
        nullptr
    );

    m_hEditFont = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Yu Gothic UI"
    );
    if (m_hwndInputEdit && m_hEditFont) {
        SendMessage(m_hwndInputEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hEditFont), TRUE);
    }

    SyncTimelineToText();

    m_synth.Initialize();

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    SetTimer(m_hwnd, 1, 16, nullptr); // 60FPS

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
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 5.0f, 5.0f);

    ID2D1SolidColorBrush* fillBrush = m_pBrushCardBg;
    if (isSelected) fillBrush = m_pBrushCardSelected;
    else if (isHovered) fillBrush = m_pBrushCardHover;
    else if (isRest) fillBrush = m_pBrushRest;

    m_pRenderTarget->FillRoundedRectangle(rounded, fillBrush);
    m_pRenderTarget->DrawRoundedRectangle(
        rounded,
        isSelected ? m_pBrushAccent : m_pBrushBorder,
        isSelected ? 2.0f : 1.0f
    );

    float midY = (rect.top + rect.bottom) * 0.5f;

    // タイトル (コード名)
    D2D1_RECT_F textRectTitle = D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, midY + 2.0f);
    m_pRenderTarget->DrawText(
        title.c_str(), static_cast<UINT32>(title.size()),
        m_pFontCardTitle, textRectTitle, m_pBrushTextPrimary
    );

    // サブタイトル (ディグリー)
    D2D1_RECT_F textRectSub = D2D1::RectF(rect.left + 2.0f, midY, rect.right - 2.0f, rect.bottom - 2.0f);
    m_pRenderTarget->DrawText(
        subtitle.c_str(), static_cast<UINT32>(subtitle.size()),
        m_pFontCardSub, textRectSub, m_pBrushAccent
    );
}

void AppWindow::DrawRoundBadge(
    const D2D1_RECT_F& rect,
    const std::wstring& title,
    const std::wstring& subtitle,
    bool isHovered,
    bool isSelected
) {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 16.0f, 16.0f);
    m_pRenderTarget->FillRoundedRectangle(rounded, isSelected ? m_pBrushCardSelected : (isHovered ? m_pBrushCardHover : m_pBrushCardBg));
    m_pRenderTarget->DrawRoundedRectangle(rounded, isSelected ? m_pBrushAccent : m_pBrushBorder, isSelected ? 2.0f : 1.0f);

    float midY = (rect.top + rect.bottom) * 0.5f;
    D2D1_RECT_F tRect = D2D1::RectF(rect.left, rect.top + 2.0f, rect.right, midY + 2.0f);
    m_pRenderTarget->DrawText(title.c_str(), static_cast<UINT32>(title.size()), m_pFontCardTitle, tRect, m_pBrushTextPrimary);

    D2D1_RECT_F sRect = D2D1::RectF(rect.left, midY, rect.right, rect.bottom - 2.0f);
    m_pRenderTarget->DrawText(subtitle.c_str(), static_cast<UINT32>(subtitle.size()), m_pFontCardSub, sRect, m_pBrushAccent);
}

void AppWindow::DrawButton(
    const D2D1_RECT_F& rect,
    const std::wstring& label,
    bool isHovered,
    [[maybe_unused]] bool isPressed,
    bool isAccent,
    D2D1_COLOR_F customBg
) {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 4.0f, 4.0f);

    if (customBg.a > 0.0f) {
        ID2D1SolidColorBrush* cBrush = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(customBg, &cBrush);
        if (cBrush) {
            m_pRenderTarget->FillRoundedRectangle(rounded, cBrush);
            cBrush->Release();
        }
    } else {
        m_pRenderTarget->FillRoundedRectangle(rounded, isAccent ? m_pBrushAccent : (isHovered ? m_pBrushCardHover : m_pBrushCardBg));
    }

    m_pRenderTarget->DrawRoundedRectangle(rounded, m_pBrushBorder, 1.0f);

    D2D1_RECT_F textRect = rect;
    m_pRenderTarget->DrawText(
        label.c_str(), static_cast<UINT32>(label.size()),
        m_pFontNormal, textRect, (isAccent || customBg.a > 0.0f) ? m_pBrushCardBg : m_pBrushTextPrimary
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

    float topBarH1 = 34.0f;
    float topBarH2 = 34.0f;
    float statusBarH = 22.0f;
    float leftPaneW = 280.0f;
    float rightPaneW = 260.0f;
    float centerPaneW = width - leftPaneW - rightPaneW;
    float contentY = topBarH1 + topBarH2;
    float contentH = height - contentY - statusBarH;

    // 1. 上部ツールバー 2段
    RenderTopToolbarRow1(width, topBarH1);
    RenderTopToolbarRow2(width, topBarH1, topBarH2);

    // 2. 左ペイン
    RenderLeftPane(0.0f, contentY, leftPaneW, contentH);

    // 3. 中央ペイン
    RenderCenterPane(leftPaneW, contentY, centerPaneW, contentH);

    // 4. 右ペイン
    RenderRightPane(width - rightPaneW, contentY, rightPaneW, contentH);

    // 5. ステータスバー
    RenderStatusBar(0.0f, height - statusBarH, width, statusBarH);

    // ドラッグ中のカードゴースト描画
    if (m_isDraggingCard) {
        D2D1_RECT_F ghostRect = D2D1::RectF(
            static_cast<float>(m_dragCurrentPos.x) - 36.0f,
            static_cast<float>(m_dragCurrentPos.y) - 24.0f,
            static_cast<float>(m_dragCurrentPos.x) + 36.0f,
            static_cast<float>(m_dragCurrentPos.y) + 24.0f
        );
        std::wstring nameW = Utf8ToWString(m_draggedChord.rootName);
        std::wstring degW = Utf8ToWString(m_draggedChord.degreeRoman);
        DrawCard(ghostRect, nameW, degW, true, true, m_draggedChord.isRest);
    }

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void AppWindow::RenderTopToolbarRow1(float width, float height) {
    D2D1_RECT_F barRect = D2D1::RectF(0.0f, 0.0f, width, height);
    m_pRenderTarget->FillRectangle(barRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(0.0f, height), D2D1::Point2F(width, height), m_pBrushBorder, 1.0f);

    float curX = 10.0f;
    float btnY = 4.0f;
    float btnH = 26.0f;

    // 新規・開く・保存
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"新規", false, false); curX += 48.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"開く", false, false); curX += 48.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"保存", false, false); curX += 54.0f;

    // Undo / Redo
    DrawButton(D2D1::RectF(curX, btnY, curX + 28.0f, btnY + btnH), L"↶", false, false); curX += 32.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 28.0f, btnY + btnH), L"↷", false, false); curX += 42.0f;

    // 再生 ▶ (緑色)
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 68.0f, btnY + btnH),
        L"▶ 再生", false, false, false,
        m_isPlaying ? D2D1::ColorF(0.12f, 0.55f, 0.22f) : D2D1::ColorF(0.15f, 0.65f, 0.28f)
    );
    curX += 72.0f;

    // 停止 ■ (赤色)
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 54.0f, btnY + btnH),
        L"■ 停止", false, false, false,
        D2D1::ColorF(0.85f, 0.25f, 0.22f)
    );
    curX += 58.0f;

    // ループ 🔁
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 54.0f, btnY + btnH),
        L"🔁 ループ", false, false, m_isLooping
    );
    curX += 62.0f;

    // MIDI保存
    DrawButton(D2D1::RectF(curX, btnY, curX + 74.0f, btnY + btnH), L"MIDI保存", false, false); curX += 78.0f;

    // DAWへMIDIドラッグ (青色・ドラッグハンドル)
    DrawButton(
        D2D1::RectF(curX, btnY, curX + 130.0f, btnY + btnH),
        L"♫ DAWへドラッグ", false, false, false,
        D2D1::ColorF(0.12f, 0.45f, 0.85f)
    );
    curX += 136.0f;

    // 設定 ⚙ (右端)
    DrawButton(
        D2D1::RectF(width - 70.0f, btnY, width - 10.0f, btnY + btnH),
        L"⚙ 設定", false, false, false
    );
}

void AppWindow::RenderTopToolbarRow2(float width, float y, float height) {
    D2D1_RECT_F barRect = D2D1::RectF(0.0f, y, width, y + height);
    m_pRenderTarget->FillRectangle(barRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(0.0f, y + height), D2D1::Point2F(width, y + height), m_pBrushBorder, 1.0f);

    float curX = 10.0f;
    float btnY = y + 4.0f;
    float btnH = 26.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;
    int bpm = pActive ? pActive->bpm : 120;

    // キー: [ C ▾ ]
    std::wstring keyStr = L"キー: " + Utf8ToWString(key) + L" ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 70.0f, btnY + btnH), keyStr, false, false);
    curX += 76.0f;

    // スケール: [ メジャー ▾ ]
    std::wstring modeStr = L"スケール: " + std::wstring((mode == MusicalMode::Major) ? L"メジャー" : L"マイナー") + L" ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 104.0f, btnY + btnH), modeStr, false, false);
    curX += 110.0f;

    // BPM: [120] [-] [+]
    std::wstring bpmStr = L"BPM: " + std::to_wstring(bpm);
    DrawButton(D2D1::RectF(curX, btnY, curX + 66.0f, btnY + btnH), bpmStr, false, false); curX += 68.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 22.0f, btnY + btnH), L"-", false, false); curX += 24.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 22.0f, btnY + btnH), L"+", false, false); curX += 26.0f;

    // Tap (幅50pxで余裕確保)
    DrawButton(D2D1::RectF(curX, btnY, curX + 50.0f, btnY + btnH), L"♫ Tap", false, false); curX += 56.0f;

    // 音色: [ Piano ▾ ]
    static const wchar_t* kInsts[] = { L"Piano", L"Pad", L"E-Piano", L"Strings" };
    std::wstring instStr = L"音色: " + std::wstring(kInsts[m_selectedSoundIndex % 4]) + L" ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 90.0f, btnY + btnH), instStr, false, false); curX += 96.0f;

    // ベース: [ なし ▾ ]
    static const wchar_t* kBasses[] = { L"なし", L"1音", L"2音", L"両方" };
    std::wstring bassStr = L"ベース: " + std::wstring(kBasses[m_selectedBassIndex % 4]) + L" ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 84.0f, btnY + btnH), bassStr, false, false); curX += 90.0f;

    // ボイシング: [ 基本形 ▾ ]
    static const wchar_t* kVoicings[] = { L"基本形", L"Drop 2", L"Drop 3", L"Open" };
    std::wstring voiceStr = L"ボイシング: " + std::wstring(kVoicings[m_selectedVoicingIndex % 4]) + L" ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 100.0f, btnY + btnH), voiceStr, false, false); curX += 106.0f;

    // グリッド: [ 1拍 ▾ ]
    std::wstring gridStr = L"グリッド: 1拍 ▾";
    if (m_selectedGridBeats == 0.25f) gridStr = L"グリッド: 1/16 ▾";
    else if (m_selectedGridBeats == 0.5f) gridStr = L"グリッド: 1/8 ▾";
    else if (m_selectedGridBeats == 2.0f) gridStr = L"グリッド: 2拍 ▾";
    else if (m_selectedGridBeats == 4.0f) gridStr = L"グリッド: 1小節 ▾";
    DrawButton(D2D1::RectF(curX, btnY, curX + 96.0f, btnY + btnH), gridStr, false, false); curX += 104.0f;

    // 操作: [ 複製 ] [ 削除 ] [ 全クリア ]
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"複製", false, false); curX += 48.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 44.0f, btnY + btnH), L"削除", false, false); curX += 48.0f;
    DrawButton(D2D1::RectF(curX, btnY, curX + 64.0f, btnY + btnH), L"全クリア", false, false);
}

void AppWindow::RenderLeftPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + width, y), D2D1::Point2F(x + width, y + height), m_pBrushBorder, 1.0f);

    float padding = 10.0f;
    float curY = y + 8.0f;

    // 1. コード / ディグリー入力
    m_pRenderTarget->DrawText(
        L"コード / ディグリー入力", 12, m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 18.0f),
        m_pBrushTextPrimary
    );
    curY += 20.0f;

    // EDIT コントロールの位置調整 (HWND)
    if (m_hwndInputEdit) {
        SetWindowPos(m_hwndInputEdit, nullptr, static_cast<int>(x + padding), static_cast<int>(curY), static_cast<int>(width - padding * 2), 70, SWP_NOZORDER);
    }
    curY += 76.0f;

    // [反映 (Ctrl+Enter)] ボタン
    DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 24.0f), L"反映 (Ctrl+Enter)", false, false, true);
    curY += 32.0f;

    // 区切り線
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + 6.0f, curY), D2D1::Point2F(x + width - 6.0f, curY), m_pBrushBorder, 1.0f);
    curY += 8.0f;

    // 2. ダイアトニックコードパレット
    m_pRenderTarget->DrawText(
        L"ダイアトニックコード", 10, m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 18.0f),
        m_pBrushTextPrimary
    );
    curY += 20.0f;

    // 3和音 / 7th 切替スイッチ
    DrawButton(D2D1::RectF(x + padding, curY, x + padding + 60.0f, curY + 22.0f), L"3和音", false, false, !m_show7thDiatonic);
    DrawButton(D2D1::RectF(x + padding + 64.0f, curY, x + padding + 130.0f, curY + 22.0f), L"7th和音", false, false, m_show7thDiatonic);
    curY += 28.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    auto diatonics = MusicEngine::GetDiatonicChords(key, mode, m_show7thDiatonic);
    float cardW = (width - padding * 2 - 3 * 5.0f) / 4.0f;
    float cardH = 44.0f;

    for (size_t i = 0; i < diatonics.size(); ++i) {
        float cx = x + padding + (i % 4) * (cardW + 5.0f);
        float cy = curY + (i / 4) * (cardH + 5.0f);
        std::wstring nameW = Utf8ToWString(diatonics[i].rootName) + Utf8ToWString(diatonics[i].qualitySuffix);
        std::wstring degW = Utf8ToWString(diatonics[i].degreeRoman);
        DrawCard(D2D1::RectF(cx, cy, cx + cardW, cy + cardH), nameW, degW, false, false, false);
    }

    // 休符 (Rest / N.C.) カード
    float restX = x + padding + 3 * (cardW + 5.0f);
    float restY = curY + 1 * (cardH + 5.0f);
    DrawCard(D2D1::RectF(restX, restY, restX + cardW, restY + cardH), L"Rest", L"N.C.", false, false, true);

    curY += (cardH + 5.0f) * 2 + 8.0f;

    // 区切り線
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + 6.0f, curY), D2D1::Point2F(x + width - 6.0f, curY), m_pBrushBorder, 1.0f);
    curY += 8.0f;

    // 3. スケール外・カスタムコードビルダー
    m_pRenderTarget->DrawText(
        L"スケール外・カスタムコード", 13, m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 18.0f),
        m_pBrushTextPrimary
    );
    curY += 20.0f;

    // 12音ルート
    static const wchar_t* kRoots[12] = { L"C", L"C#", L"D", L"Eb", L"E", L"F", L"F#", L"G", L"Ab", L"A", L"Bb", L"B" };
    float rBtnW = (width - padding * 2 - 5 * 4.0f) / 6.0f;
    for (int i = 0; i < 12; ++i) {
        float bx = x + padding + (i % 6) * (rBtnW + 4.0f);
        float by = curY + (i / 6) * 24.0f;
        DrawButton(D2D1::RectF(bx, by, bx + rBtnW, by + 20.0f), kRoots[i], false, false, m_builderRoot == i);
    }
    curY += 52.0f;

    // クオリティ
    static const wchar_t* kQualities[8] = { L"Maj", L"m", L"7", L"maj7", L"m7", L"m7b5", L"dim7", L"sus4" };
    float qBtnW = (width - padding * 2 - 3 * 4.0f) / 4.0f;
    for (int i = 0; i < 8; ++i) {
        float bx = x + padding + (i % 4) * (qBtnW + 4.0f);
        float by = curY + (i / 4) * 24.0f;
        DrawButton(D2D1::RectF(bx, by, bx + qBtnW, by + 20.0f), kQualities[i], false, false, m_builderQuality == i);
    }
    curY += 54.0f;

    // プレビューカード & [追加] ボタン
    PitchClass pRoot = static_cast<PitchClass>(m_builderRoot);
    static const ChordQuality kQEnums[8] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Diminished7, ChordQuality::Sus4
    };
    ChordQuality qEnum = kQEnums[m_builderQuality % 8];
    std::string bChordName = MusicEngine::PitchToNote(pRoot, false) + ((qEnum == ChordQuality::Minor) ? "m" : (qEnum == ChordQuality::Minor7 ? "m7" : (qEnum == ChordQuality::Major7 ? "maj7" : (qEnum == ChordQuality::Dominant7 ? "7" : (qEnum == ChordQuality::HalfDiminished7 ? "m7b5" : "")))));
    std::string bDeg = MusicEngine::CalculateDegree(pRoot, qEnum, MusicEngine::NoteToPitch(key), mode);

    DrawCard(
        D2D1::RectF(x + padding, curY, x + padding + 80.0f, curY + 44.0f),
        Utf8ToWString(bChordName),
        Utf8ToWString(bDeg),
        false, false, false
    );

    DrawButton(
        D2D1::RectF(x + padding + 86.0f, curY + 6.0f, x + width - padding, curY + 38.0f),
        L"このコードを追加", false, false, true
    );
}

void AppWindow::RenderCenterPane(float x, float y, float width, float height) {
    // 1. パートタブバー (H = 30px)
    float tabH = 30.0f;
    D2D1_RECT_F tabRect = D2D1::RectF(x, y, x + width, y + tabH);
    m_pRenderTarget->FillRectangle(tabRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + tabH), D2D1::Point2F(x + width, y + tabH), m_pBrushBorder, 1.0f);

    float tx = x + 8.0f;
    DrawButton(
        D2D1::RectF(tx, y + 3.0f, tx + 130.0f, y + tabH - 3.0f),
        L"★ 全体マージ (Full)", false, false, m_isMergedView
    );
    tx += 136.0f;

    const auto& parts = m_partManager.GetParts();
    for (size_t i = 0; i < parts.size(); ++i) {
        std::wstring pTitle = Utf8ToWString(parts[i].name) + L" (" + Utf8ToWString(parts[i].keyRoot) + L")";
        bool isAct = (!m_isMergedView && m_partManager.GetActiveIndex() == static_cast<int>(i));
        DrawButton(D2D1::RectF(tx, y + 3.0f, tx + 94.0f, y + tabH - 3.0f), pTitle, false, false, isAct);
        tx += 98.0f;
    }
    DrawButton(D2D1::RectF(tx, y + 3.0f, tx + 80.0f, y + tabH - 3.0f), L"+ パート", false, false);

    // 2. DAWタイムルーラー (H = 24px)
    float rulerY = y + tabH;
    float rulerH = 24.0f;
    D2D1_RECT_F rulerRect = D2D1::RectF(x, rulerY, x + width, rulerY + rulerH);
    m_pRenderTarget->FillRectangle(rulerRect, m_pBrushBackground);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, rulerY + rulerH), D2D1::Point2F(x + width, rulerY + rulerH), m_pBrushBorder, 1.0f);

    float barW = m_pixelsPerBeat * 4.0f;
    int totalBars = 32;

    for (int b = 0; b < totalBars; ++b) {
        float bx = x + b * barW - m_timelineScrollX;
        if (bx + barW < x || bx > x + width) continue;

        // 小節ヘッダー
        m_pRenderTarget->DrawLine(D2D1::Point2F(bx, rulerY), D2D1::Point2F(bx, rulerY + rulerH), m_pBrushBorder, 1.5f);
        std::wstring barNumStr = std::to_wstring(b + 1);
        m_pRenderTarget->DrawText(
            barNumStr.c_str(), static_cast<UINT32>(barNumStr.size()),
            m_pFontSmall,
            D2D1::RectF(bx + 4.0f, rulerY + 3.0f, bx + 30.0f, rulerY + rulerH),
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

    // 3. タイムラインクリップトラック (H = 100px)
    float trackY = rulerY + rulerH;
    float trackH = 100.0f;

    // グリッド線
    for (int b = 0; b < totalBars; ++b) {
        float bx = x + b * barW - m_timelineScrollX;
        if (bx + barW < x || bx > x + width) continue;
        m_pRenderTarget->DrawLine(D2D1::Point2F(bx, trackY), D2D1::Point2F(bx, trackY + trackH), m_pBrushBorder, 0.8f);
    }

    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});

    for (size_t bi = 0; bi < currentBars.size(); ++bi) {
        float bx = x + bi * barW - m_timelineScrollX;
        if (bx + barW < x || bx > x + width) continue;

        // 転調マーカー
        if (!currentBars[bi].keyChange.empty()) {
            std::wstring kcStr = L"🔑 " + Utf8ToWString(currentBars[bi].keyChange);
            m_pRenderTarget->DrawText(
                kcStr.c_str(), static_cast<UINT32>(kcStr.size()),
                m_pFontTiny,
                D2D1::RectF(bx + 4.0f, trackY + 2.0f, bx + barW, trackY + 16.0f),
                m_pBrushAccent
            );
        }

        // コードカード
        float curCardX = bx + 3.0f;
        for (size_t ci = 0; ci < currentBars[bi].chords.size(); ++ci) {
            const auto& chord = currentBars[bi].chords[ci];
            float cardSlotW = m_pixelsPerBeat * chord.durationBeats - 4.0f;

            std::wstring cTitle = chord.isRest ? L"Rest" : (Utf8ToWString(chord.rootName) + Utf8ToWString(chord.qualitySuffix));
            std::wstring cDeg = Utf8ToWString(chord.degreeRoman);

            bool isSel = (m_selection.startBar <= static_cast<int>(bi) && static_cast<int>(bi) <= m_selection.endBar);
            D2D1_RECT_F cRect = D2D1::RectF(curCardX, trackY + 18.0f, curCardX + cardSlotW, trackY + 92.0f);
            DrawCard(cRect, cTitle, cDeg, false, isSel, chord.isRest);

            // 削除 '×' ボタン
            D2D1_RECT_F closeBtnRect = D2D1::RectF(cRect.right - 14.0f, cRect.top + 2.0f, cRect.right - 2.0f, cRect.top + 14.0f);
            m_pRenderTarget->DrawText(L"×", 1, m_pFontTiny, closeBtnRect, m_pBrushTextSecondary);

            curCardX += m_pixelsPerBeat * chord.durationBeats;
        }
    }

    // 4. ピアノロール (構成音ノートバー可視化) (H = 150px)
    float rollY = trackY + trackH;
    float rollH = 150.0f;
    RenderPianoRoll(x, rollY, width, rollH, barW);

    // 5. リアル61鍵盤 (H = 75px)
    float keyY = rollY + rollH;
    float keyH = 75.0f;
    RenderPianoKeyboard(x, keyY, width, keyH);

    // 6. インスペクターバー (H = 30px)
    float inspY = keyY + keyH;
    float inspH = height - (inspY - y);
    RenderInspectorBar(x, inspY, width, inspH);

    // プレイヘッド描画 (ルーラー〜鍵盤まで貫く赤い縦棒)
    float playheadX = x + (m_playbackBeat / 4.0f) * barW - m_timelineScrollX;
    if (playheadX >= x && playheadX <= x + width) {
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(playheadX, rulerY),
            D2D1::Point2F(playheadX, keyY + keyH),
            m_pBrushPlayhead, 2.0f
        );
    }
}

void AppWindow::RenderPianoRoll(float x, float y, float width, float height, float barWidth) {
    D2D1_RECT_F rollRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(rollRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    // 音高レーン (C3=48 〜 C6=84 の 36半音)
    int minPitch = 48;
    int maxPitch = 84;
    int totalPitches = maxPitch - minPitch;
    float laneH = height / totalPitches;

    // 背景レーン線
    for (int p = 0; p < totalPitches; ++p) {
        float ly = y + height - (p + 1) * laneH;
        int pitch = minPitch + p;
        int semitone = pitch % 12;
        bool isBlack = (semitone == 1 || semitone == 3 || semitone == 6 || semitone == 8 || semitone == 10);
        if (isBlack) {
            D2D1_RECT_F blackLane = D2D1::RectF(x, ly, x + width, ly + laneH);
            m_pRenderTarget->FillRectangle(blackLane, m_pBrushCardHover);
        }
        m_pRenderTarget->DrawLine(D2D1::Point2F(x, ly), D2D1::Point2F(x + width, ly), m_pBrushBorder, 0.5f);
    }

    // タイムラインの各コードに対応するノートバーを描画
    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});

    float curBeat = 0.0f;
    for (const auto& b : currentBars) {
        for (const auto& c : b.chords) {
            if (!c.isRest && c.rootPitch != PitchClass::Unknown) {
                auto notes = MusicEngine::GetChordMidiNotes(c, 4, static_cast<VoicingStyle>(m_selectedVoicingIndex));
                float noteStartX = x + (curBeat / 4.0f) * barWidth - m_timelineScrollX;
                float noteW = m_pixelsPerBeat * c.durationBeats - 2.0f;

                bool isActiveChord = (m_isPlaying && m_playbackBeat >= curBeat && m_playbackBeat < curBeat + c.durationBeats);

                for (int n : notes) {
                    if (n >= minPitch && n <= maxPitch) {
                        float ny = y + height - (n - minPitch + 1) * laneH;
                        D2D1_RECT_F nRect = D2D1::RectF(noteStartX, ny + 1.0f, noteStartX + noteW, ny + laneH - 1.0f);
                        D2D1_ROUNDED_RECT nRound = D2D1::RoundedRect(nRect, 2.0f, 2.0f);
                        m_pRenderTarget->FillRoundedRectangle(nRound, isActiveChord ? m_pBrushNoteActive : m_pBrushNoteBar);
                    }
                }
            }
            curBeat += c.durationBeats;
        }
    }
}

void AppWindow::RenderPianoKeyboard(float x, float y, float width, float height) {
    D2D1_RECT_F kbRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(kbRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    // 61鍵盤 (C2=36 から C7=96: 白鍵36鍵、黒鍵25鍵)
    int numWhiteKeys = 36;
    float whiteKeyW = width / numWhiteKeys;

    // 1. 白鍵描画
    int whiteIndex = 0;
    float whiteKeyPositions[61];

    for (int midi = 36; midi <= 96; ++midi) {
        int s = midi % 12;
        bool isBlack = (s == 1 || s == 3 || s == 6 || s == 8 || s == 10);
        if (!isBlack) {
            float kx = x + whiteIndex * whiteKeyW;
            whiteKeyPositions[midi - 36] = kx;
            D2D1_RECT_F wk = D2D1::RectF(kx, y, kx + whiteKeyW, y + height);

            bool isSounding = (std::find(m_currentlySoundingNotes.begin(), m_currentlySoundingNotes.end(), midi) != m_currentlySoundingNotes.end());
            m_pRenderTarget->FillRectangle(wk, isSounding ? m_pBrushNoteActive : m_pBrushCardBg);
            m_pRenderTarget->DrawRectangle(wk, m_pBrushBorder, 1.0f);
            whiteIndex++;
        }
    }

    // 2. 黒鍵描画 (白鍵の上に重ねて描画)
    float blackKeyW = whiteKeyW * 0.65f;
    float blackKeyH = height * 0.62f;

    for (int midi = 36; midi <= 96; ++midi) {
        int s = midi % 12;
        bool isBlack = (s == 1 || s == 3 || s == 6 || s == 8 || s == 10);
        if (isBlack && midi > 36) {
            float prevWhiteX = whiteKeyPositions[midi - 36 - 1];
            float kx = prevWhiteX + whiteKeyW - (blackKeyW * 0.5f);
            D2D1_RECT_F bk = D2D1::RectF(kx, y, kx + blackKeyW, y + blackKeyH);

            bool isSounding = (std::find(m_currentlySoundingNotes.begin(), m_currentlySoundingNotes.end(), midi) != m_currentlySoundingNotes.end());
            m_pRenderTarget->FillRectangle(bk, isSounding ? m_pBrushNoteActive : m_pBrushBlackKey);
            m_pRenderTarget->DrawRectangle(bk, m_pBrushBorder, 0.8f);
        }
    }
}

void AppWindow::RenderInspectorBar(float x, float y, float width, float height) {
    D2D1_RECT_F barRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(barRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    float curX = x + 10.0f;
    float ty = y + 7.0f;

    // 幅を85pxに拡大し改行を防止
    m_pRenderTarget->DrawText(L"INSPECTOR:", 10, m_pFontHeader, D2D1::RectF(curX, ty, curX + 85.0f, ty + 16.0f), m_pBrushTextSecondary);
    curX += 90.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::wstring symb = L"-";
    std::wstring dur = L"-";
    std::wstring notesStr = L"-";

    if (pActive && !pActive->bars.empty()) {
        int barIdx = (m_selectedCardIndex >= 0) ? m_selectedCardIndex : 0;
        if (barIdx < static_cast<int>(pActive->bars.size()) && !pActive->bars[barIdx].chords.empty()) {
            const auto& c = pActive->bars[barIdx].chords[0];
            symb = Utf8ToWString(c.rootName) + Utf8ToWString(c.qualitySuffix) + L" (" + Utf8ToWString(c.degreeRoman) + L")";
            dur = std::to_wstring(c.durationBeats) + L"拍";
            auto notes = MusicEngine::GetChordMidiNotes(c, 4, static_cast<VoicingStyle>(m_selectedVoicingIndex));
            notesStr.clear();
            for (size_t i = 0; i < notes.size(); ++i) {
                int n = notes[i];
                std::string pStr = MusicEngine::PitchToNote(static_cast<PitchClass>(n % 12), false);
                notesStr += Utf8ToWString(pStr) + std::to_wstring(n / 12 - 1);
                if (i + 1 < notes.size()) notesStr += L" ";
            }
        }
    }

    std::wstring chordLabel = L"コード: " + symb;
    m_pRenderTarget->DrawText(chordLabel.c_str(), static_cast<UINT32>(chordLabel.size()), m_pFontNormal, D2D1::RectF(curX, ty, curX + 110.0f, ty + 16.0f), m_pBrushTextPrimary);
    curX += 116.0f;

    std::wstring durLabel = L"長さ: " + dur;
    m_pRenderTarget->DrawText(durLabel.c_str(), static_cast<UINT32>(durLabel.size()), m_pFontNormal, D2D1::RectF(curX, ty, curX + 66.0f, ty + 16.0f), m_pBrushTextPrimary);
    curX += 72.0f;

    std::wstring notesLabel = L"構成音: " + notesStr;
    m_pRenderTarget->DrawText(notesLabel.c_str(), static_cast<UINT32>(notesLabel.size()), m_pFontNormal, D2D1::RectF(curX, ty, curX + 260.0f, ty + 16.0f), m_pBrushAccent);
    curX += 266.0f;

    // 右端: ズームコントロール
    float zX = x + width - 130.0f;
    m_pRenderTarget->DrawText(L"ズーム:", 4, m_pFontSmall, D2D1::RectF(zX, ty, zX + 36.0f, ty + 16.0f), m_pBrushTextSecondary);
    DrawButton(D2D1::RectF(zX + 40.0f, y + 4.0f, zX + 62.0f, y + height - 4.0f), L"-", false, false);
    DrawButton(D2D1::RectF(zX + 66.0f, y + 4.0f, zX + 88.0f, y + height - 4.0f), L"+", false, false);
}

void AppWindow::RenderRightPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x, y + height), m_pBrushBorder, 1.0f);

    float curY = y + 8.0f - m_rightPaneScrollY;
    float padding = 8.0f;

    m_pRenderTarget->DrawText(
        L"音楽理論・提案パレット", 11, m_pFontHeader,
        D2D1::RectF(x + padding, curY, x + width - padding, curY + 18.0f),
        m_pBrushTextPrimary
    );
    curY += 22.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    // 1. SUGGESTIONS (次候補)
    m_pRenderTarget->DrawText(L"SUGGESTIONS (次候補)", 16, m_pFontSmall, D2D1::RectF(x + padding, curY, x + width - padding, curY + 14.0f), m_pBrushAccent);
    curY += 16.0f;

    std::vector<ChordToken> currentChordsList;
    if (pActive) {
        for (const auto& b : pActive->bars) {
            for (const auto& c : b.chords) currentChordsList.push_back(c);
        }
    }
    auto suggestions = MusicEngine::GetSuggestions(currentChordsList, key, mode);
    float badgeW = (width - padding * 2 - 3 * 6.0f) / 4.0f;
    for (size_t i = 0; i < suggestions.size() && i < 4; ++i) {
        float bx = x + padding + i * (badgeW + 6.0f);
        std::wstring sSym = Utf8ToWString(suggestions[i].rootName) + Utf8ToWString(suggestions[i].qualitySuffix);
        std::wstring sDeg = Utf8ToWString(suggestions[i].degreeRoman);
        DrawRoundBadge(D2D1::RectF(bx, curY, bx + badgeW, curY + 38.0f), sSym, sDeg, false, false);
    }
    curY += 44.0f;

    // 2. 借用和音 (同主調)
    m_pRenderTarget->DrawText(L"借用和音 (同主調)", 9, m_pFontSmall, D2D1::RectF(x + padding, curY, x + width - padding, curY + 14.0f), m_pBrushAccent);
    curY += 16.0f;

    auto miChords = MusicEngine::GetModalInterchangeChords(key, mode);
    float miW = (width - padding * 2 - 2 * 6.0f) / 3.0f;
    float miH = 40.0f;
    for (size_t i = 0; i < miChords.size(); ++i) {
        float cx = x + padding + (i % 3) * (miW + 6.0f);
        float cy = curY + (i / 3) * (miH + 5.0f);
        std::wstring nW = Utf8ToWString(miChords[i].rootName) + Utf8ToWString(miChords[i].qualitySuffix);
        std::wstring dW = Utf8ToWString(miChords[i].degreeRoman);
        DrawCard(D2D1::RectF(cx, cy, cx + miW, cy + miH), nW, dW, false, false, false);
    }
    curY += (miH + 5.0f) * 3 + 6.0f;

    // 3. セカンダリードミナント
    m_pRenderTarget->DrawText(L"セカンダリードミナント", 11, m_pFontSmall, D2D1::RectF(x + padding, curY, x + width - padding, curY + 14.0f), m_pBrushAccent);
    curY += 16.0f;

    auto secDoms = MusicEngine::GetSecondaryDominants(key, mode);
    for (size_t i = 0; i < secDoms.size(); ++i) {
        float cx = x + padding + (i % 3) * (miW + 6.0f);
        float cy = curY + (i / 3) * (miH + 5.0f);
        std::wstring nW = Utf8ToWString(secDoms[i].rootName) + L"7";
        std::wstring dW = Utf8ToWString(secDoms[i].degreeRoman);
        DrawCard(D2D1::RectF(cx, cy, cx + miW, cy + miH), nW, dW, false, false, false);
    }
    curY += (miH + 5.0f) * 2 + 6.0f;

    // 4. 定番進行テンプレート (UTF-8正常変換)
    m_pRenderTarget->DrawText(L"定番進行テンプレート", 10, m_pFontSmall, D2D1::RectF(x + padding, curY, x + width - padding, curY + 14.0f), m_pBrushAccent);
    curY += 16.0f;

    auto templates = MusicEngine::GetBuiltinTemplates();
    for (const auto& tmpl : templates) {
        std::wstring tName = Utf8ToWString(tmpl.name);
        DrawButton(D2D1::RectF(x + padding, curY, x + width - padding, curY + 24.0f), tName, false, false, false);
        curY += 28.0f;
    }

    // 単一スリムスクロールバー (右端・控えめな厚さ)
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(x + width - 2.0f, y + 2.0f),
        D2D1::Point2F(x + width - 2.0f, y + height - 2.0f),
        m_pBrushBorder, 2.0f
    );
}

void AppWindow::RenderStatusBar(float x, float y, float width, float height) {
    D2D1_RECT_F barRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(barRect, m_pBrushPaneBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    D2D1_RECT_F textRect = D2D1::RectF(x + 10.0f, y + 4.0f, x + width - 10.0f, y + height);
    m_pRenderTarget->DrawText(
        m_statusMessage.c_str(), static_cast<UINT32>(m_statusMessage.size()),
        m_pFontSmall, textRect, m_pBrushTextSecondary
    );
}

void AppWindow::StartPlayback() {
    m_isPlaying = true;
    m_lastPlaybackTime = std::chrono::high_resolution_clock::now();
    m_lastSoundedBeat = -1;
    m_statusMessage = L"再生中...";
}

void AppWindow::StopPlayback() {
    m_isPlaying = false;
    m_synth.AllNotesOff();
    m_currentlySoundingNotes.clear();
    m_lastSoundedBeat = -1;
    m_statusMessage = L"停止";
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

        int accBeats = 0;
        for (size_t bi = 0; bi < currentBars.size(); ++bi) {
            for (const auto& c : currentBars[bi].chords) {
                if (currentBeatInt >= accBeats && currentBeatInt < accBeats + c.durationBeats) {
                    if (!c.isRest && c.rootPitch != PitchClass::Unknown) {
                        auto notes = MusicEngine::GetChordMidiNotes(c, 4, static_cast<VoicingStyle>(m_selectedVoicingIndex));
                        m_currentlySoundingNotes = notes;
                        m_synth.PlayChord(notes, 0.8f);
                    } else {
                        m_currentlySoundingNotes.clear();
                        m_synth.AllNotesOff();
                    }
                    m_statusMessage = L"再生中: 小節 " + std::to_wstring(bi + 1) + L", 拍 " + std::to_wstring((currentBeatInt % 4) + 1);
                    return;
                }
                accBeats += c.durationBeats;
            }
        }
    }
}

void AppWindow::ProcessTapTempo() {
    auto now = std::chrono::high_resolution_clock::now();

    // 最後のタップから2秒以上空いていたらリセット
    if (!m_tapTimes.empty()) {
        float elapsedSinceLast = std::chrono::duration<float>(now - m_tapTimes.back()).count();
        if (elapsedSinceLast > 2.0f) {
            m_tapTimes.clear();
        }
    }

    m_tapTimes.push_back(now);
    if (m_tapTimes.size() > 5) {
        m_tapTimes.erase(m_tapTimes.begin());
    }

    if (m_tapTimes.size() >= 2) {
        float totalSec = 0.0f;
        for (size_t i = 1; i < m_tapTimes.size(); ++i) {
            totalSec += std::chrono::duration<float>(m_tapTimes[i] - m_tapTimes[i - 1]).count();
        }
        float avgInterval = totalSec / static_cast<float>(m_tapTimes.size() - 1);
        if (avgInterval >= 0.15f && avgInterval <= 2.0f) {
            int newBpm = static_cast<int>(std::round(60.0f / avgInterval));
            newBpm = std::clamp(newBpm, 40, 300);
            Part* pActive = m_partManager.GetActivePart();
            if (pActive) {
                pActive->bpm = newBpm;
            }
            m_statusMessage = L"タップテンポ設定: " + std::to_wstring(newBpm) + L" BPM (平均間隔: " +
                              std::to_wstring(static_cast<int>(avgInterval * 1000.0f)) + L" ms)";
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    } else {
        m_statusMessage = L"タップテンポ: もう一度タップしてください...";
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::EnsureVisible(int barIndex) {
    float barW = m_pixelsPerBeat * 4.0f;
    float barX = barIndex * barW;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float centerW = static_cast<float>(rc.right - rc.left) - 540.0f;

    if (barX < m_timelineScrollX) {
        m_timelineScrollX = barX - 20.0f;
    } else if (barX + barW > m_timelineScrollX + centerW) {
        m_timelineScrollX = barX + barW - centerW + 40.0f;
    }
    if (m_timelineScrollX < 0.0f) m_timelineScrollX = 0.0f;
}

void AppWindow::SyncTimelineToText() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || !m_hwndInputEdit) return;

    std::string formatted = ProgressionParser::Format(pActive->bars);
    std::wstring wFormatted = Utf8ToWString(formatted);
    SetWindowTextW(m_hwndInputEdit, wFormatted.c_str());
}

void AppWindow::ApplyTextToTimeline() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || !m_hwndInputEdit) return;

    int len = GetWindowTextLengthW(m_hwndInputEdit);
    std::wstring wText(len + 1, L'\0');
    GetWindowTextW(m_hwndInputEdit, &wText[0], len + 1);
    wText.resize(len);

    std::string text = WStringToUtf8(wText);
    pActive->bars = ProgressionParser::Parse(text, pActive->keyRoot, pActive->mode);
    pActive->rawProgressionText = text;

    m_statusMessage = L"テキスト入力をタイムラインに反映しました";
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::AddChordToTimeline(const ChordToken& chord) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    Bar b;
    b.barNumber = static_cast<int>(pActive->bars.size()) + 1;
    b.chords.push_back(chord);
    pActive->bars.push_back(b);

    SyncTimelineToText();
    EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
    m_statusMessage = L"コードを追加しました: " + Utf8ToWString(chord.rootName) + Utf8ToWString(chord.qualitySuffix);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ApplyTemplate(const MusicEngine::TemplateInfo& tmpl) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    pActive->bars = ProgressionParser::Parse(tmpl.progression, pActive->keyRoot, pActive->mode);
    pActive->rawProgressionText = tmpl.progression;

    SyncTimelineToText();
    m_statusMessage = L"テンプレートを適用しました: " + Utf8ToWString(tmpl.name);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ShowKeyMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    static const wchar_t* kKeys[12] = { L"C", L"C#", L"D", L"Eb", L"E", L"F", L"F#", L"G", L"Ab", L"A", L"Bb", L"B" };
    for (int i = 0; i < 12; ++i) {
        AppendMenuW(hMenu, MF_STRING, i + 1, kKeys[i]);
    }
    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected >= 1 && selected <= 12) {
        Part* pActive = m_partManager.GetActivePart();
        if (pActive) {
            std::string oldKey = pActive->keyRoot;
            std::string newKey = WStringToUtf8(kKeys[selected - 1]);
            pActive->keyRoot = newKey;
            pActive->bars = ProgressionParser::Parse(pActive->rawProgressionText, pActive->keyRoot, pActive->mode);
            SyncTimelineToText();
            m_statusMessage = L"キーを変更しました: " + Utf8ToWString(newKey);
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
}

void AppWindow::ShowModeMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"メジャー (Major / Ionian)");
    AppendMenuW(hMenu, MF_STRING, 2, L"ナチュラルマイナー (Natural Minor / Aeolian)");
    AppendMenuW(hMenu, MF_STRING, 3, L"ハーモニックマイナー (Harmonic Minor)");
    AppendMenuW(hMenu, MF_STRING, 4, L"ドリアン (Dorian)");
    AppendMenuW(hMenu, MF_STRING, 5, L"ミクソリディアン (Mixolydian)");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected >= 1 && selected <= 5) {
        Part* pActive = m_partManager.GetActivePart();
        if (pActive) {
            static const MusicalMode kModes[] = { MusicalMode::Major, MusicalMode::NaturalMinor, MusicalMode::HarmonicMinor, MusicalMode::Dorian, MusicalMode::Mixolydian };
            pActive->mode = kModes[selected - 1];
            pActive->bars = ProgressionParser::Parse(pActive->rawProgressionText, pActive->keyRoot, pActive->mode);
            SyncTimelineToText();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
}

void AppWindow::ShowToneMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"グランドピアノ (Piano)");
    AppendMenuW(hMenu, MF_STRING, 2, L"ウォームパッド (Pad)");
    AppendMenuW(hMenu, MF_STRING, 3, L"エレピ (E-Piano)");
    AppendMenuW(hMenu, MF_STRING, 4, L"ストリングス (Strings)");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected >= 1 && selected <= 4) {
        m_selectedSoundIndex = selected - 1;
        m_synth.SetPreset(static_cast<SynthPreset>(m_selectedSoundIndex));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::ShowBassMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"なし (None)");
    AppendMenuW(hMenu, MF_STRING, 2, L"低音 1 (1音付加)");
    AppendMenuW(hMenu, MF_STRING, 3, L"低音 2 (オクターブ下2音)");
    AppendMenuW(hMenu, MF_STRING, 4, L"両方 (Both)");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected >= 1 && selected <= 4) {
        m_selectedBassIndex = selected - 1;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::ShowVoicingMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"基本形 (Root Position)");
    AppendMenuW(hMenu, MF_STRING, 2, L"ドロップ2 (Drop 2)");
    AppendMenuW(hMenu, MF_STRING, 3, L"ドロップ3 (Drop 3)");
    AppendMenuW(hMenu, MF_STRING, 4, L"オープン (Open Spread)");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected >= 1 && selected <= 4) {
        m_selectedVoicingIndex = selected - 1;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::ShowGridMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1, L"1/16 拍");
    AppendMenuW(hMenu, MF_STRING, 2, L"1/8 拍");
    AppendMenuW(hMenu, MF_STRING, 3, L"1 拍 (標準)");
    AppendMenuW(hMenu, MF_STRING, 4, L"2 拍");
    AppendMenuW(hMenu, MF_STRING, 5, L"1 小節");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (selected == 1) m_selectedGridBeats = 0.25f;
    else if (selected == 2) m_selectedGridBeats = 0.5f;
    else if (selected == 3) m_selectedGridBeats = 1.0f;
    else if (selected == 4) m_selectedGridBeats = 2.0f;
    else if (selected == 5) m_selectedGridBeats = 4.0f;

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ShowSettingsMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hSubLang = CreatePopupMenu();

    AppendMenuW(hSubLang, (I18n::GetLanguage() == Language::Japanese ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, 101, L"日本語 (Japanese)");
    AppendMenuW(hSubLang, (I18n::GetLanguage() == Language::English ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, 102, L"English");

    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubLang), L"言語設定 (Language)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 201, L"ChordLaunchpad について (About)");

    int selected = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hSubLang);
    DestroyMenu(hMenu);

    if (selected == 101) {
        I18n::SetLanguage(Language::Japanese);
        m_statusMessage = L"言語を日本語に設定しました";
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else if (selected == 102) {
        I18n::SetLanguage(Language::English);
        m_statusMessage = L"Language set to English";
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else if (selected == 201) {
        MessageBoxW(
            m_hwnd,
            L"Chord Launchpad\n\n高速・超軽量・モダン音楽制作アシスタント\nC++20 / Direct2D / WASAPI / OLE Drag&Drop\n\n(c) 2026",
            L"About Chord Launchpad",
            MB_OK | MB_ICONINFORMATION
        );
    }
}

void AppWindow::OnLButtonDown(int x, int y, [[maybe_unused]] WPARAM wParam) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);

    // Row 1 ツールバークリック
    if (y < 34) {
        if (x >= 10 && x <= 54) {
            // 新規
            m_partManager.ResetToDefaultSong();
            SyncTimelineToText();
            m_statusMessage = L"新規プロジェクトを作成しました";
        } else if (x >= 58 && x <= 102) {
            // 開く
            m_statusMessage = L"プロジェクトを開く";
        } else if (x >= 106 && x <= 154) {
            // 保存
            m_statusMessage = L"プロジェクトを保存しました";
        } else if (x >= 160 && x <= 188) {
            // Undo
            m_statusMessage = L"元に戻す";
        } else if (x >= 192 && x <= 220) {
            // Redo
            m_statusMessage = L"やり直し";
        } else if (x >= 234 && x <= 302) {
            // 再生
            TogglePlayback();
        } else if (x >= 306 && x <= 360) {
            // 停止
            StopPlayback();
        } else if (x >= 364 && x <= 418) {
            // ループ
            m_isLooping = !m_isLooping;
        } else if (x >= 426 && x <= 500) {
            // MIDI保存
            std::vector<Bar> currentBars = m_isMergedView ?
                m_partManager.GenerateMergedProgression() :
                (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});
            Part* pAct = m_partManager.GetActivePart();
            int actBpm = pAct ? pAct->bpm : 120;
            std::wstring tempMidi = MidiBuilder::ExportToTempFile(currentBars, actBpm);
            if (!tempMidi.empty()) {
                m_statusMessage = L"MIDIファイルを保存しました: " + tempMidi;
            }
        } else if (x >= 504 && x <= 634) {
            // DAWドラッグ
            std::vector<Bar> currentBars = m_isMergedView ?
                m_partManager.GenerateMergedProgression() :
                (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});
            Part* pAct = m_partManager.GetActivePart();
            int actBpm = pAct ? pAct->bpm : 120;
            std::wstring tempMidi = MidiBuilder::ExportToTempFile(currentBars, actBpm);
            if (!tempMidi.empty()) {
                OleFileDataObject::StartFileDrag(m_hwnd, tempMidi);
            }
        } else if (x >= width - 70 && x <= width - 10) {
            // 設定 ⚙
            POINT pt = {x, y};
            ClientToScreen(m_hwnd, &pt);
            ShowSettingsMenu(pt.x, pt.y);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // Row 2 ツールバークリック
    if (y >= 34 && y < 68) {
        POINT pt = {x, y};
        ClientToScreen(m_hwnd, &pt);

        if (x >= 10 && x <= 80) {
            ShowKeyMenu(pt.x, pt.y);
        } else if (x >= 86 && x <= 190) {
            ShowModeMenu(pt.x, pt.y);
        } else if (x >= 196 && x <= 262) {
            // BPM クリックで Tap
            ProcessTapTempo();
        } else if (x >= 264 && x <= 286) {
            // BPM -
            Part* pActive = m_partManager.GetActivePart();
            if (pActive && pActive->bpm > 40) pActive->bpm -= 5;
            m_statusMessage = L"BPM: " + std::to_wstring(pActive ? pActive->bpm : 120);
        } else if (x >= 288 && x <= 310) {
            // BPM +
            Part* pActive = m_partManager.GetActivePart();
            if (pActive && pActive->bpm < 300) pActive->bpm += 5;
            m_statusMessage = L"BPM: " + std::to_wstring(pActive ? pActive->bpm : 120);
        } else if (x >= 314 && x <= 364) {
            // Tap
            ProcessTapTempo();
        } else if (x >= 370 && x <= 460) {
            ShowToneMenu(pt.x, pt.y);
        } else if (x >= 466 && x <= 550) {
            ShowBassMenu(pt.x, pt.y);
        } else if (x >= 556 && x <= 656) {
            ShowVoicingMenu(pt.x, pt.y);
        } else if (x >= 662 && x <= 758) {
            ShowGridMenu(pt.x, pt.y);
        } else if (x >= 768 && x <= 812) {
            DuplicateSelection();
        } else if (x >= 816 && x <= 860) {
            DeleteSelection();
        } else if (x >= 866 && x <= 930) {
            // 全クリア
            Part* pActive = m_partManager.GetActivePart();
            if (pActive) {
                pActive->bars.clear();
                SyncTimelineToText();
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // 左ペイン: [反映 (Ctrl+Enter)] ボタン
    if (x >= 10 && x <= 270 && y >= 184 && y <= 208) {
        ApplyTextToTimeline();
        return;
    }

    // 左ペイン: 3和音 / 7th 切替
    if (x >= 10 && x <= 70 && y >= 238 && y <= 260) {
        m_show7thDiatonic = false;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (x >= 74 && x <= 140 && y >= 238 && y <= 260) {
        m_show7thDiatonic = true;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // 左ペイン: スケール外 12音ルート選択
    if (x >= 10 && x <= 270 && y >= 408 && y <= 458) {
        float rBtnW = (260.0f - 5 * 4.0f) / 6.0f;
        int col = static_cast<int>((x - 10) / (rBtnW + 4.0f));
        int row = static_cast<int>((y - 408) / 24.0f);
        int idx = row * 6 + col;
        if (idx >= 0 && idx < 12) {
            m_builderRoot = idx;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    // 左ペイン: スケール外 クオリティ選択
    if (x >= 10 && x <= 270 && y >= 460 && y <= 512) {
        float qBtnW = (260.0f - 3 * 4.0f) / 4.0f;
        int col = static_cast<int>((x - 10) / (qBtnW + 4.0f));
        int row = static_cast<int>((y - 460) / 24.0f);
        int idx = row * 4 + col;
        if (idx >= 0 && idx < 8) {
            m_builderQuality = idx;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    // 左ペイン: [このコードを追加]
    if (x >= 96 && x <= 270 && y >= 520 && y <= 554) {
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
        AddChordToTimeline(t);
        return;
    }

    // 中央ペイン: パートタブ切替
    if (y >= 68 && y <= 98 && x >= 280 && x <= width - 260) {
        if (x >= 288 && x <= 418) {
            m_isMergedView = true;
            m_statusMessage = L"全体マージビューを表示中";
        } else {
            float tx = 424.0f;
            const auto& parts = m_partManager.GetParts();
            for (size_t i = 0; i < parts.size(); ++i) {
                if (x >= tx && x <= tx + 94.0f) {
                    m_isMergedView = false;
                    m_partManager.SetActiveIndex(static_cast<int>(i));
                    SyncTimelineToText();
                    m_statusMessage = L"パートを切り替えました: " + Utf8ToWString(parts[i].name);
                    break;
                }
                tx += 98.0f;
            }
            if (x >= tx && x <= tx + 80.0f) {
                m_partManager.AddPart("New Section");
                SyncTimelineToText();
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // 中央ペイン: ルーラークリック (シーク)
    if (y >= 98 && y <= 122 && x >= 280 && x <= width - 260) {
        float relX = x - 280.0f + m_timelineScrollX;
        int clickedBeat = static_cast<int>(relX / m_pixelsPerBeat);
        SeekToBeat(clickedBeat);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // 右ペイン: 定番テンプレートクリック
    if (x >= width - 260 && x <= width && y >= 320) {
        auto tmpls = MusicEngine::GetBuiltinTemplates();
        float ty = 340.0f - m_rightPaneScrollY;
        for (const auto& tmpl : tmpls) {
            if (y >= ty && y <= ty + 26.0f) {
                ApplyTemplate(tmpl);
                return;
            }
            ty += 28.0f;
        }
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
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    // 左ペイン: ダイアトニックダブルクリック
    if (x >= 10 && x <= 270 && y >= 256 && y <= 350) {
        float cardW = (260.0f - 3 * 5.0f) / 4.0f;
        float cardH = 44.0f;
        int col = static_cast<int>((x - 10) / (cardW + 5.0f));
        int row = static_cast<int>((y - 256) / (cardH + 5.0f));
        int idx = row * 4 + col;

        if (idx == 7) {
            ChordToken r;
            r.isRest = true;
            r.quality = ChordQuality::Rest;
            r.rootName = "R";
            r.degreeRoman = "R";
            r.durationBeats = 4;
            AddChordToTimeline(r);
        } else if (idx >= 0 && idx < 7) {
            auto dt = MusicEngine::GetDiatonicChords(pActive->keyRoot, pActive->mode, m_show7thDiatonic);
            if (idx < static_cast<int>(dt.size())) {
                ChordToken c = dt[idx];
                c.durationBeats = 4;
                AddChordToTimeline(c);
            }
        }
        return;
    }

    // 右ペイン: SUGGESTIONS ダブルクリック
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    if (x >= width - 260 && x <= width && y >= 92 && y <= 138) {
        std::vector<ChordToken> currentChordsList;
        for (const auto& b : pActive->bars) {
            for (const auto& c : b.chords) currentChordsList.push_back(c);
        }
        auto suggestions = MusicEngine::GetSuggestions(currentChordsList, pActive->keyRoot, pActive->mode);
        float badgeW = (244.0f - 3 * 6.0f) / 4.0f;
        int idx = static_cast<int>((x - (width - 252.0f)) / (badgeW + 6.0f));
        if (idx >= 0 && idx < static_cast<int>(suggestions.size())) {
            ChordToken c = suggestions[idx];
            c.durationBeats = 4;
            AddChordToTimeline(c);
        }
        return;
    }
}

void AppWindow::OnMouseWheel([[maybe_unused]] int x, [[maybe_unused]] int y, short delta) {
    m_timelineScrollX -= delta * 0.6f;
    if (m_timelineScrollX < 0.0f) m_timelineScrollX = 0.0f;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::OnKeyDown(WPARAM wParam, [[maybe_unused]] LPARAM lParam) {
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (wParam == VK_SPACE) {
        TogglePlayback();
    } else if (wParam == 'T' && !isCtrl) {
        ProcessTapTempo();
    } else if (wParam == VK_RETURN && isCtrl) {
        ApplyTextToTimeline();
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
    m_statusMessage = L"コードをクリップボードにコピーしました";
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
    SyncTimelineToText();
    EnsureVisible(static_cast<int>(pActive->bars.size()) - 1);
    m_statusMessage = L"コードを貼り付けました";
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
    SyncTimelineToText();
}

void AppWindow::SelectAll() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || pActive->bars.empty()) return;

    m_selection.startBar = 0;
    m_selection.endBar = static_cast<int>(pActive->bars.size()) - 1;
}

} // namespace clp
