#include "app_window_internal.hpp"
namespace clp {
// ---------------------------------------------------------------------------
// 6. モーダルダイアログ描画 (Settings / Templates / Help / TapTempo)
// ---------------------------------------------------------------------------
void AppWindow::RenderModal(float width, float height) {
    D2D1_RECT_F fullRc = D2D1::RectF(0, 0, width, height);
    m_pRenderTarget->FillRectangle(fullRc, m_pBrushModalBackdrop);

    float cardW = (m_activeModal == ModalType::TapTempo || m_activeModal == ModalType::RenamePart) ? 480.0f : std::min(740.0f, width - 40.0f);
    float cardH = (m_activeModal == ModalType::RenamePart) ? 240.0f : ((m_activeModal == ModalType::TapTempo) ? 320.0f : std::min(520.0f, height - 40.0f));
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
            DrawTextSafe(TR(L"パート名の変更 (Rename Part)", L"Rename Part"), m_pFontModalTitle, titleRc, m_pBrushText);
            RenderRenamePartModal(bodyRc);
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

void AppWindow::RenderDashboardModal(const D2D1_RECT_F& rc) {
    float curY = rc.top;

    // 1. 上部ボタングループ (新規作成 / 開く)
    float btnW = (rc.right - rc.left - 16.0f) * 0.5f;
    D2D1_RECT_F bNew = D2D1::RectF(rc.left, curY, rc.left + btnW, curY + 46.0f);
    DrawButton(bNew, TR(L"＋ 新規プロジェクト作成", L"+ New Project"), IconType::Plus, false, false, true, Theme::AccentAction);

    D2D1_RECT_F bOpen = D2D1::RectF(rc.left + btnW + 16.0f, curY, rc.right, curY + 46.0f);
    DrawButton(bOpen, TR(L"📂 外部ファイルを開く...", L"Open File..."), IconType::None);

    curY += 58.0f;

    // 2. プロジェクト保存先フォルダ設定
    D2D1_RECT_F pBox = D2D1::RectF(rc.left, curY, rc.right, curY + 62.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(pBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(pBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    D2D1_RECT_F pTitle = D2D1::RectF(rc.left + 14.0f, curY + 8.0f, rc.left + 350.0f, curY + 26.0f);
    DrawTextSafe(TR(L"プロジェクト保存先フォルダ", L"Project Save Directory"), m_pFontCardTitle, pTitle, m_pBrushText);

    D2D1_RECT_F pPathBox = D2D1::RectF(rc.left + 14.0f, curY + 30.0f, rc.right - 96.0f, curY + 54.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(pPathBox, 4.0f, 4.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(pPathBox, 4.0f, 4.0f), m_pBrushBorderLight, 1.0f);

    std::wstring displayProjDir = m_projectDirectory.empty() ? TR(L"(未設定 - 指定すると保存ダイアログの初期位置になります)", L"(Not set - Click browse to choose)") : m_projectDirectory;
    D2D1_RECT_F pPathText = D2D1::RectF(pPathBox.left + 8.0f, pPathBox.top + 3.0f, pPathBox.right - 8.0f, pPathBox.bottom);
    DrawTextSafe(displayProjDir, m_pFontInspector, pPathText, m_projectDirectory.empty() ? m_pBrushTextMuted : m_pBrushText);

    D2D1_RECT_F bBrowseProj = D2D1::RectF(rc.right - 88.0f, curY + 30.0f, rc.right - 14.0f, curY + 54.0f);
    DrawButton(bBrowseProj, TR(L"参照...", L"Browse..."), IconType::None);

    curY += 72.0f;

    // 3. 最近開いたファイル一覧 (Recent Projects)
    D2D1_RECT_F rTitle = D2D1::RectF(rc.left + 4.0f, curY, rc.left + 350.0f, curY + 20.0f);
    DrawTextSafe(TR(L"最近開いたファイル", L"Recent Projects"), m_pFontCardTitle, rTitle, m_pBrushText);

    curY += 24.0f;

    float rListH = rc.bottom - curY - 44.0f;
    D2D1_RECT_F rListBox = D2D1::RectF(rc.left, curY, rc.right, curY + rListH);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rListBox, 6.0f, 6.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rListBox, 6.0f, 6.0f), m_pBrushBorderLight, 1.0f);

    if (m_recentFiles.empty()) {
        D2D1_RECT_F emptyRc = D2D1::RectF(rListBox.left, rListBox.top + rListH * 0.4f, rListBox.right, rListBox.top + rListH * 0.6f);
        DrawTextSafe(TR(L"最近開いたプロジェクトはありません", L"No recent projects found"), m_pFontModalBody, emptyRc, m_pBrushTextMuted);
    } else {
        float itemH = 32.0f;
        size_t maxItems = std::min<size_t>(m_recentFiles.size(), static_cast<size_t>(std::floor((rListH - 8.0f) / (itemH + 2.0f))));
        for (size_t ri = 0; ri < maxItems; ++ri) {
            float iy = rListBox.top + 6.0f + ri * (itemH + 2.0f);
            D2D1_RECT_F itemRc = D2D1::RectF(rListBox.left + 6.0f, iy, rListBox.right - 6.0f, iy + itemH);

            bool isSel = (static_cast<int>(ri) == m_dashboardSelectedRecentIndex);
            if (isSel) {
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(itemRc, 4.0f, 4.0f), m_pBrushCardSelected);
                m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(itemRc, 4.0f, 4.0f), m_pBrushAccent, 1.0f);
            }

            std::filesystem::path fp(m_recentFiles[ri]);
            std::wstring fName = fp.filename().wstring();
            std::wstring fDir = NormalizePathString(fp.parent_path().wstring());

            D2D1_RECT_F nameRc = D2D1::RectF(itemRc.left + 10.0f, itemRc.top + 6.0f, itemRc.left + 220.0f, itemRc.bottom - 6.0f);
            DrawTextSafe(fName, m_pFontTab, nameRc, isSel ? m_pBrushAccent : m_pBrushText);

            D2D1_RECT_F dirRc = D2D1::RectF(itemRc.left + 230.0f, itemRc.top + 8.0f, itemRc.right - 80.0f, itemRc.bottom - 6.0f);
            DrawTextSafe(fDir, m_pFontInspector, dirRc, m_pBrushTextMuted);

            D2D1_RECT_F bOpenItem = D2D1::RectF(itemRc.right - 72.0f, itemRc.top + 4.0f, itemRc.right - 6.0f, itemRc.bottom - 4.0f);
            DrawButton(bOpenItem, TR(L"開く", L"Open"), IconType::None, false, false, isSel, isSel ? Theme::AccentAction : D2D1_COLOR_F{0, 0, 0, 0});
        }
    }

    // 4. フッター (起動時表示チェックボックス & 選択項目を開く & 閉じるボタン)
    D2D1_RECT_F chkRc = D2D1::RectF(rc.left + 6.0f, rc.bottom - 32.0f, rc.left + 350.0f, rc.bottom - 4.0f);
    DrawCheckboxUI(m_pRenderTarget, m_pFontInspector, m_pBrushText, m_pBrushBorderLight, m_pBrushAccent, chkRc, m_showDashboardOnStartup, TR(L"起動時にこのダッシュボードを表示する", L"Show this dashboard on startup"));

    if (m_dashboardSelectedRecentIndex >= 0 && m_dashboardSelectedRecentIndex < static_cast<int>(m_recentFiles.size())) {
        D2D1_RECT_F bOpenSel = D2D1::RectF(rc.right - 310.0f, rc.bottom - 34.0f, rc.right - 120.0f, rc.bottom);
        DrawButton(bOpenSel, TR(L"選択したプロジェクトを開く", L"Open Selected Project"), IconType::None, false, false, true, Theme::AccentAction);
    }

    D2D1_RECT_F bClose = D2D1::RectF(rc.right - 110.0f, rc.bottom - 34.0f, rc.right, rc.bottom);
    DrawButton(bClose, TR(L"閉じる", L"Close"), IconType::None);
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
    D2D1_RECT_F projPathText = D2D1::RectF(projPathBox.left + 8.0f, projPathBox.top + 3.0f, projPathBox.right - 8.0f, projPathBox.bottom);
    DrawTextSafe(displayProjDir, m_pFontInspector, projPathText, m_projectDirectory.empty() ? m_pBrushTextMuted : m_pBrushText);

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
    D2D1_RECT_F bakPathText = D2D1::RectF(bakPathBox.left + 8.0f, bakPathBox.top + 3.0f, bakPathBox.right - 8.0f, bakPathBox.bottom);
    DrawTextSafe(displayBakDir, m_pFontInspector, bakPathText, m_pBrushTextMuted);

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
        DrawTextSafe(m_tmplEditName.empty() ? TR(L"(名前を入力)", L"(Enter template name)") : m_tmplEditName, m_pFontButton, D2D1::RectF(boxName.left + 8.0f, boxName.top + 6.0f, boxName.right - 8.0f, boxName.bottom), m_tmplEditName.empty() ? m_pBrushTextMuted : m_pBrushText);

        // 2. コード進行
        D2D1_RECT_F lProg = D2D1::RectF(fx + 20.0f, fy + 112.0f, fx + formW - 20.0f, fy + 130.0f);
        DrawTextSafe(TR(L"コード進行 (例: | Fmaj7 | G7 | Em7 | Am7 |):", L"Progression (e.g., | Fmaj7 | G7 | Em7 | Am7 |):"), m_pFontInspector, lProg, m_pBrushTextMuted);
        D2D1_RECT_F boxProg = D2D1::RectF(fx + 20.0f, fy + 132.0f, fx + formW - 20.0f, fy + 164.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxProg, 4.0f, 4.0f), m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxProg, 4.0f, 4.0f), (m_tmplEditFocusField == 1) ? m_pBrushAccent : m_pBrushBorderLight, (m_tmplEditFocusField == 1) ? 2.0f : 1.0f);
        DrawTextSafe(m_tmplEditProg.empty() ? TR(L"(コード進行を入力)", L"(Enter chord progression)") : m_tmplEditProg, m_pFontButton, D2D1::RectF(boxProg.left + 8.0f, boxProg.top + 6.0f, boxProg.right - 8.0f, boxProg.bottom), m_tmplEditProg.empty() ? m_pBrushTextMuted : m_pBrushAccent);

        // 3. 説明文
        D2D1_RECT_F lDesc = D2D1::RectF(fx + 20.0f, fy + 174.0f, fx + formW - 20.0f, fy + 192.0f);
        DrawTextSafe(TR(L"説明文 (任意):", L"Description (Optional):"), m_pFontInspector, lDesc, m_pBrushTextMuted);
        D2D1_RECT_F boxDesc = D2D1::RectF(fx + 20.0f, fy + 194.0f, fx + formW - 20.0f, fy + 226.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxDesc, 4.0f, 4.0f), m_pBrushCardBg);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxDesc, 4.0f, 4.0f), (m_tmplEditFocusField == 2) ? m_pBrushAccent : m_pBrushBorderLight, (m_tmplEditFocusField == 2) ? 2.0f : 1.0f);
        DrawTextSafe(m_tmplEditDesc.empty() ? TR(L"(説明文を入力)", L"(Enter description)") : m_tmplEditDesc, m_pFontButton, D2D1::RectF(boxDesc.left + 8.0f, boxDesc.top + 6.0f, boxDesc.right - 8.0f, boxDesc.bottom), m_tmplEditDesc.empty() ? m_pBrushTextMuted : m_pBrushText);

        // フォームボタン (保存 / キャンセル)
        D2D1_RECT_F bFSave = D2D1::RectF(fx + formW - 140.0f, fy + formH - 46.0f, fx + formW - 20.0f, fy + formH - 14.0f);
        DrawButton(bFSave, TR(L"保存して登録", L"Save Template"), IconType::None, false, false, true, Theme::AccentAction);

        D2D1_RECT_F bFCancel = D2D1::RectF(fx + formW - 250.0f, fy + formH - 46.0f, fx + formW - 150.0f, fy + formH - 14.0f);
        DrawButton(bFCancel, TR(L"キャンセル", L"Cancel"), IconType::None);
    }
}

void AppWindow::RenderHelpModal(const D2D1_RECT_F& rc) {
    float tabW = 140.0f;
    float tabH = 32.0f;
    const wchar_t* helpTabs[] = {
        TR(L"入力形式", L"Input Formats"),
        TR(L"数字接尾辞", L"Numeric Suffixes"),
        TR(L"ショートカット", L"Shortcuts")
    };

    for (int i = 0; i < 3; ++i) {
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

    if (m_helpActiveTab == 0) {
        // --- タブ0: 入力形式 ---
        struct InputItem {
            const wchar_t* title;
            const wchar_t* example;
            const wchar_t* desc;
        };
        InputItem items[] = {
            { TR(L"1. 通常コード名入力", L"1. Standard Chord Symbols"),
              TR(L"C G Am F    または    Fmaj7 E7 Am7 C7", L"C G Am F    or    Fmaj7 E7 Am7 C7"),
              TR(L"標準的なコードシンボルを半角スペース区切りで並べて入力します。小節線 | を使用できます。", L"Enter standard chord symbols separated by spaces. Use | for bar lines.") },
            { TR(L"2. ディグリー入力 (ローマ数字)", L"2. Roman Numeral Degrees"),
              TR(L"IVmaj7 III7 vim7 I7    または    I V vi IV", L"IVmaj7 III7 vim7 I7    or    I V vi IV"),
              TR(L"大文字でメジャー系、小文字でマイナー系コードを解釈します。現在キーに基づいて実音コードへ変換されます。", L"Uppercase for major, lowercase for minor. Automatically transposed to the active key.") },
            { TR(L"3. アラビア数字短縮入力 (爆速入力)", L"3. Fast Numeric Shorthand"),
              TR(L"4mas 3svn 6mis 1svn    または    1 5 6 4", L"4mas 3svn 6mis 1svn    or    1 5 6 4"),
              TR(L"度数＋接尾辞（例: mas=maj7, svn=7, mis=m7）でブラインドタッチによる超高速入力が可能です。", L"Combine scale degree numbers + suffixes (mas=maj7, svn=7, mis=m7) for rapid entry.") },
            { TR(L"4. 記号 (小節線・持続・リピート・休符)", L"4. Symbols (Bar, Tie, Repeat, Rest)"),
              TR(L"| : 小節区切り    - : タイ持続    % / = : 直前リピート    R / N.C. : 休符", L"| : Bar line    - : Tie / Hold    % / = : Repeat previous    R / N.C. : Rest"),
              TR(L"例: | Fmaj7 - - - | E7 - - - | Am7 - - - | C7 - - - |", L"Example: | Fmaj7 - - - | E7 - - - | Am7 - - - | C7 - - - |") }
        };

        for (const auto& it : items) {
            D2D1_RECT_F cardRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 68.0f);
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(cardRc, 4.0f, 4.0f), m_pBrushCardBg);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(cardRc, 4.0f, 4.0f), m_pBrushBorderLight, 0.8f);

            D2D1_RECT_F tR = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + 6.0f, cardRc.right - 10.0f, cardRc.top + 24.0f);
            DrawTextSafe(it.title, m_pFontButton, tR, m_pBrushText);

            D2D1_RECT_F eR = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + 26.0f, cardRc.right - 10.0f, cardRc.top + 44.0f);
            DrawTextSafe(it.example, m_pFontCardTitle, eR, m_pBrushAccent);

            D2D1_RECT_F dR = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + 46.0f, cardRc.right - 10.0f, cardRc.top + 64.0f);
            DrawTextSafe(it.desc, m_pFontInspector, dR, m_pBrushTextMuted);

            textY += 74.0f;
        }
    } else if (m_helpActiveTab == 1) {
        // --- タブ1: 数字接尾辞一覧 (3列表形式) ---
        D2D1_RECT_F gHead = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f);
        DrawTextSafe(TR(L"アラビア数字の直後に入力することでコード品質を瞬時に指定できます (Key: Cの場合):", L"Type immediately after an Arabic numeral to specify chord quality (Example in Key of C):"), m_pFontInspector, gHead, m_pBrushTextMuted);
        textY += 26.0f;

        // 表ヘッダー
        float col1W = 160.0f;
        float col2W = 240.0f;
        D2D1_RECT_F thRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 24.0f);
        m_pRenderTarget->FillRectangle(thRc, m_pBrushBorderLight);
        DrawTextSafe(TR(L"短縮接尾辞", L"Shorthand"), m_pFontInspector, D2D1::RectF(thRc.left + 8.0f, textY + 2.0f, thRc.left + col1W, textY + 22.0f), m_pBrushText);
        DrawTextSafe(TR(L"コード品質 (Quality)", L"Chord Quality"), m_pFontInspector, D2D1::RectF(thRc.left + col1W + 8.0f, textY + 2.0f, thRc.left + col1W + col2W, textY + 22.0f), m_pBrushText);
        DrawTextSafe(TR(L"変換例 (Key: C)", L"Example (Key: C)"), m_pFontInspector, D2D1::RectF(thRc.left + col1W + col2W + 8.0f, textY + 2.0f, thRc.right - 8.0f, textY + 22.0f), m_pBrushText);
        textY += 26.0f;

        struct SuffixItem {
            const wchar_t* suffix;
            const wchar_t* quality;
            const wchar_t* example;
        };
        SuffixItem suffixes[] = {
            { L"mas / M7 / maj7", TR(L"メジャーセブンス (Major 7th)", L"Major 7th"), L"4mas -> Fmaj7, 1mas -> Cmaj7" },
            { L"svn / 7", TR(L"ドミナントセブンス (Dominant 7th)", L"Dominant 7th"), L"5svn -> G7, 3svn -> E7" },
            { L"mis / m7", TR(L"マイナーセブンス (Minor 7th)", L"Minor 7th"), L"2mis -> Dm7, 6mis -> Am7" },
            { L"m", TR(L"マイナー (借用和音など)", L"Minor (Modal Interchange, etc.)"), L"4m -> Fm, 1m -> Cm" },
            { L"hdm / m7b5", TR(L"ハーフディミニッシュ (m7b5)", L"Half-Diminished (m7b5)"), L"7hdm -> Bm7b5, 2hdm -> Dm7b5" },
            { L"dim / dim7", TR(L"ディミニッシュ (Diminished)", L"Diminished 7th"), L"7dim -> Bdim7, #4dim -> F#dim7" },
            { L"suf / sus4", TR(L"サスフォー (sus4)", L"Suspended 4th (sus4)"), L"5suf -> Gsus4, 1suf -> Csus4" },
            { L"sut / sus2", TR(L"サスツー (sus2)", L"Suspended 2nd (sus2)"), L"1sut -> Csus2, 4sut -> Fsus2" },
            { L"adn / add9", TR(L"アドナインス (add9)", L"Added 9th (add9)"), L"1adn -> Cadd9, 4adn -> Fadd9" },
            { L"six / 6", TR(L"シックス (6th)", L"Major 6th (6th)"), L"1six -> C6, 4six -> F6" }
        };

        for (size_t si = 0; si < 10; ++si) {
            D2D1_RECT_F trRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f);
            if (si % 2 == 1) {
                m_pRenderTarget->FillRectangle(trRc, m_pBrushCardBg);
            }
            DrawTextSafe(suffixes[si].suffix, m_pFontButton, D2D1::RectF(trRc.left + 8.0f, textY + 2.0f, trRc.left + col1W, textY + 20.0f), m_pBrushText);
            DrawTextSafe(suffixes[si].quality, m_pFontInspector, D2D1::RectF(trRc.left + col1W + 8.0f, textY + 2.0f, trRc.left + col1W + col2W, textY + 20.0f), m_pBrushText);
            DrawTextSafe(suffixes[si].example, m_pFontInspector, D2D1::RectF(trRc.left + col1W + col2W + 8.0f, textY + 2.0f, trRc.right - 8.0f, textY + 20.0f), m_pBrushAccent);
            textY += 23.0f;
        }
    } else {
        // --- タブ2: ショートカットキー一覧 ---
        struct ShortcutRow {
            const wchar_t* key;
            const wchar_t* func;
        };
        ShortcutRow sRows[] = {
            { L"Space", TR(L"楽曲の再生 / 一時停止 (再生位置へタイムライン追従)", L"Play / Pause progression (auto-scrolls timeline)") },
            { L"Esc", TR(L"停止 (先頭へリセット) / モーダルダイアログを閉じる", L"Stop playback (reset to start) / Close modal dialog") },
            { L"L", TR(L"ループ再生の ON / OFF 切替", L"Toggle loop playback ON / OFF") },
            { L"Ctrl + Enter", TR(L"コード進行テキスト入力欄の内容をタイムラインへ即時反映", L"Apply chord progression text box to timeline") },
            { L"Ctrl + Z / Ctrl + Y", TR(L"元に戻す (Undo) / やり直し (Redo)", L"Undo / Redo") },
            { L"Ctrl + D", TR(L"選択中のコードカードを直後に複製", L"Duplicate selected chord card") },
            { L"Delete / Backspace", TR(L"選択中のコードカードをタイムラインから削除", L"Delete selected chord card from timeline") },
            { L"Ctrl + S", TR(L"プロジェクトを保存 (.chord)", L"Save project (.chord)") },
            { L"Ctrl + O", TR(L"プロジェクトファイルを開く (.chord)", L"Open project file (.chord)") },
            { L"Ctrl + T", TR(L"定番・ジャンル別コード進行テンプレート集を開く", L"Open progression templates dialog") },
            { L"Ctrl + E", TR(L"標準 MIDI ファイル (.mid) をエクスポート保存", L"Export Standard MIDI File (.mid)") },
            { TR(L"マウスホイール", L"Mouse Wheel"), TR(L"タイムラインの水平スクロール", L"Scroll timeline horizontally") },
            { L"Ctrl + + / Ctrl + -", TR(L"タイムラインの横ズームイン / ズームアウト", L"Zoom timeline in / out horizontally") },
            { TR(L"コード左右端ドラッグ", L"Drag Card Edge"), TR(L"コードカードの音価 (Tick時間長) をマウスで伸縮変更（小節線を越える跨ぎも可）", L"Resize chord card duration in ticks (can cross bar lines)") },
            { L"Ctrl + Up / Down", TR(L"選択コードを1オクターブ上 / 下へ移動", L"Shift selected chord up / down one octave") },
            { L"Ctrl + 1..5", TR(L"タイムライングリッド切替 (1小節 / 2拍 / 1拍 / 1/2拍 / 1/4拍)", L"Timeline grid preset (1 bar / 2 beats / 1 beat / 1/2 / 1/4)") },
            { L"Ctrl + Shift + 1..5", TR(L"入力グリッド切替 (追加位置と音価)", L"Input grid preset (placement & length of new chords)") }
        };

        float col1W = 190.0f;
        for (size_t ki = 0; ki < sizeof(sRows) / sizeof(sRows[0]); ++ki) {
            D2D1_RECT_F trRc = D2D1::RectF(rc.left + pad, textY, rc.right - pad, textY + 22.0f);
            if (ki % 2 == 1) {
                m_pRenderTarget->FillRectangle(trRc, m_pBrushCardBg);
            }
            DrawTextSafe(sRows[ki].key, m_pFontButton, D2D1::RectF(trRc.left + 8.0f, textY + 2.0f, trRc.left + col1W, textY + 20.0f), m_pBrushAccent);
            DrawTextSafe(sRows[ki].func, m_pFontInspector, D2D1::RectF(trRc.left + col1W + 8.0f, textY + 2.0f, trRc.right - 8.0f, textY + 20.0f), m_pBrushText);
            textY += 23.0f;
        }
    }

    // 下部アクションボタン
    D2D1_RECT_F bDoc = D2D1::RectF(rc.left, rc.bottom - 36.0f, rc.left + 260.0f, rc.bottom);
    DrawButton(bDoc, TR(L"オンラインドキュメント (GitHub)", L"Online Documentation (GitHub)"), IconType::None);

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
    DrawTextSafe(TR(L"パートの新しい名称を入力してください:", L"Enter new name for the part:"), m_pFontCardTitle, lTitle, m_pBrushText);

    curY += 34.0f;

    D2D1_RECT_F boxRc = D2D1::RectF(rc.left + 16.0f, curY, rc.right - 16.0f, curY + 44.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(boxRc, 4.0f, 4.0f), m_pBrushBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(boxRc, 4.0f, 4.0f), m_pBrushAccent, 1.5f);

    std::wstring displayText = m_renamePartName + L"|";
    D2D1_RECT_F textRc = D2D1::RectF(boxRc.left + 12.0f, boxRc.top + 8.0f, boxRc.right - 12.0f, boxRc.bottom - 6.0f);
    DrawTextSafe(displayText, m_pFontModalTitle, textRc, m_pBrushText);

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
        pActive->bpm = m_measuredBpm;
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

    float cardW = (m_activeModal == ModalType::TapTempo || m_activeModal == ModalType::RenamePart) ? 480.0f : std::min(740.0f, m_width - 40.0f);
    float cardH = (m_activeModal == ModalType::RenamePart) ? 240.0f : ((m_activeModal == ModalType::TapTempo) ? 320.0f : std::min(520.0f, m_height - 40.0f));
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
            float curY = bodyRc.top;

            // 1. 上部ボタン (新規作成 / 外部ファイルを開く)
            float btnW = (bodyRc.right - bodyRc.left - 16.0f) * 0.5f;
            D2D1_RECT_F bNew = D2D1::RectF(bodyRc.left, curY, bodyRc.left + btnW, curY + 46.0f);
            if (PtInRectF(bNew, x, y)) {
                DoFileNew();
                CloseModal();
                return true;
            }

            D2D1_RECT_F bOpen = D2D1::RectF(bodyRc.left + btnW + 16.0f, curY, bodyRc.right, curY + 46.0f);
            if (PtInRectF(bOpen, x, y)) {
                DoFileOpen();
                if (!m_currentProjectPath.empty()) {
                    CloseModal();
                }
                return true;
            }

            curY += 58.0f;

            // 2. プロジェクト保存先フォルダ設定の参照ボタン
            D2D1_RECT_F bBrowseProj = D2D1::RectF(bodyRc.right - 88.0f, curY + 30.0f, bodyRc.right - 14.0f, curY + 54.0f);
            if (PtInRectF(bBrowseProj, x, y)) {
                ChooseDirectory(m_projectDirectory, TR(L"プロジェクト保存先フォルダを選択", L"Select Project Directory"));
                SaveAppSettings();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            curY += 72.0f;
            curY += 24.0f;

            // 3. 最近開いたファイル一覧
            float rListH = bodyRc.bottom - curY - 44.0f;
            D2D1_RECT_F rListBox = D2D1::RectF(bodyRc.left, curY, bodyRc.right, curY + rListH);
            if (!m_recentFiles.empty() && PtInRectF(rListBox, x, y)) {
                float itemH = 32.0f;
                size_t maxItems = std::min<size_t>(m_recentFiles.size(), static_cast<size_t>(std::floor((rListH - 8.0f) / (itemH + 2.0f))));
                for (size_t ri = 0; ri < maxItems; ++ri) {
                    float iy = rListBox.top + 6.0f + ri * (itemH + 2.0f);
                    D2D1_RECT_F itemRc = D2D1::RectF(rListBox.left + 6.0f, iy, rListBox.right - 6.0f, iy + itemH);
                    if (PtInRectF(itemRc, x, y)) {
                        D2D1_RECT_F bOpenItem = D2D1::RectF(itemRc.right - 72.0f, itemRc.top + 4.0f, itemRc.right - 6.0f, itemRc.bottom - 4.0f);
                        if (PtInRectF(bOpenItem, x, y)) {
                            m_dashboardSelectedRecentIndex = static_cast<int>(ri);
                            OpenRecentFile(static_cast<int>(ri));
                            CloseModal();
                            return true;
                        } else {
                            m_dashboardSelectedRecentIndex = static_cast<int>(ri);
                            InvalidateRect(m_hwnd, nullptr, FALSE);
                            return true;
                        }
                    }
                }
            }

            // 4. フッター
            D2D1_RECT_F chkRc = D2D1::RectF(bodyRc.left + 6.0f, bodyRc.bottom - 32.0f, bodyRc.left + 350.0f, bodyRc.bottom - 4.0f);
            if (PtInRectF(chkRc, x, y)) {
                m_showDashboardOnStartup = !m_showDashboardOnStartup;
                SaveAppSettings();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return true;
            }

            if (m_dashboardSelectedRecentIndex >= 0 && m_dashboardSelectedRecentIndex < static_cast<int>(m_recentFiles.size())) {
                D2D1_RECT_F bOpenSel = D2D1::RectF(bodyRc.right - 310.0f, bodyRc.bottom - 34.0f, bodyRc.right - 120.0f, bodyRc.bottom);
                if (PtInRectF(bOpenSel, x, y)) {
                    OpenRecentFile(m_dashboardSelectedRecentIndex);
                    CloseModal();
                    return true;
                }
            }

            D2D1_RECT_F bClose = D2D1::RectF(bodyRc.right - 110.0f, bodyRc.bottom - 34.0f, bodyRc.right, bodyRc.bottom);
            if (PtInRectF(bClose, x, y)) {
                CloseModal();
                return true;
            }
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
                        pActive->bars = ProgressionParser::Parse(tmpls[i].progression, pActive->keyRoot, pActive->mode);
                        pActive->rawProgressionText = tmpls[i].progression;
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
            float tabW = 140.0f;
            float tabH = 32.0f;
            for (int i = 0; i < 3; ++i) {
                float tx = bodyRc.left + i * (tabW + 8.0f);
                if (PtInRectF(D2D1::RectF(tx, bodyRc.top, tx + tabW, bodyRc.top + tabH), x, y)) {
                    m_helpActiveTab = i;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return true;
                }
            }

            D2D1_RECT_F bDoc = D2D1::RectF(bodyRc.left, bodyRc.bottom - 36.0f, bodyRc.left + 260.0f, bodyRc.bottom);
            if (PtInRectF(bDoc, x, y)) {
                OpenOnlineDocumentation();
                return true;
            }

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
                if (!m_renamePartName.empty()) {
                    auto& parts = m_partManager.GetParts();
                    if (m_renamePartIndex >= 0 && m_renamePartIndex < static_cast<int>(parts.size())) {
                        parts[m_renamePartIndex].name = ToUtf8String(m_renamePartName);
                        SyncTimelineToText();
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                }
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
