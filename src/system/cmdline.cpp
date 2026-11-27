#include <cstdio>
#include <map>
#include <string>

#include "cmdline.h"
#include "../utils.h"

namespace System
{
namespace CmdLine
{

struct Option
{
    const char *Name;
    const char *Value; // What the value is, for the usage; NULL takes none
    const char *Help;
};

static const Option Options[] = {
    {"env",          "DIR",     "Use the game folder's DIR instead of env/ for the game's setup: "
                                "Ghor_env and Taer_env play the Ghorkov and Taerkasten campaigns. "
                                "-env, as the original game took it, works too"},
    {"mod",          "NAME",    "Play the mod in mods/NAME: its files replace the game's, and it has "
                                "its own env/ and save/"},
    {"devmode",      NULL,      "Allow the keys this port added to levels and world.ini without a mod, "
                                "as devmode = on in nucleus.ini does"},
    {"renderer",     "gl|gpu",  "Draw with OpenGL or SDL_GPU, whatever the settings say"},
    {"gpudebug",     NULL,      "Check how SDL_GPU is used, which slows the drawing down"},
    {"level",        "N",       "Start level N as new, without the menus or the intro. After a "
                                "win comes the debriefing, and then the game quits. Nothing is "
                                "written to the player's profile, and the exit menu has no Save or "
                                "Load"},
    {"load",         "FILE",    "The same from the last keyframe of a saved game or a recording "
                                "(--record), such as SAVE/Unnamed2/0.sav, the saved game of the "
                                "profile Unnamed2"},
    {"step",         "N",       "With --load, go on from the keyframe at step N instead, one of "
                                "those every 600 steps; with --replay, start watching at step N"},
    {"replay",       "FILE",    "Watch a recording in the replay viewer, without the menus or the "
                                "intro, such as a replay saved in SAVE/Unnamed2/replays. The game "
                                "quits when the viewer is left"},
    {"headless",     NULL,      "Run the level --level or --load starts as fast as the machine can, "
                                "with nothing shown, heard or read from the controls, until the "
                                "level ends, the player's host station is destroyed or no enemy "
                                "host station is left. Needs no display, GPU or sound device. How "
                                "it ended goes to stdout"},
    {"time-limit",   "SECONDS", "With --headless, stop after this much game time"},
    {"ai-only",      NULL,      "With --headless, no one plays: the player's faction's AI plays it, as "
                                "the other factions' do, and the level ends when one faction's host "
                                "stations are all that's left"},
    {"check-resume", "STEP",    "With --headless, check that the state the game keeps is complete: "
                                "at step STEP, keep it, play 600 steps, start again from what was "
                                "kept and play them again. Reports whether every step came out the "
                                "same, or the first fields that didn't"},
    {"record",       "FILE",    "Record the level that --level or --load starts, until it ends, into "
                                "FILE: keyframes, the whole state the level can go on from, at its "
                                "start, every 10 s of game time and at its end, and between them "
                                "what every step changes of the game as drawn. One --load starts "
                                "from goes on from what it recorded up to there. tool/recording.py "
                                "reads it; the format is in src/world/recorder.h"},
    {"record-full",  NULL,      "With --record, a keyframe at every step, so that the level can go "
                                "on from any of them; much bigger"},
    {"host",         "N",       "Host a network game of level N: the game opens at its lobby, "
                                "without the intro, for the others to join"},
    {"join",         "ADDRESS", "Join the network game hosted at ADDRESS, a name or an IP address "
                                "(ADDRESS:PORT if the port isn't 61234), at its lobby"},
    {"callsign",     "NAME",    "With --host or --join, play under this name, not the last one "
                                "typed; with --watch, watch under it"},
    {"stream",       NULL,      "Let others watch the level --level or --load starts as it's played "
                                "(--watch), on port 61234. A level run --headless then goes at the "
                                "game's own speed"},
    {"watch",        "ADDRESS", "Watch the game streamed at ADDRESS (--stream) as it's played, in the "
                                "replay viewer; ADDRESS:PORT if the port isn't 61234. With --headless "
                                "nothing is shown: what came and how fast goes to stdout"},
    {"faction",      "N",       "With --watch, play faction N (1-7) of the game watched, with anyone "
                                "else playing it; the faction's AI stops while someone does"},
    {"bench",        NULL,      "Run the benchmark: level 21 drawn over and over from above, the frame "
                                "times reported"},
    {"bench-level",  "N",       "The benchmark's level instead"},
    {"bench-view",   "default", "The game's default view distance, not an unlimited one"},
    {"bench-frames", "N",       "Frames to measure, 600 by default"},
    {"bench-out",    "FILE",    "Where the report goes, benchmark.txt by default"},
    {"bench-csv",    "FILE",    "Every measured frame's times as well"},
    {"bench-shot",   "FILE",    "The last frame as a PNG, benchmark.png by default"},
    {"help",         NULL,      "This list. -h works too"},
};

static std::map<std::string, std::string> Given; // Name, value
static std::string Program = "UA_source";

static const Option *FindOption(const std::string &name)
{
    for (const Option &opt : Options)
    {
        if (name == opt.Name)
            return &opt;
    }

    return NULL;
}

// The usage, each option's text wrapped to fit 80 columns
static void PrintUsage(FILE *out, const std::string &program)
{
    const size_t textColumn = 26;
    const size_t width = 79;

    fprintf(out, "Usage: %s [options]\n", program.c_str());
    fprintf(out, "Start it in the game's folder.\n\n");
    fprintf(out, "Options:\n");

    for (const Option &opt : Options)
    {
        std::string line = std::string("  --") + opt.Name;
        if (opt.Value)
            line += std::string(" ") + opt.Value;

        if (line.size() + 2 > textColumn)
        {
            fprintf(out, "%s\n", line.c_str());
            line.clear();
        }

        std::string text = opt.Help;
        size_t pos = 0;

        while (pos < text.size())
        {
            line.resize(textColumn, ' ');

            size_t room = width - textColumn;
            size_t end = text.size();

            if (end - pos > room)
            {
                end = text.rfind(' ', pos + room);
                if (end == std::string::npos || end <= pos)
                    end = pos + room;
            }

            line += text.substr(pos, end - pos);
            fprintf(out, "%s\n", line.c_str());
            line.clear();

            pos = end;
            while (pos < text.size() && text[pos] == ' ')
                pos++;
        }
    }
}

static ParseResult Fail(const std::string &program, const std::string &problem)
{
    PrintError(problem);
    fprintf(stderr, "\n");
    PrintUsage(stderr, program);
    return PARSE_ERROR;
}

ParseResult Parse(int argc, char *argv[])
{
    std::string program = argc > 0 ? argv[0] : "UA_source";
    size_t slash = program.find_last_of("/\\");
    if (slash != std::string::npos)
        program = program.substr(slash + 1);

    Program = program;

    Given.clear();

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        std::string name;
        std::string value;
        bool inlineValue = false;

        if (arg == "-h")
            name = "help";
        else if (StriCmp(arg, "-env") == 0) // The original game's spelling
            name = "env";
        else if (arg.size() > 2 && arg.compare(0, 2, "--") == 0)
        {
            name = arg.substr(2);

            size_t equals = name.find('=');
            if (equals != std::string::npos)
            {
                value = name.substr(equals + 1);
                name.resize(equals);
                inlineValue = true;
            }
        }
        else if (arg.size() > 1 && arg[0] == '-' && FindOption(arg.substr(1)))
            return Fail(program, arg + " is spelled -" + arg + " now");
        else
            return Fail(program, "not an option: " + arg);

        const Option *opt = FindOption(name);
        if (!opt)
            return Fail(program, "unknown option: " + arg);

        if (opt->Value)
        {
            if (!inlineValue)
            {
                if (i + 1 >= argc)
                    return Fail(program, "--" + name + " needs " + opt->Value);

                value = argv[++i];
            }
        }
        else if (inlineValue)
            return Fail(program, "--" + name + " takes no value");

        Given[name] = value;
    }

    if (Has("help"))
    {
        PrintUsage(stdout, program);
        return PARSE_HELP;
    }

    return PARSE_OK;
}

void PrintError(const std::string &problem)
{
    fprintf(stderr, "%s: %s\n", Program.c_str(), problem.c_str());
}

bool Has(const std::string &name)
{
    return Given.count(name) != 0;
}

bool Get(const std::string &name, std::string *value)
{
    auto it = Given.find(name);
    if (it == Given.end())
        return false;

    *value = it->second;
    return true;
}

}
}
