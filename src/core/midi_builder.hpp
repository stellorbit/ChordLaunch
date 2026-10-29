#pragma once
#include "music_theory.hpp"
#include <vector>
#include <cstdint>
#include <string>

namespace clp {

class MidiBuilder {
public:
    // 小節列から標準MIDIファイル(SMF Format 0, 480 TPQN)のバイナリデータを生成
    static std::vector<uint8_t> BuildMidi(
        const std::vector<Bar>& bars,
        int bpm = 120,
        VoicingStyle voicing = VoicingStyle::RootPosition,
        int baseOctave = 4,
        int velocity = 90
    );

    // 一時ファイルとして .mid ファイルを書き出し、ファイルパスを返却（DAWドラッグ用）
    static std::wstring ExportToTempFile(
        const std::vector<Bar>& bars,
        int bpm = 120,
        VoicingStyle voicing = VoicingStyle::RootPosition,
        const std::wstring& filenamePrefix = L"ChordLaunchpad_Progression"
    );

private:
    static void WriteVariableLength(std::vector<uint8_t>& out, uint32_t value);
    static void WriteBe16(std::vector<uint8_t>& out, uint16_t value);
    static void WriteBe32(std::vector<uint8_t>& out, uint32_t value);
};

} // namespace clp
