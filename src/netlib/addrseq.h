#ifndef ZNDNET_ADDRSEQ_H_INCLUDED
#define ZNDNET_ADDRSEQ_H_INCLUDED


namespace ZNDNet
{

struct AddrSeq
{
    IPaddress addr;
    uint32_t seq;

    AddrSeq()
    {
        seq = 0;
    }

    AddrSeq(const IPaddress &_addr, uint32_t _seq)
    {
        set(_addr, _seq);
    }

    void set(const IPaddress &_addr, uint32_t _seq)
    {
        addr = _addr;
        seq = _seq;
    }

    inline bool operator== (const AddrSeq& b)
    {
        return addr == b.addr && seq == b.seq;
    };
};

}

#endif
