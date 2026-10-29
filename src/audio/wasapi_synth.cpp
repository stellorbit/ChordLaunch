#include "wasapi_synth.hpp"
#include <algorithm>

namespace clp {

WasapiSynth::WasapiSynth() = default;

WasapiSynth::~WasapiSynth() {
    Shutdown();
}

bool WasapiSynth::Initialize() {
    if (m_running.load()) return true;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&m_deviceEnumerator)
    );
    if (FAILED(hr)) return false;

    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_audioDevice);
    if (FAILED(hr)) return false;

    hr = m_audioDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&m_audioClient)
    );
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetMixFormat(&m_mixFormat);
    if (FAILED(hr)) return false;

    m_sampleRate = static_cast<float>(m_mixFormat->nSamplesPerSec);

    REFERENCE_TIME bufferDuration = 1000000; // 100ms (1000000 * 100ns)
    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        m_mixFormat,
        nullptr
    );
    if (FAILED(hr)) return false;

    m_audioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_audioEvent) return false;

    hr = m_audioClient->SetEventHandle(m_audioEvent);
    if (FAILED(hr)) return false;

    hr = m_audioClient->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&m_renderClient)
    );
    if (FAILED(hr)) return false;

    m_running.store(true);
    m_threadHandle = CreateThread(nullptr, 0, AudioThreadProc, this, 0, nullptr);
    if (!m_threadHandle) {
        m_running.store(false);
        return false;
    }

    m_audioClient->Start();
    return true;
}

void WasapiSynth::Shutdown() {
    m_running.store(false);
    if (m_audioEvent) {
        SetEvent(m_audioEvent);
    }
    if (m_threadHandle) {
        WaitForSingleObject(m_threadHandle, 1000);
        CloseHandle(m_threadHandle);
        m_threadHandle = nullptr;
    }
    if (m_audioClient) {
        m_audioClient->Stop();
    }
    if (m_renderClient) {
        m_renderClient->Release();
        m_renderClient = nullptr;
    }
    if (m_audioEvent) {
        CloseHandle(m_audioEvent);
        m_audioEvent = nullptr;
    }
    if (m_audioClient) {
        m_audioClient->Release();
        m_audioClient = nullptr;
    }
    if (m_mixFormat) {
        CoTaskMemFree(m_mixFormat);
        m_mixFormat = nullptr;
    }
    if (m_audioDevice) {
        m_audioDevice->Release();
        m_audioDevice = nullptr;
    }
    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
        m_deviceEnumerator = nullptr;
    }
}

void WasapiSynth::NoteOn(int note, float velocity) {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    // 空いているボイス、または最も音量の小さいボイスを探す
    int targetIdx = -1;
    float minLevel = 999.0f;
    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (!m_voices[i].active) {
            targetIdx = static_cast<int>(i);
            break;
        }
        if (m_voices[i].envLevel < minLevel) {
            minLevel = m_voices[i].envLevel;
            targetIdx = static_cast<int>(i);
        }
    }

    if (targetIdx >= 0) {
        m_voices[targetIdx].NoteOn(note, velocity, m_currentPreset, m_sampleRate);
    }
}

void WasapiSynth::NoteOff(int note) {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (m_voices[i].active && m_voices[i].noteNumber == note) {
            m_voices[i].NoteOff();
        }
    }
}

void WasapiSynth::PlayChord(const std::vector<int>& notes, float velocity) {
    AllNotesOff();
    for (int n : notes) {
        NoteOn(n, velocity);
    }
}

void WasapiSynth::AllNotesOff() {
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (m_voices[i].active) {
            m_voices[i].NoteOff();
        }
    }
}

void WasapiSynth::SetPreset(SynthPreset preset) {
    m_currentPreset = preset;
}

void WasapiSynth::SetMasterVolume(float vol) {
    m_masterVolume.store(std::clamp(vol, 0.0f, 1.0f));
}

DWORD WINAPI WasapiSynth::AudioThreadProc(LPVOID param) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto* self = reinterpret_cast<WasapiSynth*>(param);
    self->RenderLoop();
    CoUninitialize();
    return 0;
}

void WasapiSynth::RenderLoop() {
    UINT32 bufferFrameCount = 0;
    if (FAILED(m_audioClient->GetBufferSize(&bufferFrameCount))) return;

    bool isFloat = (m_mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                   (m_mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    WORD channels = m_mixFormat->nChannels;

    while (m_running.load()) {
        DWORD waitRes = WaitForSingleObject(m_audioEvent, 50);
        if (!m_running.load()) break;
        if (waitRes != WAIT_OBJECT_0) continue;

        UINT32 numPaddingFrames = 0;
        HRESULT hr = m_audioClient->GetCurrentPadding(&numPaddingFrames);
        if (FAILED(hr)) continue;

        UINT32 numFramesAvailable = bufferFrameCount - numPaddingFrames;
        if (numFramesAvailable == 0) continue;

        BYTE* pData = nullptr;
        hr = m_renderClient->GetBuffer(numFramesAvailable, &pData);
        if (FAILED(hr)) continue;

        float masterVol = m_masterVolume.load();

        {
            std::lock_guard<std::mutex> lock(m_voiceMutex);
            if (isFloat) {
                float* floatBuffer = reinterpret_cast<float*>(pData);
                for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                    float sample = 0.0f;
                    for (size_t v = 0; v < kMaxVoices; ++v) {
                        if (m_voices[v].active) {
                            sample += m_voices[v].ProcessSample();
                        }
                    }
                    sample *= masterVol;
                    sample = std::clamp(sample, -1.0f, 1.0f);

                    for (WORD ch = 0; ch < channels; ++ch) {
                        *floatBuffer++ = sample;
                    }
                }
            } else {
                // 16-bit PCM fallback
                int16_t* intBuffer = reinterpret_cast<int16_t*>(pData);
                for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                    float sample = 0.0f;
                    for (size_t v = 0; v < kMaxVoices; ++v) {
                        if (m_voices[v].active) {
                            sample += m_voices[v].ProcessSample();
                        }
                    }
                    sample *= masterVol;
                    sample = std::clamp(sample, -1.0f, 1.0f);
                    int16_t pcmVal = static_cast<int16_t>(sample * 32767.0f);

                    for (WORD ch = 0; ch < channels; ++ch) {
                        *intBuffer++ = pcmVal;
                    }
                }
            }
        }

        m_renderClient->ReleaseBuffer(numFramesAvailable, 0);
    }
}

} // namespace clp
