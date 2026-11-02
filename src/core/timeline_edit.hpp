#pragma once
#include "music_theory.hpp"
#include <utility>
#include <vector>

namespace clp {

// タイムライン（小節内コード列）の tick ベース編集。
// すべての関数は「小節のコード列が必ず Bar::GetTotalTicks() 分を過不足なく覆う」
// という不変条件を維持する。はみ出した内容は overflow として次小節へ引き渡す。
class TimelineEdit {
public:
    struct OverflowItem {
        int startTick = 0; // 次小節先頭からの tick
        ChordToken chord;
    };

    // 小節内の実際の合計 tick（休符含む）
    static int TotalTicks(const Bar& bar);

    // 複数小節の合計 tick（各小節の拍子ぶん）
    static int SongTicks(const std::vector<Bar>& bars);

    // 指定小節の先頭が曲頭から何 tick か
    static int BarStartTick(const std::vector<Bar>& bars, int barIndex);

    // 前小節から伸びてくる跨ぎカードが、この小節の先頭から何 tick まで覆っているか
    static int IncomingCoverUntil(const std::vector<Bar>& bars, int barIndex);

    // 連続休符の結合・0tick要素の除去・末尾休符の補完・小節超過分の切り出し。
    // allowTailSpill=true のとき、小節最後のコードが小節線を越えて伸びる
    // 「小節跨ぎカード」をそのまま保持する（プロジェクト読込用）。
    static void NormalizeBar(Bar& bar, std::vector<OverflowItem>* overflow = nullptr, bool allowTailSpill = false);

    // 指定 tick にコードを挿入する。既存コードと衝突する場合は既存コードを
    // 前後に分割し、後続を後ろへ押し出す（既存データを失わない）。
    // 戻り値は次小節へあふれた内容。bar は正規化済みで返る。
    static std::vector<OverflowItem> InsertChord(Bar& bar, const ChordToken& chord, int startTick);

    // 小節線を越えるリサイズ。targetStartTick のコードの開始/終了を絶対tickで動かし、
    // パート全体を絶対時間で再構築する（カードは1枚のまま小節を跨ぐ）。
    // 伸ばした区間に重なる他コードは、その区間だけ切り詰められる（曲長は変化しない）。
    struct SpanResult {
        std::vector<Bar> bars;
        int selectedBar = -1;
        int selectedChord = -1;
    };
    static SpanResult ResizeChordSpan(const std::vector<Bar>& baseBars,
                                      int targetStartTick,
                                      int newStartTick,
                                      int newEndTick,
                                      int minTicks);

    // tick → 小節内のコード index（範囲外は -1）
    static int ChordIndexAtTick(const Bar& bar, int tick);
};

} // namespace clp
