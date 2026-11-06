#include "app_window_internal.hpp"
#include "../version.h"
namespace clp {
// ---------------------------------------------------------------------------
// タイムライン・コード操作実装
// ---------------------------------------------------------------------------
void AppWindow::SelectChord(int barIndex, int chordIndex) {
    m_multiSelTicks.clear();
    m_selNotePc = -1; // ノートのフォーカスは、ノートをクリックした直後にだけ設定される
    m_selectedBarIndex = barIndex;
    m_selectedChordIndex = chordIndex;
    m_selAnchorTick = PrimaryChordStartTick();
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

    int addTicks = (chord.durationTicks > 0) ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
    addTicks = std::max(1, addTicks);

    // 最後のコード（休符を除く）の終端の直後へ、絶対tickで追加する。
    // 小節線を越えても1枚のまま保持され、曲末を越える分は休符小節が足される
    std::vector<Bar> work = pActive->bars;
    if (work.empty()) EnsureBarsCoverTick(work, 1);
    int appendTick = 0;
    for (size_t b = 0; b < work.size(); ++b) {
        int local = 0;
        for (const auto& c : work[b].chords) {
            const int d = (c.durationTicks > 0) ? c.durationTicks : (c.durationBeats * TICKS_PER_BEAT);
            local += d;
            if (!c.isRest && d > 0) {
                appendTick = std::max(appendTick, TimelineEdit::BarStartTick(work, static_cast<int>(b)) + local);
            }
        }
    }

    ChordToken placed = chord;
    placed.SetTicks(addTicks);
    auto res = TimelineEdit::InsertChordSpan(work, placed, appendTick);
    if (res.selectedBar < 0) return;

    PushUndo();
    m_isMergedView = false;
    pActive->bars = std::move(res.bars);
    m_selectedBarIndex = res.selectedBar;
    m_selectedChordIndex = std::max(0, res.selectedChord);
    EnsureVisible(m_selectedBarIndex);
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ApplyTemplateProgression(const std::string& progression) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    std::vector<Bar> parsed = ProgressionParser::Parse(progression, pActive->keyRoot, pActive->mode);

    // 全コードを順に並べ、各コードの音価を入力グリッドへそろえる。
    // 小節頭のセクション名・キー変更は、その小節の先頭コードに紐づけて持ち越す。
    struct Entry {
        ChordToken chord;
        std::string sectionMarker;
        std::string keyChange;
    };
    std::vector<Entry> entries;
    for (const auto& b : parsed) {
        bool first = true;
        for (const auto& c : b.chords) {
            Entry e;
            e.chord = c;
            if (first) {
                e.sectionMarker = b.sectionMarker;
                e.keyChange = b.keyChange;
                first = false;
            }
            entries.push_back(std::move(e));
        }
    }
    if (entries.empty()) {
        pActive->bars = std::move(parsed);
        return;
    }

    const int unit = std::max(1, m_inputGridTicks);
    const int total = unit * static_cast<int>(entries.size());

    std::vector<Bar> work;
    EnsureBarsCoverTick(work, total);

    std::vector<std::pair<int, const Entry*>> markerAt;
    int tick = 0;
    for (const auto& e : entries) {
        if (!e.sectionMarker.empty() || !e.keyChange.empty()) markerAt.emplace_back(tick, &e);
        if (!e.chord.isRest) {
            ChordToken placed = e.chord;
            placed.SetTicks(unit);
            auto res = TimelineEdit::InsertChordSpan(work, placed, tick);
            if (res.selectedBar >= 0) work = std::move(res.bars);
        }
        tick += unit;
    }

    for (const auto& m : markerAt) {
        int barStart = 0;
        for (auto& bar : work) {
            const int bt = std::max(1, bar.GetTotalTicks());
            if (m.first >= barStart && m.first < barStart + bt) {
                if (bar.sectionMarker.empty()) bar.sectionMarker = m.second->sectionMarker;
                if (bar.keyChange.empty()) bar.keyChange = m.second->keyChange;
                break;
            }
            barStart += bt;
        }
    }

    pActive->bars = std::move(work);
}

void AppWindow::InsertChordAtTimelineTick(const ChordToken& chord, int targetBarIdx, int targetTickInBar, bool pushUndo) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    if (targetBarIdx < 0) targetBarIdx = 0;

    // 絶対tickで配置する。小節線を越えても1枚のまま保持され、
    // 小節内 tick が小節長以上の場合はそのまま後続小節の位置として扱う
    std::vector<Bar> work = pActive->bars;
    while (static_cast<int>(work.size()) <= targetBarIdx) {
        EnsureBarsCoverTick(work, TimelineEdit::SongTicks(work) + 1);
    }
    const int absStart = TimelineEdit::BarStartTick(work, targetBarIdx) + std::max(0, targetTickInBar);

    int chordTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats > 0 ? chord.durationBeats * TICKS_PER_BEAT : TICKS_PER_BEAT);
    chordTicks = std::max(1, chordTicks);

    ChordToken placed = chord;
    placed.SetTicks(chordTicks);

    auto res = TimelineEdit::InsertChordSpan(work, placed, absStart);
    if (res.selectedBar < 0) return;

    if (pushUndo) {
        PushUndo();
    }
    m_isMergedView = false;
    pActive->bars = std::move(res.bars);
    m_selectedBarIndex = res.selectedBar;
    m_selectedChordIndex = std::max(0, res.selectedChord);

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

    if (chord.isRest) return;

    int halfTicks = totalTicks / 2;
    int snap = std::max(60, m_timelineGridTicks);
    if (totalTicks >= snap * 2) {
        halfTicks = (halfTicks / snap) * snap;
    }
    int secondTicks = totalTicks - halfTicks;

    // 絶対tickで前半・後半へ分ける（小節線を越えるカードでも後半が小節外へ取り残されない）
    int startTick = TimelineEdit::BarStartTick(pActive->bars, m_selectedBarIndex);
    for (int i = 0; i < m_selectedChordIndex; ++i) {
        const auto& c = bar.chords[i];
        startTick += (c.durationTicks > 0) ? c.durationTicks : (c.durationBeats * TICKS_PER_BEAT);
    }
    ChordToken secondChord = chord;
    secondChord.SetTicks(secondTicks);

    auto first = TimelineEdit::ResizeChordSpan(pActive->bars, startTick, startTick, startTick + halfTicks, 1);
    if (first.selectedBar < 0) return;
    auto both = TimelineEdit::InsertChordSpan(first.bars, secondChord, startTick + halfTicks);
    if (both.selectedBar < 0) return;

    PushUndo();
    m_isMergedView = false;
    pActive->bars = std::move(both.bars);
    m_selectedBarIndex = first.selectedBar;
    m_selectedChordIndex = first.selectedChord;
    m_selAnchorTick = PrimaryChordStartTick();

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

void AppWindow::ShiftSelectedChordInversion(int delta) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return;
    auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return;

    auto& chord = bar.chords[m_selectedChordIndex];
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
        SetStatusMessage(TR(L"休符は転回できません", L"Rests cannot be inverted"));
        return;
    }

    // 構成音数（ベース追加なし・基本形）から、指定可能な転回の最大値を決める（最大第3転回）
    ChordToken probe = chord;
    probe.inversion = 0;
    probe.bassPitch = PitchClass::Unknown;
    const int toneCount = static_cast<int>(MusicEngine::GetChordMidiNotes(probe, m_baseOctave, VoicingStyle::RootPosition, 0, 0).size());
    const int maxInv = std::clamp(toneCount - 1, 0, 3);
    if (maxInv == 0) return;

    // 0..maxInv を循環させる（↑で次の転回、↓で前の転回）
    const int count = maxInv + 1;
    const int next = (((chord.inversion + delta) % count) + count) % count;

    PushUndo();
    m_isMergedView = false;
    chord.inversion = next;
    std::wstring msg = TR(L"転回: ", L"Inversion: ");
    if (next == 0) msg += TR(L"基本形", L"Root position");
    else msg += TR(L"第", L"") + std::to_wstring(next) + TR(L"転回形", next == 1 ? L"st inversion" : (next == 2 ? L"nd inversion" : L"rd inversion"));
    SetStatusMessage(msg);
    AuditionChord(chord);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

bool AppWindow::HitTestPianoRollNote(float x, float y, int& outBar, int& outChord, int& outPitch) const {
    if (!m_prValid || m_isMergedView || m_prNoteRowH <= 0.0f) return false;
    const float guideW = 28.0f;
    if (x < m_prRect.left + guideW || x > m_prRect.right || y < m_prRect.top || y > m_prRect.bottom) return false;

    const Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return false;

    const float barWidth = m_pixelsPerBeat * 4.0f;
    const float relX = (x - m_prRect.left) + m_timelineScrollX;
    if (relX < 0.0f || barWidth <= 0.0f) return false;
    const size_t b = static_cast<size_t>(relX / barWidth);
    if (b >= pActive->bars.size()) return false;

    const Bar& bar = pActive->bars[b];
    int barTicks = bar.GetTotalTicks();
    if (barTicks <= 0) barTicks = TICKS_PER_BAR_4_4;
    const double tickInBar = (static_cast<double>(relX) - static_cast<double>(b) * barWidth) / barWidth * barTicks;

    int offset = 0;
    int hitChord = -1;
    for (size_t c = 0; c < bar.chords.size(); ++c) {
        const ChordToken& ch = bar.chords[c];
        const int cTicks = ch.durationTicks > 0 ? ch.durationTicks : (ch.durationBeats * TICKS_PER_BEAT);
        if (tickInBar >= offset && tickInBar < offset + cTicks) { hitChord = static_cast<int>(c); break; }
        offset += cTicks;
    }
    if (hitChord < 0) return false;
    const ChordToken& chord = bar.chords[hitChord];
    if (chord.isRest || chord.rootPitch == PitchClass::Unknown) return false;

    const int pitch = m_prMinMidi + static_cast<int>(std::floor((m_prRect.bottom - y) / m_prNoteRowH));
    const auto notes = MusicEngine::GetChordMidiNotes(chord, m_baseOctave, m_selectedVoicing, chord.inversion, m_bassOctaveMode);
    if (std::find(notes.begin(), notes.end(), pitch) == notes.end()) return false;

    outBar = static_cast<int>(b);
    outChord = hitChord;
    outPitch = pitch;
    return true;
}

// ---------------------------------------------------------------------------
// ズーム・分割バー・ホバー説明
// ---------------------------------------------------------------------------
void AppWindow::ZoomTimelineAt(int steps, float anchorX) {
    if (steps == 0 || m_pixelsPerBeat <= 0.0f) return;
    const float oldPpb = m_pixelsPerBeat;
    const float newPpb = std::clamp(oldPpb + 8.0f * static_cast<float>(steps), 24.0f, 140.0f);
    if (newPpb == oldPpb) return;

    // アンカー位置の拍が画面上で動かないようにスクロール量を再計算する
    const float rel = anchorX - m_leftPaneWidth;
    const float anchorBeat = (rel + m_timelineScrollX) / oldPpb;
    m_pixelsPerBeat = newPpb;

    const Part* pActive = m_partManager.GetActivePart();
    const size_t numBars = m_isMergedView ? m_partManager.GenerateMergedProgression().size() : (pActive ? pActive->bars.size() : 8);
    const float totalContentWidth = std::max<size_t>(numBars + 16, 64) * (newPpb * 4.0f);
    const float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    const float viewWidth = m_width - m_leftPaneWidth - rightW;
    const float maxScroll = std::max(0.0f, totalContentWidth - viewWidth);
    m_timelineScrollX = std::clamp(anchorBeat * newPpb - rel, 0.0f, maxScroll);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ZoomPianoRollV(int steps) {
    if (steps == 0) return;
    const float next = std::clamp(m_prZoomV * std::pow(1.25f, static_cast<float>(steps)), 1.0f, 4.0f);
    m_prZoomV = (next < 1.05f) ? 1.0f : next;
    if (m_prZoomV <= 1.0f) m_prScrollRows = 0;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ScrollPianoRollV(int rows) {
    if (m_prZoomV <= 1.0f || rows == 0) return;
    m_prScrollRows += rows; // 範囲外の値は描画時に有効範囲へ丸められる
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// コードカード領域＋ピアノロールに使える高さ（ルーラー下端 〜 スクロールバー上端）
float AppWindow::TimelineAvailHeight() const {
    const float dockH = m_showKeyboard ? m_bottomDockHeight : 28.0f;
    const float scrollY = m_height - dockH - m_scrollBarHeight;
    const float cardY = m_toolbarHeight + m_partTabHeight + m_rulerHeight;
    return std::max(120.0f, scrollY - cardY);
}

float AppWindow::SplitBarY() const {
    return m_toolbarHeight + m_partTabHeight + m_rulerHeight + m_cardTrackHeight;
}

bool AppWindow::HitTestSplitBar(float x, float y) const {
    if (m_activeModal != ModalType::None || !m_prValid) return false;
    const float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    if (x < m_leftPaneWidth || x > m_width - rightW) return false;
    return std::fabs(y - SplitBarY()) <= 4.0f;
}

bool AppWindow::HitTestTimelineCard(float x, float y, bool& outEdge) {
    outEdge = false;
    if (m_isMergedView) return false;
    const Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return false;

    const float cardY = m_toolbarHeight + m_partTabHeight + m_rulerHeight + 5.0f;
    const float cardH = m_cardTrackHeight - 10.0f;
    const float rightW = m_showRightPane ? m_rightPaneWidth : 0.0f;
    const float centerX = m_leftPaneWidth;
    const float centerW = m_width - rightW - centerX;
    if (x < centerX || x > centerX + centerW || y < cardY || y > cardY + cardH) return false;

    const float barWidth = m_pixelsPerBeat * 4.0f;
    for (size_t b = 0; b < pActive->bars.size(); ++b) {
        const float bx = centerX + b * barWidth - m_timelineScrollX;
        const int barTicks = std::max(1, pActive->bars[b].GetTotalTicks());
        float offsetTicks = 0.0f;
        for (const auto& chord : pActive->bars[b].chords) {
            const int cTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats * TICKS_PER_BEAT);
            const float cx = bx + (offsetTicks / barTicks) * barWidth;
            const float cw = (static_cast<float>(cTicks) / barTicks) * barWidth;
            offsetTicks += cTicks;
            if (chord.isRest) continue;
            const D2D1_RECT_F cardRc = D2D1::RectF(cx + 2.0f, cardY, cx + cw - 2.0f, cardY + cardH);
            if (!PtInRectF(cardRc, x, y)) continue;
            const float edgeMargin = std::min(8.0f, cw * 0.20f);
            outEdge = (x <= cardRc.left + edgeMargin || x >= cardRc.right - edgeMargin);
            return true;
        }
    }
    return false;
}

std::wstring AppWindow::BuildHoverTip(float x, float y) {
    if (HitTestSplitBar(x, y)) {
        return TR(L"ドラッグで表示比率を変更", L"Drag to resize panes");
    }

    // 左右ペインのコードボタン
    ChordToken chord;
    if (HitTestPaletteCard(x, y, chord)) {
        std::wstring name = chord.isRest ? std::wstring(TR(L"休符", L"Rest")) : ToWString(chord.rootName + chord.qualitySuffix);
        if (!chord.isRest && !chord.bassName.empty()) name += L"/" + ToWString(chord.bassName);
        return name + TR(L": ダブルクリックで追加", L": double-click to add");
    }

    // タイムラインのコードカード（左右端 = 長さ変更 / 本体 = 移動）
    bool edge = false;
    if (HitTestTimelineCard(x, y, edge)) {
        return edge ? std::wstring(TR(L"ドラッグで長さ変更", L"Drag to resize"))
                    : std::wstring(TR(L"ドラッグ:移動 右クリック:編集", L"Drag: move  Right-click: edit"));
    }

    // ピアノロールのノート
    int hitBar = -1, hitChord = -1, hitPitch = 0;
    if (HitTestPianoRollNote(x, y, hitBar, hitChord, hitPitch)) {
        const Part* pActive = m_partManager.GetActivePart();
        const std::string keyRoot = pActive ? pActive->keyRoot : "C";
        const MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;
        const std::string nName = MusicEngine::PitchToNoteForKey(static_cast<PitchClass>(((hitPitch % 12) + 12) % 12), keyRoot, mode) + std::to_string(hitPitch / 12 - 1);
        return ToWString(nName) + TR(L": 上下ドラッグで移動", L": drag up/down");
    }
    return L"";
}

void AppWindow::OnPianoNoteDrag(float y) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_dragBarIndex < 0 || m_dragBarIndex >= static_cast<int>(pActive->bars.size())) return;
    auto& chords = pActive->bars[m_dragBarIndex].chords;
    if (m_dragChordIndex < 0 || m_dragChordIndex >= static_cast<int>(chords.size())) return;
    ChordToken& chord = chords[m_dragChordIndex];

    // マウス位置の音高を、同じ音名の最寄りオクターブ位置へスナップする
    const float rowH = (m_prNoteRowH > 0.0f) ? m_prNoteRowH : 1.0f;
    const int row = std::clamp(m_prMinMidi + static_cast<int>(std::floor((m_prRect.bottom - y) / rowH)), m_prDragMin, m_prDragMax);
    const int deltaOct = static_cast<int>(std::lround((row - m_prDragStartPitch) / 12.0));
    const int next = std::clamp(m_prDragStartShift + deltaOct, -3, 3);
    if (next == chord.toneShift[m_prDragPc]) return;

    if (!m_prDragUndoPushed) {
        PushUndo();
        m_prDragUndoPushed = true;
    }
    m_isMergedView = false;
    chord.toneShift[m_prDragPc] = static_cast<int8_t>(next);
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

    // 小節末尾へスナップした場合は次小節の先頭として扱う（小節内の上限で引き戻さない）
    int gridTicks = std::max(1, snapTicks > 0 ? snapTicks : m_timelineGridTicks);
    int targetBarIdx = 0;
    int snappedTick = 0;
    TimelineTickFromX(x, gridTicks, &targetBarIdx, &snappedTick);

    int fallbackTicks = std::max(1, m_inputGridTicks);
    int chordTicks = chord.durationTicks > 0 ? chord.durationTicks : (chord.durationBeats > 0 ? chord.durationBeats * TICKS_PER_BEAT : fallbackTicks);
    chordTicks = std::max(1, chordTicks);

    ChordToken placedChord = chord;
    placedChord.SetTicks(chordTicks);
    InsertChordAtTimelineTick(placedChord, targetBarIdx, snappedTick, pushUndo);
}

int AppWindow::TimelineTickFromX(float x, int gridTicks, int* outBar, int* outTickInBar, int* outBarTicks) const {
    const Part* pActive = m_partManager.GetActivePart();
    const std::vector<Bar> empty;
    const std::vector<Bar>& bars = pActive ? pActive->bars : empty;

    float centerX = m_leftPaneWidth;
    float barWidth = m_pixelsPerBeat * 4.0f;
    float relX = std::max(0.0f, (x - centerX) + m_timelineScrollX);
    int bar = (barWidth > 0.0f) ? static_cast<int>(relX / barWidth) : 0;
    float ratio = (barWidth > 0.0f) ? (std::fmod(relX, barWidth) / barWidth) : 0.0f;

    // 曲末より後ろの小節は、パートの拍子（既定 4/4）で仮想的に並べる
    int defaultBarTicks = TICKS_PER_BAR_4_4;
    if (pActive && pActive->timeNumerator > 0 && pActive->timeDenominator > 0) {
        defaultBarTicks = pActive->timeNumerator * (TICKS_PER_BEAT * 4 / pActive->timeDenominator);
    }
    auto barTicksOf = [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(bars.size())) {
            int t = bars[static_cast<size_t>(idx)].GetTotalTicks();
            return t > 0 ? t : TICKS_PER_BAR_4_4;
        }
        return defaultBarTicks;
    };

    int grid = std::max(1, gridTicks);
    int barTicks = barTicksOf(bar);
    int snapped = static_cast<int>(std::round(ratio * static_cast<float>(barTicks) / static_cast<float>(grid))) * grid;
    // 小節末尾へスナップした場合は次小節の先頭として扱う（小節内の上限で引き戻さない）
    while (snapped >= barTicks) {
        snapped -= barTicks;
        ++bar;
        barTicks = barTicksOf(bar);
    }

    int size = static_cast<int>(bars.size());
    int global = TimelineEdit::BarStartTick(bars, std::min(bar, size));
    if (bar > size) global += (bar - size) * defaultBarTicks;
    global += snapped;

    if (outBar) *outBar = bar;
    if (outTickInBar) *outTickInBar = snapped;
    if (outBarTicks) *outBarTicks = barTicks;
    return global;
}

void AppWindow::EnsureBarsCoverTick(std::vector<Bar>& bars, int endTick) const {
    const Part* pActive = m_partManager.GetActivePart();
    int numerator = (pActive && pActive->timeNumerator > 0) ? pActive->timeNumerator : 4;
    int denominator = (pActive && pActive->timeDenominator > 0) ? pActive->timeDenominator : 4;

    while (TimelineEdit::SongTicks(bars) < endTick) {
        Bar newBar;
        newBar.barNumber = static_cast<int>(bars.size()) + 1;
        newBar.timeNumerator = numerator;
        newBar.timeDenominator = denominator;
        ChordToken fullRest;
        fullRest.isRest = true;
        fullRest.quality = ChordQuality::Rest;
        fullRest.rootName = "R";
        fullRest.degreeRoman = "R";
        fullRest.rawText = "_";
        fullRest.SetTicks(newBar.GetTotalTicks());
        newBar.chords.push_back(fullRest);
        bars.push_back(std::move(newBar));
    }
}

void AppWindow::MoveChordToTimelineX(int srcBar, int srcChord, float x) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || srcBar < 0 || srcBar >= static_cast<int>(pActive->bars.size())) return;
    if (srcChord < 0 || srcChord >= static_cast<int>(pActive->bars[srcBar].chords.size())) return;

    // 絶対tickで移動する。小節線をまたいでもカードは1枚のまま保持され、
    // 曲末を越える位置へは休符小節を足して収める（位置の引き戻しをしない）
    int gridTicks = std::max(1, m_timelineGridTicks);
    int newStart = TimelineTickFromX(x, gridTicks);

    int srcStart = TimelineEdit::BarStartTick(pActive->bars, srcBar);
    for (int i = 0; i < srcChord; ++i) {
        const auto& c = pActive->bars[srcBar].chords[i];
        srcStart += (c.durationTicks > 0) ? c.durationTicks : std::max(1, c.durationBeats) * TICKS_PER_BEAT;
    }
    if (newStart == srcStart) return; // 位置が変わらなければ何もしない

    const auto& srcC = pActive->bars[srcBar].chords[srcChord];
    int len = (srcC.durationTicks > 0) ? srcC.durationTicks : std::max(1, srcC.durationBeats) * TICKS_PER_BEAT;

    std::vector<Bar> work = pActive->bars;
    EnsureBarsCoverTick(work, newStart + len);
    auto res = TimelineEdit::MoveChordSpan(work, srcStart, newStart, gridTicks);
    if (res.selectedBar < 0) return;

    PushUndo();
    pActive->bars = std::move(res.bars);
    m_selectedBarIndex = res.selectedBar;
    m_selectedChordIndex = res.selectedChord;
    m_selAnchorTick = PrimaryChordStartTick();
    EnsureVisible(m_selectedBarIndex);
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::SyncTextToTimeline() {
    if (!m_hwndEdit) return;

    int len = GetWindowTextLengthW(m_hwndEdit);
    std::wstring text(len, 0);
    GetWindowTextW(m_hwndEdit, text.data(), len + 1);

    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;
    if (m_isMergedView) return; // 全体表示中は編集対象のパートがない

    // 空入力（空白のみを含む）は何もしない（既存の進行を消さない）
    std::string utf8 = ToUtf8String(text);
    if (utf8.find_first_not_of(" \t\r\n|") == std::string::npos) return;

    auto parsed = ProgressionParser::Parse(utf8, pActive->keyRoot, pActive->mode);
    if (parsed.empty()) return;

    // テキストはタイムラインと同期しない入力専用欄。入力内容を末尾へ追記し、反映後に欄を空にする
    PushUndo();
    for (auto& bar : parsed) {
        pActive->bars.push_back(std::move(bar));
    }
    ClearSelection();
    SyncTimelineToText();
    SetWindowTextW(m_hwndEdit, L"");

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// テキスト欄へは書き戻さない（入力専用）。パートの保存用テキストのみ更新する。
void AppWindow::SyncTimelineToText() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    PartManager::RenumberBars(pActive->bars); // 各パートは常に1小節目から始まる連番
    PartManager::MigrateBarMarkers(*pActive); // [名前] 記法由来の小節マーカーを自由位置マーカーへ変換し、範囲外を丸める
    pActive->rawProgressionText = ProgressionParser::Format(pActive->bars);
}

void AppWindow::PushUndo() {
    UndoSnapshot snap;
    snap.parts = m_partManager.GetParts();
    snap.activePartIndex = m_partManager.GetActiveIndex();
    m_undoStack.push_back(std::move(snap));
    m_multiSelTicks.clear(); // 編集で tick 位置が変わるため複数選択は解除する
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
    m_multiSelTicks.clear();
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
    m_multiSelTicks.clear();
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
    // 小節線を越えても1枚のまま、元コードの直後（絶対tick）へ置く
    PushUndo();
    InsertChordAtTimelineTick(copy, m_selectedBarIndex, insertTick, false);
}

void AppWindow::DeleteSelection() {
    Part* pActive = m_partManager.GetActivePart();
    if (pActive && !m_isMergedView && !m_multiSelTicks.empty()) {
        // 複数選択中は選択コードをまとめて取り除く（跡は休符になり曲長は変わらない）
        std::vector<int> ticks = m_multiSelTicks;
        PushUndo();
        pActive->bars = TimelineEdit::RemoveChords(pActive->bars, ticks);
        ClearSelection();
        SyncTimelineToText();
        return;
    }
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

namespace {

int ClipDur(const ChordToken& c) {
    if (c.durationTicks > 0) return c.durationTicks;
    return std::max(1, c.durationBeats) * TICKS_PER_BEAT;
}

// 絶対tick → (小節index, 小節内コードindex)。見つからなければ false。
bool TickToChordIndex(const std::vector<Bar>& bars, int tick, int& outBar, int& outChord) {
    int barStart = 0;
    for (size_t b = 0; b < bars.size(); ++b) {
        int bt = bars[b].GetTotalTicks();
        if (bt <= 0) bt = TICKS_PER_BAR_4_4;
        int local = 0;
        for (size_t ci = 0; ci < bars[b].chords.size(); ++ci) {
            if (barStart + local == tick && !bars[b].chords[ci].isRest) {
                outBar = static_cast<int>(b);
                outChord = static_cast<int>(ci);
                return true;
            }
            local += ClipDur(bars[b].chords[ci]);
        }
        barStart += bt;
    }
    return false;
}

} // namespace

void AppWindow::RefreshPartDegrees(Part& part) {
    PitchClass keyP = MusicEngine::NoteToPitch(part.keyRoot);
    if (keyP == PitchClass::Unknown) return;
    for (auto& bar : part.bars) {
        for (auto& chord : bar.chords) {
            if (chord.isRest || chord.rootPitch == PitchClass::Unknown) continue;
            chord.degreeRoman = MusicEngine::CalculateDegree(chord.rootPitch, chord.quality, keyP, part.mode, chord.bassPitch);
        }
    }
}

void AppWindow::ClearSelection() {
    m_multiSelTicks.clear();
    m_selNotePc = -1;
    m_selectedBarIndex = -1;
    m_selectedChordIndex = -1;
    m_selAnchorTick = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

int AppWindow::PrimaryChordStartTick() const {
    const Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView) return -1;
    if (m_selectedBarIndex < 0 || m_selectedBarIndex >= static_cast<int>(pActive->bars.size())) return -1;
    const auto& bar = pActive->bars[m_selectedBarIndex];
    if (m_selectedChordIndex < 0 || m_selectedChordIndex >= static_cast<int>(bar.chords.size())) return -1;
    if (bar.chords[m_selectedChordIndex].isRest) return -1;
    int tick = TimelineEdit::BarStartTick(pActive->bars, m_selectedBarIndex);
    for (int i = 0; i < m_selectedChordIndex; ++i) tick += ClipDur(bar.chords[i]);
    return tick;
}

std::vector<int> AppWindow::GetSelectedStartTicks() const {
    if (!m_multiSelTicks.empty()) return m_multiSelTicks;
    int t = PrimaryChordStartTick();
    if (t >= 0) return { t };
    return {};
}

void AppWindow::ToggleChordSelection(int barIndex, int chordIndex) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView) return;
    if (barIndex < 0 || barIndex >= static_cast<int>(pActive->bars.size())) return;
    const auto& bar = pActive->bars[barIndex];
    if (chordIndex < 0 || chordIndex >= static_cast<int>(bar.chords.size()) || bar.chords[chordIndex].isRest) return;

    int tick = TimelineEdit::BarStartTick(pActive->bars, barIndex);
    for (int i = 0; i < chordIndex; ++i) tick += ClipDur(bar.chords[i]);

    if (m_multiSelTicks.empty()) {
        int primary = PrimaryChordStartTick();
        if (primary >= 0) m_multiSelTicks.push_back(primary);
    }
    auto it = std::find(m_multiSelTicks.begin(), m_multiSelTicks.end(), tick);
    if (it != m_multiSelTicks.end()) {
        // 選択済みカードの再クリック: 選択から外す。主選択は残りの最後のカードへ移す
        m_multiSelTicks.erase(it);
        if (m_multiSelTicks.empty()) {
            ClearSelection();
            return;
        }
        int nb = 0, nc = 0;
        if (TickToChordIndex(pActive->bars, m_multiSelTicks.back(), nb, nc)) {
            m_selectedBarIndex = nb;
            m_selectedChordIndex = nc;
            m_selAnchorTick = m_multiSelTicks.back();
        }
        if (m_multiSelTicks.size() == 1) m_multiSelTicks.clear(); // 1枚だけ残ったら単一選択に戻す
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    m_multiSelTicks.push_back(tick);

    m_selectedBarIndex = barIndex;
    m_selectedChordIndex = chordIndex;
    m_selAnchorTick = tick;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::ExtendSelectionTo(int barIndex, int chordIndex) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView) return;
    if (barIndex < 0 || barIndex >= static_cast<int>(pActive->bars.size())) return;
    const auto& target = pActive->bars[barIndex];
    if (chordIndex < 0 || chordIndex >= static_cast<int>(target.chords.size()) || target.chords[chordIndex].isRest) return;

    int tick = TimelineEdit::BarStartTick(pActive->bars, barIndex);
    for (int i = 0; i < chordIndex; ++i) tick += ClipDur(target.chords[i]);

    int anchor = (m_selAnchorTick >= 0) ? m_selAnchorTick : tick;
    int lo = std::min(anchor, tick);
    int hi = std::max(anchor, tick);

    std::vector<int> previous = m_multiSelTicks;
    std::sort(previous.begin(), previous.end());
    int previousPrimary = PrimaryChordStartTick();

    m_multiSelTicks.clear();
    int barStart = 0;
    for (const auto& bar : pActive->bars) {
        int bt = bar.GetTotalTicks();
        if (bt <= 0) bt = TICKS_PER_BAR_4_4;
        int local = 0;
        for (const auto& c : bar.chords) {
            int s = barStart + local;
            if (!c.isRest && s >= lo && s <= hi) m_multiSelTicks.push_back(s); // 開始順に並ぶ
            local += ClipDur(c);
        }
        barStart += bt;
    }

    // 同じ範囲、または選択済みの単一カードへの再Shift+クリックは、選択を解除する
    bool sameRange = !previous.empty() && previous == m_multiSelTicks;
    bool sameSingle = previous.empty() && m_multiSelTicks.size() < 2 && previousPrimary == tick;
    if (sameRange || sameSingle) {
        ClearSelection();
        return;
    }
    if (m_multiSelTicks.size() < 2) m_multiSelTicks.clear();

    m_selectedBarIndex = barIndex;
    m_selectedChordIndex = chordIndex;
    m_selAnchorTick = anchor;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::SelectAll() {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView || pActive->bars.empty()) return;

    std::vector<int> all;
    int firstBar = -1, firstChord = -1;
    int barStart = 0;
    for (size_t b = 0; b < pActive->bars.size(); ++b) {
        const auto& bar = pActive->bars[b];
        int bt = bar.GetTotalTicks();
        if (bt <= 0) bt = TICKS_PER_BAR_4_4;
        int local = 0;
        for (size_t ci = 0; ci < bar.chords.size(); ++ci) {
            if (!bar.chords[ci].isRest) {
                all.push_back(barStart + local);
                if (firstBar < 0) { firstBar = static_cast<int>(b); firstChord = static_cast<int>(ci); }
            }
            local += ClipDur(bar.chords[ci]);
        }
        barStart += bt;
    }
    if (all.empty()) return;

    SelectChord(firstBar, firstChord);
    if (all.size() > 1) m_multiSelTicks = std::move(all);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::DoEditCopy() {
    Part* pActive = m_partManager.GetActivePart();
    std::vector<int> ticks = GetSelectedStartTicks();
    if (!pActive || ticks.empty()) {
        CopyProgressionToClipboard(); // 選択なし: 従来どおり進行全体をテキストでコピー
        return;
    }
    m_chordClipboard = TimelineEdit::ExtractChords(pActive->bars, ticks);
    m_chordClipSeq = GetClipboardSequenceNumber();
    SetStatusMessage(std::wstring(TR(L"コードをコピーしました: ", L"Copied chords: ")) + std::to_wstring(m_chordClipboard.size()));
}

void AppWindow::DoEditCut() {
    Part* pActive = m_partManager.GetActivePart();
    std::vector<int> ticks = GetSelectedStartTicks();
    if (!pActive || m_isMergedView || ticks.empty()) return;

    m_chordClipboard = TimelineEdit::ExtractChords(pActive->bars, ticks);
    m_chordClipSeq = GetClipboardSequenceNumber();

    PushUndo();
    pActive->bars = TimelineEdit::RemoveChords(pActive->bars, ticks);
    ClearSelection();
    SyncTimelineToText();
    SetStatusMessage(std::wstring(TR(L"コードを切り取りました: ", L"Cut chords: ")) + std::to_wstring(m_chordClipboard.size()));
}

void AppWindow::DoEditPaste() {
    Part* pActive = m_partManager.GetActivePart();
    // アプリ内クリップボードが無効（空、またはコピー後にOS側が更新された）なら、従来のテキスト貼り付け
    if (!pActive || m_isMergedView || m_chordClipboard.empty() || GetClipboardSequenceNumber() != m_chordClipSeq) {
        PasteProgressionFromClipboard();
        return;
    }

    // 貼り付け位置: 再生ヘッド（ルーラー）に最も近いグリッド線。選択の有無には依存しない
    const int grid = std::max(1, m_timelineGridTicks);
    const int atTick = static_cast<int>(std::llround(m_playbackTick / static_cast<double>(grid))) * grid;

    // 貼り付け内容の末端まで曲が足りなければ、休符小節を足して内容を失わないようにする
    int extent = 0;
    for (const auto& it : m_chordClipboard) extent = std::max(extent, it.startTick + ClipDur(it.chord));
    std::vector<Bar> work = pActive->bars;
    EnsureBarsCoverTick(work, atTick + extent);

    PushUndo();
    pActive->bars = TimelineEdit::InsertRange(work, m_chordClipboard, atTick);

    // ステータス表示用に、貼り付け位置を「小節・拍」で求める
    int posBar = 1;
    int posBeat = 1;
    {
        int barStart = 0;
        for (size_t b = 0; b < pActive->bars.size(); ++b) {
            int bt = pActive->bars[b].GetTotalTicks();
            if (bt <= 0) bt = TICKS_PER_BAR_4_4;
            if (atTick < barStart + bt) {
                posBar = static_cast<int>(b) + 1;
                posBeat = (atTick - barStart) / TICKS_PER_BEAT + 1;
                break;
            }
            barStart += bt;
        }
    }

    // 貼り付けたコードを選択状態にする
    std::vector<int> pasted;
    for (const auto& it : m_chordClipboard) {
        int s = atTick + it.startTick;
        int b = 0, c = 0;
        if (TickToChordIndex(pActive->bars, s, b, c)) pasted.push_back(s);
    }
    if (!pasted.empty()) {
        int b = 0, c = 0;
        if (TickToChordIndex(pActive->bars, pasted.front(), b, c)) {
            m_selectedBarIndex = b;
            m_selectedChordIndex = c;
            m_selAnchorTick = pasted.front();
            EnsureVisible(b);
        }
        if (pasted.size() > 1) m_multiSelTicks = std::move(pasted);
    }
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
    wchar_t posText[64];
    swprintf_s(posText, TR(L" (小節%d・%d拍)", L" (bar %d, beat %d)"), posBar, posBeat);
    SetStatusMessage(std::wstring(TR(L"コードを貼り付けました: ", L"Pasted chords: ")) + std::to_wstring(m_chordClipboard.size()) + posText);
}

// ---------------------------------------------------------------------------
// コード提案の基準コード（直前パートの最終コード → なければ表示中パート自身の最終コード）
// 選択コードには依存せず、パート間のキーの違い（転調）のつながりを支援する。
// ---------------------------------------------------------------------------
bool AppWindow::FindSuggestionBase(ChordToken& outChord, const Part*& outBasePart, const Part*& outTargetPart) const {
    outBasePart = nullptr;
    outTargetPart = nullptr;
    const auto& parts = m_partManager.GetParts();
    const Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return false;

    auto lastChordOf = [](const Part& p, ChordToken& out) {
        for (auto bi = p.bars.rbegin(); bi != p.bars.rend(); ++bi) {
            for (auto ci = bi->chords.rbegin(); ci != bi->chords.rend(); ++ci) {
                if (!ci->isRest && !ci->rawText.empty()) {
                    out = *ci;
                    return true;
                }
            }
        }
        return false;
    };

    const int activeIndex = m_partManager.GetActiveIndex();

    // 同じキー・モードが続く範囲の先頭パート。範囲内の全パートで基準を共通にし、
    // 同キーのパートは候補・借用・繋ぎが同じ内容になるようにする（マージ後も一貫した区間になる）
    const int runStart = m_partManager.FindKeyRunStart(activeIndex);

    // 1. 範囲の先頭より前（キーが違う直前パート。空パートはさかのぼる）の最終コードからの橋渡し。接続先は表示中パート
    for (int i = runStart - 1; i >= 0 && i < static_cast<int>(parts.size()); --i) {
        if (lastChordOf(parts[static_cast<size_t>(i)], outChord)) {
            outBasePart = &parts[static_cast<size_t>(i)];
            outTargetPart = pActive;
            return true;
        }
    }

    // 2. 範囲より前に基準がない（先頭パートの範囲など）: 範囲内で最初に最終コードを持つパートの最終コード。接続先なし
    for (int i = std::max(0, runStart); i <= activeIndex && i < static_cast<int>(parts.size()); ++i) {
        if (lastChordOf(parts[static_cast<size_t>(i)], outChord)) {
            outBasePart = &parts[static_cast<size_t>(i)];
            return true;
        }
    }
    return false;
}

std::vector<ChordToken> AppWindow::BuildSuggestionList(bool use7th) const {
    const Part* pActive = m_partManager.GetActivePart();
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;

    ChordToken base;
    const Part* basePart = nullptr;
    const Part* targetPart = nullptr;
    if (!FindSuggestionBase(base, basePart, targetPart)) {
        return MusicEngine::GetSuggestions({}, keyRoot, mode, use7th);
    }
    if (!targetPart) {
        // 接続先なし（直前パートに基準がない先頭パートなど）: 自身の最終コードの続きを提案する
        return MusicEngine::GetSuggestions({ base }, basePart->keyRoot, basePart->mode, use7th);
    }
    // 同キーなら通常の続き、異キーなら接続先キーへ向かうピボットとVを優先
    return MusicEngine::GetBridgeSuggestions(base, basePart->keyRoot, basePart->mode,
                                             targetPart->keyRoot, targetPart->mode, use7th);
}

std::vector<ChordToken> AppWindow::BuildBorrowedList(bool use7th) const {
    const Part* pActive = m_partManager.GetActivePart();
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;
    auto borrowed = MusicEngine::GetModalInterchangeChords(keyRoot, mode, use7th);

    ChordToken base;
    const Part* basePart = nullptr;
    const Part* targetPart = nullptr;
    if (!FindSuggestionBase(base, basePart, targetPart)) return borrowed;

    borrowed = MusicEngine::OrderBorrowedByContext(std::move(borrowed), base);
    if (!targetPart) return borrowed;

    // 接続先が異キーなら、転調先へ向かう借用候補（V7・副ドミナント・IV）を先頭に並べる
    auto modulation = MusicEngine::GetModulationBorrowed(basePart->keyRoot, basePart->mode,
                                                         targetPart->keyRoot, targetPart->mode, use7th);
    std::vector<ChordToken> merged;
    auto addUnique = [&](const ChordToken& c) {
        for (const auto& m : merged) {
            if (m.rootPitch == c.rootPitch && m.quality == c.quality) return;
        }
        merged.push_back(c);
    };
    for (const auto& c : modulation) addUnique(c);
    for (const auto& c : borrowed) addUnique(c);
    return merged;
}

std::vector<ChordToken> AppWindow::BuildSecondaryList() const {
    // 繋ぎコードは表示中パート（接続先）のキーで生成する。同キーなら先頭パートと同じ内容になる
    const Part* pActive = m_partManager.GetActivePart();
    std::string keyRoot = pActive ? pActive->keyRoot : "C";
    MusicalMode mode = pActive ? pActive->mode : MusicalMode::Major;
    return MusicEngine::GetSecondaryDominants(keyRoot, mode);
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

// 先頭へ戻る。再生中は先頭から再生を継続、停止中は再生位置のみ先頭へ
void AppWindow::RewindPlayback() {
    m_playbackTick = 0.0;
    m_lastSoundedChordUid = -1;
    m_synth.AllNotesOff();
    if (m_isPlaying) {
        m_lastPlaybackTime = std::chrono::steady_clock::now();
    } else {
        m_selectedBarIndex = 0;
        m_selectedChordIndex = -1;
        EnsureVisible(0);
    }
    SetStatusMessage(L"先頭へ戻りました");
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
    std::wstring title = L"Chord Launch " CHORDLAUNCH_VERSION_WSTR;
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
// ファイル操作・MIDIドラッグ
// ---------------------------------------------------------------------------
bool AppWindow::ConfirmDiscardChanges() {
    if (!m_isDirty) return true;

    int ret = MessageBoxW(
        m_hwnd,
        TR(L"保存されていない変更があります。保存しますか？", L"There are unsaved changes. Save them?"),
        TR(L"未保存の変更", L"Unsaved Changes"),
        MB_YESNOCANCEL | MB_ICONQUESTION
    );
    if (ret == IDCANCEL) return false;
    if (ret == IDYES) {
        DoFileSave();
        // 保存に失敗、または名前を付けて保存をキャンセルした場合は続行しない
        return !m_isDirty;
    }
    return true;
}

bool AppWindow::CreateProjectFromDashboard() {
    std::wstring name = m_dashNewName;
    size_t lead = 0;
    while (lead < name.size() && name[lead] == L' ') ++lead;
    name.erase(0, lead);
    while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();

    if (name.empty()) {
        MessageBoxW(m_hwnd, TR(L"プロジェクト名を入力してください。", L"Enter a project name."), TR(L"新規プロジェクト", L"New Project"), MB_OK | MB_ICONWARNING);
        return false;
    }
    if (m_projectDirectory.empty()) {
        MessageBoxW(m_hwnd, TR(L"保存先フォルダを指定してください。", L"Choose a save folder."), TR(L"新規プロジェクト", L"New Project"), MB_OK | MB_ICONWARNING);
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(m_projectDirectory, ec);
    if (ec || !std::filesystem::is_directory(m_projectDirectory)) {
        MessageBoxW(m_hwnd, TR(L"保存先フォルダを作成できません。", L"Cannot create the save folder."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    std::filesystem::path target = std::filesystem::path(m_projectDirectory) / (name + L".chord");
    if (std::filesystem::exists(target)) {
        int ret = MessageBoxW(m_hwnd, TR(L"同名のファイルが既に存在します。上書きしますか？", L"A file with the same name already exists. Overwrite?"),
                              TR(L"新規プロジェクト", L"New Project"), MB_YESNO | MB_ICONQUESTION);
        if (ret != IDYES) return false;
    }

    if (!DoFileNew()) return false;

    m_currentProjectPath = target.wstring();
    m_projectTitle = ToUtf8String(name);
    if (m_partManager.SaveToFile(
        m_currentProjectPath, m_projectTitle,
        m_selectedVoicing, m_bassOctaveMode, m_timelineGridTicks, m_inputGridTicks
    )) {
        m_isDirty = false;
        UpdateWindowTitle();
        AddRecentFile(m_currentProjectPath);
        SetStatusMessage(TR(L"新規プロジェクトを作成しました", L"New project created"));
        return true;
    }
    m_currentProjectPath.clear();
    UpdateWindowTitle();
    MessageBoxW(m_hwnd, TR(L"プロジェクトの保存に失敗しました。", L"Failed to save project."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
    return false;
}

bool AppWindow::DoFileNew() {
    if (!ConfirmDiscardChanges()) return false;
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
    return true;
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
    wcsncpy_s(szFile, MAX_PATH, initName.c_str(), _TRUNCATE);

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
        if (!ConfirmDiscardChanges()) return;

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
            // 別プロジェクトの編集履歴が混入しないよう Undo/Redo を破棄
            m_undoStack.clear();
            m_redoStack.clear();

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
        const std::vector<PartMarker> markers = CurrentViewMarkers();
        auto bytes = MidiBuilder::BuildMidi(bars, bpm, m_selectedVoicing, m_baseOctave, 90, m_bassOctaveMode, &markers);
        if (!bytes.empty()) {
            HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            bool saved = false;
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                BOOL wrote = WriteFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
                CloseHandle(hFile);
                saved = (wrote != FALSE) && (written == static_cast<DWORD>(bytes.size()));
            }
            if (saved) {
                SetStatusMessage(TR(L"MIDIファイルを保存しました", L"MIDI file saved"));
            } else {
                MessageBoxW(m_hwnd, TR(L"MIDIファイルの保存に失敗しました。", L"Failed to save MIDI file."), TR(L"エラー", L"Error"), MB_OK | MB_ICONERROR);
            }
        }
    }
}

void AppWindow::StartDawDrag() {
    Part* pActive = m_partManager.GetActivePart();
    std::vector<Bar> bars = m_isMergedView ? m_partManager.GenerateMergedProgression() : (pActive ? pActive->bars : std::vector<Bar>{});
    if (bars.empty()) return;

    int bpm = pActive ? pActive->bpm : 120;
    const std::vector<PartMarker> markers = CurrentViewMarkers();
    std::wstring tempMidiPath = MidiBuilder::ExportToTempFile(bars, bpm, m_selectedVoicing, L"ChordLaunch_Progression", m_bassOctaveMode, &markers);
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
            bool owned = false; // SetClipboardData 成功後は所有権がOSへ移る
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, wtext.c_str(), bytes);
                GlobalUnlock(hMem);
                owned = (SetClipboardData(CF_UNICODETEXT, hMem) != nullptr);
            }
            if (!owned) {
                GlobalFree(hMem);
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

    if (!ConfirmDiscardChanges()) return;

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
        // 別プロジェクトの編集履歴が混入しないよう Undo/Redo を破棄
        m_undoStack.clear();
        m_redoStack.clear();

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
            // 配列の終端 ']' は、引用符内（パス中の ']' など）を除外して探す
            size_t arrEnd = std::string::npos;
            if (arrStart != std::string::npos) {
                bool inStr = false;
                for (size_t k = arrStart + 1; k < content.size(); ++k) {
                    char ch = content[k];
                    if (inStr) {
                        if (ch == '\\') { ++k; continue; }
                        if (ch == '"') inStr = false;
                    } else if (ch == '"') {
                        inStr = true;
                    } else if (ch == ']') {
                        arrEnd = k;
                        break;
                    }
                }
            }
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
    newPart.markers = m_partManager.GenerateMergedMarkers(); // 各パート名は mergedBars の sectionMarker から変換される

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
    m_renameMarkerTick = -1;
    m_renamePartName = ToWString(parts[partIndex].name);
    OpenModal(ModalType::RenamePart);
}

std::vector<PartMarker> AppWindow::CurrentViewMarkers() const {
    if (m_isMergedView) return m_partManager.GenerateMergedMarkers();
    const Part* pActive = m_partManager.GetActivePart();
    return pActive ? pActive->markers : std::vector<PartMarker>{};
}

int AppWindow::MarkerHitToleranceTicks() const {
    float barWidth = m_pixelsPerBeat * 4.0f;
    if (barWidth <= 0.0f) return TICKS_PER_BEAT / 4;
    // 約 8px 以内を「既存マーカーを指している」とみなす
    return std::max(1, static_cast<int>(std::round(8.0f / barWidth * static_cast<float>(TICKS_PER_BAR_4_4))));
}

int AppWindow::HitTestRulerTick(float x) const {
    const Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView) return -1;
    float relX = (x - m_leftPaneWidth) + m_timelineScrollX;
    float barWidth = m_pixelsPerBeat * 4.0f;
    if (relX < 0.0f || barWidth <= 0.0f) return -1;
    double rawTick = (static_cast<double>(relX) / barWidth) * TICKS_PER_BAR_4_4;
    if (rawTick > static_cast<double>(PartManager::PartLengthTicks(*pActive))) return -1;
    return static_cast<int>(std::llround(rawTick));
}

void AppWindow::OpenMarkerModal(int tick) {
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive || m_isMergedView || tick < 0) return;

    // パート先頭（tick 0 付近）のマーカーはパートタブ名そのものなので、パート名の変更に振り替える
    const int tol = MarkerHitToleranceTicks();
    if (tick <= tol && m_partManager.FindMarkerNear(m_partManager.GetActiveIndex(), tick, tol) < 0) {
        OpenRenamePartModal(m_partManager.GetActiveIndex());
        return;
    }

    // 既存マーカーの近くなら編集、なければ（グリッドへスナップした）その位置に新規作成
    int existing = m_partManager.FindMarkerNear(m_partManager.GetActiveIndex(), tick, tol);
    std::string initial;
    if (existing >= 0) {
        tick = existing;
        for (const auto& m : pActive->markers) {
            if (m.tick == existing) initial = m.name;
        }
    } else {
        int grid = std::max(1, m_timelineGridTicks);
        tick = ((tick + grid / 2) / grid) * grid;
        if (tick <= 0) tick = grid;
    }

    m_renamePartIndex = m_partManager.GetActiveIndex();
    m_renameMarkerTick = tick;
    m_renamePartName = ToWString(initial);
    OpenModal(ModalType::RenamePart);
}

void AppWindow::AddMarkerAtPlayhead() {
    if (m_isMergedView) {
        SetStatusMessage(TR(L"マーカーはパートごとに追加します（全体表示では追加できません）", L"Add markers per part (not available in merged view)"));
        return;
    }
    Part* pActive = m_partManager.GetActivePart();
    if (!pActive) return;

    int grid = std::max(1, m_timelineGridTicks);
    int tick = static_cast<int>(std::llround(m_playbackTick / grid)) * grid;
    if (tick <= 0) {
        SetStatusMessage(TR(L"再生ヘッドをパート先頭以外へ移動してからマーカーを追加してください（先頭はパート名）",
                            L"Move the playhead past the part start to add a marker (the start is the part name)"));
        return;
    }
    OpenMarkerModal(tick);
}

void AppWindow::ClearMarker(int tick) {
    if (m_isMergedView) return;
    int idx = m_partManager.GetActiveIndex();
    if (m_partManager.FindMarkerNear(idx, tick, 0) < 0) return;

    PushUndo();
    m_partManager.RemoveMarker(idx, tick);
    SetStatusMessage(TR(L"マーカーを削除しました", L"Marker removed"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::CommitRenameModal() {
    auto& parts = m_partManager.GetParts();
    if (m_renamePartIndex < 0 || m_renamePartIndex >= static_cast<int>(parts.size())) return;

    if (m_renameMarkerTick > 0) {
        // 空文字はマーカー解除（新規で空なら何もしない）
        const std::string name = ToUtf8String(m_renamePartName);
        const bool existed = m_partManager.FindMarkerNear(m_renamePartIndex, m_renameMarkerTick, 0) >= 0;
        if (name.empty() && !existed) return;
        PushUndo();
        if (!m_partManager.SetMarker(m_renamePartIndex, m_renameMarkerTick, name)) {
            SetStatusMessage(TR(L"このマーカー名は使用できません（\"Key:\" 始まりなど）", L"This marker name is not allowed (e.g. starting with \"Key:\")"));
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (m_renamePartName.empty()) return;
    PushUndo();
    parts[static_cast<size_t>(m_renamePartIndex)].name = ToUtf8String(m_renamePartName);
    SyncTimelineToText();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::OpenDashboardModal() {
    m_dashboardSelectedRecentIndex = -1;
    m_dashNameFocused = false;
    m_dashNewName = L"MyProgression";
    OpenModal(ModalType::Dashboard);
}

} // namespace clp
