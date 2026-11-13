// Interface between the portable engine and the OS backend
// (src/platform/win32.cpp for Windows, src/platform/sdl.cpp elsewhere).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct TextMask {
    int w = 0, h = 0;          // mask size
    int ox = 0, oy = 0;        // offset of the mask relative to the TextOut point
    std::vector<uint8_t> px;   // 1 = text pixel
};

struct PcmSound {
    int channels = 1, rate = 22050, bits = 8;
    std::vector<uint8_t> data; // raw PCM as stored in the .wav
};

// Video: present an indexed 640x480 frame with a palette of 256 RGBQUADs.
bool plat_video_init();
void plat_video_shutdown();
void plat_present(const uint8_t* indexed, const uint32_t* rgbquad);

// Text rendering with the game font (28px cell, 14px wide, Shift-JIS).
bool plat_text_mask(const char* sjis, int len, TextMask& out);

// Raw device state.
void plat_keyboard_state(uint8_t keys[256]);          // GetKeyboardState layout (VK codes, bit7 = down)
bool plat_joystick_state(uint8_t joy[256]);           // 0..3 directions, 4..19 buttons (0xff/0)

// Sound effects.
bool plat_snd_init();
void plat_snd_shutdown();
bool plat_snd_create(int idx, const PcmSound& pcm);
void plat_snd_release(int idx);
void plat_snd_play(int idx);
void plat_snd_stop(int idx);

// MIDI output (short messages); used by the engine's sequencer thread.
bool plat_midi_open();
void plat_midi_close();
void plat_midi_short(uint32_t msg);
void plat_midi_long(const uint8_t* data, size_t len); // SysEx

// Misc.
uint32_t plat_time_ms();
void     plat_sleep_ms(int ms);
bool     plat_pump_messages();     // false when the application should exit
void     plat_message_box(const std::string& utf8_text, const std::string& utf8_title);
std::string plat_sjis_to_utf8(const char* sjis, int len);

// Options parsed from the command line.
struct PlatOptions {
    bool windowed = false;
    bool force_gdi = false;  // Windows: present with GDI instead of OpenGL
    bool headless = false;   // no window / audio (automated testing)
    int  max_frames = -1;    // stop after N game frames (headless testing)
    bool trace = false;      // print scene changes and score (testing)
};
extern PlatOptions g_opts;
