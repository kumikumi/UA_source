#include <utility>

#include <SDL3/SDL.h>

#include "perf.h"

namespace System
{

PerfStats Perf;

// How long values are averaged over before the summary is updated.
static const float SummaryMs = 500.0;

uint64_t PerfStats::Now()
{
    return SDL_GetPerformanceCounter();
}

double PerfStats::TicksToMs(uint64_t ticks)
{
    return ticks * 1000.0 / SDL_GetPerformanceFrequency();
}

void PerfStats::StartRecording()
{
    _recorded.clear();
    _recording = true;
}

std::vector<PerfStats::FrameRecord> PerfStats::StopRecording()
{
    _recording = false;
    return std::move(_recorded);
}

PerfStats::Timer::Timer(Section sec)
: _sec(sec), _start(Now())
{}

PerfStats::Timer::~Timer()
{
    Perf.AddTime(_sec, Now() - _start);
}

void PerfStats::FrameStart()
{
    uint64_t now = Now();

    if (_frameStart)
    {
        uint64_t frameTicks = now - _frameStart;

        _windowFrames++;
        _windowFrameTicks += frameTicks;

        if (frameTicks > _windowWorstTicks)
            _windowWorstTicks = frameTicks;

        for (int i = 0; i < SEC_MAX; i++)
            _windowTicks[i] += _frameTicks[i];

        for (int i = 0; i < CNT_MAX; i++)
            _windowCounters[i] += _frameCounters[i];

        if (_recording)
        {
            FrameRecord rec;
            rec.FrameTicks = frameTicks;
            rec.SectionTicks = _frameTicks;
            rec.Counters = _frameCounters;
            _recorded.push_back(rec);
        }
    }
    else
    {
        _windowStart = now;
    }

    _frameTicks.fill(0);
    _frameCounters.fill(0);
    _frameStart = now;

    const double msPerTick = 1000.0 / SDL_GetPerformanceFrequency();

    if (!_windowFrames || (now - _windowStart) * msPerTick < SummaryMs)
        return;

    _summary.FrameMs = _windowFrameTicks * msPerTick / _windowFrames;
    _summary.Fps = 1000.0 / _summary.FrameMs;
    _summary.WorstMs = _windowWorstTicks * msPerTick;

    for (int i = 0; i < SEC_MAX; i++)
        _summary.SectionMs[i] = _windowTicks[i] * msPerTick / _windowFrames;

    for (int i = 0; i < CNT_MAX; i++)
        _summary.Counters[i] = (_windowCounters[i] + _windowFrames / 2) / _windowFrames;

    _windowStart = now;
    _windowFrames = 0;
    _windowFrameTicks = 0;
    _windowWorstTicks = 0;
    _windowTicks.fill(0);
    _windowCounters.fill(0);
}

}
