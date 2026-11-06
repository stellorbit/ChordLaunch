#pragma once
#include <d2d1.h>

namespace clp {

struct Theme {
    // Ver 2026.9.2 準拠のソフトで洗練されたライトテーマ（目に優しい淡いグレーベース＋白カード）
    static constexpr D2D1_COLOR_F WindowBackground   = { 0.965f, 0.961f, 0.973f, 1.0f }; // #F6F5F8
    static constexpr D2D1_COLOR_F CardBackground     = { 1.000f, 1.000f, 1.000f, 1.0f }; // #FFFFFF
    static constexpr D2D1_COLOR_F CardHeaderBg       = { 0.973f, 0.976f, 0.980f, 1.0f }; // #F8F9FA
    static constexpr D2D1_COLOR_F CardHover          = { 0.945f, 0.965f, 1.000f, 1.0f }; // #F1F6FF
    static constexpr D2D1_COLOR_F CardActive         = { 0.880f, 0.925f, 0.985f, 1.0f }; // #E0ECFA
    static constexpr D2D1_COLOR_F CardSelected       = { 0.820f, 0.890f, 0.990f, 1.0f }; // #D1E3FD
    
    // 枠線色
    static constexpr D2D1_COLOR_F CardBorder         = { 0.886f, 0.894f, 0.918f, 1.0f }; // #E2E4EA
    static constexpr D2D1_COLOR_F CardBorderHover    = { 0.650f, 0.720f, 0.820f, 1.0f };
    static constexpr D2D1_COLOR_F CardBorderFocus    = { 0.231f, 0.510f, 0.965f, 1.0f }; // #3B82F6 (フォーカス枠)
    static constexpr D2D1_COLOR_F PillBorder         = { 0.820f, 0.835f, 0.859f, 1.0f }; // #D1D5DB
    
    // 文字色
    static constexpr D2D1_COLOR_F TextPrimary        = { 0.118f, 0.161f, 0.231f, 1.0f }; // #1E293B
    static constexpr D2D1_COLOR_F TextSecondary      = { 0.392f, 0.455f, 0.545f, 1.0f }; // #64748B
    static constexpr D2D1_COLOR_F TextMuted          = { 0.580f, 0.620f, 0.680f, 1.0f };
    
    // アクション・アクセント色
    static constexpr D2D1_COLOR_F AccentWarm         = { 0.850f, 0.420f, 0.220f, 1.0f }; // #D96B38 (テラコッタ)
    static constexpr D2D1_COLOR_F AccentPlay         = { 0.086f, 0.639f, 0.290f, 1.0f }; // #16A34A (鮮やかなグリーン)
    static constexpr D2D1_COLOR_F AccentStop         = { 0.863f, 0.149f, 0.149f, 1.0f }; // #DC2626 (鮮やかなレッド)
    static constexpr D2D1_COLOR_F AccentAction       = { 0.145f, 0.388f, 0.922f, 1.0f }; // #2563EB (DAWドラッグ等)
    static constexpr D2D1_COLOR_F Playhead           = { 0.900f, 0.220f, 0.200f, 1.0f }; // プレイヘッド赤
    static constexpr D2D1_COLOR_F LoopRangeFill      = { 0.231f, 0.510f, 0.965f, 0.12f }; // ループ選択範囲
    
    // タイムラインカード・ノート
    static constexpr D2D1_COLOR_F RestBlockFill      = { 0.940f, 0.940f, 0.945f, 1.0f }; // 休符背景
    static constexpr D2D1_COLOR_F WhiteKey           = { 1.000f, 1.000f, 1.000f, 1.0f };
    static constexpr D2D1_COLOR_F BlackKey           = { 0.180f, 0.190f, 0.210f, 1.0f };
    static constexpr D2D1_COLOR_F ActiveKey          = { 0.980f, 0.750f, 0.450f, 1.0f };
    
    // モーダル・ダイアログ用
    static constexpr D2D1_COLOR_F ModalBackdrop      = { 0.000f, 0.000f, 0.000f, 0.45f }; // 半透明黒
    static constexpr D2D1_COLOR_F ModalCardBg        = { 1.000f, 1.000f, 1.000f, 1.0f };
    static constexpr D2D1_COLOR_F ModalCardBorder    = { 0.800f, 0.820f, 0.850f, 1.0f };
    
    // メトリクス
    static constexpr float CardRadius = 6.0f;
    static constexpr float PillRadius = 18.0f;
    static constexpr float OuterPadding = 12.0f;
    static constexpr float ColumnGap = 8.0f;
    
    // UDフォント名（BIZ UDPゴシック優先、環境フォールバックにYu Gothic UI）
    static constexpr const wchar_t* FontName = L"BIZ UDPGothic";
    static constexpr const wchar_t* FallbackFontName = L"Yu Gothic UI";
};

} // namespace clp
