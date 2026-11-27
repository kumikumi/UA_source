#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "yw.h"
#include "ypaufo.h"
#include "log.h"

#include <math.h>
#include "world/state.h"


size_t NC_STACK_ypaufo::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_UFO;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {

            case UFO_ATT_TOGO:
                setUFO_togo(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypaufo::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_ypaufo::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {

            case UFO_ATT_TOGO:
                setUFO_togo(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypaufo::AI_layer3(update_msg *arg)
{
    float dtime = arg->frameTime / 1000.0;

    int bactCollisions = getBACT_bactCollisions();

    int nearViewer = _world->IsNearViewer(this);

    float targetDist = _target_vec.length();

    if ( targetDist != 0.0 )
        _target_dir = _target_vec / targetDist;

    int nearTarget = !_secndTtype && targetDist < World::CVSectorLength;

    if ( targetDist > _ufoTogo )
        targetDist = _ufoTogo;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    {
        _thraction = _force;

        if ( !bactCollisions
                || (!nearViewer && !nearTarget)
                || !CollisionWithBact(arg->frameTime) )
        {
            if ( _primTtype || _secndTtype )
            {
                _ufoBoost = _mass * 9.80665;
                _thraction = _force;


                if ( _ufoFlags & 1 )
                {
                    _thraction = 0;

                    if ( fabs(_fly_dir_length * _fly_dir.x) >= 0.1 || fabs(_fly_dir_length * _fly_dir.z) >= 0.1 )
                    {
                        _thraction = 0;
                        _fly_dir_length *= 0.8;
                    }
                    else
                    {
                        if ( _ufoFlags & 8 )
                        {
                            float turnStep = _maxrot * dtime;

                            if ( fabs(_ufoProcAngle) >= turnStep )
                            {
                                if ( _ufoProcAngle < 0.0 )
                                    turnStep = -turnStep;

                                _ufoProcAngle -= turnStep;

                                _rotation = mat3x3::RotateY(turnStep) * _rotation;
                            }
                            else
                            {
                                _rotation = mat3x3::RotateY(_ufoProcAngle) * _rotation;

                                _ufoFlags &= 0xFFFFFFF6;
                                _ufoProcAngle = 0;
                                _ufoTogoRisidue = targetDist;
                            }
                        }
                        else
                        {
                            if ( _ufoFlags & 2 )
                                _ufoBoost = _mass * 4.0 * 9.80665;

                            if ( _ufoFlags & 4 )
                                _ufoBoost = _mass * 0.3 * 9.80665;
                        }
                    }
                }

                move_msg moveMsg;
                moveMsg.flag = 0;
                moveMsg.period = dtime;

                Move(&moveMsg);

                TRayIntersection fwdRay;
                fwdRay.stPos = _old_pos;

                vec2d ttmp = _position.XZ() - _old_pos.XZ();

                float moveDist = ttmp.length();

                if ( moveDist >= 0.01 )
                    fwdRay.vect = vec3d::X0Z(ttmp * 300.0 / moveDist);
                else
                    fwdRay.vect = _rotation.AxisZ() * 300.0;

                TRayIntersection downRay;
                downRay.stPos = _position;
                downRay.vect = vec3d::OY( _height );

                fwdRay.flags = 0;
                downRay.flags = 0;

                _world->IntersectRay(&downRay);

                _world->IntersectRay(&fwdRay);

                if ( (fwdRay.isect && fwdRay.tVal * 300.0 < _radius) || (downRay.isect && downRay.tVal * 300.0 < _radius) )
                {
                    TCollisionNormal collNormal;

                    if ( fwdRay.isect && fwdRay.tVal * 300.0 < _radius )
                        collNormal.normal = fwdRay.skel->polygons[fwdRay.polyID].Normal();
                    else
                        collNormal.normal = downRay.skel->polygons[downRay.polyID].Normal();

                    Recoil(&collNormal);
                }

                if ( _ufoFlags & 1 )
                {
                    if ( _ufoFlags & 6 )
                    {

                        if ( _ufoFlags & 2 )
                        {
                            if ( !downRay.isect || (downRay.isect && downRay.tVal > 0.8) )
                            {
                                _ufoFlags &= 0xFFFFFFFC;
                                _ufoTogoRisidue = targetDist;
                            }
                        }

                        if ( _ufoFlags & 4 )
                        {
                            if ( downRay.isect && downRay.tVal < 0.8 )
                            {
                                _ufoFlags &= 0xFFFFFFFA;
                                _ufoTogoRisidue = targetDist;
                            }
                        }

                    }
                }
                else if ( (fwdRay.isect && fwdRay.tVal < 0.5) || (downRay.isect && downRay.tVal < 0.5) )
                {
                    if ( downRay.isect && downRay.tVal < 0.5 )
                    {
                        _ufoFlags |= 3;
                    }
                    else
                    {
                        _ufoFlags |= 9;

                        vec2d polv = fwdRay.skel->polygons[fwdRay.polyID].Normal().XZ();
                        vec2d axsz = _rotation.AxisZ().XZ();

                        float tmpsq = axsz.length();
                        float cosAngle = 0.0;
                        
                        if (isnormal(tmpsq))  // Not NULL, NAN, INF
                            cosAngle = axsz.dot( polv ) / tmpsq;

                        tmpsq = polv.length();

                        if (isnormal(tmpsq))  // Not NULL, NAN, INF
                            cosAngle /= tmpsq;
                        else
                            cosAngle = 0.0;

                        _ufoProcAngle = C_PI_2 - clp_acos(cosAngle);
                    }
                }
                else if ( _ufoTogoRisidue <= 0.0 )
                {
                    if ( downRay.isect )
                    {
                        vec2d tgt2d = _target_dir.XZ();
                        vec2d axsz = _rotation.AxisZ().XZ();

                        float tmpsq = axsz.length();
                        float cosAngle = 0.0;
                        
                        if (isnormal(tmpsq))  // Not NULL, NAN, INF
                            cosAngle = tgt2d.dot( axsz ) / tmpsq;

                        tmpsq = tgt2d.length();

                        if (isnormal(tmpsq))  // Not NULL, NAN, INF
                            cosAngle /= tmpsq;
                        else
                            cosAngle = 0.0;

                        float turnAngle = clp_acos(cosAngle);

                        if ( turnAngle > 0.2 )
                        {
                            if ( axsz.cross( tgt2d ) < 0.0 )
                                turnAngle = -turnAngle;

                            _ufoProcAngle = turnAngle;
                            _ufoFlags |= 9;
                        }
                    }
                    else
                    {
                        NC_STACK_ypabact *userVehicle = _world->UserUnit();;

                        if ( ( (_secndTtype != BACT_TGT_TYPE_UNIT || userVehicle != _secndT.pbact) && ( _primTtype != BACT_TGT_TYPE_UNIT || userVehicle != _primT.pbact) ) || _target_dir.y >= 0.0 )
                        {
                            _ufoFlags |= 5;
                        }
                    }
                }

                TFightTarget fightMsg;
                fightMsg.fperiod = dtime;
                fightMsg.g_time = _clock;

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
                    fightMsg.target.pbact = _primT.pbact;
                    fightMsg.prio = 0;

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
                        stateMsg.unsetFlags = BACT_STFLAG_FIRE;
                        stateMsg.newStatus = BACT_STATUS_NOPE;
                        stateMsg.setFlags = 0;

                        SetState(&stateMsg);
                    }
                }
            }
            else
            {
                _status_flg &= ~(BACT_STFLAG_FIGHT_P | BACT_STFLAG_FIGHT_S);

                setState_msg stateMsg;

                if ( _status_flg & BACT_STFLAG_FIRE )
                {
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.setFlags = 0;
                    stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }

                stateMsg.newStatus = BACT_STATUS_IDLE;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;

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

        if ( !getBACT_landingOnWait() )
        {
            _thraction = 0.0;
            _rotation *= mat3x3::RotateY(_maxrot * dtime);
        }
        else
        {
            _thraction = 0.0;

            _ufoBoost = 0;

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

void NC_STACK_ypaufo::User_layer(update_msg *arg)
{
    float dtime = arg->frameTime / 1000.0;

    int bactCollisions = getBACT_bactCollisions();

    _old_pos = _position;

    if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
    else if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
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

            _status_flg &= ~BACT_STFLAG_LAND;
        }
        else
        {
            TRayIntersection groundRay;
            groundRay.stPos = _position;
            groundRay.vect = vec3d(0.0, 0.0, 0.0);

            if ( _viewer_overeof <= _viewer_radius )
                groundRay.vect.y = _viewer_radius * 1.5;
            else
                groundRay.vect.y = _viewer_overeof * 1.5;

            groundRay.flags = 0;

            _world->IntersectRay(&groundRay);

            if ( !groundRay.isect || _thraction != 0.0 || _ufoBoost > _mass * 9.80665 )
            {
                _status_flg &= ~BACT_STFLAG_LAND;
            }
            else
            {
                _status_flg |= BACT_STFLAG_LAND;
                _fly_dir_length = 0;
                _thraction = 0;

                _ufoBoost = _mass * 9.80665;
            }



            if ( _primTtype != BACT_TGT_TYPE_CELL
                    || (_primTpos.XZ() - _position.XZ()).length() <= 800.0 )
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

        float yawAngle = -arg->inpt->Sliders[0] * _maxrot * dtime;

        if ( fabs(yawAngle) > 0.0 )
            _rotation *= mat3x3::RotateY(yawAngle);

        float pitchAngle = arg->inpt->Sliders[1] * _maxrot * dtime;

        if ( fabs(pitchAngle) > 0.0 )
            _rotation = mat3x3::RotateX(pitchAngle) * _rotation;

        if ( arg->inpt->Sliders[2] != 0.0 )
        {
            _ufoBoost = (arg->inpt->Sliders[2] * 4.0 + 1.0) * _mass * 9.80665;

            float altitude = _pSector->height - _position.y;
            float heightLeft = _height_max_user - altitude;
            float invMaxHeight = 1.0 / _height_max_user;

            float maxBoost = 9.80665 * _mass * 5.0 * heightLeft * invMaxHeight;

            if ( _ufoBoost > maxBoost )
                _ufoBoost = maxBoost;
        }
        else
        {
            _ufoBoost = _mass * 9.80665;
        }

        if ( bactCollisions )
        {
            if ( !(_status_flg & BACT_STFLAG_LAND) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        if ( arg->inpt->Buttons.Is(3) )
        {
            _thraction = 0;

            _ufoBoost = _mass * 9.80665;

            _fly_dir_length *= 0.7;

            if ( _fly_dir_length < 0.1 )
                _fly_dir_length = 0;
        }

        TMissileLaunch launch;

        launch.tgType = BACT_TGT_TYPE_DRCT;
        launch.tgt_pos = _rotation.AxisZ();

        TAimQuery aimQuery;
        aimQuery.flags = 5;
        aimQuery.aim_dir = _rotation.AxisZ();

        if ( UserTargeting(&aimQuery) )
        {
            launch.target.pbact = aimQuery.ret_bact;
            launch.tgType = BACT_TGT_TYPE_UNIT;
        }

        if ( arg->inpt->Buttons.IsAny({0, 1}) )
        {
            launch.weapon = _weapon;
            launch.direction = _rotation.AxisZ();
            launch.tgType = BACT_TGT_TYPE_NONE;
            launch.g_time = _clock;

            if ( _clock % 2 )
                launch.start_point.x = -_fire_pos.x;
            else
                launch.start_point.x = _fire_pos.x;

            launch.start_point.y = _fire_pos.y;
            launch.start_point.z = _fire_pos.z;
            launch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0);

            LaunchMissile(&launch);
        }

        if ( _weapon == -1 )
        {
            if ( arg->inpt->Buttons.IsAny({0, 1}) )
            {
                if ( _thraction < _force )
                {
                    _thraction += (float)arg->frameTime * _force * 0.0099999998;
                }
            }
            else if ( _thraction > 0.0 )
            {
                _thraction -= (float)arg->frameTime * _force * 0.001;
            }
            else
            {
                _thraction = 0;

                if ( arg->inpt->Sliders[2] == 0.0 )
                    _fly_dir_length *= 0.6;

                if ( _fly_dir_length < 0.1 )
                    _fly_dir_length = 0;
            }
        }
        else
        {
            _thraction = 0;

            if ( fabs(_fly_dir.x * _fly_dir_length) > 0.1 || fabs(_fly_dir.z * _fly_dir_length) > 0.1 )
                _fly_dir_length *= 0.6;

            if ( _fly_dir_length < 0.1 )
                _fly_dir_length = 0;
        }

        if ( _mgun != -1 )
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                // TODO(bug): Missing !. While fire is held this clears BACT_STFLAG_FIRE and the
                // block below sets it again, restarting the gun sound every frame, and
                // on release it stays set. The car and flyer test !Is(2) here.
                if ( arg->inpt->Buttons.Is(2) )
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

                TMinigunShot mgShot;

                mgShot.dir = _rotation.AxisZ();
                mgShot.period = dtime;
                mgShot.gTime = _clock;

                FireMinigun(&mgShot);
            }
        }

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            move_msg moveMsg;
            moveMsg.flag = 0;
            moveMsg.period = dtime;

            Move(&moveMsg);
        }
        else
        {
            TSphereContact contacts[10];

            for (int i = 3; i >= 0; i--)
            {
                move_msg moveMsg;
                moveMsg.flag = 0;
                moveMsg.period = dtime;

                Move(&moveMsg);

                TSphereCollisionQuery sphereQuery;
                sphereQuery.pos = _position;
                sphereQuery.dir = _fly_dir;
                sphereQuery.radius = _viewer_radius;
                sphereQuery.collisions = contacts;
                sphereQuery.coll_max = 10;
                sphereQuery.flags = 0;

                _world->FindSphereCollisions(&sphereQuery);

                int collided = 0;

                vec3d normalSum(0.0, 0.0, 0.0);

                if ( sphereQuery.coll_count )
                {


                    for (int j = 0 ; j < sphereQuery.coll_count; j++)
                        normalSum += sphereQuery.collisions[j].normal;

                    float normalLen = normalSum.length();

                    TCollisionNormal collNormal;

                    if ( normalLen != 0.0 )
                        collNormal.normal = normalSum / normalLen;
                    else
                        collNormal.normal = _fly_dir;

                    _thraction = 0;

                    Recoil(&collNormal);

                    collided = 1;
                }

                if ( collided == 0 )
                {
                    TRayIntersection moveRay;
                    moveRay.stPos = _old_pos;
                    moveRay.vect = _position - _old_pos;

                    moveRay.flags = 0;

                    _world->IntersectRay(&moveRay);

                    if ( moveRay.isect )
                    {
                        _thraction = 0;

                        TCollisionNormal collNormal;

                        collNormal.normal = moveRay.skel->polygons[moveRay.polyID].Normal();

                        Recoil(&collNormal);

                        collided = 1;
                    }
                }

                if ( collided == 0 )
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
                        ffEffect.pos_x = normalSum.x * 10.0 + _position.x;
                        ffEffect.pos_z = normalSum.z * 10.0 + _position.z;

                        _world->FFeedback_Play(&ffEffect);
                    }
                }
            }
        }
    }
}

void NC_STACK_ypaufo::Move(move_msg *arg)
{
    _old_pos = _position;

    float weight;

    if ( _status == BACT_STATUS_DEAD )
        weight = _mass * 39.2266;
    else
        weight = _mass * 9.80665;

    vec3d mv(0.0, 0.0, 0.0);

    if ( !(arg->flag & 1) )
    {
        vec3d az = _rotation.AxisZ().X0Z();
        float azLen = az.length();

        if ( azLen <= 0.001 )
            mv = -_rotation.AxisY().X0Z() * _ufoBoost;
        else
            mv = az * (_thraction / azLen);
    }

    float lift = 0.0;

    if ( !(arg->flag & 1) )
        lift = -1.0 * _ufoBoost;

    vec3d netForce = vec3d::OY(weight) + mv - _fly_dir * (fabs(_fly_dir_length) * _airconst) + vec3d::OY(lift);

    float forceLen = netForce.length();
    if ( forceLen > 0.0 )
    {
        vec3d velocity = _fly_dir * _fly_dir_length + netForce * (arg->period / _mass);

        float speed = velocity.length();
        if ( speed > 0.0 )
            velocity /= speed;

        _fly_dir = velocity;

        _fly_dir_length = speed;
    }

    vec3d moveStep = _fly_dir * (_fly_dir_length * arg->period * 6.0);

    if ( fabs(_fly_dir_length) > 0.1 )
        _position += moveStep;

    _ufoTogoRisidue -= moveStep.length();

    if ( _ufoTogoRisidue < 0.0 )
        _ufoTogoRisidue = 0;

    CorrectPositionInLevelBox(NULL);

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    float pitchMax;

    if ( _pitch_max <= -0.8 )
        pitchMax = 1.2;
    else
        pitchMax = _pitch_max;

    float pitchShift = fabs(_fly_dir_length) / (_force / _airconst_static) * pitchMax;

    if ( pitchShift > pitchMax )
        pitchShift = pitchMax;

    if ( _soundcarrier.Sounds[0].PSample )
        // TODO(bug): Sets the pitch with =, where the flyer, tank and bact add with +=,
        // so the base _pitch set above is replaced, not added to. In the first
        // decompile too, and yparobo does the same.
        _soundcarrier.Sounds[0].Pitch = (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * pitchShift;

}

size_t NC_STACK_ypaufo::SetPosition(TUnitPlacement *arg)
{
    if ( !NC_STACK_ypabact::SetPosition(arg))
        return 0;

    _ufoBoost = _mass * 9.80665;
    return 1;
}

void NC_STACK_ypaufo::Renew()
{
    NC_STACK_ypabact::Renew();

    _ufoTogo = 200.0;
    _ufoTogoRisidue = 0.0;
    _ufoBoost = 0.0;
    _ufoProcAngle = 0.0;
    _ufoFlags = 0;

    setBACT_landingOnWait(0);
}

void NC_STACK_ypaufo::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("ufoTogo", _ufoTogo);
    v.Field("ufoTogoResidue", _ufoTogoRisidue);
    v.Field("ufoBoost", _ufoBoost);
    v.Field("ufoProcAngle", _ufoProcAngle);
    v.Field("ufoFlags", _ufoFlags);
}


void NC_STACK_ypaufo::setBACT_inputting(bool inpt, World::Player *player)
{
    NC_STACK_ypabact::setBACT_inputting(inpt, player);

    if ( !getBACT_inputting() )
        _rotation = mat3x3::Ident();

}

void NC_STACK_ypaufo::setUFO_togo(int tog)
{
    _ufoTogo = tog;
}

int NC_STACK_ypaufo::getUFO_togo()
{
    return _ufoTogo;
}
