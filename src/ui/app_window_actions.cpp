#include "app_window_internal.hpp"
namespace clp {
// ---------------------------------------------------------------------------
// タイムライン・コード操作実装
// ---------------------------------------------------------------------------
void AppWindow::SelectChord(int barIndex, int chordIndex) {
    m_selectedBarIndex = barIndex;
    m_selectedChordIndex = chordIndex;
    EnsureVisible(barIndex);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::EnsureVisible(int barIndex) {
    if (barIndex < 0) return;
    float barWidth = m_pixelsPerBeat * 4.0f;
    float barStart = barIndex * barWidth;
    float barEnd = barStart + barWidth;

    float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    float viewWidth = m_width - m_leftPaneWidth - rightW;

    Part* pActive = m_partManager.GetActivePart();
    size_t numBars = m_isMergedView ? m_partManager.GenerateMergedProgression().size() : (pActive ? pActive->bars.size() : 8);
    float totalContentWidth = std::max<size_t>(numBars + 16, 64) * barWidth;
    float maxScroll = std::max(0.0f, totalContentWidth - viewWidth);

    if (barStart < m_timelineScrollX) {
        m_timelineScrollX = barStart;
    } else if (barEnd > m_timelineScrollX + viewWidth) {
        m_timelineScrollX = barEnd - viewWidth;
    }

    m_timelineScrollX = std::clamp(m_timelineScrollX, 0.0f, maxScroll);
}

void AppWindow::AuditionChord(const ChordToken& chord) {
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
        m_synth.AllNotesOff();
        return;
    }
    auto notes = MusicEngine::GetChordMidiNotes(chord, m_baseOctave, m_selectedVoicing, chord.inversion, m_bassOctaveMode);
    if (!notes.empty()) {
        m_synth.PlayChord(notes);
    }
}

void AppWindow::AppendChordToTimeline(const ChordToken& chord) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    PushUndo();
    m_isMergedView = false;
    int addTicks = (chord.durationTicks > 0) ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);

    if (pActive->bars.empty()) {
        Bar b;
        b.barNumber = 1;
        b.timeNumerator = pActive->timeNumerator;
        b.timeDenominator = pActive->timeDenominator;
        pActive->bars.push_back(b);
    } else {
        auto& lastBar = pActive->bars.back();
        int usedTicks = 0;
        for (const auto& existing : lastBar.chords) {
            usedTicks += (existing.durationTicks > 0) ? existing.durationTicks : (existing.durationBeats * TICKS_PER_BEAT);
        }
        int maxBarTicks = lastBar.GetTotalTicks();
        if (maxBarTicks <= 0) maxBarTicks = TICKS_PER_BAR_4_4;
        if (!lastBar.chords.empty() && usedTicks + addTicks > maxBarTicks) {
            Bar newBar;
            newBar.barNumber = static_cast<int>(pActive->bars.size()) + 1;
            newBar.timeNumerator = pActive->timeNumerator;
            newBar.timeDenominator = pActive->timeDenominator;
            pActive->bars.push_back(newBar);
        }
    }

    pActive->bars.back().chords.push_back(chord);
    m_selectedBarIndex = static_cast<int>(pActive->bars.size()) - 1;
    m_selectedChordIndex = static_cast<int>(pActive->bars.back().chords.size()) - 1;
    EnsureVisible(m_selectedBarIndex);
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::InsertChordAtTimelineTick(const ChordToken& chord, int targetBarIdx, int targetTickInBar, bool pushUndo) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    if (targetBarIdx < 0) targetBarIdx = 0;
    while (targetBarIdx >= static_cast<int>(pActive->bars.size())) {
        Bar newBar;
        newBar.barNumber = static_cast<int>(pActive->bars.size()) + 1;
        newBar.timeNumerator = pActive->timeNumerator > 0 ? pActive->timeNumerator : 4;
        newBar.timeDenominator = pActive->timeDenominator > 0 ? pActive->timeDenominator : 4;
        ChordToken fullRest;
        fullRest.isRest = true;
        fullRest.quality = ChordQuality::Rest;
        fullRest.rootName = "R";
        fullRest.degreeRoman = "R";
        fullRest.rawText = "_";
        fullRest.SetTicks(newBar.GetTotalTicks());
        newBar.chords.push_back(fullRest);
        pActive->bars.push_back(newBar);
    }

    if (pushUndo) {
        PushUndo();
    }
    m_isMergedView = false;

    Bar& targetBar = pActive->bars[targetBarIdx];
    int barTicks = targetBar.GetTotalTicks();
    if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

    int chordTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats > 0 ? chord.durationBeats * TICKS_PER_BEAT : TICKS_PER_BEAT);
    chordTicks = std::max(1, chordTicks);

    // 前小節から伸びてくる跨ぎカードに覆われた区間へは配置しない
    int minGrid = std::max(1, m_timelineGridTicks);
    int maxStart = std::max(0, barTicks - std::min(barTicks, minGrid));
    int covered = TimelineEdit::IncomingCoverUntil(pActive->bars, targetBarIdx);
    int minStart = std::min(covered, maxStart);
    int newStart = std::clamp(targetTickInBar, minStart, maxStart);

    ChordToken placed = chord;
    placed.SetTicks(chordTicks);

    auto overflow = TimelineEdit::InsertChord(targetBar, placed, newStart);
    for (const auto& over : overflow) {
        // 再帰で pActive->bars が拡張されるため参照を保持しない
        InsertChordAtTimelineTick(over.chord, targetBarIdx + 1, over.startTick, false);
    }

    m_selectedBarIndex = targetBarIdx;
    m_selectedChordIndex = TimelineEdit::ChordIndexAtTick(pActive->bars[targetBarIdx], newStart);
    if (m_selectedChordIndex < 0) m_selectedChordIndex = 0;

    EnsureVisible(m_selectedBarIndex);
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::InsertChordAtTimelinePosition(const ChordToken& chord, int targetBarIdx, int targetBeat) {
    InsertChordAtTimelineTick(chord, targetBarIdx, targetBeat * TICKS_PER_BEAT);
}

void AppWindow::SplitSelectedChord() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;
    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;

    auto& chord = bar.chords[m_selectedChordIndex];
    int totalTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);

    if (totalTicks < 240) {
        SetStatusMessage(TR(L"これ以上分割できません", L"Cannot split further"));
        return;
    }

    PushUndo();
    m_isMergedView = false;

    int halfTicks = totalTicks / 2;
    int snap = std::max(60, m_timelineGridTicks);
    if (totalTicks >= snap * 2) {
        halfTicks = (halfTicks / snap) * snap;
    }
    int secondTicks = totalTicks - halfTicks;

    chord.SetTicks(halfTicks);

    ChordToken secondChord = chord;
    secondChord.SetTicks(secondTicks);

    bar.chords.insert(bar.chords.begin() + m_selectedChordIndex + 1, secondChord);

    SyncTimelineToText();
    SetStatusMessage(TR(L"コードを分割しました (B)", L"Split chord (B)"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::DuplicateSelectedChord() {
    DuplicateSelection();
}

void AppWindow::ShiftSelectedChordOctave(int delta) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;
    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;

    auto& chord = bar.chords[m_selectedChordIndex];
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
        SetStatusMessage(TR(L"休符はオクターブ変更できません", L"Rests cannot be transposed"));
        return;
    }

    int next = std::clamp(chord.octaveOffset + delta, -3, 3);
    if (next == chord.octaveOffset) {
        SetStatusMessage(TR(L"これ以上オクターブを変更できません", L"Octave limit reached"));
        return;
    }

    PushUndo();
    m_isMergedView = false;
    chord.octaveOffset = next;
    if (next == 0) {
        SetStatusMessage(TR(L"オクターブをリセットしました", L"Octave reset"));
    } else {
        std::wstring msg = TR(L"オクターブ", L"Octave");
        msg += (next > 0) ? L" +" : L" ";
        msg += std::to_wstring(next);
        SetStatusMessage(msg);
    }
    AuditionChord(chord);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ResetSelectedChordOctave() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;
    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;

    auto& chord = bar.chords[m_selectedChordIndex];
    if (chord.isRest || chord.octaveOffset == 0) return;

    PushUndo();
    m_isMergedView = false;
    chord.octaveOffset = 0;
    SetStatusMessage(TR(L"オクターブをリセットしました", L"Octave reset"));
    AuditionChord(chord);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::InsertChordAtTimelineX(const ChordToken& chord, float x, bool pushUndo, int snapTicks) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    float centerX = m_leftPaneWidth;
    float barWidth = m_pixelsPerBeat * 4.0f;
    float relX = (x - centerX) + m_timelineScrollX;
    if (relX < 0.0f) relX = 0.0f;

    int targetBarIdx = (barWidth > 0.0f) ? static_cast<int>(relX / barWidth) : 0;
    float barOffsetRatio = (barWidth > 0.0f) ? (std::fmod(relX, barWidth) / barWidth) : 0.0f;

    int barTotalTicks = -1;
    if (targetBarIdx >= 0 && targetBarIdx < static_cast<int>(pActive->bars.size())) {
        barTotalTicks = pActive->bars[targetBarIdx].GetTotalTicks();
    }
    if (barTotalTicks <= 0) {
        int barNumerator = (pActive->timeNumerator > 0) ? pActive->timeNumerator : 4;
        barTotalTicks = barNumerator * TICKS_PER_BEAT;
    }

    int gridTicks = std::max(1, snapTicks > 0 ? snapTicks : m_timelineGridTicks);
    float rawTick = barOffsetRatio * static_cast<float>(barTotalTicks);
    int snappedTick = static_cast<int>(std::round(rawTick / static_cast<float>(gridTicks))) * gridTicks;
    int fallbackTicks = std::max(1, m_inputGridTicks);
    int chordTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats > 0 ? chord.durationBeats * TICKS_PER_BEAT : fallbackTicks);
    chordTicks = std::max(1, chordTicks);
    int maxStart = std::max(0, barTotalTicks - std::min(barTotalTicks, gridTicks));
    snappedTick = std::clamp(snappedTick, 0, maxStart);

    ChordToken placedChord = chord;
    placedChord.SetTicks(chordTicks);
    InsertChordAtTimelineTick(placedChord, targetBarIdx, snappedTick, pushUndo);
}

void AppWindow::SyncTextToTimeline() {
    if (!m_hwndEdit) return;

    int len = GetWindowTextLengthW(m_hwndEdit);
    std::wstring text(len, 0);
    GetWindowTextW(m_hwndEdit, text.data(), len + 1);

    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    PushUndo();
    std::string utf8 = ToUtf8String(text);
    pActive->bars = ProgressionParser::Parse(utf8, pActive->keyRoot, pActive->mode);
    pActive->rawProgressionText = utf8;

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::SyncTimelineToText() {
    if (!m_hwndEdit) return;

    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    std::string text = ProgressionParser::Format(pActive->bars);
    pActive->rawProgressionText = text;
    std::wstring wtext = ToWString(text);

    SetWindowTextW(m_hwndEdit, wtext.c_str());
}

void AppWindow::PushUndo() {
    UndoSnapshot snap;
    snap.parts = m_partManager.GetParts();
    snap.activePartIndex = m_partManager.GetActiveIndex();
    m_undoStack.push_back(std::move(snap));
    if (m_undoStack.size() > 30) m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear();
    m_isDirty = true;
    UpdateWindowTitle();
}

void AppWindow::Undo() {
    if (m_undoStack.empty()) return;
    UndoSnapshot current;
    current.parts = m_partManager.GetParts();
    current.activePartIndex = m_partManager.GetActiveIndex();
    m_redoStack.push_back(std::move(current));

    auto prev = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    m_partManager.GetParts() = std::move(prev.parts);
    m_partManager.SetActiveIndex(prev.activePartIndex);
    m_isDirty = true;
    UpdateWindowTitle();
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::Redo() {
    if (m_redoStack.empty()) return;
    UndoSnapshot next = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    UndoSnapshot current;
    current.parts = m_partManager.GetParts();
    current.activePartIndex = m_partManager.GetActiveIndex();
    m_undoStack.push_back(std::move(current));

    m_partManager.GetParts() = std::move(next.parts);
    m_partManager.SetActiveIndex(next.activePartIndex);
    m_isDirty = true;
    UpdateWindowTitle();
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::DuplicateSelection() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;

    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;
    if (bar.chords[m_selectedChordIndex].isRest) return;

    // 元コードの直後へ「長さぶん押し出して」複製する（小節をはみ出す分は次小節へ）
    int insertTick = 0;
    for (int i = 0; i <= m_selectedChordIndex; ++i) {
        int d = bar.chords[i].durationTicks > 0 ? bar.chords[i].durationTicks : (bar.chords[i].durationBeats * TICKS_PER_BEAT);
        insertTick += d;
    }
    ChordToken copy = bar.chords[m_selectedChordIndex];
    int copyTicks = copy.durationTicks > 0 ? copy.durationTicks : (copy.durationBeats * TICKS_PER_BEAT);
    int barTicks = bar.GetTotalTicks();
    if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

    int targetBar = m_selectedBarIndex;
    int targetTick = insertTick;
    if (targetTick + copyTicks > barTicks) {
        // 小節末尾を超える場合は次小節の先頭へ（元コードを上書きしない）
        targetBar += 1;
        targetTick = 0;
    }

    PushUndo();
    InsertChordAtTimelineTick(copy, targetBar, targetTick, false);
}

void AppWindow::DeleteSelection() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;

    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;

    PushUndo();
    bar.chords.erase(bar.chords.begin() + m_selectedChordIndex);
    if (bar.chords.empty() && pActive->bars.size() > 1) {
        pActive->bars.erase(pActive->bars.begin() + m_selectedBarIndex);
    }
    m_selectedBarIndex = -1;
    m_selectedChordIndex = -1;
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::SelectAll() {
    Part* pActive = m_partManager.GetActivePart();
    if (pActive && !pActive->bars.empty() && !pActive->bars[0].chords.empty()) {
        SelectChord(0, 0);
    }
}

// ---------------------------------------------------------------------------
// 再生エンジン (tick単位精密制御)
// ---------------------------------------------------------------------------
void AppWindow::TogglePlayback() {
    if (m_isPlaying) {
        m_isPlaying = false;
        m_synth.AllNotesOff();
    } else {
        m_isPlaying = true;
        m_lastPlaybackTime = std::chrono::steady_clock::now();
        m_lastSoundedChordUid = -1;

        // 再生開始時に現在位置のコードを即座に発音
        std::vector<Bar> currentBars = m_isMergedView ?
            m_partManager.GenerateMergedProgression() :
            (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});

        int accTicks = 0;
        int chordUidCounter = 0;
        bool chordSounded = false;
        for (size_t bi = 0; bi < currentBars.size(); ++bi) {
            const auto& b = currentBars[bi];
            int barTicks = b.GetTotalTicks();
            if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

            int chordOffset = 0;
            for (size_t ci = 0; ci < b.chords.size(); ++ci) {
                const auto& c = b.chords[ci];
                int cTicks = c.durationTicks > 0 ? c.durationTicks : (c.durationBeats * TICKS_PER_BEAT);
                int startTick = accTicks + chordOffset;
                int endTick = startTick + cTicks;

                if (cTicks > 0 && m_playbackTick >= startTick && m_playbackTick < endTick) {
                    m_lastSoundedChordUid = chordUidCounter;
                    m_selectedBarIndex = static_cast<int>(bi);
                    m_selectedChordIndex = static_cast<int>(ci);
                    EnsureVisible(m_selectedBarIndex);
                    AuditionChord(c);
                    chordSounded = true;
                    break;
                }

                chordOffset += cTicks;
                chordUidCounter++;
            }
            if (chordSounded) break;
            accTicks += barTicks;
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::StopPlayback() {
    m_isPlaying = false;
    m_playbackTick = 0.0;
    m_lastSoundedChordUid = -1;
    m_synth.AllNotesOff();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::AdvancePlayback(float deltaSeconds) {
    Part* pActive = m_partManager.GetActivePart();
    int bpm = pActive ? pActive->bpm : 120;

    double deltaTicks = deltaSeconds * (bpm / 60.0) * TICKS_PER_BEAT;
    m_playbackTick += deltaTicks;

    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (pActive ? pActive->bars : std::vector<Bar>{});

    int totalSongTicks = 0;
    for (const auto& b : currentBars) {
        int barTicks = b.GetTotalTicks();
        totalSongTicks += (barTicks > 0 ? barTicks : TICKS_PER_BAR_4_4);
    }

    if (totalSongTicks > 0 && m_playbackTick >= totalSongTicks) {
        if (m_isLooping) {
            m_playbackTick = std::fmod(m_playbackTick, static_cast<double>(totalSongTicks));
            m_lastSoundedChordUid = -1;
        } else {
            StopPlayback();
            return;
        }
    }

    int accTicks = 0;
    int chordUidCounter = 0;

    for (size_t bi = 0; bi < currentBars.size(); ++bi) {
        const auto& b = currentBars[bi];
        int barTicks = b.GetTotalTicks();
        if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

        int chordOffset = 0;
        for (size_t ci = 0; ci < b.chords.size(); ++ci) {
            const auto& c = b.chords[ci];
            int cTicks = c.durationTicks > 0 ? c.durationTicks : (c.durationBeats * TICKS_PER_BEAT);
            int startTick = accTicks + chordOffset;
            int endTick = startTick + cTicks;

            if (m_playbackTick >= startTick && m_playbackTick < endTick) {
                if (m_lastSoundedChordUid != chordUidCounter) {
                    m_lastSoundedChordUid = chordUidCounter;
                    m_selectedBarIndex = static_cast<int>(bi);
                    m_selectedChordIndex = static_cast<int>(ci);
                    EnsureVisible(m_selectedBarIndex);
                    AuditionChord(c);
                }
                return;
            }

            chordOffset += cTicks;
            chordUidCounter++;
        }
        accTicks += barTicks;
    }

    // コードが存在しない空白・休符区間に達した場合は確実に消音
    if (m_lastSoundedChordUid != -1) {
        m_lastSoundedChordUid = -1;
        m_synth.AllNotesOff();
    }
}

void AppWindow::SeekToTick(double tick) {
    m_playbackTick = std::max(0.0, tick);
    m_lastSoundedChordUid = -1;

    std::vector<Bar> currentBars = m_isMergedView ?
        m_partManager.GenerateMergedProgression() :
        (m_partManager.GetActivePart() ? m_partManager.GetActivePart()->bars : std::vector<Bar>{});

    int accTicks = 0;
    int chordUidCounter = 0;
    bool found = false;

    for (size_t bi = 0; bi < currentBars.size(); ++bi) {
        const auto& b = currentBars[bi];
        int barTicks = b.GetTotalTicks();
        if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;

        int chordOffset = 0;
        for (size_t ci = 0; ci < b.chords.size(); ++ci) {
            const auto& c = b.chords[ci];
            int cTicks = c.durationTicks > 0 ? c.durationTicks : (c.durationBeats * TICKS_PER_BEAT);
            int startTick = accTicks + chordOffset;
            int endTick = startTick + cTicks;

            if (m_playbackTick >= startTick && m_playbackTick < endTick) {
                m_selectedBarIndex = static_cast<int>(bi);
                m_selectedChordIndex = static_cast<int>(ci);
                EnsureVisible(m_selectedBarIndex);
                if (m_isPlaying) {
                    m_lastSoundedChordUid = chordUidCounter;
                    AuditionChord(c);
                }
                found = true;
                break;
            }

            chordOffset += cTicks;
            chordUidCounter++;
        }
        if (found) break;
        accTicks += barTicks;
    }

    if (!found) {
        m_synth.AllNotesOff();
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// ウィンドウタイトル更新・ステータス・自動バックアップ
// ---------------------------------------------------------------------------
void AppWindow::UpdateWindowTitle() {
    std::wstring title = L"Chord Launch";
    if (!m_currentProjectPath.empty()) {
        std::filesystem::path p(m_currentProjectPath);
        title += L" - [" + p.filename().wstring() + L"]";
    } else {
        title += L" - [" + ToWString(m_projectTitle) + L"]";
    }
    if (m_isDirty) {
        title += L" *";
    }
    SetWindowTextW(m_hwnd, title.c_str());
}

void AppWindow::SetStatusMessage(const std::wstring& msg) {
    m_statusMessage = msg;
    m_statusMessageTime = std::chrono::steady_clock::now();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::TriggerAutoBackup() {
    if (!m_isDirty) return;
    std::string data = m_partManager.SerializeProject(
        m_projectTitle, m_selectedVoicing, m_bassOctaveMode, m_timelineGridTicks, m_inputGridTicks
    );
    std::wstring backupDir;
    if (!m_autoBackupInProjectDir && !m_autoBackupDirectory.empty()) {
        backupDir = m_autoBackupDirectory;
    }
    if (PartManager::PerformAutoBackup(m_currentProjectPath, m_projectTitle, data, backupDir)) {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow;
        localtime_s(&tmNow, &tt);
        wchar_t timeBuf[32];
        wcsftime(timeBuf, 32, L"%H:%M:%S", &tmNow);
        SetStatusMessage(std::wstring(TR(L"自動バックアップ完了 (", L"Auto-backup complete (")) + timeBuf + L")");
    }
}

// ---------------------------------------------------------------------------
// テンプレート管理・試聴
// ---------------------------------------------------------------------------
void AppWindow::OpenTemplateEditor(bool isEdit) {
    m_tmplEditorOpen = true;
    m_tmplIsEditingExisting = isEdit;
    m_tmplEditFocusField = 0;

    auto categories = MusicEngine::GetTemplateMajorCategories();
    std::string curCat = (m_selectedTmplCategoryIndex < static_cast<int>(categories.size())) ? categories[m_selectedTmplCategoryIndex] : "👤 ユーザー定義";
    auto tmpls = MusicEngine::GetTemplatesByCategory(curCat);

    if (isEdit && m_selectedTmplIndex >= 0 && m_selectedTmplIndex < static_cast<int>(tmpls.size())) {
        const auto& t = tmpls[m_selectedTmplIndex];
        m_tmplEditId = t.id;
        m_tmplEditName = ToWString(t.name);
        m_tmplEditProg = ToWString(t.progression);
        m_tmplEditDesc = ToWString(t.description);
    } else {
        m_tmplEditId = "user_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        Part* pActive = m_partManager.GetActivePart();
        std::string curProg = pActive ? pActive->rawProgressionText : "C G Am F";
        m_tmplEditName = TR(L"新規テンプレート", L"New Template");
        m_tmplEditProg = ToWString(curProg);
        m_tmplEditDesc = TR(L"ユーザー作成コード進行", L"User custom progression");
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::CloseTemplateEditor() {
    m_tmplEditorOpen = false;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::SaveTemplateFromEditor() {
    if (m_tmplEditName.empty()) m_tmplEditName = TR(L"無題テンプレート", L"Untitled Template");
    if (m_tmplEditProg.empty()) m_tmplEditProg = L"C G Am F";

    ProgressionTemplate pt;
    pt.id = m_tmplEditId.empty() ? ("user_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())) : m_tmplEditId;
    pt.majorCategory = "👤 ユーザー定義";
    pt.subCategory = "カスタム";
    pt.name = ToUtf8String(m_tmplEditName);
    pt.progression = ToUtf8String(m_tmplEditProg);
    pt.description = ToUtf8String(m_tmplEditDesc);
    pt.isUserCustom = true;

    MusicEngine::SaveUserTemplate(pt);
    m_tmplEditorOpen = false;

    // カテゴリを「ユーザー定義」に切り替えて最新登録を表示
    auto categories = MusicEngine::GetTemplateMajorCategories();
    for (size_t i = 0; i < categories.size(); ++i) {
        if (categories[i] == "👤 ユーザー定義") {
            m_selectedTmplCategoryIndex = static_cast<int>(i);
            m_selectedTmplIndex = 0;
            break;
        }
    }
    SetStatusMessage(std::wstring(TR(L"テンプレートを保存しました: ", L"Saved template: ")) + m_tmplEditName);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::DeleteSelectedUserTemplate() {
    auto categories = MusicEngine::GetTemplateMajorCategories();
    if (m_selectedTmplCategoryIndex < 0 || m_selectedTmplCategoryIndex >= static_cast<int>(categories.size())) return;
    std::string curCat = categories[m_selectedTmplCategoryIndex];
    auto tmpls = MusicEngine::GetTemplatesByCategory(curCat);

    if (m_selectedTmplIndex < 0 || m_selectedTmplIndex >= static_cast<int>(tmpls.size())) return;
    const auto& t = tmpls[m_selectedTmplIndex];
    if (!t.isUserCustom) {
        MessageBoxW(m_hwnd, TR(L"組み込みの定番テンプレートは削除できません。", L"Built-in templates cannot be deleted."), TR(L"情報", L"Information"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring confirmMsg = (m_settingLanguage == 1)
        ? (L"Delete template \"" + ToWString(t.name) + L"\"?")
        : (L"テンプレート「" + ToWString(t.name) + L"」を削除しますか？");
    int ret = MessageBoxW(m_hwnd, confirmMsg.c_str(), TR(L"削除確認", L"Confirm Delete"), MB_YESNO | MB_ICONQUESTION);
    if (ret == IDYES) {
        MusicEngine::DeleteUserTemplate(t.id);
        m_selectedTmplIndex = std::max(0, m_selectedTmplIndex - 1);
        SetStatusMessage(TR(L"テンプレートを削除しました", L"Deleted template"));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void AppWindow::AuditionTemplate(const ProgressionTemplate& tmpl) {
    Part* pActive = m_partManager.GetActivePart();
    std::string key = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    auto bars = ProgressionParser::Parse(tmpl.progression, key, mode);
    if (!bars.empty()) {
        for (const auto& b : bars) {
            for (const auto& c : b.chords) {
                if (!c.isRest) {
                    AuditionChord(c);
                    return;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ヘルプ・オンラインドキュメント
// ---------------------------------------------------------------------------
void AppWindow::OpenOnlineDocumentation() {
    ShellExecuteW(nullptr, L"open", L"https://github.com/stellorbit/ChordLaunch#readme", nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
// ファイル操作・MIDIドラッグ
// ---------------------------------------------------------------------------
void AppWindow::DoFileNew() {
    PushUndo();
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
    m_currentProjectPath.clear();
    m_projectTitle = "MyProgression";
    m_isDirty = false;
    SyncTimelineToText();
    UpdateWindowTitle();
    SetStatusMessage(TR(L"新規プロジェクトを開始しました", L"Started new project"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::DoFileSave() {
    if (m_currentProjectPath.empty()) {
        DoFileSaveAs();
        return;
    }

    if (m_partManager.SaveToFile(
        m_currentProjectPath, m_projectTitle,
        m_selectedVoicing, m_bassOctaveMode, m_timelineGridTicks, m_inputGridTicks
    )) {
        m_isDirty = false;
        UpdateWindowTitle();
        AddRecentFile(m_currentProjectPath);
        SetStatusMessage(TR(L"プロジェクトを保存しました", L"Project saved"));
    } else {
        MessageBoxW(m_hwnd, TR(L"プロジェクトの保存に失敗しました。", L"Failed to save project."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
    }
}

void AppWindow::DoFileSaveAs() {
    wchar_t szFile[MAX_PATH] = L"";
    std::wstring initName = ToWString(m_projectTitle);
    if (initName.empty()) initName = L"MyProgression";
    initName += L".chord";
    wcscpy_s(szFile, initName.c_str());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"ChordLaunch Project (*.chord)\0*.chord\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"chord";
    if (!m_projectDirectory.empty() && std::filesystem::exists(m_projectDirectory)) {
        ofn.lpstrInitialDir = m_projectDirectory.c_str();
    }

    if (GetSaveFileNameW(&ofn)) {
        m_currentProjectPath = szFile;
        std::filesystem::path p(szFile);
        m_projectTitle = ToUtf8String(p.stem().wstring());

        if (m_partManager.SaveToFile(
            m_currentProjectPath, m_projectTitle,
            m_selectedVoicing, m_bassOctaveMode, m_timelineGridTicks, m_inputGridTicks
        )) {
            m_isDirty = false;
            UpdateWindowTitle();
            AddRecentFile(m_currentProjectPath);
            SetStatusMessage(TR(L"プロジェクトを別名で保存しました", L"Project saved as new file"));
        } else {
            MessageBoxW(m_hwnd, TR(L"プロジェクトの保存に失敗しました。", L"Failed to save project."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
        }
    }
}

void AppWindow::DoFileOpen() {
    wchar_t szFile[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"ChordLaunch Project (*.chord)\0*.chord\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"chord";
    if (!m_projectDirectory.empty() && std::filesystem::exists(m_projectDirectory)) {
        ofn.lpstrInitialDir = m_projectDirectory.c_str();
    }

    if (GetOpenFileNameW(&ofn)) {
        std::string loadedTitle;
        VoicingStyle loadedVoicing;
        int loadedBassMode = 0;
        int loadedGrid = TICKS_PER_BEAT;
        int loadedInputGrid = TICKS_PER_BEAT;

        if (m_partManager.LoadFromFile(szFile, loadedTitle, loadedVoicing, loadedBassMode, loadedGrid, &loadedInputGrid)) {
            m_currentProjectPath = szFile;
            m_projectTitle = loadedTitle;
            m_selectedVoicing = loadedVoicing;
            m_bassOctaveMode = loadedBassMode;
            m_timelineGridTicks = loadedGrid;
            m_inputGridTicks = loadedInputGrid;
            m_isDirty = false;

            SyncTimelineToText();
            UpdateWindowTitle();
            AddRecentFile(szFile);
            SetStatusMessage(std::wstring(TR(L"プロジェクトを開きました: ", L"Opened project: ")) + ToWString(m_projectTitle));
            InvalidateRect(m_hwnd, nullptr, FALSE);
        } else {
            MessageBoxW(m_hwnd, TR(L"プロジェクトファイルの読み込みに失敗しました。", L"Failed to load project file."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
        }
    }
}

void AppWindow::DoMidiSaveAs() {
    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    if (bars.empty()) return;

    wchar_t szFile[MAX_PATH] = L"progression.mid";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Standard MIDI File (*.mid)\0*.mid\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"mid";

    if (GetSaveFileNameW(&ofn)) {
        int bpm = pActive ? pActive->bpm : 120;
        auto bytes = MidiBuilder::BuildMidi(bars, bpm, m_selectedVoicing, m_baseOctave, 90, m_bassOctaveMode);
        if (!bytes.empty()) {
            HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
                CloseHandle(hFile);
                SetStatusMessage(TR(L"MIDIファイルを保存しました", L"MIDI file saved"));
            }
        }
    }
}

void AppWindow::StartDawDrag() {
    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    if (bars.empty()) return;

    int bpm = pActive ? pActive->bpm : 120;
    std::wstring tempMidiPath = MidiBuilder::ExportToTempFile(bars, bpm, m_selectedVoicing, L"ChordLaunch_Progression", m_bassOctaveMode);
    if (!tempMidiPath.empty()) {
        OleFileDataObject::StartFileDrag(m_hwnd, tempMidiPath);
    }
}

void AppWindow::CopyProgressionToClipboard() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;
    std::wstring wtext = ToWString(pActive->rawProgressionText);
    if (wtext.empty()) return;

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t bytes = (wtext.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, wtext.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
        SetStatusMessage(TR(L"コード進行をクリップボードにコピーしました", L"Copied progression to clipboard"));
    }
}

void AppWindow::PasteProgressionFromClipboard() {
    if (!OpenClipboard(m_hwnd)) return;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* pszText = static_cast<const wchar_t*>(GlobalLock(hData));
        if (pszText) {
            std::wstring wtext(pszText);
            GlobalUnlock(hData);
            Part* pActive = m_partManager.GetActivePart();
            if (pActive && !wtext.empty()) {
                PushUndo();
                std::string utf8 = ToUtf8String(wtext);
                pActive->bars = ProgressionParser::Parse(utf8, pActive->keyRoot, pActive->mode);
                pActive->rawProgressionText = utf8;
                SyncTimelineToText();
                SetStatusMessage(TR(L"クリップボードからコード進行を貼り付けました", L"Pasted progression from clipboard"));
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
    }
    CloseClipboard();
}

namespace {

std::string EscapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string UnescapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '"') { out += '"'; ++i; }
            else if (next == '\\') { out += '\\'; ++i; }
            else if (next == '/') { out += '/'; ++i; }
            else if (next == 'b') { out += '\b'; ++i; }
            else if (next == 'f') { out += '\f'; ++i; }
            else if (next == 'n') { out += '\n'; ++i; }
            else if (next == 'r') { out += '\r'; ++i; }
            else if (next == 't') { out += '\t'; ++i; }
            else { out += next; ++i; }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string FindJsonStr(const std::string& json, std::string_view key) {
    std::string pattern = "\"" + std::string(key) + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return "";
    size_t firstQuote = json.find('"', colon + 1);
    if (firstQuote == std::string::npos) return "";
    size_t secondQuote = json.find('"', firstQuote + 1);
    while (secondQuote != std::string::npos && json[secondQuote - 1] == '\\') {
        secondQuote = json.find('"', secondQuote + 1);
    }
    if (secondQuote == std::string::npos) return "";
    return json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::filesystem::path GetSettingsFilePath() {
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
        return std::filesystem::path(appData) / L"ChordLaunch" / L"settings.json";
    }
    return L"settings.json";
}

} // namespace

void AppWindow::AddRecentFile(const std::wstring& path) {
    if (path.empty()) return;
    std::wstring normPath = NormalizePathString(path);
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), normPath);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }
    m_recentFiles.insert(m_recentFiles.begin(), normPath);
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }
    SaveAppSettings();
    RebuildMainMenu();
}

void AppWindow::OpenRecentFile(int index) {
    if (index < 0 || index >= static_cast<int>(m_recentFiles.size())) return;
    std::wstring path = m_recentFiles[index];
    if (!std::filesystem::exists(path)) {
        MessageBoxW(m_hwnd, TR(L"指定されたプロジェクトファイルが見つかりません。", L"Project file not found."), TR(L"エラー", L"Error"), MB_OK | MB_ICONWARNING);
        m_recentFiles.erase(m_recentFiles.begin() + index);
        SaveAppSettings();
        RebuildMainMenu();
        return;
    }

    std::string loadedTitle;
    VoicingStyle loadedVoicing;
    int loadedBassMode = 0;
    int loadedGrid = TICKS_PER_BEAT;
    int loadedInputGrid = TICKS_PER_BEAT;

    if (m_partManager.LoadFromFile(path, loadedTitle, loadedVoicing, loadedBassMode, loadedGrid, &loadedInputGrid)) {
        m_currentProjectPath = path;
        m_projectTitle = loadedTitle;
        m_selectedVoicing = loadedVoicing;
        m_bassOctaveMode = loadedBassMode;
        m_timelineGridTicks = loadedGrid;
        m_inputGridTicks = loadedInputGrid;
        m_isDirty = false;

        AddRecentFile(path);
        SyncTimelineToText();
        UpdateWindowTitle();
        SetStatusMessage(std::wstring(TR(L"プロジェクトを開きました: ", L"Opened project: ")) + ToWString(m_projectTitle));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else {
        MessageBoxW(m_hwnd, TR(L"プロジェクトファイルの読み込みに失敗しました。", L"Failed to load project file."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
    }
}

void AppWindow::LoadAppSettings() {
    try {
        auto path = GetSettingsFilePath();
        if (!std::filesystem::exists(path)) return;
        std::ifstream ifs(path);
        if (!ifs.is_open()) return;
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        size_t posDash = content.find("\"showDashboardOnStartup\"");
        if (posDash != std::string::npos) {
            size_t valPos = content.find(':', posDash);
            if (valPos != std::string::npos) {
                std::string sub = content.substr(valPos + 1, 20);
                if (sub.find("false") != std::string::npos) m_showDashboardOnStartup = false;
                else if (sub.find("true") != std::string::npos) m_showDashboardOnStartup = true;
            }
        }

        size_t posBak = content.find("\"autoBackupInProjectDir\"");
        if (posBak != std::string::npos) {
            size_t valPos = content.find(':', posBak);
            if (valPos != std::string::npos) {
                std::string sub = content.substr(valPos + 1, 20);
                if (sub.find("false") != std::string::npos) m_autoBackupInProjectDir = false;
                else if (sub.find("true") != std::string::npos) m_autoBackupInProjectDir = true;
            }
        }

        m_projectDirectory = NormalizePathString(ToWString(UnescapeJson(FindJsonStr(content, "projectDirectory"))));
        m_autoBackupDirectory = NormalizePathString(ToWString(UnescapeJson(FindJsonStr(content, "autoBackupDirectory"))));

        m_recentFiles.clear();
        size_t posRec = content.find("\"recentFiles\"");
        if (posRec != std::string::npos) {
            size_t arrStart = content.find('[', posRec);
            size_t arrEnd = content.find(']', arrStart);
            if (arrStart != std::string::npos && arrEnd != std::string::npos) {
                size_t cur = arrStart + 1;
                while (cur < arrEnd) {
                    size_t sQuote = content.find('"', cur);
                    if (sQuote == std::string::npos || sQuote >= arrEnd) break;
                    size_t eQuote = content.find('"', sQuote + 1);
                    while (eQuote != std::string::npos && eQuote < arrEnd) {
                        // クォート直前の連続バックスラッシュ数をカウント
                        size_t bCount = 0;
                        size_t checkPos = eQuote;
                        while (checkPos > sQuote && content[checkPos - 1] == '\\') {
                            ++bCount;
                            --checkPos;
                        }
                        // 奇数個のバックスラッシュはエスケープされた引用符 \" なので継続
                        if (bCount % 2 == 1) {
                            eQuote = content.find('"', eQuote + 1);
                        } else {
                            break;
                        }
                    }
                    if (eQuote == std::string::npos || eQuote >= arrEnd) break;
                    std::string item = content.substr(sQuote + 1, eQuote - sQuote - 1);
                    std::wstring witem = NormalizePathString(ToWString(UnescapeJson(item)));
                    if (!witem.empty() && std::filesystem::exists(witem)) {
                        m_recentFiles.push_back(witem);
                    }
                    cur = eQuote + 1;
                }
            }
        }
    } catch (...) {}
}

void AppWindow::SaveAppSettings() {
    try {
        auto path = GetSettingsFilePath();
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path);
        if (!ofs.is_open()) return;

        ofs << "{\n";
        ofs << "  \"showDashboardOnStartup\": " << (m_showDashboardOnStartup ? "true" : "false") << ",\n";
        ofs << "  \"autoBackupInProjectDir\": " << (m_autoBackupInProjectDir ? "true" : "false") << ",\n";
        ofs << "  \"projectDirectory\": \"" << EscapeJson(ToUtf8String(NormalizePathString(m_projectDirectory))) << "\",\n";
        ofs << "  \"autoBackupDirectory\": \"" << EscapeJson(ToUtf8String(NormalizePathString(m_autoBackupDirectory))) << "\",\n";
        ofs << "  \"recentFiles\": [\n";
        for (size_t i = 0; i < m_recentFiles.size(); ++i) {
            ofs << "    \"" << EscapeJson(ToUtf8String(NormalizePathString(m_recentFiles[i]))) << "\"" << (i + 1 < m_recentFiles.size() ? "," : "") << "\n";
        }
        ofs << "  ]\n";
        ofs << "}\n";
    } catch (...) {}
}

void AppWindow::ChooseDirectory(std::wstring& outDir, const wchar_t* title) {
    IFileDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD dwOptions = 0;
        if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (title) {
            pfd->SetTitle(title);
        }
        if (!outDir.empty()) {
            IShellItem* psiFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(outDir.c_str(), nullptr, IID_PPV_ARGS(&psiFolder)))) {
                pfd->SetFolder(psiFolder);
                psiFolder->Release();
            }
        }
        if (SUCCEEDED(pfd->Show(m_hwnd))) {
            IShellItem* psiResult = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psiResult))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    outDir = pszPath;
                    CoTaskMemFree(pszPath);
                }
                psiResult->Release();
            }
        }
        pfd->Release();
    }
}

void AppWindow::DoMergeAllToNewPart() {
    auto mergedBars = m_partManager.GenerateMergedProgression();
    if (mergedBars.empty()) {
        SetStatusMessage(TR(L"マージ対象のコード進行がありません", L"No progression to merge"));
        return;
    }

    PushUndo();
    m_isMergedView = false;

    Part newPart;
    newPart.id = "part_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    newPart.name = "Full Song";
    Part* pActive = m_partManager.GetActivePart();
    newPart.keyRoot = pActive ? pActive->keyRoot : "C";
    newPart.mode = pActive ? pActive->mode : MusicalMode::Major;
    newPart.bpm = pActive ? pActive->bpm : 120;
    newPart.timeNumerator = 4;
    newPart.timeDenominator = 4;
    newPart.bars = mergedBars;

    std::string text;
    for (size_t bi = 0; bi < mergedBars.size(); ++bi) {
        text += "| ";
        for (const auto& c : mergedBars[bi].chords) {
            text += c.rawText + " ";
        }
    }
    if (!text.empty()) text += "|";
    newPart.rawProgressionText = text;

    m_partManager.GetParts().push_back(newPart);
    m_partManager.SetActiveIndex(static_cast<int>(m_partManager.GetParts().size()) - 1);

    SyncTimelineToText();
    SetStatusMessage(TR(L"全体マージパートを新規作成しました: Full Song", L"Created merged part: Full Song"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::OpenRenamePartModal(int partIndex) {
    auto& parts = m_partManager.GetParts();
    if (partIndex < 0 || partIndex >= static_cast<int>(parts.size())) return;

    m_renamePartIndex = partIndex;
    m_renamePartName = ToWString(parts[partIndex].name);
    OpenModal(ModalType::RenamePart);
}

void AppWindow::OpenDashboardModal() {
    m_dashboardSelectedRecentIndex = -1;
    OpenModal(ModalType::Dashboard);
}

} // namespace clp
