#ifndef WINDP_H_INCLUDED
#define WINDP_H_INCLUDED

#include <string>
#include "nucleas.h"
#include "network.h"
#include "lstvw.h"
#include "net/session.h"


struct windp_getNameMsg
{
    std::string name;
    uint8_t id;
};

struct windp_openSessionMsg
{
    const char *name;
    int maxplayers;
};

struct windp_createPlayerMsg
{
    uint32_t flags;
    const char *name;
    uint32_t data1;
    uint32_t data2;
};

struct TDPPlayerData
{
    enum NETPLAYERDATA
    {
        NPD_ITSME     =  (1 << 0),
    };
    
    uint32_t Index;
    std::string name;
    uint32_t flags;
    
    bool IsItMe() const { return (flags & NPD_ITSME) != 0; }
};

struct windp_recvMsg
{
    std::vector<uint8_t> _data;
    void *data;
    uint32_t size;
    uint32_t cast;
    uint32_t msgType;
    std::string senderName;
};

struct TRemoteStartInfo
{
    char callSIGN[64];
    bool isHoster;
    bool isClient;
};




class NC_STACK_ypaworld;
class UserData;

void yw_HandleNetMsg(NC_STACK_ypaworld *yw);


void yw_CheckCRCs(NC_STACK_ypaworld *yw);


int yw_DestroyPlayer(NC_STACK_ypaworld *yw, const char *playerName);



class netgamelst: public GuiBase
{
public:
    int btnWidth;
    int inputHeight;
    int maxWidth;
    int minInputHeight;
    char text[64];
    int textLen;
};

extern netgamelst netgame_wnd;

void CreateNetGameWindow(NC_STACK_ypaworld *yw);
void SetNetGameWindowLayout(NC_STACK_ypaworld *yw);
void NetGameWindowHandleInput(NC_STACK_ypaworld *yw, TInputState *inpt);




class NC_STACK_windp: public NC_STACK_network
{
public:
    virtual size_t Init(IDVList &stak);
    virtual size_t Deinit();

    virtual size_t SetMode(bool hosting);
    size_t GetMode();
    
    void SetWantedName(const std::string &name);
    std::string GetMyName();
    std::string GetCurrentSessionName();
    
    // Only for client
    bool Connect(const std::string &connString);
    bool IsConnected();
    bool HasLobby();
    bool IsJoined();
    
    
    // Only for host
    bool Host(const std::string &gameName, int playerNum);
    

    virtual size_t EnumSessions(IDVPair *stak);
    virtual size_t GetSessionName(windp_getNameMsg *arg);
    virtual size_t JoinSession(const std::string &stak);
    virtual size_t CreateSession(windp_openSessionMsg *os);
    virtual size_t GetSessionData(IDVPair *stak);
    virtual size_t CloseSession(IDVPair *stak);
    virtual size_t GetSessionStatus();
    virtual size_t SetSessionName(const char *sessName);
    

    virtual size_t CreatePlayer(windp_createPlayerMsg *pl);
    virtual size_t DeletePlayer(const char *playerName);

    virtual bool GetPlayerData(uint32_t index, TDPPlayerData *out) const;
    std::vector<TDPPlayerData> GetPlayersData() const;
    virtual uint32_t GetPlayerCount() const;
    int32_t GetMyIndex() const;
    int32_t GetPlayerIndex(const std::string &name) const;
    std::string GetPlayerName(uint32_t index) const;
    bool IsPlayer(const std::string &name) const;

    // Guaranteed messages arrive once and in order; the others may be lost
    // and are dropped rather than delivered after newer ones
    bool Send(void *data, size_t dataSize, const std::string &recvID, bool garantee);
    bool Broadcast(void *data, size_t dataSize, bool garantee);
    
    bool Recv(windp_recvMsg *recv);
    
    // Sends what is queued; returns the bytes sent since the last call
    size_t FlushBroadcastBuffer();

    virtual size_t GetCaps(IDVPair *stak);
    virtual size_t LockSession(int *);
    virtual size_t Reset(IDVPair *stak);
    

    virtual bool GetRemoteStart(TRemoteStartInfo *arg);

    virtual size_t GetLocalName(IDVPair *stak);
    virtual void   SetVersion(const std::string &arg);
    virtual size_t GetProvType();

    // Round trip (ms), packet loss (%), packets sent and received, KB sent
    // and received
    void GetStats(int *stats);

    NC_STACK_windp() {};
    virtual ~NC_STACK_windp() { Reset(NULL); };
    
    virtual const std::string ClassName() const {
        return __ClassName;
    };

    enum
    {
        SESSION_STATUS_NONE = 0,
        SESSION_STATUS_JOINED = 1,
        SESSION_STATUS_HOSTING = 2
    };
    
    enum
    {
        CONF_CONN_WAIT = 5000,
    };

protected:
    const std::vector<Net::Player> &Players() const;

    //Data
public:
    static constexpr const char * __ClassName = "windp.class";

protected:
    int connType = 0;
    Net::Session *_session = NULL;
    std::string _myName;
    std::string version_ident;
    bool version_check = false;
    uint64_t _flushedBytes = 0;
};

#endif // WINDP_H_INCLUDED
