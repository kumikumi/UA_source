#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "yw.h"
#include "ypaflyer.h"
#include "log.h"
#include <math.h>
#include "world/state.h"

size_t NC_STACK_ypaflyer::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case FLY_ATT_TYPE:
                setFLY_type(val.Get<int32_t>());
                break;
            default:
                break;
            }
        }
    }

    _bact_type = BACT_TYPES_FLYER;

    return 1;
}

size_t NC_STACK_ypaflyer::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_ypaflyer::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case FLY_ATT_TYPE:
                setFLY_type(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypaflyer::PitchAndBank(float dtime, vec3d v, int isUser)
{
    float maxSpeed = _force / _airconst;

    if ( _fly_dir.x != 0.0 || _fly_dir.y != 0.0 || _fly_dir.z != 0.0 )
    {
        if ( v.x == 0.0 && v.y == 0.0 && v.z == 0.0 )
            v = _rotation.AxisZ();

        vec2d tmp = _rotation.AxisZ().XZ();

        float roll = clp_asin( _rotation.m01 );
        float pitch = clp_asin( _rotation.m21 );

        float targetPitch = 0.0;

        if ( _flyerType & 1 && _fly_dir_length >= 0.1 )
            targetPitch = clp_asin( _fly_dir.y ) * _fly_dir_length / maxSpeed;

        if ( targetPitch > 1.2 )
            targetPitch = 1.2;
        else if ( targetPitch < -0.8 )
            targetPitch = -0.8;

        float pitchRate;

        if ( pitch >= 0.0 )
            pitchRate = (pitch + 0.6) * 0.8;
        else
            pitchRate = (pitch + -0.6) * -0.8;

        float pitchDelta = -(targetPitch - pitch);

        if ( pitchDelta >= 0.0 )
        {
            float maxrot = _maxrot * dtime * pitchRate;

            if ( pitchDelta <= maxrot )
            {
                if ( pitchDelta < 0.0005 )
                    pitchDelta = 0.0;
            }
            else
            {
                pitchDelta = maxrot;
            }
        }
        else
        {
            float maxrot = -_maxrot * dtime * pitchRate;
            if ( pitchDelta >= maxrot )
            {
                if ( pitchDelta > -0.0005 )
                    pitchDelta = 0.0;
            }
            else
            {
                pitchDelta = maxrot;
            }
        }

        vec3d vaxis = _rotation.AxisX().X0Z();

        if ( vaxis.normalise() >= 0.001 )
            _rotation *= mat3x3::AxisAngle(vaxis, pitchDelta);

        if ( _flyerType & 2 )
        {
            if ( (tmp.x != 0.0 || tmp.y != 0.0) && (v.x != 0.0 || v.z != 0.0) )
            {
                float turnAngle = clp_acos( tmp.dot( v.XZ() ) / tmp.length() / v.XZ().length() );

                if ( turnAngle < 0.001 )
                    turnAngle = 0.0;

                if ( tmp.cross( v.XZ() ) < 0.0 )
                    turnAngle = -turnAngle;

                float targetRoll = 0.0;

                if ( _fly_dir_length >= 0.1 )
                {
                    if (isnormal(dtime)) // Not NULL, NAN, INF
                        targetRoll = fabs(_fly_dir_length) * _airconst / _force * (turnAngle * 1.45 / (dtime * _maxrot));
                    else
                        targetRoll = 0.0;
                }


                float rollRate = 1.0;
                if ( !isUser )
                {
                    if ( roll >= 0.0 )
                        rollRate = roll + 0.4;
                    else
                        rollRate = -(roll - 0.4);
                }

                float rollDelta = targetRoll - roll;

                if ( rollDelta >= 0.0 )
                {
                    float rollLimit = _maxrot * dtime * rollRate;

                    if ( rollDelta <= rollLimit )
                    {
                        if ( rollDelta < 0.001 )
                            rollDelta = 0.0;
                    }
                    else
                    {
                        rollDelta = rollLimit;
                    }
                }
                else
                {
                    float rollLimit = -_maxrot * dtime * rollRate;
                    if ( rollDelta >= rollLimit )
                    {
                        if ( rollDelta > -0.001 )
                            rollDelta = 0.0;
                    }
                    else
                    {
                        rollDelta = rollLimit;
                    }
                }

                _rotation = mat3x3::RotateZ(rollDelta) * _rotation;
            }
        }
        else
        {
            if ( roll >= 0.0 )
            {
                float rollLimit = _maxrot * dtime * pitchRate;

                if ( roll <= rollLimit )
                {
                    if ( roll < 0.001 )
                        roll = 0.0;
                }
                else
                {
                    roll = rollLimit;
                }
            }
            else
            {
                float rollLimit = -_maxrot * dtime * pitchRate;

                if ( roll >= rollLimit )
                {
                    if ( roll > -0.001 )
                        roll = 0.0;
                }
                else
                {
                    roll = rollLimit;
                }
            }

            _rotation = mat3x3::RotateZ(-roll) * _rotation;
        }
    }
}


void NC_STACK_ypaflyer::TurnToTargetAndSetLift(float angl)
{
    if ( _target_dir.x != 0.0 || _target_dir.z != 0.0 )
    {
        float turnAngle = clp_acos( _target_dir.XZ().dot( _rotation.AxisZ().XZ() ) / _target_dir.XZ().length() / _rotation.AxisZ().XZ().length() );

        float maxTurn = _maxrot * angl;

        if ( fabs(turnAngle) > maxTurn )
            turnAngle = maxTurn;

        if ( _target_dir.XZ().cross( _rotation.AxisZ().XZ() ) > 0.0 )
            turnAngle = -turnAngle;

        _rotation = mat3x3::RotateY(turnAngle) * _rotation;
    }

    _thraction = _force;

    float liftMass;

    if ( fabs(_target_dir.y) >= 0.1 )
    {
        if ( _target_dir.y > -0.1 )
            liftMass = _mass * 0.7;
        else
            liftMass = _mass * 2.5;
    }
    else
        liftMass = _mass;

    _flyerBoost = liftMass * 9.80665;

    if ( _status_flg & BACT_STFLAG_UPWRD )
        _flyerBoost = _mass * 2.5 * 9.80665;
}



void NC_STACK_ypaflyer::AI_layer3(update_msg *arg)
{
    float dtime = arg->frameTime / 1000.0;

    float targetDist = _target_vec.length();

    if ( targetDist > 0.0 )
        _target_dir = _target_vec / targetDist;

    bool isViewer = getBACT_viewer();
    int exactCollisions = getBACT_exactCollisions();
    int bactCollisions = getBACT_bactCollisions();

    int nearViewer = _world->IsNearViewer(this);

    float radius;

    if ( isViewer )
        radius = _viewer_radius;
    else
        radius = _radius;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    {
        vec3d oldDir = _rotation.AxisZ();

        if ( bactCollisions )
        {
            if ( nearViewer || (!_secndTtype && targetDist < World::CVSectorLength) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        if ( !_primTtype && !_secndTtype )
        {
            _status = BACT_STATUS_IDLE;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = 0;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
            break;
        }

        //printf(" %f %f %f\n", bact->field_639.x, bact->field_639.y, bact->field_639.z);

        TRayIntersection fwdRay;
        fwdRay.isect = 0;
        fwdRay.flags = 0;
        fwdRay.stPos = _old_pos;
        fwdRay.vect = (_position - _old_pos).X0Z();

        float moveDist = fwdRay.vect.XZ().length();

        if ( moveDist <= 0.0 )
            fwdRay.vect = _rotation.AxisZ() * 300.0;
        else
            fwdRay.vect *= 300.0 / moveDist;

        TRayIntersection rightRay;

        rightRay.isect = 0;
        rightRay.flags = 0;

        if ( isViewer || _status_flg & BACT_STFLAG_DODGE_RIGHT || (nearViewer && exactCollisions) )
        {
            rightRay.stPos = _old_pos;

            rightRay.vect.x = fwdRay.vect.x * 0.93969 - fwdRay.vect.z * 0.34202;
            rightRay.vect.y = fwdRay.vect.y;
            rightRay.vect.z = fwdRay.vect.z * 0.93969 + fwdRay.vect.x * 0.34202;

            _world->IntersectRay(&rightRay);
        }

        TRayIntersection leftRay;

        leftRay.isect = 0;
        leftRay.flags = 0;

        if ( isViewer || _status_flg & BACT_STFLAG_DODGE_LEFT || (nearViewer && exactCollisions) )
        {
            leftRay.stPos = _old_pos;
            leftRay.vect.x = fwdRay.vect.x * 0.93969 + fwdRay.vect.z * 0.34202;
            leftRay.vect.y = fwdRay.vect.y;
            leftRay.vect.z = fwdRay.vect.z * 0.93969 - fwdRay.vect.x * 0.34202;

            _world->IntersectRay(&leftRay);
        }

        if ( isViewer || !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) || (nearViewer && exactCollisions) )
            _world->IntersectRay(&fwdRay);

        int hitCount = 0;

        TCollisionNormal collNormal;
        collNormal.normal = 0;

        if ( fwdRay.isect )
        {
            if ( moveDist + radius > fwdRay.tVal * 300.0 )
            {
                collNormal.normal = fwdRay.skel->polygons[fwdRay.polyID].Normal();

                hitCount++;
            }
        }

        if ( rightRay.isect )
        {
            if ( moveDist + radius > rightRay.tVal * 300.0 )
            {
                collNormal.normal += rightRay.skel->polygons[rightRay.polyID].Normal();

                hitCount++;
            }
        }

        if ( leftRay.isect )
        {
            if ( moveDist + radius > leftRay.tVal * 300.0 )
            {
                collNormal.normal += leftRay.skel->polygons[leftRay.polyID].Normal();

                hitCount++;
            }
        }

        if ( hitCount )
        {
            collNormal.normal /= (float)hitCount;

            Recoil(&collNormal);
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_LCRASH;
        }

        if ( !fwdRay.isect && !rightRay.isect && !leftRay.isect )
        {
            _status_flg &= ~(BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT);
            _status_flg |= BACT_STFLAG_MOVE;
        }

        if ( !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) )
        {
            if ( rightRay.isect == 1 && leftRay.isect == 1 )
            {
                if ( rightRay.tVal >= leftRay.tVal )
                    _status_flg |= BACT_STFLAG_DODGE_LEFT;
                else
                    _status_flg |= BACT_STFLAG_DODGE_RIGHT;
            }

            if ( rightRay.isect == 1 && !leftRay.isect )
                _status_flg |= BACT_STFLAG_DODGE_RIGHT;

            if ( !rightRay.isect && leftRay.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;

            if ( !rightRay.isect && !leftRay.isect && fwdRay.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;
        }

        TRayIntersection terrainRay;

        terrainRay.stPos = _old_pos;

        float maxForce = _mass * 9.80665;

        if ( maxForce <= _force )
            maxForce = _force;

        float drag = _airconst;

        if ( drag < 10.0 )
            drag = 10.0;

        terrainRay.vect.x = (_fly_dir.x * 200.0 * _fly_dir_length) / (maxForce / drag);

        if ( terrainRay.vect.x < -200.0 )
            terrainRay.vect.x = -200.0;

        if ( terrainRay.vect.x > 200.0 )
            terrainRay.vect.x = 200.0;

        terrainRay.vect.y = _height;

        terrainRay.vect.z = (_fly_dir.z * 200.0 * _fly_dir_length) / (maxForce / drag);

        if ( terrainRay.vect.z < -200.0 )
            terrainRay.vect.z = -200.0;

        if ( terrainRay.vect.z > 200.0 )
            terrainRay.vect.z = 200.0;

        terrainRay.flags = 0;

        _world->IntersectRay(&terrainRay);

        if ( terrainRay.isect )
        {
            _target_dir.y = -(1.0 - terrainRay.tVal);
        }
        else
        {
            NC_STACK_ypabact *userVehicle = _world->getYW_userVehicle();

            if ( ( (_secndTtype != BACT_TGT_TYPE_UNIT || (_secndT.pbact->_bact_type != BACT_TYPES_ROBO && userVehicle != _secndT.pbact))
                    && (_primTtype != BACT_TGT_TYPE_UNIT || (_primT.pbact->_bact_type != BACT_TYPES_ROBO && userVehicle != _primT.pbact)) )
                    || _target_dir.y >= -0.01 )
            {
                if ( _target_dir.y < 0.15 )
                    _target_dir.y = 0.15;
            }
        }

        if ( _status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT) )
            _target_dir.y = -0.2;

        if ( terrainRay.isect && terrainRay.tVal * _height < _radius && _fly_dir.y > 0.0 )
        {
            collNormal.normal = terrainRay.skel->polygons[terrainRay.polyID].Normal();

            Recoil(&collNormal);
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_LCRASH;
        }

        if ( _target_dir.y != 0.0 )
            _target_dir.normalise();

        float fwdLen = fwdRay.vect.XZ().length();
        if (isnormal(fwdLen)) // Not NULL, NAN, INF
        {
            if ( _status_flg & BACT_STFLAG_DODGE_LEFT )
            {
                _target_dir.x = -fwdRay.vect.z / fwdLen;
                _target_dir.z = fwdRay.vect.x / fwdLen;
            }
            else if ( _status_flg & BACT_STFLAG_DODGE_RIGHT )
            {
                _target_dir.x = fwdRay.vect.z  / fwdLen;
                _target_dir.z = -fwdRay.vect.x  / fwdLen;
            }
        }
        else // emulate watcom div 0.0
        {
            if ( _status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT) )
            {
                _target_dir.x = 0.0;
                _target_dir.z = 0.0;
            }
        }

        TurnToTargetAndSetLift(dtime);

        move_msg moveMsg;
        moveMsg.flag = 0;
        moveMsg.period = dtime;

        Move(&moveMsg);

        PitchAndBank(dtime, oldDir, 0);


        TFightTarget fightMsg;
        fightMsg.fperiod = dtime;
        fightMsg.g_time = _clock;

        //printf("%d %d \n", bact->field_3DF, bact->field_3DE);

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        {
            fightMsg.target.pbact = _secndT.pbact;
            fightMsg.prio = 1;

            FightWithBact(&fightMsg);
        }
        else if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            fightMsg.pos = _sencdTpos;
            fightMsg.target.pcell = _secndT.pcell;
            fightMsg.prio = 1;

            FightWithSect(&fightMsg);
        }
        else if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            fightMsg.prio = 0;
            fightMsg.target.pbact = _primT.pbact;

            FightWithBact(&fightMsg);
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            fightMsg.pos = _primTpos;
            fightMsg.target.pcell = _primT.pcell;
            fightMsg.prio = 0;

            FightWithSect(&fightMsg);
        }
        else
        {
            _status_flg &= ~(BACT_STFLAG_FIGHT_P | BACT_STFLAG_FIGHT_S);

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = 0;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
        }

    }
    break;

    case BACT_STATUS_DEAD:
        DeadTimeUpdate(arg);
        break;

    case BACT_STATUS_IDLE:
    {

        if ( _clock - _newtarget_time > 500 )
        {
            TTargetAssess secAssess;
            secAssess.tgType = _secndTtype;
            secAssess.priority = 1;

            TTargetAssess primAssess;
            primAssess.tgType = _primTtype;
            primAssess.priority = 0;

            int secResult = TargetAssess(&secAssess);

            int primResult = TargetAssess(&primAssess);

            if ( secResult != TA_IGNORE || primResult != TA_IGNORE )
            {

                if ( secResult == TA_CANCEL )
                {
                    setTarget_msg targetMsg;
                    targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                    targetMsg.priority = 1;

                    SetTarget(&targetMsg);
                }

                if ( primResult == TA_CANCEL )
                {
                    setTarget_msg targetMsg;
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                    targetMsg.tgt_pos.x = _position.x;
                    targetMsg.tgt_pos.z = _position.z;
                    targetMsg.priority = 0;

                    SetTarget(&targetMsg);
                }

                if ( _primTtype || _secndTtype )
                {
                    setState_msg stateMsg;
                    stateMsg.unsetFlags = BACT_STFLAG_LAND;
                    stateMsg.setFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NORMAL;

                    SetState(&stateMsg);
                    break;
                }
            }
        }

        if ( getBACT_landingOnWait() )
        {
            _thraction = 0;
            _flyerBoost = 0;

            if ( _status_flg & BACT_STFLAG_LAND )
            {
                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_IDLE;

                SetState(&stateMsg);

                TRayIntersection groundRay;
                groundRay.stPos = _position;
                groundRay.vect = vec3d::OY(_overeof + 50.0);
                groundRay.flags = 0;

                _world->IntersectRay(&groundRay);

                if ( groundRay.isect )
                    _position.y = groundRay.isectPos.y - _overeof;
            }
            else
            {
                TCrashLandStep crashStep;
                crashStep.flags = 0;
                crashStep.frameTime = arg->frameTime;

                CrashOrLand(&crashStep);
            }
        }
    }
    break;

    case BACT_STATUS_CREATE:
        CreationTimeUpdate(arg);
        break;

    case BACT_STATUS_BEAM:
        BeamingTimeUpdate(arg);
        break;
    }
}


void NC_STACK_ypaflyer::User_layer(update_msg *arg)
{
    _airconst = _airconst_static;

    float dtime = (float)arg->frameTime / 1000.0;

    int bactCollisions = getBACT_bactCollisions();

    if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
    {
        vec3d oldDir;
        oldDir = _rotation.AxisZ();

        float yawAngle = -arg->inpt->Sliders[0] * _maxrot * dtime;

        if ( bactCollisions )
        {
            if ( !(_status_flg & BACT_STFLAG_LAND) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        float minFlySpeed;

        if ( _status_flg & BACT_STFLAG_LAND )
            minFlySpeed = 0.1;
        else
            minFlySpeed = 1.0;

        if ( _fly_dir_length >= minFlySpeed )
        {
            if ( !(_status_flg & BACT_STFLAG_FIRE) )
            {
                setState_msg stateMsg;

                stateMsg.newStatus = BACT_STATUS_NORMAL;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);
            }

            _status_flg &= ~BACT_STFLAG_LAND;
        }
        else
        {
            TRayIntersection groundRay;

            groundRay.stPos = _position;
            groundRay.vect.z = 0;
            groundRay.vect.x = 0;

            float probeLen;

            if ( _viewer_overeof <= _viewer_radius )
                probeLen = _viewer_radius;
            else
                probeLen = _viewer_overeof;

            groundRay.flags = 0;
            groundRay.vect.y = probeLen * 1.5;

            _world->IntersectRay(&groundRay);

            if ( groundRay.isect && _thraction < _force * 0.001 && _flyerBoost <= _mass * 9.80665 )
            {
                _status_flg |= BACT_STFLAG_LAND;
                _fly_dir_length = 0;
                _thraction = 0;

                _flyerBoost = _mass * 9.80665;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_LAND;
            }

            float tmp = 0.0;

            if ( _primTtype == BACT_TGT_TYPE_CELL)
                tmp = (_primTpos.XZ() - _position.XZ()).length();

            if ( _primTtype != BACT_TGT_TYPE_CELL || tmp <= 800.0 )
            {
                if ( _status_flg & BACT_STFLAG_LAND )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        setState_msg stateMsg;
                        stateMsg.newStatus = BACT_STATUS_IDLE;
                        stateMsg.unsetFlags = 0;
                        stateMsg.setFlags = 0;

                        SetState(&stateMsg);
                    }
                }

                _status = BACT_STATUS_NORMAL;
            }
            else
            {
                _status = BACT_STATUS_IDLE;

                if ( _status_flg & BACT_STFLAG_LAND )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        setState_msg stateMsg;
                        stateMsg.newStatus = BACT_STATUS_IDLE;
                        stateMsg.unsetFlags = 0;
                        stateMsg.setFlags = 0;

                        SetState(&stateMsg);
                    }
                }
            }
        }

        _rotation *= mat3x3::RotateY(yawAngle);

        _thraction += _force * (dtime * 0.3) * arg->inpt->Sliders[2];

        if ( _thraction > _force )
            _thraction = _force;

        if ( _thraction < 0.0 )
            _thraction = 0;


        _flyerBoost = (fabs(_fly_dir_length) / 111.0 + 1.0) * (arg->inpt->Sliders[1] * 20000.0) * 0.5 + _mass * 9.80665;

        float altitude = _pSector->height - _position.y;

        float invMaxHeight = 1.0 / _height_max_user;

        float maxBoost = _mass * 7.0 * 9.80665  * ( 1.0 - ( POW2(altitude) * POW2(invMaxHeight) ) );

        if ( _flyerBoost > maxBoost )
            _flyerBoost = maxBoost;

        TMissileLaunch launch;
        launch.tgType = BACT_TGT_TYPE_DRCT;
        launch.tgt_pos = _rotation.AxisZ();

        TAimQuery aimQuery;
        aimQuery.flags = 5;
        aimQuery.aim_dir = _rotation.AxisZ();

        if ( UserTargeting(&aimQuery) )
        {
            launch.tgType = BACT_TGT_TYPE_UNIT;
            launch.target.pbact = aimQuery.ret_bact;
        }

        if ( arg->inpt->Buttons.IsAny({0, 1}) )
        {
            launch.direction = vec3d(0.0, 0.0, 0.0);
            launch.weapon = _weapon;
            launch.g_time = _clock;

            if ( _clock % 2 )
                launch.start_point.x = _fire_pos.x;
            else
                launch.start_point.x = -_fire_pos.x;

            launch.start_point.y = _fire_pos.y;
            launch.start_point.z = _fire_pos.z;
            launch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0) | 2;

            LaunchMissile(&launch);
        }

        if ( _mgun != -1 )
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                if ( !arg->inpt->Buttons.Is(2) )
                {
                    setState_msg stateMsg;
                    stateMsg.setFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }
            }

            if ( arg->inpt->Buttons.Is(2))
            {
                if ( !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg stateMsg;
                    stateMsg.unsetFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.setFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }

                TMinigunShot mgShot;

                mgShot.dir = _rotation.AxisZ();
                mgShot.period = dtime;
                mgShot.gTime = _clock;

                FireMinigun(&mgShot);
            }
        }

        if ( arg->inpt->Buttons.Is(3) )
            HandBrake(arg);

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            move_msg moveMsg;
            moveMsg.period = dtime;
            moveMsg.flag = 0;

            Move(&moveMsg);
        }
        else
        {
            vec3d tmp(0.0, 0.0, 0.0);

            TSphereContact contacts[10];

            for (int i = 3; i >= 0; i--)
            {
                int collided = 0;

                move_msg moveMsg;
                moveMsg.period = dtime;
                moveMsg.flag = 0;

                Move(&moveMsg);

                TSphereCollisionQuery sphereQuery;

                sphereQuery.pos = _position + _rotation.AxisZ() * 0.5 * _viewer_radius;
                sphereQuery.dir = _fly_dir;
                sphereQuery.radius = _viewer_radius;
                sphereQuery.coll_max = 10;
                sphereQuery.flags = 0;
                sphereQuery.collisions = contacts;

                _world->FindSphereCollisions(&sphereQuery);

                if ( sphereQuery.coll_count )
                {
                    tmp = vec3d(0.0, 0.0, 0.0);

                    for (int j = sphereQuery.coll_count - 1; j >= 0; j--)
                        tmp += sphereQuery.collisions[ j ].normal;

                    TCollisionNormal collNormal;
                    if ( tmp.normalise() != 0.0 )
                        collNormal.normal = tmp;
                    else
                        collNormal.normal = _fly_dir;

                    Recoil(&collNormal);

                    _flyerBoost = _mass * 9.80665;

                    collided = 1;
                }

                if ( !collided )
                {
                    TRayIntersection moveRay;
                    moveRay.stPos = _old_pos;
                    moveRay.vect = _position - _old_pos;
                    moveRay.flags = 0;

                    _world->IntersectLongRay(&moveRay);

                    if ( moveRay.isect )
                    {
                        TCollisionNormal collNormal;
                        collNormal.normal = moveRay.skel->polygons[ moveRay.polyID ].Normal();

                        Recoil(&collNormal);

                        collided = 1;
                    }
                }

                if ( !collided )
                {
                    _status_flg &= ~BACT_STFLAG_LCRASH;
                    break;
                }

                if ( !_soundcarrier.Sounds[5].IsEnabled() )
                {
                    if ( !(_status_flg & BACT_STFLAG_LCRASH) )
                    {
                        _status_flg |= BACT_STFLAG_LCRASH;

                        SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                        TFFeedbackEffect ffEffect;

                        ffEffect.effects_type = 5;
                        ffEffect.strength = 1.0;
                        ffEffect.pos_x = 10.0 * tmp.x + _position.x;
                        ffEffect.pos_z = 10.0 * tmp.z + _position.z;

                        _world->FFeedback_Play(&ffEffect);
                    }
                }

                _fly_dir.y -= 0.2;
            }

            PitchAndBank(dtime, oldDir, 1);
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
        DeadTimeUpdate(arg);
}

void NC_STACK_ypaflyer::Move(move_msg *arg)
{
    _old_pos = _position;

    float weight;

    if ( _status == BACT_STATUS_DEAD )
        weight = _mass * 39.2266;
    else
        weight = _mass * 9.80665;

    vec3d tmp(0.0, 0.0, 0.0);
    float thrust = 0.0;

    if ( !(arg->flag & 1) )
    {
        tmp = _rotation.AxisZ().X0Z();
        thrust = _thraction;
    }

    float lift = 0.0;

    if ( !(arg->flag & 1) )
        lift = -1.0 * _flyerBoost;

    float drag = _fly_dir_length * _airconst;

    vec3d tmp2 = vec3d::OY(weight) + tmp * thrust - _fly_dir * drag + vec3d::OY(lift);

    float forceLen = tmp2.length();

    if ( forceLen > 0.0 )
    {
        //vec3d tmp3 = bact->fly_dir * bact->fly_dir_length + tmp2 * (v33 / bact->mass * arg->field_0) / v33; //(arg->field_0 / bact->mass);
        vec3d tmp3 = _fly_dir * _fly_dir_length + tmp2 * (arg->period / _mass);

        _fly_dir_length = tmp3.length();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = tmp3 / _fly_dir_length;
    }

    if ( _fly_dir_length > 1.5 )
        _position += _fly_dir * _fly_dir_length * arg->period * 6.0;

    CorrectPositionInLevelBox(NULL);

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    float speedRatio = fabs(_fly_dir_length) / (_force / _airconst_static);
    float pitchMax;

    if ( _pitch_max <= -0.8 )
        pitchMax = 1.2;
    else
        pitchMax = _pitch_max;

    float pitchShift = speedRatio * pitchMax;

    if ( pitchShift > pitchMax )
        pitchShift = pitchMax;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch += (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * pitchShift;
}

size_t NC_STACK_ypaflyer::SetPosition(TUnitPlacement *arg)
{
    if ( !NC_STACK_ypabact::SetPosition(arg) )
        return 0;

    _flyerBoost = _mass * 9.80665;
    return 1;
}

void NC_STACK_ypaflyer::Renew()
{
    NC_STACK_ypabact::Renew();

    _flyerType = 0;
    _flyerBoost = 0;
}

void NC_STACK_ypaflyer::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("flyerBoost", _flyerBoost);
    v.Field("flyerType", _flyerType);
}

void NC_STACK_ypaflyer::HandBrake(update_msg *arg)
{
    NC_STACK_ypabact::HandBrake(arg);

    _thraction = 0;
    _flyerBoost = _mass * 9.80665;
}


void NC_STACK_ypaflyer::setFLY_type(int tp)
{
    _flyerType = tp;
}

int NC_STACK_ypaflyer::getFLY_type()
{
    return _flyerType;
}


