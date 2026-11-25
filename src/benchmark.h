#ifndef UA_BENCHMARK_H_INCLUDED
#define UA_BENCHMARK_H_INCLUDED

#include <string>
#include <vector>

#include "common/common.h"
#include "system/perf.h"

class NC_STACK_ypaworld;

// Draws the same frame over and over and reports how long the frames took,
// to compare renderers and to catch slowdowns:
//
//   --bench               Level 21, seen from above one corner of the map
//                         towards its middle, with nothing limiting the view
//   --bench-level N       Another level
//   --bench-view default  The game's default view distance instead
//   --bench-frames N      Frames to measure, 600 by default
//   --bench-out FILE      Where the report goes, benchmark.txt by default
//   --bench-csv FILE      Every measured frame's times as well
//   --bench-shot FILE     The last frame as a PNG, benchmark.png by default
//
// The level starts as new, without the player's buddies, and stands still
// after its first step, so all that varies is the drawing. It runs
// fullscreen, at the display's resolution, where the frame times also vary
// much less than in a window.
class Benchmark
{
public:
    // False unless --bench is given.
    bool ReadCommandLine();

    bool IsActive() const { return _phase != PHASE_OFF; }
    int  GetLevel() const { return _level; }

    // The simulation stands still and every frame shows the same.
    bool IsFrozen() const { return _phase >= PHASE_WARMUP; }

    // Called after each frame the game draws. False once the benchmark is
    // done and the game should quit.
    bool FrameDrawn(NC_STACK_ypaworld *yw);

protected:
    enum Phase
    {
        PHASE_OFF,
        PHASE_STARTING,  // Until the level has taken its first step
        PHASE_WARMUP,
        PHASE_MEASURING
    };

    void Freeze(NC_STACK_ypaworld *yw);
    void PlaceCamera(NC_STACK_ypaworld *yw);
    bool WriteScreenshot();
    bool WriteReport(NC_STACK_ypaworld *yw, const std::vector<System::PerfStats::FrameRecord> &frames);
    bool WriteCsv(const std::vector<System::PerfStats::FrameRecord> &frames);

protected:
    Phase _phase = PHASE_OFF;

    int  _level = 21;
    bool _unlimitedView = true;
    int  _warmupFrames = 120;
    int  _frames = 600;
    std::string _outFile = "benchmark.txt";
    std::string _csvFile;
    std::string _shotFile = "benchmark.png";

    int _framesDone = 0;
    Common::Point _screenSize; // Of the frames measured
    std::string _cameraText; // For the report
};

#endif
