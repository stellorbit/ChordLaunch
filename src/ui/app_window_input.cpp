#include "app_window_internal.hpp"
namespace clp {
void AppWindow::OnLButtonDown(float x, float y) {
    SetFocus(m_hwnd);

    if (OnModalClick(x, y)) return;

    // ---------------- 1. 上部ツールバー (2段構成。座標は描画と共通のレイアウト) ----------------
    if (y >= 0 && y <= m_toolbarHeight) {
        const ToolbarLayout L = ComputeToolbarLayout(m_width);

        // --- 上段 (y = 0 〜 38px) ---
        if (y <= 38.0f) {
            if (PtInRectF(L.play, x, y)) {
                TogglePlayback();
                return;
            }
            if (PtInRectF(L.stop, x, y)) {
                StopPlayback();
                return;
            }
            if (PtInRectF(L.loop, x, y)) {
                m_isLooping = !m_isLooping;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
            // 全体マージ (新規パート生成)
            if (PtInRectF(L.merge, x, y)) {
                DoMergeAllToNewPart();
                return;
            }
            if (PtInRectF(L.settings, x, y)) {
                OpenModal(ModalType::Settings);
                return;
            }
            if (PtInRectF(L.help, x, y)) {
                OpenModal(ModalType::Help);
                return;
            }
            if (PtInRectF(L.templates, x, y)) {
                OpenModal(ModalType::Templates);
                return;
            }
            if (PtInRectF(L.dawDrag, x, y)) {
                StartDawDrag();
                return;
            }
            if (PtInRectF(L.midiSave, x, y)) {
                DoMidiSaveAs();
                return;
            }
            return;
        }

        // --- 下段 (y = 38 〜 74px): キー, スケール, BPM, TAP, 音色, ボイシング, ベース, グリッド ---
        // ラベル／値のどちらをクリックしても同じプルダウンを開く
        auto openMenu = [&](const D2D1_RECT_F& lbl, const D2D1_RECT_F& val, void (AppWindow::*show)(float, float)) -> bool {
            if (!PtInRectF(lbl, x, y) && !PtInRectF(val, x, y)) return false;
            POINT btnPt = { static_cast<LONG>(lbl.left), static_cast<LONG>(val.bottom) };
            ClientToScreen(m_hwnd, &btnPt);
            (this->*show)(static_cast<float>(btnPt.x), static_cast<float>(btnPt.y));
            return true;
        };

        if (openMenu(L.keyLbl, L.key, &AppWindow::ShowKeyMenu)) return;
        if (openMenu(L.scaleLbl, L.scale, &AppWindow::ShowScaleMenu)) return;

        // BPM 上下スピンボタン
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int step = isShift ? 5 : 1;

        if (PtInRectF(L.spinUp, x, y)) {
            Part* pActive = m_partManager.GetActivePart();
            if (pActive) {
                const bool linked = (GetKeyState(VK_CONTROL) & 0x8000) == 0;
                m_partManager.ApplyBpm(std::min(280, pActive->bpm + step), linked);
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return;
        }
        if (PtInRectF(L.spinDown, x, y)) {
            Part* pActive = m_partManager.GetActivePart();
            if (pActive) {
                const bool linked = (GetKeyState(VK_CONTROL) & 0x8000) == 0;
                m_partManager.ApplyBpm(std::max(40, pActive->bpm - step), linked);
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return;
        }

        // TAP テンポ (モーダル起動)
        if (PtInRectF(L.tap, x, y)) {
            OpenModal(ModalType::TapTempo);
            HandleTapTempo();
            return;
        }

        if (openMenu(L.soundLbl, L.sound, &AppWindow::ShowPresetMenu)) return;
        if (openMenu(L.voicingLbl, L.voicing, &AppWindow::ShowVoicingMenu)) return;
        if (openMenu(L.bassLbl, L.bass, &AppWindow::ShowBassMenu)) return;
        if (openMenu(L.inputLbl, L.input, &AppWindow::ShowInputGridMenu)) return;
        if (openMenu(L.gridLbl, L.grid, &AppWindow::ShowGridMenu)) return;
        return;
    }

    // ---------------- 2. 左ペイン ----------------
    float mainY = m_toolbarHeight;
    if (x >= 0 && x <= m_leftPaneWidth && y > mainY && y < m_height - 22.0f) {
        // 描画と共通のレイアウト（上から: ダイアトニック → テキスト入力 → カスタムコードビルダー）
        const LeftPaneLayout L = ComputeLeftPaneLayout(mainY, m_leftPaneWidth);
        const float pad = L.pad;
        const float innerW = L.innerW;

        // ダイアトニック タブ (3和音 / 7th / その他▼)
        for (int t = 0; t < 3; ++t) {
            D2D1_RECT_F rcTab = GridCellRect(pad, innerW, 3, t, L.tabY, L.tabH, L.gap);
            if (!PtInRectF(rcTab, x, y)) continue;
            if (t < 2) {
                m_diatonicChordTypeTab = t;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            } else {
                POINT btnPt = { static_cast<LONG>(rcTab.left), static_cast<LONG>(rcTab.bottom) };
                ClientToScreen(m_hwnd, &btnPt);
                ShowDiatonicVariantMenu(static_cast<float>(btnPt.x), static_cast<float>(btnPt.y));
            }
            return;
        }

        // ダイアトニックカード
        Part* pActive = m_partManager.GetActivePart();
        std::string keyRoot = pActive ? pActive->keyRoot : "C";
        MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;
        auto diatonicChords = MusicEngine::GetDiatonicChordsByTab(keyRoot, mode, m_diatonicChordTypeTab);

        for (size_t i = 0; i < 8; ++i) {
            float cy = L.cardY + static_cast<float>(i / 4) * (L.cardH + L.gap);
            D2D1_RECT_F cardRc = GridCellRect(pad, innerW, 4, static_cast<int>(i % 4), cy, L.cardH, L.gap);
            if (PtInRectF(cardRc, x, y)) {
                ChordToken newChord;
                if (i < 7 && i < diatonicChords.size()) {
                    newChord = diatonicChords[i];
                } else {
                    newChord.isRest = true;
                    newChord.rootName = "R";
                    newChord.degreeRoman = "R";
                }
                newChord.durationTicks = m_inputGridTicks;
                newChord.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);

                AuditionChord(newChord);
                m_dragMode = DragMode::PaletteCardDrag;
                m_dragPaletteChord = newChord;
                m_dragStartX = x;
                m_dragStartY = y;
                m_dragCurrentX = x;
                m_dragCurrentY = y;
                m_isPaletteDragging = false;
                SetCapture(m_hwnd);
                return;
            }
        }
        // 反映ボタン（テキスト入力 → タイムライン追記、反映後にテキストを消去）
        if (PtInRectF(D2D1::RectF(pad, L.applyY, pad + innerW, L.applyY + L.applyH), x, y)) {
            SyncTextToTimeline();
            return;
        }

        // カスタムコードビルダー格納・展開ヘッダー
        if (PtInRectF(D2D1::RectF(pad, L.builderHeaderY, pad + innerW, L.builderHeaderY + 19.0f), x, y)) {
            m_accCustomBuilderOpen = !m_accCustomBuilderOpen;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }

        if (!m_accCustomBuilderOpen) return;

        // ルート音選択 (6列)
        const char* roots[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        for (int i = 0; i < 12; ++i) {
            float ry = L.rootsY + static_cast<float>(i / 6) * (L.btnH + L.rowGap);
            if (PtInRectF(GridCellRect(pad, innerW, 6, i % 6, ry, L.btnH, L.gap), x, y)) {
                m_customRoot = MusicEngine::NoteToPitch(roots[i]);
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }

        // サフィックス選択 (4列。選択中コードカードのクイック変換にも対応)
        const char* suffixes[] = { "", "m", "7", "Maj7", "m7", "m7b5", "dim7", "sus4" };
        for (int i = 0; i < 8; ++i) {
            float sy = L.suffixY + static_cast<float>(i / 4) * (L.btnH + L.rowGap);
            if (PtInRectF(GridCellRect(pad, innerW, 4, i % 4, sy, L.btnH, L.gap), x, y)) {
                m_customSuffix = suffixes[i];
                if (!m_isMergedView && !m_isPlaying && pActive && m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(pActive->bars.size())) {
                    auto& bar = pActive->bars[m_selectedBarIndex];
                    if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
                        auto& selC = bar.chords[m_selectedChordIndex];
                        if (!selC.isRest && !selC.rootName.empty()) {
                            PushUndo();
                            std::string newSym = selC.rootName + m_customSuffix;
                            if (!selC.bassName.empty()) newSym += "/" + selC.bassName;
                            ChordToken updated = MusicEngine::ParseChordSymbol(newSym, keyRoot, mode);
                            updated.durationTicks = selC.durationTicks;
                            updated.durationBeats = selC.durationBeats;
                            updated.inversion = selC.inversion;
                            selC = updated;
                            SyncTimelineToText();
                            AuditionChord(selC);
                        }
                    }
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }

        // テンション選択 (6列)
        const int8_t tensVals[] = { 0, 9, -9, 99, 11, 13 };
        for (int i = 0; i < 6; ++i) {
            if (PtInRectF(GridCellRect(pad, innerW, 6, i, L.tensionY, L.btnH, L.gap), x, y)) {
                m_customTension = tensVals[i];
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }

        // ベース音選択 (4列。選択中コードカードのクイック分数コード変換にも対応)
        const char* bassNotes[] = { "", "C", "D", "E", "F", "G", "A", "B" };
        for (int i = 0; i < 8; ++i) {
            float by = L.bassY + static_cast<float>(i / 4) * (L.btnH + L.rowGap);
            if (PtInRectF(GridCellRect(pad, innerW, 4, i % 4, by, L.btnH, L.gap), x, y)) {
                m_customBass = (i == 0) ? PitchClass::Unknown : MusicEngine::NoteToPitch(bassNotes[i]);
                if (!m_isMergedView && !m_isPlaying && pActive && m_selectedBarIndex >= 0 && m_selectedBarIndex < static_cast<int>(pActive->bars.size())) {
                    auto& bar = pActive->bars[m_selectedBarIndex];
                    if (m_selectedChordIndex >= 0 && m_selectedChordIndex < static_cast<int>(bar.chords.size())) {
                        auto& selC = bar.chords[m_selectedChordIndex];
                        if (!selC.isRest && !selC.rootName.empty()) {
                            PushUndo();
                            std::string newSym = selC.rootName + selC.qualitySuffix;
                            if (m_customBass != PitchClass::Unknown && m_customBass != selC.rootPitch) {
                                newSym += "/" + std::string(bassNotes[i]);
                            }
                            ChordToken updated = MusicEngine::ParseChordSymbol(newSym, keyRoot, mode);
                            updated.durationTicks = selC.durationTicks;
                            updated.durationBeats = selC.durationBeats;
                            updated.inversion = selC.inversion;
                            selC = updated;
                            SyncTimelineToText();
                            AuditionChord(selC);
                        }
                    }
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }

        // プレビューカードクリックで試聴
        if (PtInRectF(D2D1::RectF(pad, L.addY, pad + 70.0f, L.addY + 26.0f), x, y)) {
            std::string sym = MusicEngine::PitchToNote(m_customRoot) + m_customSuffix;
            if (m_customBass != PitchClass::Unknown) sym += "/" + MusicEngine::PitchToNote(m_customBass);
            ChordToken c = MusicEngine::ParseChordSymbol(sym, keyRoot, mode);
            if (m_customTension != 0) c.tensions.push_back(m_customTension);
            AuditionChord(c);
            return;
        }

        // 追加ボタン
        if (PtInRectF(D2D1::RectF(pad + 70.0f + L.gap, L.addY, pad + innerW, L.addY + 26.0f), x, y)) {
            std::string sym = MusicEngine::PitchToNote(m_customRoot) + m_customSuffix;
            if (m_customTension != 0) {
                if (m_customTension == 9) sym += "(9)";
                else if (m_customTension == -9) sym += "(b9)";
                else if (m_customTension == 99) sym += "(#9)";
                else if (m_customTension == 11) sym += "(11)";
                else if (m_customTension == 13) sym += "(13)";
            }
            if (m_customBass != PitchClass::Unknown) sym += "/" + MusicEngine::PitchToNote(m_customBass);
            ChordToken c = MusicEngine::ParseChordSymbol(sym, keyRoot, mode);
            c.durationTicks = m_inputGridTicks;
            c.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);

            AuditionChord(c);
            AppendChordToTimeline(c);
            // 追加直後のカードを選択したままにすると、続けてビルダーを操作した際に
            // いま追加したコードを上書きしてしまうため、選択を解除する
            ClearSelection();
            return;
        }
        return;
    }

    // ---------------- 3. 中央ペイン ----------------
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float centerX = m_leftPaneWidth;
    float centerW = m_width - rightW - centerX;

    if (x >= centerX && x <= centerX + centerW) {
        // パートタブバー
        if (y >= mainY && y <= mainY + m_partTabHeight) {
            if (!m_showRightPane && PtInRectF(D2D1::RectF(centerX + centerW - 108.0f, mainY + 3.0f, centerX + centerW - 8.0f, mainY + m_partTabHeight - 3.0f), x, y)) {
                m_showRightPane = true;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }

            const auto& parts = m_partManager.GetParts();
            float rightReserve = m_showRightPane ? 8.0f : 114.0f;
            float availTotalW = std::max(200.0f, centerW - 8.0f - rightReserve);
            size_t totalTabs = parts.empty() ? 1 : parts.size();
            bool compactAdd = (availTotalW < totalTabs * 86.0f + 68.0f);
            float addBtnW = compactAdd ? 32.0f : 66.0f;
            float availTabsW = std::max(120.0f, availTotalW - addBtnW - 6.0f);
            float pTabW = std::clamp((availTabsW - static_cast<float>(totalTabs) * 3.0f) / static_cast<float>(totalTabs), 32.0f, 120.0f);

            float curX = centerX + 8.0f;
            float tabH = m_partTabHeight - 3.0f;
            float ty = mainY + 3.0f;

            for (size_t i = 0; i < parts.size(); ++i) {
                if (PtInRectF(D2D1::RectF(curX, ty, curX + pTabW, ty + tabH), x, y)) {
                    m_isMergedView = false;
                    m_partManager.SetActiveIndex(static_cast<int>(i));
                    ClearSelection(); // 別パートの添字・tickを持ち越さない
                    SyncTimelineToText();
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                curX += pTabW + 3.0f;
            }

            curX += 2.0f;
            if (PtInRectF(D2D1::RectF(curX, ty, curX + addBtnW, mainY + m_partTabHeight - 3.0f), x, y)) {
                PushUndo();
                m_isMergedView = false;
                m_partManager.AddPart("New Part");
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
            return;
        }

        // 下部ドック (鍵盤試聴 ＆ 鍵盤切替 ＆ ズームボタン)
        float curDockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
        float dockY = m_height - curDockH;
        if (y >= dockY && y <= m_height) {
            if (m_showKeyboard) {
                float kbWidth = std::min(360.0f, centerW * 0.45f);
                float kbX = centerX + 12.0f;
                float kbY = dockY + 6.0f;
                float kbH = curDockH - 12.0f;
                if (PtInRectF(D2D1::RectF(kbX, kbY, kbX + kbWidth - 24.0f, kbY + kbH), x, y)) {
                    int startNote = 48;
                    int numWhiteKeys = 21;
                    float whiteKeyW = (kbWidth - 24.0f) / numWhiteKeys;
                    float blackKeyW = whiteKeyW * 0.65f;
                    float blackKeyH = kbH * 0.60f;
                    int curWhite = 0;
                    int clickedNote = -1;
                    for (int n = startNote; curWhite < numWhiteKeys; ++n) {
                        int ni = n % 12;
                        bool isBlack = (ni == 1 || ni == 3 || ni == 6 || ni == 8 || ni == 10);
                        if (isBlack) {
                            float bx = kbX + curWhite * whiteKeyW - blackKeyW * 0.5f;
                            if (PtInRectF(D2D1::RectF(bx, kbY, bx + blackKeyW, kbY + blackKeyH), x, y)) {
                                clickedNote = n;
                                break;
                            }
                        } else {
                            curWhite++;
                        }
                    }
                    if (clickedNote < 0) {
                        curWhite = 0;
                        for (int n = startNote; curWhite < numWhiteKeys; ++n) {
                            int ni = n % 12;
                            bool isBlack = (ni == 1 || ni == 3 || ni == 6 || ni == 8 || ni == 10);
                            if (!isBlack) {
                                float wx = kbX + curWhite * whiteKeyW;
                                if (PtInRectF(D2D1::RectF(wx, kbY, wx + whiteKeyW, kbY + kbH), x, y)) {
                                    clickedNote = n;
                                    break;
                                }
                                curWhite++;
                            }
                        }
                    }
                    if (clickedNote >= 0) {
                        m_synth.PlayChord({ clickedNote });
                    }
                    return;
                }
            }

            float ctrlY = dockY + (curDockH - 22.0f) * 0.5f;
            float kbToggleX = centerX + centerW - 124.0f;
            if (PtInRectF(D2D1::RectF(kbToggleX, ctrlY, kbToggleX + 32.0f, ctrlY + 22.0f), x, y)) {
                m_showKeyboard = !m_showKeyboard;
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }

            float zoomX = centerX + centerW - 84.0f;
            if (PtInRectF(D2D1::RectF(zoomX + 22.0f, ctrlY, zoomX + 46.0f, ctrlY + 22.0f), x, y)) {
                m_pixelsPerBeat = std::max(24.0f, m_pixelsPerBeat - 8.0f);
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
            if (PtInRectF(D2D1::RectF(zoomX + 50.0f, ctrlY, zoomX + 74.0f, ctrlY + 22.0f), x, y)) {
                m_pixelsPerBeat = std::min(140.0f, m_pixelsPerBeat + 8.0f);
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
            return;
        }

        // スクロールバー操作
        float scrollY = dockY - m_scrollBarHeight;
        if (y >= scrollY && y < dockY) {
            m_dragMode = DragMode::ScrollBarThumb;
            m_dragStartX = x;
            SetCapture(m_hwnd);
            OnMouseMove(x, y);
            return;
        }

        // タイムラインルーラー (上部クリック・ドラッグで再生ヘッドをシーク移動)
        float rulerY = mainY + m_partTabHeight;
        if (y >= rulerY && y < rulerY + m_rulerHeight) {
            float relX = (x - centerX) + m_timelineScrollX;
            if (relX >= 0.0f) {
                float barWidth = m_pixelsPerBeat * 4.0f;
                double clickedTick = (relX / barWidth) * TICKS_PER_BAR_4_4;
                SeekToTick(clickedTick);
                m_dragMode = DragMode::RulerSeek;
                SetCapture(m_hwnd);
            }
            return;
        }

        // コードカードトラック
        float cardY = mainY + m_partTabHeight + m_rulerHeight + 5.0f;
        float cardH = m_cardTrackHeight - 10.0f;

        if (y >= cardY && y <= cardY + cardH) {
            Part* pActive = m_partManager.GetActivePart();
            std::vector<Bar> mergedBars;
            if (m_isMergedView) mergedBars = m_partManager.GenerateMergedProgression();
            std::vector<Bar>& bars = m_isMergedView ? mergedBars : (pActive ? pActive->bars : mergedBars);
            float barWidth = m_pixelsPerBeat * 4.0f;

            for (size_t b = 0; b < bars.size(); ++b) {
                float bx = centerX + b * barWidth - m_timelineScrollX;
                int barTicks = bars[b].GetTotalTicks();
                if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

                float offsetTicks = 0.0f;

                for (size_t ci = 0; ci < bars[b].chords.size(); ++ci) {
                    const auto& chord = bars[b].chords[ci];
                    int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
                    float cx = bx + (offsetTicks / barTicks) * barWidth;
                    float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
                    offsetTicks += cTicks;

                    D2D1_RECT_F cardRc = D2D1::RectF(cx + 2.0f, cardY, cx + cw - 2.0f, cardY + cardH);

                    if (chord.isRest) continue; // 空白スロットはカード掴み・削除判定をスキップ

                    if (PtInRectF(cardRc, x, y)) {
                        D2D1_RECT_F closeRc = D2D1::RectF(cardRc.right - 16.0f, cardRc.top + 2.0f, cardRc.right - 2.0f, cardRc.top + 16.0f);
                        if (!m_isMergedView && PtInRectF(closeRc, x, y)) {
                            PushUndo();
                            if (bars[b].chords.size() > 1) {
                                bars[b].chords[ci].isRest = true;
                                bars[b].chords[ci].quality = ChordQuality::Rest;
                                bars[b].chords[ci].rootName = "R";
                                bars[b].chords[ci].degreeRoman = "R";
                                bars[b].chords[ci].rawText = "_";
                            } else {
                                if (bars.size() > 1) {
                                    bars.erase(bars.begin() + b);
                                } else {
                                    bars[b].chords[ci].isRest = true;
                                    bars[b].chords[ci].quality = ChordQuality::Rest;
                                    bars[b].chords[ci].rootName = "R";
                                    bars[b].chords[ci].degreeRoman = "R";
                                    bars[b].chords[ci].rawText = "_";
                                }
                            }
                            m_selectedBarIndex = -1;
                            m_selectedChordIndex = -1;
                            SyncTimelineToText();
                            InvalidateRect(m_hwnd, nullptr, FALSE);
                            return;
                        }

                        if (!m_isMergedView && (GetKeyState(VK_CONTROL) & 0x8000)) {
                            // Ctrl+クリック: 選択に追加/除外（ドラッグ操作は開始しない）
                            ToggleChordSelection(static_cast<int>(b), static_cast<int>(ci));
                            AuditionChord(chord);
                            return;
                        }
                        if (!m_isMergedView && (GetKeyState(VK_SHIFT) & 0x8000)) {
                            // Shift+クリック: アンカーからの範囲選択
                            ExtendSelectionTo(static_cast<int>(b), static_cast<int>(ci));
                            AuditionChord(chord);
                            return;
                        }

                        SelectChord(static_cast<int>(b), static_cast<int>(ci));
                        AuditionChord(chord);

                        if (!m_isMergedView) {
                            float cardW = cardRc.right - cardRc.left;
                            float edgeMargin = std::min(8.0f, cardW * 0.20f);
                            if (x >= cardRc.right - edgeMargin) {
                                PushUndo();
                                m_dragMode = DragMode::CardResizeRight;
                                m_dragBarIndex = static_cast<int>(b);
                                m_dragChordIndex = static_cast<int>(ci);
                                m_dragStartX = x;
                                int barStartTick = TimelineEdit::BarStartTick(bars, static_cast<int>(b));
                                m_dragChordStartTick = static_cast<int>(offsetTicks) - cTicks;
                                m_dragChordEndTick = static_cast<int>(offsetTicks);
                                m_dragChordStartGlobal = barStartTick + m_dragChordStartTick;
                                m_dragChordEndGlobal = barStartTick + m_dragChordEndTick;
                                m_dragBaseBars = pActive ? pActive->bars : std::vector<Bar>{};
                                SetCapture(m_hwnd);
                                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                                return;
                            }

                            if (x <= cardRc.left + edgeMargin) {
                                PushUndo();
                                m_dragMode = DragMode::CardResizeLeft;
                                m_dragBarIndex = static_cast<int>(b);
                                m_dragChordIndex = static_cast<int>(ci);
                                m_dragStartX = x;
                                int barStartTick = TimelineEdit::BarStartTick(bars, static_cast<int>(b));
                                m_dragChordStartTick = static_cast<int>(offsetTicks) - cTicks;
                                m_dragChordEndTick = static_cast<int>(offsetTicks);
                                m_dragChordStartGlobal = barStartTick + m_dragChordStartTick;
                                m_dragChordEndGlobal = barStartTick + m_dragChordEndTick;
                                m_dragBaseBars = pActive ? pActive->bars : std::vector<Bar>{};

                                SetCapture(m_hwnd);
                                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                                return;
                            }

                            // カード面全体のドラッグ移動（中央を掴んだ場合は自由に左右移動可能）
                            m_dragMode = DragMode::CardTimelineDrag;
                            m_dragPaletteChord = chord;
                            m_dragBarIndex = static_cast<int>(b);
                            m_dragChordIndex = static_cast<int>(ci);
                            m_dragStartX = x;
                            m_dragStartY = y;
                            m_dragCurrentX = x;
                            m_dragCurrentY = y;
                            m_dragCardGrabOffsetX = std::clamp(x - cx, 0.0f, cw);
                            m_isPaletteDragging = false;
                            SetCapture(m_hwnd);
                        }
                        return;
                    }
                }
            }
        }
        // カード以外（空白スロット・ピアノロール領域）のクリックで選択を解除する
        ClearSelection();
        return;
    }

    // ---------------- 4. 右ペイン ----------------
    if (x >= m_width - rightW && m_showRightPane) {
        float pad = 12.0f;
        float innerW = rightW - pad * 2.0f;
        bool use7th = MusicEngine::IsSeventhDiatonicTab(m_diatonicChordTypeTab);

        // 右ペイン 3和音 / 7th 切り替え ＆ 格納ボタン
        if (PtInRectF(D2D1::RectF(m_width - pad - 122.0f, mainY + 5.0f, m_width - pad - 90.0f, mainY + 25.0f), x, y)) {
            // 7th系のときだけ3和音へ戻す（sus4 等の非7thバリエーションは維持する）
            if (use7th) m_diatonicChordTypeTab = 0;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRectF(D2D1::RectF(m_width - pad - 88.0f, mainY + 5.0f, m_width - pad - 58.0f, mainY + 25.0f), x, y)) {
            m_diatonicChordTypeTab = 1;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRectF(D2D1::RectF(m_width - pad - 52.0f, mainY + 5.0f, m_width - pad, mainY + 25.0f), x, y)) {
            m_showRightPane = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }

        float curY = mainY + 6.0f + 22.0f;

        if (PtInRectF(D2D1::RectF(m_width - rightW + pad, curY, m_width - pad, curY + 21.0f), x, y)) {
            m_accSuggestionsOpen = !m_accSuggestionsOpen;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        curY += 24.0f;

        if (m_accSuggestionsOpen) {
            auto suggestions = BuildSuggestionList(use7th);
            float sCardW = (innerW - 3.0f * 4.0f) / 4.0f;
            float sCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(suggestions.size(), 4); ++i) {
                float cx = m_width - rightW + pad + i * (sCardW + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, curY, cx + sCardW, curY + sCardH), x, y)) {
                    ChordToken c = suggestions[i];
                    c.durationTicks = m_inputGridTicks;
                    c.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    AuditionChord(c);
                    m_dragMode = DragMode::PaletteCardDrag;
                    m_dragPaletteChord = c;
                    m_dragStartX = x;
                    m_dragStartY = y;
                    m_dragCurrentX = x;
                    m_dragCurrentY = y;
                    m_isPaletteDragging = false;
                    SetCapture(m_hwnd);
                    return;
                }
            }
            curY += sCardH + 7.0f;
        }

        if (PtInRectF(D2D1::RectF(m_width - rightW + pad, curY, m_width - pad, curY + 21.0f), x, y)) {
            m_accBorrowOpen = !m_accBorrowOpen;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        curY += 24.0f;

        if (m_accBorrowOpen) {
            auto borrowChords = BuildBorrowedList(use7th);
            float bCardW = (innerW - 2.0f * 4.0f) / 3.0f;
            float bCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(borrowChords.size(), 6); ++i) {
                float cx = m_width - rightW + pad + (i % 3) * (bCardW + 4.0f);
                float cy = curY + (i / 3) * (bCardH + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, cy, cx + bCardW, cy + bCardH), x, y)) {
                    ChordToken c = borrowChords[i];
                    c.durationTicks = m_inputGridTicks;
                    c.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    AuditionChord(c);
                    m_dragMode = DragMode::PaletteCardDrag;
                    m_dragPaletteChord = c;
                    m_dragStartX = x;
                    m_dragStartY = y;
                    m_dragCurrentX = x;
                    m_dragCurrentY = y;
                    m_isPaletteDragging = false;
                    SetCapture(m_hwnd);
                    return;
                }
            }
            curY += (bCardH * 2.0f + 4.0f) + 7.0f;
        }

        if (PtInRectF(D2D1::RectF(m_width - rightW + pad, curY, m_width - pad, curY + 21.0f), x, y)) {
            m_accSecondaryOpen = !m_accSecondaryOpen;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        curY += 24.0f;

        if (m_accSecondaryOpen) {
            auto secChords = BuildSecondaryList();
            float scCardW = (innerW - 2.0f * 4.0f) / 3.0f;
            float scCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(secChords.size(), 5); ++i) {
                float cx = m_width - rightW + pad + (i % 3) * (scCardW + 4.0f);
                float cy = curY + (i / 3) * (scCardH + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, cy, cx + scCardW, cy + scCardH), x, y)) {
                    ChordToken c = secChords[i];
                    c.durationTicks = m_inputGridTicks;
                    c.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    AuditionChord(c);
                    m_dragMode = DragMode::PaletteCardDrag;
                    m_dragPaletteChord = c;
                    m_dragStartX = x;
                    m_dragStartY = y;
                    m_dragCurrentX = x;
                    m_dragCurrentY = y;
                    m_isPaletteDragging = false;
                    SetCapture(m_hwnd);
                    return;
                }
            }
            curY += (scCardH * 2.0f + 4.0f) + 7.0f;
        }

        if (PtInRectF(D2D1::RectF(m_width - rightW + pad, curY, m_width - pad, curY + 21.0f), x, y)) {
            m_accTemplatesOpen = !m_accTemplatesOpen;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        curY += 24.0f;

        if (m_accTemplatesOpen) {
            const auto& templates = MusicEngine::GetAllGenreTemplates();
            float tplH = 22.0f;

            for (size_t i = 0; i < std::min<size_t>(templates.size(), 6); ++i) {
                if (PtInRectF(D2D1::RectF(m_width - rightW + pad, curY, m_width - pad, curY + tplH), x, y)) {
                    Part* pActive = m_partManager.GetActivePart();
                    if (pActive) {
                        PushUndo();
                        m_isMergedView = false;
                        pActive->bars = ProgressionParser::Parse(templates[i].progression, pActive->keyRoot, pActive->mode);
                        SyncTimelineToText();
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                }
                curY += tplH + 3.0f;
            }
        }
    }
}

bool AppWindow::HitTestPaletteCard(float x, float y, ChordToken& outChord) {
    float mainY = m_toolbarHeight;
    Part* pActive = m_partManager.GetActivePart();
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    // 左ペイン: ダイアトニックコード
    if (x >= 0.0f && x <= m_leftPaneWidth && y > mainY && y < m_height - 22.0f) {
        const LeftPaneLayout L = ComputeLeftPaneLayout(mainY, m_leftPaneWidth);
        auto diatonicChords = MusicEngine::GetDiatonicChordsByTab(keyRoot, mode, m_diatonicChordTypeTab);

        for (size_t i = 0; i < 8; ++i) {
            float cy = L.cardY + static_cast<float>(i / 4) * (L.cardH + L.gap);
            D2D1_RECT_F cardRc = GridCellRect(L.pad, L.innerW, 4, static_cast<int>(i % 4), cy, L.cardH, L.gap);
            if (PtInRectF(cardRc, x, y)) {
                if (i < 7 && i < diatonicChords.size()) {
                    outChord = diatonicChords[i];
                } else {
                    outChord = ChordToken{};
                    outChord.isRest = true;
                    outChord.rootName = "R";
                    outChord.degreeRoman = "R";
                }
                outChord.durationTicks = m_inputGridTicks;
                outChord.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                return true;
            }
        }
        return false;
    }

    // 右ペイン: SUGGESTIONS / 借用和音 / セカンダリードミナント
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    if (m_showRightPane && x >= m_width - rightW) {
        float curY = mainY + 6.0f + 22.0f + 24.0f;
        float pad = 12.0f;
        float innerW = rightW - pad * 2.0f;
        bool use7th = MusicEngine::IsSeventhDiatonicTab(m_diatonicChordTypeTab);

        if (m_accSuggestionsOpen) {
            auto suggestions = BuildSuggestionList(use7th);
            float sCardW = (innerW - 3.0f * 4.0f) / 4.0f;
            float sCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(suggestions.size(), 4); ++i) {
                float cx = m_width - rightW + pad + i * (sCardW + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, curY, cx + sCardW, curY + sCardH), x, y)) {
                    outChord = suggestions[i];
                    outChord.durationTicks = m_inputGridTicks;
                    outChord.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    return true;
                }
            }
            curY += sCardH + 7.0f;
        }

        curY += 24.0f;
        if (m_accBorrowOpen) {
            auto borrowChords = BuildBorrowedList(use7th);
            float bCardW = (innerW - 2.0f * 4.0f) / 3.0f;
            float bCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(borrowChords.size(), 6); ++i) {
                float cx = m_width - rightW + pad + (i % 3) * (bCardW + 4.0f);
                float cy = curY + (i / 3) * (bCardH + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, cy, cx + bCardW, cy + bCardH), x, y)) {
                    outChord = borrowChords[i];
                    outChord.durationTicks = m_inputGridTicks;
                    outChord.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    return true;
                }
            }
            curY += (bCardH * 2.0f + 4.0f) + 7.0f;
        }

        curY += 24.0f;
        if (m_accSecondaryOpen) {
            auto secChords = BuildSecondaryList();
            float scCardW = (innerW - 2.0f * 4.0f) / 3.0f;
            float scCardH = 38.0f;

            for (size_t i = 0; i < std::min<size_t>(secChords.size(), 5); ++i) {
                float cx = m_width - rightW + pad + (i % 3) * (scCardW + 4.0f);
                float cy = curY + (i / 3) * (scCardH + 4.0f);
                if (PtInRectF(D2D1::RectF(cx, cy, cx + scCardW, cy + scCardH), x, y)) {
                    outChord = secChords[i];
                    outChord.durationTicks = m_inputGridTicks;
                    outChord.durationBeats = std::max(1, (m_inputGridTicks + TICKS_PER_BEAT / 2) / TICKS_PER_BEAT);
                    return true;
                }
            }
        }
    }
    return false;
}

void AppWindow::OnLButtonDblClk(float x, float y) {
    if (m_activeModal == ModalType::Dashboard) {
        float cardW = std::min(740.0f, m_width - 40.0f);
        float cardH = std::min(520.0f, m_height - 40.0f);
        float cardX = (m_width - cardW) * 0.5f;
        float cardY = (m_height - cardH) * 0.5f;
        D2D1_RECT_F bodyRc = D2D1::RectF(cardX + 16.0f, cardY + 44.0f + 12.0f, cardX + cardW - 16.0f, cardY + cardH - 16.0f);

        float curY = bodyRc.top + 58.0f + 72.0f + 24.0f;
        float rListH = bodyRc.bottom - curY - 44.0f;
        D2D1_RECT_F rListBox = D2D1::RectF(bodyRc.left, curY, bodyRc.right, curY + rListH);
        if (!m_recentFiles.empty() && PtInRectF(rListBox, x, y)) {
            float itemH = 32.0f;
            size_t maxItems = std::min<size_t>(m_recentFiles.size(), static_cast<size_t>(std::floor((rListH - 8.0f) / (itemH + 2.0f))));
            for (size_t ri = 0; ri < maxItems; ++ri) {
                float iy = rListBox.top + 6.0f + ri * (itemH + 2.0f);
                D2D1_RECT_F itemRc = D2D1::RectF(rListBox.left + 6.0f, iy, rListBox.right - 6.0f, iy + itemH);
                if (PtInRectF(itemRc, x, y)) {
                    m_dashboardSelectedRecentIndex = static_cast<int>(ri);
                    OpenRecentFile(static_cast<int>(ri));
                    CloseModal();
                    return;
                }
            }
        }
        return;
    }

    if (m_activeModal != ModalType::None) return;

    // パートタブのダブルクリックで名前変更モーダルを開く
    float mainY = m_toolbarHeight;
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float centerX = m_leftPaneWidth;
    float centerW = m_width - rightW - centerX;
    if (x >= centerX && x <= centerX + centerW && y >= mainY && y <= mainY + m_partTabHeight) {
        const auto& parts = m_partManager.GetParts();
        float rightReserve = m_showRightPane ? 8.0f : 114.0f;
        float availTotalW = std::max(200.0f, centerW - 8.0f - rightReserve);
        size_t totalTabs = parts.empty() ? 1 : parts.size();
        bool compactAdd = (availTotalW < totalTabs * 86.0f + 68.0f);
        float addBtnW = compactAdd ? 32.0f : 66.0f;
        float availTabsW = std::max(120.0f, availTotalW - addBtnW - 6.0f);
        float pTabW = std::clamp((availTabsW - static_cast<float>(totalTabs) * 3.0f) / static_cast<float>(totalTabs), 32.0f, 120.0f);

        float curX = centerX + 8.0f;
        float tabH = m_partTabHeight - 3.0f;
        float ty = mainY + 3.0f;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (PtInRectF(D2D1::RectF(curX, ty, curX + pTabW, ty + tabH), x, y)) {
                OpenRenamePartModal(static_cast<int>(i));
                return;
            }
            curX += pTabW + 3.0f;
        }
    }

    // ルーラーのダブルクリックで、その小節のマーカー編集モーダルを開く
    {
        float rulerTop = mainY + m_partTabHeight;
        if (x >= centerX && x <= centerX + centerW && y >= rulerTop && y < rulerTop + m_rulerHeight) {
            int tick = HitTestRulerTick(x);
            if (tick >= 0) {
                if (m_dragMode == DragMode::RulerSeek) {
                    m_dragMode = DragMode::None;
                    ReleaseCapture();
                }
                OpenMarkerModal(tick);
                return;
            }
        }
    }

    ChordToken chord;
    if (HitTestPaletteCard(x, y, chord)) {
        if (m_dragMode == DragMode::PaletteCardDrag || m_dragMode == DragMode::CardTimelineDrag) {
            m_dragMode = DragMode::None;
            m_isPaletteDragging = false;
            ReleaseCapture();
        }
        AppendChordToTimeline(chord);
        return;
    }
    OnLButtonDown(x, y);
}

void AppWindow::OnMouseMove(float x, float y) {
    if (m_dragMode == DragMode::RulerSeek) {
        float centerX = m_leftPaneWidth;
        float relX = (x - centerX) + m_timelineScrollX;
        if (relX >= 0.0f) {
            float barWidth = m_pixelsPerBeat * 4.0f;
            double clickedTick = (relX / barWidth) * TICKS_PER_BAR_4_4;
            SeekToTick(clickedTick);
        }
        return;
    }

    if (m_dragMode == DragMode::PaletteCardDrag || m_dragMode == DragMode::CardTimelineDrag) {
        float dx = x - m_dragStartX;
        float dy = y - m_dragStartY;
        if (!m_isPaletteDragging && (dx * dx + dy * dy) >= 16.0f) {
            m_isPaletteDragging = true;
        }
        if (m_isPaletteDragging) {
            m_dragCurrentX = x;
            m_dragCurrentY = y;
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    if (m_dragMode == DragMode::CardResizeRight) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        Part* pActive = m_partManager.GetActivePart();
        if (pActive && !m_dragBaseBars.empty()) {
            int barTicks = m_dragBaseBars[0].GetTotalTicks();
            if (m_dragBarIndex >= 0 && m_dragBarIndex < static_cast<int>(m_dragBaseBars.size())) {
                barTicks = m_dragBaseBars[m_dragBarIndex].GetTotalTicks();
            }
            if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;
            float pxPerTick = (m_pixelsPerBeat * 4.0f) / static_cast<float>(barTicks);
            float dx = x - m_dragStartX;
            int snapTicks = std::max(1, m_timelineGridTicks);
            int deltaTicks = static_cast<int>(std::lround(dx / pxPerTick / static_cast<float>(snapTicks))) * snapTicks;

            // 開始位置を固定したまま右端を動かす。小節線を越えると次小節の先頭を切り詰める。
            auto res = TimelineEdit::ResizeChordSpan(m_dragBaseBars, m_dragChordStartGlobal,
                                                     m_dragChordStartGlobal,
                                                     m_dragChordEndGlobal + deltaTicks,
                                                     snapTicks);
            if (res.selectedBar >= 0) {
                pActive->bars = std::move(res.bars);
                m_selectedBarIndex = res.selectedBar;
                m_selectedChordIndex = res.selectedChord;
                EnsureVisible(m_selectedBarIndex);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return;
    }

    if (m_dragMode == DragMode::CardResizeLeft) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        Part* pActive = m_partManager.GetActivePart();
        if (pActive && !m_dragBaseBars.empty()) {
            int barTicks = m_dragBaseBars[0].GetTotalTicks();
            if (m_dragBarIndex >= 0 && m_dragBarIndex < static_cast<int>(m_dragBaseBars.size())) {
                barTicks = m_dragBaseBars[m_dragBarIndex].GetTotalTicks();
            }
            if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;
            float pxPerTick = (m_pixelsPerBeat * 4.0f) / static_cast<float>(barTicks);
            float dx = x - m_dragStartX; // 右へ動かすと正 (縮小)、左へ動かすと負 (伸長)
            int snapTicks = std::max(1, m_timelineGridTicks);
            int deltaTicks = static_cast<int>(std::lround(dx / pxPerTick / static_cast<float>(snapTicks))) * snapTicks;

            // 終端を固定したまま左端を動かす。小節線を越えると前小節の末尾を切り詰める。
            auto res = TimelineEdit::ResizeChordSpan(m_dragBaseBars, m_dragChordStartGlobal,
                                                     m_dragChordStartGlobal + deltaTicks,
                                                     m_dragChordEndGlobal,
                                                     snapTicks);
            if (res.selectedBar >= 0) {
                pActive->bars = std::move(res.bars);
                m_selectedBarIndex = res.selectedBar;
                m_selectedChordIndex = res.selectedChord;
                EnsureVisible(m_selectedBarIndex);
                SyncTimelineToText();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return;
    }

    if (m_dragMode == DragMode::CardMove) {
        Part* pActive = m_partManager.GetActivePart();
        if (pActive && m_dragBarIndex >= 0 && m_dragBarIndex < static_cast<int>(pActive->bars.size())) {
            auto& bar = pActive->bars[m_dragBarIndex];
            if (m_dragChordIndex >= 0 && m_dragChordIndex < static_cast<int>(bar.chords.size())) {
                float dx = x - m_dragStartX;
                if (dx > m_pixelsPerBeat && m_dragChordIndex + 1 < static_cast<int>(bar.chords.size())) {
                    std::swap(bar.chords[m_dragChordIndex], bar.chords[m_dragChordIndex + 1]);
                    m_dragChordIndex++;
                    m_selectedChordIndex = m_dragChordIndex;
                    m_dragStartX = x;
                    SyncTimelineToText();
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                } else if (dx < -m_pixelsPerBeat && m_dragChordIndex > 0) {
                    std::swap(bar.chords[m_dragChordIndex], bar.chords[m_dragChordIndex - 1]);
                    m_dragChordIndex--;
                    m_selectedChordIndex = m_dragChordIndex;
                    m_dragStartX = x;
                    SyncTimelineToText();
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
            }
        }
        return;
    }

    if (m_dragMode == DragMode::ScrollBarThumb) {
        float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
        float centerX = m_leftPaneWidth;
        float viewWidth = m_width - centerX - rightW;
        if (viewWidth > 0.0f) {
            Part* pActive = m_partManager.GetActivePart();
            size_t actualBars = m_isMergedView ? m_partManager.GenerateMergedProgression().size() : (pActive ? pActive->bars.size() : 0);
            size_t numBars = std::max<size_t>(actualBars + 16, 64);
            float totalContentWidth = numBars * (m_pixelsPerBeat * 4.0f);
            float maxScroll = std::max(0.0f, totalContentWidth - viewWidth);
            float ratio = std::clamp((x - centerX) / viewWidth, 0.0f, 1.0f);
            m_timelineScrollX = ratio * maxScroll;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    // カード端ホバー時のリサイズカーソル表示
    if (m_activeModal == ModalType::None && !m_isMergedView) {
        float mainY = m_toolbarHeight;
        float cardY = mainY + m_partTabHeight + m_rulerHeight + 5.0f;
        float cardH = m_cardTrackHeight - 10.0f;
        float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
        float centerX = m_leftPaneWidth;
        float centerW = m_width - rightW - centerX;
        if (x >= centerX && x <= centerX + centerW && y >= cardY && y <= cardY + cardH) {
            Part* pActive = m_partManager.GetActivePart();
            if (pActive) {
                float barWidth = m_pixelsPerBeat * 4.0f;
                for (size_t b = 0; b < pActive->bars.size(); ++b) {
                    float bx = centerX + b * barWidth - m_timelineScrollX;
                    int barTicks = std::max(1, pActive->bars[b].GetTotalTicks());
                    float offsetTicks = 0.0f;
                    for (size_t ci = 0; ci < pActive->bars[b].chords.size(); ++ci) {
                        const auto& chord = pActive->bars[b].chords[ci];
                        int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
                        float cx = bx + (offsetTicks / barTicks) * barWidth;
                        float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
                        offsetTicks += cTicks;
                        if (chord.isRest) continue; // 休符スロットはリサイズカーソルを表示しない
                        D2D1_RECT_F cardRc = D2D1::RectF(cx + 2.0f, cardY, cx + cw - 2.0f, cardY + cardH);
                        float edgeMargin = std::min(8.0f, cw * 0.20f);
                        if (PtInRectF(cardRc, x, y) && (x <= cardRc.left + edgeMargin || x >= cardRc.right - edgeMargin)) {
                            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                            return;
                        }
                    }
                }
            }
        }
    }
}

void AppWindow::OnLButtonUp(float x, float y) {
    if (m_dragMode == DragMode::RulerSeek) {
        m_dragMode = DragMode::None;
        ReleaseCapture();
        return;
    }

    if (m_dragMode == DragMode::CardTimelineDrag) {
        bool wasDragging = m_isPaletteDragging;
        int srcBar = m_dragBarIndex;
        int srcChord = m_dragChordIndex;
        m_dragMode = DragMode::None;
        m_isPaletteDragging = false;
        ReleaseCapture();

        if (wasDragging) {
            float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
            float centerX = m_leftPaneWidth;
            float centerW = m_width - rightW - centerX;
            float curDockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
            if (x >= centerX && x <= centerX + centerW && y >= m_toolbarHeight && y <= m_height - curDockH) {
                Part* pActive = m_partManager.GetActivePart();
                if (pActive && srcBar >= 0 && srcBar < static_cast<int>(pActive->bars.size())) {
                    if (srcChord >= 0 && srcChord < static_cast<int>(pActive->bars[srcBar].chords.size())) {
                        MoveChordToTimelineX(srcBar, srcChord, x - m_dragCardGrabOffsetX);
                    }
                }
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    if (m_dragMode == DragMode::PaletteCardDrag) {
        bool wasDragging = m_isPaletteDragging;
        ChordToken droppedChord = m_dragPaletteChord;
        m_dragMode = DragMode::None;
        m_isPaletteDragging = false;
        ReleaseCapture();

        if (wasDragging) {
            float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
            float centerX = m_leftPaneWidth;
            float centerW = m_width - rightW - centerX;
            float curDockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
            if (x >= centerX && x <= centerX + centerW && y >= m_toolbarHeight && y <= m_height - curDockH) {
                InsertChordAtTimelineX(droppedChord, x, true, m_timelineGridTicks);
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    if (m_dragMode == DragMode::CardResizeLeft) {
        m_dragMode = DragMode::None;
        m_dragBaseBars.clear();
        ReleaseCapture();
        SyncTimelineToText();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (m_dragMode != DragMode::None) {
        m_dragMode = DragMode::None;
        m_dragBaseBars.clear();
        ReleaseCapture();
    }
}
void AppWindow::OnRButtonUp(float x, float y) {
    if (m_activeModal != ModalType::None) return;

    float mainY = m_toolbarHeight;
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float centerX = m_leftPaneWidth;
    float centerW = m_width - rightW - centerX;

    // パートタブ右クリックメニュー
    if (x >= centerX && x <= centerX + centerW && y >= mainY && y <= mainY + m_partTabHeight) {
        const auto& parts = m_partManager.GetParts();
        float rightReserve = m_showRightPane ? 8.0f : 114.0f;
        float availTotalW = std::max(200.0f, centerW - 8.0f - rightReserve);
        size_t totalTabs = parts.empty() ? 1 : parts.size();
        bool compactAdd = (availTotalW < totalTabs * 86.0f + 68.0f);
        float addBtnW = compactAdd ? 32.0f : 66.0f;
        float availTabsW = std::max(120.0f, availTotalW - addBtnW - 6.0f);
        float pTabW = std::clamp((availTabsW - static_cast<float>(totalTabs) * 3.0f) / static_cast<float>(totalTabs), 32.0f, 120.0f);

        float curX = centerX + 8.0f;
        float tabH = m_partTabHeight - 3.0f;
        float ty = mainY + 3.0f;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (PtInRectF(D2D1::RectF(curX, ty, curX + pTabW, ty + tabH), x, y)) {
                m_contextPartIndex = static_cast<int>(i);
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, IDM_CTX_PART_RENAME, TR(L"名前の変更(&R)...", L"Rename Part(&R)..."));
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_CTX_PART_DUP, TR(L"このパートを複製", L"Duplicate Part"));
                AppendMenuW(hMenu, (parts.size() > 1) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_PART_DEL, TR(L"このパートを削除", L"Delete Part"));
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, (i > 0) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_PART_LEFT, TR(L"左へ移動", L"Move Left"));
                AppendMenuW(hMenu, (i + 1 < parts.size()) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_PART_RIGHT, TR(L"右へ移動", L"Move Right"));
                POINT pt = { static_cast<LONG>(x), static_cast<LONG>(y) };
                ClientToScreen(m_hwnd, &pt);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, m_hwnd, nullptr);
                DestroyMenu(hMenu);
                return;
            }
            curX += pTabW + 3.0f;
        }
        return;
    }

    // ルーラー右クリックメニュー（小節マーカーの追加・編集・削除）
    {
        float rulerTop = mainY + m_partTabHeight;
        if (x >= centerX && x <= centerX + centerW && y >= rulerTop && y < rulerTop + m_rulerHeight) {
            int tick = HitTestRulerTick(x);
            Part* pRulerPart = m_partManager.GetActivePart();
            if (tick >= 0 && pRulerPart) {
                const int tol = MarkerHitToleranceTicks();
                const int hit = m_partManager.FindMarkerNear(m_partManager.GetActiveIndex(), tick, tol);
                const bool atStart = (hit < 0 && tick <= tol);
                m_contextMarkerTick = (hit >= 0) ? hit : tick;
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, IDM_CTX_MARKER_EDIT,
                    atStart ? TR(L"パート名（先頭マーカー）を変更...", L"Rename Part (first marker)...")
                            : (hit >= 0 ? TR(L"マーカーを編集...", L"Edit Marker...") : TR(L"ここにマーカーを追加...", L"Add Marker Here...")));
                AppendMenuW(hMenu, (hit >= 0) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_MARKER_DEL, TR(L"マーカーを削除", L"Remove Marker"));
                POINT pt = { static_cast<LONG>(x), static_cast<LONG>(y) };
                ClientToScreen(m_hwnd, &pt);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, m_hwnd, nullptr);
                DestroyMenu(hMenu);
                return;
            }
        }
    }

    // コードカード右クリックメニュー
    float cardY = mainY + m_partTabHeight + m_rulerHeight + 5.0f;
    float cardH = m_cardTrackHeight - 10.0f;
    if (!m_isMergedView && x >= centerX && x <= centerX + centerW && y >= cardY && y <= cardY + cardH) {
        Part* pActive = m_partManager.GetActivePart();
        if (!pActive) return;
        float barWidth = m_pixelsPerBeat * 4.0f;
        for (size_t b = 0; b < pActive->bars.size(); ++b) {
            float bx = centerX + b * barWidth - m_timelineScrollX;
            int barTicks = std::max(1, pActive->bars[b].GetTotalTicks());
            float offsetTicks = 0.0f;
            for (size_t ci = 0; ci < pActive->bars[b].chords.size(); ++ci) {
                const auto& chord = pActive->bars[b].chords[ci];
                int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
                float cx = bx + (offsetTicks / barTicks) * barWidth;
                float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
                offsetTicks += cTicks;
                D2D1_RECT_F cardRc = D2D1::RectF(cx + 2.0f, cardY, cx + cw - 2.0f, cardY + cardH);
                if (PtInRectF(cardRc, x, y)) {
                    SelectChord(static_cast<int>(b), static_cast<int>(ci));
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SPLIT, TR(L"コードを分割 (B)", L"Split Chord (B)"));
                    AppendMenuW(hMenu, MF_STRING, IDM_EDIT_DUPLICATE, TR(L"コードを複製 (Ctrl+D)", L"Duplicate Chord (Ctrl+D)"));
                    AppendMenuW(hMenu, MF_STRING, IDM_EDIT_DELETE, TR(L"コードを削除 (Del)", L"Delete Chord (Del)"));
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, (chord.inversion == 0) ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_INV0, TR(L"基本形 (Root)", L"Root Position"));
                    AppendMenuW(hMenu, (chord.inversion == 1) ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_INV1, TR(L"第1転回形 (1st Inv)", L"1st Inversion"));
                    AppendMenuW(hMenu, (chord.inversion == 2) ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_INV2, TR(L"第2転回形 (2nd Inv)", L"2nd Inversion"));
                    AppendMenuW(hMenu, (chord.inversion == 3) ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_INV3, TR(L"第3転回形 (3rd Inv)", L"3rd Inversion"));
                    if (!chord.isRest && chord.rootPitch != PitchClass::Unknown) {
                        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                        HMENU hOctSub = CreatePopupMenu();
                        std::wstring octCur = TR(L"現在: ", L"Current: ");
                        octCur += (chord.octaveOffset > 0 ? L"+" : L"") + std::to_wstring(chord.octaveOffset) + L" Oct";
                        AppendMenuW(hOctSub, MF_STRING | MF_GRAYED, 0, octCur.c_str());
                        AppendMenuW(hOctSub, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(hOctSub, (chord.octaveOffset < 3) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_CARD_OCT_UP, TR(L"1オクターブ上へ (Ctrl+↑)", L"Octave Up (Ctrl+Up)"));
                        AppendMenuW(hOctSub, (chord.octaveOffset > -3) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_CARD_OCT_DOWN, TR(L"1オクターブ下へ (Ctrl+↓)", L"Octave Down (Ctrl+Down)"));
                        AppendMenuW(hOctSub, (chord.octaveOffset != 0) ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_CTX_CARD_OCT_RESET, TR(L"オクターブをリセット", L"Reset Octave"));
                        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hOctSub), TR(L"オクターブ変更", L"Change Octave"));
                        HMENU hBassSub = CreatePopupMenu();
                        AppendMenuW(hBassSub, chord.bassName.empty() ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_BASS_NONE, TR(L"オンベース解除 (通常コード)", L"Clear Slash Bass (Normal)"));
                        AppendMenuW(hBassSub, MF_STRING, IDM_CTX_CARD_BASS_3RD, TR(L"第3音ベース (1st Inv / On 3rd)", L"Bass on 3rd (1st Inv)"));
                        AppendMenuW(hBassSub, MF_STRING, IDM_CTX_CARD_BASS_5TH, TR(L"第5音ベース (2nd Inv / On 5th)", L"Bass on 5th (2nd Inv)"));
                        AppendMenuW(hBassSub, MF_STRING, IDM_CTX_CARD_BASS_7TH, TR(L"第7音ベース (3rd Inv / On 7th)", L"Bass on 7th (3rd Inv)"));
                        AppendMenuW(hBassSub, MF_SEPARATOR, 0, nullptr);
                        const char* noteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
                        for (int ni = 0; ni < 12; ++ni) {
                            std::wstring lbl = L"/" + ToWString(noteNames[ni]);
                            bool isCur = (!chord.bassName.empty() && static_cast<int>(chord.bassPitch) == ni);
                            AppendMenuW(hBassSub, isCur ? (MF_STRING | MF_CHECKED) : MF_STRING, IDM_CTX_CARD_BASS_BASE + ni, lbl.c_str());
                        }
                        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hBassSub), TR(L"分数コード (オンベース指定)", L"Slash Chord (Bass Note)"));
                    }
                    POINT pt = { static_cast<LONG>(x), static_cast<LONG>(y) };
                    ClientToScreen(m_hwnd, &pt);
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, m_hwnd, nullptr);
                    DestroyMenu(hMenu);
                    return;
                }
            }
        }
    }
}

void AppWindow::OnMouseWheel(short delta, float /*x*/, float /*y*/) {
    if (m_activeModal == ModalType::Templates && !m_tmplEditorOpen) {
        auto categories = MusicEngine::GetTemplateMajorCategories();
        std::string curCat = (m_selectedTmplCategoryIndex < static_cast<int>(categories.size())) ? categories[m_selectedTmplCategoryIndex] : "👤 ユーザー定義";
        auto tmpls = MusicEngine::GetTemplatesByCategory(curCat);
        int maxScroll = std::max(0, static_cast<int>(tmpls.size()) - 5);
        m_tmplListScroll = std::clamp(m_tmplListScroll - (delta / 120), 0, maxScroll);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_activeModal != ModalType::None) return;

    m_timelineScrollX -= (delta / 120.0f) * (m_pixelsPerBeat * 2.0f);

    Part* pActive = m_partManager.GetActivePart();
    size_t numBars = m_isMergedView ? m_partManager.GenerateMergedProgression().size() : (pActive ? pActive->bars.size() : 8);
    float totalContentWidth = std::max<size_t>(numBars + 16, 64) * (m_pixelsPerBeat * 4.0f);
    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float viewWidth = m_width - m_leftPaneWidth - rightW;

    float maxScroll = std::max(0.0f, totalContentWidth - viewWidth);
    m_timelineScrollX = std::clamp(m_timelineScrollX, 0.0f, maxScroll);

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::OnKeyDown(WPARAM key) {
    if (m_activeModal == ModalType::Dashboard) {
        if (key == VK_RETURN) {
            if (m_dashboardSelectedRecentIndex >= 0 && m_dashboardSelectedRecentIndex < static_cast<int>(m_recentFiles.size())) {
                OpenRecentFile(m_dashboardSelectedRecentIndex);
                CloseModal();
            }
            return;
        }
        if (key == VK_ESCAPE) {
            CloseModal();
            return;
        }
        if (key == VK_UP) {
            if (!m_recentFiles.empty()) {
                if (m_dashboardSelectedRecentIndex <= 0) {
                    m_dashboardSelectedRecentIndex = static_cast<int>(m_recentFiles.size()) - 1;
                } else {
                    --m_dashboardSelectedRecentIndex;
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return;
        }
        if (key == VK_DOWN) {
            if (!m_recentFiles.empty()) {
                if (m_dashboardSelectedRecentIndex < 0 || m_dashboardSelectedRecentIndex >= static_cast<int>(m_recentFiles.size()) - 1) {
                    m_dashboardSelectedRecentIndex = 0;
                } else {
                    ++m_dashboardSelectedRecentIndex;
                }
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return;
        }
        return;
    }

    if (m_activeModal == ModalType::RenamePart) {
        if (key == VK_RETURN) {
            CommitRenameModal();
            CloseModal();
            return;
        }
        if (key == VK_ESCAPE) {
            CloseModal();
            return;
        }
        if (key == VK_BACK) {
            if (!m_renamePartName.empty()) {
                m_renamePartName.pop_back();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return;
        }
        return;
    }

    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (isCtrl) {
        int gridIndex = -1;
        if (key == '1' || key == VK_NUMPAD1) gridIndex = 0;
        else if (key == '2' || key == VK_NUMPAD2) gridIndex = 1;
        else if (key == '3' || key == VK_NUMPAD3) gridIndex = 2;
        else if (key == '4' || key == VK_NUMPAD4) gridIndex = 3;
        else if (key == '5' || key == VK_NUMPAD5) gridIndex = 4;

        if (gridIndex >= 0) {
            const int grids[] = { TICKS_PER_BAR_4_4, TICKS_PER_BEAT * 2, TICKS_PER_BEAT, TICKS_PER_BEAT / 2, TICKS_PER_BEAT / 4 };
            const wchar_t* jaNames[] = { L"1小節", L"2拍", L"1拍", L"1/2拍", L"1/4拍" };
            const wchar_t* enNames[] = { L"1 Bar", L"2 Beats", L"1 Beat", L"1/2 Beat", L"1/4 Beat" };
            std::wstring msg;
            std::wstring combo = L" (Ctrl+" + std::wstring(isShift ? L"Shift+" : L"") + std::to_wstring(gridIndex + 1) + L")";
            if (isShift) {
                m_inputGridTicks = grids[gridIndex];
                msg = TR(L"入力グリッド: ", L"Input grid: ");
                msg += (m_settingLanguage == 1 ? enNames[gridIndex] : jaNames[gridIndex]);
                msg += combo;
            } else {
                m_timelineGridTicks = grids[gridIndex];
                msg = TR(L"タイムライングリッド: ", L"Timeline grid: ");
                msg += (m_settingLanguage == 1 ? enNames[gridIndex] : jaNames[gridIndex]);
                msg += combo;
            }
            SetStatusMessage(msg);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
        if (key == VK_UP) {
            ShiftSelectedChordOctave(1);
            return;
        }
        if (key == VK_DOWN) {
            ShiftSelectedChordOctave(-1);
            return;
        }
        if (key == 'D' || key == 'd') {
            DuplicateSelectedChord();
            return;
        }
    }

    if (key == VK_SPACE) TogglePlayback();
    else if (key == VK_ESCAPE) StopPlayback();
    else if (key == 'B' || key == 'b') SplitSelectedChord();
    else if (key == 'M' || key == 'm') {
        AddMarkerAtPlayhead();
        // 直後に届く WM_CHAR('m') が、開いたモーダルの名前欄へ入力されないよう1回だけ捨てる
        m_swallowMarkerChar = (m_activeModal == ModalType::RenamePart);
    }
    else if (key == VK_DELETE) DeleteSelection();
}

void AppWindow::OnChar(WCHAR ch) {
    if (m_swallowMarkerChar) {
        m_swallowMarkerChar = false;
        if (ch == L'm' || ch == L'M') return;
    }
    if (m_activeModal == ModalType::RenamePart) {
        if (ch >= 0x20 && ch != 0x7F) {
            if (m_renamePartName.size() < 30) {
                m_renamePartName += ch;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return;
    }
    if (m_activeModal == ModalType::Templates && m_tmplEditorOpen) {
        if (ch >= 0x20 && ch != 0x7F) {
            std::wstring* pField = (m_tmplEditFocusField == 0) ? &m_tmplEditName : ((m_tmplEditFocusField == 1) ? &m_tmplEditProg : &m_tmplEditDesc);
            if (pField->size() < 120) {
                *pField += ch;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
    }
}
} // namespace clp
