// Native Windows backend (Windows 7 .. 11, x86 / x64).
//
// Replaces the parts of the original that break on modern Windows:
//  * DirectDraw 640x480x8 exclusive mode  -> 8-bit frame converted to 32-bit
//    and scaled into a normal window (borderless full screen by default,
//    Alt+Enter toggles a resizable window).
//  * DirectMusic                           -> engine sequencer + winmm MIDI out
//    (Microsoft GS Wavetable Synth, same Roland GS sound set).
//  * DirectSound                           -> still DirectSound (static
//    buffers, one per effect, exactly like the original).
//  * Input: GetKeyboardState + joyGetPosEx exactly like the original.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <GL/gl.h>
#include <shellapi.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../engine/app.h"
#include "../engine/engine.h"
#include "../engine/platform.h"
#include "../engine/testing.h"

namespace {

HINSTANCE g_inst;
HWND      g_hwnd;
bool      g_fullscreen = true;
RECT      g_saved_rect{};
bool      g_closing = false;
std::vector<uint32_t> g_rgb(GFX_W * GFX_H);
uint32_t  g_virtual_ms = 0;

const wchar_t kClass[] = L"sotsuken";
const wchar_t kTitle[] = L"斬るビル";   // 斬るビル

// ---------------------------------------------------------------- video ---
void present_to(HDC dc)
{
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0)
        return;
    // keep the 4:3 aspect ratio, letterbox with black
    int w = cw, h = cw * GFX_H / GFX_W;
    if (h > ch) {
        h = ch;
        w = ch * GFX_W / GFX_H;
    }
    int x = (cw - w) / 2, y = (ch - h) / 2;
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (x > 0) {
        RECT r1{0, 0, x, ch}, r2{x + w, 0, cw, ch};
        FillRect(dc, &r1, black);
        FillRect(dc, &r2, black);
    }
    if (y > 0) {
        RECT r1{0, 0, cw, y}, r2{0, y + h, cw, ch};
        FillRect(dc, &r1, black);
        FillRect(dc, &r2, black);
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = GFX_W;
    bi.bmiHeader.biHeight = -GFX_H;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, x, y, w, h, 0, 0, GFX_W, GFX_H, g_rgb.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

// OpenGL 1.1 presentation: the 640x480 frame is a texture drawn as one
// letterboxed quad, so scaling to 1080p/4K full screen is done by the GPU.
// opengl32.dll ships with every Windows; if no usable context can be created
// the GDI StretchDIBits path above is used instead (--gdi forces it).
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
HDC    g_gl_dc;
HGLRC  g_glrc;
GLuint g_gl_tex;
bool   g_gl = false;

bool gl_init()
{
    g_gl_dc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_gl_dc, &pfd);
    if (!pf || !SetPixelFormat(g_gl_dc, pf, &pfd))
        return false;
    g_glrc = wglCreateContext(g_gl_dc);
    if (!g_glrc || !wglMakeCurrent(g_gl_dc, g_glrc))
        return false;
    // no vsync: the game paces itself with timeGetTime like the original
    typedef BOOL(WINAPI * SwapIntervalFn)(int);
    auto swap_interval = (SwapIntervalFn)(void*)wglGetProcAddress("wglSwapIntervalEXT");
    if (swap_interval)
        swap_interval(0);
    glGenTextures(1, &g_gl_tex);
    glBindTexture(GL_TEXTURE_2D, g_gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    // power-of-two storage for GL 1.1 implementations
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);
    if (glGetError() != GL_NO_ERROR)
        return false;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    return true;
}

void gl_shutdown()
{
    if (g_glrc) {
        if (g_gl_tex)
            glDeleteTextures(1, &g_gl_tex);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glrc);
    }
    if (g_gl_dc)
        ReleaseDC(g_hwnd, g_gl_dc);
    g_glrc = nullptr;
    g_gl_dc = nullptr;
    g_gl_tex = 0;
    g_gl = false;
}

void gl_present()
{
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0)
        return;
    int w = cw, h = cw * GFX_H / GFX_W;
    if (h > ch) {
        h = ch;
        w = ch * GFX_W / GFX_H;
    }
    glViewport(0, 0, cw, ch);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport((cw - w) / 2, (ch - h) / 2, w, h);
    glBindTexture(GL_TEXTURE_2D, g_gl_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX_W, GFX_H, GL_BGRA_EXT, GL_UNSIGNED_BYTE, g_rgb.data());
    const float u = GFX_W / 1024.f, v = GFX_H / 512.f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, v); glVertex2f(-1, -1);
    glTexCoord2f(u, v); glVertex2f(1, -1);
    glTexCoord2f(u, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();
    SwapBuffers(g_gl_dc);
}

void present_frame()
{
    if (g_gl) {
        gl_present();
    } else {
        HDC dc = GetDC(g_hwnd);
        present_to(dc);
        ReleaseDC(g_hwnd, dc);
    }
}

void set_fullscreen(bool fs, bool initial = false)
{
    g_fullscreen = fs;
    if (fs) {
        if (!initial)
            GetWindowRect(g_hwnd, &g_saved_rect);   // remember the window to restore
        HMONITOR mon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfo(mon, &mi);
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        RECT r = g_saved_rect;
        if (r.right - r.left < 320) {
            RECT wa;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
            // largest 0.5x step that fits in 85% of the work area (1.5x on 1080p)
            int half_steps = 2;
            while ((half_steps + 1) * GFX_W / 2 <= (wa.right - wa.left) * 85 / 100 &&
                   (half_steps + 1) * GFX_H / 2 <= (wa.bottom - wa.top) * 85 / 100)
                ++half_steps;
            r = {0, 0, GFX_W * half_steps / 2, GFX_H * half_steps / 2};
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
            int ww = r.right - r.left, wh = r.bottom - r.top;
            r.left = wa.left + ((wa.right - wa.left) - ww) / 2;
            r.top = wa.top + ((wa.bottom - wa.top) - wh) / 2;
            r.right = r.left + ww;
            r.bottom = r.top + wh;
        }
        SetWindowPos(g_hwnd, HWND_NOTOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_DESTROY:            // original: PostQuitMessage(0)
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:              // original: 0x4108e0 (quit flag + DestroyWindow)
        g_closing = true;
        plat_quit();
        DestroyWindow(h);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_gl)
            gl_present();
        else
            present_to(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {   // original: ShowCursor(FALSE)
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    case WM_SYSKEYDOWN:
        if (wp == VK_RETURN && (lp & (1 << 29))) {   // Alt+Enter
            set_fullscreen(!g_fullscreen);
            return 0;
        }
        break;
    case WM_SYSCHAR:
        if (wp == VK_RETURN)
            return 0;   // no beep
        break;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU)
            return 0;   // F10 / Alt must not open the (missing) menu
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// -------------------------------------------------------------- joystick ---
UINT g_joy_id = (UINT)-1;
DWORD g_joy_xlo, g_joy_xhi, g_joy_ylo, g_joy_yhi;
int g_joy_rescan = 0;

void joy_scan()
{
    g_joy_id = (UINT)-1;
    for (UINT i = 0; i < 16; ++i) {
        JOYCAPSA caps;
        if (joyGetDevCapsA(i, &caps, sizeof(caps)) != JOYERR_NOERROR)
            continue;
        JOYINFOEX ji{};
        ji.dwSize = sizeof(ji);
        ji.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(i, &ji) != JOYERR_NOERROR)
            continue;   // capabilities of an unplugged slot
        g_joy_id = i;
        // thresholds of 0x412320
        g_joy_xlo = (caps.wXmax + caps.wXmin * 2) / 3;
        g_joy_xhi = (caps.wXmin + caps.wXmax * 2) / 3;
        g_joy_ylo = (caps.wYmax + caps.wYmin * 2) / 3;
        g_joy_yhi = (caps.wYmin + caps.wYmax * 2) / 3;
        return;
    }
}

// ----------------------------------------------------------------- sound ---
LPDIRECTSOUND g_ds;
LPDIRECTSOUNDBUFFER g_primary;
LPDIRECTSOUNDBUFFER g_buf[64];

// ------------------------------------------------------------------ midi ---
HMIDIOUT g_midi;

// ------------------------------------------------------------------ text ---
HFONT g_font;
HDC   g_text_dc;
HBITMAP g_text_bmp;
uint32_t* g_text_bits;
const int kTextW = 1024, kTextH = 64;

int CALLBACK font_exists_cb(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp)
{
    *(bool*)lp = true;
    return 0;
}

bool font_exists(const wchar_t* face)
{
    HDC dc = GetDC(nullptr);
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, font_exists_cb, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}

void text_init()
{
    // CreateFontA(28, 14, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, "HGS行書体")
    // falling back to "ＭＳ 明朝" like 0x40b5f0, plus fonts shipped with
    // Windows 10/11 for systems without the Office / Japanese fonts.
    static const wchar_t* faces[] = {
        L"HGS行書体",            // HGS行書体
        L"HG行書体",             // HG行書体
        L"ＭＳ 明朝",        // ＭＳ 明朝
        L"MS Mincho",
        L"游明朝",               // 游明朝
        L"Yu Mincho",
        L"ＭＳ ゴシック",  // ＭＳ ゴシック
        L"Yu Gothic",
        L"Meiryo",
    };
    const wchar_t* face = faces[2];
    for (const wchar_t* f : faces)
        if (font_exists(f)) {
            face = f;
            break;
        }
    g_font = CreateFontW(28, 14, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0,
                         NONANTIALIASED_QUALITY, 0, face);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kTextW;
    bi.bmiHeader.biHeight = -kTextH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g_text_dc = CreateCompatibleDC(nullptr);
    g_text_bmp = CreateDIBSection(g_text_dc, &bi, DIB_RGB_COLORS, (void**)&g_text_bits, nullptr, 0);
    SelectObject(g_text_dc, g_text_bmp);
    SelectObject(g_text_dc, g_font);
    SetBkMode(g_text_dc, TRANSPARENT);
    SetTextColor(g_text_dc, RGB(255, 255, 255));
}

std::wstring sjis_to_wide(const char* s, int len)
{
    int n = MultiByteToWideChar(932, 0, s, len, nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(932, 0, s, len, &w[0], n);
    return w;
}

std::wstring utf8_to_wide(const std::string& s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

void set_working_directory_to_exe()
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;
    for (DWORD i = n; i > 0; --i)
        if (path[i - 1] == L'\\' || path[i - 1] == L'/') {
            path[i - 1] = 0;
            break;
        }
    // only switch if the game data is next to the executable
    std::wstring probe = std::wstring(path) + L"\\image\\game.pal";
    if (GetFileAttributesW(probe.c_str()) != INVALID_FILE_ATTRIBUTES)
        SetCurrentDirectoryW(path);
}

void make_dpi_aware()
{
    HMODULE user = GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI * SetCtx)(HANDLE);
    auto set_ctx = user ? (SetCtx)(void*)GetProcAddress(user, "SetProcessDpiAwarenessContext") : nullptr;
    if (set_ctx && set_ctx((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */))
        return;
    SetProcessDPIAware();
}

} // namespace

// ===================================================================== API ==

bool plat_video_init()
{
    if (g_opts.headless)
        return true;
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = g_inst;
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(102));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc))
        return false;
    g_hwnd = CreateWindowExW(0, kClass, kTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             GFX_W, GFX_H, nullptr, nullptr, g_inst, nullptr);
    if (!g_hwnd)
        return false;
    g_saved_rect = RECT{0, 0, 0, 0};
    set_fullscreen(!g_opts.windowed, true);
    if (!g_opts.force_gdi) {
        g_gl = gl_init();
        if (!g_gl)
            gl_shutdown();
    }
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);
    text_init();
    return true;
}

void plat_video_shutdown()
{
    gl_shutdown();
    if (g_text_dc) {
        DeleteDC(g_text_dc);
        g_text_dc = nullptr;
    }
    if (g_text_bmp) {
        DeleteObject(g_text_bmp);
        g_text_bmp = nullptr;
    }
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }
    if (g_hwnd && !g_closing)
        DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
}

void plat_present(const uint8_t* idx, const uint32_t* pal)
{
    if (g_opts.headless || !g_hwnd)
        return;
    for (int i = 0; i < GFX_W * GFX_H; ++i)
        g_rgb[i] = pal[idx[i]];   // RGBQUAD layout == BI_RGB 32-bit layout
    present_frame();
}

bool plat_text_mask(const char* sjis, int len, TextMask& out)
{
    if (!g_text_dc) {
        if (g_opts.headless)
            text_init();
        if (!g_text_dc)
            return false;
    }
    std::wstring w = sjis_to_wide(sjis, len);
    std::memset(g_text_bits, 0, sizeof(uint32_t) * kTextW * kTextH);
    TextOutW(g_text_dc, 0, 0, w.c_str(), (int)w.size());
    GdiFlush();
    SIZE sz{};
    GetTextExtentPoint32W(g_text_dc, w.c_str(), (int)w.size(), &sz);
    out.w = sz.cx + 16 < kTextW ? sz.cx + 16 : kTextW;
    out.h = kTextH;
    out.ox = 0;
    out.oy = 0;
    out.px.assign((size_t)out.w * out.h, 0);
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x)
            out.px[(size_t)y * out.w + x] = (g_text_bits[y * kTextW + x] & 0x808080) ? 1 : 0;
    return true;
}

void plat_keyboard_state(uint8_t keys[256])
{
    if (!GetKeyboardState(keys))
        std::memset(keys, 0, 256);
}

bool plat_joystick_state(uint8_t joy[256])
{
    if (g_joy_id == (UINT)-1) {
        if (--g_joy_rescan > 0)
            return false;
        g_joy_rescan = 120;   // look for a newly connected pad every ~2 s
        joy_scan();
        if (g_joy_id == (UINT)-1)
            return false;
    }
    JOYINFOEX ji{};
    ji.dwSize = sizeof(ji);
    ji.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(g_joy_id, &ji) != JOYERR_NOERROR) {
        g_joy_id = (UINT)-1;
        return false;
    }
    DWORD y = ji.dwYpos & 0xffff, x = ji.dwXpos & 0xffff;
    joy[0] = y < g_joy_ylo ? 0xff : 0;
    joy[1] = g_joy_yhi < y ? 0xff : 0;
    joy[2] = x < g_joy_xlo ? 0xff : 0;
    joy[3] = g_joy_xhi < x ? 0xff : 0;
    for (int b = 0; b < 16; ++b)
        joy[4 + b] = (ji.dwButtons & (1u << b)) ? 0xff : 0;
    return true;
}

bool plat_snd_init()
{
    if (g_opts.headless)
        return false;
    if (FAILED(DirectSoundCreate(nullptr, &g_ds, nullptr)))
        return false;
    g_ds->SetCooperativeLevel(g_hwnd ? g_hwnd : GetDesktopWindow(), DSSCL_PRIORITY);
    DSBUFFERDESC bd{};
    bd.dwSize = sizeof(bd);
    bd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    if (SUCCEEDED(g_ds->CreateSoundBuffer(&bd, &g_primary, nullptr))) {
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 2;
        wf.nSamplesPerSec = 44100;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = 4;
        wf.nAvgBytesPerSec = 44100 * 4;
        g_primary->SetFormat(&wf);
    }
    return true;
}

void plat_snd_shutdown()
{
    for (auto& b : g_buf)
        if (b) {
            b->Release();
            b = nullptr;
        }
    if (g_primary) {
        g_primary->Release();
        g_primary = nullptr;
    }
    if (g_ds) {
        g_ds->Release();
        g_ds = nullptr;
    }
}

bool plat_snd_create(int idx, const PcmSound& pcm)
{
    if (!g_ds || pcm.data.empty())
        return false;
    WAVEFORMATEX wf{};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = (WORD)pcm.channels;
    wf.nSamplesPerSec = (DWORD)pcm.rate;
    wf.wBitsPerSample = (WORD)pcm.bits;
    wf.nBlockAlign = (WORD)(pcm.channels * pcm.bits / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    DSBUFFERDESC bd{};
    bd.dwSize = sizeof(bd);
    // 0x180e0 = GETCURRENTPOSITION2 | GLOBALFOCUS | CTRLVOLUME | CTRLPAN | CTRLFREQUENCY
    bd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME |
                 DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY;
    bd.dwBufferBytes = (DWORD)pcm.data.size();
    bd.lpwfxFormat = &wf;
    LPDIRECTSOUNDBUFFER b = nullptr;
    if (FAILED(g_ds->CreateSoundBuffer(&bd, &b, nullptr)))
        return false;
    void *p1, *p2;
    DWORD n1, n2;
    if (SUCCEEDED(b->Lock(0, bd.dwBufferBytes, &p1, &n1, &p2, &n2, 0))) {
        std::memcpy(p1, pcm.data.data(), n1);
        if (p2)
            std::memcpy(p2, pcm.data.data() + n1, n2);
        b->Unlock(p1, n1, p2, n2);
    }
    if (g_buf[idx])
        g_buf[idx]->Release();
    g_buf[idx] = b;
    return true;
}

void plat_snd_release(int idx)
{
    if (g_buf[idx]) {
        g_buf[idx]->Release();
        g_buf[idx] = nullptr;
    }
}

void plat_snd_play(int idx)
{
    if (!g_buf[idx])
        return;
    g_buf[idx]->SetCurrentPosition(0);
    if (g_buf[idx]->Play(0, 0, 0) == DSERR_BUFFERLOST) {
        g_buf[idx]->Restore();
        g_buf[idx]->Play(0, 0, 0);
    }
}

void plat_snd_stop(int idx)
{
    if (g_buf[idx])
        g_buf[idx]->Stop();
}

bool plat_midi_open()
{
    if (g_opts.headless)
        return false;
    return midiOutOpen(&g_midi, MIDI_MAPPER, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
}

void plat_midi_close()
{
    if (g_midi) {
        midiOutReset(g_midi);
        midiOutClose(g_midi);
        g_midi = nullptr;
    }
}

void plat_midi_short(uint32_t msg)
{
    if (g_midi)
        midiOutShortMsg(g_midi, msg);
}

void plat_midi_long(const uint8_t* data, size_t len)
{
    if (!g_midi || len == 0)
        return;
    std::vector<char> buf(data, data + len);
    MIDIHDR h{};
    h.lpData = buf.data();
    h.dwBufferLength = (DWORD)len;
    if (midiOutPrepareHeader(g_midi, &h, sizeof(h)) != MMSYSERR_NOERROR)
        return;
    if (midiOutLongMsg(g_midi, &h, sizeof(h)) == MMSYSERR_NOERROR) {
        for (int i = 0; i < 200 && !(h.dwFlags & MHDR_DONE); ++i)
            Sleep(1);
    }
    midiOutUnprepareHeader(g_midi, &h, sizeof(h));
}

uint32_t plat_time_ms()
{
    if (g_opts.headless)
        return ++g_virtual_ms;   // deterministic clock for automated runs
    return timeGetTime();
}

void plat_sleep_ms(int ms) { Sleep((DWORD)ms); }

bool plat_pump_messages()
{
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT)
            return false;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return true;
}

void plat_message_box(const std::string& text, const std::string& title)
{
    if (g_opts.headless) {
        std::fprintf(stderr, "%s: %s\n", title.c_str(), text.c_str());
        return;
    }
    MessageBoxW(g_hwnd, utf8_to_wide(text).c_str(), utf8_to_wide(title).c_str(), MB_OK);
}

std::string plat_sjis_to_utf8(const char* sjis, int len)
{
    std::wstring w = sjis_to_wide(sjis, len);
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// ================================================================== entry ==

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int)
{
    g_inst = inst;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 1; argv && i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string a((size_t)(n > 0 ? n - 1 : 0), '\0');
        if (n > 1)
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, &a[0], n, nullptr, nullptr);
        args.push_back(a);
    }
    if (argv)
        LocalFree(argv);
    test_parse_args(args);

    set_working_directory_to_exe();
    make_dpi_aware();

    // single instance (0x410690): bring the running game to the front
    // (skipped in headless test mode so several instances can run at once)
    const wchar_t* kMutex = L"\u5352\u7814\uff12\uff10\uff10\uff13";  // 卒研２００３
    HANDLE mutex = g_opts.headless ? nullptr : OpenMutexW(MUTEX_ALL_ACCESS, FALSE, kMutex);
    if (mutex) {
        HWND other = FindWindowW(kClass, kTitle);
        if (other) {
            SetForegroundWindow(other);
            ShowWindow(other, SW_SHOW);
        }
        CloseHandle(mutex);
        return 0;
    }
    if (!g_opts.headless)
        mutex = CreateMutexW(nullptr, TRUE, kMutex);

    timeBeginPeriod(1);
    int rc = app_run();
    timeEndPeriod(1);

    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
