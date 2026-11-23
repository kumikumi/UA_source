#include <string.h>
#include "udp.h"

namespace ZNDNet
{

IPaddress::IPaddress(NET_Address *addr, uint16_t port)
: _addr(addr ? NET_RefAddress(addr) : NULL)
, _port(port)
{}

IPaddress::~IPaddress()
{
    if (_addr)
        NET_UnrefAddress(_addr);
}

IPaddress &IPaddress::operator=(const IPaddress &b)
{
    // Reference the new address first, in case both are the same.
    NET_Address *old = _addr;
    _addr = b._addr ? NET_RefAddress(b._addr) : NULL;
    _port = b._port;

    if (old)
        NET_UnrefAddress(old);

    return *this;
}

bool IPaddress::operator==(const IPaddress &b) const
{
    if (_port != b._port)
        return false;

    if (!_addr || !b._addr)
        return _addr == b._addr;

    return NET_CompareAddresses(_addr, b._addr) == 0;
}


bool ResolveHost(IPaddress *address, const char *host, uint16_t port)
{
    NET_Address *addr = NET_ResolveHostname(host);
    if (!addr)
        return false;

    bool resolved = (NET_WaitUntilResolved(addr, -1) == NET_SUCCESS);
    if (resolved)
        *address = IPaddress(addr, port);

    NET_UnrefAddress(addr);
    return resolved;
}

UDPsocket UDP_Open(uint16_t port)
{
    return NET_CreateDatagramSocket(NULL, port, 0);
}

void UDP_Close(UDPsocket sock)
{
    if (sock)
        NET_DestroyDatagramSocket(sock);
}

bool UDP_Send(UDPsocket sock, const UDPpacket &pkt)
{
    return NET_SendDatagram(sock, pkt.address.GetAddress(), pkt.address.GetPort(), pkt.data, pkt.len);
}

int UDP_RecvV(UDPsocket sock, UDPpacket **packets)
{
    int count = 0;

    for (; packets[count]; count++)
    {
        NET_Datagram *dgram = NULL;

        if (!NET_ReceiveDatagram(sock, &dgram))
            return count ? count : -1;

        if (!dgram)
            break; // Nothing more has arrived

        UDPpacket *pkt = packets[count];
        pkt->len = SDL_min(dgram->buflen, pkt->maxlen);
        memcpy(pkt->data, dgram->buf, pkt->len);
        pkt->address = IPaddress(dgram->addr, dgram->port);

        NET_DestroyDatagram(dgram);
    }

    return count;
}

}
