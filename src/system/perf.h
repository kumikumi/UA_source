#ifndef UA_SYSTEM_PERF_H_INCLUDED
#define UA_SYSTEM_PERF_H_INCLUDED

#include <array>
#include <cstdint>
#include <vector>

namespace System
{

// Frame timing and renderer counters for the performance overlay (F10).
// Times and counts are collected per frame and averaged over half a second,
// so the numbers stay readable while still following changes quickly.
class PerfStats
{
public:
    enum Section
    {
        SEC_SIM,     // Unit updates: AI, movement, weapons
        SEC_SCENE,   // Walking the world and queuing meshes to draw
        SEC_DRAW,    // Issuing the queued draw calls
        SEC_GUI,     // Drawing the 2D interface
        SEC_PRESENT, // Uploading the 2D layer and swapping buffers
        SEC_IDLE,    // Sleeping to keep to the fps cap
        SEC_MAX
    };

    enum Counter
    {
        CNT_DRAWCALLS,
        CNT_TRIANGLES,
        CNT_TEXCHANGES,
        CNT_PARTICLES, // Particles drawn, each with its own draw call
        CNT_UNITS,     // Units drawn
        CNT_MAX
    };

    struct Summary
    {
        float Fps = 0.0;
        float FrameMs = 0.0; // Average time between frames
        float WorstMs = 0.0; // Longest time between frames
        std::array<float, SEC_MAX> SectionMs {};
        std::array<uint32_t, CNT_MAX> Counters {}; // Averages per frame
    };

    // One frame as measured, for the benchmark.
    struct FrameRecord
    {
        uint64_t FrameTicks = 0; // From its start to the next frame's
        std::array<uint64_t, SEC_MAX> SectionTicks {};
        std::array<uint64_t, CNT_MAX> Counters {};
    };

    // Adds the time from its construction to its destruction to a section.
    class Timer
    {
    public:
        explicit Timer(Section sec);
        ~Timer();

    private:
        Section _sec;
        uint64_t _start;
    };

    // Called once at the start of every rendered frame.
    void FrameStart();

    void AddTime(Section sec, uint64_t ticks) { _frameTicks[sec] += ticks; }
    void Count(Counter cnt, uint32_t n = 1) { _frameCounters[cnt] += n; }

    const Summary &GetSummary() const { return _summary; }

    // Keeps every frame that ends from now on, until StopRecording.
    void StartRecording();
    std::vector<FrameRecord> StopRecording();

    static uint64_t Now();
    static double TicksToMs(uint64_t ticks);

private:
    std::array<uint64_t, SEC_MAX> _frameTicks {};
    std::array<uint64_t, CNT_MAX> _frameCounters {};
    uint64_t _frameStart = 0;

    uint64_t _windowStart = 0;
    uint32_t _windowFrames = 0;
    uint64_t _windowFrameTicks = 0;
    uint64_t _windowWorstTicks = 0;
    std::array<uint64_t, SEC_MAX> _windowTicks {};
    std::array<uint64_t, CNT_MAX> _windowCounters {};

    Summary _summary;

    bool _recording = false;
    std::vector<FrameRecord> _recorded;
};

extern PerfStats Perf;

}

#endif
