#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <cmath>

namespace clp {

enum class SynthPreset : uint8_t {
    GrandPiano = 0,
    WarmPad,
    ElectricPiano,
    Strings
};

struct Voice {
    bool active = false;
    int noteNumber = 60;
    float frequency = 261.63f;
    float phase = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    float velocity = 0.85f;
    float envLevel = 0.0f;
    enum class EnvState { Idle, Attack, Decay, Sustain, Release, FastChoke } state = EnvState::Idle;
    float sampleRate = 48000.0f;
    SynthPreset preset = SynthPreset::GrandPiano;

    void NoteOn(int note, float vel, SynthPreset p, float sRate) {
        noteNumber = note;
        velocity = vel;
        preset = p;
        sampleRate = (sRate > 8000.0f) ? sRate : 48000.0f;
        frequency = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
        // ボイススティーリング時の急激な位相・音量リセットによるクリックノイズを抑制
        if (!active || state == EnvState::Idle) {
            phase = 0.0f;
            phase2 = 0.0f;
            phase3 = 0.0f;
            envLevel = 0.0f;
        }
        state = EnvState::Attack;
        active = true;
    }

    void NoteOff() {
        if (active && state != EnvState::Idle && state != EnvState::FastChoke) {
            state = EnvState::Release;
        }
    }

    void Choke() {
        if (active && state != EnvState::Idle) {
            state = EnvState::FastChoke;
        }
    }

    float ProcessSample() {
        if (!active || state == EnvState::Idle) return 0.0f;

        // ADSR エンベロープ計算 (和音同時発音でも濁らず自然に減衰する設定)
        float attackRate = 0.006f;   // 6ms
        float decayRate = 1.4f;      // 1.4秒で減衰
        float sustainLevel = 0.25f;  // 持続レベルを抑えて音割れ・被りを防止
        float releaseRate = 0.25f;   // 離鍵時 250ms で余韻消滅
        constexpr float chokeRate = 0.012f; // 連続試聴時は 12ms で高速かつ滑らかに消音

        if (preset == SynthPreset::WarmPad) {
            attackRate = 0.08f;
            decayRate = 1.0f;
            sustainLevel = 0.45f;
            releaseRate = 0.35f;
        } else if (preset == SynthPreset::Strings) {
            attackRate = 0.06f;
            decayRate = 0.8f;
            sustainLevel = 0.50f;
            releaseRate = 0.30f;
        } else if (preset == SynthPreset::ElectricPiano) {
            attackRate = 0.004f;
            decayRate = 1.2f;
            sustainLevel = 0.22f;
            releaseRate = 0.22f;
        }

        switch (state) {
            case EnvState::Attack:
                envLevel += 1.0f / (attackRate * sampleRate);
                if (envLevel >= 1.0f) {
                    envLevel = 1.0f;
                    state = EnvState::Decay;
                }
                break;
            case EnvState::Decay:
                envLevel -= (1.0f - sustainLevel) / (decayRate * sampleRate);
                if (envLevel <= sustainLevel) {
                    envLevel = sustainLevel;
                    state = (sustainLevel > 0.001f) ? EnvState::Sustain : EnvState::Idle;
                    if (state == EnvState::Idle) active = false;
                }
                break;
            case EnvState::Sustain:
                // 試聴時に音がいつまでも残らないよう、サステイン中も2.2秒で自然減衰して消える設計
                envLevel -= sustainLevel / (2.2f * sampleRate);
                if (envLevel <= 0.0005f) {
                    envLevel = 0.0f;
                    state = EnvState::Idle;
                    active = false;
                }
                break;
            case EnvState::Release:
                envLevel -= 1.0f / (releaseRate * sampleRate);
                if (envLevel <= 0.0001f) {
                    envLevel = 0.0f;
                    state = EnvState::Idle;
                    active = false;
                }
                break;
            case EnvState::FastChoke:
                envLevel -= 1.0f / (chokeRate * sampleRate);
                if (envLevel <= 0.0001f) {
                    envLevel = 0.0f;
                    state = EnvState::Idle;
                    active = false;
                }
                break;
            case EnvState::Idle:
                active = false;
                return 0.0f;
        }

        constexpr float kTwoPi = 6.28318530718f;
        float dPhase = (frequency * kTwoPi) / sampleRate;

        float sample = 0.0f;
        if (preset == SynthPreset::GrandPiano) {
            // アコースティックピアノ風合成 (高域倍音と低域かぶりを調整しクリアな和音に)
            sample = std::sin(phase) * 0.62f
                   + std::sin(phase * 2.0f) * 0.22f
                   + std::sin(phase * 3.0f) * 0.10f
                   + std::sin(phase * 4.0f) * 0.04f;
        } else if (preset == SynthPreset::WarmPad) {
            sample = (std::sin(phase) + std::sin(phase * 1.002f) + std::sin(phase * 0.998f)) * 0.30f;
        } else if (preset == SynthPreset::ElectricPiano) {
            float mod = std::sin(phase * 3.0f) * 0.28f * envLevel;
            sample = std::sin(phase + mod) * 0.75f + std::sin(phase * 2.0f) * 0.15f;
        } else if (preset == SynthPreset::Strings) {
            float saw = (phase / kTwoPi) * 2.0f - 1.0f;
            float saw2 = (phase2 / kTwoPi) * 2.0f - 1.0f;
            sample = (saw + saw2) * 0.32f;
            phase2 += (frequency * 1.004f * kTwoPi) / sampleRate;
            if (phase2 >= kTwoPi) phase2 -= kTwoPi;
        }

        phase += dPhase;
        if (phase >= kTwoPi) phase -= kTwoPi;

        // ポリフォニック和音（4〜6音）発音時にクリッピング（音割れ）しないよう1ボイスあたり0.20fのヘッドルーム係数を乗算
        return sample * envLevel * velocity * 0.20f;
    }
};

class WasapiSynth {
public:
    using Preset = SynthPreset;

    WasapiSynth();
    ~WasapiSynth();

    bool Initialize();
    void Shutdown();

    void NoteOn(int note, float velocity = 0.85f);
    void NoteOff(int note);
    void PlayChord(const std::vector<int>& notes, float velocity = 0.85f);
    void AllNotesOff();

    void SetPreset(SynthPreset preset);
    SynthPreset GetPreset() const { return m_currentPreset.load(); }
    void SetMasterVolume(float vol);
    float GetMasterVolume() const { return m_masterVolume.load(); }
    bool IsRunning() const { return m_running.load(); }

private:
    static DWORD WINAPI AudioThreadProc(LPVOID param);
    void ThreadMain();
    bool InitDeviceOnThread();
    void CleanupDeviceOnThread();
    void RenderLoop();

    IMMDeviceEnumerator* m_deviceEnumerator = nullptr;
    IMMDevice* m_audioDevice = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    HANDLE m_audioEvent = nullptr;
    HANDLE m_initDoneEvent = nullptr;
    HANDLE m_threadHandle = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initSuccess{false};
    bool m_isEventDriven = true;

    WAVEFORMATEX* m_mixFormat = nullptr;
    std::atomic<float> m_sampleRate{48000.0f};

    static constexpr size_t kMaxVoices = 32;
    Voice m_voices[kMaxVoices];
    std::mutex m_voiceMutex;

    std::atomic<SynthPreset> m_currentPreset{SynthPreset::GrandPiano};
    std::atomic<float> m_masterVolume{0.85f};
};

} // namespace clp
