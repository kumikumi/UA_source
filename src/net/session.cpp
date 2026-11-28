#include <stdlib.h>
#include <algorithm>
#include <string.h>
#include <SDL3/SDL_timer.h>
#include "enet/enet.h"
#include "session.h"

namespace Net
{

namespace
{

// Sent with the connection, so other programs knocking on the port are
// turned away before they say anything
constexpr uint32_t CONNECT_MAGIC = 0x55415331; // "UAS1"

// Changes when the messages below change
constexpr uint16_t PROTOCOL = 1;

enum Channel : uint8_t
{
    CHANNEL_RELIABLE,
    CHANNEL_LATEST,
    CHANNEL_COUNT
};

enum Message : uint8_t
{
    MSG_HELLO = 1, // Joining player to host: protocol, version, wanted name
    MSG_WELCOME,   // Host to joining player: its id and name, the session's name, the players
    MSG_REFUSED,   // Host to joining player: why not
    MSG_JOINED,    // Host to players: id, name
    MSG_LEFT,      // Host to players: id
    MSG_DATA,      // From, to (0 for everyone), the data
};

constexpr size_t PLAYER_NAME_MAX = 20;
constexpr size_t DATA_HEADER = 9;

// Spare connections past the players, to tell those who knock that the
// game is full
constexpr uint32_t SPARE_PEERS = 4;

// A peer that stops answering is dropped after 10 to 30 s, depending on
// how often ENet has tried. A dropped player can't come back, so a
// machine that stops for a few seconds (loading, switching windows)
// shouldn't be.
constexpr uint32_t PEER_TIMEOUT_MIN = 10000;
constexpr uint32_t PEER_TIMEOUT_MAX = 30000;

// A peer that connected but hasn't said who it is by then is let go, so
// that one that never does doesn't keep a place others could join in
constexpr uint64_t HELLO_TIMEOUT = 10000;

// How long leaving waits for the others to hear of it
constexpr uint32_t CLOSE_WAIT = 300;

int enetUsers = 0;

void EnetUse()
{
    if ( enetUsers++ == 0 )
        enet_initialize();
}

void EnetRelease()
{
    if ( --enetUsers == 0 )
        enet_deinitialize();
}

class Writer
{
public:
    explicit Writer(Message type) { U8(type); }

    void U8(uint8_t v) { _buf.push_back(v); }
    void U16(uint16_t v) { U8(v & 0xFF); U8(v >> 8); }
    void U32(uint32_t v) { U16(v & 0xFFFF); U16(v >> 16); }
    void Str(const std::string &s)
    {
        U16(s.size());
        _buf.insert(_buf.end(), s.begin(), s.end());
    }
    void Bytes(const void *data, size_t size)
    {
        const uint8_t *p = (const uint8_t *)data;
        _buf.insert(_buf.end(), p, p + size);
    }

    const std::vector<uint8_t> &Buffer() const { return _buf; }

protected:
    std::vector<uint8_t> _buf;
};

class Reader
{
public:
    Reader(const uint8_t *data, size_t size) : _data(data), _size(size) {}

    bool Ok() const { return _ok; }
    size_t Left() const { return _size - _pos; }
    const uint8_t *Here() const { return _data + _pos; }

    uint8_t U8()
    {
        if ( Left() < 1 )
        {
            _ok = false;
            return 0;
        }
        return _data[_pos++];
    }
    uint16_t U16() { uint16_t lo = U8(); return lo | (U8() << 8); }
    uint32_t U32() { uint32_t lo = U16(); return lo | ((uint32_t)U16() << 16); }
    std::string Str()
    {
        size_t len = U16();
        if ( !_ok || Left() < len )
        {
            _ok = false;
            return std::string();
        }
        std::string s((const char *)Here(), len);
        _pos += len;
        return s;
    }

protected:
    const uint8_t *_data;
    size_t _size;
    size_t _pos = 0;
    bool _ok = true;
};

// Printable ASCII, not too long
std::string CleanName(std::string name)
{
    if ( name.size() > PLAYER_NAME_MAX )
        name.resize(PLAYER_NAME_MAX);

    for ( char &c : name )
    {
        if ( c < ' ' || c > '~' )
            c = ' ';
    }

    if ( name.empty() )
        name = "Player";

    return name;
}

bool SameName(const std::string &a, const std::string &b)
{
    return a.size() == b.size() && SDL_strncasecmp(a.c_str(), b.c_str(), a.size()) == 0;
}

}

// What the host knows about a connection
struct Session::Remote
{
    uint32_t Id = 0; // 0 until welcomed
    std::string Name;
    uint64_t Connected = 0; // When (SDL_GetTicks)
};

Session::Session()
{
    EnetUse();
}

Session::~Session()
{
    Close();
    EnetRelease();
}

void Session::Reset()
{
    if ( _host )
    {
        for ( size_t i = 0; i < _host->peerCount; i++ )
        {
            Remote *r = (Remote *)_host->peers[i].data;
            delete r;
            _host->peers[i].data = nullptr;
        }

        enet_host_destroy(_host);
    }

    _host = nullptr;
    _server = nullptr;
    _hosting = false;
    _open = false;
    _joinable = true;
    _relaying = true;
    _maxPlayers = 0;
    _nextId = 1;
    _myId = 0;
    _players.clear();
    _events.clear();
}

bool Session::Host(const std::string &myName, uint16_t port, const std::string &sessionName,
                   uint32_t maxPlayers, const std::string &version, bool checkVersion)
{
    Close();

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    _host = enet_host_create(&address, maxPlayers + SPARE_PEERS, CHANNEL_COUNT, 0, 0);
    if ( !_host )
        return false;

    _hosting = true;
    _open = true;
    _maxPlayers = maxPlayers;
    _sessionName = sessionName;
    _version = version;
    _checkVersion = checkVersion;
    _myId = _nextId++;
    _myName = CleanName(myName);
    _players.push_back({_myId, _myName});
    return true;
}

JoinResult Session::Join(const std::string &addressText, const std::string &myName,
                         const std::string &version, uint32_t timeoutMs)
{
    Close();

    std::string hostName = addressText;
    ENetAddress address;
    address.port = DEFAULT_PORT;

    // "name:port", but not an IPv6 address's colons
    size_t colon = hostName.rfind(':');
    if ( colon != std::string::npos && hostName.find(':') == colon )
    {
        address.port = atoi(hostName.c_str() + colon + 1);
        hostName.resize(colon);
    }

    if ( hostName.empty() || enet_address_set_host(&address, hostName.c_str()) != 0 )
        return JOIN_BAD_ADDRESS;

    _host = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);
    if ( !_host )
        return JOIN_NO_ANSWER;

    _server = enet_host_connect(_host, &address, CHANNEL_COUNT, CONNECT_MAGIC);
    if ( !_server )
    {
        Reset();
        return JOIN_NO_ANSWER;
    }

    enet_peer_timeout(_server, 0, PEER_TIMEOUT_MIN, PEER_TIMEOUT_MAX);

    _version = version;
    JoinResult result = JOIN_NO_ANSWER;
    uint64_t deadline = SDL_GetTicks() + timeoutMs;

    while ( !_open && SDL_GetTicks() < deadline )
    {
        ENetEvent event;
        if ( enet_host_service(_host, &event, 10) <= 0 )
            continue;

        switch ( event.type )
        {
        case ENET_EVENT_TYPE_CONNECT:
        {
            Writer hello(MSG_HELLO);
            hello.U16(PROTOCOL);
            hello.Str(version);
            hello.Str(myName);
            SendTo(_server, hello.Buffer(), RELIABLE);
            enet_host_flush(_host);
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE:
        {
            Reader rd(event.packet->data, event.packet->dataLength);
            uint8_t type = rd.U8();

            if ( type == MSG_WELCOME )
            {
                uint32_t id = rd.U32();
                std::string name = rd.Str();
                std::string sessionName = rd.Str();
                uint32_t count = rd.U8();
                std::vector<Player> players;

                for ( uint32_t i = 0; i < count && rd.Ok(); i++ )
                {
                    Player p;
                    p.Id = rd.U32();
                    p.Name = rd.Str();
                    players.push_back(p);
                }

                if ( rd.Ok() )
                {
                    _myId = id;
                    _myName = name;
                    _sessionName = sessionName;
                    _players = players;
                    _open = true;
                    result = JOIN_OK;
                }
            }
            else if ( type == MSG_REFUSED )
            {
                uint8_t why = rd.U8();
                if ( rd.Ok() )
                    result = (JoinResult)why;
            }

            enet_packet_destroy(event.packet);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT:
            Reset();
            return result;

        default:
            break;
        }
    }

    if ( !_open )
    {
        Reset();
        return result == JOIN_OK ? JOIN_NO_ANSWER : result;
    }

    return JOIN_OK;
}

void Session::Close()
{
    if ( _host )
    {
        bool waiting = false;

        for ( size_t i = 0; i < _host->peerCount; i++ )
        {
            ENetPeer *peer = &_host->peers[i];
            if ( peer->state == ENET_PEER_STATE_CONNECTED )
            {
                enet_peer_disconnect(peer, 0);
                waiting = true;
            }
        }

        // A moment for the others to hear it, so they don't wait for us to
        // time out
        uint64_t deadline = SDL_GetTicks() + CLOSE_WAIT;
        ENetEvent event;

        while ( waiting && SDL_GetTicks() < deadline && enet_host_service(_host, &event, 10) >= 0 )
        {
            if ( event.type == ENET_EVENT_TYPE_RECEIVE )
                enet_packet_destroy(event.packet);

            waiting = false;
            for ( size_t i = 0; i < _host->peerCount; i++ )
            {
                if ( _host->peers[i].state != ENET_PEER_STATE_DISCONNECTED &&
                     _host->peers[i].state != ENET_PEER_STATE_ZOMBIE )
                    waiting = true;
            }
        }
    }

    Reset();
}

void Session::SendTo(ENetPeer *peer, const std::vector<uint8_t> &msg, Delivery delivery)
{
    if ( !peer )
        return;

    uint32_t flags = delivery == RELIABLE ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
    ENetPacket *packet = enet_packet_create(msg.data(), msg.size(), flags);

    if ( enet_peer_send(peer, delivery == RELIABLE ? CHANNEL_RELIABLE : CHANNEL_LATEST, packet) < 0 )
        enet_packet_destroy(packet);
}

void Session::SendToAll(const std::vector<uint8_t> &msg, Delivery delivery, ENetPeer *except)
{
    if ( !_host )
        return;

    uint32_t flags = delivery == RELIABLE ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
    ENetPacket *packet = enet_packet_create(msg.data(), msg.size(), flags);

    for ( size_t i = 0; i < _host->peerCount; i++ )
    {
        ENetPeer *peer = &_host->peers[i];
        Remote *r = (Remote *)peer->data;

        if ( peer != except && peer->state == ENET_PEER_STATE_CONNECTED && r && r->Id )
            enet_peer_send(peer, delivery == RELIABLE ? CHANNEL_RELIABLE : CHANNEL_LATEST, packet);
    }

    // Peers keep a reference to what they send
    if ( packet->referenceCount == 0 )
        enet_packet_destroy(packet);
}

ENetPeer *Session::FindPeer(uint32_t id) const
{
    if ( !_hosting )
        return id == _myId ? nullptr : _server;

    for ( size_t i = 0; i < _host->peerCount; i++ )
    {
        Remote *r = (Remote *)_host->peers[i].data;
        if ( r && r->Id == id )
            return &_host->peers[i];
    }

    return nullptr;
}

const Player *Session::FindPlayer(uint32_t id) const
{
    for ( const Player &p : _players )
    {
        if ( p.Id == id )
            return &p;
    }
    return nullptr;
}

std::string Session::UniqueName(std::string name) const
{
    name = CleanName(name);
    std::string base = name;

    for ( int n = 2; ; n++ )
    {
        bool taken = false;
        for ( const Player &p : _players )
            taken |= SameName(p.Name, name);

        if ( !taken )
            return name;

        std::string suffix = "." + std::to_string(n);
        name = base.substr(0, PLAYER_NAME_MAX - suffix.size()) + suffix;
    }
}

void Session::Send(uint32_t to, const void *data, size_t size, Delivery delivery)
{
    if ( !_open || to == _myId )
        return;

    Writer msg(MSG_DATA);
    msg.U32(_myId);
    msg.U32(to);
    msg.Bytes(data, size);

    if ( _hosting )
        SendTo(FindPeer(to), msg.Buffer(), delivery);
    else
        SendTo(_server, msg.Buffer(), delivery);
}

void Session::Broadcast(const void *data, size_t size, Delivery delivery)
{
    if ( !_open )
        return;

    Writer msg(MSG_DATA);
    msg.U32(_myId);
    msg.U32(0);
    msg.Bytes(data, size);

    if ( _hosting )
        SendToAll(msg.Buffer(), delivery, nullptr);
    else
        SendTo(_server, msg.Buffer(), delivery);
}

void Session::Flush()
{
    if ( _host )
        enet_host_flush(_host);
}

void Session::Refuse(ENetPeer *peer, JoinResult reason)
{
    Writer msg(MSG_REFUSED);
    msg.U8(reason);
    SendTo(peer, msg.Buffer(), RELIABLE);
    enet_peer_disconnect_later(peer, 0);
}

void Session::Welcome(ENetPeer *peer, const std::string &wantedName)
{
    Remote *r = (Remote *)peer->data;
    r->Id = _nextId++;
    r->Name = UniqueName(wantedName);
    _players.push_back({r->Id, r->Name});

    Writer welcome(MSG_WELCOME);
    welcome.U32(r->Id);
    welcome.Str(r->Name);
    welcome.Str(_sessionName);
    welcome.U8(_players.size());
    for ( const Player &p : _players )
    {
        welcome.U32(p.Id);
        welcome.Str(p.Name);
    }
    SendTo(peer, welcome.Buffer(), RELIABLE);

    Writer joined(MSG_JOINED);
    joined.U32(r->Id);
    joined.Str(r->Name);
    SendToAll(joined.Buffer(), RELIABLE, peer);

    Event e;
    e.type = Event::PLAYER_JOINED;
    e.From = r->Id;
    e.Name = r->Name;
    _events.push_back(e);
}

void Session::HostEvent(ENetEvent &event)
{
    ENetPeer *peer = event.peer;

    switch ( event.type )
    {
    case ENET_EVENT_TYPE_CONNECT:
        if ( event.data != CONNECT_MAGIC )
        {
            enet_peer_disconnect_now(peer, 0);
            break;
        }

        peer->data = new Remote;
        ((Remote *)peer->data)->Connected = SDL_GetTicks();
        enet_peer_timeout(peer, 0, PEER_TIMEOUT_MIN, PEER_TIMEOUT_MAX);
        break;

    case ENET_EVENT_TYPE_RECEIVE:
    {
        Remote *r = (Remote *)peer->data;
        Reader rd(event.packet->data, event.packet->dataLength);
        uint8_t type = rd.U8();

        if ( r && !r->Id )
        {
            if ( type == MSG_HELLO )
            {
                uint16_t protocol = rd.U16();
                std::string version = rd.Str();
                std::string name = rd.Str();

                if ( !rd.Ok() || protocol != PROTOCOL || (_checkVersion && version != _version) )
                    Refuse(peer, JOIN_OTHER_VERSION);
                else if ( !_joinable )
                    Refuse(peer, JOIN_CLOSED);
                else if ( _players.size() >= _maxPlayers )
                    Refuse(peer, JOIN_FULL);
                else
                    Welcome(peer, name);
            }
        }
        else if ( r && type == MSG_DATA && rd.Left() >= DATA_HEADER - 1 )
        {
            rd.U32(); // The sender says who it is, but the host knows better
            uint32_t to = rd.U32();
            Delivery delivery = event.channelID == CHANNEL_RELIABLE ? RELIABLE : LATEST;

            if ( to == 0 || to == _myId )
            {
                Event e;
                e.type = Event::DATA;
                e.From = r->Id;
                e.Name = r->Name;
                e.Broadcast = (to == 0);
                e.Data.assign(rd.Here(), rd.Here() + rd.Left());
                _events.push_back(e);
            }

            if ( to != _myId && _relaying )
            {
                Writer msg(MSG_DATA);
                msg.U32(r->Id);
                msg.U32(to);
                msg.Bytes(rd.Here(), rd.Left());

                if ( to == 0 )
                    SendToAll(msg.Buffer(), delivery, peer);
                else
                    SendTo(FindPeer(to), msg.Buffer(), delivery);
            }
        }

        enet_packet_destroy(event.packet);
        break;
    }

    case ENET_EVENT_TYPE_DISCONNECT:
    {
        Remote *r = (Remote *)peer->data;
        peer->data = nullptr;

        if ( r && r->Id )
        {
            for ( auto it = _players.begin(); it != _players.end(); it++ )
            {
                if ( it->Id == r->Id )
                {
                    _players.erase(it);
                    break;
                }
            }

            Writer left(MSG_LEFT);
            left.U32(r->Id);
            SendToAll(left.Buffer(), RELIABLE, peer);

            Event e;
            e.type = Event::PLAYER_LEFT;
            e.From = r->Id;
            e.Name = r->Name;
            _events.push_back(e);
        }

        delete r;
        break;
    }

    default:
        break;
    }
}

void Session::ClientEvent(ENetEvent &event)
{
    switch ( event.type )
    {
    case ENET_EVENT_TYPE_RECEIVE:
    {
        Reader rd(event.packet->data, event.packet->dataLength);
        uint8_t type = rd.U8();

        if ( type == MSG_JOINED )
        {
            Player p;
            p.Id = rd.U32();
            p.Name = rd.Str();

            if ( rd.Ok() && !FindPlayer(p.Id) )
            {
                _players.push_back(p);

                Event e;
                e.type = Event::PLAYER_JOINED;
                e.From = p.Id;
                e.Name = p.Name;
                _events.push_back(e);
            }
        }
        else if ( type == MSG_LEFT )
        {
            uint32_t id = rd.U32();

            for ( auto it = _players.begin(); rd.Ok() && it != _players.end(); it++ )
            {
                if ( it->Id == id )
                {
                    Event e;
                    e.type = Event::PLAYER_LEFT;
                    e.From = id;
                    e.Name = it->Name;
                    _events.push_back(e);

                    _players.erase(it);
                    break;
                }
            }
        }
        else if ( type == MSG_DATA && rd.Left() >= DATA_HEADER - 1 )
        {
            uint32_t from = rd.U32();
            uint32_t to = rd.U32();
            const Player *sender = FindPlayer(from);

            // Data may overtake the news of its sender joining
            if ( sender && (to == 0 || to == _myId) )
            {
                Event e;
                e.type = Event::DATA;
                e.From = from;
                e.Name = sender->Name;
                e.Broadcast = (to == 0);
                e.Data.assign(rd.Here(), rd.Here() + rd.Left());
                _events.push_back(e);
            }
        }

        enet_packet_destroy(event.packet);
        break;
    }

    case ENET_EVENT_TYPE_DISCONNECT:
    {
        _server = nullptr;
        _open = false;

        // Everyone else was reached through the host
        for ( const Player &p : _players )
        {
            if ( p.Id == _myId )
                continue;

            Event e;
            e.type = Event::PLAYER_LEFT;
            e.From = p.Id;
            e.Name = p.Name;
            _events.push_back(e);
        }

        _players.erase(std::remove_if(_players.begin(), _players.end(),
                                      [this](const Player &p) { return p.Id != _myId; }),
                       _players.end());

        Event e;
        e.type = Event::CLOSED;
        _events.push_back(e);
        break;
    }

    default:
        break;
    }
}

void Session::Service()
{
    if ( !_host )
        return;

    ENetEvent event;
    while ( _host && enet_host_service(_host, &event, 0) > 0 )
    {
        if ( _hosting )
            HostEvent(event);
        else
            ClientEvent(event);
    }

    // Those who connected and haven't said who they are in time
    for ( size_t i = 0; _hosting && _host && i < _host->peerCount; i++ )
    {
        ENetPeer *peer = &_host->peers[i];
        Remote *r = (Remote *)peer->data;

        if ( r && !r->Id && SDL_GetTicks() - r->Connected > HELLO_TIMEOUT )
        {
            delete r;
            peer->data = nullptr;
            enet_peer_disconnect_now(peer, 0);
        }
    }
}

bool Session::Poll(Event *event)
{
    Service();

    if ( _events.empty() )
        return false;

    *event = std::move(_events.front());
    _events.pop_front();
    return true;
}

Session::Stats Session::GetStats() const
{
    Stats s;

    if ( !_host )
        return s;

    s.BytesSent = _host->totalSentData;
    s.BytesReceived = _host->totalReceivedData;
    s.PacketsSent = _host->totalSentPackets;
    s.PacketsReceived = _host->totalReceivedPackets;

    for ( size_t i = 0; i < _host->peerCount; i++ )
    {
        const ENetPeer &peer = _host->peers[i];
        if ( peer.state != ENET_PEER_STATE_CONNECTED )
            continue;

        if ( peer.roundTripTime > s.RoundTrip )
            s.RoundTrip = peer.roundTripTime;

        uint32_t loss = (uint64_t)peer.packetLoss * 100 / ENET_PEER_PACKET_LOSS_SCALE;
        if ( loss > s.PacketLoss )
            s.PacketLoss = loss;
    }

    return s;
}

}
