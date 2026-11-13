// Engine layer for the 斬るビル (Kiru Bill) source port.
//
// The original game (卒研2003, VC6 / DirectX 7) was built on a small set of
// engine classes: a DirectDraw 640x480x8 renderer, a DirectSound wave player,
// a DirectMusic MIDI player, a GetKeyboardState/joyGetPosEx input mapper and
// a priority ordered "task" scheduler.  This header exposes the same
// operations as plain functions.  Every function notes the original address
// it replaces so the translated game code can be compared with the binary.
#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Graphics (original singleton object at 0x41fa98, vtable 0x41c28c)
// ---------------------------------------------------------------------------
// The renderer keeps the original model: an 8-bit indexed 640x480 back
// buffer, up to 128 8-bit off-screen surfaces with a source color key, and a
// 256 entry palette.  Presenting converts the indexed frame to RGB.

enum { GFX_W = 640, GFX_H = 480, GFX_MAX_SURFACES = 128 };

bool     gfx_init();                                  // vtable+0x04 (0x410cf0)
void     gfx_shutdown();                              // vtable+0x08 (0x410ec0)

// vtable+0x0c (0x411110): color keyed blit of surface `surf` rect (sx,sy,w,h)
// to the back buffer at (x,y); clipped to the screen.  Returns 1 if drawn or
// skipped because drawing is disabled, 0 if fully clipped.
int      gfx_blit(int surf, int x, int y, int w, int h, int sx, int sy);
// vtable+0x10 (0x411260): fill the whole back buffer with a COLORREF.
int      gfx_clear_rgb(uint32_t colorref);
// vtable+0x14 (0x4112d0): fill a rect with a COLORREF (0x00BBGGRR).
int      gfx_fill_rgb(int x, int y, int w, int h, uint32_t colorref);
// 0x411370 (non virtual): fill a rect with a raw pixel value (palette index).
int      gfx_fill(int x, int y, int w, int h, uint32_t pixel);
// 0x411410: convert a COLORREF to the back buffer pixel value (nearest
// palette index, like GDI's SetPixel/GetPixel round trip).
uint32_t gfx_rgb_to_pixel(uint32_t colorref);
// vtable+0x18 (0x411650): load the palette of a .bmp/.pal file and apply it.
bool     gfx_load_palette(const char* file);
// vtable+0x1c (0x4115e0): apply 256 RGBQUAD (0x00RRGGBB as stored in a BMP:
// byte0=B, byte1=G, byte2=R) entries.
void     gfx_set_palette(const uint32_t* rgbquad256);
// 0x411760: copy the current 256 RGBQUAD palette entries.
void     gfx_get_palette(uint32_t* rgbquad256);
// vtable+0x28 (0x410f70): present the back buffer (only when drawing is
// enabled, exactly like the original Flip()).
void     gfx_flip();
// 0x4114f0: create surface `surf` sized from the BMP header of `file` and load
// it; `colorkey` is a COLORREF made transparent for gfx_blit.
bool     gfx_load_bmp(int surf, const char* file, uint32_t colorkey, uint32_t flags);
// vtable+0x2c (0x410fc0): create an empty surface.
bool     gfx_create_surface(int surf, int w, int h, uint32_t flags);
// Surface helpers (surface vtable 0x41c2bc).
//  +0x0c (0x411a60): blit from surface `src` (rect sx,sy,w,h) onto surface
//                    `dst` at (x,y) with color key, clipped to `dst`.
int      gfx_surface_blit(int dst, int src, int x, int y, int w, int h, int sx, int sy);
//  +0x14 (0x411cb0): fill rect of a surface with its color key pixel.
int      gfx_surface_fill_key(int surf, int x, int y, int w, int h);
//  +0x18 (0x411b40): fill rect of a surface with a COLORREF (0 = pixel 0).
int      gfx_surface_fill_rgb(int surf, int x, int y, int w, int h, uint32_t colorref);
//  +0x08 (0x411820): (re)load a BMP into an existing surface.
bool     gfx_surface_load(int surf, const char* file);
int      gfx_surface_width(int surf);
int      gfx_surface_height(int surf);

// The byte at gfx+0x61c: when 0 all blits/fills/flip are skipped (frame
// skipping in the main loop).
void     gfx_set_draw_enabled(bool on);
bool     gfx_draw_enabled();

// GDI text on the back buffer (GetDC + SetBkMode(TRANSPARENT) + SelectObject
// (font at gfx+0x620) + SetTextColor + TextOutA + ReleaseDC).  `sjis` is the
// original Shift-JIS byte string, `len` its byte length.  The font is the one
// created at startup: CreateFontA(28, 14, ..., SHIFTJIS_CHARSET, "HGS行書体")
// falling back to "ＭＳ 明朝".
void     gfx_text(int x, int y, const char* sjis, int len, uint32_t colorref);

// Direct access to the 8-bit back buffer (used by the port only).
uint8_t* gfx_backbuffer();

// ---------------------------------------------------------------------------
// Sound effects (0x413220..0x413760, DirectSound)
// ---------------------------------------------------------------------------
bool snd_init();                                   // 0x413220
void snd_shutdown();                               // 0x413760
bool snd_load(const char* wav, int idx);           // 0x413400
void snd_play(int idx);                            // 0x413610 SetCurrentPosition(0)+Play(no loop)
void snd_stop(int idx);                            // 0x413690
void snd_unload(int idx);                          // 0x4136e0

// ---------------------------------------------------------------------------
// Music (0x412a90..0x413120, DirectMusic segments, 20 slots)
// ---------------------------------------------------------------------------
// Loop points are DirectMusic MUSIC_TIME values (768 ticks per quarter note).
bool mus_init();                                   // 0x412a90
void mus_shutdown();                               // 0x412e10
void mus_load(const char* midi, int idx);          // 0x412b90 (unloads old first)
void mus_unload(int idx);                          // 0x412c30
void mus_play(int idx);                            // 0x412cb0 PlaySegment
void mus_stop(int idx);                            // 0x412d20 Stop
void mus_set_loop(int idx, int32_t start, int32_t end); // 0x412d80 SetLoopPoints + SetRepeats(0xffff)

// ---------------------------------------------------------------------------
// Input (0x411f70..0x412400)
// ---------------------------------------------------------------------------
// 64 virtual buttons; each has a list of (device, code) bindings.
// device 0 = keyboard (code = Windows virtual key), device 1 = joystick
// (code 0=up 1=down 2=left 3=right 4..19=buttons 1..16).
bool in_init();                                    // 0x411f70
void in_shutdown();                                // 0x412060
void in_bind(int device, int button, int code);    // 0x4121d0
void in_update();                                  // 0x412090 poll devices, flip state buffers
int  in_held(int button);                          // 0x412150 current state != 0
bool in_pressed(int button);                       // 0x412180 pressed this frame (edge)
void in_clear();                                   // 0x412130 clear current state
void in_force(int button);                         // 0x4121b0 current state = 0x80

// ---------------------------------------------------------------------------
// Platform / main loop
// ---------------------------------------------------------------------------
uint32_t plat_time_ms();                           // timeGetTime
void     plat_quit();                              // DestroyWindow -> WM_DESTROY -> PostQuitMessage (0x4108e0)
bool     plat_quit_requested();
// Show a fatal error box (0x410900 MessageBoxA(...,"error!!")).
void     plat_error(const char* sjis_message);

// ---------------------------------------------------------------------------
// MSVC CRT pieces whose exact behavior matters for replays
// ---------------------------------------------------------------------------
extern uint32_t g_rand_seed;                       // 0x41e7b8 (initial 1)
inline void crt_srand(uint32_t s) { g_rand_seed = s; }                 // 0x413f09
inline int32_t crt_rand() {                                            // 0x413f13
    g_rand_seed = g_rand_seed * 0x343fdu + 2531011u;
    return (int32_t)((g_rand_seed >> 16) & 0x7fff);
}
inline int32_t ftol(double v) { return (int32_t)v; }                  // 0x413f34 (_ftol, truncation)
uint32_t crt_time();                                                   // 0x414236 time(NULL)
