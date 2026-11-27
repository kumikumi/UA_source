#ifndef YROBO_H_INCLUDED
#define YROBO_H_INCLUDED

#include "nucleas.h"
#include "ypabact.h"
#include "world/protos.h"


class NC_STACK_yparobo;

struct TRoboAttackSlot
{
    uint32_t squadCommandID = 0;
    uint32_t targetCommandID = 0;
};

struct TCommanderSearch
{
    int flags;
    BactTarget tgt;
    uint8_t tgType;
    vec3d tgt_pos;
    uint32_t prim_comm_id;
    uint32_t comm_id;
    float distance;
    NC_STACK_ypabact *found_comm;
};

struct TRoboMessage
{
    NC_STACK_ypabact *unit;
    int msgID;
    int param1;
    int param2;
    int param3;
    int priority;
};

struct robo_loct1
{
    int energ;
    float distance;
    int aggr;
    uint8_t tgType;
    vec3d tgt_pos;
    NC_STACK_ypabact *tgt_bact;
    int command_id;
    int forbidden;
    int want;
    int good;
    int bad;
    int job;
};

class NC_STACK_yparobo: public NC_STACK_ypabact
{
public:
    virtual size_t Init(IDVList &stak);
    virtual size_t Deinit();
    virtual size_t SetParameters(IDVList &stak) override;
    virtual void AI_layer1(update_msg *arg);
    virtual void AI_layer3(update_msg *arg);
    virtual void User_layer(update_msg *arg);
    virtual void Move(move_msg *arg);
    virtual void Die();
    virtual size_t SetPosition(TUnitPlacement *arg);
    virtual void EnergyInteract(update_msg *arg);
    virtual void Renew();
    virtual void VisitView(World::State::Visitor &v) override;
    virtual void VisitInternal(World::State::Visitor &v) override;
    virtual void HandBrake(update_msg *);
    virtual void CorrectPositionOnLand();
    virtual void DeadTimeUpdate(update_msg *arg);
    virtual void FindMatchingCommander(TCommanderSearch *arg);
    virtual size_t GetTargetByCommandID(setTarget_msg *arg);

    NC_STACK_yparobo();
    virtual ~NC_STACK_yparobo() {};
    
    virtual const std::string ClassName() const {
        return __ClassName;
    };

    enum ROBOSTATE
    {
        ROBOSTATE_SAFEZONE = 1,
        ROBOSTATE_ROBOFOUND = 2,
        ROBOSTATE_DOCKINUSE = 4,
        ROBOSTATE_SETDOCKTARGET = 8,

        ROBOSTATE_BUILDRADAR = 0x10,
        ROBOSTATE_BUILDPOWER = 0x20,
        ROBOSTATE_BUILDCONQUER = 0x40,
        ROBOSTATE_BUILDDEFENSE = 0x80,
        ROBOSTATE_BUILDSAFE = 0x100,

        ROBOSTATE_READYDEFENSE = 0x200,
        ROBOSTATE_READYCONQUER = 0x400,
        ROBOSTATE_READYRADAR = 0x800,
        ROBOSTATE_READYPOWER = 0x1000,
        ROBOSTATE_READYSAFE = 0x2000,

        ROBOSTATE_PLAYERROBO = 0x4000,
        ROBOSTATE_DANGER = 0x8000,

        ROBOSTATE_FOUNDPLACE = 0x10000,
        ROBOSTATE_CHANGEPLACE = 0x20000,
        ROBOSTATE_READYRECON = 0x40000,
        ROBOSTATE_BUILDRECON = 0x80000,
        ROBOSTATE_READYROBO = 0x100000,
        ROBOSTATE_BUILDROBO = 0x200000,

        ROBOSTATE_MOVE = 0x400000,
        ROBOSTATE_USEVHCLPARTS = 0x800000
    };

    enum ROBO_ATT
    {
        ROBO_ATT_PROTO = 0x80002001,
        ROBO_ATT_EP_CONQUER = 0x80002002,
        ROBO_ATT_EP_DEFENSE = 0x80002003,
        ROBO_ATT_EP_RADAR = 0x80002004,
        ROBO_ATT_EP_POWER = 0x80002005,
        ROBO_ATT_EP_SAFETY = 0x80002006,
        ROBO_ATT_BATT_VEHICLE = 0x80002008,
        ROBO_ATT_BATT_BUILDING = 0x80002009,
        ROBO_ATT_BATT_BEAM = 0x8000200A,
        ROBO_ATT_FILL_MODE = 0x8000200B,
        ROBO_ATT_WAIT_SWAY = 0x8000200C,
        ROBO_ATT_WAIT_ROTATE = 0x8000200D,
        ROBO_ATT_GUNS = 0x8000200E,
        ROBO_ATT_EP_CHANGEPLACE = 0x8000200F,
        ROBO_ATT_EP_RECONNOITRE = 0x80002010,
        ROBO_ATT_EP_ROBO = 0x80002011,
        ROBO_ATT_ROBOSTATE = 0x80002012,
        ROBO_ATT_VIEWANGLE = 0x80002013,
        ROBO_ATT_SAFDELAY = 0x80002014,
        ROBO_ATT_POWDELAY = 0x80002015,
        ROBO_ATT_RADDELAY = 0x80002016,
        ROBO_ATT_CPLDELAY = 0x80002017,
        ROBO_ATT_DEFDELAY = 0x80002018,
        ROBO_ATT_CONDELAY = 0x80002019,
        ROBO_ATT_ROBDELAY = 0x8000201A,
        ROBO_ATT_RECDELAY = 0x8000201B,
        ROBO_ATT_LOADFLAGS = 0x8000201C,
        ROBO_ATT_LOSSFLAGS = 0x8000201D,
        ROBO_ATT_ABSRELOAD = 0x8000201E
    };


    virtual void setBACT_inputting(bool inpt, World::Player *player) override;

    // Following players' orders (its faction has players), or its own AI's.
    // An AI taking over a host station that never had plans of its own gets
    // the campaign's usual ones.
    void SetPlayerRun(bool run);

    // The nearest other host station of its faction still going; NULL when
    // there's none
    NC_STACK_yparobo *NearestFellowHostStation() const;

    // Sending a squad (by its commander) somewhere, which needs no host
    // station: along a path when it has ground units
    static int SendSquadToPosition(NC_STACK_ypabact *bact, float x, float z);
    static int SendSquadToUnit(NC_STACK_ypabact *bact, NC_STACK_ypabact *bact2);
    static int IsCommanderClosestToTarget(NC_STACK_ypabact *bact);
    static void ClearSquadSecondaryTargets(NC_STACK_ypabact *bact);
    static void CopyWaypointsToSquad(NC_STACK_ypabact *bact);
    virtual void setROBO_proto(World::TRoboProto *);
    virtual void setROBO_epConquer(int);
    virtual void setROBO_epDefense(int);
    virtual void setROBO_epRadar(int);
    virtual void setROBO_epPower(int);
    virtual void setROBO_epSafety(int);
    virtual void setROBO_battVehicle(int);
    virtual void setROBO_battBeam(int);
    virtual void setROBO_fillMode(int);
    virtual void setROBO_waitSway(int);
    virtual void setROBO_waitRotate(int);
    virtual void setROBO_epChangeplace(int);
    virtual void setROBO_epReconnoitre(int);
    virtual void setROBO_epRobo(int);
    virtual void setROBO_viewAngle(int);
    virtual void setROBO_safDelay(int);
    virtual void setROBO_powDelay(int);
    virtual void setROBO_radDelay(int);
    virtual void setROBO_cplDelay(int);
    virtual void setROBO_defDelay(int);
    virtual void setROBO_conDelay(int);
    virtual void setROBO_robDelay(int);
    virtual void setROBO_recDelay(int);

    virtual World::TRoboCollSet *getBACT_collNodes()
    { return &_roboColls; }
    
    virtual int getROBO_epConquer();
    virtual int getROBO_epDefense();
    virtual int getROBO_epRadar();
    virtual int getROBO_epPower();
    virtual int getROBO_epSafety();
    virtual int getROBO_battVehicle();
    virtual int getROBO_battBeam();
    virtual int getROBO_fillMode();
    virtual std::vector<World::TRoboGun> &GetGuns();
    virtual int getROBO_epChangeplace();
    virtual int getROBO_epReconnoitre();
    virtual int getROBO_epRobo();
    virtual int getROBO_roboState();
    virtual int getROBO_safDelay();
    virtual int getROBO_powDelay();
    virtual int getROBO_radDelay();
    virtual int getROBO_cplDelay();
    virtual int getROBO_defDelay();
    virtual int getROBO_conDelay();
    virtual int getROBO_robDelay();
    virtual int getROBO_recDelay();
    virtual int getROBO_loadFlags();
    virtual int getROBO_lossFlags();
    virtual int getROBO_absReload();
    
    void UpdateGateBeamOut();
    
    bool IsPlayerRobo() const;
    
public:
    void HandleUserCommands(World::Player *player);
    
protected:

    void checkCommander();
    void InitForce(NC_STACK_ypabact *unit);
    void DispatchDockedForce(NC_STACK_ypabact *unit);

    NC_STACK_ypabact *AllocForce(robo_loct1 *arg);

    void buildConquer();
    void buildDefense();
    void buildRobo();
    void buildReconnoitre();

    void buildRadar(update_msg *arg);
    void buildPower(update_msg *arg);
    void buildSafe(update_msg *arg);
    void changePlace();


    void AI_doWish(update_msg *arg);
    void AI_doMove(update_msg *arg);
    void AI_service(update_msg *arg);
    void AI_clearAttackerSlots();
    void AI_solveTrouble(update_msg *arg);
    void AI_checkWorld(update_msg *arg);

    void doBeamUpdate(int frameTime);
    void doUserCommands(World::Player *player);
    void wallow(update_msg *arg);
    void searchEnemyRobo();
    void usersRoboEnergyCheck();

    void checkDanger();
    size_t checkCollisions(float dtime);
    
    bool IsNotUserControlled();
    void UpdateCollisionPointPositions();
    int RateSectorForRadar(const Common::Point &sc);
    int RateSectorForFlak(const Common::Point &sc);
    int RateSectorForRecon(const Common::Point &sc);
    int RateSectorForRelocation();
    int FindEnemyRoboTarget(int *commandId, Common::Point *pCellId);
    int FindDefenseTarget(int *commandId, Common::Point *pCellId);
    int IsEnemyUnitNear();
    int IsEnemyRoboNear();
    int RateSectorForPowerStation(const Common::Point &sc);
    int RateSectorForConquer(const Common::Point &sc);
    int CountOwnFlakSectors();
    int CountOwnRadarSectors();
    int CountOwnedSectors();
    int CountUnseenSectors();
    int RateAreaControl(const Common::Point &cellId);
    float RateEnemyTarget(NC_STACK_ypabact *bctnd);
    
    
    void SendSquadAgainstEnemyRobo(NC_STACK_ypabact *unit1, NC_STACK_ypabact *unit2);
    void UpdateRoboAttackSquad(NC_STACK_ypabact *unit, TRoboAttackSlot *rbt);
    
    void RotateGunsAroundAxis(const vec3d &vaxis, float angle);
    void TurnRoboAboutOwnUpAxis(float angle);
    void TurnRoboAboutWorldUpAxis(float angle);
    
    void GetBuildSiteApproachTarget(int sectID, setTarget_msg *target);
    
    void UpdateViewerLookAngles(update_msg *arg);
    int FindBestBldPowerStation();
    int FindBestBldDefenceCenter();
    int FindBestBldRadar();


public:

    //Data
    static constexpr const char * __ClassName = "yparobo.class";

    
    float _roboFlotage;
    World::TRoboCollSet _roboColls;
    float _roboYPos;

    uint8_t _roboWFlags;
    int32_t _roboState;
    int _roboTimeScale;

    uint8_t _roboEpConquer;
    uint8_t _roboEpRadar;
    uint8_t _roboEpPower;
    uint8_t _roboEpDefense;
    uint8_t _roboEpSafety;
    uint8_t _roboEpRecon;
    uint8_t _roboEpChangePlace;
    uint8_t _roboEpRobo;

    int _roboDockEnerg;
    int _roboDockCnt;
    uint32_t _roboDockUser;
    int _roboDockTime;
    vec3d _roboDockTargetPos;
    uint8_t _roboDockTargetType;
    uint8_t _roboDockAggr;
    uint32_t _roboDockTargetCommandID;
    vec3d _roboDockPos;

    int _roboRadarValue;
    cellArea *_roboRadarCell;  //For AI
    int _roboRadarCellID;  //For AI
    int _roboRadarCellIndex;
    int _roboRadarTime;
    int _roboRadarDelay; //??

    int _roboSafetyValue;
    cellArea *_roboSafetyCell;  //For AI
    int _roboSafetyCellID;  //For AI
    int _roboSafetyCellIndex;
    int _roboSafetyTime;
    int _roboSafetyDelay; //??

    //Power
    int _roboPowerValue;
    cellArea *_roboPowerCell;  //For AI
    int _roboPowerCellID;  //For AI
    int _roboPowerCellIndex;
    int _roboPowerTime;
    int _roboPowerDelay; //??

    //Enemy
    int _roboEnemyValue;
    cellArea *_roboEnemyCell;  //For AI
    int _roboEnemyCellID;  //For AI
    uint32_t _roboEnemyCommandID;  //For AI
    int _roboEnemyTime;
    uint32_t _roboEnemyDiv;
    int _roboEnemyDelay; //??

    int _roboConqValue;
    cellArea *_roboConqCell;  //For AI
    int _roboConqCellID;  //For AI
    int _roboConqCellIndex;
    int _roboConqTime;
    int _roboConqDelay; //??

    int _roboPositionValue;
    cellArea *_roboPositionCell;  //For AI
    int _roboPositionCellID;  //For AI
    int _roboPositionCellIndex;  //For AI
    int _roboPositionTime;
    int _roboPositionDelay; //??
    
    int _roboExploreValue;
    cellArea *_roboExploreCell;  //For AI
    int _roboExploreCellID;  //For AI
    int _roboExploreCellIndex;  //For AI
    int _roboExploreTime;
    int _roboExploreDelay; //??
    
    int _roboDangerValue;
    cellArea *_roboDangerCell;  //For AI
    int _roboDangerCellID;  //For AI
    uint32_t _roboDangerCommandID;  //For AI
    int _roboDangerTime;
    int _roboDangerDelay; //??
    
    int _roboVehicleCellID;  //For AI
    cellArea *_roboVehicleCell;  //For AI
    int _roboVehicleDuty;  //For AI
    uint32_t _roboVehicleCommandID;  //For AI
    
    
    int _roboBuildingCellID;  //For AI
    cellArea *_roboBuildingCell;  //For AI
    int _roboBuildingDuty;  //For AI
    
    int _roboTestEnemyTime;
    
    std::vector<World::TRoboGun> _roboGuns;

    int _roboEnergyLife; //??
    
    int _roboEnergyMove; //??

    uint8_t _roboFillMode;
    uint8_t _roboEnergyLoadFlags;
    uint8_t _roboEnergyLossFlags;

    int _roboEnergyReloadPS; // Reload per second
    
    int _roboBuildSpare;
    int _roboVehicleSpare;
    
    int _roboBeamTimePre;
    vec3d _roboBeamPos;
    int _roboBeamFXTime;
    
    std::array<TRoboAttackSlot, 16> _roboAttackers;
    int _roboAttackersTime;
    int _roboAttackersClearTime; //FIX IT
};

#endif // YROBO_H_INCLUDED
