#pragma once

// ChordLaunch バージョン定義 (単一の定義元)
// 参照元: アプリ本体 (ウィンドウタイトル / バージョン情報)、resources\ChordLaunch.rc、
//         CMakeLists.txt (project VERSION)、scripts\Build-Release.ps1
// 更新時はここだけを変更する。
#define CHORDLAUNCH_VER_MAJOR 26
#define CHORDLAUNCH_VER_MINOR 11
#define CHORDLAUNCH_FILEVERSION_STR "26.11.0.0"  // MAJOR.MINOR.0.0 (rc の文字列リソース用。MAJOR/MINOR と一致させる)
#define CHORDLAUNCH_VERSION_STR "v26.11-Pre"
#define CHORDLAUNCH_REPO_URL "https://github.com/stellorbit/ChordLaunch"
// ワイド文字列版 (C++ 側の UI 表示用。rc では使用しない)
#define CHORDLAUNCH_VERSION_WSTR L"" CHORDLAUNCH_VERSION_STR
#define CHORDLAUNCH_REPO_URL_WSTR L"" CHORDLAUNCH_REPO_URL
