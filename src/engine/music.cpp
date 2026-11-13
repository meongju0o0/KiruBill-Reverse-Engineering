// Music – replacement for the DirectMusic wrapper (0x412a90..0x413120).
//
// The original loads Standard MIDI Files as DirectMusic segments, sets loop
// points in MUSIC_TIME (768 ticks per quarter note) with 0xffff repeats and
// plays them as the primary segment on the default synth (Microsoft GS
// soundset).  This sequencer reproduces that: notes are converted to
// (start, duration) pairs like the DirectMusic MIDI importer does, so a note
// that is still sounding when the segment loops keeps its full length, and
// MIDI output goes to the system synth (on Windows the Microsoft GS
// Wavetable Synth, which uses the same Roland GS sound set as DirectMusic).
#include "engine.h"
#include "platform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

constexpr int kPPQ = 768;   // DMUS_PPQ

enum EvType : uint8_t { EV_SHORT, EV_NOTE, EV_TEMPO, EV_SYSEX };

struct Event {
    int32_t mt = 0;            // music time
    EvType type = EV_SHORT;
    uint32_t msg = 0;          // packed short message (status | d1<<8 | d2<<16)
    int32_t dur = 0;           // EV_NOTE duration in music time
    int32_t tempo = 500000;    // EV_TEMPO microseconds per quarter
    std::vector<uint8_t> sysex;
};

struct Segment {
    std::vector<Event> ev;     // sorted by mt (stable)
    int32_t length = 0;        // music time of the last event / note end
    int32_t loop_start = 0, loop_end = 0;
    int repeats = 0;
};

std::unique_ptr<Segment> g_seg[20];
std::mutex g_mx;
std::thread g_thread;
std::atomic<bool> g_run{false};
bool g_midi_ok = false;

// playback state (guarded by g_mx)
int     g_cur = -1;
double  g_seg_mt = 0, g_perf_mt = 0;
size_t  g_cursor = 0;
int32_t g_tempo = 500000;
int     g_repeats_left = 0;
struct Off { double perf_mt; uint8_t ch, key; bool operator<(const Off& o) const { return perf_mt > o.perf_mt; } };
std::priority_queue<Off> g_offs;
std::chrono::steady_clock::time_point g_last;

uint32_t rd32be(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

bool read_vlq(const std::vector<uint8_t>& d, size_t& p, size_t end, uint32_t& v)
{
    v = 0;
    for (int i = 0; i < 4; ++i) {
        if (p >= end)
            return false;
        uint8_t b = d[p++];
        v = (v << 7) | (b & 0x7f);
        if (!(b & 0x80))
            return true;
    }
    return true;
}

std::unique_ptr<Segment> load_smf(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return nullptr;
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
    if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4))
        return nullptr;
    uint32_t hlen = rd32be(&d[4]);
    int ntracks = (d[10] << 8) | d[11];
    int division = (d[12] << 8) | d[13];
    if (division & 0x8000 || division == 0)
        return nullptr;  // SMPTE time not used by the game
    auto seg = std::make_unique<Segment>();
    auto to_mt = [&](uint64_t tick) { return (int32_t)(tick * kPPQ / (uint64_t)division); };

    size_t p = 8 + hlen;
    for (int t = 0; t < ntracks && p + 8 <= d.size(); ++t) {
        if (std::memcmp(&d[p], "MTrk", 4)) {
            p += 8 + rd32be(&d[p + 4]);
            continue;
        }
        size_t end = std::min(d.size(), p + 8 + (size_t)rd32be(&d[p + 4]));
        p += 8;
        uint64_t tick = 0;
        uint8_t running = 0;
        // open notes per channel/key: index into seg->ev (FIFO)
        std::vector<size_t> open[16][128];
        while (p < end) {
            uint32_t delta;
            if (!read_vlq(d, p, end, delta))
                break;
            tick += delta;
            if (p >= end)
                break;
            uint8_t st = d[p];
            if (st & 0x80)
                ++p;
            else
                st = running;
            if (st == 0xff) {
                if (p >= end)
                    break;
                uint8_t type = d[p++];
                uint32_t len;
                if (!read_vlq(d, p, end, len) || p + len > end)
                    break;
                if (type == 0x51 && len == 3) {
                    Event e;
                    e.mt = to_mt(tick);
                    e.type = EV_TEMPO;
                    e.tempo = (int32_t)((d[p] << 16) | (d[p + 1] << 8) | d[p + 2]);
                    seg->ev.push_back(e);
                }
                if (type == 0x2f) {
                    seg->length = std::max(seg->length, to_mt(tick));
                    p += len;
                    break;
                }
                p += len;
                continue;
            }
            if (st == 0xf0 || st == 0xf7) {
                uint32_t len;
                if (!read_vlq(d, p, end, len) || p + len > end)
                    break;
                Event e;
                e.mt = to_mt(tick);
                e.type = EV_SYSEX;
                if (st == 0xf0)
                    e.sysex.push_back(0xf0);
                e.sysex.insert(e.sysex.end(), d.begin() + (long)p, d.begin() + (long)(p + len));
                seg->ev.push_back(std::move(e));
                p += len;
                continue;
            }
            if (!(st & 0x80))
                break;  // corrupt running status
            running = st;
            int nd = ((st & 0xe0) == 0xc0) ? 1 : 2;
            if (p + nd > end)
                break;
            uint8_t a = d[p], b = nd == 2 ? d[p + 1] : 0;
            p += nd;
            int ch = st & 15;
            int32_t mt = to_mt(tick);
            if ((st & 0xf0) == 0x90 && b > 0) {
                Event e;
                e.mt = mt;
                e.type = EV_NOTE;
                e.msg = st | (a << 8) | (b << 16);
                e.dur = -1;
                open[ch][a & 127].push_back(seg->ev.size());
                seg->ev.push_back(e);
            } else if ((st & 0xf0) == 0x80 || (st & 0xf0) == 0x90) {
                auto& q = open[ch][a & 127];
                if (!q.empty()) {
                    Event& on = seg->ev[q.front()];
                    on.dur = mt - on.mt;
                    q.erase(q.begin());
                }
            } else {
                Event e;
                e.mt = mt;
                e.type = EV_SHORT;
                e.msg = st | (a << 8) | (b << 16);
                seg->ev.push_back(e);
            }
        }
        // notes never released last until the end of their track
        for (auto& chs : open)
            for (auto& q : chs)
                for (size_t i : q)
                    seg->ev[i].dur = std::max<int32_t>(1, to_mt(tick) - seg->ev[i].mt);
        p = end;
    }
    std::stable_sort(seg->ev.begin(), seg->ev.end(), [](const Event& x, const Event& y) { return x.mt < y.mt; });
    for (const Event& e : seg->ev)
        seg->length = std::max(seg->length, e.mt + (e.type == EV_NOTE ? e.dur : 0));
    return seg;
}

void all_notes_off_locked()
{
    while (!g_offs.empty()) {
        const Off& o = g_offs.top();
        plat_midi_short(0x80u | o.ch | ((uint32_t)o.key << 8));
        g_offs.pop();
    }
    for (int ch = 0; ch < 16; ++ch) {
        plat_midi_short(0xb0u | ch | (123u << 8));  // all notes off
        plat_midi_short(0xb0u | ch | (120u << 8));  // all sound off
    }
}

int32_t tempo_at(const Segment& s, int32_t mt)
{
    int32_t t = 500000;
    for (const Event& e : s.ev) {
        if (e.mt > mt)
            break;
        if (e.type == EV_TEMPO)
            t = e.tempo;
    }
    return t;
}

void dispatch(const Event& e)
{
    switch (e.type) {
    case EV_SHORT:
        plat_midi_short(e.msg);
        break;
    case EV_NOTE:
        plat_midi_short(e.msg);
        g_offs.push({g_perf_mt + e.dur, (uint8_t)(e.msg & 15), (uint8_t)((e.msg >> 8) & 127)});
        break;
    case EV_TEMPO:
        g_tempo = e.tempo > 0 ? e.tempo : 500000;
        break;
    case EV_SYSEX:
        plat_midi_long(e.sysex.data(), e.sysex.size());
        break;
    }
}

// Advance playback by `us` microseconds of real time.
void advance_locked(double us)
{
    while (us > 0) {
        double mt_avail = us * kPPQ / g_tempo;
        double next = mt_avail;
        Segment* s = (g_cur >= 0) ? g_seg[g_cur].get() : nullptr;
        if (s) {
            if (g_cursor < s->ev.size())
                next = std::min(next, s->ev[g_cursor].mt - g_seg_mt);
            if (g_repeats_left > 0 && s->loop_end > s->loop_start)
                next = std::min(next, s->loop_end - g_seg_mt);
        }
        if (!g_offs.empty())
            next = std::min(next, g_offs.top().perf_mt - g_perf_mt);
        if (next < 0)
            next = 0;
        g_seg_mt += next;
        g_perf_mt += next;
        us -= next * g_tempo / kPPQ;
        if (next >= mt_avail)
            us = 0;

        while (!g_offs.empty() && g_offs.top().perf_mt <= g_perf_mt + 1e-9) {
            const Off& o = g_offs.top();
            plat_midi_short(0x80u | o.ch | ((uint32_t)o.key << 8));
            g_offs.pop();
        }
        if (!s)
            continue;
        if (g_repeats_left > 0 && s->loop_end > s->loop_start && g_seg_mt >= s->loop_end - 1e-9) {
            --g_repeats_left;
            g_seg_mt = s->loop_start;
            g_tempo = tempo_at(*s, s->loop_start);
            g_cursor = 0;
            while (g_cursor < s->ev.size() && s->ev[g_cursor].mt < s->loop_start)
                ++g_cursor;
            continue;
        }
        while (g_cursor < s->ev.size() && s->ev[g_cursor].mt <= g_seg_mt + 1e-9)
            dispatch(s->ev[g_cursor++]);
        if (g_cursor >= s->ev.size() && g_seg_mt >= s->length && g_offs.empty())
            g_cur = -1;   // segment finished
    }
}

void thread_main()
{
    g_last = std::chrono::steady_clock::now();
    while (g_run) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto now = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(now - g_last).count();
        g_last = now;
        if (us > 200000)
            us = 200000;  // after a stall, do not burst
        std::lock_guard<std::mutex> lk(g_mx);
        advance_locked(us);
    }
}

} // namespace

bool mus_init()
{
    g_midi_ok = plat_midi_open();
    g_run = true;
    g_thread = std::thread(thread_main);
    return true;
}

void mus_shutdown()
{
    {
        std::lock_guard<std::mutex> lk(g_mx);
        all_notes_off_locked();
        g_cur = -1;
    }
    if (g_run) {
        g_run = false;
        if (g_thread.joinable())
            g_thread.join();
    }
    for (auto& s : g_seg)
        s.reset();
    if (g_midi_ok)
        plat_midi_close();
    g_midi_ok = false;
}

void mus_load(const char* midi, int idx)
{
    if (idx < 0 || idx >= 20)
        return;
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_cur == idx) {
        all_notes_off_locked();
        g_cur = -1;
    }
    g_seg[idx] = load_smf(midi);
    if (!g_seg[idx])
        std::fprintf(stderr, "music: cannot load %s\n", midi);
}

void mus_unload(int idx)
{
    if (idx < 0 || idx >= 20)
        return;
    mus_stop(idx);
    std::lock_guard<std::mutex> lk(g_mx);
    g_seg[idx].reset();
}

void mus_play(int idx)
{
    if (idx < 0 || idx >= 20)
        return;
    std::lock_guard<std::mutex> lk(g_mx);
    if (!g_seg[idx])
        return;
    all_notes_off_locked();   // the new primary segment replaces the old one
    for (int ch = 0; ch < 16; ++ch)
        plat_midi_short(0xb0u | ch | (121u << 8));  // reset all controllers
    g_cur = idx;
    g_seg_mt = 0;
    g_cursor = 0;
    g_tempo = 500000;
    g_repeats_left = g_seg[idx]->repeats;
    g_last = std::chrono::steady_clock::now();
    advance_locked(0);
    while (g_cursor < g_seg[idx]->ev.size() && g_seg[idx]->ev[g_cursor].mt <= 0)
        dispatch(g_seg[idx]->ev[g_cursor++]);
}

void mus_stop(int idx)
{
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_cur == idx) {
        all_notes_off_locked();
        g_cur = -1;
    }
}

void mus_set_loop(int idx, int32_t start, int32_t end)
{
    if (idx < 0 || idx >= 20)
        return;
    std::lock_guard<std::mutex> lk(g_mx);
    if (!g_seg[idx])
        return;
    g_seg[idx]->loop_start = start;
    g_seg[idx]->loop_end = end;
    g_seg[idx]->repeats = 0xffff;   // SetRepeats(0xffff)
}
