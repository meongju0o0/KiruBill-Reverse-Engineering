// 8-bit DirectDraw renderer replacement (original class at 0x41fa98).
//
// Behaviour reproduced from the original:
//  * one 640x480x8 primary + 1 back buffer flip chain (Flip swaps contents)
//  * BltFast with source color key; BltFast/Blt do not clip, an invalid
//    rectangle makes the whole call fail (nothing is drawn).  The game's own
//    wrapper (0x411110) clips against the screen before calling BltFast.
//  * GDI color matching (SetPixel/GetPixel round trip, BitBlt of DIBs into the
//    palettised surface) = nearest palette entry.
#include "engine.h"
#include "platform.h"
#include "testing.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

struct Surface {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    uint32_t key = 0;        // color key pixel (index)
    bool has_key = false;
    std::string file;
};

std::unique_ptr<Surface> g_surf[GFX_MAX_SURFACES];
uint8_t  g_buf[2][GFX_W * GFX_H];
int      g_back = 0;
uint32_t g_pal[256];            // displayed DirectDraw palette, RGBQUAD (B | G<<8 | R<<16)
uint32_t g_base_pal[256];       // gfx+0x21c: palette loaded from game.pal
bool     g_draw = true;         // gfx+0x61c

inline uint8_t* back() { return g_buf[g_back]; }

int nearest_index(int r, int g, int b)
{
    int best = 0;
    long bestd = 0x7fffffff;
    for (int i = 0; i < 256; ++i) {
        int pr = (g_pal[i] >> 16) & 0xff, pg = (g_pal[i] >> 8) & 0xff, pb = g_pal[i] & 0xff;
        long d = (long)(pr - r) * (pr - r) + (long)(pg - g) * (pg - g) + (long)(pb - b) * (pb - b);
        if (d < bestd) {
            bestd = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    return best;
}

inline uint32_t colorref_to_index(uint32_t c)
{
    return (uint32_t)nearest_index(c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff);
}

// Minimal BMP reader (BI_RGB, 1/4/8/24/32 bpp, bottom-up or top-down).
struct Bmp {
    int w = 0, h = 0, bpp = 0;
    uint32_t pal[256] = {};
    int ncolors = 0;
    std::vector<uint8_t> bits;  // top-down rows, `stride` bytes each
    int stride = 0;
};

uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool read_file(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    size_t got = n > 0 ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == out.size();
}

bool load_bmp(const char* path, Bmp& b)
{
    std::vector<uint8_t> d;
    if (!read_file(path, d) || d.size() < 54 || d[0] != 'B' || d[1] != 'M')
        return false;
    uint32_t off = rd32(&d[10]);
    uint32_t hs = rd32(&d[14]);
    if (hs < 40)
        return false;
    int32_t w = (int32_t)rd32(&d[18]);
    int32_t h = (int32_t)rd32(&d[22]);
    b.bpp = rd16(&d[28]);
    uint32_t comp = rd32(&d[30]);
    uint32_t clr = rd32(&d[46]);
    if (comp != 0 || w <= 0 || h == 0)
        return false;
    bool topdown = h < 0;
    if (h < 0)
        h = -h;
    b.w = w;
    b.h = h;
    if (b.bpp <= 8) {
        b.ncolors = clr ? (int)clr : (1 << b.bpp);
        if (b.ncolors > 256)
            b.ncolors = 256;
        for (int i = 0; i < b.ncolors; ++i) {
            size_t p = 14 + hs + (size_t)i * 4;
            if (p + 4 > d.size())
                return false;
            b.pal[i] = rd32(&d[p]) & 0xffffff;
        }
    }
    int stride = ((w * b.bpp + 31) / 32) * 4;
    b.stride = stride;
    if ((size_t)off + (size_t)stride * h > d.size())
        return false;
    b.bits.resize((size_t)stride * h);
    for (int y = 0; y < h; ++y) {
        int sy = topdown ? y : (h - 1 - y);
        std::memcpy(&b.bits[(size_t)y * stride], &d[off + (size_t)sy * stride], stride);
    }
    return true;
}

// Convert a BMP into palette indices of the current palette (GDI BitBlt).
void bmp_to_surface(const Bmp& b, Surface& s)
{
    uint8_t lut[256];
    if (b.bpp <= 8)
        for (int i = 0; i < 256; ++i) {
            uint32_t c = i < b.ncolors ? b.pal[i] : 0;
            lut[i] = (uint8_t)nearest_index((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
        }
    int cw = b.w < s.w ? b.w : s.w;
    int ch = b.h < s.h ? b.h : s.h;
    for (int y = 0; y < ch; ++y) {
        const uint8_t* row = &b.bits[(size_t)y * b.stride];
        uint8_t* dst = &s.px[(size_t)y * s.w];
        for (int x = 0; x < cw; ++x) {
            switch (b.bpp) {
            case 8: dst[x] = lut[row[x]]; break;
            case 4: dst[x] = lut[(row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15]; break;
            case 1: dst[x] = lut[(row[x >> 3] >> (7 - (x & 7))) & 1]; break;
            case 24: dst[x] = (uint8_t)nearest_index(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]); break;
            case 32: dst[x] = (uint8_t)nearest_index(row[x * 4 + 2], row[x * 4 + 1], row[x * 4]); break;
            default: dst[x] = 0; break;
            }
        }
    }
}

// Blt/BltFast rectangle validation (no clipper attached in the original).
inline bool rect_ok(int x, int y, int w, int h, int sw, int sh)
{
    return w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= sw && y + h <= sh;
}

void blt_fast(uint8_t* dst, int dw, int dh, int x, int y, const Surface& s, int sx, int sy, int w, int h)
{
    if (!rect_ok(x, y, w, h, dw, dh) || !rect_ok(sx, sy, w, h, s.w, s.h))
        return;
    for (int j = 0; j < h; ++j) {
        const uint8_t* sp = &s.px[(size_t)(sy + j) * s.w + sx];
        uint8_t* dp = &dst[(size_t)(y + j) * dw + x];
        if (s.has_key) {
            const uint8_t key = (uint8_t)s.key;
            for (int i = 0; i < w; ++i)
                if (sp[i] != key)
                    dp[i] = sp[i];
        } else {
            std::memcpy(dp, sp, (size_t)w);
        }
    }
}

void color_fill(uint8_t* dst, int dw, int dh, int x, int y, int w, int h, uint32_t pixel)
{
    if (!rect_ok(x, y, w, h, dw, dh))
        return;
    for (int j = 0; j < h; ++j)
        std::memset(&dst[(size_t)(y + j) * dw + x], (int)(pixel & 0xff), (size_t)w);
}

} // namespace

// ---------------------------------------------------------------------------

bool gfx_init()
{
    std::memset(g_buf, 0, sizeof(g_buf));
    g_back = 0;
    // Initial 3-3-2 palette set by 0x410cf0 before game.pal is loaded.
    for (int j = 0; j < 256; ++j) {
        int r = ((j >> 5) & 7) * 255 / 7;
        int g = ((j >> 2) & 7) * 255 / 7;
        int b = (j & 3) * 255 / 3;
        g_pal[j] = (uint32_t)(b | (g << 8) | (r << 16));
    }
    g_pal[0] = 0x000000;
    g_pal[255] = 0xffffff;
    g_draw = true;
    return true;
}

void gfx_shutdown()
{
    for (auto& s : g_surf)
        s.reset();
}

void gfx_set_draw_enabled(bool on) { g_draw = on; }
bool gfx_draw_enabled() { return g_draw; }
uint8_t* gfx_backbuffer() { return back(); }

// 0x411110
int gfx_blit(int surf, int x, int y, int w, int h, int sx, int sy)
{
    if (!g_draw)
        return 1;
    const int sw = GFX_W, sh = GFX_H;
    if (x + w < 0)
        return 0;
    if (x > sw)
        return 0;
    if (y + h < 0)
        return 0;
    if (y > sh)
        return 0;
    if (x + w > sw)
        w = sw - x;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y + h > sh)
        h = sh - y;
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    if (surf >= 0 && surf < GFX_MAX_SURFACES && g_surf[surf])
        blt_fast(back(), GFX_W, GFX_H, x, y, *g_surf[surf], sx, sy, w, h);
    return 1;
}

// 0x411410
uint32_t gfx_rgb_to_pixel(uint32_t colorref) { return colorref_to_index(colorref); }

// 0x411260
int gfx_clear_rgb(uint32_t colorref)
{
    if (!g_draw)
        return 1;
    std::memset(back(), (int)colorref_to_index(colorref), GFX_W * GFX_H);
    return 1;
}

// 0x4112d0
int gfx_fill_rgb(int x, int y, int w, int h, uint32_t colorref)
{
    if (!g_draw)
        return 1;
    color_fill(back(), GFX_W, GFX_H, x, y, w, h, colorref_to_index(colorref));
    return 1;
}

// 0x411370
int gfx_fill(int x, int y, int w, int h, uint32_t pixel)
{
    if (!g_draw)
        return 1;
    color_fill(back(), GFX_W, GFX_H, x, y, w, h, pixel);
    return 1;
}

// 0x411650
bool gfx_load_palette(const char* file)
{
    std::vector<uint8_t> d;
    if (!read_file(file, d) || d.size() < 54 || d[0] != 'B' || d[1] != 'M')
        return false;
    if (rd32(&d[14]) < 40 || rd32(&d[30]) != 0)
        return false;
    int bpp = rd16(&d[28]);
    if (bpp != 8 && bpp != 4 && bpp != 1)
        return false;
    uint32_t clr = rd32(&d[46]);
    size_t bytes = clr ? (size_t)clr * 4 : (size_t)(4u << bpp);
    size_t start = 14 + rd32(&d[14]);
    if (start + bytes > d.size() || bytes > 1024)
        return false;
    for (size_t i = 0; i < bytes / 4; ++i)
        g_base_pal[i] = rd32(&d[start + i * 4]);
    gfx_set_palette(g_base_pal);
    return true;
}

// 0x4115e0
// The DirectDraw palette is created with DDPCAPS_8BIT only (no
// DDPCAPS_ALLOW256), so entries 0 and 255 stay reserved for black and white.
// This matters: game.pal has magenta at index 0, so every magenta pixel (and
// the 0xff00ff color key) is matched to index 11 instead.
void gfx_set_palette(const uint32_t* rgbquad256)
{
    for (int i = 0; i < 256; ++i)
        g_pal[i] = rgbquad256[i] & 0xffffff;
    g_pal[0] = 0x000000;
    g_pal[255] = 0xffffff;
}

// 0x411760: copy of the stored palette (gfx+0x21c), not the faded one.
void gfx_get_palette(uint32_t* out) { std::memcpy(out, g_base_pal, sizeof(g_base_pal)); }

// 0x410f70
void gfx_flip()
{
    if (!g_draw)
        return;
    test_on_present(back(), g_pal);
    plat_present(back(), g_pal);
    g_back ^= 1;
}

// 0x410fc0
bool gfx_create_surface(int surf, int w, int h, uint32_t /*flags*/)
{
    if (surf < 0 || surf >= GFX_MAX_SURFACES || w <= 0 || h <= 0)
        return false;
    auto s = std::make_unique<Surface>();
    s->w = w;
    s->h = h;
    s->px.assign((size_t)w * h, 0);
    g_surf[surf] = std::move(s);
    return true;
}

// 0x411820
bool gfx_surface_load(int surf, const char* file)
{
    if (surf < 0 || surf >= GFX_MAX_SURFACES || !g_surf[surf])
        return false;
    Bmp b;
    if (!load_bmp(file, b))
        return false;
    g_surf[surf]->file = file;
    bmp_to_surface(b, *g_surf[surf]);
    return true;
}

// 0x4114f0
bool gfx_load_bmp(int surf, const char* file, uint32_t colorkey, uint32_t flags)
{
    Bmp b;
    if (!load_bmp(file, b)) {
        std::fprintf(stderr, "gfx: cannot load %s\n", file);
        return false;
    }
    if (!gfx_create_surface(surf, b.w, b.h, flags))
        return false;
    Surface& s = *g_surf[surf];
    s.file = file;
    bmp_to_surface(b, s);
    s.key = colorref_to_index(colorkey);   // surface vtbl+0x10 (0x411b00)
    s.has_key = true;
    return true;
}

// surface vtbl+0x0c (0x411a60)
int gfx_surface_blit(int dst, int src, int x, int y, int w, int h, int sx, int sy)
{
    if (dst < 0 || dst >= GFX_MAX_SURFACES || src < 0 || src >= GFX_MAX_SURFACES || !g_surf[dst] || !g_surf[src])
        return 0;
    Surface& d = *g_surf[dst];
    if (x + w > d.w)
        w = d.w - x;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y + h > d.h)
        h = d.h - y;
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    blt_fast(d.px.data(), d.w, d.h, x, y, *g_surf[src], sx, sy, w, h);
    return 1;
}

// surface vtbl+0x14 (0x411cb0)
int gfx_surface_fill_key(int surf, int x, int y, int w, int h)
{
    if (surf < 0 || surf >= GFX_MAX_SURFACES || !g_surf[surf])
        return 0;
    Surface& s = *g_surf[surf];
    color_fill(s.px.data(), s.w, s.h, x, y, w, h, s.key);
    return 1;
}

// surface vtbl+0x18 (0x411b40)
int gfx_surface_fill_rgb(int surf, int x, int y, int w, int h, uint32_t colorref)
{
    if (surf < 0 || surf >= GFX_MAX_SURFACES || !g_surf[surf])
        return 0;
    Surface& s = *g_surf[surf];
    color_fill(s.px.data(), s.w, s.h, x, y, w, h, colorref ? colorref_to_index(colorref) : 0);
    return 1;
}

int gfx_surface_width(int surf) { return (surf >= 0 && surf < GFX_MAX_SURFACES && g_surf[surf]) ? g_surf[surf]->w : 0; }
int gfx_surface_height(int surf) { return (surf >= 0 && surf < GFX_MAX_SURFACES && g_surf[surf]) ? g_surf[surf]->h : 0; }

void gfx_text(int x, int y, const char* sjis, int len, uint32_t colorref)
{
    TextMask m;
    if (len <= 0 || !plat_text_mask(sjis, len, m))
        return;
    uint8_t c = (uint8_t)colorref_to_index(colorref);
    uint8_t* b = back();
    for (int j = 0; j < m.h; ++j) {
        int py = y + m.oy + j;
        if (py < 0 || py >= GFX_H)
            continue;
        for (int i = 0; i < m.w; ++i) {
            int px = x + m.ox + i;
            if (px < 0 || px >= GFX_W)
                continue;
            if (m.px[(size_t)j * m.w + i])
                b[py * GFX_W + px] = c;
        }
    }
}
