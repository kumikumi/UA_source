#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "yw.h"
#include "ypamissile.h"
#include "yparobo.h"
#include "yw_net.h"

#include "log.h"

#include <math.h>
#include "world/state.h"

size_t NC_STACK_ypamissile::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_MISSLE;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_VIEWER:
                setBACT_viewer(val.Get<bool>());
                break;

            case MISS_ATT_LAUNCHER:
                SetLauncherBact(val.Get<NC_STACK_ypabact *>());
                break;

            case MISS_ATT_TYPE:
                SetMissileType(val.Get<int32_t>());
                break;

            case MISS_ATT_LIFETIME:
                SetLifeTime(val.Get<int32_t>());
                break;

            case MISS_ATT_DELAY:
                SetDelay(val.Get<int32_t>());
                break;

            case MISS_ATT_DRIVETIME:
                SetDriveTime(val.Get<int32_t>());
                break;

            case MISS_ATT_IGNOREBUILDS:
                SetIgnoreBuilds ( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypamissile::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_ypamissile::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_VIEWER:
                setBACT_viewer(val.Get<bool>());
                break;

            case MISS_ATT_LAUNCHER:
                SetLauncherBact(val.Get<NC_STACK_ypabact *>());
                break;

            case MISS_ATT_TYPE:
                SetMissileType(val.Get<int32_t>());
                break;

            case MISS_ATT_LIFETIME:
                SetLifeTime(val.Get<int32_t>());
                break;

            case MISS_ATT_DELAY:
                SetDelay(val.Get<int32_t>());
                break;

            case MISS_ATT_DRIVETIME:
                SetDriveTime(val.Get<int32_t>());
                break;

            case MISS_ATT_IGNOREBUILDS:
                SetIgnoreBuilds ( val.Get<int32_t>() );
                break;

            case MISS_ATT_POW_HELI:
                SetPowerHeli(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_TANK:
                SetPowerTank(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_FLYER:
                SetPowerFlyer(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_ROBO:
                SetPowerRobo(val.Get<int32_t>());
                break;

            case MISS_ATT_RAD_HELI:
                SetRadiusHeli(val.Get<float>());
                break;

            case MISS_ATT_RAD_TANK:
                SetRadiusTank(val.Get<float>());
                break;

            case MISS_ATT_RAD_FLYER:
                SetRadiusFlyer(val.Get<float>());
                break;

            case MISS_ATT_RAD_ROBO:
                SetRadiusRobo(val.Get<float>());
                break;

            case MISS_ATT_STHEIGHT:
                SetStartHeight(val.Get<float>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypamissile::AI_layer1(update_msg *arg)
{
    if ( _status == BACT_STATUS_DEAD )
        _yls_time -= arg->frameTime;


    if ( _primTtype )
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
            _target_vec = _primT.pbact->_position - _position;
        else
            _target_vec = _primTpos - _position;
    }

    AI_layer2(arg);
}

void NC_STACK_ypamissile::AI_layer2(update_msg *arg)
{
    AI_layer3(arg);
}

bool NC_STACK_ypamissile::TubeCollisionTest()
{
    vec3d collisionSumPosition(0.0, 0.0, 0.0);
    int collisionCount = 0;
    float collisionSumRadius = 0.0;

    bool canHitOwn = _mislEmitter->getBACT_inputting();

    if ( !canHitOwn )
        canHitOwn = getBACT_viewer();

    TSectorInfo sectInfo;
    sectInfo.pos_x = _old_pos.x;
    sectInfo.pos_z = _old_pos.z;
    _world->GetSectorInfo(&sectInfo);

    cellArea *pCells[3];

    pCells[0] = sectInfo.pcell;

    sectInfo.pos_x = _position.x;
    sectInfo.pos_z = _position.z;
    _world->GetSectorInfo(&sectInfo);

    pCells[2] = sectInfo.pcell;

    if ( sectInfo.pcell == pCells[0] )
    {
        pCells[1] = pCells[0];
    }
    else
    {
        sectInfo.pos_x = (_position.x - _old_pos.x) * 0.5 + _old_pos.x;
        sectInfo.pos_z = (_position.z - _old_pos.z) * 0.5 + _old_pos.z;
        _world->GetSectorInfo(&sectInfo);

        pCells[1] = sectInfo.pcell;
    }

    for (int i = 0; i < 3; i++)
    {
        if ( i == 0 || pCells[i] != pCells[i - 1] )
        {
            if (pCells[i] == NULL)
            {
                ypa_log_out("TubeCollisionTest NULL sector i = %d, 621: %f %f 62D: %f %f \n", i, _position.x, _position.z, _old_pos.x, _old_pos.z);
                continue;
            }

            for ( NC_STACK_ypabact* &bct : pCells[i]->unitsList )
            {
                if ( bct == this || bct == _mislEmitter || bct->_bact_type == BACT_TYPES_MISSLE || bct->_status == BACT_STATUS_DEAD )
                    continue;

                if (bct->_bact_type == BACT_TYPES_GUN && bct->_shield >= 100)
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

                    if ( gun->IsRoboGun() )
                        continue;
                }

                if ( !canHitOwn && bct->_owner == _mislEmitter->_owner )
                {
                    continue;
                }

                if (_mislEmitter->_bact_type == BACT_TYPES_GUN)
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( _mislEmitter );

                    if (bct->_owner == _owner)
                    {
                        if (gun->IsRoboGun())
                        {
                            if (bct->_bact_type == BACT_TYPES_ROBO)
                                continue;

                            if (bct->_bact_type == BACT_TYPES_GUN )
                            {
                                NC_STACK_ypagun *bgun = dynamic_cast<NC_STACK_ypagun *>( bct );

                                if (bgun->IsRoboGun())
                                    continue;
                            }
                        }
                    }
                }

                if ( _mislType == MISL_BOMB && bct->_position.y < _mislStartHeight )
                    continue;

                World::TRoboCollSet *collNodes = bct->getBACT_collNodes();

                int collCount;
                if ( collNodes )
                    collCount = collNodes->roboColls.size();
                else
                    collCount = 1;

                for (int j = collCount - 1; j >= 0; j--)
                {
                    float radius;
                    vec3d ttmp;

                    if ( collNodes )
                    {
                        World::TRoboColl *coll = &collNodes->roboColls[j];
                        radius = coll->robo_coll_radius;

                        ttmp = bct->_position + bct->_rotation.Transpose().Transform(coll->coll_pos);
                    }
                    else
                    {
                        ttmp = bct->_position;
                        radius = bct->_radius;
                    }

                    if ( !collNodes || radius >= 0.01 )
                    {
                        vec3d to_enemy = ttmp - _old_pos;
                        vec3d dist_vect = _position - _old_pos;

                        if ( to_enemy.dot( _rotation.AxisZ() )>= 0.3 )
                        {
                            float dist_vect_len = dist_vect.normalise();

                            vec3d vp = dist_vect * to_enemy;

                            float wpn_radius = 0.0;

                            switch ( bct->_bact_type )
                            {
                            case BACT_TYPES_BACT:
                                wpn_radius = _mislRadiusHeli;
                                break;

                            case BACT_TYPES_TANK:
                            case BACT_TYPES_CAR:
                                wpn_radius = _mislRadiusTank;
                                break;

                            case BACT_TYPES_FLYER:
                            case BACT_TYPES_UFO:
                                wpn_radius = _mislRadiusFlyer;
                                break;

                            case BACT_TYPES_ROBO:
                                wpn_radius = _mislRadiusRobo;
                                break;

                            default:
                                wpn_radius = _radius;
                                break;
                            }

                            if ( wpn_radius == 0.0)
                                wpn_radius = _radius;

                            float vp_len = vp.length();
                            float to_enemy_len = to_enemy.length();

                            if ( radius + wpn_radius > vp_len )
                            {
                                /*  Tube collision test, not cylinder!
                                    Will hit only when distance ~ wpn_radius */
                                if ( sqrt( POW2(dist_vect_len) + POW2(vp_len) ) > fabs(to_enemy_len - wpn_radius) )
                                {
                                    NC_STACK_ypabact *userStation = _world->getYW_userHostStation();

                                    collisionSumRadius += radius;
                                    collisionCount++;
                                    collisionSumPosition += bct->_position;

                                    bct->_status_flg &= ~BACT_STFLAG_LAND;

                                    

                                    int targetIsUser = bct->getBACT_inputting();

                                    int damage = 0;

                                    switch ( bct->_bact_type )
                                    {
                                    case BACT_TYPES_BACT:
                                        damage = _energy * _mislEnergyHeli;
                                        break;

                                    case BACT_TYPES_TANK:
                                    case BACT_TYPES_CAR:
                                        damage = _energy * _mislEnergyTank;
                                        break;

                                    case BACT_TYPES_FLYER:
                                    case BACT_TYPES_UFO:
                                        damage = _energy * _mislEnergyFlyer;
                                        break;

                                    case BACT_TYPES_ROBO:
                                        damage = _energy * _mislEnergyRobo;
                                        break;

                                    default:
                                        damage = _energy;
                                        break;
                                    }

                                    float rawDamage;
                                    float damageDiv;

                                    if ( targetIsUser || bct->getBACT_viewer() )
                                    {
                                        rawDamage = damage * (100 - bct->_shield);
                                        damageDiv = 250;
                                    }
                                    else
                                    {
                                        rawDamage = damage * (100 - bct->_shield);
                                        damageDiv = 100.0;
                                    }

                                    damage = ceil(rawDamage / damageDiv); //Missile damage (ceil for less damage)
                                    if ( damage )
                                    {
                                        TEnergyChange energyMsg;
                                        energyMsg.energy = -damage;
                                        energyMsg.unit = _mislEmitter;

                                        if ( userStation->_owner == _owner || !_world->_isNetGame )
                                            bct->ModifyEnergy(&energyMsg);
                                    }

                                    break;
                                }
                            }
                        }
                    }
                }

            }
        }
    }

    if ( collisionCount > 0 )
    {
        // Set new position between collided objects
        _position = collisionSumPosition / (float)collisionCount;

        collisionSumRadius /= (float)collisionCount;

        if ( collisionSumRadius >= 50.0 )
        {
            vec3d posDelta = _position - _old_pos;
            float deltaLen = posDelta.length();

            if ( deltaLen < 1.0 )
                deltaLen = 1.0;

            _position -= (posDelta / deltaLen) * collisionSumRadius;
        }
    }

    return collisionCount > 0;
}

vec3d NC_STACK_ypamissile::CalcForceVector()
{
    _thraction = _force;

    return vec3d::Normalise(  _fly_dir * _fly_dir_length * _airconst
                            + _target_dir * _thraction
                            - vec3d(0.0, _mass * 9.80665, 0.0));
}

void NC_STACK_ypamissile::AI_layer3(update_msg *arg)
{
    _world->IsNearViewer(this);

    float targetDist = _target_vec.length();

    if ( targetDist > 0.1 )
    {
        if ( _primTtype != BACT_TGT_TYPE_DRCT )
            _target_dir = _target_vec / targetDist;
    }

    _AI_time1 = 0;

    _thraction = _force;

    float fTime = arg->frameTime * 0.001;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _mislFlags & FLAG_MISL_COUNTDELAY)
            _mislDelayTime -= arg->frameTime;

        if ( (_mislFlags & FLAG_MISL_COUNTDELAY)  &&  _mislDelayTime <= 0 )
        {
            Impact();

            _status = BACT_STATUS_DEAD;

            setState_msg stateMsg;
            stateMsg.setFlags = BACT_STFLAG_DEATH2;
            stateMsg.unsetFlags = 0;
            stateMsg.newStatus = BACT_STATUS_NOPE;

            SetState(&stateMsg);

            if ( !(_mislFlags & FLAG_MISL_IGNOREBUILDS) || _pSector->PurposeType == cellArea::PT_NONE )
            {
                if ( _world->_userRobo->_owner == _owner || !_world->_isNetGame )
                {
                    TSectorDamage sectorDamage;

                    sectorDamage.pos.x = _fly_dir.x * 5.0 + _position.x;
                    sectorDamage.pos.z = _fly_dir.z * 5.0 + _position.z;
                    sectorDamage.energy = _energy;
                    sectorDamage.unit = _mislEmitter;

                    ChangeSectorEnergy(&sectorDamage);
                }
            }
        }
        else
        {
            move_msg moveMsg;

            switch ( _mislType )
            {
            case MISL_BOMB:
                moveMsg.period = fTime;
                moveMsg.flag = 1;
                Move(&moveMsg);
                break;

            case MISL_DIRECT:
                moveMsg.period = fTime;
                moveMsg.flag = 0;
                moveMsg.thrustDir = CalcForceVector();
                Move(&moveMsg);
                break;

            case MISL_TARGETED:
                moveMsg.period = fTime;
                moveMsg.flag = 0;
                moveMsg.thrustDir = CalcForceVector();
                Move(&moveMsg);
                break;

            case MISL_GRENADE:
                moveMsg.period = fTime;
                moveMsg.thrustDir = _fly_dir;
                moveMsg.flag = 0;

                Move(&moveMsg);
                break;

            default:
                break;
            }

            if ( TubeCollisionTest() )
            {
                setState_msg stateMsg;
                Impact();

                stateMsg.newStatus = BACT_STATUS_DEAD;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);

                ResetViewing();
                return;
            }
            
            if ( _mislType == MISL_INTERNAL )
                return;
            
            TRayIntersection ray;
            ray.stPos = _old_pos;
            ray.vect = _position - _old_pos;
            ray.flags = 0;

            _world->IntersectRay(&ray);

            if ( ray.isect )
            {
                AlignMissileByNormal( ray.skel->polygons[ ray.polyID ].Normal() );

                _position = ray.isectPos;

                ResetViewing();

                _mislType = MISL_INTERNAL;
                _mislFlags |= FLAG_MISL_COUNTDELAY;

                if ( !_mislDelayTime )
                {
                    Impact();

                    _status = BACT_STATUS_DEAD;

                    setState_msg stateMsg;
                    stateMsg.setFlags = BACT_STFLAG_DEATH2;
                    stateMsg.unsetFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;

                    SetState(&stateMsg);

                    if ( !(_mislFlags & FLAG_MISL_IGNOREBUILDS) || _pSector->PurposeType == cellArea::PT_NONE )
                    {
                        if ( _world->_userRobo->_owner == _owner || !_world->_isNetGame )
                        {
                            TSectorDamage sectorDamage;

                            sectorDamage.pos.x = _fly_dir.x * 5.0 + _position.x;
                            sectorDamage.pos.z = _fly_dir.z * 5.0 + _position.z;
                            sectorDamage.energy = _energy;
                            sectorDamage.unit = _mislEmitter;

                            ChangeSectorEnergy(&sectorDamage);
                        }
                    }
                }

                int emitterIsUser = _mislEmitter->getBACT_inputting();

                if ( emitterIsUser )
                {
                    if ( _mislEmitter->IsParentMyRobo() )
                    {
                        setTarget_msg targetMsg;
                        targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                        targetMsg.tgt_pos = _position;
                        targetMsg.priority = 0;

                        _mislEmitter->SetTarget(&targetMsg);
                    }
                }
            }
            else
            {
                _mislDriveTime -= arg->frameTime;

                if ( _mislDriveTime < 0 )
                {
                    _mislType = MISL_BOMB;

                    _airconst = 10.0;
                    _airconst_static = 10.0;
                }

                _mislLifeTime -= arg->frameTime;

                if ( _mislLifeTime >= 0 )
                {
                    AlignMissile( arg->frameTime * 0.001 );
                }
                else
                {
                    Impact();

                    setState_msg stateMsg;
                    stateMsg.unsetFlags = 0;
                    stateMsg.setFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_DEAD;

                    SetState(&stateMsg);

                    ResetViewing();
                }
            }
        }
    }
}

void NC_STACK_ypamissile::User_layer(update_msg *arg)
{
    _old_pos = _position;

    if (_status == BACT_STATUS_NORMAL)
        AI_layer1(arg);
    else
        ResetViewing();
}

void NC_STACK_ypamissile::Move(move_msg *arg)
{
    _old_pos = _position;

    float gravity;

    if ( _status != BACT_STATUS_DEAD && _mislType != MISL_BOMB )
        gravity = _mass * 9.80665;
    else
        gravity = _mass * 39.2266;

    vec3d thrust(0.0, 0.0, 0.0);

    if ( !(arg->flag & 1) )
        thrust = arg->thrustDir * _thraction;

    vec3d vec1 = vec3d(0.0, gravity, 0.0) + thrust - _fly_dir * (_fly_dir_length * _airconst);

    float forceMag = vec1.normalise();

    if ( forceMag > 0.0 )
    {
        vec3d velocity = _fly_dir * _fly_dir_length + vec1 * (forceMag / _mass * arg->period);

        float speed = velocity.length();

        if ( speed > 0.0 )
            velocity /= speed;

        _fly_dir = velocity;

        _fly_dir_length = speed;
    }

    _position += _fly_dir * (_fly_dir_length * arg->period * 6.0);

    CorrectPositionInLevelBox(NULL);
}

void NC_STACK_ypamissile::SetState(setState_msg *arg)
{
    SetStateInternal(arg);
}

void NC_STACK_ypamissile::Renew()
{
    NC_STACK_ypabact::Renew();

    _mislType = MISL_BOMB;
    _mislEmitter = NULL;
    _mislLifeTime = 5000;
    _mislDriveTime = 0;
    _mislDelayTime = 0;
    _mislFlags = 0;
    _mislStartHeight = 0.0;
    _mislEnergyHeli = 0.0;
    _mislEnergyTank = 0.0;
    _mislEnergyFlyer = 0.0;
    _mislEnergyRobo = 0.0;
    _mislRadiusHeli = 0.0;
    _mislRadiusTank = 0.0;
    _mislRadiusFlyer = 0.0;
    _mislRadiusRobo = 0.0;

    setBACT_yourLastSeconds(3000);
}

void NC_STACK_ypamissile::VisitView(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitView(v);

    v.Unit("emitter", _mislEmitter, World::State::VIEW);
}

void NC_STACK_ypamissile::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("missileType", _mislType);
    v.Field("lifeTime", _mislLifeTime);
    v.Field("driveTime", _mislDriveTime);
    v.Field("delayTime", _mislDelayTime);
    v.Field("missileFlags", _mislFlags);
    v.Field("startHeight", _mislStartHeight);
    v.Field("energyHeli", _mislEnergyHeli);
    v.Field("energyTank", _mislEnergyTank);
    v.Field("energyFlyer", _mislEnergyFlyer);
    v.Field("energyRobo", _mislEnergyRobo);
    v.Field("radiusHeli", _mislRadiusHeli);
    v.Field("radiusTank", _mislRadiusTank);
    v.Field("radiusFlyer", _mislRadiusFlyer);
    v.Field("radiusRobo", _mislRadiusRobo);
}

size_t NC_STACK_ypamissile::SetStateInternal(setState_msg *arg)
{
    SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
    SFXEngine::SFXe.stopSound(&_soundcarrier, 0);
    SFXEngine::SFXe.stopSound(&_soundcarrier, 1);

    if ( arg->newStatus )
        _status = arg->newStatus;

    if ( arg->setFlags )
        _status_flg |= arg->setFlags;

    if ( arg->unsetFlags )
        _status_flg &= ~arg->unsetFlags;

    if ( arg->newStatus == BACT_STATUS_DEAD )
    {
        SetVP(_vp_dead);

        SFXEngine::SFXe.startSound(&_soundcarrier, 2);

        StartDestFXByType(World::DestFX::FX_DEATH);

        _fly_dir_length = 0;
    }

    if ( arg->newStatus == BACT_STATUS_NORMAL )
    {
        SetVP(_vp_normal);

        SFXEngine::SFXe.startSound(&_soundcarrier, 0);
    }

    if ( arg->unsetFlags == BACT_STFLAG_DEATH2 )
    {
        SetVP(_vp_normal);

        SFXEngine::SFXe.startSound(&_soundcarrier, 0);
    }

    if ( arg->setFlags == BACT_STFLAG_DEATH2 )
    {
        _status = BACT_STATUS_DEAD;

        SetVP(_vp_megadeth);

        SFXEngine::SFXe.startSound(&_soundcarrier, 2);

        StartDestFXByType(World::DestFX::FX_MEGADETH);

        _fly_dir_length = 0;
    }

    return 1;
}

void NC_STACK_ypamissile::ResetViewing()
{
    if ( getBACT_viewer() )
    {
        setBACT_viewer(false);
        setBACT_inputting(false);

        if ( _mislEmitter->_status != BACT_STATUS_DEAD || _mislEmitter->_parent == NULL )
        {
            _mislEmitter->setBACT_viewer(true);
            _mislEmitter->setBACT_inputting(true);
        }
        else
        {
            _mislEmitter->_parent->setBACT_viewer(true);
            _mislEmitter->_parent->setBACT_inputting(true);
        }

    }
}

void NC_STACK_ypamissile::Impact()
{
    TImpulse impulse;
    impulse.energ = _energy;
    impulse.pos = _position;
    impulse.dir = _fly_dir;
    impulse.force = _fly_dir_length;
    impulse.mass = _mass;

    float momentum = _fly_dir_length * _mass;

    if ( momentum > _world->_maxImpulse && _world->_maxImpulse > 0.0 )
    {
        float scale = _world->_maxImpulse / momentum;
        impulse.force *= scale;
        impulse.mass *= scale;
    }
    
    /* FIXME:
       Needs to check all near sectors too if effective radius affect it*/

    for( NC_STACK_ypabact* &bct : _pSector->unitsList )
    {
        if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_ROBO && bct->_bact_type != BACT_TYPES_TANK && bct->_bact_type != BACT_TYPES_CAR && bct->_bact_type != BACT_TYPES_GUN && bct->_bact_type != BACT_TYPES_HOVER && !(bct->_status_flg & BACT_STFLAG_DEATH2) )
        {
            int canApply = 1;

            if ( _world->_isNetGame )
            {
                if ( _owner != bct->_owner )
                    canApply = 0;
            }

            if ( canApply )
                bct->ApplyImpulse(&impulse);
        }
    }

    if ( _world->_isNetGame )
    {
        uamessage_impulse impMsg;
        impMsg.msgID = UAMSG_IMPULSE;
        impMsg.owner = _owner;
        impMsg.id = _gid;
        impMsg.pos = _position;
        impMsg.impulse = _energy;
        impMsg.dir = _fly_dir;
        impMsg.dir_len = _fly_dir_length;
        impMsg.mass = _mass;

        _world->NetBroadcastMessage(&impMsg, sizeof(impMsg), true);
    }
}

void NC_STACK_ypamissile::AlignMissile(float dtime)
{
    if ( _fly_dir != vec3d(0.0, 0.0, 0.0) )
    {
        vec3d dir = _rotation.AxisZ(); // Get Z-axis, as dir
        vec3d u = vec3d::Normalise(dir * _fly_dir); // vector cross product

        // If length == 0 - no rotation
        if ( u.length() > 0.0 )
        {
            //scalar cross product
            float rotAngle = clp_acos( dir.dot(_fly_dir) );

            if ( _mislType == MISL_BOMB )
            {
                if ( dtime != 0.0 )
                {
                    float mxrot = _maxrot * dtime;

                    if ( rotAngle < -mxrot )
                        rotAngle = -mxrot;

                    if ( rotAngle > mxrot )
                        rotAngle = mxrot;
                }
            }

            if ( fabs(rotAngle) > BOMB_MIN_ANGLE )
                _rotation *= mat3x3::AxisAngle(u, rotAngle);
        }

        // Fix camera Z-axis rotation
        if ( _mislFlags & FLAG_MISL_VIEW )
        {
            float ZAngle = clp_acos( _rotation.AxisX().XZ().length() ); // Get degree of current Z-axis rotation

            if ( _rotation.m11 < 0.0 )
                ZAngle = C_PI - ZAngle;

            if ( _rotation.m01 < 0.0 )
                ZAngle = -ZAngle;

            _rotation = mat3x3::RotateZ(-ZAngle) * _rotation;
        }
    }
}

void NC_STACK_ypamissile::AlignMissileByNormal(const vec3d &normal)
{
    vec3d UpVector = _rotation.AxisY();

    vec3d vaxis = UpVector * normal;

    if ( vaxis.normalise() != 0.0 )
    {
        float angle = clp_acos( UpVector.dot(normal) );

        if ( fabs(angle) > BACT_MIN_ANGLE )
            _rotation *= mat3x3::AxisAngle(vaxis, angle);
    }
}


NC_STACK_ypamissile::NC_STACK_ypamissile() 
{
    _mislType = 0;
    _mislEmitter = NULL;
    _mislLifeTime = 0;
    _mislDriveTime = 0;
    _mislDelayTime = 0;
    _mislFlags = 0;
    _mislStartHeight = 0.;
    _mislEnergyHeli = 0.;
    _mislEnergyTank = 0.;
    _mislEnergyFlyer = 0.;
    _mislEnergyRobo = 0.;
    _mislRadiusHeli = 0.;
    _mislRadiusTank = 0.;
    _mislRadiusFlyer = 0.;
    _mislRadiusRobo = 0.;
}


void NC_STACK_ypamissile::setBACT_viewer(bool vwr)
{
    NC_STACK_ypabact::setBACT_viewer(vwr);

    if ( vwr )
        _mislFlags |= FLAG_MISL_VIEW;
    else
        _mislFlags &= ~FLAG_MISL_VIEW;
}

void NC_STACK_ypamissile::SetLauncherBact(NC_STACK_ypabact *bact)
{
    _mislEmitter = bact;
}

void NC_STACK_ypamissile::SetMissileType(int tp)
{
    _mislType = tp;
}

void NC_STACK_ypamissile::SetLifeTime(int time)
{
    _mislLifeTime = time;
}

void NC_STACK_ypamissile::SetDelay(int delay)
{
    _mislDelayTime = delay;
}

void NC_STACK_ypamissile::SetDriveTime(int time)
{
    _mislDriveTime = time;
}

void NC_STACK_ypamissile::SetIgnoreBuilds(int ign)
{
    if ( ign )
        _mislFlags |= FLAG_MISL_IGNOREBUILDS;
    else
        _mislFlags &= ~FLAG_MISL_IGNOREBUILDS;
}

void NC_STACK_ypamissile::SetPowerHeli(int po)
{
    _mislEnergyHeli = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerTank(int po)
{
    _mislEnergyTank = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerFlyer(int po)
{
    _mislEnergyFlyer = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerRobo(int po)
{
    _mislEnergyRobo = po * 0.001;
}

void NC_STACK_ypamissile::SetRadiusHeli(float rad)
{
    _mislRadiusHeli = rad;
}

void NC_STACK_ypamissile::SetRadiusTank(float rad)
{
    _mislRadiusTank = rad;
}

void NC_STACK_ypamissile::SetRadiusFlyer(float rad)
{
    _mislRadiusFlyer = rad;
}

void NC_STACK_ypamissile::SetRadiusRobo(float rad)
{
    _mislRadiusRobo = rad;
}

void NC_STACK_ypamissile::SetStartHeight(float posy)
{
    _mislStartHeight = posy;
}



NC_STACK_ypabact *NC_STACK_ypamissile::GetLauncherBact()
{
    return _mislEmitter;
}

int NC_STACK_ypamissile::GetMissileType()
{
    return _mislType;
}

int NC_STACK_ypamissile::GetLifeTime()
{
    return _mislLifeTime;
}

int NC_STACK_ypamissile::GetDelay()
{
    return _mislDelayTime;
}

int NC_STACK_ypamissile::GetDriveTime()
{
    return _mislDriveTime;
}

int NC_STACK_ypamissile::GetIgnoreBuilds()
{
    return (_mislFlags & FLAG_MISL_IGNOREBUILDS) != 0;
}

int NC_STACK_ypamissile::GetPowerHeli()
{
    return _mislEnergyHeli * 1000.0;
}

int NC_STACK_ypamissile::GetPowerTank()
{
    return _mislEnergyTank * 1000.0;
}

int NC_STACK_ypamissile::GetPowerFlyer()
{
    return _mislEnergyFlyer * 1000.0;
}

int NC_STACK_ypamissile::GetPowerRobo()
{
    return _mislEnergyRobo * 1000.0;
}

float NC_STACK_ypamissile::GetRadiusHeli()
{
    return _mislRadiusHeli;
}

float NC_STACK_ypamissile::GetRadiusTank()
{
    return _mislRadiusTank;
}

float NC_STACK_ypamissile::GetRadiusFlyer()
{
    return _mislRadiusFlyer;
}

float NC_STACK_ypamissile::GetRadiusRobo()
{
    return _mislRadiusRobo;
}

float NC_STACK_ypamissile::GetStartHeight()
{
    return _mislStartHeight;
}


