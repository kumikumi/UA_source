#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3_image/SDL_image.h>

#include "benchmark.h"
#include "yw.h"
#include "log.h"
#include "system/system.h"
#include "fmtlib/printf.h"

// How far above the map's highest ground the camera is.
static const float CameraHeight = 400.0;

static const std::array<const char *, System::PerfStats::SEC_MAX> SectionNames = {
    "sim", "scene", "draw", "gui", "present", "wait", "idle"
};

static const std::array<const char *, System::PerfStats::CNT_MAX> CounterNames = {
    "draw calls", "triangles", "texture changes", "particles", "units"
};

// The value following an option, if the option is given.
static bool GetOptionValue(const std::string &option, std::string *value)
{
    std::vector<std::string> &cmdl = System::GetCmdLineArray();
    int32_t i = System::FindCmdLineArg(option);

    if (i < 0 || i + 1 >= (int32_t)cmdl.size())
        return false;

    *value = cmdl[i + 1];
    return true;
}

static bool GetOptionNumber(const std::string &option, int minValue, int *value)
{
    std::string str;
    if ( !GetOptionValue(option, &str) )
        return false;

    int number = std::atoi(str.c_str());
    if (number < minValue)
    {
        ypa_log_out("Benchmark: %s %s is not a number from %d up, keeping %d\n", option.c_str(), str.c_str(), minValue, *value);
        return false;
    }

    *value = number;
    return true;
}

bool Benchmark::ReadCommandLine()
{
    if ( System::FindCmdLineArg("-bench") < 0 )
        return false;

    GetOptionNumber("-bench-level", 1, &_level);
    GetOptionNumber("-bench-frames", 1, &_frames);
    GetOptionValue("-bench-out", &_outFile);
    GetOptionValue("-bench-csv", &_csvFile);
    GetOptionValue("-bench-shot", &_shotFile);

    std::string view;
    if ( GetOptionValue("-bench-view", &view) )
        _unlimitedView = (StriCmp(view, "default") != 0);

    _phase = PHASE_STARTING;
    return true;
}

bool Benchmark::FrameDrawn(NC_STACK_ypaworld *yw)
{
    if ( _phase == PHASE_STARTING )
    {
        if ( yw->_framesElapsed > 0 )
        {
            Freeze(yw);
            _phase = PHASE_WARMUP;
            _framesDone = 0;
            _screenSize = yw->_screenSize;
        }
        return true;
    }

    // All the measured frames have the same size: going fullscreen, for one,
    // takes a moment after the start.
    if ( yw->_screenSize != _screenSize )
    {
        ypa_log_out("Benchmark: the window changed size to %dx%d, warming up again\n", yw->_screenSize.x, yw->_screenSize.y);

        System::Perf.StopRecording();
        _phase = PHASE_WARMUP;
        _framesDone = 0;
        _screenSize = yw->_screenSize;
        return true;
    }

    if ( _phase == PHASE_WARMUP )
    {
        if ( ++_framesDone >= _warmupFrames )
        {
            System::Perf.StartRecording();
            _phase = PHASE_MEASURING;
            _framesDone = 0;
        }
        return true;
    }

    if ( ++_framesDone < _frames )
        return true;

    std::vector<System::PerfStats::FrameRecord> frames = System::Perf.StopRecording();

    WriteScreenshot();
    WriteReport(yw, frames);

    if ( !_csvFile.empty() )
        WriteCsv(frames);

    _phase = PHASE_OFF;
    return false;
}

void Benchmark::Freeze(NC_STACK_ypaworld *yw)
{
    if ( _unlimitedView )
    {
        TViewDistance view;
        view.SectorRadius = TViewDistance::ALL_SECTORS;
        view.ClipDistance = TViewDistance::UNLIMITED;
        view.FogEnd = TViewDistance::UNLIMITED;
        view.FadeOutEnd = TViewDistance::UNLIMITED;
        yw->SetViewDistance(view);
    }
    else
    {
        yw->ResetViewDistance();
    }

    // As many frames as the machine can draw
    GFX::Engine.SetVsync(false);
    GFX::Engine.FpsMaxTicks = 0;

    PlaceCamera(yw);
}

void Benchmark::PlaceCamera(NC_STACK_ypaworld *yw)
{
    // Over the playable corner sector at the bottom left of the map, above
    // the highest ground anywhere on it, looking down at its middle. Up is
    // towards -y.
    Common::Point from(1, yw->_mapSize.y - 2);
    Common::Point to(yw->_mapSize.x / 2, yw->_mapSize.y / 2);

    float highest = 0.0;
    for (const cellArea &cell : yw->_cells)
        highest = std::min(highest, cell.height);

    vec3d eye = World::SectorIDToCenterPos3(from) + vec3d::OY(highest - CameraHeight);
    vec3d target = World::SectorIDToCenterPos3(to) + vec3d::OY(yw->_cells(to).height);

    // The view looks along the matrix's third row, with the first to the
    // right and the second down, as for the units' rotations. * is the
    // cross product.
    vec3d forward = target - eye;
    forward.normalise();

    vec3d right = vec3d::OY(1.0) * forward;
    right.normalise();

    vec3d down = forward * right;

    _camera.Pos = eye;
    _camera.SclRot = mat3x3::Basis(right, down, forward);
    GFX::Engine.matrixAspectCorrection(_camera.SclRot, false);

    TF::Engine.SetViewPoint(&_camera);

    // The sectors drawn and the sky are placed around the viewer, so move
    // it to the camera. Only the viewer itself: a host station would take
    // its guns along. Host stations are drawn whatever sector they are in,
    // and with the simulation standing still, this one where it was.
    TUnitPlacement placement;
    placement.pos = eye;
    placement.flags = 2; // Exactly there
    yw->_viewerBact->NC_STACK_ypabact::SetPosition(&placement);

    _cameraText = fmt::sprintf("over sector %d,%d %.0f above the highest ground, looking at sector %d,%d",
                               from.x, from.y, CameraHeight, to.x, to.y);
}

struct TimeStats
{
    double Mean = 0.0;
    double Median = 0.0;
    double P95 = 0.0;
    double P99 = 0.0;
    double Min = 0.0;
    double Max = 0.0;
};

static TimeStats MakeStats(std::vector<double> ms)
{
    TimeStats st;
    if ( ms.empty() )
        return st;

    std::sort(ms.begin(), ms.end());

    // The value that this share of the frames does not exceed
    auto percentile = [&ms](double share)
    {
        size_t i = std::ceil(share * ms.size());
        return ms[ std::min(std::max(i, (size_t)1), ms.size()) - 1 ];
    };

    double sum = 0.0;
    for (double v : ms)
        sum += v;

    st.Mean = sum / ms.size();
    st.Median = percentile(0.5);
    st.P95 = percentile(0.95);
    st.P99 = percentile(0.99);
    st.Min = ms.front();
    st.Max = ms.back();
    return st;
}

static std::string FormatStats(const char *name, const TimeStats &st)
{
    return fmt::sprintf("%-8s %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f\n",
                        name, st.Mean, st.Median, st.P95, st.P99, st.Min, st.Max);
}

static std::string ViewDistanceText(NC_STACK_ypaworld *yw)
{
    TViewDistance view = yw->GetViewDistance();

    std::string sectors = (view.SectorRadius >= yw->GetMapSectorRadius()) ? "all" : fmt::sprintf("%d around", view.SectorRadius);
    std::string clip = (view.ClipDistance == TViewDistance::UNLIMITED) ? "none" : fmt::sprintf("%.0f", view.ClipDistance);
    std::string fog = (view.FogEnd == TViewDistance::UNLIMITED) ? "none" : fmt::sprintf("%.0f to %.0f", view.FogStart, view.FogEnd);
    std::string fade = (view.FadeOutEnd == TViewDistance::UNLIMITED) ? "none" : fmt::sprintf("%.0f to %.0f", view.FadeOutStart, view.FadeOutEnd);

    return fmt::sprintf("sectors %s, clip %s, fog %s, fade-out %s%s", sectors, clip, fog, fade,
                        yw->IsViewDistanceSet() ? "" : " (default)");
}

bool Benchmark::WriteScreenshot()
{
    int w = 0, h = 0;
    uint8_t *pixels = GFX::Engine.MakeScreenCopy(&w, &h);

    if ( !pixels )
        return false;

    // The copy has the bottom line first
    SDL_Surface *img = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    bool saved = false;

    if ( img )
    {
        for (int y = 0; y < h; y++)
            memcpy((uint8_t *)img->pixels + y * img->pitch, pixels + (h - 1 - y) * w * 3, w * 3);

        saved = IMG_SavePNG(img, _shotFile.c_str());
        SDL_DestroySurface(img);
    }

    free(pixels);

    if ( !saved )
        ypa_log_out("Benchmark: could not write %s\n", _shotFile.c_str());

    return saved;
}

bool Benchmark::WriteReport(NC_STACK_ypaworld *yw, const std::vector<System::PerfStats::FrameRecord> &frames)
{
    using PS = System::PerfStats;

    std::string report = "Urban Assault benchmark\n";
    report += fmt::sprintf("Level %d, %dx%d sectors, camera %s\n", _level, yw->_mapSize.x, yw->_mapSize.y, _cameraText);
    report += fmt::sprintf("View distance: %s\n", ViewDistanceText(yw));
    report += GFX::Engine.GetRendererInfo() + "\n";
    report += fmt::sprintf("%d frames measured after %d to warm up\n\n", (int)frames.size(), _warmupFrames);

    report += fmt::sprintf("%-8s %8s %8s %8s %8s %8s %8s\n", "ms", "mean", "median", "p95", "p99", "min", "max");

    std::vector<double> ms(frames.size());

    for (size_t i = 0; i < frames.size(); i++)
        ms[i] = PS::TicksToMs(frames[i].FrameTicks);

    TimeStats frameStats = MakeStats(ms);
    report += FormatStats("frame", frameStats);

    for (int sec = 0; sec < PS::SEC_MAX; sec++)
    {
        for (size_t i = 0; i < frames.size(); i++)
            ms[i] = PS::TicksToMs(frames[i].SectionTicks[sec]);

        report += FormatStats(SectionNames[sec], MakeStats(ms));
    }

    if ( frameStats.Mean > 0.0 )
        report += fmt::sprintf("\n%.1f fps from the mean frame time\n", 1000.0 / frameStats.Mean);

    report += "Per frame:";

    for (int cnt = 0; cnt < PS::CNT_MAX; cnt++)
    {
        uint64_t sum = 0;
        for (const PS::FrameRecord &rec : frames)
            sum += rec.Counters[cnt];

        uint64_t perFrame = frames.empty() ? 0 : (sum + frames.size() / 2) / frames.size();
        report += fmt::sprintf("%s %d %s", (cnt ? "," : ""), (int)perFrame, CounterNames[cnt]);
    }

    report += "\n";

    fputs(report.c_str(), stdout);
    fflush(stdout);

    FILE *fil = fopen(_outFile.c_str(), "w");
    if ( !fil )
    {
        ypa_log_out("Benchmark: could not write %s\n", _outFile.c_str());
        return false;
    }

    fputs(report.c_str(), fil);
    fclose(fil);
    return true;
}

bool Benchmark::WriteCsv(const std::vector<System::PerfStats::FrameRecord> &frames)
{
    using PS = System::PerfStats;

    FILE *fil = fopen(_csvFile.c_str(), "w");
    if ( !fil )
    {
        ypa_log_out("Benchmark: could not write %s\n", _csvFile.c_str());
        return false;
    }

    fputs("frame_ms", fil);

    for (const char *name : SectionNames)
        fprintf(fil, ",%s_ms", name);

    for (const char *name : CounterNames)
    {
        std::string col = name;
        std::replace(col.begin(), col.end(), ' ', '_');
        fprintf(fil, ",%s", col.c_str());
    }

    fputs("\n", fil);

    for (const PS::FrameRecord &rec : frames)
    {
        fprintf(fil, "%.4f", PS::TicksToMs(rec.FrameTicks));

        for (uint64_t ticks : rec.SectionTicks)
            fprintf(fil, ",%.4f", PS::TicksToMs(ticks));

        for (uint64_t count : rec.Counters)
            fprintf(fil, ",%llu", (unsigned long long)count);

        fputs("\n", fil);
    }

    fclose(fil);
    return true;
}
