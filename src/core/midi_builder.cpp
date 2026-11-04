#include "midi_builder.hpp"
#include "music_engine.hpp"
#include <windows.h>
#include <fstream>
#include <algorithm>

namespace clp {

void MidiBuilder::WriteBe16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void MidiBuilder::WriteBe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void MidiBuilder::WriteVariableLength(std::vector<uint8_t>& out, uint32_t value) {
    uint32_t buffer = value & 0x7F;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    while (true) {
        out.push_back(static_cast<uint8_t>(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

std::vector<uint8_t> MidiBuilder::BuildMidi(
    const std::vector<Bar>& bars,
    int bpm,
    VoicingStyle voicing,
    int baseOctave,
    int velocity,
    int bassOctaveMode
) {
    std::vector<uint8_t> midiData;

    // SMF Header Chunk: 'MThd', len=6, format=0, tracks=1, division=480 ticks/quarter
    midiData.push_back('M'); midiData.push_back('T'); midiData.push_back('h'); midiData.push_back('d');
    WriteBe32(midiData, 6);
    WriteBe16(midiData, 0);   // Format 0
    WriteBe16(midiData, 1);   // 1 track
    WriteBe16(midiData, static_cast<uint16_t>(TICKS_PER_BEAT)); // 480 TPQN

    // Track Chunk Data
    std::vector<uint8_t> trackBytes;

    // 1. Time Signature Meta Event (4/4)
    WriteVariableLength(trackBytes, 0); // delta 0
    trackBytes.push_back(0xFF);
    trackBytes.push_back(0x58);
    trackBytes.push_back(0x04);
    trackBytes.push_back(0x04); // numerator 4
    trackBytes.push_back(0x02); // denominator 2^2 = 4
    trackBytes.push_back(0x18); // 24 MIDI clocks per metronome click
    trackBytes.push_back(0x08); // 8 32nd notes per 24 MIDI clocks

    // 2. Set Tempo Meta Event
    uint32_t usPerQuarter = (bpm > 0) ? (60000000 / bpm) : 500000;
    WriteVariableLength(trackBytes, 0);
    trackBytes.push_back(0xFF);
    trackBytes.push_back(0x51);
    trackBytes.push_back(0x03);
    trackBytes.push_back(static_cast<uint8_t>((usPerQuarter >> 16) & 0xFF));
    trackBytes.push_back(static_cast<uint8_t>((usPerQuarter >> 8) & 0xFF));
    trackBytes.push_back(static_cast<uint8_t>(usPerQuarter & 0xFF));

    // 3. Track Name Meta Event ("ChordLaunch")
    WriteVariableLength(trackBytes, 0);
    trackBytes.push_back(0xFF);
    trackBytes.push_back(0x03);
    std::string trackName = "ChordLaunch";
    WriteVariableLength(trackBytes, static_cast<uint32_t>(trackName.size()));
    trackBytes.insert(trackBytes.end(), trackName.begin(), trackName.end());

    // 各小節のコードをノートイベントに変換 (480 ticks = 1 beat)
    uint32_t pendingDeltaTicks = 0;

    for (const auto& bar : bars) {
        // セクションマーカー / 転調マーカー
        if (!bar.sectionMarker.empty()) {
            WriteVariableLength(trackBytes, pendingDeltaTicks);
            pendingDeltaTicks = 0;
            trackBytes.push_back(0xFF);
            trackBytes.push_back(0x06); // Marker
            WriteVariableLength(trackBytes, static_cast<uint32_t>(bar.sectionMarker.size()));
            trackBytes.insert(trackBytes.end(), bar.sectionMarker.begin(), bar.sectionMarker.end());
        }
        if (!bar.keyChange.empty()) {
            WriteVariableLength(trackBytes, pendingDeltaTicks);
            pendingDeltaTicks = 0;
            trackBytes.push_back(0xFF);
            trackBytes.push_back(0x01); // Text Meta Event
            std::string text = "Key: " + bar.keyChange;
            WriteVariableLength(trackBytes, static_cast<uint32_t>(text.size()));
            trackBytes.insert(trackBytes.end(), text.begin(), text.end());
        }

        for (const auto& chord : bar.chords) {
            uint32_t durationTicks = chord.durationTicks > 0
                ? static_cast<uint32_t>(chord.durationTicks)
                : static_cast<uint32_t>(chord.durationBeats * TICKS_PER_BEAT);

            // 休符の場合はデルタタイムを加算蓄積
            if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
                pendingDeltaTicks += durationTicks;
                continue;
            }

            auto notes = MusicEngine::GetChordMidiNotes(chord, baseOctave, voicing, chord.inversion, bassOctaveMode);
            if (notes.empty()) {
                pendingDeltaTicks += durationTicks;
                continue;
            }

            // Note On (最初のノートに pendingDeltaTicks を適用)
            for (size_t ni = 0; ni < notes.size(); ++ni) {
                WriteVariableLength(trackBytes, (ni == 0) ? pendingDeltaTicks : 0);
                trackBytes.push_back(0x90); // Note On Ch 1
                trackBytes.push_back(static_cast<uint8_t>(std::clamp(notes[ni], 0, 127)));
                trackBytes.push_back(static_cast<uint8_t>(std::clamp(velocity, 1, 127)));
            }
            pendingDeltaTicks = 0;

            // Note Off (ゲートタイムは 95% = durationTicks * 95 / 100)
            uint32_t gateTicks = (durationTicks * 95) / 100;
            if (gateTicks == 0) gateTicks = 1;
            uint32_t restTicks = durationTicks > gateTicks ? (durationTicks - gateTicks) : 0;

            for (size_t ni = 0; ni < notes.size(); ++ni) {
                WriteVariableLength(trackBytes, (ni == 0) ? gateTicks : 0);
                trackBytes.push_back(0x80); // Note Off Ch 1
                trackBytes.push_back(static_cast<uint8_t>(std::clamp(notes[ni], 0, 127)));
                trackBytes.push_back(0);
            }

            // 残りの余白時間 (CC#123 を出さずに次のイベントへ繰り越し)
            pendingDeltaTicks += restTicks;
        }
    }

    // End of Track Meta Event: FF 2F 00
    WriteVariableLength(trackBytes, pendingDeltaTicks);
    trackBytes.push_back(0xFF);
    trackBytes.push_back(0x2F);
    trackBytes.push_back(0x00);

    // Track Chunk Header: 'MTrk', len
    midiData.push_back('M'); midiData.push_back('T'); midiData.push_back('r'); midiData.push_back('k');
    WriteBe32(midiData, static_cast<uint32_t>(trackBytes.size()));
    midiData.insert(midiData.end(), trackBytes.begin(), trackBytes.end());

    return midiData;
}

std::wstring MidiBuilder::ExportToTempFile(
    const std::vector<Bar>& bars,
    int bpm,
    VoicingStyle voicing,
    const std::wstring& filenamePrefix,
    int bassOctaveMode
) {
    auto midiBytes = BuildMidi(bars, bpm, voicing, 4, 90, bassOctaveMode);
    if (midiBytes.empty()) return L"";

    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);

    std::wstring filePath = std::wstring(tempPath) + filenamePrefix + L"_" + std::to_wstring(GetTickCount64()) + L".mid";

    // 過去の一時MIDIファイル (24時間以上前) を削除してTempの肥大化を防ぐ
    {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((std::wstring(tempPath) + filenamePrefix + L"_*.mid").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FILETIME nowFt;
            GetSystemTimeAsFileTime(&nowFt);
            ULARGE_INTEGER nowUi;
            nowUi.LowPart = nowFt.dwLowDateTime;
            nowUi.HighPart = nowFt.dwHighDateTime;
            constexpr ULONGLONG kDay100ns = 24ULL * 3600ULL * 10000000ULL;
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                ULARGE_INTEGER wt;
                wt.LowPart = fd.ftLastWriteTime.dwLowDateTime;
                wt.HighPart = fd.ftLastWriteTime.dwHighDateTime;
                if (nowUi.QuadPart > wt.QuadPart && nowUi.QuadPart - wt.QuadPart > kDay100ns) {
                    DeleteFileW((std::wstring(tempPath) + fd.cFileName).c_str());
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs.is_open()) return L"";

    ofs.write(reinterpret_cast<const char*>(midiBytes.data()), static_cast<std::streamsize>(midiBytes.size()));
    ofs.close();
    if (ofs.fail()) return L"";

    return filePath;
}

} // namespace clp
