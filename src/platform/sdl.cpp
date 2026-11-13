// SDL2 backend (Linux / macOS).  Used for development and automated
// testing of the port; the Windows build uses src/platform/win32.cpp.
//
// Testing options (--headless, --input, --trace, ...) are in engine/testing.h.
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#include <iconv.h>

#include "../engine/app.h"
#include "../engine/engine.h"
#include "../engine/platform.h"
#include "../engine/testing.h"

namespace {

SDL_Window* g_win;
SDL_Renderer* g_ren;
SDL_Texture* g_tex;
std::vector<uint32_t> g_rgb(GFX_W * GFX_H);
uint32_t g_virtual_ms = 0;
bool g_fullscreen = false;

// ---------------------------------------------------------------- mixer ---
struct Voice {
    std::vector<float> pcm;   // mono/stereo interleaved, normalised
    int channels = 1, rate = 22050;
    double pos = 0;
    bool playing = false;
};
Voice g_voice[64];
std::mutex g_audio_mx;
SDL_AudioDeviceID g_audio = 0;

void audio_cb(void*, Uint8* stream, int len)
{
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    int frames = len / 4;
    std::vector<float> mix((size_t)frames * 2, 0.f);
    {
        std::lock_guard<std::mutex> lk(g_audio_mx);
        for (Voice& v : g_voice) {
            if (!v.playing)
                continue;
            double step = (double)v.rate / 44100.0;
            size_t nframes = v.pcm.size() / (size_t)v.channels;
            for (int i = 0; i < frames; ++i) {
                size_t p = (size_t)v.pos;
                if (p + 1 >= nframes) {
                    v.playing = false;
                    break;
                }
                double f = v.pos - (double)p;
                for (int c = 0; c < 2; ++c) {
                    int sc = v.channels == 2 ? c : 0;
                    float a = v.pcm[p * v.channels + sc], b = v.pcm[(p + 1) * v.channels + sc];
                    mix[(size_t)i * 2 + c] += (float)(a + (b - a) * f);
                }
                v.pos += step;
            }
        }
    }
    for (int i = 0; i < frames * 2; ++i) {
        float s = std::max(-1.f, std::min(1.f, mix[(size_t)i]));
        out[i] = (int16_t)(s * 32767.f);
    }
}

// ---------------------------------------------------------------- text ----
#ifdef HAVE_FREETYPE
FT_Library g_ft;
FT_Face g_face;
bool g_ft_ok = false;
#endif

std::u32string sjis_to_u32(const char* s, int len)
{
    std::u32string out;
    iconv_t cd = iconv_open("UTF-32LE", "CP932");
    if (cd == (iconv_t)-1)
        return out;
    std::vector<char> in(s, s + len);
    std::vector<char> buf((size_t)len * 4 + 16);
    char* ip = in.data();
    size_t il = in.size();
    char* op = buf.data();
    size_t ol = buf.size();
    iconv(cd, &ip, &il, &op, &ol);
    iconv_close(cd);
    size_t n = (buf.size() - ol) / 4;
    for (size_t i = 0; i < n; ++i) {
        uint32_t c;
        std::memcpy(&c, &buf[i * 4], 4);
        out.push_back((char32_t)c);
    }
    return out;
}

int vk_from_scancode(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return 'A' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return '1' + (sc - SDL_SCANCODE_1);
    switch (sc) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_UP: return 0x26;
    case SDL_SCANCODE_DOWN: return 0x28;
    case SDL_SCANCODE_LEFT: return 0x25;
    case SDL_SCANCODE_RIGHT: return 0x27;
    case SDL_SCANCODE_ESCAPE: return 0x1b;
    case SDL_SCANCODE_TAB: return 0x09;
    case SDL_SCANCODE_RETURN: return 0x0d;
    case SDL_SCANCODE_SPACE: return 0x20;
    case SDL_SCANCODE_LSHIFT: return 0xa0;
    case SDL_SCANCODE_RSHIFT: return 0xa1;
    case SDL_SCANCODE_LCTRL: return 0xa2;
    case SDL_SCANCODE_RCTRL: return 0xa3;
    case SDL_SCANCODE_BACKSPACE: return 0x08;
    default: return 0;
    }
}

SDL_Joystick* g_joy;



} // namespace

// ===================================================================== API ==

bool plat_video_init()
{
#ifdef HAVE_FREETYPE
    if (!FT_Init_FreeType(&g_ft)) {
        static const char* fonts[] = {
            "/usr/share/fonts/truetype/fonts-japanese-mincho.ttf",
            "/usr/share/fonts/opentype/ipafont-mincho/ipam.ttf",
            "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
            "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
            "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        };
        for (const char* f : fonts)
            if (!FT_New_Face(g_ft, f, 0, &g_face)) {
                FT_Set_Pixel_Sizes(g_face, 28, 28);
                g_ft_ok = true;
                break;
            }
    }
#endif
    if (g_opts.headless)
        return true;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
        return false;
    g_fullscreen = !g_opts.windowed;
    g_win = SDL_CreateWindow("\xe6\x96\xac\xe3\x82\x8b\xe3\x83\x93\xe3\x83\xab",  // 斬るビル
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, GFX_W * 2, GFX_H * 2,
                             SDL_WINDOW_RESIZABLE | (g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!g_win)
        return false;
    g_ren = SDL_CreateRenderer(g_win, -1, g_opts.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    if (!g_ren)
        return false;
    SDL_RenderSetLogicalSize(g_ren, GFX_W, GFX_H);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GFX_W, GFX_H);
    SDL_ShowCursor(SDL_DISABLE);
    if (SDL_NumJoysticks() > 0)
        g_joy = SDL_JoystickOpen(0);
    return g_tex != nullptr;
}

void plat_video_shutdown()
{
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    if (g_ren)
        SDL_DestroyRenderer(g_ren);
    if (g_win)
        SDL_DestroyWindow(g_win);
    g_tex = nullptr;
    g_ren = nullptr;
    g_win = nullptr;
}

void plat_present(const uint8_t* idx, const uint32_t* pal)
{
    for (int i = 0; i < GFX_W * GFX_H; ++i)
        g_rgb[i] = pal[idx[i]] | 0xff000000u;
    if (!g_tex)
        return;
    SDL_UpdateTexture(g_tex, nullptr, g_rgb.data(), GFX_W * 4);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, nullptr, nullptr);
    SDL_RenderPresent(g_ren);
}

bool plat_text_mask(const char* sjis, int len, TextMask& out)
{
#ifdef HAVE_FREETYPE
    if (!g_ft_ok)
        return false;
    std::u32string s = sjis_to_u32(sjis, len);
    out.w = (int)s.size() * 28 + 8;
    out.h = 34;
    out.ox = 0;
    out.oy = 0;
    out.px.assign((size_t)out.w * out.h, 0);
    int pen = 0;
    const int baseline = 24;
    for (char32_t c : s) {
        if (FT_Load_Char(g_face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO))
            continue;
        FT_GlyphSlot g = g_face->glyph;
        int cell = c < 0x80 ? 14 : 28;     // 14 px wide font: SBCS 14, DBCS 28
        int ox = pen + (cell - (int)g->bitmap.width) / 2;
        for (unsigned y = 0; y < g->bitmap.rows; ++y)
            for (unsigned x = 0; x < g->bitmap.width; ++x) {
                const uint8_t* row = g->bitmap.buffer + y * g->bitmap.pitch;
                bool on = (row[x >> 3] >> (7 - (x & 7))) & 1;
                int px = ox + (int)x, py = baseline - g->bitmap_top + (int)y;
                if (on && px >= 0 && px < out.w && py >= 0 && py < out.h)
                    out.px[(size_t)py * out.w + px] = 1;
            }
        pen += cell;
    }
    return true;
#else
    (void)sjis;
    (void)len;
    (void)out;
    return false;
#endif
}

void plat_keyboard_state(uint8_t keys[256])
{
    std::memset(keys, 0, 256);
    if (g_opts.headless)
        return;
    int n = 0;
    const Uint8* st = SDL_GetKeyboardState(&n);
    for (int sc = 0; sc < n; ++sc)
        if (st[sc]) {
            int vk = vk_from_scancode((SDL_Scancode)sc);
            if (vk)
                keys[vk] = 0x80;
        }
}

bool plat_joystick_state(uint8_t joy[256])
{
    if (!g_joy)
        return false;
    int x = SDL_JoystickGetAxis(g_joy, 0), y = SDL_JoystickGetAxis(g_joy, 1);
    joy[0] = y < -10922 ? 0xff : 0;
    joy[1] = y > 10922 ? 0xff : 0;
    joy[2] = x < -10922 ? 0xff : 0;
    joy[3] = x > 10922 ? 0xff : 0;
    int nb = std::min(16, SDL_JoystickNumButtons(g_joy));
    for (int b = 0; b < 16; ++b)
        joy[4 + b] = (b < nb && SDL_JoystickGetButton(g_joy, b)) ? 0xff : 0;
    return true;
}

bool plat_snd_init()
{
    if (g_opts.headless)
        return false;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return false;
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_cb;
    g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!g_audio)
        return false;
    SDL_PauseAudioDevice(g_audio, 0);
    return true;
}

void plat_snd_shutdown()
{
    if (g_audio)
        SDL_CloseAudioDevice(g_audio);
    g_audio = 0;
}

bool plat_snd_create(int idx, const PcmSound& pcm)
{
    std::lock_guard<std::mutex> lk(g_audio_mx);
    Voice& v = g_voice[idx];
    v = Voice();
    v.channels = pcm.channels == 2 ? 2 : 1;
    v.rate = pcm.rate;
    if (pcm.bits == 8)
        for (uint8_t b : pcm.data)
            v.pcm.push_back(((int)b - 128) / 128.f);
    else
        for (size_t i = 0; i + 1 < pcm.data.size(); i += 2)
            v.pcm.push_back((int16_t)(pcm.data[i] | (pcm.data[i + 1] << 8)) / 32768.f);
    return true;
}

void plat_snd_release(int idx)
{
    std::lock_guard<std::mutex> lk(g_audio_mx);
    g_voice[idx] = Voice();
}

void plat_snd_play(int idx)
{
    std::lock_guard<std::mutex> lk(g_audio_mx);
    g_voice[idx].pos = 0;
    g_voice[idx].playing = !g_voice[idx].pcm.empty();
}

void plat_snd_stop(int idx)
{
    std::lock_guard<std::mutex> lk(g_audio_mx);
    g_voice[idx].playing = false;
}

// No system MIDI synth through SDL: music is silent in this backend.
bool plat_midi_open() { return false; }
void plat_midi_close() {}
void plat_midi_short(uint32_t) {}
void plat_midi_long(const uint8_t*, size_t) {}

uint32_t plat_time_ms()
{
    if (g_opts.headless)
        return ++g_virtual_ms;
    return SDL_GetTicks();
}

void plat_sleep_ms(int ms) { SDL_Delay((Uint32)ms); }

bool plat_pump_messages()
{
    if (g_opts.headless)
        return true;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
            g_fullscreen = !g_fullscreen;
            SDL_SetWindowFullscreen(g_win, g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
        if (e.type == SDL_JOYDEVICEADDED && !g_joy)
            g_joy = SDL_JoystickOpen(e.jdevice.which);
    }
    return true;
}

void plat_message_box(const std::string& text, const std::string& title)
{
    std::fprintf(stderr, "%s: %s\n", title.c_str(), text.c_str());
    if (!g_opts.headless)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), g_win);
}

std::string plat_sjis_to_utf8(const char* sjis, int len)
{
    std::string out;
    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == (iconv_t)-1)
        return std::string(sjis, (size_t)len);
    std::vector<char> in(sjis, sjis + len), buf((size_t)len * 4 + 16);
    char* ip = in.data();
    size_t il = in.size();
    char* op = buf.data();
    size_t ol = buf.size();
    iconv(cd, &ip, &il, &op, &ol);
    iconv_close(cd);
    return std::string(buf.data(), buf.size() - ol);
}

int main(int argc, char** argv)
{
    test_parse_args(std::vector<std::string>(argv + 1, argv + argc));
    int rc = app_run();
    SDL_Quit();
    return rc;
}
