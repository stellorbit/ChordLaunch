# ==============================================================================
# CppOptimization.cmake
# C++ ビルド・コンパイル時間爆速化 & バイナリ実行速度極大化 共通モジュール
#
# 1. ビルド高速化: Ninja + ccache + lld-link + PCH + Unity Build
# 2. 実行速度極大化: /O2 /Oi /Ot /Oy /GL /LTCG /arch:AVX2 /fp:fast /Gw /OPT:REF,ICF
# 3. PGO (Profile Guided Optimization): /GENPROFILE -> 走行計測 -> /USEPROFILE
# ==============================================================================

include_guard(GLOBAL)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

option(ENABLE_CCACHE "Use ccache compiler cache if available" ON)
option(USE_LLD_LINK "Use LLVM lld-link for ultra-fast linking (for non-/GL builds)" OFF)
option(ENABLE_UNITY_BUILD "Enable CMake Unity Build (batch compilation)" ON)
option(ENABLE_PCH "Enable Precompiled Headers (PCH)" ON)
option(ENABLE_MAX_OPTIMIZATION "Enable maximum binary runtime optimization (/O2 /GL /LTCG /arch:AVX2 /fp:fast /Gw)" ON)
set(CPU_ARCH_FLAG "/arch:AVX2" CACHE STRING "MSVC SIMD architecture flag (/arch:AVX2 or /arch:AVX512)")
set(PGO_MODE "NONE" CACHE STRING "Profile Guided Optimization mode: NONE, INSTRUMENT, OPTIMIZE")
set_property(CACHE PGO_MODE PROPERTY STRINGS "NONE" "INSTRUMENT" "OPTIMIZE")

function(enable_cpp_build_optimizations)
    # --------------------------------------------------------------------------
    # 1. コンパイラキャッシュ ccache の自動検出・適用
    # --------------------------------------------------------------------------
    if(ENABLE_CCACHE)
        find_program(CCACHE_PROGRAM ccache
            HINTS
                "$ENV{USERPROFILE}/scoop/shims"
                "$ENV{USERPROFILE}/scoop/apps/ccache/current"
        )
        if(CCACHE_PROGRAM)
            set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" PARENT_SCOPE)
            set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" PARENT_SCOPE)
            # MSVC + ccache では mspdbsrv 競合を避けキャッシュヒット率を最大化する /Z7 (Embedded) を使用
            if(MSVC)
                set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" PARENT_SCOPE)
            endif()
            message(STATUS "[CppOpt] ccache enabled: ${CCACHE_PROGRAM}")
        endif()
    endif()

    # --------------------------------------------------------------------------
    # 2. Unity Build (複数 .cpp の一括コンパイル)
    # --------------------------------------------------------------------------
    if(ENABLE_UNITY_BUILD)
        set(CMAKE_UNITY_BUILD ON PARENT_SCOPE)
        if(NOT DEFINED CMAKE_UNITY_BUILD_BATCH_SIZE)
            set(CMAKE_UNITY_BUILD_BATCH_SIZE 8 PARENT_SCOPE)
        endif()
        message(STATUS "[CppOpt] Unity Build enabled (batch size: 8)")
    else()
        set(CMAKE_UNITY_BUILD OFF PARENT_SCOPE)
    endif()

    if(NOT MSVC)
        return()
    endif()

    # --------------------------------------------------------------------------
    # 3. リンカー lld-link の検出と切り替え
    #    ※ MSVC の /GL (独自CIL bitcode) や PGO (.pgd) は link.exe 専用のため、
    #       USE_LLD_LINK=ON かつ PGO_MODE=NONE の場合のみ lld-link を使用する。
    # --------------------------------------------------------------------------
    set(_USING_LLD FALSE)
    if(USE_LLD_LINK AND PGO_MODE STREQUAL "NONE")
        find_program(LLD_LINK_PROGRAM lld-link
            HINTS
                "$ENV{USERPROFILE}/scoop/apps/llvm/current/bin"
                "$ENV{USERPROFILE}/scoop/shims"
        )
        if(LLD_LINK_PROGRAM)
            set(CMAKE_LINKER "${LLD_LINK_PROGRAM}" CACHE FILEPATH "Linker executable" FORCE)
            set(_USING_LLD TRUE)
            message(STATUS "[CppOpt] Ultra-fast linker enabled: ${LLD_LINK_PROGRAM}")
        endif()
    else()
        if(CMAKE_LINKER MATCHES "lld-link")
            find_program(MSVC_LINK_PROGRAM link.exe)
            if(MSVC_LINK_PROGRAM)
                set(CMAKE_LINKER "${MSVC_LINK_PROGRAM}" CACHE FILEPATH "Linker executable" FORCE)
            endif()
        endif()
    endif()

    # --------------------------------------------------------------------------
    # 4. バイナリ実行速度を限界まで高めるコンパイル・リンクオプション
    # --------------------------------------------------------------------------
    if(ENABLE_MAX_OPTIMIZATION)
        add_compile_options(
            $<$<NOT:$<CONFIG:Debug>>:/O2>
            $<$<NOT:$<CONFIG:Debug>>:/Oi>
            $<$<NOT:$<CONFIG:Debug>>:/Ot>
            $<$<NOT:$<CONFIG:Debug>>:/Oy>
            $<$<NOT:$<CONFIG:Debug>>:${CPU_ARCH_FLAG}>
            $<$<NOT:$<CONFIG:Debug>>:/fp:fast>
            $<$<NOT:$<CONFIG:Debug>>:/Gw>
            $<$<NOT:$<CONFIG:Debug>>:/Gy>
        )

        # 不要コード・重複データの完全削除 (/OPT:REF,ICF)
        add_link_options(
            $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
            $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>
        )

        # 全体最適化 LTO (/GL + /LTCG) および PGO (Profile Guided Optimization)
        if(NOT _USING_LLD)
            add_compile_options(
                $<$<NOT:$<CONFIG:Debug>>:/GL>
                $<$<NOT:$<CONFIG:Debug>>:/wd4961>
            )

            if(PGO_MODE STREQUAL "INSTRUMENT")
                add_link_options(
                    $<$<NOT:$<CONFIG:Debug>>:/LTCG>
                    $<$<NOT:$<CONFIG:Debug>>:/GENPROFILE>
                )
                message(STATUS "[CppOpt] PGO Mode: INSTRUMENT (/GL + /LTCG + /GENPROFILE)")
            elseif(PGO_MODE STREQUAL "OPTIMIZE")
                add_link_options(
                    $<$<NOT:$<CONFIG:Debug>>:/LTCG>
                    $<$<NOT:$<CONFIG:Debug>>:/USEPROFILE>
                )
                message(STATUS "[CppOpt] PGO Mode: OPTIMIZE (/GL + /LTCG + /USEPROFILE)")
            else()
                add_link_options($<$<NOT:$<CONFIG:Debug>>:/LTCG>)
                message(STATUS "[CppOpt] Max Optimization enabled (/O2 ${CPU_ARCH_FLAG} /fp:fast /Gw /GL /LTCG /OPT:REF,ICF)")
            endif()
        else()
            message(STATUS "[CppOpt] Fast Dev Optimization enabled (/O2 ${CPU_ARCH_FLAG} /fp:fast /Gw + lld-link /OPT:REF,ICF)")
        endif()
    endif()
endfunction()

# ------------------------------------------------------------------------------
# ターゲット別プリコンパイル済みヘッダー (PCH) 適用関数
# ------------------------------------------------------------------------------
function(enable_target_pch TARGET_NAME)
    if(ENABLE_PCH AND TARGET ${TARGET_NAME})
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE /external:anglebrackets /external:W0)
        endif()
        target_precompile_headers(${TARGET_NAME} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:<windows.h>>
            $<$<COMPILE_LANGUAGE:CXX>:<unknwn.h>>
            $<$<COMPILE_LANGUAGE:CXX>:<wrl/client.h>>
            $<$<COMPILE_LANGUAGE:CXX>:<vector>>
            $<$<COMPILE_LANGUAGE:CXX>:<string>>
            $<$<COMPILE_LANGUAGE:CXX>:<string_view>>
            $<$<COMPILE_LANGUAGE:CXX>:<memory>>
            $<$<COMPILE_LANGUAGE:CXX>:<algorithm>>
            $<$<COMPILE_LANGUAGE:CXX>:<functional>>
            $<$<COMPILE_LANGUAGE:CXX>:<filesystem>>
            $<$<COMPILE_LANGUAGE:CXX>:<cmath>>
            $<$<COMPILE_LANGUAGE:CXX>:<cstdint>>
        )
        message(STATUS "[CppOpt] PCH enabled for target: ${TARGET_NAME}")
    endif()
endfunction()
