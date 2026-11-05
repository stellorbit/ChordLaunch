#include "icon_sheet.hpp"
#include "../resource.h"
#include <wincodec.h>
#include <cmath>
#include <algorithm>

namespace clp {

namespace {

constexpr WORD kResourceIds[] = { IDR_UIICONS_24, IDR_UIICONS_48, IDR_UIICONS_96 };
static_assert(sizeof(kResourceIds) / sizeof(kResourceIds[0]) == sizeof(kUiIconCellSizes) / sizeof(kUiIconCellSizes[0]),
              "resource.h の IDR_UIICONS_* と kUiIconCellSizes の数が一致していません");

template <typename T>
void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

// RCDATA の PNG を WIC でデコードして D2D ビットマップ (32bpp PBGRA) にする
ID2D1Bitmap* LoadPngResource(ID2D1RenderTarget* pRT, IWICImagingFactory* pWic, HINSTANCE hInstance, WORD resId) {
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return nullptr;
    HGLOBAL hData = LoadResource(hInstance, hRes);
    const DWORD size = SizeofResource(hInstance, hRes);
    const void* pData = hData ? LockResource(hData) : nullptr;
    if (!pData || size == 0) return nullptr;

    IWICStream* pStream = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    ID2D1Bitmap* pBitmap = nullptr;

    HRESULT hr = pWic->CreateStream(&pStream);
    if (SUCCEEDED(hr)) hr = pStream->InitializeFromMemory(const_cast<BYTE*>(static_cast<const BYTE*>(pData)), size);
    if (SUCCEEDED(hr)) hr = pWic->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pFrame);
    if (SUCCEEDED(hr)) hr = pWic->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                    nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    }
    if (SUCCEEDED(hr)) hr = pRT->CreateBitmapFromWicBitmap(pConverter, nullptr, &pBitmap);

    SafeRelease(pConverter);
    SafeRelease(pFrame);
    SafeRelease(pDecoder);
    SafeRelease(pStream);
    return SUCCEEDED(hr) ? pBitmap : nullptr;
}

} // namespace

bool IconSheet::Create(ID2D1RenderTarget* pRT, HINSTANCE hInstance) {
    Release();
    if (!pRT) return false;

    IWICImagingFactory* pWic = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWic));
    if (FAILED(hr) || !pWic) return false;

    bool ok = true;
    for (int i = 0; i < kSheetCount; ++i) {
        m_bitmaps[i] = LoadPngResource(pRT, pWic, hInstance, kResourceIds[i]);
        if (!m_bitmaps[i]) { ok = false; break; }
    }
    pWic->Release();

    if (!ok) {
        Release();
        return false;
    }
    m_ready = true;
    return true;
}

void IconSheet::Release() {
    for (auto& b : m_bitmaps) SafeRelease(b);
    m_ready = false;
}

bool IconSheet::Draw(ID2D1RenderTarget* pRT, UiIcon icon, const D2D1_RECT_F& dest, float side,
                     ID2D1Brush* pBrush, float dpiScale) const {
    const int index = static_cast<int>(icon);
    if (!m_ready || !pRT || !pBrush || index < 0 || index >= static_cast<int>(UiIcon::Count)) return false;
    if (side < 1.0f) return false;

    // 実ピクセル数以上で最小のシートを選ぶ（無ければ最大）
    const float needPx = side * std::max(1.0f, dpiScale);
    int sheet = kSheetCount - 1;
    for (int i = 0; i < kSheetCount; ++i) {
        if (static_cast<float>(kUiIconCellSizes[i]) >= needPx) { sheet = i; break; }
    }

    const float cell = static_cast<float>(kUiIconCellSizes[sheet]);
    const D2D1_RECT_F src = D2D1::RectF(cell * index, 0.0f, cell * (index + 1), cell);

    // 中心を整数ピクセルに揃えてにじみを防ぐ
    const float cx = std::round((dest.left + dest.right) * 0.5f);
    const float cy = std::round((dest.top + dest.bottom) * 0.5f);
    const float half = std::round(side * 0.5f);
    const D2D1_RECT_F dst = D2D1::RectF(cx - half, cy - half, cx + half, cy + half);

    // FillOpacityMask はエイリアス描画モードが必須（マスク自体が既にアンチエイリアス済み）
    const D2D1_ANTIALIAS_MODE prevAa = pRT->GetAntialiasMode();
    pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    pRT->FillOpacityMask(m_bitmaps[sheet], pBrush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS, &dst, &src);
    pRT->SetAntialiasMode(prevAa);
    return true;
}

} // namespace clp
