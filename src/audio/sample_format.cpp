#include "audio/sample_format.h"

#include <algorithm>
#include <cstdint>

namespace audio {
namespace {

// Subformatos de WAVEFORMATEXTENSIBLE. Definidos aquí para no depender de ksmedia.h.
const GUID kSubtypePcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kSubtypeIeeeFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

template <typename T, typename Scale>
void MixInterleaved(const BYTE* data, UINT32 frames, int channels, std::vector<float>& out, Scale scale) {
    const T* s = reinterpret_cast<const T*>(data);
    const float inv = 1.0f / static_cast<float>(channels);
    for (UINT32 f = 0; f < frames; ++f) {
        float acc = 0.0f;
        for (int c = 0; c < channels; ++c) acc += scale(s[f * channels + c]);
        out[f] = acc * inv;
    }
}

} // namespace

SampleFormat DetectSampleFormat(const WAVEFORMATEX* wfx) {
    WORD tag = wfx->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (IsEqualGUID(ext->SubFormat, kSubtypeIeeeFloat)) tag = WAVE_FORMAT_IEEE_FLOAT;
        else if (IsEqualGUID(ext->SubFormat, kSubtypePcm)) tag = WAVE_FORMAT_PCM;
    }
    if (tag == WAVE_FORMAT_IEEE_FLOAT && wfx->wBitsPerSample == 32) return SampleFormat::Float32;
    if (tag == WAVE_FORMAT_PCM) {
        switch (wfx->wBitsPerSample) {
        case 16: return SampleFormat::Pcm16;
        case 24: return SampleFormat::Pcm24;
        case 32: return SampleFormat::Pcm32;
        default: break;
        }
    }
    return SampleFormat::Unknown;
}

const char* SampleFormatName(SampleFormat format) {
    switch (format) {
    case SampleFormat::Float32: return "float32";
    case SampleFormat::Pcm16: return "PCM 16 bits";
    case SampleFormat::Pcm24: return "PCM 24 bits";
    case SampleFormat::Pcm32: return "PCM 32 bits";
    default: return "desconocido";
    }
}

void ConvertToMono(const BYTE* data, UINT32 frames, int channels, SampleFormat format, bool silent, std::vector<float>& out) {
    out.resize(frames);
    if (silent || data == nullptr || channels <= 0) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }
    switch (format) {
    case SampleFormat::Float32:
        MixInterleaved<float>(data, frames, channels, out, [](float v) { return v; });
        break;
    case SampleFormat::Pcm16:
        MixInterleaved<int16_t>(data, frames, channels, out, [](int16_t v) { return v * (1.0f / 32768.0f); });
        break;
    case SampleFormat::Pcm32:
        MixInterleaved<int32_t>(data, frames, channels, out, [](int32_t v) { return v * (1.0f / 2147483648.0f); });
        break;
    case SampleFormat::Pcm24: {
        // Tres bytes little-endian con signo por muestra.
        const float inv = 1.0f / static_cast<float>(channels);
        for (UINT32 f = 0; f < frames; ++f) {
            float acc = 0.0f;
            for (int c = 0; c < channels; ++c) {
                const BYTE* b = data + (f * channels + c) * 3;
                int32_t v = static_cast<int32_t>(b[0]) | (static_cast<int32_t>(b[1]) << 8) | (static_cast<int32_t>(b[2]) << 16);
                if (v & 0x00800000) v |= static_cast<int32_t>(0xFF000000);
                acc += v * (1.0f / 8388608.0f);
            }
            out[f] = acc * inv;
        }
        break;
    }
    default:
        std::fill(out.begin(), out.end(), 0.0f);
        break;
    }
}

} // namespace audio
