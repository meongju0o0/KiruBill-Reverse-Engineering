// Application driver – port of WinMain (0x410690), init (0x4107c0),
// shutdown (0x410890) and the message loop (0x410bd0).
#include "app.h"
#include "engine.h"
#include "platform.h"
#include "testing.h"
#include "../game/game.h"

#include <cstdio>
#include <cstring>
#include <chrono>
#include <ctime>

PlatOptions g_opts;
uint32_t g_rand_seed = 1;     // 0x41e7b8

namespace {
bool g_quit = false;          // 0x41fa90

// --trace: wall-clock time of each start-up phase
std::chrono::steady_clock::time_point g_t0 = std::chrono::steady_clock::now();
void trace_phase(const char* what)
{
    if (!g_opts.trace)
        return;
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_t0).count();
    std::printf("[%8.1f ms] %s\n", ms, what);
    std::fflush(stdout);
}
}

// 0x4108e0: DestroyWindow -> WM_DESTROY -> PostQuitMessage
void plat_quit() { g_quit = true; }
bool plat_quit_requested() { return g_quit; }

// 0x410900
void plat_error(const char* sjis_message)
{
    plat_message_box(plat_sjis_to_utf8(sjis_message, (int)std::strlen(sjis_message)), "error!!");
}

uint32_t crt_time() { return (uint32_t)std::time(nullptr); }

// 0x4107c0
static bool app_init()
{
    // ShowCursor(0) is handled by the platform window.
    if (!in_init()) {
        plat_error(STR_41e794);   // 入力機器の初期化に失敗！
        return false;
    }
    trace_phase("input ready");
    if (!snd_init()) {
        plat_error(STR_41e770);   // ダイレクトサウンドの初期化に失敗！
        return false;
    }
    trace_phase("DirectSound ready");
    if (!mus_init()) {
        plat_error(STR_41e748);   // ダイレクトミュージックの初期化に失敗！
        return false;
    }
    trace_phase("MIDI ready");
    // 0x410960 switches the window to its full screen style.
    if (!gfx_init()) {
        plat_error(STR_41e724);   // ダイレクトドロウの初期化に失敗！
        return false;
    }
    gfx_load_palette("image/game.pal");
    trace_phase("palette ready, loading resources (loading screen)");
    f_40b5f0();                   // load resources, set the first scene
    trace_phase("resources loaded");
    return true;
}

// 0x410890
static void app_shutdown()
{
    f_40bf30();
    in_shutdown();
    snd_shutdown();
    mus_shutdown();
    gfx_shutdown();
    plat_video_shutdown();
}

int app_run()
{
    // The port needs the original game data next to it.
    if (FILE* f = std::fopen("image/game.pal", "rb")) {
        std::fclose(f);
    } else {
        plat_message_box("\xea\xb2\x8c\xec\x9e\x84 \xeb\x8d\xb0\xec\x9d\xb4\xed\x84\xb0(image/game.pal)\xeb\xa5\xbc "
                         "\xec\xb0\xbe\xec\x9d\x84 \xec\x88\x98 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.\n"
                         "KiruBill.exe\xeb\xa5\xbc \xec\x9b\x90\xeb\xb3\xb8 \xea\xb2\x8c\xec\x9e\x84 \xed\x8f\xb4\xeb\x8d\x94"
                         "(image, sound, data, save \xed\x8f\xb4\xeb\x8d\x94\xea\xb0\x80 \xec\x9e\x88\xeb\x8a\x94 \xea\xb3\xb3)"
                         "\xec\x97\x90 \xeb\x84\xa3\xea\xb3\xa0 \xec\x8b\xa4\xed\x96\x89\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.\n\n"
                         "Game data (image/game.pal) not found. Put KiruBill.exe into the original game folder.",
                         "KiruBill");
        return 1;
    }
    mem_init();
    task_system_init();
    // C++ static constructors of the original (.data 0x41e004..0x41e00c):
    // 0x40b550 builds the replay recorder at 0x41f7b0; 0x411ee0 (input
    // binding lists) and 0x413120 (wave reader) are engine objects here.
    f_40b550();
    // 0x410a40: the window exists before the devices are initialised.
    trace_phase("start");
    if (!plat_video_init()) {
        plat_error(STR_41e724);
        return 0;
    }
    trace_phase("window ready");
    if (!app_init()) {
        app_shutdown();
        return 0;
    }
    // 0x410bd0: run a game frame whenever the message queue is empty.
    int frames_called = 0;
    for (;;) {
        if (!plat_pump_messages())
            break;
        if (g_quit)
            break;
        uint32_t before = MU32(0x41f830);   // frame counter of 0x40bf50
        int32_t scene_before = MI32(0x41f798);
        f_40bf50();
        if (g_opts.trace && MI32(0x41f798) != scene_before) {
            std::printf("frame %d: scene %d -> %d  score=%d combo=%d\n", frames_called, scene_before,
                        MI32(0x41f798), MI32(GS + 0x10), MI32(GS + 0x18));
            std::fflush(stdout);
        }
        if (MU32(0x41f830) != before) {
            ++frames_called;
            if (g_opts.max_frames >= 0 && frames_called >= g_opts.max_frames)
                break;
        } else if (!g_opts.headless) {
            plat_sleep_ms(1);   // the original busy-waited
        }
    }
    app_shutdown();
    test_report();
    return 0;
}
