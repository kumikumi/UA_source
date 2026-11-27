#ifndef NET_VIEWSERVER_H_INCLUDED
#define NET_VIEWSERVER_H_INCLUDED

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "session.h"
#include "../world/playerorder.h"
#include "../world/playercontrols.h"
#include "../world/viewstream.h"

struct z_stream_s;

namespace World { class Recorder; }

namespace Net
{

// What those who watch a game and the game that's watched send each other,
// to tell them from other sessions
constexpr const char *VIEW_VERSION = "ua-view-4";

// What a watcher sends the game, each message beginning with its type,
// unpacked; on the reliable channel:
//
//   'J'  join a faction: u8 its owner (1-7), or 0 to only watch again
//   'O'  an order, as World::PlayerOrder writes it
//
// and every step, on the latest-only channel:
//
//   'C'  their controls: u8 how many, then that many as
//        World::PlayerControls writes them, the newest last: the last few
//        they made, so that those lost on the way come with the next
enum WatcherMessage : uint8_t
{
    WATCHER_JOIN = 'J',
    WATCHER_ORDER = 'O',
    WATCHER_CONTROLS = 'C',
};

// The game's side of others watching it as it's played (--stream): whoever
// joins gets the level and the view, then every step (World::ViewStream),
// each watcher through a zlib stream of their own, flushed at each message,
// on the reliable channel. Watchers can play too: what they ask for comes
// as requests, which the game takes between steps.
class ViewServer
{
public:
    struct Request
    {
        enum Type
        {
            JOIN,  // Owner the faction, 0 for none
            ORDER, // Order
            LEFT,  // The watcher is gone
        };

        Type RequestType = JOIN;
        uint32_t Watcher = 0;
        std::string Name;
        uint8_t Owner = 0;
        World::PlayerOrder Order;
    };

    ViewServer();
    ~ViewServer();

    // Lets others watch on the port. False when it can't be opened there.
    bool Open(uint16_t port, const std::string &name);

    // The fields of units' whole state, which those who play need to read
    // the unit they control (SendUnit): before the first step
    void SetUnitSchema(const World::State::Schema &schema) { _unitSchema = schema; }

    // After a step the recorder recorded: the step to those watching, and
    // the level and the view to those who came since
    void Step(int level, const std::string &levelDescription, const World::Recorder &recorder);

    // After the step: to a watcher who plays, the unit they control
    void SendUnit(uint32_t watcher, const World::ControlledUnit &unit);

    // Between steps: takes in those who come and go, and keeps in touch
    void Service();

    // The level was left: tells everyone, and closes
    void Close();

    // What watchers asked for since the last time, in the order it came
    std::vector<Request> TakeRequests();

    // A watcher plays as a player from the next step on (0: only watches)
    void SetPlayer(uint32_t watcher, uint32_t player);

    // Before a step: a watcher's controls for it. They're taken one a step
    // in the order they were made, the last again until the next comes, so
    // that the game goes by the same controls step by step as the
    // watcher's machine did. False when none came lately.
    bool TakeControls(uint32_t watcher, World::PlayerControls &out);

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
        uint32_t Player = 0;    // Who they play as
        bool PlayerToSend = false;
        std::deque<World::PlayerControls> Controls; // Waiting to be taken
        World::PlayerControls Taken;                 // The last taken
        uint32_t ControlsNumber = 0; // The newest that came
        uint64_t ControlsTicks = 0;  // When (SDL_GetTicks); 0 for none yet
    };

    void Poll();
    void Receive(const Event &event);
    Watcher *Find(uint32_t id);
    void Send(Watcher &watcher, const std::string &msg);
    void Drop(uint32_t id);

    Session _session;
    std::vector<std::unique_ptr<Watcher>> _watchers;
    std::vector<Request> _requests;
    World::State::Schema _unitSchema;
    uint64_t _unpacked = 0, _sent = 0;
};

}

#endif
