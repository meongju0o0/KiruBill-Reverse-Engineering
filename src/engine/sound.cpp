// Sound effects – replacement for the DirectSound wrapper (0x413220..0x413760).
// WAV files are parsed like the original mmio code (RIFF/WAVE, "fmt ", "data")
// and handed to the platform backend as static buffers.
#include "engine.h"
#include "platform.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
bool g_snd_ok = false;
bool g_loaded[64];

uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool parse_wav(const char* path, PcmSound& out)
{
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::vector<uint8_t> d;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        d.resize((size_t)n);
        if (std::fread(d.data(), 1, d.size(), f) != d.size())
            d.clear();
    }
    std::fclose(f);
    if (d.size() < 12 || std::memcmp(&d[0], "RIFF", 4) || std::memcmp(&d[8], "WAVE", 4))
        return false;
    size_t p = 12;
    bool have_fmt = false;
    while (p + 8 <= d.size()) {
        uint32_t len = rd32(&d[p + 4]);
        const uint8_t* body = &d[p + 8];
        size_t avail = d.size() - (p + 8);
        if (!std::memcmp(&d[p], "fmt ", 4) && len >= 16 && avail >= 16) {
            if (rd16(body) != 1)   // WAVE_FORMAT_PCM only
                return false;
            out.channels = rd16(body + 2);
            out.rate = (int)rd32(body + 4);
            out.bits = rd16(body + 14);
            have_fmt = true;
        } else if (!std::memcmp(&d[p], "data", 4) && have_fmt) {
            size_t take = len < avail ? len : avail;
            out.data.assign(body, body + take);
            return true;
        }
        p += 8 + len + (len & 1);
    }
    return false;
}
} // namespace

bool snd_init()
{
    g_snd_ok = plat_snd_init();
    std::memset(g_loaded, 0, sizeof(g_loaded));
    return true;  // the game never fails without a sound card in the port
}

void snd_shutdown()
{
    for (int i = 0; i < 64; ++i)
        snd_unload(i);
    plat_snd_shutdown();
    g_snd_ok = false;
}

bool snd_load(const char* wav, int idx)
{
    if (idx < 0 || idx >= 64)
        return false;
    snd_unload(idx);
    PcmSound pcm;
    if (!parse_wav(wav, pcm)) {
        std::fprintf(stderr, "snd: cannot load %s\n", wav);
        return false;
    }
    if (!g_snd_ok)
        return true;
    g_loaded[idx] = plat_snd_create(idx, pcm);
    return g_loaded[idx];
}

void snd_play(int idx)
{
    if (g_snd_ok && idx >= 0 && idx < 64 && g_loaded[idx])
        plat_snd_play(idx);
}

void snd_stop(int idx)
{
    if (g_snd_ok && idx >= 0 && idx < 64 && g_loaded[idx])
        plat_snd_stop(idx);
}

void snd_unload(int idx)
{
    if (idx >= 0 && idx < 64 && g_loaded[idx]) {
        plat_snd_stop(idx);
        plat_snd_release(idx);
        g_loaded[idx] = false;
    }
}
