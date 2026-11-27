#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "yw.h"
#include "ypacar.h"
#include "yw_net.h"

#include "log.h"
#include "world/state.h"


NC_STACK_ypacar::NC_STACK_ypacar() 
{
    _carKamikaze = false;
    _carBlast = 0;
}
    
size_t NC_STACK_ypacar::Init(IDVList &stak)
{
    if ( !NC_STACK_ypatank::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_CAR;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case CAR_ATT_KAMIKAZE:
                setCAR_kamikaze(val.Get<int32_t>());
                break;

            case CAR_ATT_BLAST:
                setCAR_blast(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypacar::Deinit()
{
    return NC_STACK_ypatank::Deinit();
}

void NC_STACK_ypacar::Renew()
{
    NC_STACK_ypatank::Renew();

    _carKamikaze = false;
    _carBlast = 0;
}

void NC_STACK_ypacar::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypatank::VisitInternal(v);

    v.Field("carKamikaze", _carKamikaze);
    v.Field("carBlast", _carBlast);
}

size_t NC_STACK_ypacar::SetParameters(IDVList &stak)
{
    NC_STACK_ypatank::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case CAR_ATT_KAMIKAZE:
                setCAR_kamikaze(val.Get<int32_t>());
                break;

            case CAR_ATT_BLAST:
                setCAR_blast(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypacar::DoKamikaze()
{
    float blastRadius;

    if ( _carBlast >= 10000 )
        blastRadius = log(10000.0 / _carBlast) * -428.5714285714286;
    else
        blastRadius = 1.0;

    int radiusSteps = (blastRadius / 300.0);

    for (int i = 0; i < radiusSteps; i++)
    {
        // TODO(bug): A circle would be sqrt(r^2 - i^2); this is sqrt(i^2 + r^2), so the
        // blast covers a square-ish area wider than its radius. rowSectors
        // below has the same pattern.
        float rowSteps = sqrt( POW2(i) + POW2(radiusSteps) );

        for (int j = 0; j <= rowSteps; j++)
        {
            TSectorDamage sectorDamage;
            // TODO(bug): Treats each 300-unit step as a full 1200-unit sector, so sector
            // damage falls off 4x faster than the unit damage below. In the first
            // decompile too.
            sectorDamage.energy = exp( sqrt( POW2(i) + POW2(j) ) * (World::CVSectorLength * -2.8) / World::CVSectorLength ) * _carBlast;
            sectorDamage.pos = vec3d(i, 0, j) * 300.0 + _position.X0Z();
            sectorDamage.unit = this;

            if ( sectorDamage.pos.x > World::CVSectorLength  &&  sectorDamage.pos.x < _wrldSize.x - World::CVSectorLength  &&  sectorDamage.pos.z < -World::CVSectorLength  &&  sectorDamage.pos.z > _wrldSize.y + World::CVSectorLength )
                ChangeSectorEnergy(&sectorDamage);

            if ( i )
            {
                sectorDamage.pos.x = _position.x - i * 300.0;
                sectorDamage.pos.z = _position.z + j * 300.0;

                if ( sectorDamage.pos.x > World::CVSectorLength )
                {
                    if ( sectorDamage.pos.x < _wrldSize.x - World::CVSectorLength && sectorDamage.pos.z < -World::CVSectorLength && sectorDamage.pos.z > _wrldSize.y + World::CVSectorLength )
                        ChangeSectorEnergy(&sectorDamage);
                }
            }

            if ( j )
            {
                sectorDamage.pos.x = _position.x + i * 300.0;
                sectorDamage.pos.z = _position.z - j * 300.0;

                if ( sectorDamage.pos.x > World::CVSectorLength )
                {
                    if ( sectorDamage.pos.x < _wrldSize.x - World::CVSectorLength  &&  sectorDamage.pos.z < -World::CVSectorLength  &&  sectorDamage.pos.z > _wrldSize.y + World::CVSectorLength )
                        ChangeSectorEnergy(&sectorDamage);
                }
            }

            if ( i && j )
            {
                sectorDamage.pos.x = _position.x - i * 300.0;
                sectorDamage.pos.z = _position.z - j * 300.0;

                if ( sectorDamage.pos.x > World::CVSectorLength )
                {
                    if ( sectorDamage.pos.x < _wrldSize.x - World::CVSectorLength  &&  sectorDamage.pos.z < -World::CVSectorLength  &&  sectorDamage.pos.z > _wrldSize.y + World::CVSectorLength )
                        ChangeSectorEnergy(&sectorDamage);
                }
            }
        }
    }

    int radiusSectors = blastRadius / World::CVSectorLength;

    for (int i = 0; i < radiusSectors; i++)
    {
        int rowSectors = sqrt( POW2( i ) + POW2( radiusSectors ) );

        for (int j = 0; j <= rowSectors; j++ )
        {
            // TODO(bug): Only looks at _cellId + (i, j) with i, j >= 0, so units in the other
            // three quadrants are never hit. Under a 1200 blast radius radiusSectors
            // is 0 and no unit is hit at all.
            Common::Point pt = _cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                for ( NC_STACK_ypabact* &unit : _world->SectorAt(pt).unitsList )
                {
                    int netSent = 0;

                    if ( unit != this )
                    {
                        if ( unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            vec3d tmp = _position - unit->_position;

                            int damage = exp(tmp.length() * -2.8 / World::CVSectorLength) * _carBlast;
                            int realDamage = ((1.0 - (float)unit->_shield * 0.01) * (float)damage);

                            unit->_energy -= realDamage;

                            if ( _world->_isNetGame )
                            {
                                netSent = 1;

                                uamessage_vhclEnergy veMsg;
                                veMsg.msgID = UAMSG_VHCLENERGY;
                                veMsg.tstamp = _world->_timeStamp;
                                veMsg.owner = unit->_owner;
                                veMsg.id = unit->_gid;
                                veMsg.energy = -realDamage;

                                _world->NetBroadcastMessage(&veMsg, sizeof(veMsg), true);
                            }
                        }

                        if ( unit->_energy <= 0 || unit->_bact_type == BACT_TYPES_MISSLE )
                        {
                            setState_msg stateMsg;
                            stateMsg.setFlags = 0;
                            stateMsg.unsetFlags = 0;
                            stateMsg.newStatus = BACT_STATUS_DEAD;

                            if ( unit->_bact_type == BACT_TYPES_MISSLE )
                                unit->SetState(&stateMsg);
                            else
                                unit->SetStateInternal(&stateMsg);

                            if ( unit->_bact_type == BACT_TYPES_ROBO )
                            {
                                if ( !netSent )
                                {
                                    TRoboDestroyed roboDestroyed;
                                    roboDestroyed.bact = unit;
                                    roboDestroyed.killerOwner = _owner;

                                    _world->HandleRoboDestroyed(&roboDestroyed);
                                }
                            }
                        }
                    }
                }

            }
        }
    }

    _rotation.m21 = 1.9;

    vec3d az = _rotation.AxisZ();
    az.normalise();

    _rotation.SetZ(az);

    if ( fabs(_rotation.m22) <= 0.1 )
    {
        float m20 = _rotation.m20;

        if (isnormal(m20)) // not NULL, NAN, INF
        {
            _rotation.m02 = sqrt( 1.0 / (POW2(_rotation.m22) / (POW2(m20)) + 1.0) );
            _rotation.m00 = -_rotation.m22 * _rotation.m02 / m20;
        }
        else // emulate watcom div 0.0
        {
            _rotation.m02 = sqrt( 1.0 );
            _rotation.m00 = 0.0;
        }
        
    }
    else
    {
        float m22 = _rotation.m22;
        if (isnormal(m22)) // not NULL, NAN, INF
        {
            _rotation.m00 = sqrt( 1.0 / (POW2(_rotation.m20) / (POW2(m22)) + 1.0) );
            _rotation.m02 = -_rotation.m20 * _rotation.m00 / m22;
        }
        else // emulate watcom div 0.0
        {
            _rotation.m00 = sqrt( 1.0 );
            _rotation.m02 = 0.0;
        }
    }

    _rotation.m01 = 0.0;

    if ( _rotation.AxisZ().XZ().cross( _rotation.AxisX().XZ() ) > 0.0 )
    {
        _rotation.m00 = -_rotation.m00;
        _rotation.m02 = -_rotation.m02;
    }

    _rotation.SetY( _rotation.AxisZ() * _rotation.AxisX() );

    _fly_dir = -_rotation.AxisZ();

    _fly_dir_length = 200.0;
    _status_flg &= ~BACT_STFLAG_LAND;
    _bact_type = BACT_TYPES_FLYER;

    setState_msg stateMsg;
    stateMsg.newStatus = BACT_STATUS_DEAD;
    stateMsg.setFlags = 0;
    stateMsg.unsetFlags = 0;

    SetState(&stateMsg);

    _bact_type = BACT_TYPES_CAR;
    _energy = -10;
}

void NC_STACK_ypacar::User_layer(update_msg *arg)
{
    _airconst = _airconst_static;

    int bactCollisions = getBACT_bactCollisions();

    _old_pos = _position;

    float dtime = arg->frameTime / 1000.0;
    vec3d oldPos = _rotation.AxisZ();

    if (_status == BACT_STATUS_DEAD)
        DeadTimeUpdate(arg);
    else if (_status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE)
    {
        if ( _fly_dir_length != 0.0 )
        {
            if ( ! (_status_flg & BACT_STFLAG_FIRE) )
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

                if ( _status_flg & BACT_STFLAG_FIRE )
                {
                    _status = BACT_STATUS_IDLE;
                }
                else
                {
                    setState_msg stateMsg;
                    stateMsg.newStatus = BACT_STATUS_IDLE;
                    stateMsg.unsetFlags = 0;
                    stateMsg.setFlags = 0;

                    SetState(&stateMsg);
                }
            }
        }

        if ( arg->inpt->Sliders[3] != 0.0 )
        {
            if ( _fly_dir_length != 0.0 )
            {
                float speed = fabs(_fly_dir_length);
                float angle = -arg->inpt->Sliders[3] * _maxrot * dtime * (sqrt(speed) * 0.2);

                _rotation = mat3x3::RotateY(angle) * _rotation;
            }
        }

        if ( (_fly_dir_length < 0.0 && _thraction > 0.0) || (_fly_dir_length > 0.0 && _thraction < 0.0) )
        {
            if ( fabs(_fly_dir.y) > 0.1 )
            {
                float damping = 1.0 - (dtime * 4.0);

                if ( damping < 0.1 )
                    damping = 0.1;

                _fly_dir_length *= damping;
            }
        }

        float throttle = arg->inpt->Sliders[4];
        float throttleAbs = fabs(arg->inpt->Sliders[4]);

        if ( throttle < -1.0 )
            throttle = -1.0;
        else if ( throttle > 1.0)
            throttle = 1.0;

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

        TMissileLaunch launch;
        launch.tgType = BACT_TGT_TYPE_DRCT;
        launch.tgt_pos = _rotation.AxisZ();

        // Where its gun points, the same on any screen: the sights are drawn
        // there as each screen shows it
        TAimQuery aimQuery;
        aimQuery.flags = 5;
        aimQuery.aim_dir = _rotation.AxisZ() - vec3d::OY(_gun_angle_user);

        if ( UserTargeting(&aimQuery) )
        {
            launch.tgType = BACT_TGT_TYPE_UNIT;
            launch.target.pbact = aimQuery.ret_bact;
        }

        if ( arg->inpt->Buttons.IsAny({0, 1}) )
        {
            if ( _carKamikaze )
            {
                DoKamikaze();
            }
            else
            {
                launch.weapon = _weapon;
                launch.direction = _rotation.AxisZ() - _rotation.AxisY() * _gun_angle_user;
                launch.g_time = _clock;

                if ( _clock % 2 )
                    launch.start_point.x = -_fire_pos.x;
                else
                    launch.start_point.x = _fire_pos.x;

                launch.start_point.y = _fire_pos.y;
                launch.start_point.z = _fire_pos.z;

                launch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0) | 2;

                LaunchMissile(&launch);
            }
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
                mgShot.dir = _rotation.AxisZ() - _rotation.AxisY() * _gun_angle_user;
                mgShot.period = dtime;
                mgShot.gTime = _clock;

                FireMinigun(&mgShot);
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
                    float brakeDamping = 1.0 - dtime * 4.0;

                    if ( brakeDamping < 0.1 )
                        brakeDamping = 0.1;

                    _fly_dir_length *= brakeDamping * 0.1;
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

            // TODO(bug): Overwrites the flag set just above, so the handbrake flag 2
            // never reaches Move. The tank version doesn't do this. Could be in the
            // original too.
            moveMsg.flag = 0;
            moveMsg.period = dtime;

            if ( _status_flg & BACT_STFLAG_MOVE )
                Move(&moveMsg);

            if ( bactCollisions && CollisionWithBact(arg->frameTime) )
            {
                AlignVehicleUser(dtime, oldPos);
            }
            else
            {
                TRayIntersection moveRay;

                moveRay.stPos = _old_pos;
                moveRay.vect = _position - _old_pos;
                moveRay.flags = 0;

                _world->IntersectRay(&moveRay);

                if ( moveRay.isect )
                {

                    _position = _old_pos;

                    _fly_dir_length = 0;
                    _thraction = 0;
                }
                else
                {
                    AlignVehicleUser(dtime, oldPos);
                }
            }
        }
        else
        {
            TCrashLandStep crashStep;
            crashStep.flags = 1;
            crashStep.frameTime = arg->frameTime;

            CrashOrLand(&crashStep);
        }
    }
}

int NC_STACK_ypacar::AlignVehicleAI(float dtime, vec3d *pNormal)
{
    float groundOffset;

    if ( getBACT_viewer() )
        groundOffset = _viewer_overeof;
    else
        groundOffset = _overeof;

    TRayIntersection ray;
    ray.stPos = _rotation.AxisY() * groundOffset + _position - vec3d::OY(50.0);
    ray.vect = vec3d::OY( _overeof + 110.0 );
    ray.flags = 0;

    _world->IntersectRay(&ray);

    if ( !ray.isect )
        return ALIGN_NONE;

    vec3d normal = ray.skel->polygons[ ray.polyID ].Normal();

    if ( normal.y < 0.6 )
    {
        if (pNormal)
            *pNormal = normal;
        return ALIGN_NORMAL;
    }

    vec3d vaxis = _rotation.AxisY() * normal;

    if ( vaxis.normalise() > 0.0 )
    {
        float angle = clp_acos( _rotation.AxisY().dot( normal ) );
        float maxAngle = _maxrot * 2.0 * dtime;

        if ( angle > maxAngle )
            angle = maxAngle;

        if ( fabs(angle) < 0.01 )
            angle = 0.0;

        _rotation *= mat3x3::AxisAngle(vaxis, angle);
    }

    _position = ray.isectPos - _rotation.AxisY() * groundOffset;

    return ALIGN_DONE;
}

vec3d NC_STACK_ypacar::CarTip(float dtime, const vec3d &oldDir, vec3d rot)
{
    vec2d tmp = _rotation.AxisZ().XZ();
    vec2d varg = oldDir.XZ();

    float pitchScale;

    if ( getBACT_viewer() )
        pitchScale = -0.5;
    else
        pitchScale = -0.2;

    float netForce = _thraction - _airconst * _fly_dir_length;

    if ( fabs(netForce) > 0.1 )
    {
        float pitchAngle = pitchScale * netForce / _force;

        rot = mat3x3::AxisAngle(_rotation.AxisX(), pitchAngle).Transform(rot);
    }
    
    if (dtime != 0.0)
    {
        float tmpsq = tmp.length();
        float cosTurn = 0.0;        
        
        if (isnormal(tmpsq)) // Not NULL, NAN, INF
            cosTurn = tmp.dot( varg ) / tmpsq;

        tmpsq = varg.length();
        if (isnormal(tmpsq)) // Not NULL, NAN, INF
            cosTurn /= tmpsq;
    
        float rollAngle = clp_acos(cosTurn) * (fabs(_fly_dir_length) * 0.002 / dtime);

        if ( rollAngle > 0.001 )
        {
            if ( tmp.cross(varg) < 0.0 )
                rollAngle = -rollAngle;

            rot = mat3x3::AxisAngle(_rotation.AxisZ(), rollAngle).Transform(rot);
        }
    }
    
    return rot;
}

int NC_STACK_ypacar::AlignVehicleUser(float dtime, const vec3d &oldDir)
{
    float rayStartScale = 1.73;
    float rayLenScale = 1.7;
    int rightMissed = 0;
    int trailMissed = 0;
    int leftMissed = 0;

    bool isViewer = getBACT_viewer();
    int isInputting = getBACT_inputting();

    float groundOffset;

    if ( isViewer )
        groundOffset = _viewer_overeof;
    else
        groundOffset = _overeof;

    vec3d t1, t2, t3;
    float moveSign;

    if ( _fly_dir_length < 0.0 )
    {
        t1 = _position + (_rotation.AxisX() - _rotation.AxisZ()) * _viewer_radius * 0.7071;

        t2 = _position + _rotation.AxisZ() * _viewer_radius;

        t3 = _position + (-_rotation.AxisX() - _rotation.AxisZ()) * _viewer_radius * 0.7071;

        moveSign = -1.0;
    }
    else
    {
        t1 = _position + (_rotation.AxisX() + _rotation.AxisZ()) * _viewer_radius * 0.7071;

        t2 = _position - _rotation.AxisZ() * _viewer_radius;

        t3 = _position + (-_rotation.AxisX() + _rotation.AxisZ()) * _viewer_radius * 0.7071;

        moveSign = 1.0;
    }

    t1 += vec3d::OY(_viewer_overeof);
    t2 += vec3d::OY(_viewer_overeof);
    t3 += vec3d::OY(_viewer_overeof);

    TRayIntersection rightRay, trailRay, leftRay;

    rightRay.stPos = _position - vec3d::OY(rayStartScale * _viewer_radius);
    rightRay.vect = (t1 - rightRay.stPos) * rayLenScale;
    rightRay.flags = 0;

    trailRay.stPos = _position - vec3d::OY(rayStartScale * _viewer_radius);
    trailRay.vect = (t2 - trailRay.stPos) * rayLenScale;
    trailRay.flags = 0;

    leftRay.stPos = _position - vec3d::OY(rayStartScale * _viewer_radius);
    leftRay.vect = (t3 - leftRay.stPos) * rayLenScale;
    leftRay.flags = 0;

    _world->IntersectRay(&rightRay);

    if ( rightRay.isect )
    {
        UAskeleton::Polygon *poly = &rightRay.skel->polygons[ rightRay.polyID ];

        if ( fabs(poly->B) < 0.6 )
        {
            float wallDot = _rotation.AxisZ().dot( poly->Normal() ) * moveSign;

            if ( wallDot <= 0.0 )
            {
                _thraction = 0;
            }
            else
            {
                if ( wallDot >= 0.82 )
                {
                    _thraction = 0;
                    _fly_dir_length = 0;
                    _position = _old_pos;
                }
                else
                {
                    _position.x = _old_pos.x - poly->A * 10.0;
                    _position.z = _old_pos.z - poly->C * 10.0;
                }

                if ( _fly_dir_length > 2.333333333333334 )
                {
                    SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                    if ( isInputting )
                    {
                        TFFeedbackEffect ffEffect;

                        ffEffect.pos_x = rightRay.isectPos.x;
                        ffEffect.pos_z = rightRay.isectPos.z;
                        ffEffect.effects_type = 5;
                        ffEffect.strength = 1.0;

                        _world->FFeedback_Play(this, &ffEffect);
                    }
                }
            }
            return 2;
        }
    }
    else
    {
        if ( !isInputting )
            return 2;

        rightRay.isectPos = _position + rightRay.vect - vec3d::OY(rayStartScale * _viewer_radius);

        rightMissed = 1;
    }

    _world->IntersectRay(&trailRay);

    if ( trailRay.isect )
    {
        UAskeleton::Polygon *poly = &trailRay.skel->polygons[ trailRay.polyID ];

        if ( fabs(poly->B) < 0.6 )
        {
            float wallDot = _rotation.AxisZ().dot( poly->Normal() ) * moveSign;

            if ( wallDot <= 0.0 )
            {
                _thraction = 0;
            }
            else
            {
                if ( wallDot >= 0.82 )
                {
                    _fly_dir_length = 0;
                    _position = _old_pos;
                    _thraction = 0;
                }
                else
                {
                    _position.x = _old_pos.x - poly->A * 10.0;
                    _position.z = _old_pos.z - poly->C * 10.0;
                }

                if ( _fly_dir_length > 2.333333333333334 )
                {
                    SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                    if ( isInputting )
                    {
                        TFFeedbackEffect ffEffect;
                        ffEffect.strength = 1.0;
                        ffEffect.pos_x = trailRay.isectPos.x;
                        ffEffect.pos_z = trailRay.isectPos.z;
                        ffEffect.effects_type = 5;

                        _world->FFeedback_Play(this, &ffEffect);
                    }
                }
            }

            return 1;
        }
    }
    else
    {
        if ( !isInputting )
            return 1;

        trailRay.isectPos = _position + trailRay.vect - vec3d::OY(rayStartScale * _viewer_radius);

        trailMissed = 1;
    }

    _world->IntersectRay(&leftRay);

    if ( leftRay.isect )
    {
        UAskeleton::Polygon *poly = &leftRay.skel->polygons[ leftRay.polyID ];

        if ( fabs(poly->B) < 0.6 )
        {
            _position += _fly_dir * _fly_dir_length * dtime * 6.0;

            _thraction = 0;

            if ( _fly_dir_length < -2.333333333333334 )
            {
                SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                if ( isInputting )
                {
                    TFFeedbackEffect ffEffect;
                    ffEffect.strength = 1.0;
                    ffEffect.pos_x = leftRay.isectPos.x;
                    ffEffect.pos_z = leftRay.isectPos.z;
                    ffEffect.effects_type = 5;

                    _world->FFeedback_Play(this, &ffEffect);
                }
            }
            return 3;
        }
    }
    else
    {
        leftRay.isectPos = _position + leftRay.vect - vec3d::OY(rayStartScale * _viewer_radius);

        leftMissed = 1;
    }

    if ( isViewer )
    {
        if ( moveSign > 0.0 )
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
                        SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                        if ( isInputting )
                        {
                            TFFeedbackEffect ffEffect;
                            ffEffect.strength = 1.0;
                            ffEffect.pos_x = frontRay.isectPos.x;
                            ffEffect.pos_z = frontRay.isectPos.z;
                            ffEffect.effects_type = 5;

                            _world->FFeedback_Play(this, &ffEffect);
                        }
                    }

                    _thraction = 0;
                    _fly_dir_length = 0;
                    _position = _old_pos;
                }
            }
        }
    }

    if ( trailRay.isect && rightRay.isect && leftRay.isect )
    {
        vec3d tmp = rightRay.isectPos - leftRay.isectPos;
        vec3d tmp2 = trailRay.isectPos - leftRay.isectPos;

        vec3d tmp3 = tmp * tmp2;

        float normalLen = tmp3.length() * moveSign;
        if ( normalLen != 0.0 )
            tmp3 /= normalLen;
        else
            tmp3 = vec3d::OY(1.0);

        if ( tmp3.y < -0.1 )
            tmp3 = -tmp3;

        int tipEnabled = getTANK_tip();

        if ( tipEnabled )
            tmp3 = CarTip(dtime, oldDir, tmp3);


        float speed = fabs(_fly_dir_length);

        vec3d vaxis = _rotation.AxisY() * tmp3;

        float minAngle;

        if ( speed >= 0.5 )
            minAngle = 0.007;
        else
            minAngle = 0.01;

        if ( vaxis.normalise() > 0.0 )
        {
            float angle = clp_acos( _rotation.AxisY().dot( tmp3 ) );
            float maxrot = _maxrot * 2.0 * dtime;

            if ( angle > maxrot )
                angle = maxrot;

            if ( fabs(angle) > minAngle )
                _rotation *= mat3x3::AxisAngle(vaxis, angle);
        }
    }

    if ( !rightRay.isect && !trailRay.isect && !leftRay.isect )
    {
        _status_flg &= ~BACT_STFLAG_LAND;
        return 0;
    }

    if ( !rightMissed && !trailMissed && !leftMissed )
    {
        _position.y = (rightRay.isectPos.y + trailRay.isectPos.y + leftRay.isectPos.y) * 0.33333334 - groundOffset;
        return 0;
    }

    float probeHalfLen = _viewer_overeof * rayLenScale * 0.8;

    TRayIntersection downRay;
    downRay.stPos = _position - vec3d::OY(probeHalfLen);
    downRay.vect = vec3d::OY(probeHalfLen * 2.0);
    downRay.flags = 0;

    _world->IntersectRay(&downRay);

    if ( downRay.isect && (!downRay.isect || downRay.skel->polygons[ downRay.polyID ].B >= 0.6) )
    {
        _position.y = downRay.isectPos.y - groundOffset;
        return 0;
    }

    vec3d tf(0.0, 0.0, 0.0);

    _status_flg &= ~BACT_STFLAG_LAND;

    if ( rightMissed )
        tf += _rotation.AxisZ() * moveSign - _rotation.AxisX();

    if ( trailMissed )
        tf += _rotation.AxisZ() * moveSign + _rotation.AxisX();

    if ( leftMissed )
        tf -= _rotation.AxisZ() * moveSign;

    _position += tf * (dtime * 400.0);

    return 0;
}


void NC_STACK_ypacar::setCAR_kamikaze(bool kam)
{
    _carKamikaze = kam;
}

void NC_STACK_ypacar::setCAR_blast(int blast)
{
    _carBlast = blast;
}


bool NC_STACK_ypacar::getCAR_kamikaze()
{
    return _carKamikaze;
}

int NC_STACK_ypacar::getCAR_blast()
{
    return _carBlast;
}


