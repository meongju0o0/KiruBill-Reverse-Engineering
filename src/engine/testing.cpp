#include "testing.h"
#include "engine.h"
#include "platform.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct ScriptKey { long poll; int vk; int down; };
std::vector<ScriptKey> g_script;
size_t g_script_pos = 0;
long g_poll = 0;
uint8_t g_script_keys[256];

std::string g_shot_dir;
int g_shot_every = 0;
int g_present = 0;

std::vector<uint32_t> g_find_ref;
long g_find_best = -1, g_find_best_diff = 1L << 30;

bool read_bmp24(const char* path, std::vector<uint32_t>& px)
{
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    if (d.size() < 54 || d[0] != 'B' || d[28] != 24)
        return false;
    uint32_t off = d[10] | (d[11] << 8) | (d[12] << 16);
    px.assign(GFX_W * GFX_H, 0);
    for (int y = 0; y < GFX_H; ++y)
        for (int x = 0; x < GFX_W; ++x) {
            size_t p = off + (size_t)(GFX_H - 1 - y) * GFX_W * 3 + (size_t)x * 3;
            if (p + 2 < d.size())
                px[(size_t)y * GFX_W + x] = d[p] | (d[p + 1] << 8) | (d[p + 2] << 16);
        }
    return true;
}

void write_bmp24(const std::string& path, const uint8_t* idx, const uint32_t* pal)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return;
    uint8_t h[54] = {'B', 'M'};
    auto put32 = [&](int o, uint32_t v) {
        h[o] = v & 255;
        h[o + 1] = (v >> 8) & 255;
        h[o + 2] = (v >> 16) & 255;
        h[o + 3] = v >> 24;
    };
    put32(2, 54 + GFX_W * GFX_H * 3);
    put32(10, 54);
    put32(14, 40);
    put32(18, GFX_W);
    put32(22, GFX_H);
    h[26] = 1;
    h[28] = 24;
    put32(34, GFX_W * GFX_H * 3);
    std::fwrite(h, 1, 54, f);
    std::vector<uint8_t> row(GFX_W * 3);
    for (int y = GFX_H - 1; y >= 0; --y) {
        for (int x = 0; x < GFX_W; ++x) {
            uint32_t c = pal[idx[y * GFX_W + x]];
            row[x * 3] = c & 255;
            row[x * 3 + 1] = (c >> 8) & 255;
            row[x * 3 + 2] = (c >> 16) & 255;
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
}

} // namespace

void test_parse_args(const std::vector<std::string>& args)
{
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        const char* next = i + 1 < args.size() ? args[i + 1].c_str() : nullptr;
        if (a == "-window" || a == "--window" || a == "--windowed" || a == "-w")
            g_opts.windowed = true;
        else if (a == "--fullscreen" || a == "-f")
            g_opts.windowed = false;
        else if (a == "--gdi")
            g_opts.force_gdi = true;
        else if (a == "--headless")
            g_opts.headless = true;
        else if (a == "--trace")
            g_opts.trace = true;
        else if (a == "--frames" && next) {
            g_opts.max_frames = std::atoi(next);
            ++i;
        } else if (a == "--shots" && next) {
            g_shot_dir = next;
            ++i;
        } else if (a == "--every" && next) {
            g_shot_every = std::atoi(next);
            ++i;
        } else if (a == "--find" && next) {
            read_bmp24(next, g_find_ref);
            ++i;
        } else if (a == "--input" && next) {
            FILE* f = std::fopen(next, "r");
            long p;
            int vk, d;
            while (f && std::fscanf(f, "%ld %i %d", &p, &vk, &d) == 3)
                g_script.push_back({p, vk, d});
            if (f)
                std::fclose(f);
            std::stable_sort(g_script.begin(), g_script.end(),
                             [](const ScriptKey& x, const ScriptKey& y) { return x.poll < y.poll; });
            ++i;
        }
    }
    if (!g_shot_dir.empty() && g_shot_every <= 0)
        g_shot_every = 1;
}

void test_apply_keyboard(uint8_t keys[256])
{
    if (g_script.empty())
        return;
    ++g_poll;
    while (g_script_pos < g_script.size() && g_script[g_script_pos].poll <= g_poll) {
        const ScriptKey& k = g_script[g_script_pos++];
        g_script_keys[k.vk & 255] = k.down ? 0x80 : 0;
    }
    for (int i = 0; i < 256; ++i)
        keys[i] |= g_script_keys[i];
}

void test_on_present(const uint8_t* idx, const uint32_t* pal)
{
    ++g_present;
    if (!g_shot_dir.empty() && g_present % g_shot_every == 0) {
        char name[64];
        std::snprintf(name, sizeof(name), "/frame_%06d.bmp", g_present);
        write_bmp24(g_shot_dir + name, idx, pal);
    }
    if (!g_find_ref.empty()) {
        long diff = 0;
        for (int i = 0; i < GFX_W * GFX_H; ++i)
            diff += ((pal[idx[i]] & 0xffffff) != g_find_ref[i]);
        if (diff < g_find_best_diff) {
            g_find_best_diff = diff;
            g_find_best = g_present;
        }
    }
}

void test_report()
{
    if (!g_find_ref.empty())
        std::printf("best match: presented frame %ld, %ld differing pixels\n", g_find_best, g_find_best_diff);
    std::fflush(stdout);
}
