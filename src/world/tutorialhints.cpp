#include "tutorialhints.h"
#include "../yw.h"
#include "../yw_internal.h"
#include "state.h"

extern tehMap robo_map;

namespace World
{

// The player sits in their host station, or in one of its guns
static bool PlayerInHostStation(NC_STACK_ypaworld *world)
{
    return world->UserUnit() == world->UserRobo() || world->_playerInHSGun;
}

// Every other faction is out of units
static bool EnemiesDefeated(NC_STACK_ypaworld *world)
{
    uint32_t factions = world->FactionsWithUnits();

    if ( world->LocalOwner() > 0 )
        factions &= ~(1u << world->LocalOwner());

    return !factions;
}

// These three are also done once the enemies are defeated, and while the
// player is out of the host station in one of their units

static bool HasSquadsOrLeftStation(NC_STACK_ypaworld *world)
{
    return EnemiesDefeated(world) || !PlayerInHostStation(world) || !world->_cmdrsRemap.empty();
}

static bool HasNoSquadsOrLeftStation(NC_STACK_ypaworld *world)
{
    return EnemiesDefeated(world) || !PlayerInHostStation(world) || world->_cmdrsRemap.empty();
}

static bool MapOpenOrLeftStation(NC_STACK_ypaworld *world)
{
    return EnemiesDefeated(world) || !PlayerInHostStation(world) || robo_map.IsOpen();
}

static bool OwnsPowerStation(NC_STACK_ypaworld *world)
{
    for ( const auto &ps : world->_powerStations )
    {
        if ( ps.second.pCell && ps.second.pCell->owner == world->LocalOwner() )
            return true;
    }

    return false;
}

static bool StationOnPowerStation(NC_STACK_ypaworld *world)
{
    const cellArea *cell = world->UserRobo()->_pSector;
    return cell && cell->PurposeType == cellArea::PT_POWERSTATION;
}

static bool GateOpen(NC_STACK_ypaworld *world)
{
    for ( const TMapGate &gate : world->_levelInfo.Gates )
    {
        if ( gate.PCell && gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
            return true;
    }

    return false;
}


void TutorialHints::Start(int eventLoopId)
{
    Stop();

    switch ( eventLoopId )
    {
    case 1:
        _hints = {
            {HINT_AFTER_PAUSE, 5000,  NULL,                     {0}},
            {HINT_UNTIL_DONE,  20000, HasSquadsOrLeftStation,   {47}},
            {HINT_UNTIL_DONE,  20000, HasNoSquadsOrLeftStation, {48}},
            {HINT_UNTIL_DONE,  20000, EnemiesDefeated,          {49}},
            {HINT_FINISH,      15000, NULL,                     {60}},
        };
        break;

    case 2:
        _hints = {
            {HINT_AFTER_PAUSE, 5000,  NULL,                   {0}},
            {HINT_UNTIL_DONE,  20000, HasSquadsOrLeftStation, {47}},
            {HINT_UNTIL_DONE,  20000, MapOpenOrLeftStation,   {50}},
            {HINT_REPEAT,      20000, EnemiesDefeated,        {64, 51, 52}},
            {HINT_UNTIL_DONE,  20000, EnemiesDefeated,        {53}},
            {HINT_FINISH,      15000, NULL,                   {61}},
        };
        break;

    case 3:
        _hints = {
            {HINT_AFTER_PAUSE,    3000,  NULL,                  {0}},
            {HINT_AFTER_PAUSE,    8000,  NULL,                  {59}},
            {HINT_UNTIL_DONE,     60000, OwnsPowerStation,      {54, 94}},
            {HINT_AFTER_PAUSE,    3000,  NULL,                  {95}},
            {HINT_UNTIL_DONE,     60000, StationOnPowerStation, {97, 96}},
            {HINT_AFTER_PAUSE,    3000,  NULL,                  {98}},
            {HINT_AFTER_PAUSE,    9000,  NULL,                  {99}},
            {HINT_AFTER_PAUSE,    5000,  NULL,                  {0}},
            {HINT_UNTIL_DONE,     60000, EnemiesDefeated,       {101, 100}},
            {HINT_AFTER_PAUSE,    4000,  NULL,                  {102}},
            {HINT_UNTIL_DONE,     60000, GateOpen,              {55}},
            {HINT_AFTER_PAUSE,    3000,  NULL,                  {103}},
            {HINT_AFTER_PAUSE,    45000, NULL,                  {0}},
            {HINT_NOW_AND_REPEAT, 60000, NULL,                  {104, 56, 65}},
        };
        break;

    default:
        break;
    }
}

void TutorialHints::Stop()
{
    _hints.clear();
    _lastPlayedHint = -1;
    _lastPlayTime = 0;
}

void TutorialHints::VisitState(State::Visitor &v)
{
    std::vector<uint8_t> reached, played;
    std::vector<int32_t> reachedTime, nextMessage;

    for ( const Hint &hint : _hints )
    {
        reached.push_back(hint.Reached);
        reachedTime.push_back(hint.ReachedTime);
        played.push_back(hint.Played);
        nextMessage.push_back(hint.NextMessage);
    }

    v.Field("hintReached", reached);
    v.Field("hintReachedTime", reachedTime);
    v.Field("hintPlayed", played);
    v.Field("hintNextMessage", nextMessage);
    v.Field("lastHint", _lastPlayedHint);
    v.Field("lastHintTime", _lastPlayTime);

    if ( v.Reads(State::INTERNAL) )
    {
        for (size_t i = 0; i < _hints.size(); i++)
        {
            Hint &hint = _hints[i];
            hint.Reached = i < reached.size() && reached[i];
            hint.ReachedTime = i < reachedTime.size() ? reachedTime[i] : 0;
            hint.Played = i < played.size() && played[i];
            hint.NextMessage = i < nextMessage.size() ? nextMessage[i] : 0;
        }
    }
}

void TutorialHints::Update(NC_STACK_ypaworld *world)
{
    size_t id;
    Result result = RESULT_SKIP;

    for ( id = 0; id < _hints.size(); id++ )
    {
        result = Consider(world, _hints[id]);

        if ( result != RESULT_SKIP )
            break;
    }

    if ( result != RESULT_PLAY )
        return;

    // The same hint again only after its time
    if ( (int)id == _lastPlayedHint && world->_timeStamp - _lastPlayTime <= _hints[id].Time )
        return;

    Play(world, id);
}

TutorialHints::Result TutorialHints::Consider(NC_STACK_ypaworld *world, Hint &hint)
{
    int32_t now = world->_timeStamp;

    if ( !hint.Reached )
    {
        hint.Reached = true;
        hint.ReachedTime = now;
    }

    switch ( hint.Type )
    {
    case HINT_REPEAT:
        if ( now - hint.ReachedTime <= hint.Time )
            return RESULT_SKIP;

        hint.ReachedTime = now;

        if ( hint.IsDone && hint.IsDone(world) )
            return RESULT_SKIP;

        return RESULT_PLAY;

    case HINT_UNTIL_DONE:
        if ( hint.IsDone && !hint.IsDone(world) )
            return RESULT_PLAY;

        return RESULT_SKIP;

    case HINT_NOW_AND_REPEAT:
        if ( !hint.Played )
            hint.Played = true;
        else if ( now - hint.ReachedTime <= hint.Time )
            return RESULT_SKIP;
        else
            hint.ReachedTime = now;

        if ( hint.IsDone && hint.IsDone(world) )
            return RESULT_SKIP;

        return RESULT_PLAY;

    case HINT_AFTER_PAUSE:
    case HINT_FINISH:
    {
        if ( now - hint.ReachedTime <= hint.Time )
            return RESULT_WAIT;

        if ( hint.Played )
            return RESULT_SKIP;

        hint.Played = true;

        Result result = RESULT_PLAY;

        if ( hint.IsDone && hint.IsDone(world) )
            result = RESULT_SKIP;

        if ( hint.Type == HINT_FINISH )
            OpenExitTrainingDialog(world);

        return result;
    }
    }

    return RESULT_SKIP;
}

void TutorialHints::Play(NC_STACK_ypaworld *world, size_t hintId)
{
    Hint &hint = _hints[hintId];

    _lastPlayedHint = hintId;
    _lastPlayTime = world->_timeStamp;

    if ( hint.Messages.empty() )
        return;

    TInfoMessage msg;
    msg.Priority = 100;
    msg.unit = NULL;
    msg.MsgID = hint.Messages[hint.NextMessage];

    // The messages take turns, the first again after the last
    hint.NextMessage = (hint.NextMessage + 1) % hint.Messages.size();

    world->ShowMessage(&msg);
}

}
