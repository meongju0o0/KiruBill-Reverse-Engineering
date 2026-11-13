// Automated-testing hooks shared by all backends (never active in normal play).
//
//   --headless            no window, no audio; deterministic virtual clock
//   --frames N            quit after N game frames
//   --trace               print scene changes with score / combo to stdout
//   --input FILE          scripted keys: lines "<keyboard poll #> <vk> <0|1>"
//   --shots DIR [--every N]  save every N-th presented frame as a 24-bit BMP
//   --find FILE.bmp       report the presented frame closest to a reference
#pragma once

#include <cstdint>
#include <string>
#include <vector>

void test_parse_args(const std::vector<std::string>& args);
// Called right after the keyboard state is read (before key bindings).
void test_apply_keyboard(uint8_t keys[256]);
// Called for every presented frame (also in headless mode).
void test_on_present(const uint8_t* indexed, const uint32_t* rgbquad);
// Prints the --find result.
void test_report();
