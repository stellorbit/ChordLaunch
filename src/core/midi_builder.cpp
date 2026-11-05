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
    int bassOctaveMode,
    const std::vector<PartMarker>* markers
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

    // 全イベントを絶対 tick で収集し、tick 順（同 tick は メタ → Note Off → Note On）に並べてから
    // デルタタイムへ変換する。これにより小節途中のマーカーも正確な位置に出力できる。
    struct MidiEvent {
        uint32_t tick = 0;
        int priority = 0; // 0: メタ, 1: Note Off, 2: Note On
        std::vector<uint8_t> bytes;
    };
    std::vector<MidiEvent> events;

    auto addMeta = [&events](uint32_t tick, uint8_t type, const std::string& text) {
        MidiEvent ev;
        ev.tick = tick;
        ev.priority = 0;
        ev.bytes.push_back(0xFF);
        ev.bytes.push_back(type);
        // 可変長の長さは呼び出し側で後付けできないため、ここで直接エンコードする
        uint32_t len = static_cast<uint32_t>(text.size());
        uint8_t buf[5];
        int n = 0;
        buf[n++] = static_cast<uint8_t>(len & 0x7F);
        while ((len >>= 7) > 0) buf[n++] = static_cast<uint8_t>((len & 0x7F) | 0x80);
        while (n > 0) ev.bytes.push_back(buf[--n]);
        ev.bytes.insert(ev.bytes.end(), text.begin(), text.end());
        events.push_back(std::move(ev));
    };

    uint32_t curTick = 0;
    for (const auto& bar : bars) {
        // セクションマーカー / 転調マーカー
        if (!bar.sectionMarker.empty()) addMeta(curTick, 0x06, bar.sectionMarker); // Marker
        if (!bar.keyChange.empty()) addMeta(curTick, 0x01, "Key: " + bar.keyChange); // Text

        for (const auto& chord : bar.chords) {
            uint32_t durationTicks = chord.durationTicks > 0
                ? static_cast<uint32_t>(chord.durationTicks)
                : static_cast<uint32_t>(chord.durationBeats * TICKS_PER_BEAT);

            if (chord.isRest || chord.rootPitch == PitchClass::Unknown) {
                curTick += durationTicks;
                continue;
            }

            auto notes = MusicEngine::GetChordMidiNotes(chord, baseOctave, voicing, chord.inversion, bassOctaveMode);
            if (notes.empty()) {
                curTick += durationTicks;
                continue;
            }

            // ゲートタイムは 95% = durationTicks * 95 / 100
            uint32_t gateTicks = (durationTicks * 95) / 100;
            if (gateTicks == 0) gateTicks = 1;

            for (int note : notes) {
                MidiEvent on;
                on.tick = curTick;
                on.priority = 2;
                on.bytes = { 0x90, static_cast<uint8_t>(std::clamp(note, 0, 127)), static_cast<uint8_t>(std::clamp(velocity, 1, 127)) };
                events.push_back(std::move(on));
            }
            for (int note : notes) {
                MidiEvent off;
                off.tick = curTick + gateTicks;
                off.priority = 1;
                off.bytes = { 0x80, static_cast<uint8_t>(std::clamp(note, 0, 127)), 0 };
                events.push_back(std::move(off));
            }
            curTick += durationTicks;
        }
    }

    // パート内の任意位置のマーカー（マージ後の絶対 tick）
    if (markers) {
        for (const auto& m : *markers) {
            if (m.name.empty() || m.tick < 0) continue;
            addMeta(static_cast<uint32_t>(m.tick), 0x06, m.name);
        }
    }

    std::stable_sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.priority < b.priority;
    });

    uint32_t lastTick = 0;
    for (const auto& ev : events) {
        WriteVariableLength(trackBytes, ev.tick - lastTick);
        lastTick = ev.tick;
        trackBytes.insert(trackBytes.end(), ev.bytes.begin(), ev.bytes.end());
    }

    // End of Track Meta Event: FF 2F 00（最後のイベント以降の余白を含む）
    WriteVariableLength(trackBytes, curTick > lastTick ? (curTick - lastTick) : 0);
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
    int bassOctaveMode,
    const std::vector<PartMarker>* markers
) {
    auto midiBytes = BuildMidi(bars, bpm, voicing, 4, 90, bassOctaveMode, markers);
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
