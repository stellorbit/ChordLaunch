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

    Part& AddPart(std::string_view name = "New Part", std::string_view key = "C", MusicalMode mode = MusicalMode::Major);
    bool RemovePart(int index);
    bool DuplicatePart(int index);
    void MovePart(int fromIndex, int toIndex);

    // 全体マージ（結合）処理
    // 全パートを順次結合し、転調箇所にキー変更マーカーを自動割り当てした小節列を生成
    std::vector<Bar> GenerateMergedProgression() const;

    // デフォルト構成で初期化（Intro, Aメロ, Bメロ, サビ）
    void ResetToDefaultSong();

private:
    std::vector<Part> m_parts;
    int m_activePartIndex = 0;
};

} // namespace clp
