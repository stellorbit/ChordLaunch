#pragma once
#include "music_theory.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace clp {

class ProgressionParser {
public:
    // テキストから小節・コード列への解析
    static std::vector<Bar> Parse(
        std::string_view text,
        std::string_view defaultKey = "C",
        MusicalMode defaultMode = MusicalMode::Major,
        int defaultNumerator = 4,
        int defaultDenominator = 4
    );

    // 小節・コード列から標準テキストへの整形
    static std::string Format(
        const std::vector<Bar>& bars,
        int barsPerLine = 4
    );

    // ハイフン連接トークン（例: "C---", "Fmaj7--"）の分離・正規化
    static std::vector<std::string> NormalizeTokens(const std::vector<std::string>& tokens);

private:
    static std::vector<std::string> TokenizeBar(std::string_view barContent);
    static bool IsRepeatToken(std::string_view token);
    static bool IsRestToken(std::string_view token);
};

} // namespace clp
