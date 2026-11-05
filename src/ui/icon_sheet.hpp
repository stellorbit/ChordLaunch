#pragma once
#include <windows.h>
#include <d2d1.h>
#include "ui_icon_sheet.h"

namespace clp {

// PNG スプライトシート（白＋透明）をリソースから読み込み、アルファをマスクとして
// 任意のブラシ色で描画する。ビットマップはデバイス依存のため、描画ターゲットの
// 再作成時は Release() → Create() をやり直すこと。
class IconSheet {
public:
    IconSheet() = default;
    ~IconSheet() { Release(); }
    IconSheet(const IconSheet&) = delete;
    IconSheet& operator=(const IconSheet&) = delete;

    // 全サイズのシートを読み込む。1つでも失敗したら false（Draw は false を返し続ける）
    bool Create(ID2D1RenderTarget* pRT, HINSTANCE hInstance);
    void Release();
    bool IsReady() const { return m_ready; }

    // dest の中央に、一辺 side(px, 整数) の正方形でアイコンを描く。
    // dpiScale: 画面DPI/96。実ピクセル数に最も近い大きさのシートを選ぶ。
    bool Draw(ID2D1RenderTarget* pRT, UiIcon icon, const D2D1_RECT_F& dest, float side,
              ID2D1Brush* pBrush, float dpiScale) const;

private:
    static constexpr int kSheetCount = static_cast<int>(sizeof(kUiIconCellSizes) / sizeof(kUiIconCellSizes[0]));
    ID2D1Bitmap* m_bitmaps[kSheetCount] = {};
    bool m_ready = false;
};

} // namespace clp
