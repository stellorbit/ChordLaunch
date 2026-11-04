#include "wasapi_synth.hpp"
#include <algorithm>

namespace clp {

WasapiSynth::WasapiSynth() = default;

WasapiSynth::~WasapiSynth() {
    Shutdown();
}

bool WasapiSynth::Initialize() {
    if (m_running.load()) return true;

    m_initSuccess.store(false);
    m_initDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_initDoneEvent) return false;

    m_running.store(true);
    m_threadHandle = CreateThread(nullptr, 0, AudioThreadProc, this, 0, nullptr);
    if (!m_threadHandle) {
        m_running.store(false);
        CloseHandle(m_initDoneEvent);
        m_initDoneEvent = nullptr;
        return false;
    }

    // ワーカースレッド上でのWASAPI初期化完了を待機 (最大2秒)
    WaitForSingleObject(m_initDoneEvent, 2000);
    CloseHandle(m_initDoneEvent);
    m_initDoneEvent = nullptr;

    if (!m_initSuccess.load()) {
        Shutdown();
        return false;
    }

    return true;
}

void WasapiSynth::Shutdown() {
    // RenderLoop は最大10msごとに m_running を確認するため、m_audioEvent への SetEvent は不要。
    // (オーディオスレッド側で m_audioEvent が閉じられる競合を避ける)
    m_running.store(false);
    if (m_threadHandle) {
        DWORD waitResult = WaitForSingleObject(m_threadHandle, 5000);
        // スレッドが終了していない場合にハンドルを閉じるとスレッドが使用中の状態を壊すため、閉じずに放棄する
        if (waitResult == WAIT_OBJECT_0) {
            CloseHandle(m_threadHandle);
        }
        m_threadHandle = nullptr;
    }
    if (m_initDoneEvent) {
        CloseHandle(m_initDoneEvent);
        m_initDoneEvent = nullptr;
    }
}

DWORD WINAPI WasapiSynth::AudioThreadProc(LPVOID param) {
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto* self = reinterpret_cast<WasapiSynth*>(param);
    self->ThreadMain();
    if (SUCCEEDED(hrCom)) {
        CoUninitialize();
    }
    return 0;
}

void WasapiSynth::ThreadMain() {
    bool ok = InitDeviceOnThread();
    m_initSuccess.store(ok);
    if (m_initDoneEvent) {
        SetEvent(m_initDoneEvent);
    }

    if (ok) {
        RenderLoop();
    }

    CleanupDeviceOnThread();
    m_running.store(false);
}

bool WasapiSynth::InitDeviceOnThread() {
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&m_deviceEnumerator)
    );
    if (FAILED(hr) || !m_deviceEnumerator) return false;

    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_audioDevice);
    if (FAILED(hr) || !m_audioDevice) return false;

    hr = m_audioDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(&m_audioClient)
    );
    if (FAILED(hr) || !m_audioClient) return false;

    hr = m_audioClient->GetMixFormat(&m_mixFormat);
    if (FAILED(hr) || !m_mixFormat) return false;

    m_sampleRate.store(static_cast<float>(m_mixFormat->nSamplesPerSec));

    // バッファ期間: 60ms (低遅延かつアンダーラン耐性の高いバランス)
    REFERENCE_TIME bufferDuration = 600000;

    // 1. イベント駆動モードで初期化を試行
    m_isEventDriven = true;
    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        m_mixFormat,
        nullptr
    );

    if (SUCCEEDED(hr)) {
        m_audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (m_audioEvent) {
            hr = m_audioClient->SetEventHandle(m_audioEvent);
            if (FAILED(hr)) {
                CloseHandle(m_audioEvent);
                m_audioEvent = nullptr;
                m_isEventDriven = false;
            }
        } else {
            m_isEventDriven = false;
        }
    } else {
        m_isEventDriven = false;
    }

    // 2. イベント駆動が拒否された場合はポーリング駆動へ自動フォールバック
    if (!m_isEventDriven) {
        if (m_audioClient) {
            m_audioClient->Release();
            m_audioClient = nullptr;
        }
        hr = m_audioDevice->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(&m_audioClient)
        );
        if (FAILED(hr) || !m_audioClient) return false;

        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            bufferDuration,
            0,
            m_mixFormat,
            nullptr
        );
        if (FAILED(hr)) return false;
    }

    hr = m_audioClient->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&m_renderClient)
    );
    if (FAILED(hr) || !m_renderClient) return false;

    // 起動前サイレント・プレロール (WASAPI共有モードでのストール防止に必須)
    UINT32 bufferFrameCount = 0;
    hr = m_audioClient->GetBufferSize(&bufferFrameCount);
    if (SUCCEEDED(hr) && bufferFrameCount > 0) {
        BYTE* pData = nullptr;
        hr = m_renderClient->GetBuffer(bufferFrameCount, &pData);
        if (SUCCEEDED(hr)) {
            m_renderClient->ReleaseBuffer(bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
        }
    }

    hr = m_audioClient->Start();
    return SUCCEEDED(hr);
}

void WasapiSynth::CleanupDeviceOnThread() {
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
    float sRate = m_sampleRate.load();
    SynthPreset preset = m_currentPreset.load();

    // すでに同一ノート番号が発音中であればそのボイスを再トリガー (位相維持でクリック防止)
    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (m_voices[i].active && m_voices[i].noteNumber == note) {
            m_voices[i].NoteOn(note, velocity, preset, sRate);
            return;
        }
    }

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
        m_voices[targetIdx].NoteOn(note, velocity, preset, sRate);
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
    std::lock_guard<std::mutex> lock(m_voiceMutex);
    float sRate = m_sampleRate.load();
    SynthPreset preset = m_currentPreset.load();

    // 連続して別コードを試聴した際、前のコードの余韻が被って濁らないよう高速チョーク消音 (12msフェードアウト)
    for (size_t i = 0; i < kMaxVoices; ++i) {
        if (m_voices[i].active) {
            bool inNewChord = false;
            for (int n : notes) {
                if (m_voices[i].noteNumber == n) {
                    inNewChord = true;
                    break;
                }
            }
            if (!inNewChord) {
                m_voices[i].Choke();
            }
        }
    }

    // 新規コード構成音を発音 (5音以上の多声和音時は音割れしないようベロシティを自動スケーリング)
    float scaledVel = velocity;
    if (notes.size() > 4) {
        scaledVel *= (4.0f / static_cast<float>(notes.size()));
    }

    for (int note : notes) {
        bool retriggered = false;
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (m_voices[i].active && m_voices[i].noteNumber == note && m_voices[i].state != Voice::EnvState::FastChoke) {
                m_voices[i].NoteOn(note, scaledVel, preset, sRate);
                retriggered = true;
                break;
            }
        }
        if (retriggered) continue;

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
            m_voices[targetIdx].NoteOn(note, scaledVel, preset, sRate);
        }
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
    m_currentPreset.store(preset);
}

void WasapiSynth::SetMasterVolume(float vol) {
    m_masterVolume.store(std::clamp(vol, 0.0f, 1.0f));
}

void WasapiSynth::RenderLoop() {
    UINT32 bufferFrameCount = 0;
    if (FAILED(m_audioClient->GetBufferSize(&bufferFrameCount)) || bufferFrameCount == 0) return;

    bool isFloat = (m_mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                   (m_mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    WORD channels = m_mixFormat->nChannels;
    WORD bitsPerSample = m_mixFormat->wBitsPerSample;

    while (m_running.load()) {
        if (m_isEventDriven && m_audioEvent) {
            // イベント待機 (10msタイムアウト時もそのままパディング確認へ進むハイブリッド設計)
            WaitForSingleObject(m_audioEvent, 10);
        } else {
            Sleep(8);
        }
        if (!m_running.load()) break;

        UINT32 numPaddingFrames = 0;
        HRESULT hr = m_audioClient->GetCurrentPadding(&numPaddingFrames);
        if (FAILED(hr)) continue;

        UINT32 numFramesAvailable = (bufferFrameCount > numPaddingFrames) ? (bufferFrameCount - numPaddingFrames) : 0;
        if (numFramesAvailable == 0) continue;

        BYTE* pData = nullptr;
        hr = m_renderClient->GetBuffer(numFramesAvailable, &pData);
        if (FAILED(hr) || !pData) continue;

        // 未対応フォーマットでは未初期化バッファを再生しないよう無音として返す
        const bool formatSupported = isFloat || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32;
        if (!formatSupported) {
            m_renderClient->ReleaseBuffer(numFramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
            continue;
        }

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
                    if (sample > 0.7f) sample = 0.7f + 0.25f * std::tanh((sample - 0.7f) / 0.25f);
                    else if (sample < -0.7f) sample = -0.7f + 0.25f * std::tanh((sample + 0.7f) / 0.25f);

                    for (WORD ch = 0; ch < channels; ++ch) {
                        *floatBuffer++ = sample;
                    }
                }
            } else if (bitsPerSample == 16) {
                int16_t* intBuffer = reinterpret_cast<int16_t*>(pData);
                for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                    float sample = 0.0f;
                    for (size_t v = 0; v < kMaxVoices; ++v) {
                        if (m_voices[v].active) {
                            sample += m_voices[v].ProcessSample();
                        }
                    }
                    sample *= masterVol;
                    if (sample > 0.7f) sample = 0.7f + 0.25f * std::tanh((sample - 0.7f) / 0.25f);
                    else if (sample < -0.7f) sample = -0.7f + 0.25f * std::tanh((sample + 0.7f) / 0.25f);
                    int16_t pcmVal = static_cast<int16_t>(std::clamp(sample * 32767.0f, -32767.0f, 32767.0f));

                    for (WORD ch = 0; ch < channels; ++ch) {
                        *intBuffer++ = pcmVal;
                    }
                }
            } else if (bitsPerSample == 24) {
                BYTE* byteBuffer = pData;
                for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                    float sample = 0.0f;
                    for (size_t v = 0; v < kMaxVoices; ++v) {
                        if (m_voices[v].active) {
                            sample += m_voices[v].ProcessSample();
                        }
                    }
                    sample *= masterVol;
                    if (sample > 0.7f) sample = 0.7f + 0.25f * std::tanh((sample - 0.7f) / 0.25f);
                    else if (sample < -0.7f) sample = -0.7f + 0.25f * std::tanh((sample + 0.7f) / 0.25f);
                    int32_t pcmVal = static_cast<int32_t>(std::clamp(sample * 8388607.0f, -8388607.0f, 8388607.0f));

                    for (WORD ch = 0; ch < channels; ++ch) {
                        *byteBuffer++ = static_cast<BYTE>(pcmVal & 0xFF);
                        *byteBuffer++ = static_cast<BYTE>((pcmVal >> 8) & 0xFF);
                        *byteBuffer++ = static_cast<BYTE>((pcmVal >> 16) & 0xFF);
                    }
                }
            } else if (bitsPerSample == 32) {
                int32_t* intBuffer = reinterpret_cast<int32_t*>(pData);
                for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                    float sample = 0.0f;
                    for (size_t v = 0; v < kMaxVoices; ++v) {
                        if (m_voices[v].active) {
                            sample += m_voices[v].ProcessSample();
                        }
                    }
                    sample *= masterVol;
                    if (sample > 0.7f) sample = 0.7f + 0.25f * std::tanh((sample - 0.7f) / 0.25f);
                    else if (sample < -0.7f) sample = -0.7f + 0.25f * std::tanh((sample + 0.7f) / 0.25f);
                    int32_t pcmVal = static_cast<int32_t>(std::clamp(sample * 2147483647.0f, -2147483647.0f, 2147483647.0f));

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
