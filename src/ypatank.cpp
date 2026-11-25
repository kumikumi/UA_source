#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "yw.h"
#include "ypatank.h"
#include "yparobo.h"
#include "log.h"

#include "yw_net.h"
#include "world/state.h"

NC_STACK_ypatank::NC_STACK_ypatank()
{
    _tankFlags = 0;
    _tankWaitCount = 0;
    _tankCollisionAngle = 0.;
    _tankCollisionWay = 0.;
    _tankCollisionFlag = 0;
    _tankExpectTgt = false;
}

size_t NC_STACK_ypatank::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_TANK;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case TANK_ATT_TIP:
                setTANK_tip( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypatank::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}


size_t NC_STACK_ypatank::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case TANK_ATT_TIP:
                setTANK_tip( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypatank::AI_layer3(update_msg *arg)
{
    float dtime = arg->frameTime / 1000.0;

    int bactCollisions = getBACT_bactCollisions();

    float probeLen = _radius + 300.0;

    int nearViewer = _world->IsNearViewer(this);

    float targetDist = _target_vec.length();

    if ( targetDist > 0.0 )
        _target_dir = _target_vec / targetDist;

    bool nearTarget = !_secndTtype && targetDist < World::CVSectorLength;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    {
        if ( !(_status_flg & BACT_STFLAG_LAND) )
        {
            TCrashLandStep crashLand;
            crashLand.flags = 1;
            crashLand.frameTime = arg->frameTime;

            CrashOrLand(&crashLand);
            break;
        }

        if ( !_primTtype && !_secndTtype )
        {
            _status = BACT_STATUS_IDLE;
            _status_flg &= ~(BACT_STFLAG_FIGHT_P | BACT_STFLAG_FIGHT_S);

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

        if ( targetDist == 0.0 )
            break;

        if ( _waitCol_time > 0 )
        {
            _waitCol_time -= arg->frameTime;
        }
        else
        {
            float tmpsq = _rotation.AxisZ().XZ().length();
            float cosAngle = 0.0;
            
            if (isnormal(tmpsq))  // Not NULL, NAN, INF
                cosAngle = _target_dir.XZ().dot( _rotation.AxisZ().XZ() ) / tmpsq;

            tmpsq = _target_dir.XZ().length();
            if (isnormal(tmpsq))  // Not NULL, NAN, INF
                cosAngle /= tmpsq;
            else
                cosAngle = 0.0;

            float turnAngle = clp_acos(cosAngle);

            if ( _tankCollisionFlag & (COLL_WALL_L | COLL_WALL_R | COLL_HILL_L | COLL_HILL_R) )
            {
                if ( _tankCollisionVector.XZ().dot( _target_dir.XZ() ) < 0.0
                        && _tankCollisionAngle == 0.0
                        && _tankCollisionWay == 0.0 )
                {
                    _tankCollisionWay = 0;
                    _tankCollisionAngle = 0;
                    _tankCollisionFlag &= ~(COLL_WALL_L | COLL_WALL_R | COLL_HILL_L | COLL_HILL_R);
                    break;
                }

                if ( (_tankCollisionFlag & (COLL_HILL_L | COLL_HILL_R)) && 
                     _tankCollisionAngle == 0.0 && 
                     _tankCollisionWay == 0.0 )
                {
                    TRayIntersection hillRay;

                    if ( _tankCollisionFlag & COLL_HILL_L )
                    {
                        hillRay.stPos = _position - _rotation.AxisX() * (2.0 * _radius);
                        hillRay.vect = (_rotation.AxisX() * 4.0 + _rotation.AxisY() * 3.0) * _radius;
                    }
                    else
                    {
                        hillRay.stPos = _position + _rotation.AxisX() * (2.0 * _radius);
                        hillRay.vect = (_rotation.AxisX() * -4.0 + _rotation.AxisY() * 3.0) * _radius;
                    }
                    hillRay.flags = 1;

                    _world->IntersectRay(&hillRay);

                    if ( !hillRay.isect || hillRay.skel->polygons[ hillRay.polyID ].B > 0.6 )
                        _tankCollisionFlag &= ~(COLL_HILL_L | COLL_HILL_R);

                    if ( !(_tankCollisionFlag & (COLL_HILL_L | COLL_HILL_R)) )
                        _tankCollisionWay = 80.0;
                }

                if ( _tankCollisionFlag & (COLL_WALL_L | COLL_WALL_R) )
                {
                    if ( _tankCollisionAngle == 0.0 && _tankCollisionWay == 0.0 )
                    {
                        TRayIntersection wallRay;

                        if ( _tankCollisionFlag & COLL_WALL_L )
                            wallRay.vect = -_rotation.AxisX() * 150.0;
                        else
                            wallRay.vect = _rotation.AxisX() * 150.0;

                        wallRay.stPos = _position;
                        wallRay.flags = 1;

                        _world->IntersectRay(&wallRay);

                        if ( !wallRay.isect || wallRay.skel->polygons[ wallRay.polyID ].B > 0.6 )
                            _tankCollisionFlag &= ~(COLL_WALL_L | COLL_WALL_R);

                        if ( !(_tankCollisionFlag & (COLL_WALL_L | COLL_WALL_R)) )
                            _tankCollisionWay = 80.0;
                    }
                }

                if ( _tankCollisionAngle <= 0.001 )
                {
                    _tankCollisionAngle = 0;
                }
                else
                {
                    float rotStep = _maxrot * dtime;

                    if ( rotStep <= _tankCollisionAngle )
                    {
                        _tankCollisionAngle -= rotStep;
                    }
                    else
                    {
                        rotStep = _tankCollisionAngle;
                        _tankCollisionAngle = 0;
                    }

                    if ( _tankCollisionFlag & (COLL_WALL_L | COLL_HILL_L) )
                        _rotation = mat3x3::RotateY(-rotStep) * _rotation;
                    else
                        _rotation = mat3x3::RotateY(rotStep) * _rotation;
                }

                if ( _tankCollisionAngle == 0.0 )
                    _status_flg |= BACT_STFLAG_MOVE;
            }
            else
            {
                _status_flg |= BACT_STFLAG_MOVE;

                if ( turnAngle > 1.1 && _tankCollisionWay <= 0.001 )
                    _thraction *= 0.1 / (dtime + 0.1);

                if ( _rotation.AxisZ().XZ().cross( _target_dir.XZ() ) < 0.0 )
                    turnAngle = -turnAngle;

                if ( turnAngle >= 0.0 )
                {
                    float maxTurn = _maxrot * dtime;

                    if ( turnAngle <= maxTurn )
                    {
                        if ( turnAngle < 0.05 )
                            turnAngle = 0.0;
                    }
                    else
                    {
                        turnAngle = maxTurn;
                    }
                }
                else
                {
                    float minTurn = -_maxrot * dtime;

                    if ( turnAngle >= minTurn )
                    {
                        if ( turnAngle > -0.05 )
                            turnAngle = 0.0;
                    }
                    else
                    {
                        turnAngle = minTurn;
                    }
                }

                if ( _tankCollisionWay <= 0.0001 )
                    _rotation = mat3x3::RotateY(turnAngle) * _rotation;
            }

            if ( _status_flg & BACT_STFLAG_MOVE )
            {
                _thraction += _force * dtime * 0.8;

                if ( _thraction > _force )
                    _thraction = _force;

                if ( !(_tankCollisionFlag & (COLL_WALL_L | COLL_WALL_R | COLL_HILL_L | COLL_HILL_R)) && 
                      _tankCollisionWay > 0.0 )
                    _thraction -= _force * dtime * 0.6;

                if ( _thraction < 0.0 )
                    _thraction = 0;

                if ( !(_status_flg & BACT_STFLAG_ATTACK) || !_tankExpectTgt || _tankCollisionFlag )
                {
                    move_msg moveMsg;
                    moveMsg.flag = 0;
                    moveMsg.period = dtime;

                    Move(&moveMsg);
                }

                if ( _tankCollisionWay > 0.0 )
                {
                    _tankCollisionWay -= ( _position.XZ() - _old_pos.XZ() ).length();

                    if ( _tankCollisionWay <= 0.0 )
                        _tankCollisionWay = 0;
                }
            }
            else
            {
                _old_pos = _position;
                _thraction = 0;
            }

            TRayIntersection ray;
            ray.stPos = _old_pos;
            ray.vect = _position - _old_pos;

            float moveLen = ray.vect.length();

            if ( moveLen > 5.0 && _status_flg & BACT_STFLAG_MOVE )
                ray.vect *= probeLen / moveLen;
            else
                ray.vect = _rotation.AxisZ() * probeLen;

            ray.flags = 1;

            _world->IntersectRay(&ray);

            if ( ray.isect )
            {
                vec3d wallNormal = ray.skel->polygons[ ray.polyID ].Normal();

                if ( wallNormal.y < 0.6 && _tankCollisionAngle == 0.0)
                {
                    // TODO(cleanup): fabs(0.0) < 0.7 is always true. It is in the first decompile too, so
                    // probably a term the compiler folded away.
                    if ( ray.tVal * probeLen > (_radius + 50.0) && fabs(turnAngle) < 1.0 && fabs(0.0) < 0.7 && targetDist >= 840.0 )
                    {
                        if ( !_secndTtype )
                        {
                            float avoidTurn = (_fly_dir_length * 0.1) * arg->frameTime / 1000.0;

                            if ( _rotation.AxisZ().XZ().cross( wallNormal.XZ() ) >= 0.0 )
                                _rotation = mat3x3::RotateY(-avoidTurn) * _rotation;
                            else
                                _rotation = mat3x3::RotateY(avoidTurn) * _rotation;

                            _tankCollisionWay = ray.tVal * probeLen + 10.0;
                        }
                    }

                    if ( ray.tVal * probeLen <= (_radius + 50.0) )
                    {
                        _status_flg &= ~BACT_STFLAG_MOVE;
                        _position = _old_pos;

                        TRayIntersection groundRay;
                        groundRay.stPos = _position;
                        groundRay.vect = _rotation.AxisY() * 300.0;
                        groundRay.flags = 1;

                        _world->IntersectRay(&groundRay);

                        vec3d edgeDir;

                        if ( groundRay.isect )
                            edgeDir = wallNormal * groundRay.skel->polygons[ groundRay.polyID ].Normal();
                        else
                            edgeDir = vec3d(-wallNormal.z, 0.0, wallNormal.x);

                        if ( edgeDir == vec3d(0.0, 0.0, 0.0) )
                            edgeDir = _rotation.AxisX();

                        _tankCollisionVector = _rotation.AxisY() * edgeDir;

                        if ( _rotation.AxisZ().dot(_tankCollisionVector) < 0.0 )
                            _tankCollisionVector = -_tankCollisionVector;

                        float tmpLn = _tankCollisionVector.XZ().length();

                        if (isnormal(tmpLn))  // Not NULL, NAN, INF
                            tmpLn = 1.0 / tmpLn;
                        else
                            tmpLn = 0.0;

                        TRayIntersection rightRay;
                        TRayIntersection leftRay;

                        rightRay.vect.x = _tankCollisionVector.z * 150.0 * tmpLn;
                        rightRay.vect.y = 0;
                        rightRay.vect.z = -_tankCollisionVector.x * 150.0 * tmpLn;
                        rightRay.stPos = _old_pos;
                        rightRay.flags = 1;

                        leftRay.vect.x = -_tankCollisionVector.z * 150.0 * tmpLn;
                        leftRay.vect.y = 0;
                        leftRay.vect.z = _tankCollisionVector.x * 150.0 * tmpLn;
                        leftRay.stPos = _old_pos;
                        leftRay.flags = 1;

                        _world->IntersectRay(&rightRay);
                        _world->IntersectRay(&leftRay);

                        bool rightBlocked = rightRay.isect && rightRay.skel->polygons[ rightRay.polyID ].B < 0.6;
                        bool leftBlocked = leftRay.isect && leftRay.skel->polygons[ leftRay.polyID ].B < 0.6;

                        bool wallLeft = false;

                        vec2d az2d = _rotation.AxisZ().XZ();

                        if ( (leftBlocked && !rightBlocked) || (rightBlocked && !leftBlocked) )
                        {
                            if ( leftBlocked )
                            {
                                float dotLen = 0.0;
                                tmpLn = az2d.length();
                                
                                if (isnormal(tmpLn))  // Not NULL, NAN, INF
                                    dotLen = rightRay.vect.XZ().dot( az2d ) / tmpLn / 150.0;
                                
                                _tankCollisionAngle = clp_acos(dotLen);
                                wallLeft = true;
                            }
                            else
                            {
                                float dotLen = 0.0;
                                tmpLn = az2d.length();
                                
                                if (isnormal(tmpLn))  // Not NULL, NAN, INF
                                    dotLen = az2d.dot( leftRay.vect.XZ() ) / tmpLn / 150.0;
                                
                                _tankCollisionAngle = clp_acos(dotLen);
                                wallLeft = false;
                            }
                        }
                        else
                        {
                            float dotLen = 0.0; 
                            tmpLn = az2d.length();
                            
                            if (isnormal(tmpLn))  // Not NULL, NAN, INF
                                dotLen = az2d.dot( _tankCollisionVector.XZ() ) / tmpLn;
                            
                            tmpLn = _tankCollisionVector.XZ().length();
                            
                            if (isnormal(tmpLn))  // Not NULL, NAN, INF
                                dotLen = dotLen / tmpLn;
                            else
                                dotLen = 0.0;

                            _tankCollisionAngle = C_PI_2 - clp_acos(dotLen) + 0.01;

                            if ( az2d.cross( _tankCollisionVector.XZ() ) <= 0.0 )
                                wallLeft = false;
                            else
                                wallLeft = true;
                        }

                        if ( (_tankCollisionFlag & COLL_WALL_L) && !wallLeft )
                        {
                            _tankCollisionAngle = C_PI - _tankCollisionAngle;
                        }
                        else if ( (_tankCollisionFlag & COLL_WALL_R) && wallLeft )
                        {
                            _tankCollisionAngle = C_PI - _tankCollisionAngle;
                        }
                        else if ( wallLeft )
                        {
                            _tankCollisionFlag |= COLL_WALL_L;
                        }
                        else
                        {
                            _tankCollisionFlag |= COLL_WALL_R;
                        }

                        _tankCollisionWay = 100.0;
                    }
                }
            }

            vec3d alignNorm;
            int alignRes = AlignVehicleAI(dtime, &alignNorm);
            if ( alignRes != ALIGN_DONE )
            {
                _fly_dir_length = 0;
                _thraction = 0;

                ray.isect = 0;

                if ( alignRes == ALIGN_NONE )
                {
                    ray.stPos = _position + _rotation.AxisZ() * _radius;

                    float groundProbeLen = _radius * 8.0;
                    ray.vect = (_rotation.AxisY() - _rotation.AxisZ()) * groundProbeLen;
                    ray.flags = 1;

                    _world->IntersectRay(&ray);

                    if ( !ray.isect || ( ray.isect && ray.tVal * groundProbeLen * 0.7 > (_overeof + 30.0) ) )
                    {
                        _status_flg &= ~BACT_STFLAG_LAND;
                        break;
                    }
                }

                _position = _old_pos;

                if ( (ray.isect && ray.skel->polygons[ ray.polyID ].B < 0.6) || alignRes == ALIGN_NORMAL )
                {
                    _status_flg &= ~BACT_STFLAG_MOVE;
                    _tankCollisionFlag &= ~(COLL_WALL_L | COLL_WALL_R);

                    vec3d slopeNormal(0.0, 0.0, 0.0);

                    if ( ray.isect )
                        slopeNormal = -ray.skel->polygons[ ray.polyID ].Normal();

                    TRayIntersection groundRay;

                    groundRay.stPos = _position;
                    groundRay.vect = _rotation.AxisY() * 300.0;
                    groundRay.flags = 1;

                    _world->IntersectRay(&groundRay);

                    if ( alignRes == ALIGN_NORMAL )
                        slopeNormal = -alignNorm;

                    vec3d edgeDir;

                    if ( groundRay.isect )
                        edgeDir = slopeNormal * groundRay.skel->polygons[groundRay.polyID].Normal();
                    else
                        edgeDir = vec3d(-slopeNormal.z, 0.0, slopeNormal.x);

                    _tankCollisionVector = _rotation.AxisY() * edgeDir;

                    if ( _tankCollisionVector.dot( _rotation.AxisZ() ) < 0.0 )
                        _tankCollisionVector = -_tankCollisionVector;

                    vec2d axsZ = _rotation.AxisZ().XZ();

                    float ln = axsZ.length();
                    float dotLen = 0.0;
                    
                    if (isnormal(ln))  // Not NULL, NAN, INF
                        dotLen = axsZ.dot( _tankCollisionVector.XZ() ) / ln;
                    else
                        dotLen = 0.0;

                    ln = _tankCollisionVector.XZ().length();

                    if (isnormal(ln))  // Not NULL, NAN, INF
                        dotLen /= ln;
                    else
                        dotLen = 0.0;

                    _tankCollisionAngle = C_PI_2 - clp_acos(dotLen);

                    if ( _tankCollisionAngle < 0.1 )
                        _tankCollisionAngle = 0.1;

                    bool collHillLeft = axsZ.cross( _tankCollisionVector.XZ() ) > 0.0;

                    if ( (_tankCollisionFlag & COLL_HILL_L) && !collHillLeft )
                    {
                        _tankCollisionAngle = C_PI - _tankCollisionAngle;
                    }
                    else if ( (_tankCollisionFlag & COLL_HILL_R) && collHillLeft )
                    {
                        _tankCollisionAngle = C_PI - _tankCollisionAngle;
                    }
                    else if ( collHillLeft )
                    {
                        _tankCollisionFlag |= COLL_HILL_L;
                    }
                    else
                    {
                        _tankCollisionFlag |= COLL_HILL_R;
                    }

                    _tankCollisionWay = 100.0;
                }
            }

            if ( bactCollisions && ( nearViewer || nearTarget ) )
            {
                if ( CollisionWithBact(arg->frameTime) )
                    break;
            }
        }

        TFightTarget fight;
        fight.fperiod = dtime;
        fight.g_time = _clock;

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        {
            fight.prio = 1;
            fight.target.pbact = _secndT.pbact;

            FightWithBact(&fight);
        }
        else if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            fight.pos = _sencdTpos;
            fight.target.pcell = _secndT.pcell;
            fight.prio = 1;

            FightWithSect(&fight);
        }
        else if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            fight.target.pbact = _primT.pbact;
            fight.prio = 0;

            FightWithBact(&fight);
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            fight.pos = _primTpos;
            fight.target.pcell = _primT.pcell;
            fight.prio = 0;

            FightWithSect(&fight);
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

    case BACT_STATUS_IDLE:
    {
        if ( _clock - _newtarget_time > 500 )
        {
            TTargetAssess secAssess;
            TTargetAssess primAssess;

            secAssess.tgType = _secndTtype;
            secAssess.priority = 1;

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

                if ( primResult == TA_CANCEL)
                {
                    setTarget_msg targetMsg;
                    targetMsg.priority = 0;
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                    targetMsg.tgt_pos.x = _position.x;
                    targetMsg.tgt_pos.z = _position.z;

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

        setState_msg stateMsg;
        stateMsg.unsetFlags = 0;
        stateMsg.setFlags = 0;
        stateMsg.newStatus = BACT_STATUS_IDLE;

        SetState(&stateMsg);

        _thraction = 0;

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            _fly_dir_length = 0;

            if ( _tankWaitCount )
            {
                if ( _status_flg & BACT_STFLAG_DODGE_LEFT )
                    _rotation = mat3x3::RotateY(dtime * 0.33333334) * _rotation;
                else if ( _status_flg & BACT_STFLAG_DODGE_RIGHT )
                    _rotation = mat3x3::RotateY(-dtime * 0.33333334) * _rotation;

                _tankWaitCount--;
            }
            else
            {
                _status_flg &= ~(BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT);
                _tankWaitCount = arg->frameTime / 5;

                if ( _tankFlags & FLAG_TANK_ROTWAIT )
                {
                    // Now and then a turn on the spot, picked by the unit's
                    // id: its address, as the original took, differs from
                    // run to run
                    switch( (_gid + _clock) % 7 )
                    {
                        case 0:
                            _status_flg |= BACT_STFLAG_DODGE_LEFT;
                            break;
                        case 1:
                            _status_flg |= BACT_STFLAG_DODGE_RIGHT;
                            break;
                        default:
                            break;
                    }
                }
            }
            if ( AlignVehicleAI(dtime, NULL) != ALIGN_DONE )
                _status_flg &= ~BACT_STFLAG_LAND;
        }
        else
        {
            TCrashLandStep crashLand;
            crashLand.flags = 1;
            crashLand.frameTime = arg->frameTime;

            CrashOrLand(&crashLand);
        }
    }
    break;

    case BACT_STATUS_DEAD:
        DeadTimeUpdate(arg);
        break;

    case BACT_STATUS_CREATE:
        CreationTimeUpdate(arg);
        break;

    case BACT_STATUS_BEAM:
        BeamingTimeUpdate(arg);
        break;
    }
}

void NC_STACK_ypatank::User_layer(update_msg *arg)
{
    _airconst = _airconst_static;

    int bactCollisions = getBACT_bactCollisions();

    float dtime = arg->frameTime * 0.001;
    vec3d oldDir = _rotation.AxisZ();

    if (_status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE)
    {
        if ( _fly_dir_length != 0.0 )
        {
            if ( !(_status_flg & BACT_STFLAG_FIRE) )
            {
                setState_msg stateMsg;
                stateMsg.newStatus = BACT_STATUS_NORMAL;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);
            }
        }
        else
        {

            if ( _primTtype != BACT_TGT_TYPE_CELL || (_primTpos.XZ() - _position.XZ()).length() <= 800.0 )
            {
                if ( !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg stateMsg;
                    stateMsg.newStatus = BACT_STATUS_IDLE;
                    stateMsg.unsetFlags = 0;
                    stateMsg.setFlags = 0;

                    SetState(&stateMsg);
                }

                _status = BACT_STATUS_NORMAL;
            }
            else
            {
                if ( !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg stateMsg;
                    stateMsg.newStatus = BACT_STATUS_IDLE;
                    stateMsg.unsetFlags = 0;
                    stateMsg.setFlags = 0;

                    SetState(&stateMsg);
                }
                else
                    _status = BACT_STATUS_IDLE;
            }
        }

        float turnAngle = -arg->inpt->Sliders[3] * _maxrot * dtime * 2.0;

        if ( fabs(turnAngle) > 0.0 )
            _rotation = mat3x3::RotateY(turnAngle) * _rotation;

        if ( (_fly_dir_length < 0.0 && _thraction > 0.0) || (_fly_dir_length > 0.0 && _thraction < 0.0) )
        {
            if ( fabs(_fly_dir.y) > 0.1 )
            {
                float damping = 1.0 - dtime * 4.0;

                if ( damping < 0.1 )
                    damping = 0.1;

                _fly_dir_length *= damping;
            }
        }

        float throttle = arg->inpt->Sliders[4];
        float throttleAbs = fabs(throttle);

        if ( throttle > 1.0 )
            throttle = 1.0;
        else if ( throttle < -1.0 )
            throttle = -1.0;

        _thraction += _force * (dtime * 0.75) * throttle;

        float maxThrust;

        if ( arg->inpt->Buttons.Is(31) )
            maxThrust = _force * throttleAbs;
        else
            maxThrust = _force;

        if ( _thraction > maxThrust )
            _thraction = maxThrust;
        if ( _thraction < -maxThrust )
            _thraction = -maxThrust;

        if ( fabs(throttle) > 0.001 )
            _status_flg |= BACT_STFLAG_MOVE;

        _gun_angle_user += dtime * arg->inpt->Sliders[5];

        if ( _gun_angle_user < -0.3 )
            _gun_angle_user = -0.3;

        if ( _gun_angle_user > 0.8 )
            _gun_angle_user = 0.8;


        _gun_leftright = _gun_leftright - dtime * arg->inpt->Sliders[15];

        if ( _gun_leftright < -0.8 )
            _gun_leftright = -0.8;

        if ( _gun_leftright > 0.8 )
            _gun_leftright = 0.8;

        vec3d aimDir;
        aimDir = _rotation.AxisZ();

        float corW, corH;
        GFX::Engine.getAspectCorrection(corW, corH, true);

        aimDir -= _rotation.AxisY() * (_gun_angle_user * corH);
        aimDir -= _rotation.AxisX() * (_gun_leftright * corW);

        float aimLen = aimDir.length();

        if ( aimLen > 0.0001 )
            aimDir /= aimLen;

        TMissileLaunch launch;
        TAimQuery aimQuery;

        launch.tgt_pos = aimDir;
        launch.tgType = BACT_TGT_TYPE_DRCT;

        aimQuery.aim_dir = aimDir;
        aimQuery.flags = 5;

        if ( UserTargeting(&aimQuery) )
        {
            launch.target.pbact = aimQuery.ret_bact;
            launch.tgType = BACT_TGT_TYPE_UNIT;
        }

        if ( arg->inpt->Buttons.IsAny({0, 1}) )
        {
            launch.direction = aimDir;
            launch.weapon = _weapon;
            launch.g_time = _clock;

            if ( _clock % 2 )
                launch.start_point.x = -_fire_pos.x;
            else
                launch.start_point.x = _fire_pos.x;

            launch.start_point.y = _fire_pos.y;
            launch.start_point.z = _fire_pos.z;
            launch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0);
            launch.flags |= 2;

            LaunchMissile(&launch);
        }

        if ( _mgun != -1 )
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                if ( !(arg->inpt->Buttons.Is(2)) )
                {
                    setState_msg stateMsg;
                    stateMsg.setFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }
            }

            if ( arg->inpt->Buttons.Is(2) )
            {
                if ( !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg stateMsg;
                    stateMsg.setFlags = BACT_STFLAG_FIRE;
                    stateMsg.unsetFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;

                    SetState(&stateMsg);
                }

                TMinigunShot shot;
                shot.dir = aimDir;
                shot.period = dtime;
                shot.gTime = _clock;

                FireMinigun(&shot);
            }

        }

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            move_msg moveMsg;

            if ( arg->inpt->Buttons.Is(3) )
            {
                _thraction = 0;

                if ( fabs(_fly_dir_length) >= 4.0 )
                {
                    float damping = 1.0 - dtime * 4.0;

                    if ( damping < 0.1 )
                        damping = 0.1;

                    _fly_dir_length *= damping;
                }
                else
                {
                    _status_flg &= ~BACT_STFLAG_MOVE;
                    _fly_dir_length = 0;
                }

                moveMsg.flag = 2;
            }
            else
            {
                moveMsg.flag = 0;
            }

            moveMsg.period = dtime;

            if ( _status_flg & BACT_STFLAG_MOVE )
                Move(&moveMsg);

            TRayIntersection ray;

            ray.stPos = _old_pos;
            ray.vect = _position - _old_pos;
            ray.flags = 0;

            _world->IntersectRay(&ray);

            if ( (bactCollisions && CollisionWithBact(arg->frameTime)) || !ray.isect ) // Check it
            {
                AlignVehicleUser(dtime, oldDir);
            }
            else
            {
                _position = _old_pos;
                _fly_dir_length = 0;
                _thraction = 0;
            }
        }
        else
        {
            TCrashLandStep crashLand;
            crashLand.flags = 1;
            crashLand.frameTime = arg->frameTime;

            CrashOrLand(&crashLand);
        }
    }
    else if (_status == BACT_STATUS_DEAD)
    {
        DeadTimeUpdate(arg);
    }
}

void NC_STACK_ypatank::Move(move_msg *arg)
{
    _old_pos = _position;

    float gravity;

    if ( arg->flag & 2 && _status_flg & BACT_STFLAG_LAND )
    {
        gravity = 0.0;
    }
    else
    {
        if ( _status == BACT_STATUS_DEAD )
            gravity = _mass * 39.2266;
        else
            gravity = _mass * 9.80665;
    }

    vec3d thrust;

    if ( arg->flag & 1 )
        thrust = vec3d::OY(gravity * 2.0);
    else
        thrust = _rotation.AxisZ() * _thraction;

    vec3d netForce = vec3d::OY(gravity) + thrust - _fly_dir * (_fly_dir_length * _airconst);

    float netForceLen = netForce.length();

    if ( netForceLen > 0.0 )
    {
        vec3d velocity = _fly_dir * _fly_dir_length + netForce * ( arg->period / _mass );

        float speed = velocity.length();

        if ( speed > 0.0 )
            velocity /= speed;

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            _fly_dir = _rotation.AxisZ();

            _fly_dir_length = velocity.dot( _fly_dir ) * speed;
        }
        else
        {
            _fly_dir = velocity;
            _fly_dir_length = speed;
        }
    }

    _position += _fly_dir * (_fly_dir_length * arg->period * 6.0);

    CorrectPositionInLevelBox(NULL);

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    float speedRatio = fabs(_fly_dir_length) / (_force / _airconst_static);
    float maxPitch;

    if ( _pitch_max <= -0.8 )
        maxPitch = 1.1;
    else
        maxPitch = _pitch_max;

    float pitchShift = maxPitch * speedRatio;

    if ( pitchShift > maxPitch )
        pitchShift = maxPitch;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch += (float)(_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * pitchShift;
}

size_t NC_STACK_ypatank::SetPosition(TUnitPlacement *arg)
{
    if ( NC_STACK_ypabact::SetPosition(arg) )
    {
        if ( arg->flags & 1 )
        {
            TRayIntersection groundRay;
            groundRay.stPos = _position.X0Z() - vec3d::OY(30000.0);
            groundRay.vect = vec3d::OY(50000.0);
            groundRay.flags = 0;

            _world->IntersectRay(&groundRay);

            if ( !groundRay.isect )
                return 0;

            if ( getBACT_viewer() )
                _position.y = groundRay.isectPos.y - _viewer_overeof;
            else
                _position.y = groundRay.isectPos.y - _overeof;

            _status_flg |= BACT_STFLAG_LAND;
        }
        return 1;
    }
    return 0;
}

void NC_STACK_ypatank::ApplyImpulse(TImpulse *arg)
{
    float massFactor = 500.0 / _mass;

    vec3d offset = _position - arg->pos;

    float strength = arg->energ * 0.0004;

    float dist = offset.length();

    if ( dist <= 1.0 )
    {
        offset = arg->dir;
        dist = 1.0;
    }
    else
        offset /= dist;

    offset *= strength * 10.0 * massFactor / dist;

    vec3d newPos = _position + vec3d(offset.x, -offset.y, offset.z);

    TRayIntersection ray;
    ray.stPos = _position;
    ray.vect = newPos - _position;
    ray.flags = 0;

    _world->IntersectRay(&ray);

    if ( !ray.isect )
        _position = newPos;
}

void NC_STACK_ypatank::TankAbsorbPlasmaEnergy(NC_STACK_ypabact *bact2)
{
    int plasmaLifetime = (float)bact2->_energy_max * 0.7;

    if ( plasmaLifetime < 10000 )
        plasmaLifetime = 10000;

    if ( plasmaLifetime > 25000 )
        plasmaLifetime = 25000;

    float gain = (float)bact2->_scale_time * 0.2 / (float)plasmaLifetime * (float)bact2->_energy_max;

    if ( gain + _energy > _energy_max )
    {
        NC_STACK_yparobo *robo = _host_station;

        // TODO(bug): Fills the tank before working out the excess, so the host
        // station gets the whole amount, not just the overflow. AbsorbPlasmaEnergy
        // in ypabact.cpp does it in the right order; this otherwise duplicates it.
        _energy = _energy_max;

        int excess = (int)gain - (_energy_max - _energy);

        if ( excess + robo->_energy > robo->_energy_max )
        {
            int oldRoboEnergy = robo->_energy;

            robo->_energy = robo->_energy_max;
            int roboExcess = excess - (robo->_energy_max - oldRoboEnergy);
            int newLifeEnergy = roboExcess + robo->_roboEnergyLife;

            if ( newLifeEnergy >= robo->_energy_max )
            {
                int oldLifeEnergy = robo->_roboEnergyLife;
                robo->_roboEnergyLife = robo->_energy_max;

                robo->_roboEnergyMove += roboExcess - (robo->_energy_max - oldLifeEnergy);

                if ( robo->_roboEnergyMove > robo->_energy_max )
                    robo->_roboEnergyMove = robo->_energy_max;
            }
            else
            {
                robo->_roboEnergyLife = newLifeEnergy;
            }
        }
        else
        {
            robo->_energy = excess + robo->_energy;
        }
    }
    else
    {
        _energy = gain + _energy;
    }
}

size_t NC_STACK_ypatank::CollisionWithBact(int arg)
{
    int otherNotWaiting = 0;
    int otherBlocking = 0;

    float hitAngle = 0.0;
    int hitOnRight = 0;

    NC_STACK_ypabact *pushBacts[10];
    vec3d pushDirs[10];

    if ( _status_flg & BACT_STFLAG_LAND )
    {
        int isUser = getBACT_inputting();

        float minCos;

        if ( isUser )
            minCos = 0.6;
        else
            minCos = 0.82;

        TSectorInfo sectInfo;
        sectInfo.pos_x = _position.x;
        sectInfo.pos_z = _position.z;

        int pushCount = 0;
        int inContact = 0;

        NC_STACK_ypabact *nearestUnit = NULL;

        float checkRange = 300.0;
        float dtime = arg * 0.001;

        if ( !_world->GetSectorInfo(&sectInfo) )
            return 0;

        float nearestDist = World::CVSectorLength;

        for ( NC_STACK_ypabact* &unit : sectInfo.pcell->unitsList )
        {
            int absorbable = unit->_status == BACT_STATUS_DEAD   &&   unit->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE   &&  isUser &&   unit->_scale_time > 0;

            if ( unit->_bact_type != BACT_TYPES_MISSLE
                    && (!unit->IsDestroyed() || absorbable)
                    && unit != this )
            {

                World::TRoboCollSet *collSet = unit->getBACT_collNodes();

                int collCount;

                if ( collSet )
                    collCount = collSet->roboColls.size();
                else
                    collCount = 1;

                for (int j = collCount - 1; j >= 0; j--)
                {
                    vec3d collPos;
                    float collRadius;

                    if ( !collSet )
                    {
                        collPos = unit->_position;
                        collRadius = unit->_radius;
                    }
                    else
                    {
                        World::TRoboColl &coll = collSet->roboColls[j];

                        collPos = unit->_position + unit->_rotation.Transpose().Transform(coll.coll_pos);
                        collRadius = coll.robo_coll_radius;
                    }

                    if (!collSet || collRadius >= 0.01 )
                    {
                        vec3d toColl = collPos - _position;

                        float dist = toColl.length();

                        if ( dist < 300.0 )
                        {
                            float touchDist = _radius + collRadius;

                            if ( absorbable && dist < touchDist )
                            {
                                TankAbsorbPlasmaEnergy(unit);

                                unit->_scale_time = -1;

                                if ( _world->_GameShell )
                                    SFXEngine::SFXe.startSound(&_world->_GameShell->samples1_info, World::SOUND_ID_PLASMA);

                                if ( _world->_isNetGame )
                                {
                                    uamessage_endPlasma eplMsg;
                                    eplMsg.msgID = UAMSG_ENDPLASMA;
                                    eplMsg.owner = unit->_owner;
                                    eplMsg.id = unit->_gid;

                                    _world->NetBroadcastMessage(&eplMsg, sizeof(eplMsg), true);

                                    if ( _owner != unit->_owner )
                                    {
                                        unit->_vp_extra[0].flags = 0;
                                        unit->_vp_extra[0].SetVP((NC_STACK_base::Instance *)NULL);
                                    }
                                }
                                break;
                            }
                            else
                            {

                                float aheadCos = toColl.dot( _rotation.AxisZ() );

                                if ( dist > 0.01 )
                                    aheadCos = aheadCos / dist;

                                if ( fabs(_fly_dir_length) >= 0.1 )
                                {
                                    if ( _fly_dir_length < 0.0 )
                                    {
                                        aheadCos = -aheadCos;
                                    }
                                }
                                else if ( _thraction < 0.0 )
                                {
                                    aheadCos = -aheadCos;
                                }

                                if ( aheadCos >= minCos && dist < touchDist )
                                {
                                    if ( !nearestUnit || dist < nearestDist )
                                    {
                                        nearestDist = dist;
                                        nearestUnit = unit;

                                        hitAngle = clp_acos(aheadCos);

                                        float side = toColl.XZ().cross( _rotation.AxisZ().XZ() );
                                        hitOnRight = side > 0.0;

                                        if ( dist < touchDist )
                                            inContact = 1;

                                        if ( _rotation.AxisZ().dot( nearestUnit->_rotation.AxisZ() ) < 0.0 )
                                            otherBlocking = 1;

                                        if ( nearestUnit->_status == BACT_STATUS_IDLE )
                                            otherBlocking = 1;

                                        if ( nearestUnit->_waitCol_time <= 0 )
                                            otherNotWaiting = 1;
                                    }

                                    if ( isUser )
                                    {
                                        if ( !collSet && pushCount < 10 )
                                        {
                                            pushBacts[pushCount] = unit;

                                            if ( dist <= 0.0001 )
                                                pushDirs[pushCount] = toColl;
                                            else
                                                pushDirs[pushCount] = toColl / dist;
                                            pushCount++;
                                        }
                                    }
                                }

                            }
                        }
                    }



                }


            }
        }


        if ( !nearestUnit )
        {
            _status_flg &= ~BACT_STFLAG_BCRASH;
            return 0;
        }

        if ( isUser )
        {
            if ( inContact && pushCount )
            {
                vec3d avgDir(0.0, 0.0, 0.0);

                for (int i = 0; i < pushCount; i++)
                {
                    if ( !_world->_isNetGame || _owner == pushBacts[i]->_owner )
                    {
                        float momentum = _mass * 8.0 * fabs(_fly_dir_length);
                        float thrustPush = _thraction * dtime * 100.0;

                        float pushDist = dtime * ((momentum + thrustPush) / pushBacts[i]->_mass);

                        if ( pushDist > 0.05 )
                        {
                            vec3d newPos = pushDirs[i] * pushDist + pushBacts[i]->_position;

                            TRayIntersection ray;
                            ray.stPos = pushBacts[i]->_position;
                            ray.vect = newPos - pushBacts[i]->_position;
                            ray.flags = 0;

                            _world->IntersectRay(&ray);

                            if ( !ray.isect )
                            {

                                pushBacts[i]->_old_pos = pushBacts[i]->_position;

                                pushBacts[i]->_position = newPos;

                                pushBacts[i]->_status_flg &= ~BACT_STFLAG_LAND;

                                pushBacts[i]->CorrectPositionInLevelBox(NULL);
                            }
                        }

                        avgDir += pushDirs[i];
                    }
                }

                avgDir /= (float)pushCount;

                vec3d moveDir = _rotation.AxisZ();

                if ( _fly_dir_length < 0.0 )
                    moveDir = -moveDir;

                float blockCos = moveDir.XZ().dot( avgDir.XZ() );

                float moveDirLen = moveDir.XZ().length();

                if ( moveDirLen > 0.001 )
                    blockCos = blockCos / moveDirLen;

                float avgDirLen = avgDir.XZ().length();

                if ( avgDirLen > 0.001 )
                    blockCos = blockCos / avgDirLen;

                if ( blockCos >= 0.95 )
                {
                    _position = _old_pos;
                    _fly_dir_length = 0;
                }
                else
                {
                    if ( avgDir.XZ().cross( moveDir.XZ() ) >= 0.0 )
                    {
                        // TODO(bug): oldX is an int, so the old avgDir.x (|x| <= 1) becomes 0 or +-1 when
                        // it moves into avgDir.z, here and in the else branch. The slide
                        // direction comes out wrong.
                        int oldX = avgDir.x;
                        avgDir.x = -avgDir.z;
                        avgDir.z = oldX;
                    }
                    else
                    {
                        int oldX = avgDir.x;
                        avgDir.x = avgDir.z;
                        avgDir.z = -oldX;
                    }

                    float slideDist = fabs(_fly_dir_length) * dtime * 6.0;

                    _position = avgDir * slideDist + _old_pos;
                }

                if ( !(_status_flg & BACT_STFLAG_BCRASH) )
                {
                    SFXEngine::SFXe.startSound(&_soundcarrier, 6);
                    _status_flg |= BACT_STFLAG_BCRASH;
                }

                TFFeedbackEffect feedback;

                feedback.strength = 1.0;
                feedback.pos_x = avgDir.x;
                feedback.pos_z = avgDir.z;
                feedback.effects_type = 5;

                _world->FFeedback_Play(&feedback);
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_BCRASH;
            }
        }
        else
        {
            if ( inContact )
            {
                _fly_dir_length = 0;
                _thraction = 0;

                _position = _old_pos;
                _status_flg &= ~BACT_STFLAG_MOVE;

                if ( !(_status_flg & BACT_STFLAG_BCRASH) )
                    _status_flg |= BACT_STFLAG_BCRASH;

                if ( otherBlocking || !otherNotWaiting )
                {
                    if ( !(_tankCollisionFlag & (COLL_HILL_L | COLL_HILL_R)) )
                    {
                        if ( _tankCollisionFlag & (COLL_WALL_L | COLL_WALL_R) )
                        {
                            _tankCollisionWay = 100.0;
                            _tankCollisionAngle = 1.5;
                        }
                        else
                        {
                            if ( hitOnRight )
                                _tankCollisionFlag |= COLL_WALL_R;
                            else
                                _tankCollisionFlag |= COLL_WALL_L;

                            _tankCollisionWay = 80.0;
                            _tankCollisionAngle = 1.5 - hitAngle;
                        }
                    }
                }
                else
                {
                    _waitCol_time = 1000;
                }
            }
            else
            {
                _fly_dir_length *= nearestDist / checkRange;
                _status_flg &= ~BACT_STFLAG_BCRASH;
            }
        }

        if ( isUser && inContact )
            return 1;
        return 0;

    }

    return NC_STACK_ypabact::CollisionWithBact(arg);
}

void NC_STACK_ypatank::Recoil(TCollisionNormal *arg)
{
    if ( !(_status_flg & BACT_STFLAG_LAND) )
    {
        NC_STACK_ypabact::Recoil(arg);

        _fly_dir = _fly_dir * _fly_dir_length - arg->normal * 5.0;

        _fly_dir_length = _fly_dir.length();

        if ( _fly_dir_length > 0.001 )
            _fly_dir /= _fly_dir_length;
    }
}

void NC_STACK_ypatank::Renew()
{
    NC_STACK_ypabact::Renew();

    _tankFlags = (FLAG_TANK_TIP | FLAG_TANK_ROTWAIT);
    _tankWaitCount = 0;
    _tankCollisionVector = vec3d();
    _tankCollisionAngle = 0.0;
    _tankCollisionWay = 0.0;
    _tankCollisionFlag = 0;
    _tankExpectTgt = false;
}

void NC_STACK_ypatank::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("tankFlags", _tankFlags);
    v.Field("tankWaitCount", _tankWaitCount);
    v.Field("tankCollisionVector", _tankCollisionVector);
    v.Field("tankCollisionAngle", _tankCollisionAngle);
    v.Field("tankCollisionWay", _tankCollisionWay);
    v.Field("tankCollisionFlag", _tankCollisionFlag);
    v.Field("tankExpectTarget", _tankExpectTgt);
}

size_t NC_STACK_ypatank::CheckFireAI(TFireCheck *arg)
{
    vec3d toTarget = arg->pos - _position;

    float targetDist = toTarget.length();

    if ( targetDist == 0.0 )
        return 0;

    toTarget /= targetDist;

    _tankExpectTgt = true;

    TRayIntersection ray;

    ray.stPos = _position;
    ray.vect = arg->pos - ray.stPos;
    ray.flags = 0;

    _world->IntersectLongRay(&ray);

    if ( ray.isect )
    {
        if ( arg->tgType == 2 )
        {
            NC_STACK_ypabact *target;

            if ( _secndTtype == BACT_TGT_TYPE_UNIT )
                target = _secndT.pbact;
            else
                target = _primT.pbact;

            if ( target )
            {
                if ( target->_bact_type == BACT_TYPES_TANK || target->_bact_type == BACT_TYPES_CAR || target->_bact_type == BACT_TYPES_GUN )
                    _tankExpectTgt = false;
            }
        }
        else
        {
            TSectorInfo targetSect;
            TSectorInfo hitSect;

            targetSect.pos_x = arg->pos.x;
            targetSect.pos_z = arg->pos.z;

            hitSect.pos_x = ray.isectPos.x;
            hitSect.pos_z = ray.isectPos.z;

            if ( _world->GetSectorInfo(&targetSect) )
            {
                if ( _world->GetSectorInfo(&hitSect) )
                {
                    if ( targetSect.pcell != hitSect.pcell )
                    {
                        _tankExpectTgt = false;
                    }
                    else
                    {
                        if ( targetSect.pcell->SectorType != 1 )
                        {
                            int tgtSubX = (targetSect.pos_x + -150.0) / 300.0;
                            int tgtSubZ = (targetSect.pos_z + 150.0) / 300.0;

                            int hitSubX = (hitSect.pos_x + -150.0) / 300.0;
                            int hitSubZ = (hitSect.pos_z + 150.0) / 300.0;

                            if ( tgtSubX != hitSubX || hitSubZ != tgtSubZ )
                            {
                                _tankExpectTgt = false;
                            }
                        }
                    }
                }
            }
        }
    }

    World::TWeapProto *weapon = NULL;
    int directFire = 0;

    if ( _weapon == -1 )
    {
        weapon = NULL;
    }
    else
    {
        weapon = &_world->GetWeaponsProtos().at( _weapon );

        if ( weapon->_weaponFlags & 1 )
            directFire = weapon->_weaponFlags & 0xFE;
        else
            weapon = NULL;
    }

    if ( !weapon )
    {
        if ( _mgun == -1 )
            return 0;
        directFire = 2;
    }

    float cosAngle = 0.0;

    if ( toTarget.XZ() != vec2d(0.0, 0.0) )
    {
        float tmpsq = toTarget.XZ().length();

        if (isnormal(tmpsq))  // Not NULL, NAN, INF
            cosAngle = toTarget.XZ().dot( _rotation.AxisZ().XZ() ) / tmpsq;

        tmpsq = _rotation.AxisZ().XZ().length();

        if (isnormal(tmpsq))  // Not NULL, NAN, INF
            cosAngle /= tmpsq;
        else
            cosAngle = 0.0;
    }
    else
    {
        cosAngle = 0.0;
    }

    if ( arg->tgType == 2 )
    {
        if ( directFire )
        {
            if ( targetDist < World::CVSectorLength && cosAngle > 0.93 )
            {
                if ( toTarget.y > -0.85 && toTarget.y < 0.2 )
                    return 1;
            }
        }
        else if ( toTarget.y > 0.98 )
        {
            return 1;
        }
    }
    else if ( weapon )
    {
        if ( directFire )
        {
            if ( targetDist < World::CVSectorLength && cosAngle > 0.91 )
            {
                if ( toTarget.y > -0.4 && toTarget.y < 0.3 )
                    return 1;
            }
        }
        else if ( toTarget.y > 0.92 )
        {
            return 1;
        }
    }
    return 0;
}

bool NC_STACK_ypatank::TestTargetSector(const NC_STACK_ypabact * tgt) const
{
    if ( tgt->_cellId.x == _cellId.x || tgt->_cellId.y == _cellId.y )
    {
        if ( fabs(_pSector->height - tgt->_pSector->height) >= 500.0 )
            return 0;
    }
    else
    {
        TSectorInfo sectInfo;
        sectInfo.pos_x = _position.x;
        sectInfo.pos_z = tgt->_position.z;

        _world->GetSectorInfo(&sectInfo);

        cellArea *cornerCell = sectInfo.pcell;
        sectInfo.pos_x = tgt->_position.x;
        sectInfo.pos_z = _position.z;

        _world->GetSectorInfo(&sectInfo);

        if ( fabs(_pSector->height    - cornerCell->height) >= 500.0 && fabs(sectInfo.pcell->height -   tgt->_pSector->height) >= 500.0 )
            return 0;
        if ( fabs(_pSector->height    - sectInfo.pcell->height) >= 500.0 && fabs(cornerCell->height -   tgt->_pSector->height) >= 500.0 )
            return 0;
        if ( fabs(_pSector->height    - cornerCell->height) >= 500.0 && fabs(_pSector->height    - sectInfo.pcell->height) >= 500.0 )
            return 0;
        if ( fabs(cornerCell->height -   tgt->_pSector->height) >= 500.0 && fabs(sectInfo.pcell->height -   tgt->_pSector->height) >= 500.0 )
            return 0;
    }
    return 1;
}

void NC_STACK_ypatank::CorrectPositionOnLand()
{
    NC_STACK_ypabact::CorrectPositionOnLand();

    if ( _status_flg & BACT_STFLAG_LAND )
    {
        float groundOffset;

        if ( getBACT_viewer() )
            groundOffset = _viewer_overeof;
        else
            groundOffset = _overeof;

        TRayIntersection groundRay;
        groundRay.stPos = _position;
        groundRay.vect = vec3d::OY( groundOffset * 2.0 );
        groundRay.flags = 0;

        _world->IntersectRay(&groundRay);

        if ( groundRay.isect )
            _position.y = groundRay.isectPos.y - groundOffset;
        else
            _status_flg &= ~BACT_STFLAG_LAND;
    }
}

vec3d NC_STACK_ypatank::TankTip(vec3d v)
{
    float force = _thraction - _airconst * _fly_dir_length;

    if ( fabs(force) > 3.0 )
    {
        float angle = 0.2 * (force / _force);

        v = mat3x3::AxisAngle(_rotation.AxisX(), -angle).Transform(v);
    }
    return v;
}

int NC_STACK_ypatank::AlignVehicleAI(float dtime, vec3d *pNormal)
{
    float groundOffset;

    if ( getBACT_viewer() )
        groundOffset = _viewer_overeof;
    else
        groundOffset = _overeof;

    TRayIntersection groundRay;
    groundRay.stPos = _rotation.AxisY() * groundOffset + _position - vec3d::OY(50.0);
    groundRay.vect = vec3d::OY( _overeof + 110.0 );
    groundRay.flags = 0;

    _world->IntersectRay(&groundRay);

    if ( !groundRay.isect )
        return ALIGN_NONE;
    
    vec3d normal = groundRay.skel->polygons[ groundRay.polyID ].Normal();

    if ( normal.y < 0.6 )
    {
        if (pNormal)
            *pNormal = normal;
        return ALIGN_NORMAL;
    }

    if ( _tankFlags & FLAG_TANK_TIP )
        normal = TankTip(normal);

    vec3d vaxis = _rotation.AxisY() * normal;

    float axisLen = vaxis.length();

    if ( axisLen > 0.0 )
    {
        float tiltAngle = clp_acos( normal.dot( _rotation.AxisY() ) );

        float maxTilt = _maxrot * 2.0 * dtime;

        if ( tiltAngle > maxTilt )
            tiltAngle = maxTilt;

        if ( fabs(tiltAngle) < 0.01 )
            tiltAngle = 0.0;

        vaxis /= axisLen;

        _rotation *= mat3x3::AxisAngle(vaxis, tiltAngle);
    }

    _position = groundRay.isectPos - _rotation.AxisY() * groundOffset;

    return ALIGN_DONE;
}

int NC_STACK_ypatank::AlignVehicleUser(float dtime, const vec3d &oldDir)
{
    float liftFactor = 1.73;
    float rayLenFactor = 1.7;
    int leftMiss = 0;
    int rightMiss = 0;
    int rearMiss = 0;

    bool isViewer = getBACT_viewer();

    int isUser = getBACT_inputting();

    float groundOffset;
    if ( isViewer )
        groundOffset = _viewer_overeof;
    else
        groundOffset = _overeof;

    vec3d rightPoint;
    vec3d leftPoint;
    vec3d rearPoint;
    float dirSign;

    if ( _fly_dir_length < 0.0 )
    {
        leftPoint = _position + (-_rotation.AxisZ() - _rotation.AxisX()) * _viewer_radius * 0.7071;

        rightPoint = _position + (_rotation.AxisX() - _rotation.AxisZ()) * _viewer_radius * 0.7071;

        rearPoint = _position + _rotation.AxisZ() * _viewer_radius;

        dirSign = -1.0;
    }
    else
    {
        leftPoint = _position + (_rotation.AxisZ() - _rotation.AxisX()) * _viewer_radius * 0.7071;

        rightPoint = _position + (_rotation.AxisZ() + _rotation.AxisX()) * _viewer_radius * 0.7071;

        rearPoint = _position - _rotation.AxisZ() * _viewer_radius;

        dirSign = 1.0;
    }

    rearPoint.y += _viewer_overeof;
    leftPoint.y += _viewer_overeof;
    rightPoint.y += _viewer_overeof;

    TRayIntersection leftRay;
    TRayIntersection rightRay;
    TRayIntersection rearRay;

    leftRay.stPos = _position - vec3d::OY(liftFactor * _viewer_radius);
    rightRay.stPos = _position - vec3d::OY(liftFactor * _viewer_radius);
    rearRay.stPos = _position - vec3d::OY(liftFactor * _viewer_radius);

    leftRay.vect = (leftPoint - leftRay.stPos) * rayLenFactor;

    rightRay.vect = (rightPoint - rightRay.stPos) * rayLenFactor;

    rearRay.vect = (rearPoint - rearRay.stPos) * rayLenFactor;

    if ( isViewer )
    {
        leftRay.flags = 0;
        rightRay.flags = 0;
        rearRay.flags = 0;
    }
    else
    {
        leftRay.flags = 1;
        rightRay.flags = 1;
        rearRay.flags = 1;
    }

    _world->IntersectRay(&leftRay);

    if ( leftRay.isect )
    {
        vec3d wallNormal = leftRay.skel->polygons[ leftRay.polyID ].Normal();

        if ( fabs(wallNormal.y) < 0.6 )
        {
            float wallCos = wallNormal.dot(_rotation.AxisZ()) * dirSign;

            if ( wallCos <= 0.0 )
            {
                _thraction = 0;
            }
            else
            {

                if ( _fly_dir_length > 2.333333333333334 )
                {
                    if ( isUser )
                    {
                        SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                        TFFeedbackEffect feedback;
                        feedback.strength = 1.0;
                        feedback.pos_x = leftRay.isectPos.x;
                        feedback.pos_z = leftRay.isectPos.z;
                        feedback.effects_type = 5;

                        _world->FFeedback_Play(&feedback);
                    }
                }

                if ( wallCos >= 0.82 )
                {
                    _thraction = 0;
                    _fly_dir_length = 0;
                    _position = _old_pos;
                    return 2;
                }

                _position.x = _old_pos.x - wallNormal.x * 10.0;
                _position.z = _old_pos.z - wallNormal.z * 10.0;
            }
            return 2;
        }
    }
    else
    {
        if ( !isUser )
            return 2;

        leftRay.isectPos = _position + leftRay.vect - vec3d::OY(liftFactor * _viewer_radius);

        leftMiss = 1;
    }

    _world->IntersectRay(&rightRay);

    if ( rightRay.isect )
    {
        vec3d wallNormal = rightRay.skel->polygons[ rightRay.polyID ].Normal();

        if ( fabs(wallNormal.y) < 0.6 )
        {
            float wallCos = wallNormal.dot(_rotation.AxisZ()) * dirSign;

            if ( wallCos <= 0.0 )
            {
                _thraction = 0;
            }
            else
            {
                if ( _fly_dir_length > 2.333333333333334 )
                {
                    if ( isUser )
                    {
                        SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                        TFFeedbackEffect feedback;
                        feedback.pos_x = rightRay.isectPos.x;
                        feedback.pos_z = rightRay.isectPos.z;
                        feedback.effects_type = 5;
                        feedback.strength = 1.0;

                        _world->FFeedback_Play(&feedback);
                    }
                }
                if ( wallCos >= 0.82 )
                {
                    _thraction = 0;
                    _fly_dir_length = 0;
                    _position = _old_pos;

                    return 1;
                }

                _position.x = _old_pos.x - wallNormal.x * 10.0;
                _position.z = _old_pos.z - wallNormal.z * 10.0;
            }
            return 1;
        }
    }
    else
    {
        if ( !isUser )
            return 1;

        rightRay.isectPos = _position + rightRay.vect - vec3d::OY(liftFactor * _viewer_radius);
        rightMiss = 1;
    }

    _world->IntersectRay(&rearRay);

    if ( rearRay.isect )
    {
        if ( fabs(rearRay.skel->polygons[rearRay.polyID].B) < 0.6 )
        {
            _thraction = 0;

            if ( _fly_dir_length < -2.333333333333334 )
            {
                if ( isUser )
                {
                    SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                    TFFeedbackEffect feedback;
                    feedback.strength = 1.0;
                    feedback.pos_x = rearRay.isectPos.x;
                    feedback.pos_z = rearRay.isectPos.z;
                    feedback.effects_type = 5;

                    _world->FFeedback_Play(&feedback);
                }
            }

            _position += _fly_dir * (_fly_dir_length * dtime * 6.0);

            return 3;
        }
    }
    else
    {
        rearRay.isectPos = _position + rearRay.vect - vec3d::OY(liftFactor * _viewer_radius);

        rearMiss = 1;
    }

    if ( isViewer )
    {
        if ( dirSign > 0.0 )
        {
            TRayIntersection frontRay;
            frontRay.stPos = _position;
            frontRay.vect = _rotation.AxisZ() * _viewer_radius;
            frontRay.flags = 0;

            _world->IntersectRay(&frontRay);

            if ( frontRay.isect )
            {
                if ( frontRay.skel->polygons[ frontRay.polyID ].B < 0.6 )
                {
                    if ( _fly_dir_length > 2.333333333333334 )
                    {
                        if ( isUser )
                        {
                            SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                            TFFeedbackEffect feedback;
                            feedback.effects_type = 5;
                            feedback.strength = 1.0;
                            feedback.pos_x = frontRay.isectPos.x;
                            feedback.pos_z = frontRay.isectPos.z;

                            _world->FFeedback_Play(&feedback);
                        }
                    }

                    _thraction = 0;
                    _fly_dir_length = 0;
                    _position = _old_pos;
                }
            }
        }
    }

    if ( rightRay.isect && leftRay.isect && rearRay.isect )
    {

        vec3d rightEdge = rightRay.isectPos - rearRay.isectPos;
        vec3d leftEdge = leftRay.isectPos - rearRay.isectPos;
        vec3d groundNormal = leftEdge * rightEdge;

        float normalLen = groundNormal.length() * dirSign;

        if ( fabs(normalLen) <= 0.0001 )
            groundNormal = vec3d::OY(1.0);
        else
            groundNormal /= normalLen;

        if ( groundNormal.y < -0.1 )
            groundNormal = -groundNormal;

        if ( _tankFlags & FLAG_TANK_TIP )
            groundNormal = TankTip(groundNormal);

        float speed = fabs(_fly_dir_length);

        vec3d vaxis = _rotation.AxisY() * groundNormal;;

        float minTilt;
        if ( speed >= 5.0 )
            minTilt = 0.007;
        else
            minTilt = 0.01;

        float axisLen = vaxis.length();
        if ( axisLen > 0.0 )
        {
            vaxis /= axisLen;

            float tiltAngle = clp_acos( groundNormal.dot( _rotation.AxisY() ) );

            if ( tiltAngle > _maxrot * 2.0 * dtime )
                tiltAngle = _maxrot * 2.0 * dtime;

            if ( fabs(tiltAngle) > minTilt )
                _rotation *= mat3x3::AxisAngle(vaxis, tiltAngle);
        }
    }

    if ( leftRay.isect || rightRay.isect || rearRay.isect )
    {
        if ( leftMiss || rightMiss || rearMiss )
        {
            float probeHalf = _viewer_overeof * rayLenFactor * 0.8;

            TRayIntersection groundRay;
            groundRay.stPos = _position - vec3d::OY(probeHalf);
            groundRay.vect = vec3d::OY( probeHalf * 2.0 );

            groundRay.flags = isViewer == 0;

            _world->IntersectRay(&groundRay);

            if ( !groundRay.isect || (groundRay.isect && groundRay.skel->polygons[ groundRay.polyID ].B < 0.6) )
            {
                vec3d tmp(0.0, 0.0, 0.0);

                _status_flg &= ~BACT_STFLAG_LAND;

                if ( leftMiss )
                    tmp = _rotation.AxisZ() * dirSign - _rotation.AxisX();

                if ( rightMiss )
                    tmp += _rotation.AxisZ() * dirSign + _rotation.AxisX();

                if ( rearMiss )
                    tmp -= _rotation.AxisZ() * dirSign;

                _position += tmp * (dtime * 400.0);
                return 0;
            }
            _position.y = groundRay.isectPos.y - groundOffset;
        }
        else
        {
            _position.y = (leftRay.isectPos.y + rightRay.isectPos.y + rearRay.isectPos.y) * 0.33333334 - groundOffset;
        }
    }
    else
    {
        _status_flg &= ~BACT_STFLAG_LAND;
    }
    return 0;
}

void NC_STACK_ypatank::setTANK_tip(int tip)
{
    if ( tip )
        _tankFlags |= FLAG_TANK_TIP;
    else
        _tankFlags &= ~FLAG_TANK_TIP;
}

int NC_STACK_ypatank::getTANK_tip()
{
    if (_tankFlags & FLAG_TANK_TIP)
        return 1;
    return 0;
}


