#ifndef YBACT_H_INCLUDED
#define YBACT_H_INCLUDED

#include "nucleas.h"
#include "system/gfx.h"
#include "base.h"
#include "listnode.h"
#include "types.h"
#include "world/protos.h"

// !!!! if period is small, then this never happen
#define BACT_MIN_ANGLE 0.0002

class NC_STACK_ypabact;
class NC_STACK_yparobo;
class NC_STACK_ypamissile;

class NC_STACK_ypaworld;

namespace World { namespace State { class Visitor; } }

struct TSectorDamage;

struct cellArea;





enum EVPROTO_FLAG
{
    EVPROTO_FLAG_ACTIVE = 1,
    EVPROTO_FLAG_SCALE  = 2
};

struct extra_vproto
{
    float scale = 0.0;
    vec3d pos;
    mat3x3 rotate;
    int flags = 0;
    NC_STACK_base::Instance *vp = NULL;
    
    ~extra_vproto()
    {
        if (vp)
            delete vp;
    }
    
    void SetVP(NC_STACK_base::Instance *newVP)
    {
        Common::DeleteAndNull(&vp);
        vp = newVP;
    }
    
    void SetVP(NC_STACK_base *bas)
    {
        Common::DeleteAndNull(&vp);
        
        if (bas)
            vp = bas->GenRenderInstance();
        else
            vp = NULL;
    }
};

enum BACT_TGT_TYPE
{
    BACT_TGT_TYPE_NONE = 0,
    BACT_TGT_TYPE_CELL = 1,
    BACT_TGT_TYPE_UNIT = 2,
    BACT_TGT_TYPE_FRMT = 3,
    BACT_TGT_TYPE_DRCT = 4,
    BACT_TGT_TYPE_CELL_IND = 5,
    BACT_TGT_TYPE_UNIT_IND = 6
};

enum BACT_STATUS
{
    BACT_STATUS_NOPE = 0,
    BACT_STATUS_NORMAL = 1,
    BACT_STATUS_DEAD = 2,
    BACT_STATUS_IDLE = 3,
    BACT_STATUS_CREATE = 4,
    BACT_STATUS_BEAM = 5
};

enum BACT_STFLAG
{
    BACT_STFLAG_FIGHT_P     = 1, // Primary target fight
    BACT_STFLAG_FIGHT_S     = 2, // Secondary target fight
    BACT_STFLAG_FORMATION   = 4,
    //BACT_STFLAG_DODGE = 8,          //Unused flag
    BACT_STFLAG_DODGE_LEFT  = 0x10,
    BACT_STFLAG_DODGE_RIGHT = 0x20,
    BACT_STFLAG_MOVE        = 0x40,
    BACT_STFLAG_UPWRD       = 0x80,
    BACT_STFLAG_FIRE        = 0x100,
    BACT_STFLAG_LAND        = 0x200,
    BACT_STFLAG_DEATH1      = 0x400,
    BACT_STFLAG_DEATH2      = 0x800,
    //BACT_STFLAG_AKKU        = 0x1000,  //Unused flag
    BACT_STFLAG_APPROACH    = 0x2000,
    BACT_STFLAG_ESCAPE      = 0x4000,
    BACT_STFLAG_XLEFT       = 0x8000,
    BACT_STFLAG_YUP         = 0x10000,
    BACT_STFLAG_BCRASH      = 0x20000,
    BACT_STFLAG_LCRASH      = 0x40000,
    BACT_STFLAG_UNUSE       = 0x80000,
    BACT_STFLAG_SCALE       = 0x100000,
    BACT_STFLAG_SHAKE       = 0x200000,
    BACT_STFLAG_NORENDER    = 0x400000,
    BACT_STFLAG_ISVIEW      = 0x800000,
    BACT_STFLAG_SEFFECT     = 0x1000000,
    BACT_STFLAG_CLEAN       = 0x2000000,
    BACT_STFLAG_WAYPOINT    = 0x4000000,
    BACT_STFLAG_WAYPOINTCCL = 0x8000000,
    BACT_STFLAG_NOMSG       = 0x10000000,
    BACT_STFLAG_DSETTED     = 0x20000000,
    BACT_STFLAG_ATTACK      = 0x40000000
};

enum BACT_OFLAG
{
    BACT_OFLAG_VIEWER     = 1,
    BACT_OFLAG_USERINPT   = 2,
    BACT_OFLAG_EXACTCOLL  = 4,
    BACT_OFLAG_BACTCOLL   = 8,
    BACT_OFLAG_LANDONWAIT = 0x10,
    BACT_OFLAG_EXTRAVIEW  = 0x20,
    BACT_OFLAG_ALWAYSREND = 0x40
};

union BactTarget
{
    NC_STACK_ypabact *pbact;
    cellArea *pcell;
};




enum BACT_TYPES
{
    BACT_TYPES_NOPE = 0,
    BACT_TYPES_BACT = 1,
    BACT_TYPES_TANK = 2,
    BACT_TYPES_ROBO = 3,
    BACT_TYPES_MISSLE = 4,
    BACT_TYPES_ZEPP = 5,  //No real class
    BACT_TYPES_FLYER = 6,
    BACT_TYPES_UFO = 7,
    BACT_TYPES_CAR = 8,
    BACT_TYPES_GUN = 9,
    BACT_TYPES_HOVER = 10 //No real class
};

struct newMaster_msg
{
    NC_STACK_ypabact *bact;
    World::RefBactList *list;
};

struct TUnitPlacement
{
    vec3d pos;
    int flags;
};

struct setState_msg
{
    int newStatus;
    int setFlags;
    int unsetFlags;
    
    setState_msg()
    {
        newStatus = 0;
        setFlags = 0;
        unsetFlags = 0;
    }
};

struct update_msg
{
    int gTime = 0;
    int frameTime = 0;
    TInputState *inpt = NULL;
    int units_count = 0;
    int user_action = 0;
    int protoID = 0;
    NC_STACK_ypabact *selectBact = NULL;
    cellArea *target_Sect = NULL;
    vec3d target_point;
    NC_STACK_ypabact *target_Bact = NULL;
    int energy = 0;
};

struct setTarget_msg
{
    uint8_t tgt_type;
    uint32_t priority;
    BactTarget tgt;
    vec3d tgt_pos;
};

struct TPathRequest
{
    vec2d from;
    vec2d to;
    int16_t steps_cnt;
    int16_t tgt_type;
    vec3d waypoints[32];
};

struct TEnergyChange
{
    int energy;
    NC_STACK_ypabact *unit;
    uint8_t killerOwner = 0; // Credited with a kill when there is no unit, e.g. the Stoudson bomb's owner
};

struct TCollisionNormal
{
    vec3d normal;
};

struct TFormationSlot
{
    int slot;
    vec3d pos;
    //vec3d pos2;
};

struct move_msg
{
    float period;
    vec3d thrustDir;
    int flag;
};

struct TSummaryQuery
{
    int enrg_type;
    int enrg_sum;
};

struct TReorgOrder
{
    int mode;
    NC_STACK_ypabact *unit;
};

struct TForcesRatio
{
    vec3d pos;
    int ownEnergy;
    int enemyEnergy;
    int flags;
};

struct THudVisor
{
    int mgunSight;
    int weaponSight;
    float mgunX;
    float mgunY;
    float weaponX;
    float weaponY;
    NC_STACK_ypabact *target;

    THudVisor()
    {
        mgunSight = 0;
        weaponSight = 0;
        mgunX = 0.0;
        mgunY = 0.0;
        weaponX = 0.0;
        weaponY = 0.0;
        target = NULL;
    }
};

struct TMinigunShot
{
    vec3d dir;
    float period;
    int gTime;
};

struct TAimQuery
{
    int flags;
    vec3d aim_dir;
    NC_STACK_ypabact *ret_bact;
};

struct TMissileLaunch
{
    vec3d direction;
    vec3d start_point;
    int tgType;
    BactTarget target;
    vec3d tgt_pos;
    int weapon;
    int g_time;
    int flags;
};

struct TFightTarget
{
    vec3d pos;
    BactTarget target;
    int prio;
    float fperiod;
    int g_time;
};

struct TTargetAssess
{
    int tgType;
    int priority;
};

struct TCrashLandStep
{
    int flags;
    int frameTime;
};

struct TFireCheck
{
    vec3d pos;
    int tgType;
    float radius;
};

struct TImpulse
{
    int energ;
    vec3d pos;
    vec3d dir;
    float force;
    float mass;
};

struct TBactAttacker
{
	int type = 0; // primary / secondary
	NC_STACK_ypabact *attacker = NULL;

    TBactAttacker(int t = 0, NC_STACK_ypabact *bact = NULL)
    : type(t), attacker(bact) {};

    inline bool operator==(const TBactAttacker &b) const
    {
        if (type != b.type)
            return false;
        
        if (attacker != b.attacker)
            return false;
        return true;
    }
};


class NC_STACK_ypabact: public NC_STACK_nucleus
{
public:
    enum TA
    {
        TA_CANCEL,
        TA_MOVE,
        TA_FIGHT,
        TA_IGNORE
    };
public:
    virtual size_t Init(IDVList &stak);
    virtual size_t Deinit();
    virtual size_t SetParameters(IDVList &stak);
    virtual void Update(update_msg *arg);
    virtual void Render(baseRender_msg *arg);
    virtual void SetTarget(setTarget_msg *arg);
    virtual void AI_layer1(update_msg *arg);
    virtual void AI_layer2(update_msg *arg);
    virtual void AI_layer3(update_msg *arg);
    virtual void User_layer(update_msg *arg);
    virtual void AddSubject(NC_STACK_ypabact *kid);
    virtual void SetNewMaster(newMaster_msg *arg);
    virtual void Move(move_msg *arg);
    virtual void FightWithBact(TFightTarget *arg);
    virtual void FightWithSect(TFightTarget *arg);
    virtual void Die();
    virtual void SetState(setState_msg *arg);
    virtual size_t LaunchMissile(TMissileLaunch *arg);
    virtual size_t SetPosition(TUnitPlacement *arg);
    virtual void GetSummary(TSummaryQuery *arg);
    virtual void EnergyInteract(update_msg *arg);
    virtual void ApplyImpulse(TImpulse *arg);
    virtual void ModifyEnergy(TEnergyChange *arg);
    virtual bool IsFastAlong(vec3d *arg);
    virtual size_t CrashOrLand(TCrashLandStep *arg);
    virtual size_t CollisionWithBact(int arg);
    virtual void Recoil(TCollisionNormal *arg);
    virtual NC_STACK_ypabact *GetSectorTarget(Common::Point CellId) const;
    virtual void GetBestSectorPart(vec3d *arg);
    virtual void GetForcesRatio(TForcesRatio *arg);
    virtual void GetFormationPosition(TFormationSlot *arg);
    virtual void Renew();

    // Its state, field by field: see World::State
    virtual void VisitState(World::State::Visitor &v);

    // Which of its models an instance draws: 0 none, 1 normal, 2 firing,
    // 3 waiting, 4 dead, 5 exploding, 6 beaming in, 7 another, 8 and on its
    // effect models (_vp_fx_models)
    uint8_t ModelIndex(const NC_STACK_base::Instance *inst) const;
    NC_STACK_base *ModelByIndex(uint8_t index) const;

    virtual void HandBrake(update_msg *arg);
    virtual void CreationTimeUpdate(update_msg *arg);
    virtual size_t IsDestroyed();
    virtual size_t CheckFireAI(TFireCheck *arg);
    virtual void MarkSectorsForView();
    virtual void StuckFree(update_msg *arg);
    virtual size_t FireMinigun(TMinigunShot *arg);
    virtual size_t UserTargeting(TAimQuery *arg);
    virtual void HandleVisChildrens(int *arg);
    virtual bool GetFightMotivation(float *arg);
    virtual void ReorganizeGroup(TReorgOrder *arg);
    virtual size_t TargetAssess(TTargetAssess *arg);
    virtual bool TestTargetSector(const NC_STACK_ypabact *) const { return true; };
    virtual void BeamingTimeUpdate(update_msg *arg);
    virtual void StartDestFXByType(uint8_t arg);
    virtual void CorrectPositionOnLand();
    virtual void CorrectPositionInLevelBox(void *);
    virtual void NetUpdate(update_msg *arg);
    virtual void NetPredictMotion(update_msg *arg);
    virtual void Release();
    virtual size_t SetStateInternal(setState_msg *arg);
    virtual void ChangeSectorEnergy(TSectorDamage *arg);
    virtual void DeadTimeUpdate(update_msg *arg);
    virtual void NetInterpolateMotion(update_msg *arg);
    virtual void NetExtrapolateMotion(update_msg *arg);
    virtual size_t PathFinder(TPathRequest *arg);
    virtual size_t SetPath(TPathRequest *arg);
    
    virtual bool IsHidden() const;
    virtual bool IsHiddenFor(uint8_t owner) const;

    NC_STACK_ypabact();
    virtual ~NC_STACK_ypabact();
    
    virtual const std::string ClassName() const {
        return __ClassName;
    };

    enum BACT_ATT
    {
        BACT_ATT_WORLD = 0x80001001,
        BACT_ATT_PTRANSFORM = 0x80001002,
        BACT_ATT_VIEWER = 0x80001004,       // bool
        BACT_ATT_INPUTTING = 0x80001005,    // bool
        BACT_ATT_EXACTCOLL = 0x80001006,    // bool
        BACT_ATT_BACTCOLL = 0x80001007,     // bool
        BACT_ATT_ATTACKLIST = 0x80001008,
        BACT_ATT_AIRCONST = 0x80001009,
        BACT_ATT_LANDINGONWAIT = 0x8000100A, // bool
        BACT_ATT_YOURLS = 0x8000100B,
        BACT_ATT_VISPROT = 0x8000100C,
        BACT_ATT_AGGRESSION = 0x8000100D,
        BACT_ATT_COLLNODES = 0x8000100E,
        //BACT_ATT_VPTRANSFORM = 0x8000100F,
        BACT_ATT_EXTRAVIEWER = 0x80001010,  // bool
        BACT_ATT_P_ATTACKNODE = 0x80001011,
        BACT_ATT_S_ATTACKNODE = 0x80001012,
        BACT_ATT_ALWAYSRENDER = 0x80001013  // bool
    };

    virtual void setBACT_viewer(bool);
    virtual void setBACT_inputting(bool);
    virtual void setBACT_exactCollisions(bool);
    virtual void setBACT_bactCollisions(bool);
    virtual void setBACT_airconst(int);
    virtual void setBACT_landingOnWait(bool);
    virtual void setBACT_yourLastSeconds(int);
    virtual void SetVP(NC_STACK_base *vp);
    virtual void setBACT_aggression(int);
    virtual void setBACT_extraViewer(bool);
    virtual void setBACT_alwaysRender(bool);

    NC_STACK_ypabact * GetEnemyCandidateInSector(const cellArea &cell, float *radius, char *job) const;

    virtual NC_STACK_ypaworld *getBACT_pWorld()
    { return _world; }
    
    virtual TF::TForm3D *getBACT_pTransform()
    { return &_tForm; }
    
    virtual bool getBACT_viewer() const 
    { return (_oflags & BACT_OFLAG_VIEWER) != 0; }
    
    virtual bool getBACT_inputting() const 
    { return (_oflags & BACT_OFLAG_USERINPT) != 0; }

    virtual bool getBACT_exactCollisions() const 
    { return (_oflags & BACT_OFLAG_EXACTCOLL) != 0; }
    
    virtual bool getBACT_bactCollisions() const 
    { return (_oflags & BACT_OFLAG_BACTCOLL) != 0; }
    
    virtual bool getBACT_landingOnWait() const 
    { return (_oflags & BACT_OFLAG_LANDONWAIT) != 0; }
    
    virtual int getBACT_yourLastSeconds() const 
    { return _yls_time; }
    
    virtual NC_STACK_base *GetVP()
    {
        if (_current_vp)
            return _current_vp->Bas;

        return NULL;
    }
    
    virtual int getBACT_aggression() const
    { return _aggr; }
    
    virtual World::TRoboCollSet *getBACT_collNodes()
    { return NULL; }
    
    virtual bool getBACT_extraViewer() const
    { return (_oflags & BACT_OFLAG_EXTRAVIEW) != 0; }    
    
    virtual bool getBACT_alwaysRender() const
    { return (_oflags & BACT_OFLAG_ALWAYSREND) != 0; }
    
    
    void ChangeEscapeFlag(bool escape);
    
    virtual bool IsGroundUnit() const { return false; };
    
    bool IsNeedsWaypoints() const;
    bool IsAnyKidWithoutSecondUnitTarget() const;
    
    void UpdateVisor(NC_STACK_ypabact *bact2, int noWeaponSight);
    void NetBroadcastReorder(NC_STACK_ypabact *bact2, int mode);
    void StartDestFX(const World::DestFX &fx);

    void DoTargetWaypoint();
    void FixSectorFall();
    void FixBeyondTheWorld();
    void CleanAttackersTarget();

    void DeleteAttacker(NC_STACK_ypabact *bact, int tgtType);
    void AddAttacker(NC_STACK_ypabact *bact, int tgtType);
    
    void CopyTargetOf(NC_STACK_ypabact *commander);
    
    bool IsParentMyRobo() const;
    
    void CopyWaypointsStuff(NC_STACK_ypabact *bact);
    
    World::RefBactList &GetKidList() { return _kidList; }
    

    // static methods for return correspond for reflist  kid ref node
    static World::RefBactList::Node& GetCellRefNode(NC_STACK_ypabact *&bact)
    {
        return bact->_cellRef;
    }
    
    static World::RefBactList::Node& GetKidRefNode(NC_STACK_ypabact *&bact)
    {
        return bact->_kidRef;
    }
 
protected:
    void SetKidsPath(int beginWp);

    //Data
public:
    static constexpr const char * __ClassName = "ypabact.class";
public:    
    
    World::RefBactList::Node _cellRef; 
    
    Common::Point _cellId;
    cellArea *_pSector = NULL;
    
    vec2d _wrldSize;
    
    Common::Point _wrldSectors;
 
    int _bact_type;
    uint32_t _gid = 0; // global bact id
    uint8_t _vehicleID; // vehicle id, from scr files
    uint8_t _bflags;
    uint32_t _commandID = 0;
    NC_STACK_yparobo *_host_station; // parent robo?
    NC_STACK_ypabact *_parent;
    World::RefBactList _kidList;
    World::RefBactList::Node _kidRef;
    TSndCarrier _soundcarrier;
    int _soundFlags;
    int _volume;
    int _pitch;
    float _pitch_max;
    int _energy;
    int _energy_max;
    int _reload_const;
//    int16_t field_3CE;
    uint8_t _shield;
//    char field_3D1;
    uint8_t _radar; // num sectors view
    uint8_t _owner;
    uint8_t _aggr;
    uint8_t _status;
//    uint64_t paddiong;
    int _status_flg; //Additional status flags
//    int field_3DA;
    uint8_t _primTtype;
    uint8_t _secndTtype;
    uint32_t _primT_cmdID;
    uint32_t _secndT_cmdID;
    BactTarget _primT;
    vec3d _primTpos;
    BactTarget _secndT;
    vec3d _sencdTpos;

    float _adist_sector;
    float _adist_bact;
    float _sdist_sector;
    float _sdist_bact;
    vec3d _waypoints[32]; //waypoints
    int16_t _current_waypoint;
    int16_t _waypoints_count;
    int _m_cmdID;
    uint8_t _m_owner;
    uint32_t _fe_cmdID; // found enemy group ID
    int _fe_time; //
    float _mass;
    float _force;
    float _airconst;
    float _airconst_static;
    float _maxrot;

    vec3d _viewer_position;
    mat3x3 _viewer_rotation;
    float _viewer_horiz_angle;
    float _viewer_vert_angle;
    float _viewer_max_up;
    float _viewer_max_down;
    float _viewer_max_side;

    float _thraction;
    vec3d _fly_dir;
    float _fly_dir_length;

    vec3d _position; //Current pos
    vec3d _old_pos; //Prev pos
    vec3d _target_vec; //Vector to target
    vec3d _target_dir; //Target 1-vector direction
    mat3x3 _rotation;

    float _height;
    float _height_max_user;
    vec3d _scale;
    NC_STACK_base *_vp_normal;
    NC_STACK_base *_vp_fire;
    NC_STACK_base *_vp_wait;
    NC_STACK_base *_vp_dead;
    NC_STACK_base *_vp_megadeth;
    NC_STACK_base *_vp_genesis;
    int _vp_active;
    extra_vproto _vp_extra[3];
    int _vp_extra_mode;
    std::vector<World::DestFX> _destroyFX;    // dest_fx
    std::vector<World::DestFX> _extDestroyFX; // ext_dest_fx
    float _radius;
    float _viewer_radius;
    float _overeof;
    float _viewer_overeof;
//    float pos_x_cntr;
//
//    float pos_y_cntr;

    TF::TForm3D _tForm;

    // _tForm at the start of the simulation step _prevStep, for drawing
    // between steps (NC_STACK_ypaworld::InterpolateUnitTransforms)
    vec3d _prevTFormPos;
    mat3x3 _prevTFormSclRot;
    uint32_t _prevStep = 0;

    int _clock;           // local time
    int _AI_time1;
    int _AI_time2;
//    int field_921;
//    int field_925;
    int _search_time1;
    int _search_time2;
    int _scale_time;
    int _brkfr_time;
    int _brkfr_time2;
//    int field_93D;
    int _newtarget_time;
    int _assess_time;
    int _waitCol_time; //Used in tank
    int _slider_time;
//    int field_951;
    int _dead_time;
    int _beam_time;
    int _energy_time;
    vec3d _mpos;
    int _weapon;
    uint8_t _weapon_flags;
    int _mgun;
    uint8_t _num_weapons;

    World::MissileList _missiles_list;
    int _weapon_time;
    vec3d _fire_pos;
    float _gun_angle;
    float _gun_angle_user;
    float _gun_leftright;
    float _gun_radius;
    float _gun_power;
    int _mgun_time;
    int _salve_counter;
    int _kill_after_shot;
    float _heading_speed;
    NC_STACK_ypabact *_killer;
    int16_t _killer_owner;
    int16_t _reb_count;
    int _atk_ret;
    uint32_t _lastFrmStamp;
    mat3x3 _netDRot;
    mat3x3 _netRotation;
    vec3d _netDSpeed;
    float _scale_start;
    float _scale_speed;
    float _scale_accel;
    int _scale_duration;
    int _scale_pos;
    int _scale_delay;
    NC_STACK_base *_vp_fx_models[32];

    int _oflags = 0;
    NC_STACK_base::Instance *_current_vp = NULL;
    std::list<TBactAttacker> _attackersList;
    int _yls_time;  
    
    bool _hidden = false;
    int8_t _unhideRadar = 0;
    
protected:
    NC_STACK_ypaworld *_world;
};

void ShrinkAndSpinPlasma(NC_STACK_ypabact *bact, int plasmaDuration, int frameTime, float maxScale);

#endif // YBACT_H_INCLUDED
