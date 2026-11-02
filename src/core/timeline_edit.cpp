#include "timeline_edit.hpp"
#include <algorithm>

namespace clp {

namespace {

struct Item {
    int start = 0;
    ChordToken chord;
};

int DurOf(const ChordToken& c) {
    if (c.durationTicks > 0) return c.durationTicks;
    if (c.durationBeats > 0) return c.durationBeats * TICKS_PER_BEAT;
    return TICKS_PER_BEAT;
}

int BarTicksOf(const Bar& bar) {
    int ticks = bar.GetTotalTicks();
    return ticks > 0 ? ticks : TICKS_PER_BAR_4_4;
}

ChordToken MakeRest(int ticks) {
    ChordToken rest;
    rest.isRest = true;
    rest.quality = ChordQuality::Rest;
    rest.rootName = "R";
    rest.degreeRoman = "R";
    rest.rawText = "_";
    rest.SetTicks(ticks);
    return rest;
}

std::vector<Item> PlacementsOf(const Bar& bar) {
    std::vector<Item> items;
    int tick = 0;
    for (const auto& c : bar.chords) {
        int d = DurOf(c);
        if (d <= 0) continue;
        items.push_back(Item{tick, c});
        tick += d;
    }
    return items;
}

std::vector<Item> MergeAndSort(std::vector<Item> items) {
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.start < b.start;
    });
    return items;
}

// 配置リストを小節のコード列へ書き戻す（隙間は休符で埋め、連続休符は結合）。
void WriteItems(Bar& bar, std::vector<Item> items) {
    int barTicks = BarTicksOf(bar);
    std::vector<ChordToken> out;
    int walker = 0;
    for (auto& it : items) {
        int d = DurOf(it.chord);
        if (d <= 0) continue;
        if (it.start > walker) {
            out.push_back(MakeRest(it.start - walker));
            walker = it.start;
        } else if (it.start < walker) {
            int trim = walker - it.start;
            if (trim >= d) continue;
            it.chord.SetTicks(d - trim);
            d = DurOf(it.chord);
        }
        if (!out.empty() && out.back().isRest && it.chord.isRest) {
            out.back().SetTicks(DurOf(out.back()) + d);
        } else {
            out.push_back(it.chord);
        }
        walker += d;
    }
    if (walker < barTicks) {
        if (!out.empty() && out.back().isRest) {
            out.back().SetTicks(DurOf(out.back()) + (barTicks - walker));
        } else {
            out.push_back(MakeRest(barTicks - walker));
        }
    }
    bar.chords = std::move(out);
}

// 挿入で押し出された分を、挿入位置より後ろの休符から吸収する。
// 休符を詰めた分だけ後続を前へ詰めるため、小節内の総tickは変化しない。
void AbsorbByRests(std::vector<Item>& items, int barTicks, int insertStart) {
    int totalEnd = 0;
    for (const auto& it : items) {
        totalEnd = std::max(totalEnd, it.start + DurOf(it.chord));
    }
    int deficit = totalEnd - barTicks;
    if (deficit <= 0) return;

    for (int i = static_cast<int>(items.size()) - 1; i >= 0 && deficit > 0; --i) {
        if (!items[i].chord.isRest) continue;
        if (items[i].start < insertStart) continue;
        int d = DurOf(items[i].chord);
        int take = std::min(d, deficit);
        for (size_t j = static_cast<size_t>(i) + 1; j < items.size(); ++j) {
            items[j].start -= take;
        }
        if (take >= d) {
            items.erase(items.begin() + i);
        } else {
            items[i].chord.SetTicks(d - take);
        }
        deficit -= take;
    }
}

} // namespace

int TimelineEdit::TotalTicks(const Bar& bar) {
    int total = 0;
    for (const auto& c : bar.chords) total += DurOf(c);
    return total;
}

int TimelineEdit::ChordIndexAtTick(const Bar& bar, int tick) {
    int walker = 0;
    for (size_t i = 0; i < bar.chords.size(); ++i) {
        int d = DurOf(bar.chords[i]);
        if (d <= 0) continue;
        if (tick >= walker && tick < walker + d) return static_cast<int>(i);
        walker += d;
    }
    return -1;
}

void TimelineEdit::NormalizeBar(Bar& bar, std::vector<OverflowItem>* overflow, bool allowTailSpill) {
    const int barTicks = BarTicksOf(bar);

    std::vector<Item> items = PlacementsOf(bar);
    std::vector<Item> kept;
    const int lastIndex = static_cast<int>(items.size()) - 1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        Item& it = items[static_cast<size_t>(i)];
        int d = DurOf(it.chord);
        if (it.start >= barTicks) {
            if (overflow) {
                it.start -= barTicks;
                overflow->push_back(OverflowItem{it.start, it.chord});
            }
            continue;
        }
        if (it.start + d > barTicks) {
            // 小節跨ぎカード（末尾コードの小節線越え）はそのまま保持する
            if (allowTailSpill && i == lastIndex && !it.chord.isRest) {
                kept.push_back(it);
                continue;
            }
            int inBar = barTicks - it.start;
            int over = d - inBar;
            if (overflow && over > 0) {
                ChordToken overChord = it.chord;
                overChord.SetTicks(over);
                overflow->push_back(OverflowItem{0, overChord});
            }
            it.chord.SetTicks(inBar);
        }
        kept.push_back(it);
    }
    WriteItems(bar, std::move(kept));
}

int TimelineEdit::SongTicks(const std::vector<Bar>& bars) {
    int total = 0;
    for (const auto& b : bars) total += BarTicksOf(b);
    return total;
}

int TimelineEdit::BarStartTick(const std::vector<Bar>& bars, int barIndex) {
    int tick = 0;
    for (int i = 0; i < barIndex && i < static_cast<int>(bars.size()); ++i) {
        tick += BarTicksOf(bars[static_cast<size_t>(i)]);
    }
    return tick;
}

int TimelineEdit::IncomingCoverUntil(const std::vector<Bar>& bars, int barIndex) {
    if (barIndex <= 0 || barIndex >= static_cast<int>(bars.size())) return 0;

    const int barStart = BarStartTick(bars, barIndex);
    int covered = 0;
    int start = 0;
    for (int k = 0; k < barIndex; ++k) {
        int local = 0;
        for (const auto& c : bars[static_cast<size_t>(k)].chords) {
            int d = DurOf(c);
            int gs = start + local;
            if (!c.isRest && d > 0 && gs < barStart && gs + d > barStart) {
                covered = std::max(covered, gs + d - barStart);
            }
            local += d;
        }
        start += BarTicksOf(bars[static_cast<size_t>(k)]);
    }
    return std::clamp(covered, 0, BarTicksOf(bars[static_cast<size_t>(barIndex)]));
}

TimelineEdit::SpanResult TimelineEdit::ResizeChordSpan(const std::vector<Bar>& baseBars,
                                                       int targetStartTick,
                                                       int newStartTick,
                                                       int newEndTick,
                                                       int minTicks) {
    SpanResult res;
    res.bars = baseBars;
    if (baseBars.empty()) return res;

    // 1. 全コードを絶対tickの配置として収集（休符は隙間として再生成する）
    std::vector<Item> placements;
    int barStart = 0;
    for (const auto& bar : baseBars) {
        int local = 0;
        for (const auto& c : bar.chords) {
            int d = DurOf(c);
            if (d <= 0) continue;
            if (!c.isRest) placements.push_back(Item{barStart + local, c});
            local += d;
        }
        barStart += BarTicksOf(bar);
    }
    const int songTicks = barStart;

    int targetIdx = -1;
    for (size_t i = 0; i < placements.size(); ++i) {
        if (placements[i].start == targetStartTick) {
            targetIdx = static_cast<int>(i);
            break;
        }
    }
    if (targetIdx < 0) return res;

    int oldLen = DurOf(placements[static_cast<size_t>(targetIdx)].chord);
    int guard = std::min(std::max(1, minTicks), std::max(1, oldLen));
    newStartTick = std::clamp(newStartTick, 0, std::max(0, songTicks - guard));
    newEndTick = std::clamp(newEndTick, newStartTick + guard, songTicks);

    // 2. 対象コードの新しい区間と重なる他コードを切り詰める（曲長は不変）
    std::vector<Item> out;
    out.reserve(placements.size() + 1);
    for (int i = 0; i < static_cast<int>(placements.size()); ++i) {
        if (i == targetIdx) continue;
        const Item& p = placements[static_cast<size_t>(i)];
        int ps = p.start;
        int pe = ps + DurOf(p.chord);
        if (pe <= newStartTick || ps >= newEndTick) {
            out.push_back(p);
            continue;
        }
        if (ps < newStartTick) {
            ChordToken left = p.chord;
            left.SetTicks(newStartTick - ps);
            out.push_back(Item{ps, left});
        }
        if (pe > newEndTick) {
            ChordToken right = p.chord;
            right.SetTicks(pe - newEndTick);
            out.push_back(Item{newEndTick, right});
        }
    }
    ChordToken targetChord = placements[static_cast<size_t>(targetIdx)].chord;
    targetChord.SetTicks(newEndTick - newStartTick);
    out.push_back(Item{newStartTick, targetChord});
    std::stable_sort(out.begin(), out.end(), [](const Item& a, const Item& b) {
        return a.start < b.start;
    });

    // 3. 小節ごとに再構築（小節の開始位置は拍子ぶん固定。跨ぎカードは開始小節に保持）
    res.bars.clear();
    res.bars.reserve(baseBars.size());
    size_t pi = 0;
    int coveredUntil = 0;
    barStart = 0;
    for (const auto& baseBar : baseBars) {
        Bar bar = baseBar;
        int barTicks = BarTicksOf(bar);
        int b0 = barStart;
        int b1 = barStart + barTicks;

        std::vector<Item> local;
        while (pi < out.size() && out[pi].start < b0) ++pi; // 前小節開始分は前小節に格納済み
        while (pi < out.size() && out[pi].start < b1) {
            Item p = out[pi];
            int start = std::max(p.start, coveredUntil); // 跨ぎカードに覆われた区間は切り詰め
            int end = p.start + DurOf(p.chord);
            if (start >= b1) break;
            if (end > start) {
                ChordToken c = p.chord;
                c.SetTicks(end - start);
                local.push_back(Item{start - b0, c});
                coveredUntil = std::max(coveredUntil, end);
            }
            ++pi;
        }
        WriteItems(bar, std::move(local));
        res.bars.push_back(std::move(bar));
        barStart = b1;
    }

    // 4. 対象コードの新しい位置を選択状態として返す
    barStart = 0;
    for (size_t i = 0; i < res.bars.size(); ++i) {
        int bt = BarTicksOf(res.bars[i]);
        if (newStartTick >= barStart && newStartTick < barStart + bt) {
            res.selectedBar = static_cast<int>(i);
            res.selectedChord = ChordIndexAtTick(res.bars[i], newStartTick - barStart);
            break;
        }
        barStart += bt;
    }
    return res;
}

std::vector<TimelineEdit::OverflowItem> TimelineEdit::InsertChord(Bar& bar, const ChordToken& chord, int startTick) {
    std::vector<OverflowItem> overflow;
    NormalizeBar(bar, &overflow);

    const int barTicks = BarTicksOf(bar);
    int chordTicks = std::max(1, DurOf(chord));
    startTick = std::clamp(startTick, 0, std::max(0, barTicks - 1));

    if (chord.isRest) {
        return overflow;
    }

    ChordToken newChord = chord;
    newChord.SetTicks(chordTicks);
    const int newStart = startTick;
    const int newEnd = startTick + chordTicks;

    std::vector<Item> placed = PlacementsOf(bar);
    bool hasCollision = false;
    for (const auto& p : placed) {
        if (p.chord.isRest) continue;
        int ps = p.start;
        int pe = ps + DurOf(p.chord);
        if (std::max(ps, newStart) < std::min(pe, newEnd)) {
            hasCollision = true;
            break;
        }
    }

    std::vector<Item> resolved;
    if (!hasCollision) {
        for (const auto& p : placed) {
            int ps = p.start;
            int pe = ps + DurOf(p.chord);
            if (p.chord.isRest && pe > newStart && ps < newEnd) {
                if (ps < newStart) {
                    ChordToken leftRest = p.chord;
                    leftRest.SetTicks(newStart - ps);
                    resolved.push_back(Item{ps, leftRest});
                }
                if (pe > newEnd) {
                    ChordToken rightRest = p.chord;
                    rightRest.SetTicks(pe - newEnd);
                    resolved.push_back(Item{newEnd, rightRest});
                }
                continue;
            }
            resolved.push_back(p);
        }
        resolved.push_back(Item{newStart, newChord});
    } else {
        for (const auto& p : placed) {
            int ps = p.start;
            int pe = ps + DurOf(p.chord);
            if (p.chord.isRest) {
                if (pe <= newStart || ps >= newEnd) {
                    resolved.push_back(p);
                } else {
                    if (ps < newStart) {
                        ChordToken leftRest = p.chord;
                        leftRest.SetTicks(newStart - ps);
                        resolved.push_back(Item{ps, leftRest});
                    }
                    if (pe > newEnd) {
                        ChordToken rightRest = p.chord;
                        rightRest.SetTicks(pe - newEnd);
                        resolved.push_back(Item{newEnd, rightRest});
                    }
                }
            } else if (pe <= newStart) {
                resolved.push_back(p);
            } else if (ps >= newStart) {
                // 挿入位置より後ろのコードは挿入長ぶん後方へ押し出す
                Item shifted = p;
                shifted.start += chordTicks;
                resolved.push_back(shifted);
            } else {
                // 挿入位置をまたぐコードは前後に分割（挿入区間は置き換え）
                if (newStart > ps) {
                    ChordToken left = p.chord;
                    left.SetTicks(newStart - ps);
                    resolved.push_back(Item{ps, left});
                }
                if (pe > newEnd) {
                    ChordToken right = p.chord;
                    right.SetTicks(pe - newEnd);
                    resolved.push_back(Item{newEnd, right});
                }
            }
        }
        resolved.push_back(Item{newStart, newChord});
    }

    resolved = MergeAndSort(std::move(resolved));
    AbsorbByRests(resolved, barTicks, newEnd);
    WriteItems(bar, std::move(resolved));
    NormalizeBar(bar, &overflow);
    return overflow;
}

} // namespace clp
