#pragma once
#include <d2d1.h>

namespace clp {

struct Theme {
    // 暖色系ライトテーマ (目に優しいソフトなオフホワイト・アイボリー基調)
    static constexpr D2D1_COLOR_F Background        = { 0.965f, 0.960f, 0.950f, 1.0f }; // #F6F5F2
    static constexpr D2D1_COLOR_F PaneBackground      = { 0.990f, 0.985f, 0.980f, 1.0f }; // #FCFBF9
    static constexpr D2D1_COLOR_F TopBarBackground    = { 0.940f, 0.930f, 0.915f, 1.0f }; // #F0EDE9
    static constexpr D2D1_COLOR_F CardBackground      = { 1.000f, 1.000f, 1.000f, 1.0f }; // #FFFFFF
    static constexpr D2D1_COLOR_F CardHover           = { 0.950f, 0.970f, 1.000f, 1.0f }; // #F2F7FF
    static constexpr D2D1_COLOR_F CardActive          = { 0.880f, 0.920f, 0.980f, 1.0f }; // #E0EBFA
    static constexpr D2D1_COLOR_F CardSelected        = { 0.820f, 0.890f, 0.990f, 1.0f }; // #D1E3FD
    static constexpr D2D1_COLOR_F Border              = { 0.840f, 0.825f, 0.805f, 1.0f }; // #D6D2CD
    static constexpr D2D1_COLOR_F BorderHover         = { 0.600f, 0.650f, 0.720f, 1.0f };
    static constexpr D2D1_COLOR_F TextPrimary         = { 0.160f, 0.160f, 0.170f, 1.0f }; // #29292B
    static constexpr D2D1_COLOR_F TextSecondary       = { 0.450f, 0.440f, 0.430f, 1.0f }; // #73706E
    static constexpr D2D1_COLOR_F AccentWarm          = { 0.850f, 0.420f, 0.220f, 1.0f }; // #D96B38 (暖色アンバー・テラコッタ)
    static constexpr D2D1_COLOR_F AccentPlay          = { 0.200f, 0.650f, 0.350f, 1.0f }; // #33A659
    static constexpr D2D1_COLOR_F Playhead            = { 0.900f, 0.250f, 0.200f, 1.0f }; // 赤色プレイヘッド
    static constexpr D2D1_COLOR_F LoopRangeFill       = { 0.850f, 0.500f, 0.200f, 0.15f }; // 半透明ループ範囲
    static constexpr D2D1_COLOR_F SelectionRectFill   = { 0.200f, 0.500f, 0.900f, 0.18f };
    static constexpr D2D1_COLOR_F SelectionRectBorder = { 0.200f, 0.500f, 0.900f, 0.80f };
    static constexpr D2D1_COLOR_F RestBlockFill       = { 0.920f, 0.910f, 0.900f, 1.0f }; // 休符背景
    static constexpr D2D1_COLOR_F WhiteKey            = { 1.000f, 1.000f, 1.000f, 1.0f };
    static constexpr D2D1_COLOR_F BlackKey            = { 0.220f, 0.220f, 0.230f, 1.0f };
    static constexpr D2D1_COLOR_F ActiveKey           = { 0.980f, 0.750f, 0.450f, 1.0f }; // 鍵盤ハイライト

    // UDフォント名（BIZ UDPゴシック優先、環境フォールバックにYu Gothic UI）
    static constexpr const wchar_t* FontName = L"BIZ UDPGothic";
    static constexpr const wchar_t* FallbackFontName = L"Yu Gothic UI";
};

} // namespace clp
