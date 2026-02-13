#include "PcmDsd.hxx"
#include <stdint.h>
#include <array>
#include <span>

struct Modulator7 {
    float w[7] = {0.0f};
    const float a[7] = {1.0e-5f, 8.0e-5f, 6.0e-4f, 5.0e-3f, 4.0e-2f, 0.15f, 0.5f};

    inline bool Process(float input) noexcept {
        float x = input;
        float feedback = (w[6] > 0.0f) ? 1.0f : -1.0f;
        w[0] += x - feedback * a[0];
        w[1] += w[0] - feedback * a[1];
        w[2] += w[1] - feedback * a[2];
        w[3] += w[2] - feedback * a[3];
        w[4] += w[3] - feedback * a[4];
        w[5] += w[4] - feedback * a[5];
        w[6] += w[5] - feedback * a[6];
        return w[6] > 0.0f;
    }
};

static thread_local Modulator7 mod_l, mod_r;

void
PcmToDsd::Convert(std::span<const float> src, std::span<uint32_t> dst) noexcept
{
    // 动态倍率：out_rate 是我们在 ALSA 插件里强制设置的 352800 或 384000
    const unsigned ratio = this->out_rate / this->in_rate;
    const size_t num_frames = src.size() / 2;
    size_t dst_idx = 0;

    for (size_t i = 0; i < num_frames; ++i) {
        // -3dB Headroom 防止高阶系统饱和
        float s_l = src[i * 2] * 0.707f;
        float s_r = src[i * 2 + 1] * 0.707f;

        for (unsigned r = 0; r < ratio; ++r) {
            uint32_t dsd_l = 0, dsd_r = 0;
            // Native DSD U32BE 打包
            for (int b = 31; b >= 0; --b) {
                if (mod_l.Process(s_l)) dsd_l |= (1u << b);
                if (mod_r.Process(s_r)) dsd_r |= (1u << b);
            }
            dst[dst_idx++] = dsd_l;
            dst[dst_idx++] = dsd_r;
        }
    }
}
