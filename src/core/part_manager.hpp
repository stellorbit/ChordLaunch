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

    Part& AddPart(std::string_view name = "New Part", std::string_view key = "C", MusicalMode mode = MusicalMode::Major);
    bool RemovePart(int index);
    bool DuplicatePart(int index);
    void MovePart(int fromIndex, int toIndex);

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
