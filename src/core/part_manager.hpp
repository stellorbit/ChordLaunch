#pragma once
#include "music_theory.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace clp {

class PartManager {
public:
    PartManager();

    // パート一覧の操作
    const std::vector<Part>& GetParts() const { return m_parts; }
    std::vector<Part>& GetParts() { return m_parts; }

    int GetActiveIndex() const { return m_activePartIndex; }
    void SetActiveIndex(int index);

    Part* GetActivePart();
    const Part* GetActivePart() const;

    // 指定パートから、キーとモードが同じ間だけ前へさかのぼり、その連続範囲の先頭パートの index を返す。
    // 範囲外の index はそのまま返す。
    int FindKeyRunStart(int index) const;

    // アクティブパートのキー / モード / BPM を変更する。
    // linked=true: 変更前にアクティブパートと同じ値だった全パートへ反映する（異なる値のパートは変更しない）。
    // linked=false: アクティブパートのみ変更する。
    // 戻り値は値が書き換わったパートの index 一覧（アクティブパートが先頭とは限らない）。
    std::vector<int> ApplyKey(std::string_view newKey, bool linked);
    std::vector<int> ApplyMode(MusicalMode newMode, bool linked);
    std::vector<int> ApplyBpm(int newBpm, bool linked);

    // 新規パートは末尾パートのキー・モード・BPM を引き継ぐ（パートが無ければ引数の既定値）。
    Part& AddPart(std::string_view name = "New Part", std::string_view key = "C", MusicalMode mode = MusicalMode::Major);
    bool RemovePart(int index);
    bool DuplicatePart(int index);
    void MovePart(int fromIndex, int toIndex);

    // 小節番号を1から振り直す（各パートは常に1小節目から始まる）
    static void RenumberBars(std::vector<Bar>& bars);

    // パートの総 tick 数（各小節の GetTotalTicks の合計）
    static int PartLengthTicks(const Part& part);

    // パート内の任意位置（パート先頭からの tick）にマーカーを設定する。空文字で解除。同一 tick は上書き。
    // tick <= 0（パート先頭＝タブ名の位置）、パート長が 2 tick 未満、パート不正、"Key:" 始まりの名前は false。
    // パート長以上の tick は末尾（長さ-1）へ丸める。[ ] と改行は名前から除去する。
    bool SetMarker(int partIndex, int tick, std::string_view name);
    bool RemoveMarker(int partIndex, int tick);
    // 指定 tick から tolerance 以内で最も近いマーカーの tick を返す（無ければ -1）
    int FindMarkerNear(int partIndex, int tick, int tolerance) const;

    // 旧形式（Bar::sectionMarker）のマーカーを自由位置マーカーへ変換し、範囲外のマーカーを丸める。
    // 先頭小節のマーカーはタブ名と同義のため破棄する。変換後は全小節の sectionMarker が空になる。
    static void MigrateBarMarkers(Part& part);

    // 全パートの自由位置マーカーを、曲頭からの絶対 tick に直して返す（パート先頭のタブ名は含まない）
    std::vector<PartMarker> GenerateMergedMarkers() const;

    // 全体マージ（結合）処理
    // 全パートを順次結合し、転調箇所にキー変更マーカーを自動割り当てした小節列を生成
    std::vector<Bar> GenerateMergedProgression() const;

    // デフォルト構成で初期化（Intro, Aメロ, Bメロ, サビ）
    void ResetToDefaultSong();

    // プロジェクト全体のシリアライズ / デシリアライズ
    std::string SerializeProject(
        std::string_view projectTitle = "Untitled",
        VoicingStyle voicing = VoicingStyle::RootPosition,
        int bassMode = 0,
        int gridTicks = TICKS_PER_BEAT,
        int inputGridTicks = 0
    ) const;

    bool DeserializeProject(
        const std::string& data,
        std::string& outTitle,
        VoicingStyle& outVoicing,
        int& outBassMode,
        int& outGridTicks,
        int* outInputGridTicks = nullptr
    );

    // ファイル保存・読み込み
    bool SaveToFile(
        const std::wstring& filePath,
        std::string_view projectTitle = "Untitled",
        VoicingStyle voicing = VoicingStyle::RootPosition,
        int bassMode = 0,
        int gridTicks = TICKS_PER_BEAT,
        int inputGridTicks = 0
    ) const;

    bool LoadFromFile(
        const std::wstring& filePath,
        std::string& outTitle,
        VoicingStyle& outVoicing,
        int& outBassMode,
        int& outGridTicks,
        int* outInputGridTicks = nullptr
    );

    // 自動バックアップ保存 (最新5件保持ローテーション)
    static bool PerformAutoBackup(
        const std::wstring& currentProjectFilePath,
        const std::string& projectTitle,
        const std::string& projectData,
        const std::wstring& customBackupDir = L""
    );

private:
    std::vector<Part> m_parts;
    int m_activePartIndex = 0;
    int m_nextPartId = 1;
};

} // namespace clp
