#ifndef SYSTEM_CMDLINE_H_INCLUDED
#define SYSTEM_CMDLINE_H_INCLUDED

#include <string>

namespace System
{

// The launch options: --name, with a value as --name VALUE or --name=VALUE.
// --help lists them all. The original game's -env is taken as --env.
namespace CmdLine
{
    enum ParseResult
    {
        PARSE_OK,
        PARSE_HELP,  // The usage went to stdout, as --help asked
        PARSE_ERROR, // An option not known, or without its value: the
                     // problem and the usage went to stderr
    };

    ParseResult Parse(int argc, char *argv[]);

    // Names without the dashes
    bool Has(const std::string &name);
    bool Get(const std::string &name, std::string *value);

    // A problem with what the options ask for, to stderr as
    // "UA_source: problem"
    void PrintError(const std::string &problem);
}

}

#endif
