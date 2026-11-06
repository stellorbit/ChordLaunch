#include "app_window_internal.hpp"
#include "../version.h"
namespace clp {
// ---------------------------------------------------------------------------
// 6. モーダルダイアログ描画 (Settings / Templates / Help / TapTempo)
// ---------------------------------------------------------------------------
void AppWindow::RenderModal(float width, float height) {
    D2D1_RECT_F fullRc = D2D1::RectF(0, 0, width, height);
    m_pRenderTarget->FillRectangle(fullRc, m_pBrushModalBackdrop);

    float cardW = (m_activeModal == ModalType::TapTempo || m_activeModal == ModalType::RenamePart) ? 480.0f
                : ((m_activeModal == ModalType::About || m_activeModal == ModalType::Dashboard) ? std::min(560.0f, width - 40.0f) : std::min(740.0f, width - 40.0f));
    float cardH = (m_activeModal == ModalType::RenamePart) ? 240.0f
                : ((m_activeModal == ModalType::TapTempo) ? 320.0f
                : ((m_activeModal == ModalType::Dashboard) ? std::min(480.0f, height - 40.0f)
                : ((m_activeModal == ModalType::About) ? std::min(360.0f, height - 40.0f)
                : ((m_activeModal == ModalType::Help) ? std::min(600.0f, height - 40.0f) : std::min(520.0f, height - 40.0f)))));
    float cardX = (width - cardW) * 0.5f;
    float cardY = (height - cardH) * 0.5f;

    D2D1_RECT_F modalCardRc = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);
    D2D1_ROUNDED_RECT modalRr = D2D1::RoundedRect(modalCardRc, 8.0f, 8.0f);
    m_pRenderTarget->FillRoundedRectangle(modalRr, m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(modalRr, m_pBrushBorder, 1.5f);

    float titleH = 44.0f;
    m_pRenderTarget->DrawLine(D2D1::Point2F(cardX, cardY + titleH), D2D1::Point2F(cardX + cardW, cardY + titleH), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F closeRc = D2D1::RectF(cardX + cardW - 36.0f, cardY + 8.0f, cardX + cardW - 8.0f, cardY + 36.0f);
    DrawIconClose(closeRc, m_pBrushTextMuted);

    D2D1_RECT_F bodyRc = D2D1::RectF(cardX + 16.0f, cardY + titleH + 12.0f, cardX + cardW - 16.0f, cardY + cardH - 16.0f);

    switch (m_activeModal) {
        case ModalType::Dashboard: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"スタートダッシュボード (Dashboard)", L"Dashboard"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderDashboardModal(bodyRc);
            break;
        }
        case ModalType::Settings: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"環境設定 (Settings)", L"Settings"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderSettingsModal(bodyRc);
            break;
        }
        case ModalType::Templates: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"定番・ジャンル別コード進行テンプレート", L"Chord Progression Templates"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderTemplatesModal(bodyRc);
            break;
        }
        case ModalType::Help: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"Chord Launch 記法・操作リファレンス", L"Chord Launch Notation & Shortcut Reference"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderHelpModal(bodyRc);
            break;
        }
        case ModalType::TapTempo: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"TAP TEMPO (タップテンポ測定)", L"TAP TEMPO"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderTapTempoModal(bodyRc);
            break;
        }
        case ModalType::RenamePart: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            if (m_renameMarkerTick > 0) {
                DrawTextSafe(TR(L"マーカーの編集 (Marker)", L"Marker"), m_pFontModalTitle, titleRc, m_pBrushText);
            } else {
                DrawTextSafe(TR(L"パート名の変更 (Rename Part)", L"Rename Part"), m_pFontModalTitle, titleRc, m_pBrushText);
            }
            RenderRenamePartModal(bodyRc);
            break;
        }
        case ModalType::About: {
            D2D1_RECT_F titleRc = D2D1::RectF(cardX + 20.0f, cardY + 10.0f, cardX + cardW - 50.0f, cardY + 36.0f);
            DrawTextSafe(TR(L"バージョン情報 (About)", L"About ChordLaunch"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderAboutModal(bodyRc);
            break;
        }
        default: break;
    }
}

namespace {

void DrawCheckboxUI(
    ID2D1RenderTarget* pTarget,
    IDWriteTextFormat* pFont,
    ID2D1SolidColorBrush* pTextBrush,
    ID2D1SolidColorBrush* pBorderBrush,
    ID2D1SolidColorBrush* pAccentBrush,
    const D2D1_RECT_F& rc,
    bool isChecked,
    const std::wstring& label,
    bool isGrayed = false
) {
    if (!pTarget || !pFont) return;
    float boxSize = 16.0f;
    float boxY = rc.top + (rc.bottom - rc.top - boxSize) * 0.5f;
    D2D1_RECT_F boxRc = D2D1::RectF(rc.left, boxY, rc.left + boxSize, boxY + boxSize);
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(boxRc, 3.0f, 3.0f);

    pTarget->DrawRoundedRectangle(rr, isGrayed ? pBorderBrush : (isChecked ? pAccentBrush : pBorderBrush), 1.5f);

    if (isChecked) {
        D2D1_RECT_F innerRc = D2D1::RectF(boxRc.left + 3.0f, boxRc.top + 3.0f, boxRc.right - 3.0f, boxRc.bottom - 3.0f);
        pTarget->FillRoundedRectangle(D2D1::RoundedRect(innerRc, 2.0f, 2.0f), isGrayed ? pBorderBrush : pAccentBrush);
    }

    D2D1_RECT_F textRc = D2D1::RectF(rc.left + boxSize + 8.0f, rc.top, rc.right, rc.bottom);
    pTarget->DrawTextW(label.c_str(), static_cast<UINT32>(label.size()), pFont, textRc, isGrayed ? pBorderBrush : pTextBrush);
}

} // namespace

AppWindow::DashboardLayout AppWindow::GetDashboardLayout(const D2D1_RECT_F& rc) const {
    DashboardLayout L;
    float curY = rc.top;

    // 1. 新規プロジェクト (名前 + 保存先 + 作成)
    // 上から 名前 → 保存先 → 作成 の順に配置
    L.newBox = D2D1::RectF(rc.left, curY, rc.right, curY + 128.0f);
    float fieldL = rc.left + 14.0f + 52.0f;
    float fieldR = rc.right - 14.0f - 80.0f;
    L.nameBox = D2D1::RectF(fieldL, curY + 28.0f, rc.right - 14.0f, curY + 52.0f);
    L.dirBox = D2D1::RectF(fieldL, curY + 60.0f, fieldR, curY + 84.0f);
    L.browseBtn = D2D1::RectF(rc.right - 88.0f, curY + 60.0f, rc.right - 14.0f, curY + 84.0f);
    L.createBtn = D2D1::RectF(rc.right - 124.0f, curY + 92.0f, rc.right - 14.0f, curY + 118.0f);
    curY += 136.0f;

    // 2. 最近開いたファイル見出し + ファイルを開く
    L.recentTitle = D2D1::RectF(rc.left + 4.0f, curY + 2.0f, rc.left + 250.0f, curY + 24.0f);
    L.openFileBtn = D2D1::RectF(rc.right - 140.0f, curY, rc.right, curY + 24.0f);
    curY += 28.0f;

    // 3. 一覧
    float listH = std::max(60.0f, rc.bottom - curY - 44.0f);
    L.listBox = D2D1::RectF(rc.left, curY, rc.right, curY + listH);
    float perItem = L.itemH + L.itemGap;
    size_t fit = static_cast<size_t>(std::max(0.0f, std::floor((listH - 8.0f) / perItem)));
    L.maxItems = std::min<size_t>(m_recentFiles.size(), fit);

    // 4. フッター
    L.checkbox = D2D1::RectF(rc.left + 6.0f, rc.bottom - 32.0f, rc.left + 196.0f, rc.bottom - 4.0f);
    L.openBtn = D2D1::RectF(rc.right - 222.0f, rc.bottom - 34.0f, rc.right - 120.0f, rc.bottom);
    L.closeBtn = D2D1::RectF(rc.right - 110.0f, rc.bottom - 34.0f, rc.right, rc.bottom);
    return L;
}

D2D1_RECT_F AppWindow::GetDashboardBodyRc() const {
    float cardW = std::min(560.0f, m_width - 40.0f);
    float cardH = std::min(480.0f, m_height - 40.0f);
    float cardX = (m_width - cardW) * 0.5f;
    float cardY = (m_height - cardH) * 0.5f;
    return D2D1::RectF(cardX + 16.0f, cardY + 44.0f + 12.0f, cardX + cardW - 16.0f, cardY + cardH - 16.0f);
}

int AppWindow::HitTestDashboardRecent(float x, float y) const {
    DashboardLayout L = GetDashboardLayout(GetDashboardBodyRc());
    if (m_recentFiles.empty() || !PtInRectF(L.listBox, x, y)) return -1;
    for (size_t ri = 0; ri < L.maxItems; ++ri) {
        float iy = L.listBox.top + 6.0f + ri * (L.itemH + L.itemGap);
        D2D1_RECT_F itemRc = D2D1::RectF(L.listBox.left + 6.0f, iy, L.listBox.right - 6.0f, iy + L.itemH);
        if (PtInRectF(itemRc, x, y)) return static_cast<int>(ri);
    }
    return -1;
}

// 一時的に左揃えで描画する (フォントの既定は中央揃えのため)
void AppWindow::DrawTextLeft(const std::wstring& text, IDWriteTextFormat* pFont, const D2D1_RECT_F& rc, ID2D1SolidColorBrush* pBrush) {
    if (!pFont) return;
    DWRITE_TEXT_ALIGNMENT prev = pFont->GetTextAlignment();
    pFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    DrawTextSafe(text, pFont, rc, pBrush);
    pFont->SetTextAlignment(prev);
}

// 入力欄の文字: 枠の高さ全体を基準に左寄せ・上下中央で描画する（フォント既定の揃え方に依存しない）
void AppWindow::DrawTextInBox(const std::wstring& text, IDWriteTextFormat* pFont, const D2D1_RECT_F& box, ID2D1Brush* pBrush, float padX) {
    if (!pFont || text.empty()) return;
    const DWRITE_TEXT_ALIGNMENT prevH = pFont->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT prevV = pFont->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING prevW = pFont->GetWordWrapping();
    pFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    pFont->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    pFont->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    const D2D1_RECT_F rc = D2D1::RectF(box.left + padX, box.top, box.right - padX, box.bottom);
    m_pRenderTarget->PushAxisAlignedClip(rc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    DrawTextSafe(text, pFont, rc, pBrush);
    m_pRenderTarget->PopAxisAlignedClip();
    pFont->SetWordWrapping(prevW);
    pFont->SetParagraphAlignment(prevV);
    pFont->SetTextAlignment(prevH);
}

// DirectWrite による文字列の描画幅（測れない場合は 0）
static float MeasureTextWidth(IDWriteFactory* pFactory, IDWriteTextFormat* pFont, const std::wstring& text) {
    if (!pFactory || !pFont || text.empty()) return 0.0f;
    IDWriteTextLayout* pLayout = nullptr;
    if (FAILED(pFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), pFont, 10000.0f, 100.0f, &pLayout)) || !pLayout) {
        return 0.0f;
    }
    DWRITE_TEXT_METRICS metrics{};
    pLayout->GetMetrics(&metrics);
    pLayout->Release();
    return metrics.widthIncludingTrailingWhitespace;
}

// キーキャップ（kbd 風: 角丸の枠 + 下辺の影線）。x から描画し、右端 x を返す。maxRight を超える場合は描画しない
float AppWindow::DrawKeyCap(const std::wstring& label, float x, float cy, float maxRight, bool large) {
    IDWriteTextFormat* capFont = (large && m_pFontHelpKey) ? m_pFontHelpKey : m_pFontButton;
    const float capH = large ? 22.0f : 17.0f;
    const float textW = MeasureTextWidth(m_pDWriteFactory, capFont, label);
    const float capW = std::max(large ? 26.0f : 20.0f, textW + (large ? 16.0f : 12.0f));
    if (x + capW > maxRight) return maxRight;

    const D2D1_RECT_F cap = D2D1::RectF(x, cy - capH * 0.5f, x + capW, cy + capH * 0.5f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(cap, 3.0f, 3.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(cap, 3.0f, 3.0f), m_pBrushBorder, 1.0f);
    // 下辺を太くして押せるキーらしく見せる
    m_pRenderTarget->DrawLine(D2D1::Point2F(cap.left + 3.0f, cap.bottom + 0.5f), D2D1::Point2F(cap.right - 3.0f, cap.bottom + 0.5f), m_pBrushBorder, 2.0f);
    DrawTextSafe(label, capFont, cap, m_pBrushText);
    return cap.right;
}

// "[Ctrl] + [Z]  /  [Ctrl] + [Y]" のように、[ ] 内をキーキャップ、その外を通常文字として左から順に描画する
void AppWindow::DrawKeyCombo(const std::wstring& combo, float x, float cy, float maxRight, bool large) {
    IDWriteTextFormat* plainFont = (large && m_pFontHelp) ? m_pFontHelp : m_pFontInspector;
    size_t i = 0;
    float curX = x;
    while (i < combo.size() && curX < maxRight) {
        if (combo[i] == L'[') {
            const size_t close = combo.find(L']', i + 1);
            if (close == std::wstring::npos) break;
            curX = DrawKeyCap(combo.substr(i + 1, close - i - 1), curX, cy, maxRight, large) + (large ? 4.0f : 3.0f);
            i = close + 1;
        } else {
            size_t next = combo.find(L'[', i);
            if (next == std::wstring::npos) next = combo.size();
            const std::wstring seg = combo.substr(i, next - i);
            const float w = MeasureTextWidth(m_pDWriteFactory, plainFont, seg);
            const D2D1_RECT_F segRc = D2D1::RectF(curX, cy - 12.0f, std::min(maxRight, curX + w + 2.0f), cy + 12.0f);
            DrawTextInBox(seg, plainFont, segRc, large ? m_pBrushText : m_pBrushTextMuted, 0.0f);
            curX += w;
            i = next;
        }
    }
}

void AppWindow::RenderDashboardModal(const D2D1_RECT_F& rc) {
    DashboardLayout L = GetDashboardLayout(rc);

    // 1. 新規プロジェクト項目
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(L.newBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(L.newBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F nTitle = D2D1::RectF(rc.left + 14.0f, L.newBox.top + 5.0f, rc.left + 350.0f, L.newBox.top + 25.0f);
    DrawTextLeft(TR(L"新規プロジェクト", L"New Project"), m_pFontCardTitle, nTitle, m_pBrushText);

    D2D1_RECT_F nameLbl = D2D1::RectF(rc.left, L.nameBox.top, L.nameBox.left - 4.0f, L.nameBox.bottom);
    DrawTextInBox(TR(L"名前", L"Name"), m_pFontInspector, nameLbl, m_pBrushText, 14.0f);
    D2D1_RECT_F dirLbl = D2D1::RectF(rc.left, L.dirBox.top, L.dirBox.left - 4.0f, L.dirBox.bottom);
    DrawTextInBox(TR(L"保存先", L"Folder"), m_pFontInspector, dirLbl, m_pBrushText, 14.0f);

    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(L.nameBox, 4.0f, 4.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(L.nameBox, 4.0f, 4.0f), m_dashNameFocused ? m_pBrushAccent : m_pBrushBorderLight, m_dashNameFocused ? 1.5f : 1.0f);
    std::wstring nameText = m_dashNewName + (m_dashNameFocused ? L"|" : L"");
    DrawTextInBox(nameText, m_pFontInspector, L.nameBox, m_pBrushText, 8.0f);

    DrawButton(L.createBtn, TR(L"作成", L"Create"), IconType::None, false, false, true, Theme::AccentAction);

    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(L.dirBox, 4.0f, 4.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(L.dirBox, 4.0f, 4.0f), m_pBrushBorderLight, 1.0f);
    std::wstring displayProjDir = m_projectDirectory.empty() ? TR(L"(未設定 - 参照...で指定)", L"(Not set - click Browse)") : m_projectDirectory;
    DrawTextInBox(displayProjDir, m_pFontInspector, L.dirBox, m_projectDirectory.empty() ? m_pBrushTextMuted : m_pBrushText, 8.0f);
    DrawButton(L.browseBtn, TR(L"参照...", L"Browse..."), IconType::None);

    // 2. 最近開いたファイル
    DrawTextLeft(TR(L"最近開いたファイル", L"Recent Projects"), m_pFontCardTitle, L.recentTitle, m_pBrushText);
    DrawButton(L.openFileBtn, TR(L"ファイルを開く...", L"Open File..."), IconType::None);

    float rListH = L.listBox.bottom - L.listBox.top;
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(L.listBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(L.listBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    if (m_recentFiles.empty()) {
        D2D1_RECT_F emptyRc = D2D1::RectF(L.listBox.left, L.listBox.top + rListH * 0.4f, L.listBox.right, L.listBox.top + rListH * 0.6f);
        DrawTextSafe(TR(L"最近開いたプロジェクトはありません", L"No recent projects found"), m_pFontModalBody, emptyRc, m_pBrushTextMuted);
    } else {
        for (size_t ri = 0; ri < L.maxItems; ++ri) {
            float iy = L.listBox.top + 6.0f + ri * (L.itemH + L.itemGap);
            D2D1_RECT_F itemRc = D2D1::RectF(L.listBox.left + 6.0f, iy, L.listBox.right - 6.0f, iy + L.itemH);

            bool isSel = (static_cast<int>(ri) == m_dashboardSelectedRecentIndex);
            if (isSel) {
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(itemRc, 4.0f, 4.0f), m_pBrushCardSelected);
                m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(itemRc, 4.0f, 4.0f), m_pBrushAccent, 1.0f);
            }

            std::filesystem::path fp(m_recentFiles[ri]);
            std::wstring fName = fp.filename().wstring();
            std::wstring fDir = NormalizePathString(fp.parent_path().wstring());

            D2D1_RECT_F nameRc = D2D1::RectF(itemRc.left + 10.0f, itemRc.top + 4.0f, itemRc.right - 10.0f, itemRc.top + 24.0f);
            DrawTextLeft(fName, m_pFontTab, nameRc, isSel ? m_pBrushAccent : m_pBrushText);

            D2D1_RECT_F dirRc = D2D1::RectF(itemRc.left + 10.0f, itemRc.top + 25.0f, itemRc.right - 10.0f, itemRc.bottom - 3.0f);
            DrawTextSafe(fDir, m_pFontInspector, dirRc, m_pBrushTextMuted);
        }
    }

    // 3. フッター
    DrawCheckboxUI(m_pRenderTarget, m_pFontInspector, m_pBrushText, m_pBrushBorderLight, m_pBrushAccent, L.checkbox, m_showDashboardOnStartup, TR(L"起動時に表示", L"Show on startup"));

    if (m_dashboardSelectedRecentIndex >= 0 && m_dashboardSelectedRecentIndex < static_cast<int>(m_recentFiles.size())) {
        DrawButton(L.openBtn, TR(L"開く", L"Open"), IconType::None, false, false, true, Theme::AccentAction);
    }

    DrawButton(L.closeBtn, TR(L"閉じる", L"Close"), IconType::None);
}

void AppWindow::RenderSettingsModal(const D2D1_RECT_F& rc) {
    float curY = rc.top;

    // 1. 表示言語
    D2D1_RECT_F lBox = D2D1::RectF(rc.left, curY, rc.right, curY + 54.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(lBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(lBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F lTitle = D2D1::RectF(rc.left + 14.0f, curY + 16.0f, rc.left + 300.0f, curY + 36.0f);
    DrawTextSafe(TR(L"表示言語 / Language", L"Display Language"), m_pFontCardTitle, lTitle, m_pBrushText);

    D2D1_RECT_F bJa = D2D1::RectF(rc.right - 230.0f, curY + 12.0f, rc.right - 120.0f, curY + 42.0f);
    DrawButton(bJa, L"日本語 (JP)", IconType::None, false, false, m_settingLanguage == 0);

    D2D1_RECT_F bEn = D2D1::RectF(rc.right - 114.0f, curY + 12.0f, rc.right - 16.0f, curY + 42.0f);
    DrawButton(bEn, L"English (US)", IconType::None, false, false, m_settingLanguage == 1);

    curY += 62.0f;

    // 2. ズーム操作スタイル
    D2D1_RECT_F zBox = D2D1::RectF(rc.left, curY, rc.right, curY + 76.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(zBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(zBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F zTitle = D2D1::RectF(rc.left + 14.0f, curY + 8.0f, rc.left + 400.0f, curY + 28.0f);
    DrawTextSafe(TR(L"タイムライン横ズーム操作スタイル", L"Timeline Horizontal Zoom Shortcut Style"), m_pFontCardTitle, zTitle, m_pBrushText);

    float zBtnW = (rc.right - rc.left - 28.0f - 18.0f) / 4.0f;
    float zBtnH = 30.0f;
    float zBtnY = curY + 34.0f;

    const wchar_t* zoomLabels[] = { L"Pro Tools (R/T)", L"Cubase (G/H)", L"Studio One (W/E)", TR(L"標準 (Ctrl +/-)", L"Default (Ctrl +/-)") };
    for (int i = 0; i < 4; ++i) {
        float zx = rc.left + 14.0f + i * (zBtnW + 6.0f);
        D2D1_RECT_F zRc = D2D1::RectF(zx, zBtnY, zx + zBtnW, zBtnY + zBtnH);
        DrawButton(zRc, zoomLabels[i], IconType::None, false, false, m_settingZoomStyle == i);
    }

    curY += 84.0f;

    // 3. 起動時ダッシュボード表示設定
    D2D1_RECT_F dBox = D2D1::RectF(rc.left, curY, rc.right, curY + 46.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(dBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(dBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F dChk = D2D1::RectF(rc.left + 14.0f, curY + 12.0f, rc.right - 14.0f, curY + 34.0f);
    DrawCheckboxUI(m_pRenderTarget, m_pFontTab, m_pBrushText, m_pBrushBorderLight, m_pBrushAccent, dChk, m_showDashboardOnStartup, TR(L"Chord Launch 起動時にスタートダッシュボードを表示する", L"Show start dashboard on startup"));

    curY += 54.0f;

    // 4. プロジェクト保存先フォルダ設定
    D2D1_RECT_F projBox = D2D1::RectF(rc.left, curY, rc.right, curY + 62.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(projBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(projBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F projTitle = D2D1::RectF(rc.left + 14.0f, curY + 8.0f, rc.left + 350.0f, curY + 26.0f);
    DrawTextSafe(TR(L"プロジェクト保存先フォルダ", L"Project Save Directory"), m_pFontCardTitle, projTitle, m_pBrushText);

    D2D1_RECT_F projPathBox = D2D1::RectF(rc.left + 14.0f, curY + 30.0f, rc.right - 96.0f, curY + 54.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(projPathBox, 4.0f, 4.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(projPathBox, 4.0f, 4.0f), m_pBrushBorderLight, 1.0f);

    std::wstring displayProjDir = m_projectDirectory.empty() ? TR(L"(未設定 - 指定すると保存ダイアログの初期位置になります)", L"(Not set - Click browse to choose)") : m_projectDirectory;
    DrawTextInBox(displayProjDir, m_pFontInspector, projPathBox, m_projectDirectory.empty() ? m_pBrushTextMuted : m_pBrushText, 8.0f);

    D2D1_RECT_F bBrowseProj = D2D1::RectF(rc.right - 88.0f, curY + 30.0f, rc.right - 14.0f, curY + 54.0f);
    DrawButton(bBrowseProj, TR(L"参照...", L"Browse..."), IconType::None);

    curY += 70.0f;

    // 5. 自動バックアップ設定 & プロジェクトフォルダ内格納オプション
    D2D1_RECT_F bakBox = D2D1::RectF(rc.left, curY, rc.right, curY + 98.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(bakBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(bakBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F bakChk = D2D1::RectF(rc.left + 14.0f, curY + 10.0f, rc.right - 14.0f, curY + 30.0f);
    DrawCheckboxUI(m_pRenderTarget, m_pFontTab, m_pBrushText, m_pBrushBorderLight, m_pBrushAccent, bakChk, m_autoBackupInProjectDir, TR(L"プロジェクトフォルダ内に自動保存ファイルを格納", L"Store auto-backup files inside project folder"));

    // 自動保存先フォルダ (カスタム指定)
    bool isCustomBakGrayed = m_autoBackupInProjectDir;
    D2D1_RECT_F bakSubTitle = D2D1::RectF(rc.left + 14.0f, curY + 38.0f, rc.left + 350.0f, curY + 56.0f);
    DrawTextSafe(TR(L"自動保存先フォルダ (カスタム指定)", L"Custom Auto-Backup Directory"), m_pFontCardTitle, bakSubTitle, isCustomBakGrayed ? m_pBrushTextMuted : m_pBrushText);

    D2D1_RECT_F bakPathBox = D2D1::RectF(rc.left + 14.0f, curY + 62.0f, rc.right - 96.0f, curY + 86.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(bakPathBox, 4.0f, 4.0f), isCustomBakGrayed ? m_pBrushBg : m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(bakPathBox, 4.0f, 4.0f), m_pBrushBorderLight, 1.0f);

    std::wstring displayBakDir = isCustomBakGrayed ? TR(L"(プロジェクトフォルダ内の Project Backup に自動保存されます)", L"(Automatically stored in project's Backup folder)") : (m_autoBackupDirectory.empty() ? TR(L"(未設定 - LocalAppData/ChordLaunch/AutoBackup に保存)", L"(Not set - Uses default AppData)") : m_autoBackupDirectory);
    DrawTextInBox(displayBakDir, m_pFontInspector, bakPathBox, m_pBrushTextMuted, 8.0f);

    D2D1_RECT_F bBrowseBak = D2D1::RectF(rc.right - 88.0f, curY + 62.0f, rc.right - 14.0f, curY + 86.0f);
    if (isCustomBakGrayed) {
        // グレーアウト描画 (無効化)
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(bBrowseBak, 4.0f, 4.0f), m_pBrushBorderLight, 1.0f);
        D2D1_RECT_F btnTextRc = D2D1::RectF(bBrowseBak.left, bBrowseBak.top + 3.0f, bBrowseBak.right, bBrowseBak.bottom);
        DrawTextSafe(TR(L"参照...", L"Browse..."), m_pFontTab, btnTextRc, m_pBrushTextMuted);
    } else {
        DrawButton(bBrowseBak, TR(L"参照...", L"Browse..."), IconType::None);
    }

    // 6. 下部ボタングループ (保存して適用 / キャンセル)
    D2D1_RECT_F bSave = D2D1::RectF(rc.right - 130.0f, rc.bottom - 36.0f, rc.right, rc.bottom);
    DrawButton(bSave, TR(L"保存して適用", L"Save & Apply"), IconType::None, false, false, true, Theme::AccentAction);

    D2D1_RECT_F bCancel = D2D1::RectF(rc.right - 230.0f, rc.bottom - 36.0f, rc.right - 140.0f, rc.bottom);
    DrawButton(bCancel, TR(L"キャンセル", L"Cancel"), IconType::None);
}

void AppWindow::RenderTemplatesModal(const D2D1_RECT_F& rc) {
    // ---------------- 左側: ジャンル大分類リスト ----------------
    float catW = 190.0f;
    D2D1_RECT_F catBox = D2D1::RectF(rc.left, rc.top, rc.left + catW, rc.bottom - 44.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(catBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(catBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    auto categories = MusicEngine::GetTemplateMajorCategories();
    float itemH = 34.0f;
    for (size_t i = 0; i < categories.size(); ++i) {
        float iy = rc.top + 6.0f + i * (itemH + 4.0f);
        D2D1_RECT_F itRc = D2D1::RectF(rc.left + 6.0f, iy, rc.left + catW - 6.0f, iy + itemH);
        bool isSel = (static_cast<int>(i) == m_selectedTmplCategoryIndex);

        if (isSel) {
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(itRc, 4.0f, 4.0f), m_pBrushAccent);
        }
        std::wstring wCat = GetLocalizedCategoryName(categories[i], m_settingLanguage);
        DrawTextSafe(wCat, m_pFontTab, itRc, isSel ? m_pBrushBg : m_pBrushText);
    }

    // ---------------- 右側: テンプレート管理・一覧 ----------------
    float listX = rc.left + catW + 12.0f;
    float topBarH = 32.0f;

    // ヘッダーアクションバー (+ 新規追加, + 現在の進行を登録)
    D2D1_RECT_F bAddNew = D2D1::RectF(rc.right - 230.0f, rc.top, rc.right - 120.0f, rc.top + topBarH);
    DrawButton(bAddNew, TR(L"新規追加", L"Add New"), IconType::Plus, false, false, true, Theme::AccentWarm);

    D2D1_RECT_F bAddCurr = D2D1::RectF(rc.right - 114.0f, rc.top, rc.right, rc.top + topBarH);
    DrawButton(bAddCurr, TR(L"進行を登録", L"Save Current"), IconType::Plus, false, false, true, Theme::AccentAction);

    // 一覧ボックス
    D2D1_RECT_F listBox = D2D1::RectF(listX, rc.top + topBarH + 6.0f, rc.right, rc.bottom - 44.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(listBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(listBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    std::string curCat = (m_selectedTmplCategoryIndex < static_cast<int>(categories.size())) ? categories[m_selectedTmplCategoryIndex] : "👤 ユーザー定義";
    auto tmpls = MusicEngine::GetTemplatesByCategory(curCat);

    int maxScroll = std::max(0, static_cast<int>(tmpls.size()) - 5);
    m_tmplListScroll = std::clamp(m_tmplListScroll, 0, maxScroll);

    float tItemH = 66.0f;
    size_t startIdx = static_cast<size_t>(m_tmplListScroll);
    size_t endIdx = std::min<size_t>(tmpls.size(), startIdx + 5);
    for (size_t i = startIdx; i < endIdx; ++i) {
        size_t slot = i - startIdx;
        float ty = listBox.top + 6.0f + slot * (tItemH + 6.0f);
        D2D1_RECT_F tRc = D2D1::RectF(listX + 6.0f, ty, rc.right - 6.0f, ty + tItemH);
        bool isSel = (static_cast<int>(i) == m_selectedTmplIndex);

        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(tRc, 6.0f, 6.0f), isSel ? m_pBrushCardActive : m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(tRc, 6.0f, 6.0f), isSel ? m_pBrushAccent : m_pBrushBorderLight, 1.0f);

        float textRight = tRc.right - (tmpls[i].isUserCustom ? 236.0f : 140.0f);
        D2D1_RECT_F textClipRc = D2D1::RectF(tRc.left + 8.0f, ty + 2.0f, textRight, ty + tItemH - 2.0f);
        m_pRenderTarget->PushAxisAlignedClip(textClipRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        // テンプレート名
        D2D1_RECT_F nRc = D2D1::RectF(tRc.left + 10.0f, ty + 5.0f, textRight, ty + 23.0f);
        std::wstring wName = GetLocalizedTemplateName(tmpls[i], m_settingLanguage);
        DrawTextSafe(wName, m_pFontCardTitle, nRc, m_pBrushText);

        // コード進行表記
        D2D1_RECT_F pRc = D2D1::RectF(tRc.left + 10.0f, ty + 24.0f, textRight, ty + 42.0f);
        std::wstring progText = ToWString(tmpls[i].progression);
        DrawTextSafe(progText, m_pFontInspector, pRc, m_pBrushAccent);

        // 説明文
        D2D1_RECT_F dRc = D2D1::RectF(tRc.left + 10.0f, ty + 43.0f, textRight, ty + 62.0f);
        std::wstring descText = GetLocalizedTemplateDesc(tmpls[i], m_settingLanguage);
        DrawTextSafe(descText, m_pFontInspector, dRc, m_pBrushTextMuted);

        m_pRenderTarget->PopAxisAlignedClip();

        // ボタン群: 試聴, 適用, (ユーザー定義の場合: 編集, 削除)
        float btnY = ty + 17.0f;
        float btnH = 32.0f;
        float bRight = tRc.right - 6.0f;

        // 適用
        bRight -= 58.0f;
        D2D1_RECT_F bApply = D2D1::RectF(bRight, btnY, bRight + 56.0f, btnY + btnH);
        DrawButton(bApply, TR(L"適用", L"Apply"), IconType::None, false, false, true, Theme::AccentAction);

        // 試聴
        bRight -= 70.0f;
        D2D1_RECT_F bAudition = D2D1::RectF(bRight, btnY, bRight + 66.0f, btnY + btnH);
        DrawButton(bAudition, TR(L"試聴", L"Preview"), IconType::Play, false, false, false);

        if (tmpls[i].isUserCustom) {
            // 編集
            bRight -= 48.0f;
            D2D1_RECT_F bEdit = D2D1::RectF(bRight, btnY, bRight + 44.0f, btnY + btnH);
            DrawButton(bEdit, TR(L"編集", L"Edit"), IconType::None, false, false, false);

            // 削除
            bRight -= 48.0f;
            D2D1_RECT_F bDel = D2D1::RectF(bRight, btnY, bRight + 44.0f, btnY + btnH);
            DrawButton(bDel, TR(L"削除", L"Delete"), IconType::None, false, false, true, Theme::AccentStop);
        }
    }

    // フッター
    D2D1_RECT_F bClose = D2D1::RectF(rc.right - 100.0f, rc.bottom - 36.0f, rc.right, rc.bottom);
    DrawButton(bClose, TR(L"閉じる", L"Close"), IconType::None);

    // ---------------- サブフォーム (テンプレート追加・編集パネル) ----------------
    if (m_tmplEditorOpen) {
        // オーバーレイ背景
        m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left - 20.0f, rc.top - 60.0f, rc.right + 20.0f, rc.bottom + 20.0f), m_pBrushModalBackdrop);

        float formW = 520.0f;
        float formH = 340.0f;
        float fx = (rc.left + rc.right - formW) * 0.5f;
        float fy = (rc.top + rc.bottom - formH) * 0.5f;
        D2D1_RECT_F formRc = D2D1::RectF(fx, fy, fx + formW, fy + formH);

        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(formRc, 8.0f, 8.0f), m_pBrushBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(formRc, 8.0f, 8.0f), m_pBrushBorder, 1.5f);

        // フォームタイトル
        D2D1_RECT_F fTitleRc = D2D1::RectF(fx + 20.0f, fy + 16.0f, fx + formW - 20.0f, fy + 40.0f);
        DrawTextSafe(m_tmplIsEditingExisting ? TR(L"テンプレートの編集", L"Edit Template") : TR(L"コードテンプレートの新規登録", L"Register New Progression Template"), m_pFontModalTitle, fTitleRc, m_pBrushText);

        // 1. テンプレート名
        D2D1_RECT_F lName = D2D1::RectF(fx + 20.0f, fy + 50.0f, fx + formW - 20.0f, fy + 68.0f);
        DrawTextSafe(TR(L"テンプレート名 (例: 王道サビ進行):", L"Template Name (e.g., Chorus Royal Road):"), m_pFontInspector, lName, m_pBrushTextMuted);
        D2D1_RECT_F boxName = D2D1::RectF(fx + 20.0f, fy + 70.0f, fx + formW - 20.0f, fy + 102.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxName, 4.0f, 4.0f), m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxName, 4.0f, 4.0f), (m_tmplEditFocusField == 0) ? m_pBrushAccent : m_pBrushBorderLight, (m_tmplEditFocusField == 0) ? 2.0f : 1.0f);
        DrawTextInBox(m_tmplEditName.empty() ? TR(L"(名前を入力)", L"(Enter template name)") : m_tmplEditName, m_pFontButton, boxName, m_tmplEditName.empty() ? m_pBrushTextMuted : m_pBrushText, 8.0f);

        // 2. コード進行
        D2D1_RECT_F lProg = D2D1::RectF(fx + 20.0f, fy + 112.0f, fx + formW - 20.0f, fy + 130.0f);
        DrawTextSafe(TR(L"コード進行 (例: | Fmaj7 | G7 | Em7 | Am7 |):", L"Progression (e.g., | Fmaj7 | G7 | Em7 | Am7 |):"), m_pFontInspector, lProg, m_pBrushTextMuted);
        D2D1_RECT_F boxProg = D2D1::RectF(fx + 20.0f, fy + 132.0f, fx + formW - 20.0f, fy + 164.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxProg, 4.0f, 4.0f), m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxProg, 4.0f, 4.0f), (m_tmplEditFocusField == 1) ? m_pBrushAccent : m_pBrushBorderLight, (m_tmplEditFocusField == 1) ? 2.0f : 1.0f);
        DrawTextInBox(m_tmplEditProg.empty() ? TR(L"(コード進行を入力)", L"(Enter chord progression)") : m_tmplEditProg, m_pFontButton, boxProg, m_tmplEditProg.empty() ? m_pBrushTextMuted : m_pBrushAccent, 8.0f);

        // 3. 説明文
        D2D1_RECT_F lDesc = D2D1::RectF(fx + 20.0f, fy + 174.0f, fx + formW - 20.0f, fy + 192.0f);
        DrawTextSafe(TR(L"説明文 (任意):", L"Description (Optional):"), m_pFontInspector, lDesc, m_pBrushTextMuted);
        D2D1_RECT_F boxDesc = D2D1::RectF(fx + 20.0f, fy + 194.0f, fx + formW - 20.0f, fy + 226.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxDesc, 4.0f, 4.0f), m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxDesc, 4.0f, 4.0f), (m_tmplEditFocusField == 2) ? m_pBrushAccent : m_pBrushBorderLight, (m_tmplEditFocusField == 2) ? 2.0f : 1.0f);
        DrawTextInBox(m_tmplEditDesc.empty() ? TR(L"(説明文を入力)", L"(Enter description)") : m_tmplEditDesc, m_pFontButton, boxDesc, m_tmplEditDesc.empty() ? m_pBrushTextMuted : m_pBrushText, 8.0f);

        // フォームボタン (保存 / キャンセル)
        D2D1_RECT_F bFSave = D2D1::RectF(fx + formW - 140.0f, fy + formH - 46.0f, fx + formW - 20.0f, fy + formH - 14.0f);
        DrawButton(bFSave, TR(L"保存して登録", L"Save Template"), IconType::None, false, false, true, Theme::AccentAction);

        D2D1_RECT_F bFCancel = D2D1::RectF(fx + formW - 250.0f, fy + formH - 46.0f, fx + formW - 150.0f, fy + formH - 14.0f);
        DrawButton(bFCancel, TR(L"キャンセル", L"Cancel"), IconType::None);
    }
}

// バージョン情報ダイアログ本体 (製品名・バージョン・後継の旨・生成AI使用表明・リポジトリ)
void AppWindow::RenderAboutModal(const D2D1_RECT_F& rc) {
    float y = rc.top;
    DrawTextSafe(L"ChordLaunch", m_pFontModalTitle, D2D1::RectF(rc.left, y, rc.right, y + 30.0f), m_pBrushText);
    y += 34.0f;

    std::wstring ver = std::wstring(TR(L"バージョン: ", L"Version: ")) + CHORDLAUNCH_VERSION_WSTR +
                       TR(L"（プレリリース）", L" (Pre-release)");
    DrawTextSafe(ver, m_pFontModalTitle, D2D1::RectF(rc.left, y, rc.right, y + 24.0f), m_pBrushAccent);
    y += 24.0f;

    std::wstring build = std::wstring(TR(L"ビルド日付: ", L"Build date: ")) + L"" __DATE__;
    DrawTextSafe(build, m_pFontInspector, D2D1::RectF(rc.left, y, rc.right, y + 20.0f), m_pBrushTextMuted);
    y += 30.0f;

    DrawTextSafe(
        TR(L"ChordLaunch は、旧アプリ「ChordLaunchpad」の後継アプリです。",
           L"ChordLaunch is the successor to the former app \"ChordLaunchpad\"."),
        m_pFontInspector, D2D1::RectF(rc.left, y, rc.right, y + 20.0f), m_pBrushText);
    y += 32.0f;

    DrawTextSafe(TR(L"生成AI使用表明", L"AI Usage Disclosure"), m_pFontModalTitle,
                 D2D1::RectF(rc.left, y, rc.right, y + 24.0f), m_pBrushAccent);
    y += 24.0f;
    DrawTextSafe(
        TR(L"本ソフトのコード・ドキュメントは、生成AI（Gemini 4 Argon / Gemini 3.8 Flash / Claude Sonnet 5.5）を使用して作成・改修しています。",
           L"The code and documentation of this software were created and modified with generative AI (Gemini 4 Argon / Gemini 3.8 Flash / Claude Sonnet 5.5)."),
        m_pFontInspector, D2D1::RectF(rc.left, y, rc.right, y + 44.0f), m_pBrushText);
    y += 54.0f;

    DrawTextSafe(std::wstring(TR(L"リポジトリ: ", L"Repository: ")) + CHORDLAUNCH_REPO_URL_WSTR,
                 m_pFontInspector, D2D1::RectF(rc.left, y, rc.right, y + 20.0f), m_pBrushTextMuted);

    // 下部アクションボタン
    D2D1_RECT_F bClose = D2D1::RectF(rc.right - 100.0f, rc.bottom - 36.0f, rc.right, rc.bottom);
    DrawButton(bClose, TR(L"閉じる", L"Close"), IconType::None);
}

void AppWindow::RenderHelpModal(const D2D1_RECT_F& rc) {
    float tabW = 132.0f;
    float tabH = 32.0f;
    const wchar_t* helpTabs[] = {
        TR(L"入力形式", L"Input Formats"),
        TR(L"数字接尾辞", L"Numeric Suffixes"),
        TR(L"コード記号", L"Chord Symbols"),
        TR(L"キーボード", L"Keyboard"),
        TR(L"マウス・操作", L"Mouse & Tools")
    };

    for (int i = 0; i < 5; ++i) {
        float tx = rc.left + i * (tabW + 8.0f);
        D2D1_RECT_F tRc = D2D1::RectF(tx, rc.top, tx + tabW, rc.top + tabH);
        DrawButton(tRc, helpTabs[i], IconType::None, false, false, m_helpActiveTab == i);
    }

    float bodyY = rc.top + tabH + 10.0f;
    D2D1_RECT_F boxRc = D2D1::RectF(rc.left, bodyY, rc.right, rc.bottom - 46.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxRc, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxRc, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    float textY = bodyY + 12.0f;
    float pad = 16.0f;

    // 表の罫線: 外枠・行区切り・列区切り（colX は表左端からの各列開始位置）
    auto drawTableGrid = [&](float left, float top, float width, float rowH, int rows, const float* colX, int cols) {
        const float bottom = top + rowH * static_cast<float>(rows);
        m_pRenderTarget->DrawRectangle(D2D1::RectF(left, top, left + width, bottom), m_pBrushBorder, 1.0f);
        for (int r = 1; r < rows; ++r) {
            const float ly = top + rowH * static_cast<float>(r);
            m_pRenderTarget->DrawLine(D2D1::Point2F(left, ly), D2D1::Point2F(left + width, ly), m_pBrushBorderLight, 1.0f);
        }
        for (int c = 1; c < cols; ++c) {
            const float lx = left + colX[c];
            m_pRenderTarget->DrawLine(D2D1::Point2F(lx, top), D2D1::Point2F(lx, bottom), m_pBrushBorderLight, 1.0f);
        }
    };

    if (m_helpActiveTab == 0) {
        // --- タブ0: 入力形式 ---
        struct InputItem {
            const wchar_t* title;
            const wchar_t* example;
            const wchar_t* desc;
        };
        auto drawItemCard = [&](const InputItem& it, float cardH) -> D2D1_RECT_F {
            D2D1_RECT_F cardRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + cardH);
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(cardRc, 4.0f, 4.0f), m_pBrushCardBg);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(cardRc, 4.0f, 4.0f), m_pBrushBorderLight, 0.8f);
            DrawTextInBox(it.title, m_pFontButton, D2D1::RectF(cardRc.left, cardRc.top + 6.0f, cardRc.right, cardRc.top + 24.0f), m_pBrushText, 10.0f);
            return cardRc;
        };
        auto drawSimpleItem = [&](const InputItem& it) {
            D2D1_RECT_F cardRc = drawItemCard(it, 68.0f);
            DrawTextInBox(it.example, m_pFontCardTitle, D2D1::RectF(cardRc.left, cardRc.top + 26.0f, cardRc.right, cardRc.top + 44.0f), m_pBrushAccent, 10.0f);
            DrawTextInBox(it.desc, m_pFontInspector, D2D1::RectF(cardRc.left, cardRc.top + 46.0f, cardRc.right, cardRc.top + 64.0f), m_pBrushTextMuted, 10.0f);
            textY += 74.0f;
        };

        drawSimpleItem({ TR(L"1. 通常コード名入力", L"1. Standard Chord Symbols"),
              TR(L"C G Am F    または    Fmaj7 E7 Am7 C7", L"C G Am F    or    Fmaj7 E7 Am7 C7"),
              TR(L"標準的なコードシンボルを半角スペース区切りで並べて入力します。小節線 | を使用できます。", L"Enter standard chord symbols separated by spaces. Use | for bar lines.") });
        drawSimpleItem({ TR(L"2. ディグリー入力 (ローマ数字)", L"2. Roman Numeral Degrees"),
              TR(L"IVmaj7 III7 vim7 I7    または    I V vi IV", L"IVmaj7 III7 vim7 I7    or    I V vi IV"),
              TR(L"大文字でメジャー系、小文字でマイナー系コードを解釈します。現在キーに基づいて実音コードへ変換されます。", L"Uppercase for major, lowercase for minor. Automatically transposed to the active key.") });

        // 3. アラビア数字入力: コマンド入力・通常記号入力・複合入力・数字のみ を「入力 → 結果」の表で示す
        {
            const float tblRowH = 20.0f;
            const int tblRows = 5; // 見出し + 4行
            const float cardH = 30.0f + tblRowH * tblRows + 30.0f;
            D2D1_RECT_F cardRc = drawItemCard({ TR(L"3. アラビア数字入力 (度数＋接尾辞)", L"3. Arabic Numeral Input (Degree + Suffix)"), L"", L"" }, cardH);
            const float tblLeft = cardRc.left + 10.0f;
            const float tblW = (cardRc.right - cardRc.left) - 20.0f;
            const float tblTop = cardRc.top + 28.0f;
            const float colX[3] = { 0.0f, tblW * 0.22f, tblW * 0.62f };
            m_pRenderTarget->FillRectangle(D2D1::RectF(tblLeft + 1.0f, tblTop + 1.0f, tblLeft + tblW - 1.0f, tblTop + tblRowH), m_pBrushBorderLight);
            drawTableGrid(tblLeft, tblTop, tblW, tblRowH, tblRows, colX, 3);

            struct NumRow { const wchar_t* kind; const wchar_t* input; const wchar_t* result; };
            const NumRow numRows[] = {
                { TR(L"種類", L"Type"), TR(L"入力 (Key: C)", L"Input (Key: C)"), TR(L"変換結果", L"Result") },
                { TR(L"コマンド入力", L"Command suffix"), L"4mas 3svn 6mis 1", L"FMaj7 E7 Am7 C" },
                { TR(L"通常の記号", L"Standard symbols"), L"4M7 37 6m7 1", L"FMaj7 E7 Am7 C" },
                { TR(L"複合入力", L"Mixed input"), L"4M7 E7 6m7 C", L"FMaj7 E7 Am7 C" },
                { TR(L"数字のみ", L"Numbers only"), L"1 5 6 4", L"C G Am F" }
            };
            for (int r = 0; r < tblRows; ++r) {
                const float ry = tblTop + tblRowH * static_cast<float>(r);
                const bool head = (r == 0);
                IDWriteTextFormat* f = head ? m_pFontInspector : m_pFontButton;
                DrawTextInBox(numRows[r].kind, f, D2D1::RectF(tblLeft + colX[0], ry, tblLeft + colX[1], ry + tblRowH), m_pBrushText, 8.0f);
                DrawTextInBox(numRows[r].input, f, D2D1::RectF(tblLeft + colX[1], ry, tblLeft + colX[2], ry + tblRowH), head ? m_pBrushText : m_pBrushAccent, 8.0f);
                DrawTextInBox(numRows[r].result, f, D2D1::RectF(tblLeft + colX[2], ry, tblLeft + tblW, ry + tblRowH), head ? m_pBrushText : m_pBrushAccent, 8.0f);
            }
            DrawTextInBox(TR(L"度数(1〜7)の直後に接尾辞を続けます。通常コード名と同じ行に混在できます (接尾辞の一覧は「数字接尾辞」タブ)。",
                             L"Follow the degree (1-7) with a suffix. Mixing with standard chord names on one line is fine (see the Numeric Suffixes tab)."),
                          m_pFontInspector, D2D1::RectF(cardRc.left, tblTop + tblRowH * tblRows + 4.0f, cardRc.right, cardRc.bottom - 2.0f), m_pBrushTextMuted, 10.0f);
            textY += cardH + 6.0f;
        }

        drawSimpleItem({ TR(L"4. 記号 (小節線・持続・リピート・休符)", L"4. Symbols (Bar, Tie, Repeat, Rest)"),
              TR(L"| : 小節区切り    - : タイ持続    % / = : 直前リピート    R / N.C. : 休符", L"| : Bar line    - : Tie / Hold    % / = : Repeat previous    R / N.C. : Rest"),
              TR(L"例: | Fmaj7 - - - | E7 - - - | Am7 - - - | C7 - - - |", L"Example: | Fmaj7 - - - | E7 - - - | Am7 - - - | C7 - - - |") });
    } else if (m_helpActiveTab == 1) {
        // --- タブ1: 数字接尾辞一覧 (罫線付きの表) ---
        DrawTextInBox(TR(L"度数の直後に入力してコード品質を指定します (Key: C の場合)。「入力」を打つと「結果」のコードになります。",
                         L"Type right after the degree number to set the chord quality (Key: C). The Input column becomes the Result chord."),
                      m_pFontInspector, D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f), m_pBrushTextMuted, 0.0f);
        textY += 28.0f;

        const float tblLeft = rc.left + pad;
        const float tblW = (rc.right - rc.left) - pad * 2.0f;
        const float colX[4] = { 0.0f, tblW * 0.22f, tblW * 0.60f, tblW * 0.78f };
        const float rowH = 26.0f;

        struct SuffixItem {
            const wchar_t* suffix;
            const wchar_t* quality;
            const wchar_t* input;
            const wchar_t* result;
        };
        const SuffixItem suffixes[] = {
            { L"mas / M7 / maj7", TR(L"メジャーセブンス (Major 7th)", L"Major 7th"), L"4mas", L"FMaj7" },
            { L"svn / 7", TR(L"ドミナントセブンス (Dominant 7th)", L"Dominant 7th"), L"5svn", L"G7" },
            { L"mis / m7", TR(L"マイナーセブンス (Minor 7th)", L"Minor 7th"), L"2mis", L"Dm7" },
            { L"m", TR(L"マイナー (借用和音など)", L"Minor (Modal Interchange, etc.)"), L"4m", L"Fm" },
            { L"hdm / m7b5", TR(L"ハーフディミニッシュ (m7b5)", L"Half-Diminished (m7b5)"), L"7hdm", L"Bm7b5" },
            { L"dim", TR(L"ディミニッシュ (Diminished)", L"Diminished triad"), L"7dim", L"Bdim" },
            { L"dim7", TR(L"ディミニッシュセブンス", L"Diminished 7th"), L"7dim7", L"Bdim7" },
            { L"suf / sus4", TR(L"サスフォー (sus4)", L"Suspended 4th (sus4)"), L"5suf", L"Gsus4" },
            { L"sut / sus2", TR(L"サスツー (sus2)", L"Suspended 2nd (sus2)"), L"1sut", L"Csus2" },
            { L"adn / add9", TR(L"アドナインス (add9)", L"Added 9th (add9)"), L"4adn", L"Fadd9" },
            { L"six / 6", TR(L"シックス (6th)", L"Major 6th (6th)"), L"1six", L"C6" }
        };
        const int sufCount = static_cast<int>(sizeof(suffixes) / sizeof(suffixes[0]));
        const int tblRows = sufCount + 1;
        const float firstRowY = textY;

        // 見出し行の塗り → 縞模様の塗り → 罫線 → 文字 の順に描く（罫線が塗りに隠れない）
        m_pRenderTarget->FillRectangle(D2D1::RectF(tblLeft, firstRowY, tblLeft + tblW, firstRowY + rowH), m_pBrushBorderLight);
        for (int si = 0; si < sufCount; ++si) {
            if (si % 2 == 1) {
                const float ry = firstRowY + rowH * static_cast<float>(si + 1);
                m_pRenderTarget->FillRectangle(D2D1::RectF(tblLeft, ry, tblLeft + tblW, ry + rowH), m_pBrushCardBg);
            }
        }
        drawTableGrid(tblLeft, firstRowY, tblW, rowH, tblRows, colX, 4);

        DrawTextInBox(TR(L"短縮接尾辞", L"Suffix"), m_pFontInspector, D2D1::RectF(tblLeft + colX[0], firstRowY, tblLeft + colX[1], firstRowY + rowH), m_pBrushText, 8.0f);
        DrawTextInBox(TR(L"コード品質 (Quality)", L"Chord Quality"), m_pFontInspector, D2D1::RectF(tblLeft + colX[1], firstRowY, tblLeft + colX[2], firstRowY + rowH), m_pBrushText, 8.0f);
        DrawTextInBox(TR(L"入力", L"Input"), m_pFontInspector, D2D1::RectF(tblLeft + colX[2], firstRowY, tblLeft + colX[3], firstRowY + rowH), m_pBrushText, 8.0f);
        DrawTextInBox(TR(L"結果", L"Result"), m_pFontInspector, D2D1::RectF(tblLeft + colX[3], firstRowY, tblLeft + tblW, firstRowY + rowH), m_pBrushText, 8.0f);

        for (int si = 0; si < sufCount; ++si) {
            const float ry = firstRowY + rowH * static_cast<float>(si + 1);
            DrawTextInBox(suffixes[si].suffix, m_pFontButton, D2D1::RectF(tblLeft + colX[0], ry, tblLeft + colX[1], ry + rowH), m_pBrushText, 8.0f);
            DrawTextInBox(suffixes[si].quality, m_pFontInspector, D2D1::RectF(tblLeft + colX[1], ry, tblLeft + colX[2], ry + rowH), m_pBrushText, 8.0f);
            DrawTextInBox(suffixes[si].input, m_pFontButton, D2D1::RectF(tblLeft + colX[2], ry, tblLeft + colX[3], ry + rowH), m_pBrushAccent, 8.0f);
            DrawTextInBox(suffixes[si].result, m_pFontButton, D2D1::RectF(tblLeft + colX[3], ry, tblLeft + tblW, ry + rowH), m_pBrushAccent, 8.0f);
        }
        textY = firstRowY + rowH * static_cast<float>(tblRows) + 6.0f;
        DrawTextInBox(TR(L"接尾辞を省略した度数 (例: 1 5 6 4) は、キーのダイアトニックコードになります。",
                         L"A degree without a suffix (e.g. 1 5 6 4) becomes the diatonic chord of the key."),
                      m_pFontInspector, D2D1::RectF(tblLeft, textY, tblLeft + tblW, textY + 20.0f), m_pBrushTextMuted, 0.0f);
    } else if (m_helpActiveTab == 2) {
        // --- タブ2: コード記号（通常コード名入力で使える表記） ---
        D2D1_RECT_F gHead = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f);
        DrawTextSafe(TR(L"通常コード名入力で使える表記です (ルートを C とした例)。数字接尾辞はこの上位互換として併用できます。", L"Notation usable in standard chord symbols (examples with root C). Numeric suffixes can be mixed in."), m_pFontInspector, gHead, m_pBrushTextMuted);
        textY += 26.0f;

        float col1W = 170.0f;
        float col2W = 270.0f;
        D2D1_RECT_F thRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 24.0f);
        m_pRenderTarget->FillRectangle(thRc, m_pBrushBorderLight);
        DrawTextSafe(TR(L"記号", L"Symbol"), m_pFontInspector, D2D1::RectF(thRc.left + 8.0f, textY + 2.0f, thRc.left + col1W, textY + 22.0f), m_pBrushText);
        DrawTextSafe(TR(L"意味", L"Meaning"), m_pFontInspector, D2D1::RectF(thRc.left + col1W + 8.0f, textY + 2.0f, thRc.left + col1W + col2W, textY + 22.0f), m_pBrushText);
        DrawTextSafe(TR(L"入力例", L"Example"), m_pFontInspector, D2D1::RectF(thRc.left + col1W + col2W + 8.0f, textY + 2.0f, thRc.right - 8.0f, textY + 22.0f), m_pBrushText);
        textY += 26.0f;

        struct SymbolItem {
            const wchar_t* symbol;
            const wchar_t* meaning;
            const wchar_t* example;
        };
        SymbolItem symbols[] = {
            { L"(なし) / m", TR(L"メジャー / マイナー", L"Major / Minor"), L"C   Cm" },
            { L"7 / maj7 (M7, ^7)", TR(L"ドミナント7th / メジャー7th", L"Dominant 7th / Major 7th"), L"C7   Cmaj7   CM7" },
            { L"m7 / mM7", TR(L"マイナー7th / マイナーメジャー7th", L"Minor 7th / Minor-Major 7th"), L"Cm7   CmM7" },
            { L"m7b5 (m7-5, hdm)", TR(L"ハーフディミニッシュ", L"Half-Diminished"), L"Cm7b5   Chdm" },
            { L"dim7 (o7)", TR(L"ディミニッシュ7th", L"Diminished 7th"), L"Cdim7   Co7" },
            { L"aug (+) / 7#5",TR(L"オーギュメント / オーギュメント7th", L"Augmented / Augmented 7th"), L"Caug   C+   C7#5" },
            { L"sus2 / sus4", TR(L"サス2 / サス4", L"Suspended 2nd / 4th"), L"Csus2   Csus4" },
            { L"7sus4 (7sus)", TR(L"ドミナント7th サス4", L"Dominant 7th sus4"), L"C7sus4" },
            { L"add9", TR(L"アド9 (7thなし)", L"Added 9th (no 7th)"), L"Cadd9" },
            { L"6 / m6", TR(L"シックス / マイナーシックス", L"Major 6th / Minor 6th"), L"C6   Cm6" },
            { L"( ) テンション", TR(L"テンション追加 (9 b9 #9 11 #11 13 b13)", L"Tensions: 9 b9 #9 11 #11 13 b13"), L"C7(b9)   Fmaj7(9)   G7(#9,13)" },
            { L"/ (スラッシュ)", TR(L"分数コード: 指定した音をベースにする", L"Slash chord: use the given note as the bass"), L"C/E   Am7/G   F/C" },
            { L"# / b", TR(L"ルートの半音上げ / 下げ", L"Sharp / Flat root"), L"F#m7   Bbmaj7   Db" },
            { L"R / N.C.", TR(L"休符", L"Rest"), L"| C R | N.C. |" }
        };

        for (size_t si = 0; si < sizeof(symbols) / sizeof(symbols[0]); ++si) {
            D2D1_RECT_F trRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f);
            if (si % 2 == 1) {
                m_pRenderTarget->FillRectangle(trRc, m_pBrushCardBg);
            }
            DrawTextSafe(symbols[si].symbol, m_pFontButton, D2D1::RectF(trRc.left + 8.0f, textY + 2.0f, trRc.left + col1W, textY + 20.0f), m_pBrushText);
            DrawTextSafe(symbols[si].meaning, m_pFontInspector, D2D1::RectF(trRc.left + col1W + 8.0f, textY + 2.0f, trRc.left + col1W + col2W, textY + 20.0f), m_pBrushText);
            DrawTextSafe(symbols[si].example, m_pFontInspector, D2D1::RectF(trRc.left + col1W + col2W + 8.0f, textY + 2.0f, trRc.right - 8.0f, textY + 20.0f), m_pBrushAccent);
            textY += 23.0f;
        }
    } else if (m_helpActiveTab == 3) {
        // --- タブ3: キーボード・修飾キー操作（カテゴリ別に表示。キーは kbd 風のキーキャップ） ---
        struct ShortcutRow {
            int category;          // 0=再生 1=選択 2=編集 3=ファイル 4=表示
            const wchar_t* key;    // [ ] で囲んだ部分がキーキャップ
            const wchar_t* func;
        };
        const wchar_t* kbCategories[] = {
            TR(L"再生", L"Playback"), TR(L"選択・移動", L"Select / Move"), TR(L"編集", L"Edit"),
            TR(L"ファイル", L"File"), TR(L"表示", L"View")
        };
        const int kbCategoryCount = static_cast<int>(sizeof(kbCategories) / sizeof(kbCategories[0]));
        const ShortcutRow sRows[] = {
            { 0, L"[Space]", TR(L"再生 / 一時停止", L"Play / Pause") },
            { 0, L"[Enter]", TR(L"再生位置を先頭へ戻す", L"Return the playhead to the start") },
            { 0, L"[Esc]", TR(L"停止 / 選択解除 / ダイアログを閉じる", L"Stop / Clear selection / Close dialog") },
            { 0, L"[L]", TR(L"ループ再生の ON / OFF", L"Toggle loop playback") },

            { 1, L"[←] / [→]", TR(L"前 / 次のコードを選択して試聴", L"Select previous / next chord") },
            { 1, L"[Ctrl] + クリック", TR(L"カードを選択に追加 / 除外 (複数選択)", L"Add / remove a card to the selection") },
            { 1, L"[Shift] + クリック", TR(L"最後に選択したカードから範囲選択", L"Select a range from the last selected card") },
            { 1, L"[Ctrl] + [A]", TR(L"すべてのコードを選択", L"Select all chords") },
            { 1, L"[↑] / [↓]", TR(L"転回形を切替 (基本形→第1→第2→第3)", L"Cycle inversion (Root > 1st > 2nd > 3rd)") },
            { 1, L"[Ctrl] / [Shift] + [↑] / [↓]", TR(L"1オクターブ上 / 下へ移動", L"Move up / down one octave") },

            { 2, L"[Ctrl] + [Enter]", TR(L"入力欄のコード進行をタイムラインへ反映", L"Apply the text box to the timeline") },
            { 2, L"[Ctrl] + [Z]", TR(L"元に戻す (Undo)", L"Undo") },
            { 2, L"[Ctrl] + [Y]", TR(L"やり直し (Redo)", L"Redo") },
            { 2, L"[Ctrl] + [C]", TR(L"コピー", L"Copy") },
            { 2, L"[Ctrl] + [X]", TR(L"切り取り", L"Cut") },
            { 2, L"[Ctrl] + [V]", TR(L"貼り付け (再生ヘッド最寄りのグリッドへ)", L"Paste (at the grid nearest the playhead)") },
            { 2, L"[Ctrl] + [D]", TR(L"選択中のコードカードを複製", L"Duplicate the selected card") },
            { 2, L"[Delete] / [Backspace]", TR(L"選択中のコードカードを削除", L"Delete the selected card") },
            { 2, L"[B]", TR(L"選択中のコードカードを分割", L"Split the selected card") },
            { 2, L"[M]", TR(L"再生ヘッド位置にマーカーを追加", L"Add a marker at the playhead") },

            { 3, L"[Ctrl] + [N]", TR(L"新規作成", L"New project") },
            { 3, L"[Ctrl] + [O]", TR(L"プロジェクトを開く (.chord)", L"Open project (.chord)") },
            { 3, L"[Ctrl] + [S]", TR(L"プロジェクトを保存", L"Save project") },
            { 3, L"[Ctrl] + [Shift] + [S]", TR(L"名前を付けて保存", L"Save As") },
            { 3, L"[Ctrl] + [E]", TR(L"標準 MIDI ファイル (.mid) を書き出し", L"Export Standard MIDI File (.mid)") },

            { 4, L"[Ctrl] + [T]", TR(L"テンプレート集を開く", L"Open templates") },
            { 4, L"[Ctrl] + [H]", TR(L"このヘルプを開く", L"Open this help") },
            { 4, L"[Ctrl] + [+]", TR(L"タイムラインの横ズームイン", L"Zoom timeline in") },
            { 4, L"[Ctrl] + [-]", TR(L"タイムラインの横ズームアウト", L"Zoom timeline out") },
            { 4, L"[Ctrl] + [0]", TR(L"横ズームを標準に戻す", L"Reset timeline zoom") },
            { 4, L"[Ctrl] + [1] ~ [5]", TR(L"タイムライングリッド切替", L"Timeline grid preset") },
            { 4, L"[Ctrl] + [Shift] + [1] ~ [5]", TR(L"入力グリッド切替", L"Input grid preset") }
        };

        // カテゴリ切替チップ（1画面に出す情報量を絞る）
        const float chipW = 118.0f;
        const float chipH = 28.0f;
        for (int ci = 0; ci < kbCategoryCount; ++ci) {
            const float cx = rc.left + pad + static_cast<float>(ci) * (chipW + 6.0f);
            DrawButton(D2D1::RectF(cx, textY, cx + chipW, textY + chipH), kbCategories[ci], IconType::None, false, false, m_helpKbCategory == ci);
        }
        textY += chipH + 10.0f;

        const float col1W = 300.0f;
        const float rowH = 30.0f;
        int shown = 0;
        for (const auto& row : sRows) {
            if (row.category != m_helpKbCategory) continue;
            D2D1_RECT_F trRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + rowH);
            if (shown % 2 == 1) {
                m_pRenderTarget->FillRectangle(trRc, m_pBrushCardBg);
            }
            DrawKeyCombo(row.key, trRc.left + 10.0f, textY + rowH * 0.5f, trRc.left + col1W, true);
            DrawTextInBox(row.func, m_pFontHelp, D2D1::RectF(trRc.left + col1W, textY, trRc.right, textY + rowH), m_pBrushText, 8.0f);
            textY += rowH;
            ++shown;
        }
    } else {
        // --- タブ4: マウス操作・画面ツール一覧（カテゴリ別に表示。キーボードタブと同じ構成） ---
        struct MouseRow {
            int category;         // 0=カード 1=ピアノロール 2=ホイール 3=画面ツール 4=リンク
            const wchar_t* key;   // [ ] で囲んだ部分はキーキャップ。それ以外は操作名
            const wchar_t* func;
        };
        const wchar_t* msCategories[] = {
            TR(L"カード", L"Cards"), TR(L"ピアノロール", L"Piano roll"), TR(L"ホイール", L"Wheel"),
            TR(L"画面ツール", L"Screen tools"), TR(L"リンク", L"Link")
        };
        const int msCategoryCount = static_cast<int>(sizeof(msCategories) / sizeof(msCategories[0]));
        const MouseRow mRows[] = {
            { 0, TR(L"カード クリック", L"Click card"), TR(L"コードを選択", L"Select the chord") },
            { 0, TR(L"[Ctrl] + クリック", L"[Ctrl] + Click"), TR(L"選択に追加 / 除外 (複数選択)", L"Add / remove a card to the selection") },
            { 0, TR(L"[Shift] + クリック", L"[Shift] + Click"), TR(L"最後に選択したカードから範囲選択", L"Select a range from the last card") },
            { 0, TR(L"カード本体 ドラッグ", L"Drag card body"), TR(L"移動 (小節またぎ可・グリッドにスナップ)", L"Move (crosses bars, snaps to grid)") },
            { 0, TR(L"カード左右端 ドラッグ", L"Drag card edge"), TR(L"音価(長さ)を伸縮 (小節線またぎ可)", L"Resize duration (crosses bar lines)") },
            { 0, TR(L"カード 右クリック", L"Right-click card"), TR(L"転回形・ベース音・オクターブのメニュー", L"Menu: inversion, bass note, octave") },
            { 0, TR(L"提案カード ドラッグ", L"Drag palette card"), TR(L"左右ペインのコードをタイムラインへ追加", L"Add a suggested chord to the timeline") },

            { 1, TR(L"音符 クリック", L"Click note"), TR(L"コードカードと音符を同時に選択", L"Select the chord card and note together") },
            { 1, TR(L"音符 ドラッグ", L"Drag note"), TR(L"構成音を1オクターブ単位で上下移動", L"Move a chord tone in whole octaves") },
            { 1, TR(L"境界バー ドラッグ", L"Drag divider"), TR(L"カードとピアノロールの表示比率を変更", L"Resize cards vs. piano roll") },
            { 1, TR(L"境界バー ダブルクリック", L"Double-click divider"), TR(L"表示比率を初期値に戻す", L"Reset the ratio") },

            { 2, TR(L"ホイール", L"Wheel"), TR(L"タイムラインを水平スクロール", L"Scroll timeline horizontally") },
            { 2, TR(L"[Ctrl] + ホイール", L"[Ctrl] + Wheel"), TR(L"横ズーム (カーソル位置を中心に拡大縮小)", L"Zoom horizontally (around the cursor)") },
            { 2, TR(L"[Ctrl] + [Shift] + ホイール", L"[Ctrl] + [Shift] + Wheel"), TR(L"ピアノロールの縦ズーム (ピアノロール上)", L"Zoom piano roll vertically (over it)") },
            { 2, TR(L"[Shift] + ホイール", L"[Shift] + Wheel"), TR(L"縦ズーム中のピアノロールを上下スクロール", L"Scroll the zoomed piano roll up / down") },

            { 3, TR(L"ルーラー クリック/ドラッグ", L"Ruler click / drag"), TR(L"再生位置を移動 (シーク)", L"Seek the playhead") },
            { 3, TR(L"マーカー 右クリック", L"Right-click marker"), TR(L"編集・削除 (追加は M キー)", L"Edit / delete (add with M)") },
            { 3, TR(L"パートタブ 右クリック", L"Right-click part tab"), TR(L"複製・削除・左右移動・名前変更", L"Duplicate, delete, move, rename") },
            { 3, TR(L"上段 新規/開く/保存", L"Top-left icons"), TR(L"新規作成 / 開く / 保存 (.chord)", L"New / Open / Save (.chord)") },
            { 3, TR(L"MIDI保存 / DAWへドラッグ", L"Save MIDI / Drag to DAW"), TR(L"MIDI出力 / DAWへ直接ドラッグ", L"Export MIDI / drag into a DAW") },
            { 3, TR(L"ボタン・カード・音符にカーソル", L"Hover buttons / cards / notes"), TR(L"上段のTipsエリアに操作を表示", L"Shows a hint in the top tips area") },

            { 4, TR(L"キー/スケール/BPM 変更", L"Change Key/Scale/BPM"), TR(L"変更前と同じ値のパートへ一括反映 (リンクON時)", L"Applies to parts sharing the old value (link ON)") },
            { 4, TR(L"リンクボタン (下段右端)", L"Link button (bottom right)"), TR(L"ON: 同じ値のパートすべて / OFF: 表示中のみ", L"ON: all parts sharing the value / OFF: active only") },
            { 4, TR(L"[Ctrl] を押しながら変更", L"Hold [Ctrl] while changing"), TR(L"その操作だけ ON/OFF を反転 (ボタン点灯も反転)", L"Inverts link for that change (lamp inverts too)") }
        };

        // カテゴリ切替チップ（キーボードタブと同じ座標。クリック判定は OnModalClick と共通）
        const float chipW = 118.0f;
        const float chipH = 28.0f;
        for (int ci = 0; ci < msCategoryCount; ++ci) {
            const float cx = rc.left + pad + static_cast<float>(ci) * (chipW + 6.0f);
            DrawButton(D2D1::RectF(cx, textY, cx + chipW, textY + chipH), msCategories[ci], IconType::None, false, false, m_helpMouseCategory == ci);
        }
        textY += chipH + 10.0f;

        const float col1W = 270.0f;
        const float rowH = 30.0f;
        int shown = 0;
        for (const auto& row : mRows) {
            if (row.category != m_helpMouseCategory) continue;
            D2D1_RECT_F trRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + rowH);
            if (shown % 2 == 1) {
                m_pRenderTarget->FillRectangle(trRc, m_pBrushCardBg);
            }
            if (std::wcschr(row.key, L'[') != nullptr) {
                DrawKeyCombo(row.key, trRc.left + 10.0f, textY + rowH * 0.5f, trRc.left + col1W, true);
            } else {
                DrawTextInBox(row.key, m_pFontHelp, D2D1::RectF(trRc.left, textY, trRc.left + col1W, textY + rowH), m_pBrushAccent, 10.0f);
            }
            DrawTextInBox(row.func, m_pFontHelp, D2D1::RectF(trRc.left + col1W, textY, trRc.right, textY + rowH), m_pBrushText, 8.0f);
            textY += rowH;
            ++shown;
        }
    }

    // 下部アクションボタン
    D2D1_RECT_F bClose = D2D1::RectF(rc.right - 100.0f, rc.bottom - 36.0f, rc.right, rc.bottom);
    DrawButton(bClose, TR(L"閉じる", L"Close"), IconType::None);
}

// ---------------------------------------------------------------------------
// タップテンポ測定モーダル描画
// ---------------------------------------------------------------------------
void AppWindow::RenderTapTempoModal(const D2D1_RECT_F& rc) {
    // 巨大なBPM表示
    D2D1_RECT_F bpmRc = D2D1::RectF(rc.left, rc.top + 10.0f, rc.right, rc.top + 55.0f);
    std::wstring bpmStr = L"BPM: " + std::to_wstring(m_measuredBpm);
    DrawTextSafe(bpmStr, m_pFontBigNumber, bpmRc, m_pBrushAccent);

    // 説明案内
    D2D1_RECT_F guideRc = D2D1::RectF(rc.left, rc.top + 60.0f, rc.right, rc.top + 80.0f);
    DrawTextSafe(TR(L"マウスクリック または Tキー を一定のリズムでタップしてください", L"Click the button or press [T] repeatedly at a steady rhythm"), m_pFontInspector, guideRc, m_pBrushTextMuted);

    // タップボタン (巨大な角丸ブロック、点灯アニメーション)
    float tapBtnW = 280.0f;
    float tapBtnH = 90.0f;
    float tapX = (rc.left + rc.right - tapBtnW) * 0.5f;
    float tapY = rc.top + 95.0f;
    D2D1_RECT_F tapRc = D2D1::RectF(tapX, tapY, tapX + tapBtnW, tapY + tapBtnH);

    D2D1_COLOR_F tapBg = m_tapBlinked ? Theme::AccentWarm : Theme::CardActive;
    ID2D1SolidColorBrush* pTapBrush = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(tapBg, &pTapBrush);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(tapRc, 10.0f, 10.0f), pTapBrush);
    pTapBrush->Release();

    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(tapRc, 10.0f, 10.0f), m_pBrushAccent, 2.0f);

    D2D1_RECT_F tapTextRc = D2D1::RectF(tapX, tapY + 28.0f, tapX + tapBtnW, tapY + 62.0f);
    DrawTextSafe(m_tapBlinked ? L"♪ TAP! ♪" : TR(L"ここをクリック または [T] キー", L"Click Here or Press [T]"), m_pFontCardTitle, tapTextRc, m_tapBlinked ? m_pBrushBg : m_pBrushText);

    // 下部アクションボタン (OK / キャンセル)
    D2D1_RECT_F bOk = D2D1::RectF(rc.right - 140.0f, rc.bottom - 36.0f, rc.right - 10.0f, rc.bottom);
    DrawButton(bOk, TR(L"OK (確定: Enter)", L"OK (Enter)"), IconType::None, false, false, true, Theme::AccentAction);

    D2D1_RECT_F bCancel = D2D1::RectF(rc.right - 250.0f, rc.bottom - 36.0f, rc.right - 150.0f, rc.bottom);
    DrawButton(bCancel, TR(L"キャンセル (Esc)", L"Cancel (Esc)"), IconType::None);
}

void AppWindow::RenderRenamePartModal(const D2D1_RECT_F& rc) {
    float curY = rc.top + 10.0f;

    D2D1_RECT_F lTitle = D2D1::RectF(rc.left + 16.0f, curY, rc.right - 16.0f, curY + 24.0f);
    if (m_renameMarkerTick > 0) {
        std::wstring prompt = TR(L"マーカー名を入力してください（空欄で解除）:", L"Enter marker name (empty to remove):");
        DrawTextSafe(prompt, m_pFontCardTitle, lTitle, m_pBrushText);
    } else {
        DrawTextSafe(TR(L"パートの新しい名称を入力してください:", L"Enter new name for the part:"), m_pFontCardTitle, lTitle, m_pBrushText);
    }

    curY += 34.0f;

    D2D1_RECT_F boxRc = D2D1::RectF(rc.left + 16.0f, curY, rc.right - 16.0f, curY + 44.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxRc, 4.0f, 4.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxRc, 4.0f, 4.0f), m_pBrushAccent, 1.5f);

    std::wstring displayText = m_renamePartName + L"|";
    DrawTextInBox(displayText, m_pFontModalTitle, boxRc, m_pBrushText, 12.0f);

    D2D1_RECT_F bConfirm = D2D1::RectF(rc.right - 140.0f, rc.bottom - 36.0f, rc.right - 10.0f, rc.bottom);
    DrawButton(bConfirm, TR(L"変更を確定", L"Confirm"), IconType::None, false, false, true, Theme::AccentAction);

    D2D1_RECT_F bCancel = D2D1::RectF(rc.right - 250.0f, rc.bottom - 36.0f, rc.right - 150.0f, rc.bottom);
    DrawButton(bCancel, TR(L"キャンセル", L"Cancel"), IconType::None);
}

// ---------------------------------------------------------------------------
// タップテンポ計算ロジック
// ---------------------------------------------------------------------------
void AppWindow::HandleTapTempo() {
    auto now = std::chrono::steady_clock::now();
    m_tapBlinked = true;
    m_lastTapBlinkTime = now;

    // 前回のタップから2秒以上空いた場合はリセット
    if (!m_tapTimes.empty()) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_tapTimes.back()).count();
        if (diff > 2000) {
            m_tapTimes.clear();
        }
    }

    m_tapTimes.push_back(now);
    if (m_tapTimes.size() > 8) {
        m_tapTimes.erase(m_tapTimes.begin());
    }

    if (m_tapTimes.size() >= 2) {
        double totalSec = 0.0;
        for (size_t i = 1; i < m_tapTimes.size(); ++i) {
            totalSec += std::chrono::duration<double>(m_tapTimes[i] - m_tapTimes[i - 1]).count();
        }
        double avgSec = totalSec / (m_tapTimes.size() - 1);
        if (avgSec > 0.0) {
            int bpm = static_cast<int>(std::round(60.0 / avgSec));
            m_measuredBpm = std::clamp(bpm, 40, 280);
        }
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ConfirmTapTempo() {
    Part* pActive = m_partManager.GetActivePart();
    if (pActive) {
        const bool linked = IsPartLinkActive();
        m_partManager.ApplyBpm(m_measuredBpm, linked);
    }
    CloseModal();
}

// ---------------------------------------------------------------------------
// 各種プルダウンメニューの表示 (TrackPopupMenu)
// ---------------------------------------------------------------------------
void AppWindow::ShowKeyMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const char* keys[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    Part* pActive = m_partManager.GetActivePart();
    std::string curKey = pActive ? pActive->keyRoot : "C";

    for (int i = 0; i < 12; ++i) {
        UINT flags = MF_STRING;
        if (keys[i] == curKey) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_KEY_BASE + i, ToWString(keys[i]).c_str());
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowScaleMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* modeNames[] = {
        TR(L"メジャー (Major / Ionian)", L"Major (Ionian)"),
        TR(L"ナチュラルマイナー (Natural Minor / Aeolian)", L"Natural Minor (Aeolian)"),
        TR(L"ハーモニックマイナー (Harmonic Minor)", L"Harmonic Minor"),
        TR(L"メロディックマイナー (Melodic Minor)", L"Melodic Minor"),
        TR(L"ドリアン (Dorian)", L"Dorian"),
        TR(L"フリジアン (Phrygian)", L"Phrygian"),
        TR(L"リディアン (Lydian)", L"Lydian"),
        TR(L"ミクソリディアン (Mixolydian)", L"Mixolydian"),
        TR(L"ロクリアン (Locrian)", L"Locrian")
    };
    Part* pActive = m_partManager.GetActivePart();
    int curMode = pActive ? static_cast<int>(pActive->mode) : 0;

    for (int i = 0; i < 9; ++i) {
        UINT flags = MF_STRING;
        if (i == curMode) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_SCALE_BASE + i, modeNames[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowPresetMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* presetNames[] = {
        TR(L"グランドピアノ (Grand Piano)", L"Grand Piano"),
        TR(L"ウォームパッド (Warm Pad)", L"Warm Pad"),
        TR(L"エレクトリックピアノ (Electric Piano)", L"Electric Piano"),
        TR(L"シンセストリングス (Synth Strings)", L"Synth Strings")
    };
    int curP = static_cast<int>(m_synth.GetPreset());
    for (int i = 0; i < 4; ++i) {
        UINT flags = MF_STRING;
        if (i == curP) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_PRESET_BASE + i, presetNames[i]);
    }
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowVoicingMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* voicingNames[] = {
        TR(L"基本形 (Root Position)", L"Root Position"),
        TR(L"Drop 2 (ジャズ・ポップス定番)", L"Drop 2 (Jazz / Pop Standard)"),
        L"Drop 3",
        TR(L"Open Spread (広がり重視)", L"Open Spread (Wide)"),
        TR(L"Closed (密集配置)", L"Closed Voicing")
    };
    int curV = static_cast<int>(m_selectedVoicing);

    for (int i = 0; i < 5; ++i) {
        UINT flags = MF_STRING;
        if (i == curV) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_VOICING_BASE + i, voicingNames[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowBassMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* bassNames[] = {
        TR(L"ベース: なし", L"Bass: Off"),
        TR(L"ベース: -1 Oct (標準ベース付加)", L"Bass: -1 Oct (Standard Bass)"),
        TR(L"ベース: -2 Oct (サブベース付加)", L"Bass: -2 Oct (Sub Bass)"),
        TR(L"ベース: 両方 (-1 & -2 Oct)", L"Bass: Both (-1 & -2 Oct)")
    };

    for (int i = 0; i < 4; ++i) {
        UINT flags = MF_STRING;
        if (i == m_bassOctaveMode) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_BASS_BASE + i, bassNames[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowGridMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* gridNames[] = {
        TR(L"1小節", L"1 Bar"),
        TR(L"2拍", L"2 Beats"),
        TR(L"1拍", L"1 Beat"),
        TR(L"1/2拍", L"1/2 Beat"),
        TR(L"1/4拍", L"1/4 Beat")
    };
    int grids[] = { TICKS_PER_BAR_4_4, TICKS_PER_BEAT * 2, TICKS_PER_BEAT, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4 };

    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, TR(L"タイムライングリッド (表示と移動/伸縮スナップ)", L"Timeline Grid (Display & Editing Snap)"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < 5; ++i) {
        UINT flags = MF_STRING;
        if (m_timelineGridTicks == grids[i]) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_GRID_BASE + i, gridNames[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowDiatonicVariantMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    // ダイアトニックタブ 2〜8 に対応（順序は MusicEngine::GetDiatonicChordsByTab と一致させる）
    const wchar_t* names[] = {
        L"sus4",
        L"add9",
        TR(L"分数コード (オンコード)", L"Slash Chords"),
        L"sus2",
        L"6 / m6",
        L"7sus4",
        TR(L"add9 / madd9 (調性準拠)", L"add9 / madd9 (key-aware)")
    };
    for (int i = 0; i < 7; ++i) {
        UINT flags = MF_STRING;
        if (m_diatonicChordTypeTab == 2 + i) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_DIA_VARIANT_BASE + i, names[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ShowInputGridMenu(float screenX, float screenY) {
    HMENU hMenu = CreatePopupMenu();
    const wchar_t* gridNames[] = {
        TR(L"1小節", L"1 Bar"),
        TR(L"2拍", L"2 Beats"),
        TR(L"1拍", L"1 Beat"),
        TR(L"1/2拍", L"1/2 Beat"),
        TR(L"1/4拍", L"1/4 Beat")
    };
    int grids[] = { TICKS_PER_BAR_4_4, TICKS_PER_BEAT * 2, TICKS_PER_BEAT, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4 };

    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, TR(L"入力グリッド (追加位置スナップと音価)", L"Input Grid (Placement & Length)"));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < 5; ++i) {
        UINT flags = MF_STRING;
        if (m_inputGridTicks == grids[i]) flags |= MF_CHECKED;
        AppendMenuW(hMenu, flags, IDM_INPUT_GRID_BASE + i, gridNames[i]);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, static_cast<int>(screenX), static_cast<int>(screenY), 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// イベント処理 (マウスクリック・ドラッグ・キーボード)
// ---------------------------------------------------------------------------
bool AppWindow::OnModalClick(float x, float y) {
    if (m_activeModal == ModalType::None) return false;

    float cardW = (m_activeModal == ModalType::TapTempo || m_activeModal == ModalType::RenamePart) ? 480.0f
                : ((m_activeModal == ModalType::About || m_activeModal == ModalType::Dashboard) ? std::min(560.0f, m_width - 40.0f) : std::min(740.0f, m_width - 40.0f));
    float cardH = (m_activeModal == ModalType::RenamePart) ? 240.0f
                : ((m_activeModal == ModalType::TapTempo) ? 320.0f
                : ((m_activeModal == ModalType::Dashboard) ? std::min(480.0f, m_height - 40.0f)
                : ((m_activeModal == ModalType::About) ? std::min(360.0f, m_height - 40.0f)
                : ((m_activeModal == ModalType::Help) ? std::min(600.0f, m_height - 40.0f) : std::min(520.0f, m_height - 40.0f)))));
    float cardX = (m_width - cardW) * 0.5f;
    float cardY = (m_height - cardH) * 0.5f;

    D2D1_RECT_F closeRc = D2D1::RectF(cardX + cardW - 36.0f, cardY + 8.0f, cardX + cardW - 8.0f, cardY + 36.0f);
    if (PtInRectF(closeRc, x, y)) {
        CloseModal();
        return true;
    }

    if (!PtInRectF(D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH), x, y)) {
        CloseModal();
        return true;
    }

    D2D1_RECT_F bodyRc = D2D1::RectF(cardX + 16.0f, cardY + 44.0f + 12.0f, cardX + cardW - 16.0f, cardY + cardH - 16.0f);

    switch (m_activeModal) {
        case ModalType::TapTempo: {
            float tapBtnW = 280.0f;
            float tapBtnH = 90.0f;
            float tapX = (bodyRc.left + bodyRc.right - tapBtnW) * 0.5f;
            float tapY = bodyRc.top + 95.0f;
            D2D1_RECT_F tapRc = D2D1::RectF(tapX, tapY, tapX + tapBtnW, tapY + tapBtnH);

            if (PtInRectF(tapRc, x, y)) {
                HandleTapTempo();
                return true;
            }

            D2D1_RECT_F bOk = D2D1::RectF(bodyRc.right - 140.0f, bodyRc.bottom - 36.0f, bodyRc.right - 10.0f, bodyRc.bottom);
            if (PtInRectF(bOk, x, y)) {
                ConfirmTapTempo();
                return true;
            }

            D2D1_RECT_F bCancel = D2D1::RectF(bodyRc.right - 250.0f, bodyRc.bottom - 36.0f, bodyRc.right - 150.0f, bodyRc.bottom);
            if (PtInRectF(bCancel, x, y)) {
                CloseModal();
                return true;
            }
            break;
        }
        case ModalType::Dashboard: {
            DashboardLayout L = GetDashboardLayout(bodyRc);

            // 1. 新規プロジェクト (名前入力 / 保存先参照 / 作成)
            if (PtInRectF(L.nameBox, x, y)) {
                m_dashNameFocused = true;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }
            if (PtInRectF(L.createBtn, x, y)) {
                if (CreateProjectFromDashboard()) {
                    CloseModal();
                } else {
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
                return true;
            }
            if (PtInRectF(L.browseBtn, x, y) || PtInRectF(L.dirBox, x, y)) {
                ChooseDirectory(m_projectDirectory, TR(L"プロジェクト保存先フォルダを選択", L"Select Project Directory"));
                SaveAppSettings();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            // 2. ファイルを開く
            if (PtInRectF(L.openFileBtn, x, y)) {
                DoFileOpen();
                if (!m_currentProjectPath.empty()) {
                    CloseModal();
                }
                return true;
            }

            // 3. 最近開いたファイル一覧 (単クリックは選択のみ。ダブルクリックで開く)
            m_dashNameFocused = false;
            int hit = HitTestDashboardRecent(x, y);
            if (hit >= 0) {
                m_dashboardSelectedRecentIndex = hit;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            // 4. フッター
            if (PtInRectF(L.checkbox, x, y)) {
                m_showDashboardOnStartup = !m_showDashboardOnStartup;
                SaveAppSettings();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            if (m_dashboardSelectedRecentIndex >= 0 && m_dashboardSelectedRecentIndex < static_cast<int>(m_recentFiles.size())) {
                if (PtInRectF(L.openBtn, x, y)) {
                    OpenRecentFile(m_dashboardSelectedRecentIndex);
                    CloseModal();
                    return true;
                }
            }

            if (PtInRectF(L.closeBtn, x, y)) {
                CloseModal();
                return true;
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
            break;
        }
        case ModalType::Settings: {
            float curY = bodyRc.top;

            // 1. 言語
            D2D1_RECT_F bJa = D2D1::RectF(bodyRc.right - 230.0f, curY + 12.0f, bodyRc.right - 120.0f, curY + 42.0f);
            if (PtInRectF(bJa, x, y)) {
                m_settingLanguage = 0;
                I18n::SetLanguage(Language::Japanese);
                RebuildMainMenu();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }
            D2D1_RECT_F bEn = D2D1::RectF(bodyRc.right - 114.0f, curY + 12.0f, bodyRc.right - 16.0f, curY + 42.0f);
            if (PtInRectF(bEn, x, y)) {
                m_settingLanguage = 1;
                I18n::SetLanguage(Language::English);
                RebuildMainMenu();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            curY += 62.0f;

            // 2. ズームスタイル
            float zBtnW = (bodyRc.right - bodyRc.left - 28.0f - 18.0f) / 4.0f;
            float zBtnH = 30.0f;
            float zBtnY = curY + 34.0f;
            for (int i = 0; i < 4; ++i) {
                float zx = bodyRc.left + 14.0f + i * (zBtnW + 6.0f);
                if (PtInRectF(D2D1::RectF(zx, zBtnY, zx + zBtnW, zBtnY + zBtnH), x, y)) {
                    m_settingZoomStyle = i;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
            }

            curY += 84.0f;

            // 3. 起動時ダッシュボード表示設定
            D2D1_RECT_F dChk = D2D1::RectF(bodyRc.left + 14.0f, curY + 12.0f, bodyRc.right - 14.0f, curY + 34.0f);
            if (PtInRectF(dChk, x, y)) {
                m_showDashboardOnStartup = !m_showDashboardOnStartup;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            curY += 54.0f;

            // 4. プロジェクト保存先フォルダの参照
            D2D1_RECT_F bBrowseProj = D2D1::RectF(bodyRc.right - 88.0f, curY + 30.0f, bodyRc.right - 14.0f, curY + 54.0f);
            if (PtInRectF(bBrowseProj, x, y)) {
                ChooseDirectory(m_projectDirectory, TR(L"プロジェクト保存先フォルダを選択", L"Select Project Directory"));
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            curY += 70.0f;

            // 5. 自動バックアップ設定
            D2D1_RECT_F bakChk = D2D1::RectF(bodyRc.left + 14.0f, curY + 10.0f, bodyRc.right - 14.0f, curY + 30.0f);
            if (PtInRectF(bakChk, x, y)) {
                m_autoBackupInProjectDir = !m_autoBackupInProjectDir;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            if (!m_autoBackupInProjectDir) {
                D2D1_RECT_F bBrowseBak = D2D1::RectF(bodyRc.right - 88.0f, curY + 62.0f, bodyRc.right - 14.0f, curY + 86.0f);
                if (PtInRectF(bBrowseBak, x, y)) {
                    ChooseDirectory(m_autoBackupDirectory, TR(L"自動保存先フォルダを選択", L"Select Auto-Backup Directory"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
            }

            // 6. フッターボタン
            D2D1_RECT_F bSave = D2D1::RectF(bodyRc.right - 130.0f, bodyRc.bottom - 36.0f, bodyRc.right, bodyRc.bottom);
            if (PtInRectF(bSave, x, y)) {
                SaveAppSettings();
                CloseModal();
                return true;
            }

            D2D1_RECT_F bCancel = D2D1::RectF(bodyRc.right - 230.0f, bodyRc.bottom - 36.0f, bodyRc.right - 140.0f, bodyRc.bottom);
            if (PtInRectF(bCancel, x, y)) {
                LoadAppSettings();
                CloseModal();
                return true;
            }
            break;
        }
        case ModalType::Templates: {
            // サブフォーム (テンプレート追加・編集パネル) が開いている場合
            if (m_tmplEditorOpen) {
                float formW = 520.0f;
                float formH = 340.0f;
                float fx = (bodyRc.left + bodyRc.right - formW) * 0.5f;
                float fy = (bodyRc.top + bodyRc.bottom - formH) * 0.5f;

                // フォーカス切り替え
                D2D1_RECT_F boxName = D2D1::RectF(fx + 20.0f, fy + 70.0f, fx + formW - 20.0f, fy + 102.0f);
                if (PtInRectF(boxName, x, y)) {
                    m_tmplEditFocusField = 0;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
                D2D1_RECT_F boxProg = D2D1::RectF(fx + 20.0f, fy + 132.0f, fx + formW - 20.0f, fy + 164.0f);
                if (PtInRectF(boxProg, x, y)) {
                    m_tmplEditFocusField = 1;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
                D2D1_RECT_F boxDesc = D2D1::RectF(fx + 20.0f, fy + 194.0f, fx + formW - 20.0f, fy + 226.0f);
                if (PtInRectF(boxDesc, x, y)) {
                    m_tmplEditFocusField = 2;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }

                // 保存
                D2D1_RECT_F bFSave = D2D1::RectF(fx + formW - 140.0f, fy + formH - 46.0f, fx + formW - 20.0f, fy + formH - 14.0f);
                if (PtInRectF(bFSave, x, y)) {
                    SaveTemplateFromEditor();
                    return true;
                }

                // キャンセル
                D2D1_RECT_F bFCancel = D2D1::RectF(fx + formW - 250.0f, fy + formH - 46.0f, fx + formW - 150.0f, fy + formH - 14.0f);
                if (PtInRectF(bFCancel, x, y)) {
                    CloseTemplateEditor();
                    return true;
                }
                return true;
            }

            // --- テンプレート一覧通常画面 ---
            float catW = 190.0f;
            auto categories = MusicEngine::GetTemplateMajorCategories();
            float itemH = 34.0f;
            for (size_t i = 0; i < categories.size(); ++i) {
                float iy = bodyRc.top + 6.0f + i * (itemH + 4.0f);
                if (PtInRectF(D2D1::RectF(bodyRc.left + 6.0f, iy, bodyRc.left + catW - 6.0f, iy + itemH), x, y)) {
                    m_selectedTmplCategoryIndex = static_cast<int>(i);
                    m_selectedTmplIndex = 0;
                    m_tmplListScroll = 0;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
            }

            float topBarH = 32.0f;
            // + 新規追加
            D2D1_RECT_F bAddNew = D2D1::RectF(bodyRc.right - 230.0f, bodyRc.top, bodyRc.right - 120.0f, bodyRc.top + topBarH);
            if (PtInRectF(bAddNew, x, y)) {
                OpenTemplateEditor(false);
                return true;
            }

            // 進行を登録
            D2D1_RECT_F bAddCurr = D2D1::RectF(bodyRc.right - 114.0f, bodyRc.top, bodyRc.right, bodyRc.top + topBarH);
            if (PtInRectF(bAddCurr, x, y)) {
                OpenTemplateEditor(false);
                Part* pActive = m_partManager.GetActivePart();
                if (pActive) {
                    m_tmplEditProg = ToWString(pActive->rawProgressionText);
                }
                m_tmplEditName = TR(L"マイコード進行", L"My Progression");
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            std::string curCat = (m_selectedTmplCategoryIndex < static_cast<int>(categories.size())) ? categories[m_selectedTmplCategoryIndex] : "👤 ユーザー定義";
            auto tmpls = MusicEngine::GetTemplatesByCategory(curCat);
            float tItemH = 66.0f;
            float listBoxTop = bodyRc.top + topBarH + 6.0f;

            size_t startIdx = static_cast<size_t>(std::max(0, m_tmplListScroll));
            size_t endIdx = std::min<size_t>(tmpls.size(), startIdx + 5);
            for (size_t i = startIdx; i < endIdx; ++i) {
                size_t slot = i - startIdx;
                float ty = listBoxTop + 6.0f + slot * (tItemH + 6.0f);
                float btnY = ty + 17.0f;
                float btnH = 32.0f;
                float bRight = bodyRc.right - 12.0f;

                // 適用
                bRight -= 58.0f;
                D2D1_RECT_F bApply = D2D1::RectF(bRight, btnY, bRight + 56.0f, btnY + btnH);
                if (PtInRectF(bApply, x, y)) {
                    Part* pActive = m_partManager.GetActivePart();
                    if (pActive) {
                        PushUndo();
                        ApplyTemplateProgression(tmpls[i].progression);
                        SyncTimelineToText();
                        m_isDirty = true;
                        UpdateWindowTitle();
                        SetStatusMessage(std::wstring(TR(L"テンプレートを適用しました: ", L"Applied template: ")) + GetLocalizedTemplateName(tmpls[i], m_settingLanguage));
                    }
                    CloseModal();
                    return true;
                }

                // 試聴
                bRight -= 70.0f;
                D2D1_RECT_F bAudition = D2D1::RectF(bRight, btnY, bRight + 66.0f, btnY + btnH);
                if (PtInRectF(bAudition, x, y)) {
                    m_selectedTmplIndex = static_cast<int>(i);
                    AuditionTemplate(tmpls[i]);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }

                if (tmpls[i].isUserCustom) {
                    // 編集
                    bRight -= 48.0f;
                    D2D1_RECT_F bEdit = D2D1::RectF(bRight, btnY, bRight + 44.0f, btnY + btnH);
                    if (PtInRectF(bEdit, x, y)) {
                        m_selectedTmplIndex = static_cast<int>(i);
                        OpenTemplateEditor(true);
                        return true;
                    }

                    // 削除
                    bRight -= 48.0f;
                    D2D1_RECT_F bDel = D2D1::RectF(bRight, btnY, bRight + 44.0f, btnY + btnH);
                    if (PtInRectF(bDel, x, y)) {
                        m_selectedTmplIndex = static_cast<int>(i);
                        DeleteSelectedUserTemplate();
                        return true;
                    }
                }
            }

            D2D1_RECT_F bClose = D2D1::RectF(bodyRc.right - 100.0f, bodyRc.bottom - 36.0f, bodyRc.right, bodyRc.bottom);
            if (PtInRectF(bClose, x, y)) {
                CloseModal();
                return true;
            }
            break;
        }
        case ModalType::Help: {
            float tabW = 132.0f;
            float tabH = 32.0f;
            for (int i = 0; i < 5; ++i) {
                float tx = bodyRc.left + i * (tabW + 8.0f);
                if (PtInRectF(D2D1::RectF(tx, bodyRc.top, tx + tabW, bodyRc.top + tabH), x, y)) {
                    m_helpActiveTab = i;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
            }

            // キーボードタブのカテゴリ切替チップ（描画側 RenderHelpModal と同じ座標）
            if (m_helpActiveTab == 3 || m_helpActiveTab == 4) {
                const float chipY = bodyRc.top + tabH + 10.0f + 12.0f;
                for (int ci = 0; ci < 5; ++ci) {
                    const float cx = bodyRc.left + 16.0f + static_cast<float>(ci) * (118.0f + 6.0f);
                    if (PtInRectF(D2D1::RectF(cx, chipY, cx + 118.0f, chipY + 28.0f), x, y)) {
                        (m_helpActiveTab == 3 ? m_helpKbCategory : m_helpMouseCategory) = ci;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                        return true;
                    }
                }
            }

            D2D1_RECT_F bClose = D2D1::RectF(bodyRc.right - 100.0f, bodyRc.bottom - 36.0f, bodyRc.right, bodyRc.bottom);
            if (PtInRectF(bClose, x, y)) {
                CloseModal();
                return true;
            }
            break;
        }
        case ModalType::About: {
            D2D1_RECT_F bClose = D2D1::RectF(bodyRc.right - 100.0f, bodyRc.bottom - 36.0f, bodyRc.right, bodyRc.bottom);
            if (PtInRectF(bClose, x, y)) {
                CloseModal();
                return true;
            }
            break;
        }
        case ModalType::RenamePart: {
            D2D1_RECT_F bConfirm = D2D1::RectF(bodyRc.right - 140.0f, bodyRc.bottom - 36.0f, bodyRc.right - 10.0f, bodyRc.bottom);
            if (PtInRectF(bConfirm, x, y)) {
                CommitRenameModal();
                CloseModal();
                return true;
            }

            D2D1_RECT_F bCancel = D2D1::RectF(bodyRc.right - 250.0f, bodyRc.bottom - 36.0f, bodyRc.right - 150.0f, bodyRc.bottom);
            if (PtInRectF(bCancel, x, y)) {
                CloseModal();
                return true;
            }
            break;
        }
        default: break;
    }
    return true;
}
} // namespace clp
