#ifndef NET_VIEWCLIENT_H_INCLUDED
#define NET_VIEWCLIENT_H_INCLUDED

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "session.h"
#include "../world/playback.h"
#include "../world/viewstream.h"
#include "../world/playerorder.h"

struct z_stream_s;

namespace Net
{

// Watching a game as it's played elsewhere (--watch): joined to the game's
// ViewServer, it reads its view stream into frames for the replay viewer,
// keeping only the latest.
class ViewClient : public World::FrameSource
{
public:
    ViewClient();
    ~ViewClient();

    // Joins the game watched at the address, as name, and waits up to
    // timeoutMs for its level and view. False, after logging why, when
    // that doesn't come.
    bool Join(const std::string &address, const std::string &name, uint32_t timeoutMs);

    int Level() const { return _reader.Level(); }
    const std::string &LevelDescription() const { return _reader.LevelDescription(); }

    // The frames kept, the last one the latest step that came
    uint32_t FirstStep() const override;
    uint32_t LastStep() const override;

    // From the step it joined at to the latest
    int32_t Duration() const override;

    bool FrameAt(double step, World::Frame &out) override;
    bool Jumped() const override { return _jumped; }

    bool Live() const override { return true; }
    bool Update() override;

    // The game was left, or can't be reached
    bool Ended() const { return _ended; }

    // Asks to play a faction (1-7) from now on, or only to watch again (0)
    void JoinFaction(uint8_t owner);

    // The player they play as, once the game says so; 0 for none
    uint32_t Player() const override { return _reader.Player(); }

    // An order as that player
    void SendOrder(const World::PlayerOrder &order) override;

    // Their controls this step, which go on the latest-only channel: the
    // game goes by the last that came
    void SendControls(const World::PlayerControls &controls) override;

    // What came so far, as it came and unpacked, and the steps
    uint64_t BytesReceived() const { return _received; }
    uint64_t BytesUnpacked() const { return _unpacked; }
    uint32_t StepsReceived() const { return _steps; }

    // A hash of the view after the latest step, to compare with the
    // game's: of every object's kind, id and bytes, in the order of their
    // keys (64-bit FNV-1a)
    uint64_t ViewHash() const;

    // Keeps the hash of the view after every step that's a multiple of
    // this many, from now on
    void KeepViewHashes(uint32_t every) { _hashEvery = every; }
    const std::vector<std::pair<uint32_t, uint64_t>> &ViewHashes() const { return _hashes; }

protected:
    typedef World::Frame Frame;

    // Unpacks and reads what came; false when it doesn't read
    bool Take(const std::vector<uint8_t> &packed);

    // To the game, which hosts the session
    void SendToGame(const std::string &msg);

    Session _session;
    std::unique_ptr<z_stream_s> _unpacker;
    World::ViewStreamReader _reader;
    std::unique_ptr<World::FrameMaker> _maker;
    std::deque<Frame> _frames;
    uint32_t _joinStep = 0;
    bool _jumped = false;
    bool _jumpPending = false; // A new image came: the next frame drawn jumps
    bool _ended = false;
    uint64_t _received = 0, _unpacked = 0;
    uint32_t _steps = 0;
    uint32_t _controlsSent = 0;
    uint32_t _hashEvery = 0;
    std::vector<std::pair<uint32_t, uint64_t>> _hashes;
};

}

#endif
