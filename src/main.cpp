#include <windows.h>
#include "app_window.hpp"

int WINAPI wWinMain(
    HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] PWSTR pCmdLine,
    int nCmdShow
) {
    // 高DPI対応設定 (PerMonitorV2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // OLE / COM 初期化 (STA)
    HRESULT hr = OleInitialize(nullptr);
    if (FAILED(hr)) {
        return 1;
    }

    {
        clp::AppWindow app;
        if (app.Create(hInstance, nCmdShow)) {
            app.RunMessageLoop();
        }
    }

    OleUninitialize();
    return 0;
}
