#pragma once
#include "music_theory.hpp"
#include <utility>
#include <vector>

namespace clp {

// タイムライン（小節内コード列）の tick ベース編集。
// 小節線に縛られず、コードは絶対tickで配置する。小節線を越えるカードは
// 開始小節に1枚のまま保持し、後続小節側は覆われた区間を切り詰める。
class TimelineEdit {
public:
    // 小節内の実際の合計 tick（休符含む）
    static int TotalTicks(const Bar& bar);

    // 複数小節の合計 tick（各小節の拍子ぶん）
    static int SongTicks(const std::vector<Bar>& bars);

    // 追記位置: 最後のコード（休符を除く）の終端 tick。コードが無い（空・全休符）場合は 0
    static int AppendTick(const std::vector<Bar>& bars);

    // 指定小節の先頭が曲頭から何 tick か
    static int BarStartTick(const std::vector<Bar>& bars, int barIndex);

    // 前小節から伸びてくる跨ぎカードが、この小節の先頭から何 tick まで覆っているか
    static int IncomingCoverUntil(const std::vector<Bar>& bars, int barIndex);

    // 連続休符の結合・0tick要素の除去・末尾休符の補完。
    // 小節最後のコードが小節線を越えて伸びる「小節跨ぎカード」はそのまま保持し、
    // それ以外の小節超過分は切り詰める（プロジェクト読込用）。
    static void NormalizeBar(Bar& bar);

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

    // 1枚のコードを絶対tickで移動する。長さは保持し、小節線を越えても1枚のままとする。
    // 移動先に重なる他コードはその区間だけ切り詰められる（曲長は変化しない）。
    static SpanResult MoveChordSpan(const std::vector<Bar>& baseBars,
                                    int targetStartTick,
                                    int newStartTick,
                                    int minTicks);

    // 絶対tick startTick へコードを挿入する。長さは保持し、小節線を越えても1枚のままとする。
    // 既存コードと衝突する場合は、またぐコードを前後に分割し、挿入位置以降のコードを
    // 後方の休符（隙間）で吸収しながら押し出す。曲末を越える分は休符小節を足して収める。
    // 休符コードの挿入は何もしない（selectedBar = -1）。
    static SpanResult InsertChordSpan(const std::vector<Bar>& baseBars,
                                      const ChordToken& chord,
                                      int startTick);

    // クリップボード用: 選択コード群を、先頭コードを 0 とした相対tickで保持する。
    struct RangeItem {
        int startTick = 0; // 先頭コードからの相対 tick
        ChordToken chord;
    };

    // 絶対開始tickで指定したコード群を取り出す（開始順。該当なしは空）。
    static std::vector<RangeItem> ExtractChords(const std::vector<Bar>& bars,
                                                const std::vector<int>& startTicks);

    // 絶対開始tickで指定したコード群を取り除く（跡は休符になり、曲長は変化しない）。
    static std::vector<Bar> RemoveChords(const std::vector<Bar>& bars,
                                         const std::vector<int>& startTicks);

    // 取り出したコード群を atTick（絶対tick）を基点に貼り付ける。
    // 重なる既存コードはその区間だけ切り詰め、曲末を越える分は貼り付けない。
    static std::vector<Bar> InsertRange(const std::vector<Bar>& bars,
                                        const std::vector<RangeItem>& items,
                                        int atTick);

    // tick → 小節内のコード index（範囲外は -1）
    static int ChordIndexAtTick(const Bar& bar, int tick);
};

} // namespace clp
