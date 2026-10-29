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
    float velocity = 0.8f;
    float envLevel = 0.0f;
    enum class EnvState { Idle, Attack, Decay, Sustain, Release } state = EnvState::Idle;
    float sampleRate = 48000.0f;
    SynthPreset preset = SynthPreset::GrandPiano;

    void NoteOn(int note, float vel, SynthPreset p, float sRate) {
        noteNumber = note;
        velocity = vel;
        preset = p;
        sampleRate = sRate;
        frequency = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
        phase = 0.0f;
        phase2 = 0.0f;
        phase3 = 0.0f;
        state = EnvState::Attack;
        active = true;
    }

    void NoteOff() {
        if (active && state != EnvState::Idle) {
            state = EnvState::Release;
        }
    }

    float ProcessSample() {
        if (!active || state == EnvState::Idle) return 0.0f;

        // ADSR エンベロープ計算
        float attackRate = 0.005f;  // 秒
        float decayRate = 0.8f;     // 秒
        float sustainLevel = 0.0f;  // ピアノは減衰
        float releaseRate = 0.3f;   // 秒

        if (preset == SynthPreset::WarmPad) {
            attackRate = 0.3f;
            decayRate = 0.5f;
            sustainLevel = 0.7f;
            releaseRate = 0.8f;
        } else if (preset == SynthPreset::Strings) {
            attackRate = 0.15f;
            decayRate = 0.4f;
            sustainLevel = 0.75f;
            releaseRate = 0.6f;
        } else if (preset == SynthPreset::ElectricPiano) {
            attackRate = 0.005f;
            decayRate = 1.2f;
            sustainLevel = 0.1f;
            releaseRate = 0.25f;
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
                    state = (sustainLevel > 0.01f) ? EnvState::Sustain : EnvState::Idle;
                    if (state == EnvState::Idle) active = false;
                }
                break;
            case EnvState::Sustain:
                // 維持
                break;
            case EnvState::Release:
                envLevel -= 1.0f / (releaseRate * sampleRate);
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
            // 基本波 + 倍音
            sample = std::sin(phase) * 0.6f + std::sin(phase * 2.0f) * 0.25f + std::sin(phase * 3.0f) * 0.15f;
        } else if (preset == SynthPreset::WarmPad) {
            // デチューン合算
            sample = (std::sin(phase) + std::sin(phase * 1.002f) + std::sin(phase * 0.998f)) * 0.33f;
        } else if (preset == SynthPreset::ElectricPiano) {
            // FM風倍音
            float mod = std::sin(phase * 3.0f) * 0.3f * envLevel;
            sample = std::sin(phase + mod) * 0.8f;
        } else if (preset == SynthPreset::Strings) {
            // 擬似ノコギリ波
            float saw = (phase / kTwoPi) * 2.0f - 1.0f;
            float saw2 = (phase2 / kTwoPi) * 2.0f - 1.0f;
            sample = (saw + saw2) * 0.4f;
            phase2 += (frequency * 1.004f * kTwoPi) / sampleRate;
            if (phase2 >= kTwoPi) phase2 -= kTwoPi;
        }

        phase += dPhase;
        if (phase >= kTwoPi) phase -= kTwoPi;

        return sample * envLevel * velocity;
    }
};

class WasapiSynth {
public:
    WasapiSynth();
    ~WasapiSynth();

    bool Initialize();
    void Shutdown();

    void NoteOn(int note, float velocity = 0.8f);
    void NoteOff(int note);
    void PlayChord(const std::vector<int>& notes, float velocity = 0.8f);
    void AllNotesOff();

    void SetPreset(SynthPreset preset);
    void SetMasterVolume(float vol);
    float GetMasterVolume() const { return m_masterVolume.load(); }

private:
    static DWORD WINAPI AudioThreadProc(LPVOID param);
    void RenderLoop();

    IMMDeviceEnumerator* m_deviceEnumerator = nullptr;
    IMMDevice* m_audioDevice = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    HANDLE m_audioEvent = nullptr;
    HANDLE m_threadHandle = nullptr;
    std::atomic<bool> m_running{false};

    WAVEFORMATEX* m_mixFormat = nullptr;
    float m_sampleRate = 48000.0f;

    static constexpr size_t kMaxVoices = 32;
    Voice m_voices[kMaxVoices];
    std::mutex m_voiceMutex;

    SynthPreset m_currentPreset = SynthPreset::GrandPiano;
    std::atomic<float> m_masterVolume{0.7f};
};

} // namespace clp
