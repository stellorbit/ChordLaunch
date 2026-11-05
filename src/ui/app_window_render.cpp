#include "app_window_internal.hpp"
namespace clp {
// ---------------------------------------------------------------------------
// 全体描画ルーチン
// ---------------------------------------------------------------------------
void AppWindow::OnRender() {
    if (FAILED(CreateDeviceResources())) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(Theme::WindowBackground);

    float w = m_width;
    float h = m_height;

    // 1. 上部2段ツールバー (高さ 74px)
    RenderToolbar(0, 0, w, m_toolbarHeight);

    float mainY = m_toolbarHeight;
    float mainH = h - mainY;

    // 2. 左ペイン (ツールバー直下から開始)
    RenderLeftPane(0, mainY, m_leftPaneWidth, mainH);

    // 3. 右ペイン (音楽理論パレット)
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float rightX = w - rightW;
    if (m_showRightPane) {
        RenderRightPane(rightX, mainY, rightW, mainH);
    }

    // 4. 中央ペイン (パートタブ、タイムライン、拡大ピアノロール、鍵盤＋インスペクター統合ドック)
    float centerX = m_leftPaneWidth;
    float centerW = rightX - centerX;
    RenderCenterPane(centerX, mainY, centerW, mainH);

    // パレットおよびタイムラインドラッグ中のドロップ先グリッドスナップ可視化（ゴーストカード描画）
    if ((m_dragMode == DragMode::PaletteCardDrag || m_dragMode == DragMode::CardTimelineDrag) && m_isPaletteDragging) {
        float dockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
        float dragTargetX = (m_dragMode == DragMode::CardTimelineDrag) ? (m_dragCurrentX - m_dragCardGrabOffsetX) : m_dragCurrentX;

        if (dragTargetX >= centerX - 20.0f && dragTargetX <= centerX + centerW + 20.0f &&
            m_dragCurrentY >= mainY && m_dragCurrentY <= h - dockH) {
            float barWidth = m_pixelsPerBeat * 4.0f;
            float relX = (dragTargetX - centerX) + m_timelineScrollX;
            if (relX < 0.0f) relX = 0.0f;

            Part* pActive = m_partManager.GetActivePart();
            int targetBarIdx = (barWidth > 0.0f) ? static_cast<int>(relX / barWidth) : 0;
            float barOffsetRatio = (barWidth > 0.0f) ? (std::fmod(relX, barWidth) / barWidth) : 0.0f;

            // ドロップ先の小節が実在する場合はその拍子を優先（挿入処理と完全一致させる）
            int barTotalTicks = -1;
            if (pActive && targetBarIdx >= 0 && targetBarIdx < static_cast<int>(pActive->bars.size())) {
                barTotalTicks = pActive->bars[targetBarIdx].GetTotalTicks();
            }
            if (barTotalTicks <= 0) {
                int barNumerator = (pActive && pActive->timeNumerator > 0) ? pActive->timeNumerator : 4;
                barTotalTicks = barNumerator * TICKS_PER_BEAT;
            }

            int gridTicks = std::max(1, m_timelineGridTicks);
            float rawTick = barOffsetRatio * static_cast<float>(barTotalTicks);
            int snappedTick = static_cast<int>(std::round(rawTick / static_cast<float>(gridTicks))) * gridTicks;
            int fallbackTicks = std::max(1, m_inputGridTicks);
            int chordTicks = m_dragPaletteChord.durationTicks > 0 ? m_dragPaletteChord.durationTicks : (m_dragPaletteChord.durationBeats > 0 ? m_dragPaletteChord.durationBeats * TICKS_PER_BEAT : fallbackTicks);
            chordTicks = std::max(1, chordTicks);
            int maxStart = std::max(0, barTotalTicks - std::min(barTotalTicks, gridTicks));
            int covered = (pActive) ? TimelineEdit::IncomingCoverUntil(pActive->bars, targetBarIdx) : 0;
            snappedTick = std::clamp(snappedTick, std::min(covered, maxStart), maxStart);
            if (m_dragMode == DragMode::CardTimelineDrag) {
                // 既存カードの移動は小節内の上限・前小節のカバー範囲で制限せず、実際の移動処理と同じ位置を表示する
                TimelineTickFromX(dragTargetX, gridTicks, &targetBarIdx, &snappedTick, &barTotalTicks);
            }

            float barLeftX = centerX + targetBarIdx * barWidth - m_timelineScrollX;
            float slotX = barLeftX + (static_cast<float>(snappedTick) / static_cast<float>(barTotalTicks)) * barWidth;
            float slotW = (static_cast<float>(chordTicks) / static_cast<float>(barTotalTicks)) * barWidth - 4.0f;

            float cardTrackY = mainY + m_partTabHeight + m_rulerHeight + 5.0f;
            float cardTrackH = m_cardTrackHeight - 10.0f;

            // 1. ピアノロール上のグリッド列ハイライト
            float rollYTop = cardTrackY + cardTrackH + 5.0f;
            float rollYBot = h - dockH - m_scrollBarHeight;
            if (rollYBot > rollYTop) {
                D2D1_RECT_F rollSlotRc = D2D1::RectF(slotX + 2.0f, rollYTop, slotX + 2.0f + slotW, rollYBot);
                D2D1_RECT_F clippedRollRc = D2D1::RectF(std::max(centerX, rollSlotRc.left), rollSlotRc.top, std::min(centerX + centerW, rollSlotRc.right), rollSlotRc.bottom);
                if (clippedRollRc.right > clippedRollRc.left) {
                    m_pRenderTarget->FillRectangle(clippedRollRc, m_pBrushCardActive);
                }
            }

            // 2. カードトラック上のドロップ先ゴーストカード枠 (半透明青背景＆アクセント枠線)
            D2D1_RECT_F slotCardRc = D2D1::RectF(slotX + 2.0f, cardTrackY, slotX + 2.0f + slotW, cardTrackY + cardTrackH);
            D2D1_RECT_F clippedSlotRc = D2D1::RectF(std::max(centerX, slotCardRc.left), slotCardRc.top, std::min(centerX + centerW, slotCardRc.right), slotCardRc.bottom);
            if (clippedSlotRc.right > clippedSlotRc.left) {
                m_pRenderTarget->PushAxisAlignedClip(D2D1::RectF(centerX, cardTrackY, centerX + centerW, cardTrackY + cardTrackH), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(slotCardRc, 6.0f, 6.0f), m_pBrushCardSelected);
                m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(slotCardRc, 6.0f, 6.0f), m_pBrushAccent, 2.0f);

                std::wstring gName = m_dragPaletteChord.isRest ? L"Rest" : ToWString(m_dragPaletteChord.rootName + m_dragPaletteChord.qualitySuffix);
                if (!m_dragPaletteChord.isRest && !m_dragPaletteChord.bassName.empty()) {
                    gName += L"/" + ToWString(m_dragPaletteChord.bassName);
                }

                int beatNum = (snappedTick / TICKS_PER_BEAT) + 1;
                int subBeat = ((snappedTick % TICKS_PER_BEAT) * 4) / TICKS_PER_BEAT;
                wchar_t slotHint[64];
                if (subBeat == 0) {
                    swprintf_s(slotHint, L"Bar %d : %d拍", targetBarIdx + 1, beatNum);
                } else {
                    swprintf_s(slotHint, L"Bar %d : %d.%d拍", targetBarIdx + 1, beatNum, subBeat);
                }

                D2D1_RECT_F tRc = D2D1::RectF(slotCardRc.left + 6.0f, slotCardRc.top + 4.0f, slotCardRc.right - 6.0f, slotCardRc.top + cardTrackH * 0.55f);
                DrawTextSafe(gName, m_pFontCardTitle, tRc, m_pBrushAccent);

                D2D1_RECT_F dRc = D2D1::RectF(slotCardRc.left + 6.0f, slotCardRc.top + cardTrackH * 0.52f, slotCardRc.right - 6.0f, slotCardRc.bottom - 4.0f);
                DrawTextSafe(slotHint, m_pFontCardDegree, dRc, m_pBrushTextMuted);

                m_pRenderTarget->PopAxisAlignedClip();
            }
        }
    }

    // 5. モーダルダイアログ
    if (m_activeModal != ModalType::None) {
        RenderModal(w, h);
    }

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

// ---------------------------------------------------------------------------
// 1. 上部2段ツールバー描画 (ウィンドウ幅縮小時の被りを完全根絶)
// ---------------------------------------------------------------------------
void AppWindow::RenderToolbar(float x, float y, float width, float height) {
    D2D1_RECT_F tbRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(tbRect, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + height), D2D1::Point2F(x + width, y + height), m_pBrushBorder, 1.0f);

    // ---------------- 上段 (y = 0 〜 38px): 再生系 ＆ 保存・各種モーダル ----------------
    float r1Y = y + 4.0f;
    float r1H = 28.0f;

    // 左側: 再生/停止/ループ
    float curX = x + 12.0f;

    D2D1_RECT_F rcPlay = D2D1::RectF(curX, r1Y, curX + 74.0f, r1Y + r1H);
    DrawButton(rcPlay, m_isPlaying ? TR(L"一時停止", L"Pause") : TR(L"再生", L"Play"), m_isPlaying ? IconType::Pause : IconType::Play, false, false, true, m_isPlaying ? Theme::AccentStop : Theme::AccentPlay);
    curX += 78.0f;

    D2D1_RECT_F rcStop = D2D1::RectF(curX, r1Y, curX + 58.0f, r1Y + r1H);
    DrawButton(rcStop, TR(L"停止", L"Stop"), IconType::Stop, false, false, false);
    curX += 62.0f;

    D2D1_RECT_F rcLoop = D2D1::RectF(curX, r1Y, curX + 66.0f, r1Y + r1H);
    DrawButton(rcLoop, TR(L"ループ", L"Loop"), IconType::Loop, false, false, m_isLooping, Theme::AccentAction);
    curX += 74.0f;

    // 全体マージ (新規パート生成) — 上段の再生系の並びに配置
    D2D1_RECT_F rcMerge = D2D1::RectF(curX + 6.0f, r1Y, curX + 116.0f, r1Y + r1H);
    DrawButton(rcMerge, TR(L"全体マージ", L"Merge All"), IconType::None, false, false, true, Theme::AccentAction);

    // 小節 / 拍カウンター (BAR 1 : 1) - ツールバー中央に配置して視認性を向上
    float counterW = 124.0f;
    float counterX = x + (width - counterW) * 0.5f;
    D2D1_RECT_F rcCounter = D2D1::RectF(counterX, r1Y, counterX + counterW, r1Y + r1H);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcCounter, 4.0f, 4.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcCounter, 4.0f, 4.0f), m_pBrushAccent, 1.2f);

    int curBar = static_cast<int>(m_playbackTick / TICKS_PER_BAR_4_4) + 1;
    int curBeat = static_cast<int>(std::fmod(m_playbackTick, static_cast<double>(TICKS_PER_BAR_4_4)) / TICKS_PER_BEAT) + 1;
    wchar_t counterBuf[32];
    swprintf_s(counterBuf, L"BAR %d : %d", curBar, curBeat);
    DrawTextSafe(counterBuf, m_pFontCardTitle, rcCounter, m_pBrushAccent);

    // 右側: テンプレート集, ヘルプ, 設定, MIDI保存, DAWへドラッグ
    float rightCurX = x + width - 12.0f;

    // 設定
    rightCurX -= 34.0f;
    D2D1_RECT_F rcSettings = D2D1::RectF(rightCurX, r1Y, rightCurX + 32.0f, r1Y + r1H);
    DrawButton(rcSettings, L"", IconType::Settings);

    // ヘルプ
    rightCurX -= 34.0f;
    D2D1_RECT_F rcHelp = D2D1::RectF(rightCurX, r1Y, rightCurX + 32.0f, r1Y + r1H);
    DrawButton(rcHelp, L"", IconType::Help);

    // テンプレート集
    rightCurX -= 110.0f;
    D2D1_RECT_F rcTmpl = D2D1::RectF(rightCurX, r1Y, rightCurX + 104.0f, r1Y + r1H);
    DrawButton(rcTmpl, TR(L"テンプレート集", L"Templates"), IconType::Template);

    // DAWへドラッグ
    rightCurX -= 130.0f;
    D2D1_RECT_F rcDawDrag = D2D1::RectF(rightCurX, r1Y, rightCurX + 124.0f, r1Y + r1H);
    DrawButton(rcDawDrag, TR(L"DAWへドラッグ", L"Drag to DAW"), IconType::DawDrag, false, false, true, Theme::AccentAction);

    // MIDI保存
    rightCurX -= 95.0f;
    D2D1_RECT_F rcMidiSave = D2D1::RectF(rightCurX, r1Y, rightCurX + 88.0f, r1Y + r1H);
    DrawButton(rcMidiSave, TR(L"MIDI保存", L"Save MIDI"), IconType::SaveDisk);

    // ---------------- 下段 (y = 38 〜 74px): キー, スケール, BPM, TAP, ボイシング, ベース, グリッド ----------------
    float r2Y = y + 38.0f;
    float r2H = 28.0f;
    curX = x + 12.0f;

    // 下段背景の薄い区切り線
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, r2Y - 2.0f), D2D1::Point2F(x + width, r2Y - 2.0f), m_pBrushBorderLight, 0.8f);

    Part* pActive = m_partManager.GetActivePart();

    // 1. キー (プルダウンメニュー)
    std::wstring keyText = std::wstring(TR(L"キー: ", L"Key: ")) + ToWString(pActive ? pActive->keyRoot : "C") + L" ▼";
    D2D1_RECT_F rcKey = D2D1::RectF(curX, r2Y, curX + 78.0f, r2Y + r2H);
    DrawButton(rcKey, keyText, IconType::None);
    curX += 82.0f;

    // 2. スケール (プルダウンメニュー)
    const wchar_t* modeNamesJa[] = {
        L"メジャー", L"マイナー", L"ハーモニック", L"メロディック",
        L"ドリアン", L"フリジアン", L"リディアン", L"ミクソリディアン", L"ロクリアン"
    };
    const wchar_t* modeNamesEn[] = {
        L"Major", L"Minor", L"Harmonic", L"Melodic",
        L"Dorian", L"Phrygian", L"Lydian", L"Mixolydian", L"Locrian"
    };
    int mIdx = std::clamp(pActive ? static_cast<int>(pActive->mode) : 0, 0, 8);
    std::wstring modeText = std::wstring(TR(L"スケール: ", L"Scale: ")) + (m_settingLanguage == 1 ? modeNamesEn[mIdx] : modeNamesJa[mIdx]) + L" ▼";
    D2D1_RECT_F rcMode = D2D1::RectF(curX, r2Y, curX + 115.0f, r2Y + r2H);
    DrawButton(rcMode, modeText, IconType::None);
    curX += 120.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(curX, r2Y + 2.0f), D2D1::Point2F(curX, r2Y + r2H - 2.0f), m_pBrushBorderLight, 1.0f);
    curX += 10.0f;

    // 3. BPM (テキスト表示 ＋ 上下スピンボタン)
    D2D1_RECT_F rcBpmLabel = D2D1::RectF(curX, r2Y, curX + 34.0f, r2Y + r2H);
    DrawTextSafe(L"BPM:", m_pFontButton, rcBpmLabel, m_pBrushTextMuted);
    curX += 34.0f;

    // BPM 数値ボックス
    D2D1_RECT_F rcBpmVal = D2D1::RectF(curX, r2Y, curX + 40.0f, r2Y + r2H);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcBpmVal, 3.0f, 3.0f), m_pBrushCardBg);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcBpmVal, 3.0f, 3.0f), m_pBrushBorder, 1.0f);
    std::wstring bpmNumStr = std::to_wstring(pActive ? pActive->bpm : 120);
    DrawTextSafe(bpmNumStr, m_pFontButton, rcBpmVal, m_pBrushText);
    curX += 42.0f;

    // 上下スピンボタン
    D2D1_RECT_F rcSpinUp = D2D1::RectF(curX, r2Y, curX + 18.0f, r2Y + r2H * 0.5f - 1.0f);
    DrawButton(rcSpinUp, L"▲", IconType::None);

    D2D1_RECT_F rcSpinDown = D2D1::RectF(curX, r2Y + r2H * 0.5f + 1.0f, curX + 18.0f, r2Y + r2H);
    DrawButton(rcSpinDown, L"▼", IconType::None);
    curX += 24.0f;

    // 4. TAPテンポボタン (点灯対応)
    D2D1_RECT_F rcTap = D2D1::RectF(curX, r2Y, curX + 54.0f, r2Y + r2H);
    DrawButton(rcTap, L"TAP", IconType::None, false, false, m_tapBlinked, Theme::AccentWarm);
    curX += 62.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(curX, r2Y + 2.0f), D2D1::Point2F(curX, r2Y + r2H - 2.0f), m_pBrushBorderLight, 1.0f);
    curX += 10.0f;

    // 5. 音色 (プルダウンメニュー)
    const wchar_t* presetNamesJa[] = { L"グランドピアノ", L"ウォームパッド", L"エレピ", L"ストリングス" };
    const wchar_t* presetNamesEn[] = { L"Grand Piano", L"Warm Pad", L"E.Piano", L"Strings" };
    int pIdx = std::clamp(static_cast<int>(m_synth.GetPreset()), 0, 3);
    std::wstring presetStr = std::wstring(TR(L"音色: ", L"Sound: ")) + (m_settingLanguage == 1 ? presetNamesEn[pIdx] : presetNamesJa[pIdx]) + L" ▼";
    D2D1_RECT_F rcPreset = D2D1::RectF(curX, r2Y, curX + 126.0f, r2Y + r2H);
    DrawButton(rcPreset, presetStr, IconType::None);
    curX += 130.0f;

    // 6. ボイシング (プルダウンメニュー)
    const wchar_t* voicingNamesJa[] = { L"基本形", L"Drop2", L"Drop3", L"OpenSpread", L"Closed" };
    const wchar_t* voicingNamesEn[] = { L"Root", L"Drop2", L"Drop3", L"OpenSpread", L"Closed" };
    int vIdx = std::clamp(static_cast<int>(m_selectedVoicing), 0, 4);
    std::wstring voicingStr = std::wstring(TR(L"ボイシング: ", L"Voicing: ")) + (m_settingLanguage == 1 ? voicingNamesEn[vIdx] : voicingNamesJa[vIdx]) + L" ▼";
    D2D1_RECT_F rcVoicing = D2D1::RectF(curX, r2Y, curX + 125.0f, r2Y + r2H);
    DrawButton(rcVoicing, voicingStr, IconType::None);
    curX += 129.0f;

    // 7. ベース (プルダウンメニュー)
    const wchar_t* bassNamesJa[] = { L"ベース: なし", L"ベース: -1 Oct", L"ベース: -2 Oct", L"ベース: 両方" };
    const wchar_t* bassNamesEn[] = { L"Bass: Off", L"Bass: -1 Oct", L"Bass: -2 Oct", L"Bass: Both" };
    int bIdx = std::clamp(m_bassOctaveMode, 0, 3);
    std::wstring bassStr = std::wstring(m_settingLanguage == 1 ? bassNamesEn[bIdx] : bassNamesJa[bIdx]) + L" ▼";
    D2D1_RECT_F rcBass = D2D1::RectF(curX, r2Y, curX + 115.0f, r2Y + r2H);
    DrawButton(rcBass, bassStr, IconType::None);
    curX += 119.0f;

    // 8. 入力グリッド (追加するコードの音価)
    std::wstring inGridStr = TR(L"入力: 1拍 ▼", L"In: 1 Beat ▼");
    if (m_inputGridTicks == TICKS_PER_BAR_4_4) inGridStr = TR(L"入力: 1小節 ▼", L"In: 1 Bar ▼");
    else if (m_inputGridTicks == TICKS_PER_BEAT * 2) inGridStr = TR(L"入力: 2拍 ▼", L"In: 2 Beats ▼");
    else if (m_inputGridTicks == TICKS_PER_BEAT / 2) inGridStr = TR(L"入力: 1/2拍 ▼", L"In: 1/2 Beat ▼");
    else if (m_inputGridTicks == TICKS_PER_BEAT / 4) inGridStr = TR(L"入力: 1/4拍 ▼", L"In: 1/4 Beat ▼");

    D2D1_RECT_F rcInputGrid = D2D1::RectF(curX, r2Y, curX + 100.0f, r2Y + r2H);
    DrawButton(rcInputGrid, inGridStr, IconType::None);
    curX += 104.0f;

    // 9. タイムライングリッド (スナップ/表示)
    std::wstring gridStr = TR(L"グリッド: 1拍 ▼", L"Grid: 1 Beat ▼");
    if (m_timelineGridTicks == TICKS_PER_BAR_4_4) gridStr = TR(L"グリッド: 1小節 ▼", L"Grid: 1 Bar ▼");
    else if (m_timelineGridTicks == TICKS_PER_BEAT * 2) gridStr = TR(L"グリッド: 2拍 ▼", L"Grid: 2 Beats ▼");
    else if (m_timelineGridTicks == TICKS_PER_BEAT / 2) gridStr = TR(L"グリッド: 1/2拍 ▼", L"Grid: 1/2 Beat ▼");
    else if (m_timelineGridTicks == TICKS_PER_BEAT / 4) gridStr = TR(L"グリッド: 1/4拍 ▼", L"Grid: 1/4 Beat ▼");

    D2D1_RECT_F rcGrid = D2D1::RectF(curX, r2Y, curX + 110.0f, r2Y + r2H);
    DrawButton(rcGrid, gridStr, IconType::None);
    curX += 114.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(curX, r2Y + 2.0f), D2D1::Point2F(curX, r2Y + r2H - 2.0f), m_pBrushBorderLight, 1.0f);
}

// ---------------------------------------------------------------------------
// 2. 左ペイン描画 (入力欄・ダイアトニック・カスタムコードビルダー)
// ---------------------------------------------------------------------------
void AppWindow::RenderLeftPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + width, y), D2D1::Point2F(x + width, y + height), m_pBrushBorder, 1.0f);

    // 最下部ステータスバーの上端より上に描画領域をクリップし、ボタンとの重複を完全防止
    float statusTopY = y + height - 22.0f;
    D2D1_RECT_F contentClip = D2D1::RectF(x, y, x + width, std::max(y, statusTopY));
    m_pRenderTarget->PushAxisAlignedClip(contentClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float curY = y + 5.0f;
    float pad = 16.0f;
    float innerW = width - pad * 2.0f;

    // セクション1: コード進行テキスト入力
    D2D1_RECT_F rcTitle1 = D2D1::RectF(x + pad, curY, x + width - pad, curY + 17.0f);
    DrawTextSafe(TR(L"コード進行テキスト入力", L"Chord Progression Input"), m_pFontInspector, rcTitle1, m_pBrushText);
    curY += 18.0f;

    curY += 54.0f; // EDIT コントロールスペース (50px + 4px)

    // 反映 (Ctrl+Enter) ボタン
    D2D1_RECT_F rcApply = D2D1::RectF(x + pad, curY, x + width - pad, curY + 24.0f);
    DrawButton(rcApply, TR(L"反映 (Ctrl+Enter)", L"Apply (Ctrl+Enter)"), IconType::None, false, false, true, Theme::AccentAction);
    curY += 28.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(x + pad, curY), D2D1::Point2F(x + width - pad, curY), m_pBrushBorderLight, 1.0f);
    curY += 4.0f;

    // セクション2: ダイアトニックコード
    D2D1_RECT_F rcTitle2 = D2D1::RectF(x + pad, curY, x + width - pad, curY + 17.0f);
    DrawTextSafe(TR(L"ダイアトニックコード", L"Diatonic Chords"), m_pFontInspector, rcTitle2, m_pBrushText);
    curY += 17.0f;

    float tabBtnW = (innerW - 4.0f * 3.0f) / 5.0f;
    const wchar_t* diaTabsJa[] = { L"3和音", L"7th", L"sus4", L"add9", L"分数" };
    const wchar_t* diaTabsEn[] = { L"Triad", L"7th", L"sus4", L"add9", L"Slash" };
    for (int t = 0; t < 5; ++t) {
        float tx = x + pad + t * (tabBtnW + 3.0f);
        D2D1_RECT_F rcTab = D2D1::RectF(tx, curY, tx + tabBtnW, curY + 21.0f);
        DrawButton(rcTab, (m_settingLanguage == 1) ? diaTabsEn[t] : diaTabsJa[t], IconType::None, false, false, (m_diatonicChordTypeTab % 5) == t);
    }
    curY += 24.0f;

    Part* pActive = m_partManager.GetActivePart();
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    auto diatonicChords = MusicEngine::GetDiatonicChordsByTab(keyRoot, mode, m_diatonicChordTypeTab % 5);
    float cardW = (innerW - 3.0f * 6.0f) / 4.0f;
    float cardH = 38.0f;

    for (size_t i = 0; i < 8; ++i) {
        float cx = x + pad + (i % 4) * (cardW + 6.0f);
        float cy = curY + (i / 4) * (cardH + 4.0f);
        D2D1_RECT_F cardRc = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);

        if (i < 7 && i < diatonicChords.size()) {
            const auto& chord = diatonicChords[i];
            std::wstring cLabel = ToWString(chord.rootName + chord.qualitySuffix);
            if (!chord.bassName.empty()) cLabel += L"/" + ToWString(chord.bassName);
            DrawCard(cardRc, cLabel, ToWString(chord.degreeRoman), false, false);
        } else if (i == 7) {
            DrawCard(cardRc, L"Rest", L"N.C.", false, false, true);
        }
    }
    curY += (cardH * 2.0f + 4.0f) + 6.0f;

    m_pRenderTarget->DrawLine(D2D1::Point2F(x + pad, curY), D2D1::Point2F(x + width - pad, curY), m_pBrushBorderLight, 1.0f);
    curY += 4.0f;

    // セクション3: カスタムコードビルダー (クリックで格納・展開可能)
    D2D1_RECT_F rcTitle3 = D2D1::RectF(x + pad, curY, x + width - pad, curY + 19.0f);
    DrawSectionHeader(rcTitle3, TR(L"カスタムコードビルダー", L"Custom Chord Builder"), m_accCustomBuilderOpen);
    curY += 22.0f;

    if (m_accCustomBuilderOpen) {
        // ルート音選択
        const char* roots[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        float rBtnW = (innerW - 5.0f * 4.0f) / 6.0f;
        float rBtnH = 20.0f;

        for (int i = 0; i < 12; ++i) {
            float rx = x + pad + (i % 6) * (rBtnW + 4.0f);
            float ry = curY + (i / 6) * (rBtnH + 3.0f);
            D2D1_RECT_F btnRc = D2D1::RectF(rx, ry, rx + rBtnW, ry + rBtnH);
            PitchClass pc = MusicEngine::NoteToPitch(roots[i]);
            DrawButton(btnRc, ToWString(roots[i]), IconType::None, false, false, pc == m_customRoot);
        }
        curY += (rBtnH * 2.0f + 3.0f) + 4.0f;

        // サフィックス選択
        const char* suffixes[] = { "", "m", "7", "Maj7", "m7", "m7b5", "dim7", "sus4" };
        const wchar_t* suffixLabels[] = { L"Maj", L"m", L"7", L"Maj7", L"m7", L"m7b5", L"dim7", L"sus4" };
        float sBtnW = (innerW - 3.0f * 4.0f) / 4.0f;
        float sBtnH = 20.0f;

        for (int i = 0; i < 8; ++i) {
            float sx = x + pad + (i % 4) * (sBtnW + 4.0f);
            float sy = curY + (i / 4) * (sBtnH + 3.0f);
            D2D1_RECT_F btnRc = D2D1::RectF(sx, sy, sx + sBtnW, sy + sBtnH);
            DrawButton(btnRc, suffixLabels[i], IconType::None, false, false, m_customSuffix == suffixes[i]);
        }
        curY += (sBtnH * 2.0f + 3.0f) + 4.0f;

        // テンション選択
        const int8_t tensVals[] = { 0, 9, -9, 99, 11, 13 };
        const wchar_t* tensLabelsJa[] = { L"なし", L"9", L"b9", L"#9", L"11", L"13" };
        const wchar_t* tensLabelsEn[] = { L"None", L"9", L"b9", L"#9", L"11", L"13" };
        float tBtnW = (innerW - 5.0f * 4.0f) / 6.0f;
        float tBtnH = 20.0f;

        for (int i = 0; i < 6; ++i) {
            float tx = x + pad + i * (tBtnW + 4.0f);
            D2D1_RECT_F btnRc = D2D1::RectF(tx, curY, tx + tBtnW, curY + tBtnH);
            DrawButton(btnRc, (m_settingLanguage == 1) ? tensLabelsEn[i] : tensLabelsJa[i], IconType::None, false, false, m_customTension == tensVals[i]);
        }
        curY += tBtnH + 4.0f;

        // オンコードベース
        const char* bassNotes[] = { "", "C", "D", "E", "F", "G", "A", "B" };
        const wchar_t* bassLabelsJa[] = { L"ベース無", L"/C", L"/D", L"/E", L"/F", L"/G", L"/A", L"/B" };
        const wchar_t* bassLabelsEn[] = { L"No Bass", L"/C", L"/D", L"/E", L"/F", L"/G", L"/A", L"/B" };
        float bBtnW = (innerW - 3.0f * 4.0f) / 4.0f;
        float bBtnH = 19.0f;

        for (int i = 0; i < 8; ++i) {
            float bx = x + pad + (i % 4) * (bBtnW + 4.0f);
            float by = curY + (i / 4) * (bBtnH + 3.0f);
            D2D1_RECT_F btnRc = D2D1::RectF(bx, by, bx + bBtnW, by + bBtnH);
            PitchClass bpc = (i == 0) ? PitchClass::Unknown : MusicEngine::NoteToPitch(bassNotes[i]);
            DrawButton(btnRc, (m_settingLanguage == 1) ? bassLabelsEn[i] : bassLabelsJa[i], IconType::None, false, false, m_customBass == bpc);
        }
        curY += (bBtnH * 2.0f + 3.0f) + 5.0f;

        // プレビュー ＆ 追加ボタン
        std::string previewName = MusicEngine::PitchToNote(m_customRoot) + m_customSuffix;
        if (m_customTension != 0) {
            if (m_customTension == 9) previewName += "(9)";
            else if (m_customTension == -9) previewName += "(b9)";
            else if (m_customTension == 99) previewName += "(#9)";
            else if (m_customTension == 11) previewName += "(11)";
            else if (m_customTension == 13) previewName += "(13)";
        }
        if (m_customBass != PitchClass::Unknown) {
            previewName += "/" + MusicEngine::PitchToNote(m_customBass);
        }

        D2D1_RECT_F rcPreviewCard = D2D1::RectF(x + pad, curY, x + pad + 70.0f, curY + 26.0f);
        DrawCard(rcPreviewCard, ToWString(previewName), L"", false, false);

        D2D1_RECT_F rcAddBtn = D2D1::RectF(x + pad + 76.0f, curY, x + width - pad, curY + 26.0f);
        DrawButton(rcAddBtn, TR(L"このコードを追加", L"Add This Chord"), IconType::Plus, false, false, true, Theme::AccentAction);
    }

    m_pRenderTarget->PopAxisAlignedClip();

    // 最下部ステータスメッセージ表示 (不透明背景帯＋境界線でボタンとの重複を完全防止)
    D2D1_RECT_F rcStatusBg = D2D1::RectF(x, statusTopY, x + width - 1.0f, y + height);
    m_pRenderTarget->FillRectangle(rcStatusBg, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + pad, statusTopY), D2D1::Point2F(x + width - pad, statusTopY), m_pBrushBorderLight, 0.8f);
    D2D1_RECT_F rcStatus = D2D1::RectF(x + pad, statusTopY + 3.0f, x + width - pad, y + height - 2.0f);
    DrawTextSafe(m_statusMessage.empty() ? TR(L"準備完了", L"Ready") : m_statusMessage, m_pFontInspector, rcStatus, m_pBrushTextMuted);
}

// ---------------------------------------------------------------------------
// 4. 中央ペイン描画 (パートタブ・ルーラー・コード・拡大ピアノロール・鍵盤インスペクター統合ドック)
// ---------------------------------------------------------------------------
void AppWindow::RenderCenterPane(float x, float y, float width, float height) {
    D2D1_RECT_F centerClip = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->PushAxisAlignedClip(centerClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_pRenderTarget->FillRectangle(centerClip, m_pBrushBg);

    // パートタブバー (中央ペイン上部、幅 centerW)
    RenderPartTabs(x, y, width, m_partTabHeight);
    float contentY = y + m_partTabHeight;

    // 最下部: 鍵盤＋インスペクター統合ドック (鍵盤表示時 68px / 非表示時 28px)
    float curDockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
    float dockY = y + height - curDockH;
    RenderBottomDock(x, dockY, width, curDockH);

    // スクロールバー
    float scrollY = dockY - m_scrollBarHeight;
    RenderTimelineScrollBar(x, scrollY, width, m_scrollBarHeight);

    // ルーラー背景
    float rulerY = contentY;
    D2D1_RECT_F rulerRc = D2D1::RectF(x, rulerY, x + width, rulerY + m_rulerHeight);
    m_pRenderTarget->FillRectangle(rulerRc, m_pBrushCardBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, rulerY + m_rulerHeight), D2D1::Point2F(x + width, rulerY + m_rulerHeight), m_pBrushBorder, 1.0f);

    // ルーラー上の小節線・小節番号描画
    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    float barWidth = m_pixelsPerBeat * 4.0f;
    size_t numBars = std::max<size_t>(bars.size() + 16, 64);

    for (size_t b = 0; b <= numBars; ++b) {
        float bx = x + b * barWidth - m_timelineScrollX;
        if (bx < x - barWidth || bx > x + width) continue;
        if (bx >= x) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(bx, rulerY), D2D1::Point2F(bx, rulerY + m_rulerHeight), m_pBrushBorder, 1.5f);
        }
        if (b < numBars) {
            std::wstring bNum = std::to_wstring(b + 1);
            D2D1_RECT_F bNumRc = D2D1::RectF(std::max(x + 2.0f, bx + 4.0f), rulerY + 2.0f, bx + 40.0f, rulerY + m_rulerHeight);
            if (bNumRc.right > bNumRc.left) {
                DrawTextSafe(bNum, m_pFontRuler, bNumRc, m_pBrushText);
            }

            // 拍（2, 3, 4拍目）のサブ目盛りレーン
            float beatW = barWidth / 4.0f;
            for (int beat = 1; beat < 4; ++beat) {
                float beatX = bx + beat * beatW;
                if (beatX >= x && beatX <= x + width) {
                    m_pRenderTarget->DrawLine(
                        D2D1::Point2F(beatX, rulerY + m_rulerHeight - 6.0f),
                        D2D1::Point2F(beatX, rulerY + m_rulerHeight),
                        m_pBrushBorderLight, 1.0f
                    );
                    if (beatW >= 28.0f) {
                        D2D1_RECT_F beatRc = D2D1::RectF(beatX + 2.0f, rulerY + m_rulerHeight - 12.0f, beatX + 16.0f, rulerY + m_rulerHeight);
                        DrawTextSafe(std::to_wstring(beat + 1), m_pFontRuler, beatRc, m_pBrushTextMuted);
                    }
                }
            }
        }
    }

    // コードカードレーン (グリッド線はカード背景側に描画し、文字への被りを完全解消)
    float cardY = rulerY + m_rulerHeight;
    float timelineAvailH = std::max(120.0f, scrollY - cardY);
    m_cardTrackHeight = std::clamp(std::floor(timelineAvailH * 0.28f), 58.0f, 80.0f);
    RenderTimelineCards(x, cardY, width, m_cardTrackHeight);

    // ピアノロール (グリッド線はノートバーおよび左端音名ガイドの背面に描画)
    float prY = cardY + m_cardTrackHeight;
    float prH = scrollY - prY;
    if (prH > 20.0f) {
        RenderPianoRoll(x, prY, width, prH);
    }

    // 再生ヘッド
    if (m_playbackTick >= 0.0) {
        float beats = static_cast<float>(m_playbackTick / TICKS_PER_BEAT);
        float playheadX = x + (beats / 4.0f) * barWidth - m_timelineScrollX;
        if (playheadX >= x && playheadX <= x + width) {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(playheadX, rulerY),
                D2D1::Point2F(playheadX, dockY),
                m_pBrushPlayhead, 2.0f
            );
        }
    }

    m_pRenderTarget->PopAxisAlignedClip();
}

// ---------------------------------------------------------------------------
// パートタブバー描画 (中央ペイン上部にフォルダタブ形式で配置)
// ---------------------------------------------------------------------------
void AppWindow::RenderPartTabs(float x, float y, float width, float height) {
    D2D1_RECT_F tabBarRc = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(tabBarRc, m_pBrushCardBg);
    float bottomY = y + height;
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, bottomY), D2D1::Point2F(x + width, bottomY), m_pBrushBorder, 1.0f);

    auto drawFolderTab = [&](const D2D1_RECT_F& rc, const std::wstring& label, bool isActive) {
        m_pRenderTarget->FillRectangle(rc, isActive ? m_pBrushBg : m_pBrushCardBg);
        m_pRenderTarget->DrawLine(D2D1::Point2F(rc.left, rc.top), D2D1::Point2F(rc.left, rc.bottom), m_pBrushBorder, 1.0f);
        m_pRenderTarget->DrawLine(D2D1::Point2F(rc.right, rc.top), D2D1::Point2F(rc.right, rc.bottom), m_pBrushBorder, 1.0f);
        if (isActive) {
            m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.top + 3.0f), m_pBrushAccent);
            // 下辺境界線を背景色で上書きし、下のタイムライン領域と直結させる
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.left + 1.0f, bottomY), D2D1::Point2F(rc.right - 1.0f, bottomY), m_pBrushBg, 2.0f);
        } else {
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.left, rc.top), D2D1::Point2F(rc.right, rc.top), m_pBrushBorder, 1.0f);
        }
        m_pRenderTarget->PushAxisAlignedClip(D2D1::RectF(rc.left + 2.0f, rc.top, rc.right - 2.0f, rc.bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        DrawTextSafe(label, m_pFontTab, rc, isActive ? m_pBrushAccent : m_pBrushTextMuted);
        m_pRenderTarget->PopAxisAlignedClip();
    };

    const auto& parts = m_partManager.GetParts();
    int activeIdx = m_partManager.GetActiveIndex();

    // 文字列の描画幅を実測する（DirectWrite）。測れない場合は十分大きい値を返して短縮側へ倒す
    auto measureWidth = [&](const std::wstring& text) -> float {
        if (!m_pDWriteFactory || !m_pFontTab || text.empty()) return text.empty() ? 0.0f : 10000.0f;
        IDWriteTextLayout* pLayout = nullptr;
        HRESULT hr = m_pDWriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), m_pFontTab, 10000.0f, 100.0f, &pLayout);
        if (FAILED(hr) || !pLayout) return 10000.0f;
        DWRITE_TEXT_METRICS metrics{};
        pLayout->GetMetrics(&metrics);
        pLayout->Release();
        return metrics.widthIncludingTrailingWhitespace;
    };

    // タブ幅に収まる最も詳しい表記を選ぶ: 「名前 (D min)」→「名前 (Dm)」→「名前」→「名前…」
    auto fitTabLabel = [&](const Part& part, float maxW) -> std::wstring {
        std::wstring name = ToWString(part.name);
        const bool minor = (part.mode == MusicalMode::NaturalMinor || part.mode == MusicalMode::HarmonicMinor ||
                            part.mode == MusicalMode::MelodicMinor);
        std::wstring full = name + L" (" + ToWString(MusicEngine::KeyModeLabel(part.keyRoot, part.mode)) + L")";
        std::wstring compact = name + L" (" + ToWString(part.keyRoot) + (minor ? L"m" : L"") + L")";
        if (measureWidth(full) <= maxW) return full;
        if (measureWidth(compact) <= maxW) return compact;
        if (measureWidth(name) <= maxW) return name;
        // 名前だけでも収まらない場合は、末尾を省略記号に置き換えて収める
        std::wstring cut = name;
        while (!cut.empty() && measureWidth(cut + L"…") > maxW) cut.pop_back();
        return cut.empty() ? std::wstring(L"…") : cut + L"…";
    };

    // 横幅縮小時にタブや［＋パート］ボタンが隠れないよう動的縮小計算
    float rightReserve = m_showRightPane ? 8.0f : 114.0f;
    float availTotalW = std::max(200.0f, width - 8.0f - rightReserve);
    size_t totalTabs = parts.empty() ? 1 : parts.size();
    bool compactAdd = (availTotalW < totalTabs * 86.0f + 68.0f);
    float addBtnW = compactAdd ? 32.0f : 66.0f;
    float availTabsW = std::max(120.0f, availTotalW - addBtnW - 6.0f);
    float pTabW = std::clamp((availTabsW - static_cast<float>(totalTabs) * 3.0f) / static_cast<float>(totalTabs), 32.0f, 120.0f);

    float curX = x + 8.0f;
    float tabY = y + 3.0f;

    for (size_t i = 0; i < parts.size(); ++i) {
        bool isActive = (!m_isMergedView && static_cast<int>(i) == activeIdx);
        std::wstring pName = fitTabLabel(parts[i], pTabW - 12.0f);
        D2D1_RECT_F rcPart = D2D1::RectF(curX, tabY, curX + pTabW, bottomY);
        drawFolderTab(rcPart, pName, isActive);
        curX += pTabW + 3.0f;
    }

    curX += 2.0f;
    D2D1_RECT_F rcAdd = D2D1::RectF(curX, y + 3.0f, curX + addBtnW, bottomY - 3.0f);
    DrawButton(rcAdd, compactAdd ? L"" : TR(L"パート", L"Part"), IconType::Plus);

    // 右ペイン格納時は右端に展開ボタンを表示
    if (!m_showRightPane) {
        D2D1_RECT_F rcExpand = D2D1::RectF(x + width - 108.0f, y + 3.0f, x + width - 8.0f, bottomY - 3.0f);
        DrawButton(rcExpand, TR(L"« コード提案", L"« Chords"), IconType::None, false, false, true, Theme::AccentAction);
    }
}

// ---------------------------------------------------------------------------
// タイムラインコードカード描画 (グリッド線を背景に描画し、カードを前面に不透明描画)
// ---------------------------------------------------------------------------
void AppWindow::RenderTimelineCards(float x, float y, float width, float height) {
    D2D1_RECT_F bgRc = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(bgRc, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + height), D2D1::Point2F(x + width, y + height), m_pBrushBorder, 1.0f);

    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    float barWidth = m_pixelsPerBeat * 4.0f;
    size_t numBars = std::max<size_t>(bars.size() + 16, 64);
    int gridDivisions = std::clamp(TICKS_PER_BAR_4_4 / std::max(120, m_timelineGridTicks), 1, 16);

    // 1. 背景グリッド線 (カードの下層に描画するため、コード文字の上に線が被らない)
    for (size_t b = 0; b <= numBars; ++b) {
        float bx = x + b * barWidth - m_timelineScrollX;
        if (bx < x - barWidth || bx > x + width) continue;
        if (bx >= x) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(bx, y), D2D1::Point2F(bx, y + height), m_pBrushBorder, 1.5f);
        }
        if (b < numBars) {
            for (int div = 1; div < gridDivisions; ++div) {
                float subX = bx + (static_cast<float>(div) / gridDivisions) * barWidth;
                if (subX >= x && subX <= x + width) {
                    bool isQuarterBeat = ((div * 4) % gridDivisions == 0);
                    m_pRenderTarget->DrawLine(
                        D2D1::Point2F(subX, y),
                        D2D1::Point2F(subX, y + height),
                        isQuarterBeat ? m_pBrushBorder : m_pBrushBorderLight,
                        isQuarterBeat ? 0.75f : 0.40f
                    );
                }
            }
        }
    }

    // 2. コードカード本体 (不透明塗りつぶしによりカード内部のテキスト視認性を完全保護)
    float cardH = height - 10.0f;
    float cardY = y + 5.0f;

    for (size_t b = 0; b < bars.size(); ++b) {
        const auto& bar = bars[b];
        float bx = x + b * barWidth - m_timelineScrollX;
        int barTicks = bar.GetTotalTicks();
        if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

        float chordOffsetTicks = 0.0f;
        const int multiBarStart = m_multiSelTicks.empty() ? 0 : TimelineEdit::BarStartTick(bars, static_cast<int>(b));

        for (size_t ci = 0; ci < bar.chords.size(); ++ci) {
            const auto& chord = bar.chords[ci];
            int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);

            float cx = bx + (chordOffsetTicks / barTicks) * barWidth;
            float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
            int chordStartTick = multiBarStart + static_cast<int>(chordOffsetTicks);
            chordOffsetTicks += cTicks;

            if (cx + cw < x || cx > x + width) continue;

            D2D1_RECT_F cardRc = D2D1::RectF(cx + 2.0f, cardY, cx + cw - 2.0f, cardY + cardH);

            bool isSel = (static_cast<int>(b) == m_selectedBarIndex && static_cast<int>(ci) == m_selectedChordIndex);
            if (!m_isMergedView && !m_multiSelTicks.empty()) {
                // 複数選択中は選択集合に含まれるカードだけを強調する（主選択が外れていれば強調しない）
                isSel = std::find(m_multiSelTicks.begin(), m_multiSelTicks.end(), chordStartTick) != m_multiSelTicks.end();
            }
            if (chord.isRest) {
                // 休符・空白区間はカードを描画せず、背景グリッドを透過させてDAWライクな空白を表現
                if (isSel) {
                    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(cardRc, 6.0f, 6.0f);
                    m_pRenderTarget->DrawRoundedRectangle(rr, m_pBrushAccent, 1.5f);
                }
                continue;
            }

            std::wstring cName = ToWString(chord.rootName + chord.qualitySuffix);
            if (!chord.bassName.empty()) cName += L"/" + ToWString(chord.bassName);

            std::wstring degree = ToWString(chord.degreeRoman);
            if (chord.octaveOffset != 0) {
                degree += (chord.octaveOffset > 0) ? L" +" : L" ";
                degree += std::to_wstring(chord.octaveOffset);
                degree += L"oct";
            }

            DrawCard(cardRc, cName, degree, false, isSel, false, true, false);
        }
    }
}

// ---------------------------------------------------------------------------
// ピアノロール描画 (グリッド線をノートバー背面に配置し、ノート音名・左端ガイドへの被りを解消)
// ---------------------------------------------------------------------------
void AppWindow::RenderPianoRoll(float x, float y, float width, float height) {
    D2D1_RECT_F prRc = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(prRc, m_pBrushCardBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    float barWidth = m_pixelsPerBeat * 4.0f;
    float guideW = 28.0f;

    // 現在のコード群の構成音レンジを計測し、無駄な上下音域を削って1行高を最大化する
    int actualMin = 127;
    int actualMax = 0;
    for (const auto& bar : bars) {
        for (const auto& chord : bar.chords) {
            if (!chord.isRest && chord.rootPitch != PitchClass::Unknown) {
                auto notes = MusicEngine::GetChordMidiNotes(chord, m_baseOctave, m_selectedVoicing, chord.inversion, m_bassOctaveMode);
                for (int n : notes) {
                    actualMin = std::min(actualMin, n);
                    actualMax = std::max(actualMax, n);
                }
            }
        }
    }

    int minMidi = 55; // G3
    int maxMidi = 72; // C5
    if (actualMin <= actualMax) {
        minMidi = actualMin - 1;
        maxMidi = actualMax + 1;
        int span = maxMidi - minMidi + 1;
        if (span < 15) {
            int padLow = (15 - span) / 2;
            int padHigh = 15 - span - padLow;
            minMidi -= padLow;
            maxMidi += padHigh;
        } else if (span > 22) {
            minMidi = actualMax - 21;
        }
    }
    float noteRowH = height / static_cast<float>(maxMidi - minMidi + 1);

    // 1. 水平の黒鍵/白鍵背景帯
    for (int p = minMidi; p <= maxMidi; ++p) {
        float py = y + height - (p - minMidi + 1) * noteRowH;
        int noteInOctave = p % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        if (isBlackKey) {
            m_pRenderTarget->FillRectangle(D2D1::RectF(x + guideW, py, x + width, py + noteRowH), m_pBrushBorderLight);
        }
        if (noteInOctave == 0) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(x + guideW, py + noteRowH), D2D1::Point2F(x + width, py + noteRowH), m_pBrushBorder, 0.8f);
        }
    }

    // 2. 垂直グリッド線 (ノートバーの背面に描画し、左端ガイド列 x+guideW より右側のみに制限)
    size_t numBars = std::max<size_t>(bars.size() + 16, 64);
    int gridDivisions = std::clamp(TICKS_PER_BAR_4_4 / std::max(120, m_timelineGridTicks), 1, 16);
    for (size_t b = 0; b <= numBars; ++b) {
        float bx = x + b * barWidth - m_timelineScrollX;
        if (bx < x - barWidth || bx > x + width) continue;

        if (bx >= x + guideW) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(bx, y), D2D1::Point2F(bx, y + height), m_pBrushBorder, 1.4f);
        }

        if (b < numBars) {
            for (int div = 1; div < gridDivisions; ++div) {
                float subX = bx + (static_cast<float>(div) / gridDivisions) * barWidth;
                if (subX >= x + guideW && subX <= x + width) {
                    bool isQuarterBeat = ((div * 4) % gridDivisions == 0);
                    m_pRenderTarget->DrawLine(
                        D2D1::Point2F(subX, y),
                        D2D1::Point2F(subX, y + height),
                        isQuarterBeat ? m_pBrushBorder : m_pBrushBorderLight,
                        isQuarterBeat ? 0.75f : 0.40f
                    );
                }
            }
        }
    }

    if (m_pFontCardDegree) {
        m_pFontCardDegree->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // 3. ノートバー ＆ 音名ラベル (グリッド線の前面に描画)
    for (size_t b = 0; b < bars.size(); ++b) {
        const auto& bar = bars[b];
        float bx = x + b * barWidth - m_timelineScrollX;
        int barTicks = bar.GetTotalTicks();
        if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

        float offsetTicks = 0.0f;

        for (const auto& chord : bar.chords) {
            int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
            float cx = bx + (offsetTicks / barTicks) * barWidth;
            float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
            offsetTicks += cTicks;

            if (cx + cw < x + guideW || cx > x + width) continue;

            if (!chord.isRest && chord.rootPitch != PitchClass::Unknown) {
                auto notes = MusicEngine::GetChordMidiNotes(chord, m_baseOctave, m_selectedVoicing, chord.inversion, m_bassOctaveMode);
                for (int n : notes) {
                    if (n >= minMidi && n <= maxMidi) {
                        float ny = y + height - (n - minMidi + 1) * noteRowH;
                        D2D1_RECT_F noteRc = D2D1::RectF(std::max(x + guideW + 1.0f, cx + 1.5f), ny + 0.5f, cx + cw - 1.5f, ny + noteRowH - 0.5f);
                        if (noteRc.right > noteRc.left) {
                            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(noteRc, 2.5f, 2.5f), m_pBrushPianoRollNote);

                            if ((noteRc.right - noteRc.left) >= 22.0f && noteRowH >= 9.0f) {
                                m_pRenderTarget->PushAxisAlignedClip(noteRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                                std::string nName = MusicEngine::PitchToNoteForKey(static_cast<PitchClass>(n % 12), keyRoot, mode) + std::to_string(n / 12 - 1);
                                D2D1_RECT_F lblRc = D2D1::RectF(noteRc.left + 4.0f, ny - 2.0f, noteRc.right - 2.0f, ny + noteRowH + 2.0f);
                                DrawTextSafe(ToWString(nName), m_pFontCardDegree, lblRc, m_pBrushBg);
                                m_pRenderTarget->PopAxisAlignedClip();
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. 左端の音程ガイド列 (最前面に不透明描画し、スクロール時の重なりを完全防止)
    m_pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + guideW, y + height), m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + guideW, y), D2D1::Point2F(x + guideW, y + height), m_pBrushBorderLight, 1.0f);
    for (int p = minMidi; p <= maxMidi; ++p) {
        int noteInOctave = p % 12;
        if (noteInOctave == 0 || noteInOctave == 5 || noteInOctave == 7) {
            float py = y + height - (p - minMidi + 1) * noteRowH;
            const wchar_t* nChar = (noteInOctave == 0) ? L"C" : ((noteInOctave == 5) ? L"F" : L"G");
            std::wstring octLbl = std::wstring(nChar) + std::to_wstring(p / 12 - 1);
            D2D1_RECT_F gRc = D2D1::RectF(x + 3.0f, py - 2.0f, x + guideW - 1.0f, py + noteRowH + 2.0f);
            DrawTextSafe(octLbl, m_pFontCardDegree, gRc, noteInOctave == 0 ? m_pBrushText : m_pBrushTextMuted);
        }
    }

    if (m_pFontCardDegree) {
        m_pFontCardDegree->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }
}

// ---------------------------------------------------------------------------
// 鍵盤 ＆ インスペクター統合ドック (鍵盤表示切替対応)
// ---------------------------------------------------------------------------
void AppWindow::RenderBottomDock(float x, float y, float width, float height) {
    D2D1_RECT_F dockRc = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(dockRc, m_pBrushCardBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> dockBars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});

    float inspX = x + 12.0f;

    if (m_showKeyboard) {
        // 左側: コンパクト鍵盤プレビュー (幅 360px)
        float kbWidth = std::min(360.0f, width * 0.45f);
        float kbX = x + 12.0f;
        float kbY = y + 6.0f;
        float kbH = height - 12.0f;

        std::vector<int> activeNotes;
        if (m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(dockBars.size())) {
            const auto& bar = dockBars[m_selectedBarIndex];
            if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
                const auto& selChord = bar.chords[m_selectedChordIndex];
                activeNotes = MusicEngine::GetChordMidiNotes(selChord, m_baseOctave, m_selectedVoicing, selChord.inversion, m_bassOctaveMode);
            }
        }

        int startNote = 48; // C3
        int numWhiteKeys = 21;
        float whiteKeyW = (kbWidth - 24.0f) / numWhiteKeys;
        float whiteKeyH = kbH;

        int curWhite = 0;
        for (int n = startNote; curWhite < numWhiteKeys; ++n) {
            int ni = n % 12;
            bool isBlack = (ni == 1 || ni == 3 || ni == 6 || ni == 8 || ni == 10);
            if (!isBlack) {
                float wx = kbX + curWhite * whiteKeyW;
                D2D1_RECT_F wRc = D2D1::RectF(wx, kbY, wx + whiteKeyW - 1.0f, kbY + whiteKeyH);
                bool isHighlighted = (std::find(activeNotes.begin(), activeNotes.end(), n) != activeNotes.end());
                m_pRenderTarget->FillRectangle(wRc, isHighlighted ? m_pBrushAccent : m_pBrushBg);
                m_pRenderTarget->DrawRectangle(wRc, m_pBrushBorder, 1.0f);
                curWhite++;
            }
        }

        curWhite = 0;
        float blackKeyW = whiteKeyW * 0.65f;
        float blackKeyH = whiteKeyH * 0.60f;

        for (int n = startNote; curWhite < numWhiteKeys; ++n) {
            int ni = n % 12;
            bool isBlack = (ni == 1 || ni == 3 || ni == 6 || ni == 8 || ni == 10);
            if (isBlack) {
                float bx = kbX + curWhite * whiteKeyW - blackKeyW * 0.5f;
                D2D1_RECT_F bRc = D2D1::RectF(bx, kbY, bx + blackKeyW, kbY + blackKeyH);
                bool isHighlighted = (std::find(activeNotes.begin(), activeNotes.end(), n) != activeNotes.end());
                m_pRenderTarget->FillRectangle(bRc, isHighlighted ? m_pBrushAccent : m_pBrushText);
                m_pRenderTarget->DrawRectangle(bRc, m_pBrushBorder, 1.0f);
            } else {
                curWhite++;
            }
        }

        float sepX = kbX + kbWidth;
        m_pRenderTarget->DrawLine(D2D1::Point2F(sepX, y + 8.0f), D2D1::Point2F(sepX, y + height - 8.0f), m_pBrushBorderLight, 1.0f);
        inspX = sepX + 16.0f;
    }

    // インスペクター情報
    std::wstring mainChordText = TR(L"コード: 選択なし", L"Chord: None");
    std::wstring detailsText = L"";

    if (m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(dockBars.size())) {
        const auto& bar = dockBars[m_selectedBarIndex];
        if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
            const auto& chord = bar.chords[m_selectedChordIndex];
            mainChordText = std::wstring(TR(L"コード: ", L"Chord: ")) + ToWString(chord.rootName + chord.qualitySuffix);
            if (!chord.bassName.empty()) mainChordText += L"/" + ToWString(chord.bassName);
            if (!chord.degreeRoman.empty()) mainChordText += L" (" + ToWString(chord.degreeRoman) + L")";

            if (m_settingLanguage == 1) {
                detailsText = L"Length: " + std::to_wstring(chord.durationBeats) + (chord.durationBeats == 1 ? L" beat" : L" beats");
            } else {
                detailsText = L"長さ: " + std::to_wstring(chord.durationBeats) + L"拍";
            }

            auto notes = MusicEngine::GetChordMidiNotes(chord, m_baseOctave, m_selectedVoicing, chord.inversion, m_bassOctaveMode);
            if (!notes.empty()) {
                detailsText += TR(L" | 構成音: ", L" | Notes: ");
                for (size_t ni = 0; ni < notes.size(); ++ni) {
                    detailsText += ToWString(MusicEngine::PitchToNote(static_cast<PitchClass>(notes[ni] % 12)));
                    detailsText += std::to_wstring(notes[ni] / 12 - 1) + L" ";
                }
            }
        }
    }

    float rightControlsW = 132.0f;
    if (m_showKeyboard) {
        D2D1_RECT_F rcMainText = D2D1::RectF(inspX, y + 10.0f, x + width - rightControlsW, y + 32.0f);
        DrawTextSafe(mainChordText, m_pFontInspector, rcMainText, m_pBrushText);

        D2D1_RECT_F rcDetails = D2D1::RectF(inspX, y + 34.0f, x + width - rightControlsW, y + 60.0f);
        DrawTextSafe(detailsText, m_pFontInspector, rcDetails, m_pBrushTextMuted);
    } else {
        std::wstring combined = mainChordText + (detailsText.empty() ? L"" : (L"   |   " + detailsText));
        D2D1_RECT_F rcSingle = D2D1::RectF(inspX, y + 5.0f, x + width - rightControlsW, y + height - 3.0f);
        DrawTextSafe(combined, m_pFontInspector, rcSingle, m_pBrushText);
    }

    // 鍵盤切替アイコンボタン ＆ 虫眼鏡ズームコントロール
    float ctrlY = y + (height - 22.0f) * 0.5f;
    float kbToggleX = x + width - 124.0f;
    D2D1_RECT_F rcKbToggle = D2D1::RectF(kbToggleX, ctrlY, kbToggleX + 32.0f, ctrlY + 22.0f);
    DrawButton(rcKbToggle, L"", IconType::Keyboard, false, false, m_showKeyboard);

    float zoomX = x + width - 84.0f;
    D2D1_RECT_F rcZoomIcon = D2D1::RectF(zoomX, ctrlY + 1.0f, zoomX + 20.0f, ctrlY + 21.0f);
    DrawIcon(IconType::Zoom, rcZoomIcon, m_pBrushTextMuted);

    D2D1_RECT_F rcMinus = D2D1::RectF(zoomX + 22.0f, ctrlY, zoomX + 46.0f, ctrlY + 22.0f);
    DrawButton(rcMinus, L"-", IconType::None);

    D2D1_RECT_F rcPlus = D2D1::RectF(zoomX + 50.0f, ctrlY, zoomX + 74.0f, ctrlY + 22.0f);
    DrawButton(rcPlus, L"+", IconType::None);
}

// ---------------------------------------------------------------------------
// タイムラインスクロールバー描画
// ---------------------------------------------------------------------------
void AppWindow::RenderTimelineScrollBar(float x, float y, float width, float height) {
    D2D1_RECT_F sbRc = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(sbRc, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), m_pBrushBorder, 1.0f);

    Part* pActive = m_partManager.GetActivePart();
    size_t actualBars = m_isMergedView ? m_partManager.GenerateMergedProgression().size() : (pActive ? pActive->bars.size() : 0);
    size_t numBars = std::max<size_t>(actualBars + 16, 64);

    float totalContentWidth = numBars * (m_pixelsPerBeat * 4.0f);
    float viewWidth = width;

    if (totalContentWidth <= viewWidth) return;

    float thumbWidth = std::max(30.0f, (viewWidth / totalContentWidth) * width);
    float maxScroll = totalContentWidth - viewWidth;
    float scrollRatio = std::clamp(m_timelineScrollX / maxScroll, 0.0f, 1.0f);
    float thumbX = x + scrollRatio * (width - thumbWidth);

    D2D1_ROUNDED_RECT thumbRc = D2D1::RoundedRect(D2D1::RectF(thumbX, y + 2.0f, thumbX + thumbWidth, y + height - 2.0f), 3.0f, 3.0f);
    m_pRenderTarget->FillRoundedRectangle(thumbRc, m_pBrushBorder);
}

// ---------------------------------------------------------------------------
// 5. 右ペイン描画 (音楽理論・提案パレット)
// ---------------------------------------------------------------------------
void AppWindow::RenderRightPane(float x, float y, float width, float height) {
    D2D1_RECT_F paneRect = D2D1::RectF(x, y, x + width, y + height);
    m_pRenderTarget->FillRectangle(paneRect, m_pBrushBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x, y + height), m_pBrushBorder, 1.0f);

    float curY = y + 6.0f;
    float pad = 12.0f;
    float innerW = width - pad * 2.0f;

    bool use7th = ((m_diatonicChordTypeTab % 5) == 1) || (m_diatonicChordTypeTab >= 5);

    D2D1_RECT_F titleRc = D2D1::RectF(x + pad, curY, x + width - pad - 126.0f, curY + 20.0f);
    DrawTextSafe(TR(L"コード提案", L"Chord Ideas"), m_pFontInspector, titleRc, m_pBrushText);

    D2D1_RECT_F rcMode3 = D2D1::RectF(x + width - pad - 122.0f, curY - 1.0f, x + width - pad - 90.0f, curY + 19.0f);
    D2D1_RECT_F rcMode7 = D2D1::RectF(x + width - pad - 88.0f, curY - 1.0f, x + width - pad - 58.0f, curY + 19.0f);
    DrawButton(rcMode3, TR(L"3和音", L"Triad"), IconType::None, false, false, !use7th);
    DrawButton(rcMode7, L"7th", IconType::None, false, false, use7th);

    D2D1_RECT_F rcCollapse = D2D1::RectF(x + width - pad - 52.0f, curY - 1.0f, x + width - pad, curY + 19.0f);
    DrawButton(rcCollapse, TR(L"» 格納", L"» Hide"), IconType::None);
    curY += 22.0f;

    // 1. 次のコード候補
    D2D1_RECT_F hSug = D2D1::RectF(x + pad, curY, x + width - pad, curY + 21.0f);
    DrawSectionHeader(hSug, TR(L"次のコード候補", L"Next Chord Suggestions"), m_accSuggestionsOpen);
    curY += 24.0f;

    if (m_accSuggestionsOpen) {
        auto suggestions = BuildSuggestionList(use7th);
        float sCardW = (innerW - 3.0f * 4.0f) / 4.0f;
        float sCardH = 38.0f;

        for (size_t i = 0; i < std::min<size_t>(suggestions.size(), 4); ++i) {
            float cx = x + pad + i * (sCardW + 4.0f);
            D2D1_RECT_F cardRc = D2D1::RectF(cx, curY, cx + sCardW, curY + sCardH);
            const auto& chord = suggestions[i];
            DrawCard(cardRc, ToWString(chord.rootName + chord.qualitySuffix), ToWString(chord.degreeRoman), false, false);
        }
        curY += sCardH + 7.0f;
    }

    // 2. 借用コード
    D2D1_RECT_F hBorrow = D2D1::RectF(x + pad, curY, x + width - pad, curY + 21.0f);
    DrawSectionHeader(hBorrow, TR(L"借用コード (エモ・転調感)", L"Borrowed Chords (Modal)"), m_accBorrowOpen);
    curY += 24.0f;

    if (m_accBorrowOpen) {
        auto borrowChords = BuildBorrowedList(use7th);
        float bCardW = (innerW - 2.0f * 4.0f) / 3.0f;
        float bCardH = 38.0f;

        for (size_t i = 0; i < std::min<size_t>(borrowChords.size(), 6); ++i) {
            float cx = x + pad + (i % 3) * (bCardW + 4.0f);
            float cy = curY + (i / 3) * (bCardH + 4.0f);
            D2D1_RECT_F cardRc = D2D1::RectF(cx, cy, cx + bCardW, cy + bCardH);
            const auto& chord = borrowChords[i];
            DrawCard(cardRc, ToWString(chord.rootName + chord.qualitySuffix), ToWString(chord.degreeRoman), false, false);
        }
        curY += (bCardH * 2.0f + 4.0f) + 7.0f;
    }

    // 3. 繋ぎコード
    D2D1_RECT_F hSec = D2D1::RectF(x + pad, curY, x + width - pad, curY + 21.0f);
    DrawSectionHeader(hSec, TR(L"繋ぎコード (セカンダリー)", L"Secondary Dominants"), m_accSecondaryOpen);
    curY += 24.0f;

    if (m_accSecondaryOpen) {
        auto secChords = BuildSecondaryList();
        float scCardW = (innerW - 2.0f * 4.0f) / 3.0f;
        float scCardH = 38.0f;

        for (size_t i = 0; i < std::min<size_t>(secChords.size(), 5); ++i) {
            float cx = x + pad + (i % 3) * (scCardW + 4.0f);
            float cy = curY + (i / 3) * (scCardH + 4.0f);
            D2D1_RECT_F cardRc = D2D1::RectF(cx, cy, cx + scCardW, cy + scCardH);
            const auto& chord = secChords[i];
            DrawCard(cardRc, ToWString(chord.rootName + chord.qualitySuffix), ToWString(chord.degreeRoman), false, false);
        }
        curY += (scCardH * 2.0f + 4.0f) + 7.0f;
    }

    // 4. 定番テンプレート
    D2D1_RECT_F hTpl = D2D1::RectF(x + pad, curY, x + width - pad, curY + 21.0f);
    DrawSectionHeader(hTpl, TR(L"定番テンプレート", L"Quick Templates"), m_accTemplatesOpen);
    curY += 24.0f;

    if (m_accTemplatesOpen) {
        const auto& templates = MusicEngine::GetAllGenreTemplates();
        float tplH = 22.0f;

        for (size_t i = 0; i < std::min<size_t>(templates.size(), 6); ++i) {
            D2D1_RECT_F tplRc = D2D1::RectF(x + pad, curY, x + width - pad, curY + tplH);
            DrawButton(tplRc, GetLocalizedTemplateName(templates[i], m_settingLanguage == 1), IconType::None);
            curY += tplH + 3.0f;
        }
    }
}
} // namespace clp
