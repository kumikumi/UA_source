#ifndef NET_VIEWSERVER_H_INCLUDED
#define NET_VIEWSERVER_H_INCLUDED

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "session.h"

struct z_stream_s;

namespace World { class Recorder; }

namespace Net
{

// What those who watch a game and the game that's watched send each other,
// to tell them from other sessions
constexpr const char *VIEW_VERSION = "ua-view-1";

// The game's side of others watching it as it's played (--stream): whoever
// joins gets the level and the view, then every step (World::ViewStream),
// each watcher through a zlib stream of their own, flushed at each message,
// on the reliable channel.
class ViewServer
{
public:
    ViewServer();
    ~ViewServer();

    // Lets others watch on the port. False when it can't be opened there.
    bool Open(uint16_t port, const std::string &name);

    // After a step the recorder recorded: the step to those watching, and
    // the level and the view to those who came since
    void Step(int level, const std::string &levelDescription, const World::Recorder &recorder);

    // Between steps: takes in those who come and go, and keeps in touch
    void Service();

    // The level was left: tells everyone, and closes
    void Close();

    bool IsOpen() const { return _session.IsOpen(); }
    size_t Watchers() const { return _watchers.size(); }

    // What was sent so far, to all of them, unpacked and as sent
    uint64_t BytesUnpacked() const { return _unpacked; }
    uint64_t BytesSent() const { return _sent; }

protected:
    struct Watcher
    {
        uint32_t Id = 0;
        std::string Name;
        std::unique_ptr<z_stream_s> Packer;
        bool HasLevel = false;
        bool HasView = false;
        uint32_t ViewEpoch = 0; // Of the recorder's view they have
    };

    void Poll();
    void Send(Watcher &watcher, const std::string &msg);
    void Drop(uint32_t id);

    Session _session;
    std::vector<std::unique_ptr<Watcher>> _watchers;
    uint64_t _unpacked = 0, _sent = 0;
};

}

#endif
