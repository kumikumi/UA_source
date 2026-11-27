#ifndef NET_SESSION_H_INCLUDED
#define NET_SESSION_H_INCLUDED

#include <inttypes.h>
#include <deque>
#include <string>
#include <vector>

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetEvent ENetEvent;

namespace Net
{

// The port a host listens on
constexpr uint16_t DEFAULT_PORT = 61234;

// How a message travels
enum Delivery
{
    // Arrives once, in order with the sender's other reliable messages
    RELIABLE,

    // May be lost, and is dropped rather than delivered after a newer one
    // from the same sender
    LATEST,
};

struct Player
{
    uint32_t Id = 0;
    std::string Name;
};

struct Event
{
    enum Type
    {
        PLAYER_JOINED,
        PLAYER_LEFT,
        DATA,
        CLOSED, // The host ended the session, or can't be reached; everyone
                // else has left before
    };

    Type type = DATA;
    uint32_t From = 0; // The player the event is about, or who sent the data
    std::string Name;  // That player's name
    bool Broadcast = false; // The data went to everyone, not only to us
    std::vector<uint8_t> Data;
};

enum JoinResult
{
    JOIN_OK,
    JOIN_BAD_ADDRESS,
    JOIN_NO_ANSWER,
    JOIN_FULL,
    JOIN_CLOSED,        // The game has started or the host doesn't take players
    JOIN_OTHER_VERSION, // The host runs another version of the game
};

// A network game's players, connected through the one who hosts it: the
// others send everything to the host, which passes it on. Everything happens
// on the calling thread, in Poll and Flush.
class Session
{
public:
    Session();
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    // Opens a session others can join on the port. Players who run another
    // version can't join if checkVersion is set.
    bool Host(const std::string &myName, uint16_t port, const std::string &sessionName,
              uint32_t maxPlayers, const std::string &version, bool checkVersion);

    // Joins the session hosted at the address ("host" or "host:port"),
    // waiting up to timeoutMs for the host to take us in.
    JoinResult Join(const std::string &address, const std::string &myName,
                    const std::string &version, uint32_t timeoutMs);

    // Leaves the session; the host ends it for everyone
    void Close();

    bool IsOpen() const { return _open; }
    bool IsHost() const { return _hosting; }

    // Whether the host lets new players in
    void SetJoinable(bool joinable) { _joinable = joinable; }

    void Send(uint32_t to, const void *data, size_t size, Delivery delivery);
    void Broadcast(const void *data, size_t size, Delivery delivery);

    // Sends everything queued so far
    void Flush();

    // Takes the next event, if there is one. Sends and receives as well, so
    // it has to be called often.
    bool Poll(Event *event);

    // The host first, the others in the order they joined
    const std::vector<Player> &Players() const { return _players; }

    uint32_t MyId() const { return _myId; }
    const std::string &MyName() const { return _myName; } // As the host made it unique
    const std::string &SessionName() const { return _sessionName; }

    struct Stats
    {
        uint64_t BytesSent = 0;
        uint64_t BytesReceived = 0;
        uint32_t PacketsSent = 0;
        uint32_t PacketsReceived = 0;
        uint32_t RoundTrip = 0;  // Milliseconds; the worst of the host's players
        uint32_t PacketLoss = 0; // Percent, the same way
    };

    Stats GetStats() const;

protected:
    struct Remote;

    void Reset();
    void Service();
    void HostEvent(ENetEvent &event);
    void ClientEvent(ENetEvent &event);
    void Welcome(ENetPeer *peer, const std::string &wantedName);
    void Refuse(ENetPeer *peer, JoinResult reason);
    void SendTo(ENetPeer *peer, const std::vector<uint8_t> &msg, Delivery delivery);
    void SendToAll(const std::vector<uint8_t> &msg, Delivery delivery, ENetPeer *except);
    ENetPeer *FindPeer(uint32_t id) const;
    const Player *FindPlayer(uint32_t id) const;
    std::string UniqueName(std::string name) const;

    ENetHost *_host = nullptr;
    ENetPeer *_server = nullptr; // When joined
    bool _hosting = false;
    bool _open = false;
    bool _joinable = true;
    bool _checkVersion = true;
    uint32_t _maxPlayers = 0;
    uint32_t _nextId = 1;
    uint32_t _myId = 0;
    std::string _myName;
    std::string _sessionName;
    std::string _version;
    std::vector<Player> _players;
    std::deque<Event> _events;
};

}

#endif
