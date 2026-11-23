#ifndef ZNDNET_UDP_H_INCLUDED
#define ZNDNET_UDP_H_INCLUDED

#include <inttypes.h>
#include <SDL3_net/SDL_net.h>

namespace ZNDNet
{

// Where a packet comes from or goes to. SDL3_net's addresses are shared,
// reference counted objects; this holds a reference of its own, so the net
// code can keep, copy and compare addresses as plain values.
class IPaddress
{
public:
    IPaddress() {};
    IPaddress(NET_Address *addr, uint16_t port);
    IPaddress(const IPaddress &b) : IPaddress(b._addr, b._port) {};
    ~IPaddress();

    IPaddress &operator=(const IPaddress &b);
    bool operator==(const IPaddress &b) const;

    NET_Address *GetAddress() const { return _addr; };
    uint16_t GetPort() const { return _port; }; // In host byte order

protected:
    NET_Address *_addr = NULL;
    uint16_t _port = 0;
};

struct UDPpacket
{
    IPaddress address;
    uint8_t *data = NULL;
    int len = 0;
    int maxlen = 0;
};

typedef NET_DatagramSocket *UDPsocket;

// Looks up a host name or address, waiting for the answer.
bool ResolveHost(IPaddress *address, const char *host, uint16_t port);

// On all local addresses; port 0 takes any free port.
UDPsocket UDP_Open(uint16_t port);
void UDP_Close(UDPsocket sock);

// True if the packet went out.
bool UDP_Send(UDPsocket sock, const UDPpacket &pkt);

// Fills the packets of a NULL-terminated array with the datagrams that have
// arrived, without waiting. Datagrams longer than a packet's maxlen are cut.
// Returns the number of packets filled, or -1 on error.
int UDP_RecvV(UDPsocket sock, UDPpacket **packets);

}

#endif
