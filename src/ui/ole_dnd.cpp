#include "ole_dnd.hpp"

namespace clp {

// OleFileDropSource 実装
OleFileDropSource::OleFileDropSource() : m_refCount(1) {}

HRESULT STDMETHODCALLTYPE OleFileDropSource::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDropSource) {
        *ppvObject = static_cast<IDropSource*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE OleFileDropSource::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE OleFileDropSource::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE OleFileDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
    if (fEscapePressed) {
        return DRAGDROP_S_CANCEL;
    }
    // 左マウスボタンが離されたらドロップ実行
    if (!(grfKeyState & MK_LBUTTON)) {
        return DRAGDROP_S_DROP;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OleFileDropSource::GiveFeedback([[maybe_unused]] DWORD dwEffect) {
    return DRAGDROP_S_USEDEFAULTCURSORS;
}

// OleFileDataObject 実装
OleFileDataObject::OleFileDataObject(const std::wstring& filePath)
    : m_refCount(1), m_filePath(filePath) {
    m_formatEtc.cfFormat = CF_HDROP;
    m_formatEtc.ptd = nullptr;
    m_formatEtc.dwAspect = DVASPECT_CONTENT;
    m_formatEtc.lindex = -1;
    m_formatEtc.tymed = TYMED_HGLOBAL;
}

OleFileDataObject::~OleFileDataObject() = default;

HRESULT STDMETHODCALLTYPE OleFileDataObject::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDataObject) {
        *ppvObject = static_cast<IDataObject*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE OleFileDataObject::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE OleFileDataObject::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) {
    if (!pformatetcIn || !pmedium) return E_POINTER;
    if (pformatetcIn->cfFormat != CF_HDROP || !(pformatetcIn->tymed & TYMED_HGLOBAL)) {
        return DV_E_FORMATETC;
    }

    // DROPFILES 構造体 + NULL終端ワイド文字列 + 終端NULL
    size_t charCount = m_filePath.size() + 2;
    size_t bufferSize = sizeof(DROPFILES) + charCount * sizeof(wchar_t);

    HGLOBAL hGlobal = GlobalAlloc(GHND, bufferSize);
    if (!hGlobal) return E_OUTOFMEMORY;

    DROPFILES* pDropFiles = reinterpret_cast<DROPFILES*>(GlobalLock(hGlobal));
    if (!pDropFiles) {
        GlobalFree(hGlobal);
        return E_OUTOFMEMORY;
    }

    pDropFiles->pFiles = sizeof(DROPFILES);
    pDropFiles->pt.x = 0;
    pDropFiles->pt.y = 0;
    pDropFiles->fNC = FALSE;
    pDropFiles->fWide = TRUE;

    wchar_t* pFileList = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(pDropFiles) + sizeof(DROPFILES));
    wcsncpy_s(pFileList, charCount, m_filePath.c_str(), m_filePath.size());
    pFileList[m_filePath.size()] = L'\0';
    pFileList[m_filePath.size() + 1] = L'\0';

    GlobalUnlock(hGlobal);

    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->hGlobal = hGlobal;
    pmedium->pUnkForRelease = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::GetDataHere([[maybe_unused]] FORMATETC* pformatetc, [[maybe_unused]] STGMEDIUM* pmedium) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::QueryGetData(FORMATETC* pformatetc) {
    if (!pformatetc) return E_POINTER;
    if (pformatetc->cfFormat == CF_HDROP && (pformatetc->tymed & TYMED_HGLOBAL)) {
        return S_OK;
    }
    return DV_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::GetCanonicalFormatEtc([[maybe_unused]] FORMATETC* pformatectIn, [[maybe_unused]] FORMATETC* pformatetcOut) {
    return DATA_S_SAMEFORMATETC;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::SetData([[maybe_unused]] FORMATETC* pformatetc, [[maybe_unused]] STGMEDIUM* pmedium, [[maybe_unused]] BOOL fRelease) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::EnumFormatEtc([[maybe_unused]] DWORD dwDirection, [[maybe_unused]] IEnumFORMATETC** ppenumFormatEtc) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::DAdvise([[maybe_unused]] FORMATETC* pformatetc, [[maybe_unused]] DWORD advf, [[maybe_unused]] IAdviseSink* pAdvSink, [[maybe_unused]] DWORD* pdwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::DUnadvise([[maybe_unused]] DWORD dwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE OleFileDataObject::EnumDAdvise([[maybe_unused]] IEnumSTATDATA** ppenumAdvise) {
    return OLE_E_ADVISENOTSUPPORTED;
}

DWORD OleFileDataObject::StartFileDrag([[maybe_unused]] HWND hwnd, const std::wstring& filePath) {
    OleFileDataObject* pDataObject = new OleFileDataObject(filePath);
    OleFileDropSource* pDropSource = new OleFileDropSource();

    DWORD dwEffect = 0;
    DoDragDrop(pDataObject, pDropSource, DROPEFFECT_COPY, &dwEffect);

    pDropSource->Release();
    pDataObject->Release();

    return dwEffect;
}

} // namespace clp
