#include <algorithm>
#include <zlib.h>
#include <SDL3/SDL_timer.h>

#include "viewclient.h"
#include "viewserver.h"
#include "../world/consts.h"
#include "../log.h"

namespace Net
{

// The frames kept: the viewer shows the latest, a little behind
constexpr size_t FRAMES_KEPT = 4 * World::CVStepsPerSecond;

// The controls sent again with the next ones
constexpr size_t CONTROLS_RESENT = 4;

ViewClient::ViewClient() {}

ViewClient::~ViewClient()
{
    if ( _unpacker )
        inflateEnd(_unpacker.get());

    _session.Close();
}

bool ViewClient::Join(const std::string &address, const std::string &name, uint32_t timeoutMs)
{
    uint64_t deadline = SDL_GetTicks() + timeoutMs;

    _unpacker.reset(new z_stream_s());
    if ( inflateInit(_unpacker.get()) != Z_OK )
    {
        _unpacker.reset();
        return false;
    }

    JoinResult result = _session.Join(address, name, VIEW_VERSION, timeoutMs);

    if ( result != JOIN_OK )
    {
        ypa_log_out("Watching: can't join %s (%d)\n", address.c_str(), (int)result);
        return false;
    }

    // The level and the view, before anything can be shown
    while ( !_reader.HasView() && SDL_GetTicks() < deadline )
    {
        if ( !Update() )
            return false;

        SDL_Delay(5);
    }

    if ( !_reader.HasView() )
    {
        ypa_log_out("Watching: %s sent no view\n", address.c_str());
        return false;
    }

    return true;
}

bool ViewClient::Take(const std::vector<uint8_t> &packed)
{
    z_stream_s *z = _unpacker.get();
    std::string msg;
    unsigned char buf[65536];

    z->next_in = (Bytef *)packed.data();
    z->avail_in = packed.size();

    do
    {
        z->next_out = buf;
        z->avail_out = sizeof(buf);
        uInt before = z->avail_in;

        int r = inflate(z, Z_SYNC_FLUSH);
        size_t got = sizeof(buf) - z->avail_out;

        if ( (r != Z_OK && r != Z_BUF_ERROR) || (!got && z->avail_in == before && z->avail_in > 0) )
        {
            ypa_log_out("Watching: the stream doesn't unpack\n");
            return false;
        }

        msg.append((const char *)buf, got);
    }
    while ( z->avail_in > 0 || z->avail_out == 0 );

    _received += packed.size();
    _unpacked += msg.size();

    uint8_t type = _reader.Read((const uint8_t *)msg.data(), msg.size());

    switch ( type )
    {
    case 0:
        return false;

    case World::ViewStream::HELLO:
        _maker.reset(new World::FrameMaker(_reader.ViewSchema(), true));
        _frames.clear();
        break;

    case World::ViewStream::IMAGE:
        // The view anew: the frames start over from it
        if ( _frames.empty() )
            _joinStep = _reader.Step();

        _frames.clear();
        _jumpPending = true;
        _frames.emplace_back();
        _maker->FromView(_reader.View(), _reader.Step(), NULL, _frames.back());
        break;

    case World::ViewStream::STEP:
    {
        if ( !_reader.HasView() )
            break;

        Frame frame;
        _maker->FromView(_reader.View(), _reader.Step(), _frames.empty() ? NULL : &_frames.back(), frame);
        _frames.push_back(std::move(frame));
        _steps++;

        if ( _hashEvery && _reader.Step() % _hashEvery == 0 )
            _hashes.emplace_back(_reader.Step(), ViewHash());

        while ( _frames.size() > FRAMES_KEPT )
            _frames.pop_front();
        break;
    }

    case World::ViewStream::YOU:
        ypa_log_out("Watching: playing as player %u\n", _reader.Player());
        break;

    case World::ViewStream::UNIT:
        _unitFresh = true;
        break;

    case World::ViewStream::END:
        ypa_log_out("Watching: the game was left\n");
        _ended = true;
        break;
    }

    return true;
}

bool ViewClient::Update()
{
    if ( _ended )
        return false;

    Event event;

    while ( _session.Poll(&event) )
    {
        if ( event.type == Event::CLOSED )
        {
            ypa_log_out("Watching: lost the game\n");
            _ended = true;
            return false;
        }

        if ( event.type == Event::DATA && !Take(event.Data) )
        {
            _ended = true;
            return false;
        }
    }

    return !_ended;
}

void ViewClient::SendToGame(const std::string &msg)
{
    if ( _ended || _session.Players().empty() )
        return;

    _session.Send(_session.Players().front().Id, msg.data(), msg.size(), RELIABLE);
    _session.Flush();
}

void ViewClient::JoinFaction(uint8_t owner)
{
    std::string msg{(char)WATCHER_JOIN, (char)owner};
    SendToGame(msg);
}

void ViewClient::SendOrder(const World::PlayerOrder &order)
{
    SendToGame(std::string(1, (char)WATCHER_ORDER) + order.Write());
}

void ViewClient::SendControls(const World::PlayerControls &controls)
{
    if ( _ended || _session.Players().empty() )
        return;

    // With the few before, should those have been lost
    _controlsSent.push_back(controls);

    while ( _controlsSent.size() > CONTROLS_RESENT )
        _controlsSent.pop_front();

    std::string msg{(char)WATCHER_CONTROLS, (char)_controlsSent.size()};

    for ( const World::PlayerControls &c : _controlsSent )
        msg += c.Write();

    _session.Send(_session.Players().front().Id, msg.data(), msg.size(), LATEST);
    _session.Flush();
}

bool ViewClient::TakeControlledUnit(World::ControlledUnit &out)
{
    if ( !_unitFresh )
        return false;

    out = _reader.Unit();
    _unitFresh = false;
    return true;
}

uint32_t ViewClient::FirstStep() const
{
    return _frames.empty() ? 0 : _frames.front().Step;
}

uint32_t ViewClient::LastStep() const
{
    return _frames.empty() ? 0 : _frames.back().Step;
}

int32_t ViewClient::Duration() const
{
    return (LastStep() - std::min(LastStep(), _joinStep)) * World::CVStepTicks;
}

bool ViewClient::FrameAt(double step, World::Frame &out)
{
    if ( _frames.empty() )
        return false;

    step = std::max(step, (double)FirstStep());
    step = std::min(step, (double)LastStep());

    uint32_t whole = (uint32_t)step;
    uint32_t next = std::min(whole + 1, LastStep());

    const Frame &a = _frames[whole - FirstStep()];
    const Frame &b = _frames[next - FirstStep()];

    Frame::Between(a, b, step - whole, out);

    _jumped = _jumpPending;
    _jumpPending = false;
    return true;
}

uint64_t ViewClient::ViewHash() const
{
    uint64_t hash = 14695981039346656037ull;

    auto add = [&hash](const void *data, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            hash ^= ((const uint8_t *)data)[i];
            hash *= 1099511628211ull;
        }
    };

    for ( const auto &it : _reader.View() )
    {
        uint8_t kind = it.first >> 32;
        uint32_t id = it.first & 0xFFFFFFFF;
        add(&kind, 1);
        add(&id, 4);
        add(it.second.Data.data(), it.second.Data.size());
    }

    return hash;
}

}
