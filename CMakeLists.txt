cmake_minimum_required(VERSION 3.25)
# バージョンの定義元は src/version.h (MAJOR/MINOR を読み込む)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h" _clp_ver_lines REGEX "^#define CHORDLAUNCH_VER_(MAJOR|MINOR) ")
foreach(_line IN LISTS _clp_ver_lines)
    if(_line MATCHES "CHORDLAUNCH_VER_MAJOR ([0-9]+)")
        set(_clp_ver_major "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "CHORDLAUNCH_VER_MINOR ([0-9]+)")
        set(_clp_ver_minor "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(NOT DEFINED _clp_ver_major OR NOT DEFINED _clp_ver_minor)
    message(FATAL_ERROR "src/version.h から CHORDLAUNCH_VER_MAJOR/MINOR を取得できません。")
endif()

project(ChordLaunch VERSION ${_clp_ver_major}.${_clp_ver_minor} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 完全静的リンク (/MT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 共通C++高速化モジュール (Ninja + ccache + lld-link + PCH/Unity + /O2 /GL /LTCG /arch:AVX2 /fp:fast /Gw + PGO)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CppOptimization.cmake")
enable_cpp_build_optimizations()

if (MSVC)
    add_compile_options(
        /W4
        /WX
        /permissive-
        /EHsc
        /utf-8
        /DUNICODE
        /D_UNICODE
        /DWIN32_LEAN_AND_MEAN
        /DNOMINMAX
    )
    add_link_options(
        /DYNAMICBASE
        /NXCOMPAT
        /HIGHENTROPYVA
    )
endif()

# コアライブラリ
add_library(clp_core STATIC
    src/core/music_engine.cpp
    src/core/progression_parser.cpp
    src/core/timeline_edit.cpp
    src/core/part_manager.cpp
    src/core/midi_builder.cpp
    src/core/i18n.cpp
    src/audio/wasapi_synth.cpp
    src/ui/ole_dnd.cpp
    src/ui/icon_sheet.cpp
    src/ui/app_window.cpp
    src/ui/app_window_render.cpp
    src/ui/app_window_modals.cpp
    src/ui/app_window_input.cpp
    src/ui/app_window_actions.cpp
)

enable_target_pch(clp_core)

target_include_directories(clp_core PUBLIC
    src/core
    src/audio
    src/ui
)

target_link_libraries(clp_core PUBLIC
    d2d1.lib
    dwrite.lib
    windowscodecs.lib
    ole32.lib
    comctl32.lib
    user32.lib
    gdi32.lib
    shell32.lib
)

# メイン実行ファイル (GUI / WinMain)
add_executable(ChordLaunch WIN32
    src/main.cpp
    resources/ChordLaunch.rc
)
target_link_libraries(ChordLaunch PRIVATE clp_core)
# アイコン PNG の更新でもリソースを再コンパイルさせる
set_source_files_properties(resources/ChordLaunch.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/ui_icons_24.png;${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/ui_icons_48.png;${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/ui_icons_96.png")

# 自動単体テスト実行ファイル
add_executable(clp_tests
    tests/test_main.cpp
)
target_link_libraries(clp_tests PRIVATE clp_core)
