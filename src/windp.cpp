#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "includes.h"
#include "system/inpt.h"
#include "windp.h"
#include "button.h"
#include "yw_net.h"
#include "system/inivals.h"
#include "locale/locale.h"

size_t NC_STACK_windp::Init(IDVList &stak)
{
    if ( !NC_STACK_network::Init(stak))
        return 0;

    System::IniConf::ReadFromNucleusIni();
    version_check = System::IniConf::NetVersionCheck.Get<bool>();
    return 1;
}

size_t NC_STACK_windp::Deinit()
{
    Reset(NULL);
    return NC_STACK_nucleus::Deinit();
}


size_t NC_STACK_windp::SetMode(bool hosting)
{
    Reset(NULL);
    connType = hosting ? 1 : 2;
    _session = new Net::Session;
    return 1;
}

size_t NC_STACK_windp::GetMode()
{
    return connType;
}

void NC_STACK_windp::SetWantedName(const std::string &name)
{
    _myName = name;
}

std::string NC_STACK_windp::GetMyName()
{
    return _myName;
}

bool NC_STACK_windp::Connect(const std::string &connString)
{
    if (connType != 2)
        return false;

    Net::JoinResult result = _session->Join(connString, _myName, version_ident, CONF_CONN_WAIT);

    if ( result != Net::JOIN_OK )
    {
        log_netlog("Couldn't join the game at %s: %s\n", connString.c_str(),
                   result == Net::JOIN_BAD_ADDRESS ? "no such address" :
                   result == Net::JOIN_FULL ? "it is full" :
                   result == Net::JOIN_CLOSED ? "it doesn't take players now" :
                   result == Net::JOIN_OTHER_VERSION ? "it runs another version" : "no answer");
        return false;
    }

    _myName = _session->MyName();
    return true;
}

bool NC_STACK_windp::IsConnected()
{
    return connType == 2 && _session && _session->IsOpen();
}

bool NC_STACK_windp::HasLobby()
{
    return false;
}

bool NC_STACK_windp::IsJoined()
{
    return IsConnected();
}


bool NC_STACK_windp::Host(const std::string &gameName, int playerNum)
{
    if (connType != 1)
        return false;

    if ( !_session->Host(_myName, Net::DEFAULT_PORT, gameName, playerNum, version_ident, version_check) )
    {
        log_netlog("Couldn't host a game on port %d\n", Net::DEFAULT_PORT);
        return false;
    }

    _myName = _session->MyName();
    return true;
}



size_t NC_STACK_windp::EnumSessions(IDVPair *stak)
{
    return 0;
}

size_t NC_STACK_windp::GetSessionName(windp_getNameMsg *sessName)
{
    return 0;
}

std::string NC_STACK_windp::GetCurrentSessionName()
{
    return _session ? _session->SessionName() : std::string();
}

size_t NC_STACK_windp::JoinSession(const std::string &sessName)
{
    return 0;
}

size_t NC_STACK_windp::CreateSession(windp_openSessionMsg *osm)
{
    return 0;
}

size_t NC_STACK_windp::GetSessionData(IDVPair *stak)
{
    return 0;
}

size_t NC_STACK_windp::CloseSession(IDVPair *stak)
{
    // Ends the game for everyone
    if ( _session && _session->IsHost() )
        _session->Close();

    return 0;
}

size_t NC_STACK_windp::GetSessionStatus()
{
    return SESSION_STATUS_NONE;
}

size_t NC_STACK_windp::SetSessionName(const char *sessName)
{
    return 0;
}

size_t NC_STACK_windp::CreatePlayer(windp_createPlayerMsg *cp)
{
    return 0;
}

size_t NC_STACK_windp::DeletePlayer(const char *playerName)
{
    return 0;
}


const std::vector<Net::Player> &NC_STACK_windp::Players() const
{
    static const std::vector<Net::Player> none;
    return _session ? _session->Players() : none;
}

bool NC_STACK_windp::GetPlayerData(uint32_t index, TDPPlayerData *out) const
{
    const std::vector<Net::Player> &players = Players();

    if (index >= players.size())
        return false;

    out->name = players[index].Name;
    out->flags = players[index].Id == _session->MyId() ? TDPPlayerData::NPD_ITSME : 0;
    out->Index = index;
    return true;
}

std::vector<TDPPlayerData> NC_STACK_windp::GetPlayersData() const
{
    std::vector<TDPPlayerData> tmp( Players().size() );

    for(uint32_t i = 0; i < tmp.size(); i++)
        GetPlayerData(i, &tmp[i]);

    return tmp;
}

int32_t NC_STACK_windp::GetMyIndex() const
{
    const std::vector<Net::Player> &players = Players();

    for(size_t i = 0; i < players.size(); i++)
    {
        if (players[i].Id == _session->MyId())
            return i;
    }

    return -1;
}

std::string NC_STACK_windp::GetPlayerName(uint32_t index) const
{
    const std::vector<Net::Player> &players = Players();

    if (index >= players.size())
        return std::string();

    return players[index].Name;
}

int32_t NC_STACK_windp::GetPlayerIndex(const std::string &name) const
{
    const std::vector<Net::Player> &players = Players();

    for(size_t i = 0; i < players.size(); i++)
    {
        if ( !StriCmp(players[i].Name, name) )
            return i;
    }

    return -1;
}

bool NC_STACK_windp::IsPlayer(const std::string &name) const
{
    return GetPlayerIndex(name) != -1;
}

uint32_t NC_STACK_windp::GetPlayerCount() const
{
    return Players().size();
}


bool NC_STACK_windp::Broadcast(void *data, size_t dataSize, bool garantee)
{
    if ( !_session )
        return false;

    _session->Broadcast(data, dataSize, garantee ? Net::RELIABLE : Net::LATEST);
    return true;
}

bool NC_STACK_windp::Send(void *data, size_t dataSize, const std::string &recvID, bool garantee)
{
    int32_t index = GetPlayerIndex(recvID);

    if ( index == -1 )
        return false;

    _session->Send(Players()[index].Id, data, dataSize, garantee ? Net::RELIABLE : Net::LATEST);
    return true;
}

bool NC_STACK_windp::Recv(windp_recvMsg *recv)
{
    Net::Event evt;

    if ( !_session || !_session->Poll(&evt) )
        return false;

    recv->msgType = RECVMSG_NONE;

    switch( evt.type )
    {
        case Net::Event::PLAYER_JOINED:
            recv->msgType = RECVMSG_CREATEPLAYER;
            recv->senderName = evt.Name;
            break;

        case Net::Event::PLAYER_LEFT:
            recv->msgType = RECVMSG_DESTROYPLAYER;
            recv->senderName = evt.Name;
            break;

        case Net::Event::DATA:
            recv->msgType = RECVMSG_NORMAL;
            recv->_data = std::move(evt.Data);
            recv->data = recv->_data.data();
            recv->size = recv->_data.size();
            recv->senderName = evt.Name;
            recv->cast = evt.Broadcast ? 1 : 0;
            break;

        case Net::Event::CLOSED:
            log_netlog("The connection to the host is gone\n");
            return false;
    }

    return true;
}

size_t NC_STACK_windp::FlushBroadcastBuffer()
{
    if ( !_session )
        return 0;

    _session->Flush();

    // What went out since the last time, for the statistics
    uint64_t sent = _session->GetStats().BytesSent;
    size_t delta = sent >= _flushedBytes ? sent - _flushedBytes : 0;
    _flushedBytes = sent;
    return delta;
}

size_t NC_STACK_windp::GetCaps(IDVPair *stak)
{
    return 0;
}

size_t NC_STACK_windp::LockSession(int *lock)
{
    // Locked, the host turns away new players
    if ( _session && _session->IsHost() )
        _session->SetJoinable(*lock == 0);

    return 1;
}

size_t NC_STACK_windp::Reset(IDVPair *stak)
{
    delete _session;
    _session = NULL;
    _flushedBytes = 0;
    return 0;
}

bool NC_STACK_windp::GetRemoteStart(TRemoteStartInfo *arg)
{
    //check remote start
    arg->callSIGN[0] = '\0';
    arg->isClient = false;
    arg->isHoster = false;

    if ( !_session || !_session->IsOpen() )
        return true;

    if ( _session->IsHost() )
        arg->isHoster = true;
    else
        arg->isClient = true;

    return true;
}

size_t NC_STACK_windp::GetLocalName(IDVPair *stak)
{
    return 0;
}

void NC_STACK_windp::SetVersion(const std::string &ver)
{
    version_ident = ver;
}

size_t NC_STACK_windp::GetProvType()
{
    return connType;
}

void NC_STACK_windp::GetStats(int *stats)
{
    Net::Session::Stats s;

    if ( _session )
        s = _session->GetStats();

    stats[0] = s.RoundTrip;
    stats[1] = s.PacketLoss;
    stats[2] = s.PacketsSent;
    stats[3] = s.PacketsReceived;
    stats[4] = s.BytesSent / 1024;
    stats[5] = s.BytesReceived / 1024;
}
















void UserData::yw_FractionInit()
{
    const TMapRegionInfo &lvl = p_YW->_globalMapRegions.MapRegions[ netLevelID ];

    FreeFraction = lvl.FractionsBits & ( World::OWNER_RESIST_BIT |   World::OWNER_GHOR_BIT
                                     |   World::OWNER_MYKO_BIT   |   World::OWNER_TAER_BIT );

    uint32_t frkt = 0;
    if ( FreeFraction & World::OWNER_RESIST_BIT )
    {
        frkt = World::OWNER_RESIST_BIT;
    }
    else if ( FreeFraction & World::OWNER_GHOR_BIT )
    {
        frkt =  World::OWNER_GHOR_BIT;
    }
    else if ( FreeFraction & World::OWNER_MYKO_BIT )
    {
        frkt = World::OWNER_MYKO_BIT;
    }
    else if ( FreeFraction & World::OWNER_TAER_BIT )
    {
        frkt = World::OWNER_TAER_BIT;
    }

    SelectedFraction = frkt;
    FreeFraction &= ~frkt;
}

void UserData::AfterMapChoose()
{
    if ( netSel < 0 )
        return;
    
    switch ( p_YW->_netDriver->GetMode() )
    {
        case 1:
        {
            std::string bff = fmt::sprintf("%d%s%s%s%s", netLevelID, "|", p_YW->_netDriver->GetMyName(), "|", p_YW->_buildDate);

            if ( !p_YW->_netDriver->Host(bff, p_YW->_globalMapRegions.MapRegions[ netLevelID ].RoboCount) )
            {
                ShowConfirmDialog(4, Locale::Text::Advanced(Locale::ADV_CANTHOST), "", 1);
                return;
            }
            
            p_YW->_isNetGame = true;
            netSel = -1;
            netSelMode = NETSCREEN_INSESSION;
            netName = "";
            netNameCurPos = 0;
            network_listvw.firstShownEntries = 0;
            network_listvw.selectedEntry = 0;

            lobbyPlayers[ 0 ].Name = p_YW->_netDriver->GetMyName();

            yw_FractionInit();

            rdyStart = true;
            lobbyPlayers[0].Ready = true;
        }
        break;
            
        case 2:
            
            break;
        
        default:
            break;
    }
    return;

    if ( p_YW->_netDriver->GetSessionStatus() )
    {
        TMapRegionInfo &lvl = p_YW->_globalMapRegions.MapRegions[ netLevelID ];

        int numpl = p_YW->_netDriver->GetPlayerCount();

        netSelMode = NETSCREEN_INSESSION;
        netName = "";
        netNameCurPos = 0;
        network_listvw.firstShownEntries = 0;
        network_listvw.selectedEntry = 0;
        netSel = -1;

        bool selected = false;
        int plid = 0;

        if ( lvl.IsFraction(World::OWNER_RESIST) )
        {
            SelectedFraction = World::OWNER_RESIST_BIT;
            FreeFraction &= ~World::OWNER_RESIST_BIT;
            lobbyPlayers[ plid ].NetFraction = World::OWNER_RESIST_BIT;
            numpl--;
            plid++;
            selected = true;
        }
        else
        {
            FreeFraction &= ~World::OWNER_RESIST_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_GHOR) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_GHOR_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_GHOR_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_GHOR_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_GHOR_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_MYKO) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_MYKO_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_MYKO_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_MYKO_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_MYKO_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_TAER) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_TAER_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_TAER_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_TAER_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_TAER_BIT;
        }

        for(TNetPlayerLobbyData &pl : lobbyPlayers)
            pl.DataChecksum = 0;

        uamessage_setLevel msg_slvl;
        msg_slvl.lvlID = netLevelID;
        msg_slvl.msgID = UAMSG_SETLEVEL;
        msg_slvl.owner = 0;

        p_YW->NetBroadcastMessage(&msg_slvl, sizeof(msg_slvl), true);

        p_YW->SendCRC(netLevelID);

        p_YW->_netDriver->FlushBroadcastBuffer();

        std::string bff = fmt::sprintf("%d%s%s%s%s", netLevelID, "|", netPlayerName, "|", p_YW->_buildDate);

        p_YW->_netDriver->SetSessionName(bff.c_str());
    }
    else if ( remoteMode )
    {
        TMapRegionInfo &lvl = p_YW->_globalMapRegions.MapRegions[ netLevelID ];

        int numpl = p_YW->_netDriver->GetPlayerCount();

        netSelMode = NETSCREEN_INSESSION;
        netName = "";
        netNameCurPos = 0;
        network_listvw.firstShownEntries = 0;
        network_listvw.selectedEntry = 0;
        netSel = -1;

        bool selected = false;
        int plid = 0;

        if ( lvl.IsFraction(World::OWNER_RESIST) )
        {
            SelectedFraction = World::OWNER_RESIST_BIT;
            FreeFraction &= ~World::OWNER_RESIST_BIT;
            lobbyPlayers[ plid ].NetFraction = World::OWNER_RESIST_BIT;
            numpl--;
            plid++;
            selected = true;
        }
        else
        {
            FreeFraction &= ~World::OWNER_RESIST_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_GHOR) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_GHOR_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_GHOR_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_GHOR_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_GHOR_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_MYKO) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_MYKO_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_MYKO_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_MYKO_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_MYKO_BIT;
        }

        if ( lvl.IsFraction(World::OWNER_TAER) )
        {
            if (numpl > 0)
            {
                numpl--;
                FreeFraction &= ~World::OWNER_TAER_BIT;
                lobbyPlayers[ plid ].NetFraction = World::OWNER_TAER_BIT;
                plid++;
                if (!selected)
                {
                    selected = true;
                    SelectedFraction = World::OWNER_TAER_BIT;
                }
            }
        }
        else
        {
            FreeFraction &= ~World::OWNER_TAER_BIT;
        }

        uamessage_lobbyInit limsg;
        limsg.lvlID = netLevelID;
        limsg.msgID = UAMSG_LOBBYINIT;
        limsg.owner = 0;

        p_YW->NetBroadcastMessage(&limsg, sizeof(limsg), true);

        p_YW->_netDriver->FlushBroadcastBuffer();
    }
    else
    {
        if (isHost)
        {
            std::string bff = fmt::sprintf("%d%s%s%s%s", netLevelID, "|", netPlayerName, "|", p_YW->_buildDate);

            if (p_YW->_netDriver->GetProvType() == 4) //MODEM!!!!
                GFX::Engine.BeginSystemDialog(NULL);

            windp_openSessionMsg os;
            os.name = bff.c_str();
            os.maxplayers = 4;

            size_t res = p_YW->_netDriver->CreateSession(&os);

            if (p_YW->_netDriver->GetProvType() == 4) //MODEM!!!!
                GFX::Engine.EndSystemDialog(NULL);

            if ( !res )
                return;
        }

        windp_createPlayerMsg cp;
        cp.name = netPlayerName.c_str();
        cp.flags = 1;

        if (p_YW->_netDriver->CreatePlayer(&cp))
        {
            p_YW->_isNetGame = true;
            netSel = -1;
            netSelMode = NETSCREEN_INSESSION;
            netName = "";
            netNameCurPos = 0;
            network_listvw.firstShownEntries = 0;
            network_listvw.selectedEntry = 0;

            int plid = p_YW->_netDriver->GetPlayerCount();

            lobbyPlayers[ plid - 1 ].Name = cp.name;

            yw_FractionInit();

            rdyStart = true;
            lobbyPlayers[ plid - 1].Ready = true;
        }
    }
}

void UserData::yw_NetOKProvider()
{
    if ( netSel == 0 || netSel == 1 )
    {
        GFX::Engine.BeginSystemDialog(NULL);

        if ( p_YW->_netDriver->SetMode(netSel == 0) )
        {
            netSelMode = NETSCREEN_ENTER_NAME;
            netSel = -1;
            network_listvw.firstShownEntries = 0;
            network_listvw.selectedEntry = 0;

            p_YW->GuiWinClose( &network_listvw );

            netName = netPlayerName;

            netNameCurPos = netName.size();
        }

        GFX::Engine.EndSystemDialog(NULL);

        update_time_norm = 200;
        flush_time_norm = 200;
    }
}

void UserData::yw_JoinNetGame()
{
    if ( p_YW->_netDriver->GetProvType() != 4 || modemAskSession )
    {
        windp_getNameMsg gName;
        gName.id = netSel;

        if ( p_YW->_netDriver->GetSessionName(&gName) )
        {
            if ( p_YW->_netDriver->GetProvType() == 4 )
                GFX::Engine.BeginSystemDialog(NULL);

            if ( p_YW->_netDriver->JoinSession(gName.name) )
            {
                if ( p_YW->_netDriver->GetProvType() == 4 )
                    GFX::Engine.EndSystemDialog(NULL);

                netName = "";
                netNameCurPos = 0;
                netLevelID = 0;

                std::string buf = gName.name;

                for (size_t i = 0; i < buf.length(); i++)
                {
                    if (buf[i] == '"')
                    {
                        buf.resize(i);

                        netLevelID = atoi(buf.c_str());
                        break;
                    }
                }
                network_listvw.firstShownEntries = 0;
                netLevelName = p_YW->GetLevelName(netLevelID);

                for(TDPPlayerData &p : p_YW->_netDriver->GetPlayersData())
                {
                    lobbyPlayers[p.Index].Name = p.name;
                    
                    if (p.IsItMe())
                    {
                        lobbyPlayers[p.Index].Name += "X";
                    }
                }

                windp_createPlayerMsg crPlayerMsg;
                crPlayerMsg.name = netPlayerName.c_str();
                crPlayerMsg.flags = 1;

                if ( p_YW->_netDriver->CreatePlayer(&crPlayerMsg) )
                {
                    p_YW->_isNetGame = true;
                    netSel = -1;
                    netSelMode = NETSCREEN_INSESSION;
                    netName = "";
                    netNameCurPos = 0;
                    network_listvw.firstShownEntries = 0;
                    network_listvw.selectedEntry = 0;
                    isHost = false;

                    yw_FractionInit();
                }

                rdyStart = false;
                int32_t myIndex = p_YW->_netDriver->GetMyIndex();
                if (myIndex != -1)
                    lobbyPlayers[myIndex].Ready = false;

                p_YW->SendCRC(netLevelID);
            }
            else
            {
                if ( p_YW->_netDriver->GetProvType() == 4 )
                    GFX::Engine.BeginSystemDialog(NULL);

                //sb_0x46bb54__sub0(p_ypaworld, get_lang_string(p_ypaworld->string_pointers_p2, 2400, "YPA ERROR MESSAGE"), get_lang_string(p_ypaworld->string_pointers_p2, 2401, "SESSION NOT LONGER AVAILABLE"));
                fmt::printf("%s: %s\n", Locale::Text::Advanced(Locale::ADV_ERRHEAD), Locale::Text::Advanced(Locale::ADV_ERRNOSESS));

                if ( p_YW->_netDriver->GetProvType() == 4 )
                {
                    netSel = -1;
                    ExitFromNetworkToMain();
                }
                else
                {
                    p_YW->_netDriver->EnumSessions(NULL);
                }
            }
        }
    }
    else
    {
        if ( p_YW->_netDriver->GetProvType() == 4 )
            GFX::Engine.BeginSystemDialog(NULL);

        if ( p_YW->_netDriver->EnumSessions(NULL) )
        {
            modemAskSession = true;
        }

        if ( p_YW->_netDriver->GetProvType() == 4 )
            GFX::Engine.EndSystemDialog(NULL);
    }
}

void UserData::JoinLobbyLessGame()
{
    if (p_YW->_netDriver->IsConnected())
    {
        uint32_t waitForSession = SDL_GetTicks() + 1000;
        while (SDL_GetTicks() < waitForSession && !p_YW->_netDriver->IsJoined())
            SDL_Delay(10);
        
        if (p_YW->_netDriver->IsJoined())
        {
            netName = "";
            netNameCurPos = 0;
            netLevelID = 0;

            std::string buf = p_YW->_netDriver->GetCurrentSessionName();
            printf("%s\n", buf.c_str());

            for (size_t i = 0; i < buf.length(); i++)
            {
                if (buf[i] == '|')
                {
                    buf.resize(i);

                    netLevelID = std::stoi(buf);
                    break;
                }
            }
            network_listvw.firstShownEntries = 0;
            netLevelName = p_YW->GetLevelName(netLevelID);

            for(TDPPlayerData &p : p_YW->_netDriver->GetPlayersData())
            {
                lobbyPlayers[p.Index].Name = p.name;

                if (p.IsItMe())
                {
                    lobbyPlayers[p.Index].Name += "X";
                }
            }
                
            p_YW->_isNetGame = true;
            netSel = -1;
            netSelMode = NETSCREEN_INSESSION;
            netName = "";
            netNameCurPos = 0;
            network_listvw.firstShownEntries = 0;
            network_listvw.selectedEntry = 0;
            p_YW->GuiWinOpen( &network_listvw );
            isHost = false;

            yw_FractionInit();
        }
    }
    return;
    
    if ( p_YW->_netDriver->GetProvType() != 4 || modemAskSession )
    {
        windp_getNameMsg gName;
        gName.id = netSel;

        if ( p_YW->_netDriver->GetSessionName(&gName) )
        {
            if ( p_YW->_netDriver->GetProvType() == 4 )
                GFX::Engine.BeginSystemDialog(NULL);

            if ( p_YW->_netDriver->JoinSession(gName.name) )
            {
                if ( p_YW->_netDriver->GetProvType() == 4 )
                    GFX::Engine.EndSystemDialog(NULL);

                netName = "";
                netNameCurPos = 0;
                netLevelID = 0;

                std::string buf = gName.name;

                for (size_t i = 0; i < buf.length(); i++)
                {
                    if (buf[i] == '"')
                    {
                        buf.resize(i);

                        netLevelID = atoi(buf.c_str());
                        break;
                    }
                }
                network_listvw.firstShownEntries = 0;
                netLevelName = p_YW->GetLevelName(netLevelID);

                for(TDPPlayerData &p : p_YW->_netDriver->GetPlayersData())
                {
                    lobbyPlayers[p.Index].Name = p.name;
                    
                    if (p.IsItMe())
                    {
                        lobbyPlayers[p.Index].Name += "X";
                    }
                }

                windp_createPlayerMsg crPlayerMsg;
                crPlayerMsg.name = netPlayerName.c_str();
                crPlayerMsg.flags = 1;

                if ( p_YW->_netDriver->CreatePlayer(&crPlayerMsg) )
                {
                    p_YW->_isNetGame = true;
                    netSel = -1;
                    netSelMode = NETSCREEN_INSESSION;
                    netName = "";
                    netNameCurPos = 0;
                    network_listvw.firstShownEntries = 0;
                    network_listvw.selectedEntry = 0;
                    isHost = false;

                    yw_FractionInit();
                }

                rdyStart = false;
                int32_t myIndex = p_YW->_netDriver->GetMyIndex();
                if (myIndex != -1)
                    lobbyPlayers[myIndex].Ready = false;

                p_YW->SendCRC(netLevelID);
            }
            else
            {
                if ( p_YW->_netDriver->GetProvType() == 4 )
                    GFX::Engine.BeginSystemDialog(NULL);

                //sb_0x46bb54__sub0(p_ypaworld, get_lang_string(p_ypaworld->string_pointers_p2, 2400, "YPA ERROR MESSAGE"), get_lang_string(p_ypaworld->string_pointers_p2, 2401, "SESSION NOT LONGER AVAILABLE"));
                fmt::printf("%s: %s\n", Locale::Text::Advanced(Locale::ADV_ERRHEAD), Locale::Text::Advanced(Locale::ADV_ERRNOSESS));

                if ( p_YW->_netDriver->GetProvType() == 4 )
                {
                    netSel = -1;
                    ExitFromNetworkToMain();
                }
                else
                {
                    p_YW->_netDriver->EnumSessions(NULL);
                }
            }
        }
    }
    else
    {
        if ( p_YW->_netDriver->GetProvType() == 4 )
            GFX::Engine.BeginSystemDialog(NULL);

        if ( p_YW->_netDriver->EnumSessions(NULL) )
        {
            modemAskSession = true;
        }

        if ( p_YW->_netDriver->GetProvType() == 4 )
            GFX::Engine.EndSystemDialog(NULL);
    }
}

int yw_DestroyPlayer(NC_STACK_ypaworld *yw, const char *playerName)
{
    int32_t plID = yw->_netDriver->GetPlayerIndex(playerName);
    UserData *usr = yw->_GameShell;

    if ( plID != -1 )
    {
        for (size_t i = plID; i < usr->lobbyPlayers.size() - 1; i++ )
            usr->lobbyPlayers[ i ] = usr->lobbyPlayers[ i + 1 ];
    }

    return yw->_netDriver->DeletePlayer(playerName);
}

void UserData::yw_netcleanup()
{
    p_YW->_netDriver->FlushBroadcastBuffer();

    if( netSelMode ==NETSCREEN_INSESSION || p_YW->_isNetGameStarted )
        yw_DestroyPlayer( p_YW, netPlayerName.c_str() );

    if( ( netSelMode == NETSCREEN_INSESSION && isHost) || p_YW->_isNetGameStarted )
        p_YW->_netDriver->CloseSession(NULL);

    p_YW->_netDriver->Reset(NULL);

    netSelMode = NETSCREEN_MODE_SELECT;

    p_YW->_isNetGame = false;

    network_listvw.firstShownEntries = 0;
    netSel = -1;
    nInputMode = 0;
    isHost = false;
    blocked = false;
    netPlayerOwner = 0;
    netLevelID = 0;
    modemAskSession = false;
    sentAQ = false;

    for (TNetPlayerLobbyData &pl : lobbyPlayers)
        pl = UserData::TNetPlayerLobbyData();

    for (TNetPlayerData &pl : netPlayers)
        pl.Name.clear();

    rdyStart = false;

    p_YW->_isNetGameStarted = false;

    msgBuffers.clear();
    lastSender.clear();
    disconnected = false;
    problemTimer = 0;
    netProblem = 0;
    netProblemCountDown = 0;
    takTime = 0;
    noSent = false;
    FreeFraction = (World::OWNER_GHOR_BIT | World::OWNER_MYKO_BIT | World::OWNER_TAER_BIT);
    SelectedFraction = World::OWNER_RESIST_BIT;
    remoteMode = false;

    log_netlog("netcleanup:      ende\n");
}

void UserData::ExitFromNetworkToMain()
{
    EnvMode = ENVMODE_TITLE;

    network_button->HideScreen();

    titel_button->ShowScreen();

    p_YW->GuiWinClose(&network_listvw);

    NC_STACK_button::WidgetState btnState;
    btnState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;
    btnState.state = 2;

    sub_bar_button->SetState(&btnState);

    btnState.butID = 1219;
    btnState.state = 2;
    sub_bar_button->SetState(&btnState);

    yw_netcleanup();
}

bool yw_initNetLogFile()
{
    dprintf("MAKE ME %s (multiplayer)\n", "yw_initNetLogFile");
    return true;
}

void UserData::yw_NetPrintStartInfo()
{
    if ( yw_initNetLogFile() )
    {
        log_netlog("-------------- Start YPA network session ------------------\n\n");

        size_t pT = p_YW->_netDriver->GetProvType();

        if ( pT == 1 )
            log_netlog("Provider: Hosting\n");
        else if ( pT == 2 )
            log_netlog("Provider: Connect\n");
        else
            log_netlog("!!! Unknown provider\n");

        log_netlog("for this povider I send a dplay-msg after %d ms\n", flush_time_norm);
        log_netlog("Following players startet with the game:\n");

        for(uint32_t i = 0; i < p_YW->_netDriver->GetPlayerCount(); i++)
        {
            int id;
            if ( (int32_t)i == p_YW->_netDriver->GetMyIndex() )
            {
                id = SelectedFraction;
            }
            else
            {
                id = lobbyPlayers[i].NetFraction;
            }
            
            std::string name = p_YW->_netDriver->GetPlayerName(i);

            switch ( id )
            {
            case World::OWNER_RESIST_BIT:
                log_netlog("    %s and plays Resistance\n", name.c_str());
                break;

            case World::OWNER_MYKO_BIT:
                log_netlog("    %s and plays Mykonier\n", name.c_str());
                break;

            case World::OWNER_TAER_BIT:
                log_netlog("    %s and plays Taerkasten\n", name.c_str());
                break;

            case World::OWNER_GHOR_BIT:
                log_netlog("    %s and plays Ghorkov\n", name.c_str());
                break;

            default:
                log_netlog("    %s and plays an unknown race\n", name.c_str());
                break;
            }
        }

        if ( isHost )
            log_netlog("\nThe local player is %s and is HOST\n", netPlayerName.c_str());
        else
            log_netlog("\nThe local player is %s and is CLIENT\n", netPlayerName.c_str());

        std::string tmp = p_YW->GetLevelName(netLevelID);
        log_netlog("They play level %d, this is %s\n", netLevelID, tmp.c_str());
        log_netlog("the session started at timeindex %d\n", p_YW->_timeStamp / 1000);
        log_netlog("\n\n--------------------------------------------------------\n");
    }
    else
    {
        ypa_log_out("Unable to open Network log script\n");
    }
}


void UserData::LogNetStatistics()
{
//  p_ypaworld->windp->windp_func91(v2);
    log_netlog("---------------- YPA Network Statistics -------------------\n\n");
    log_netlog("Sending:\n");
//  log_netlog("   bytes per second: %d\n", field_5487);
//  log_netlog("   bps minimum:      %d\n", field_547F);
//  log_netlog("   bps maximum:      %d\n", field_5483);
//  log_netlog("   packet size:      %d\n", field_549F);
//  log_netlog("   packet minimum:   %d\n", field_5493);
//  log_netlog("   packet maximum:   %d\n", field_5497);
    log_netlog("receiving:\n");
//  log_netlog("   bytes per second: %d\n", field_547B);
//  log_netlog("   bps minimum:      %d\n", field_5473);
//  log_netlog("   bps maximum:      %d\n", field_5477);
    log_netlog("statistical sr-thread info\n");
//  log_netlog("   max. in send list %d\n", v2[3]);
//  log_netlog("   max. in recv list %d\n", v2[2]);
    log_netlog("\nthe session ended at timeindex %d\n", p_YW->_timeStamp / 1000);
    log_netlog("-----------------------------------------------------------\n");

    dprintf("MAKE ME %s (multiplayer)\n", "LogNetStatistics");
}




//////////////multiplayer wnd///////////
netgamelst netgame_wnd;

// Spans the net game window across the screen at two thirds of its height.
void SetNetGameWindowLayout(NC_STACK_ypaworld *yw)
{
    netgame_wnd.x = 0;
    netgame_wnd.y = 2 * yw->_screenSize.y / 3;
    netgame_wnd.w = yw->_screenSize.x - 1;
    netgame_wnd.maxWidth = yw->_screenSize.x;
}

void CreateNetGameWindow(NC_STACK_ypaworld *yw)
{
    netgame_wnd.flags = 0x138;
    netgame_wnd.h = 2 * yw->_fontH + 2;

    netgame_wnd.buttons.resize(2);
    netgame_wnd.buttons[0] = ButtonBox(yw->_guiTiles[0]->map[97].w, 0,
                                       yw->_guiTiles[0]->map[97].w, yw->_fontH);
    netgame_wnd.buttons[1] = ButtonBox(0, 0,
                                       yw->_guiTiles[0]->map[97].w, yw->_fontH);

    netgame_wnd.btnWidth = yw->_guiTiles[0]->map[97].w;
    netgame_wnd.inputHeight = 2 * yw->_fontH;
    netgame_wnd.minInputHeight = netgame_wnd.inputHeight;
    netgame_wnd.text[0] = 0;
    netgame_wnd.textLen = 0;

    SetNetGameWindowLayout(yw);

    netgame_wnd.cmdCommands.reserve(1024);

}

//netgui update
// TODO(multiplayer): A stub, so netgamelst's members are only ever written.
void NetGameWindowHandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    dprintf("MAKE ME %s (multiplayer)\n", "NetGameWindowHandleInput");
}
