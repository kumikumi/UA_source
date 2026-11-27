#include <algorithm>
#include <zlib.h>
#include <SDL3/SDL_timer.h>

#include "viewserver.h"
#include "../world/viewstream.h"
#include "../log.h"

namespace Net
{

// Watchers at most: each gets the whole stream
constexpr uint32_t MAX_WATCHERS = 16;

ViewServer::ViewServer() {}

ViewServer::~ViewServer()
{
    Close();
}

bool ViewServer::Open(uint16_t port, const std::string &name)
{
    if ( !_session.Host(name, port, name, MAX_WATCHERS, VIEW_VERSION, true) )
    {
        ypa_log_out("Watching: can't open port %d\n", (int)port);
        return false;
    }

    ypa_log_out("Watching: open on port %d\n", (int)port);
    return true;
}

void ViewServer::Drop(uint32_t id)
{
    for (auto it = _watchers.begin(); it != _watchers.end(); it++)
    {
        if ( (*it)->Id == id )
        {
            deflateEnd((*it)->Packer.get());
            _watchers.erase(it);
            return;
        }
    }
}

void ViewServer::Close()
{
    if ( !_session.IsOpen() )
        return;

    std::string end = World::ViewStream::End();

    for ( std::unique_ptr<Watcher> &watcher : _watchers )
    {
        if ( watcher->HasLevel )
            Send(*watcher, end);
    }

    _session.Flush();

    while ( !_watchers.empty() )
        Drop(_watchers.front()->Id);

    _session.Close();
    ypa_log_out("Watching: closed, %llu bytes sent (%llu unpacked)\n", (unsigned long long)_sent, (unsigned long long)_unpacked);
}

void ViewServer::Poll()
{
    Event event;

    while ( _session.Poll(&event) )
    {
        switch ( event.type )
        {
        case Event::PLAYER_JOINED:
        {
            std::unique_ptr<Watcher> watcher(new Watcher);
            watcher->Id = event.From;
            watcher->Name = event.Name;
            watcher->Packer.reset(new z_stream_s());

            if ( deflateInit(watcher->Packer.get(), 6) != Z_OK )
            {
                ypa_log_out("Watching: no packer for %s\n", event.Name.c_str());
                break;
            }

            ypa_log_out("Watching: %s joined\n", event.Name.c_str());
            _watchers.push_back(std::move(watcher));
            break;
        }

        case Event::PLAYER_LEFT:
        {
            ypa_log_out("Watching: %s left\n", event.Name.c_str());

            Request left;
            left.RequestType = Request::LEFT;
            left.Watcher = event.From;
            left.Name = event.Name;
            _requests.push_back(left);

            Drop(event.From);
            break;
        }

        case Event::DATA:
            Receive(event);
            break;

        default:
            break;
        }
    }
}

ViewServer::Watcher *ViewServer::Find(uint32_t id)
{
    for ( std::unique_ptr<Watcher> &watcher : _watchers )
    {
        if ( watcher->Id == id )
            return watcher.get();
    }

    return NULL;
}

const ViewServer::Watcher *ViewServer::Find(uint32_t id) const
{
    return const_cast<ViewServer *>(this)->Find(id);
}

void ViewServer::Receive(const Event &event)
{
    Watcher *watcher = Find(event.From);

    if ( !watcher || event.Data.empty() )
        return;

    Request request;
    request.Watcher = event.From;
    request.Name = event.Name;

    const uint8_t *data = event.Data.data();
    size_t size = event.Data.size();

    switch ( data[0] )
    {
    case WATCHER_JOIN:
        if ( size != 2 || data[1] > 7 )
            break;

        request.RequestType = Request::JOIN;
        request.Owner = data[1];
        _requests.push_back(request);
        return;

    case WATCHER_ORDER:
        if ( !request.Order.Read(data + 1, size - 1) )
            break;

        request.RequestType = Request::ORDER;
        _requests.push_back(request);
        return;

    case WATCHER_CONTROLS:
    {
        World::PlayerControls controls;

        if ( !controls.Read(data + 1, size - 1) )
            break;

        // The newest only, should older ones come after it
        if ( !watcher->ControlsTicks || controls.Number > watcher->Controls.Number )
        {
            watcher->Controls = controls;
            watcher->ControlsTicks = SDL_GetTicks();
        }
        return;
    }

    default:
        break;
    }

    ypa_log_out("Watching: %s sent what can't be read (%d bytes)\n", event.Name.c_str(), (int)size);
}

std::vector<ViewServer::Request> ViewServer::TakeRequests()
{
    std::vector<Request> taken;
    taken.swap(_requests);
    return taken;
}

void ViewServer::SetPlayer(uint32_t watcher, uint32_t player)
{
    if ( Watcher *w = Find(watcher) )
    {
        w->Player = player;
        w->PlayerToSend = true;
    }
}

bool ViewServer::Controls(uint32_t watcher, uint32_t maxAgeMs, World::PlayerControls &out) const
{
    const Watcher *w = Find(watcher);

    if ( !w || !w->ControlsTicks || SDL_GetTicks() - w->ControlsTicks > maxAgeMs )
        return false;

    out = w->Controls;
    return true;
}

void ViewServer::Service()
{
    if ( !_session.IsOpen() )
        return;

    Poll();
    _session.Flush();
}

// Packs a message on the watcher's stream and sends it whole
void ViewServer::Send(Watcher &watcher, const std::string &msg)
{
    z_stream_s *z = watcher.Packer.get();
    std::string packed;
    unsigned char buf[16384];

    z->next_in = (Bytef *)msg.data();
    z->avail_in = msg.size();

    do
    {
        z->next_out = buf;
        z->avail_out = sizeof(buf);
        deflate(z, Z_SYNC_FLUSH);
        packed.append((const char *)buf, sizeof(buf) - z->avail_out);
    }
    while ( z->avail_out == 0 );

    _session.Send(watcher.Id, packed.data(), packed.size(), RELIABLE);
    _unpacked += msg.size();
    _sent += packed.size();
}

void ViewServer::Step(int level, const std::string &levelDescription, const World::Recorder &recorder)
{
    if ( !_session.IsOpen() )
        return;

    Poll();

    std::string step, image;

    for ( std::unique_ptr<Watcher> &watcher : _watchers )
    {
        if ( !watcher->HasLevel )
        {
            Send(*watcher, World::ViewStream::Hello(level, levelDescription, recorder.ViewSchema()));
            watcher->HasLevel = true;
        }

        // Who they play as since this step
        if ( watcher->PlayerToSend )
        {
            Send(*watcher, World::ViewStream::You(watcher->Player));
            watcher->PlayerToSend = false;
        }

        // The view after this step, to read the next ones from: to those
        // who just came, and to everyone when it was taken anew
        if ( !watcher->HasView || watcher->ViewEpoch != recorder.ViewEpoch() )
        {
            if ( image.empty() )
                image = World::ViewStream::Image(recorder);

            Send(*watcher, image);
            watcher->HasView = true;
            watcher->ViewEpoch = recorder.ViewEpoch();
            continue;
        }

        if ( step.empty() )
            step = World::ViewStream::Step(recorder);

        Send(*watcher, step);
    }

    _session.Flush();
}

}
