#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "yw.h"
#include "ypagun.h"
#include "yparobo.h"
#include "world/state.h"

size_t NC_STACK_ypagun::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_GUN;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case GUN_ATT_SIDEANGLE:
                setGUN_sideAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_UPANGLE:
                setGUN_upAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_DOWNANGLE:
                setGUN_downAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_FIRETYPE:
                setGUN_fireType(val.Get<int32_t>());
                break;

            case GUN_ATT_FIRETIME:
                setGUN_fireTime(val.Get<int32_t>());
                break;

            case GUN_ATT_SETGROUND:
                setGUN_setGround ( val.Get<int32_t>() );
                break;

            case GUN_ATT_ROBOGUN:
                setGUN_roboGun ( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypagun::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_ypagun::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case GUN_ATT_SIDEANGLE:
                setGUN_sideAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_UPANGLE:
                setGUN_upAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_DOWNANGLE:
                setGUN_downAngle(val.Get<int32_t>());
                break;

            case GUN_ATT_FIRETYPE:
                setGUN_fireType(val.Get<int32_t>());
                break;

            case GUN_ATT_FIRETIME:
                setGUN_fireTime(val.Get<int32_t>());
                break;

            case GUN_ATT_SETGROUND:
                setGUN_setGround ( val.Get<int32_t>() );
                break;

            case GUN_ATT_ROBOGUN:
                setGUN_roboGun ( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

bool NC_STACK_ypagun::CheckPedestal()
{
    TRayIntersection ray;
    ray.stPos = _position;
    ray.vect.x = 0;

    if ( _gunFlags & GUN_FLAGS_FALLDOWN )
        ray.vect.y = -_height;
    else
        ray.vect.y = _height;

    ray.vect.z = 0;
    ray.flags = 0;

    _world->IntersectRay(&ray);

    return ray.isect != 0;
}

void NC_STACK_ypagun::AI_layer3(update_msg *arg)
{
    float fTime = arg->frameTime / 1000.0;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    case BACT_STATUS_IDLE:
    {
        if ( !(_gunFlags & GUN_FLAGS_ROBO) )
        {
            if ( _clock - _gunDownTime > 800 )
            {
                _gunDownTime = _clock;

                if ( !CheckPedestal() )
                {
                    _energy = -10;
                    break;
                }
            }
        }

        if ( _gunType == GUN_TYPE_REAL && _gunFireCount > 0 )
        {
            _gunFireCount -= arg->frameTime;

            if ( _gunFireCount <= 0 )
            {
                _gunFireCount = 0;

                setState_msg stateMsg;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);
            }
        }

        if ( _secndTtype != BACT_TGT_TYPE_UNIT )
        {
            if ( !_secndTtype && _gunType == GUN_TYPE_PROTO )
            {
                if ( _status_flg & BACT_STFLAG_FIRE )
                {
                    setState_msg stateMsg;
                    stateMsg.unsetFlags = BACT_STFLAG_FIRE;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.setFlags = 0;

                    SetState(&stateMsg);
                }
            }
            break;
        }

        vec3d vTgt = _secndT.pbact->_position - _position;

        float dist = vTgt.length();

        if ( dist <= 0.001 )
            break;

        vTgt /= dist;

        //vec3d norm = ypagun.basis * ypagun.rott;

        float xzAngle = 0.0; // Current
        float xzWanted = 0.0; // Wanted

        // local X coordinate axis. Used for calculate rotation on XZ-plane
        // because always lie on XZ-plane
        vec3d lx = _gunRott * _gunBasis;

        // Calculate projection gun rotation X-axis onto XZ basis plane
        vec2d xzRot( _rotation.AxisX().dot(lx),
                     _rotation.AxisX().dot(_gunBasis));

        // Normolise for unit circle.
        // If it's too short - X-rotation-axis near -/+rott axis,
        if (xzRot.normalise() > 0.001)
            xzAngle = xzRot.xyAngle();

        vec2d xzWant( vTgt.dot(_gunBasis),
                      vTgt.dot(-lx) );

        if ( xzWant.normalise() > 0.001 )
            xzWanted = xzWant.xyAngle();
        else
            xzWanted = xzAngle;

        if ( _gunMaxSide <= 3.1 )
        {
            if ( xzWanted < -_gunMaxSide )
                xzWanted = -_gunMaxSide;

            if ( xzWanted > _gunMaxSide )
                xzWanted = _gunMaxSide;
        }

        float xz_delta = xzWanted - xzAngle;

        if ( _gunMaxSide > 3.1 )
        {
            if ( fabs(xz_delta) > C_PI )
            {
                if ( xz_delta < -C_PI )
                    xz_delta += C_2PI;

                if ( xz_delta > C_PI )
                    xz_delta -= C_2PI;
            }
        }

        float MaxRot = _maxrot * fTime;

        if ( xz_delta >= 0.0 )
        {
            if ( xz_delta > MaxRot )
                xz_delta = MaxRot;
        }
        else
        {
            if ( xz_delta < -MaxRot )
                xz_delta = -MaxRot;
        }

        if ( fabs(xz_delta) > 0.001 )
            _rotation *= mat3x3(_gunRott, xz_delta);

        vec3d invRed = -_gunRott;

        float y_angle = clp_asin( invRed.dot(_rotation.AxisZ()) );

        float y_want = clp_asin( invRed.dot( vTgt ) );

        if ( y_want > _gunMaxUp )
            y_want = _gunMaxUp;

        if ( y_want < -_gunMaxDown )
            y_want = -_gunMaxDown;

        float y_delta = y_want - y_angle;

        if ( y_delta > MaxRot )
            y_delta = MaxRot;
        else if ( y_delta < -MaxRot )
            y_delta = -MaxRot;

        if ( fabs(y_delta) > 0.001 )
            _rotation = mat3x3::RotateX(y_delta * 0.3) * _rotation;

        TFightTarget fightTarget;
        fightTarget.fperiod = fTime;
        fightTarget.g_time = _clock;
        fightTarget.target.pbact = _secndT.pbact;

        FightWithBact(&fightTarget);

        if ( !TestTargetSector(_secndT.pbact) )
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 1;

            SetTarget(&targetMsg);

            if ( _gunType == GUN_TYPE_PROTO )
            {
                setState_msg stateMsg;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);
            }
        }
    }
    break;

    case BACT_STATUS_DEAD:
        DeadTimeUpdate(arg);
        break;

    case BACT_STATUS_CREATE:
        _scale_time -= arg->frameTime;

        if ( _scale_time <= 0 )
        {
            setState_msg stateMsg;
            stateMsg.newStatus = BACT_STATUS_NORMAL;
            stateMsg.setFlags = 0;
            stateMsg.unsetFlags = 0;

            SetState(&stateMsg);
        }
        break;

    default:
        break;
    }
}

void NC_STACK_ypagun::User_layer(update_msg *arg)
{
    float fTime = arg->frameTime / 1000.0;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _gunFlags & GUN_FLAGS_ROBO || CheckPedestal() )
        {
            if ( _viewer_position.length() >= 3.0 )
            {
                float damping = arg->frameTime + 50.0;

                _viewer_position *= 50.0 / damping;
            }
            else
            {
                _viewer_position = 0;
            }

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                if ( !arg->inpt->Buttons.IsAny({0, 1}) )
                {
                    setState_msg stateMsg;
                    stateMsg.setFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }
            }

            TMissileLaunch launch;
            launch.tgType = BACT_TGT_TYPE_CELL;
            launch.tgt_pos = _position + _rotation.AxisZ() * (World::CVSectorLength * 3.0);

            TAimQuery aimQuery;
            aimQuery.aim_dir = _rotation.AxisZ();
            aimQuery.flags = 5;

            if ( UserTargeting(&aimQuery) )
            {
                launch.target.pbact = aimQuery.ret_bact;
                launch.tgType = BACT_TGT_TYPE_UNIT;
            }

            if ( arg->inpt->Buttons.IsAny({0, 1}) )
            {
                if ( _gunType == GUN_TYPE_REAL )
                {
                    launch.weapon = _weapon;
                    launch.direction = _rotation.AxisZ();
                    launch.g_time = _clock;
                    launch.start_point = _fire_pos;
                    launch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0) | 2;

                    if ( LaunchMissile(&launch) )
                    {
                        _viewer_position = vec3d(0, 0, -25.0);
                    }
                }
                else if ( _gunType == GUN_TYPE_PROTO )
                {
//            if ( ~(ypabact.field_3D6 & 0x100) ) CHECK IT
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        setState_msg stateMsg;
                        stateMsg.unsetFlags = 0;
                        stateMsg.newStatus = BACT_STATUS_NOPE;
                        stateMsg.setFlags = BACT_STFLAG_FIRE;

                        SetState(&stateMsg);
                    }

                    TMinigunShot shot;
                    shot.dir = _rotation.AxisZ();
                    shot.period = fTime;
                    shot.gTime = _clock;

                    FireMinigun(&shot);

                    if ( _gunFlags & GUN_FLAGS_SHOT )
                    {
                        _gunFlags &= ~GUN_FLAGS_SHOT;
                    }
                    else
                    {
                        _gunFlags |= GUN_FLAGS_SHOT;

                        _viewer_position = vec3d(0, -20.0, -30.0);
                    }
                }
            }

            float yRot = arg->inpt->Sliders[0] * _maxrot * fTime;

            if ( fabs( yRot ) > 0.001 )
            {
                vec3d tmp = _gunRott * _gunBasis;

                float yAngle = clp_acos( tmp.dot( _rotation.AxisX() ) );

                if ( tmp.x * _rotation.m02 - tmp.z * _rotation.m00 > 0.0 )
                    yAngle = -yAngle;

                if ( yAngle + yRot < -_gunMaxSide )
                    yRot = -_gunMaxSide - yAngle;

                if ( yAngle + yRot > _gunMaxSide )
                    yRot = _gunMaxSide - yAngle;

                _rotation *= mat3x3(_gunRott, yRot, MAT_FLAG_INV_SIN);
            }

            float xRot = arg->inpt->Sliders[1] * _maxrot * fTime;

            if ( fabs(xRot) > 0.001 )
            {
                vec3d tmp = -_gunRott;
                float xAngle = clp_asin( tmp.dot( _rotation.AxisZ() ) );

                if ( xAngle + xRot < -_gunMaxDown )
                    xRot = -_gunMaxDown - xAngle;

                if ( xAngle + xRot > _gunMaxUp )
                    xRot = _gunMaxUp - xAngle;

                _rotation = mat3x3::RotateX(xRot) * _rotation;
            }

            _viewer_rotation = _rotation;
        }
        else
        {
            _energy = -10;
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
        DeadTimeUpdate(arg);
}

void NC_STACK_ypagun::FightWithBact(TFightTarget *arg)
{
    vec3d vTgt = arg->target.pbact->_position - _position;

    float dist = vTgt.length();

    if ( dist < 0.001 )
        return;

    vTgt /= dist;

    if ( dist <= 2400.0 && vTgt.dot( _rotation.AxisZ() ) >= 0.95 )
    {
        if ( _gunType == GUN_TYPE_REAL )
        {
            TMissileLaunch launch;
            launch.direction = vTgt;
            launch.start_point = _fire_pos;
            launch.tgType = BACT_TGT_TYPE_UNIT;
            launch.target.pbact = arg->target.pbact;
            launch.weapon = _weapon;
            launch.g_time = arg->g_time;
            launch.flags = (!getBACT_inputting() ? 4 : 0);

            if ( LaunchMissile(&launch) )
            {
                _gunFireCount = _gunFireTime;

                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
        }
        else if ( _gunType == GUN_TYPE_PROTO )
        {
            if ( !(_status_flg & BACT_STFLAG_FIRE) )
            {
                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }

            TMinigunShot shot;
            shot.period = arg->fperiod;
            shot.gTime = arg->g_time;
            shot.dir = _rotation.AxisZ();

            FireMinigun(&shot);
        }
    }
    else if ( _gunType == GUN_TYPE_PROTO )
    {
        setState_msg stateMsg;
        stateMsg.setFlags = 0;
        stateMsg.newStatus = BACT_STATUS_NOPE;
        stateMsg.unsetFlags = BACT_STFLAG_FIRE;

        SetState(&stateMsg);
    }
}

void NC_STACK_ypagun::Die()
{
    if ( !(_status_flg & BACT_STFLAG_DEATH1) )
    {
        NC_STACK_ypabact::Die();

        int visMode = 1;
        HandleVisChildrens(&visMode);

        if ( (_gunFlags & GUN_FLAGS_ROBO) && _host_station )
        {
            for (World::TRoboGun &gun : _host_station->GetGuns())
            {
                if ( this == gun.gun_obj )
                    gun.gun_obj = NULL;
            }
        }
    }
}

size_t NC_STACK_ypagun::SetPosition(TUnitPlacement *arg)
{
    if ( !NC_STACK_ypabact::SetPosition(arg) )
        return 0;

    if ( _gunFlags & GUN_FLAGS_GROUND )
    {
        TRayIntersection ray;
        ray.stPos.x = arg->pos.x;
        ray.stPos.y = arg->pos.y + -10000.0;
        ray.stPos.z = arg->pos.z;
        ray.vect.x = 0;
        ray.vect.y = 20000.0;
        ray.vect.z = 0;
        ray.flags = 0;

        _world->IntersectRay(&ray);

        if ( ray.isect )
            _position.y = ray.isectPos.y - _overeof;

        SetGunBasis(vec3d(0, 0, 1.0), false);
    }
    else if ( !(arg->flags & 4) )
    {
        _position.y = arg->pos.y + _pSector->height;
    }

    _old_pos = _position;

    int visMode = 2;
    HandleVisChildrens(&visMode);

    return 1;
}

void NC_STACK_ypagun::EnergyInteract(update_msg *arg)
{
    if ( _status != BACT_STATUS_DEAD )
    {
        if ( _pSector->owner == _owner )
            _energy += _energy_max * (arg->frameTime / 1000.0) * _pSector->energy_power / 40000.0;

        if ( _energy > _energy_max )
            _energy = _energy_max;
    }
}

void NC_STACK_ypagun::Renew()
{
    NC_STACK_ypabact::Renew();

    _gunMaxUp = 0.0;
    _gunMaxDown = 0.0;
    _gunMaxSide = 0.0;
    _gunBasis = vec3d();
    _gunRott = vec3d();
    _gunType = GUN_TYPE_REAL;
    _gunFireTime = 100;
    _gunFireCount = 0;
    _gunFlags = 0;
    _gunDownTime = 0;

    setBACT_extraViewer(true);

    _viewer_position = vec3d(0, 0, 0);

    _viewer_rotation = _rotation;
}

void NC_STACK_ypagun::VisitView(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitView(v);

    // Whether it's one of a host station's, which the map leaves out
    v.Field("gunFlags", _gunFlags, World::State::VIEW);
}

void NC_STACK_ypagun::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("gunMaxUp", _gunMaxUp);
    v.Field("gunMaxDown", _gunMaxDown);
    v.Field("gunMaxSide", _gunMaxSide);
    v.Field("gunBasis", _gunBasis);
    v.Field("gunRott", _gunRott);
    v.Field("gunType", _gunType);
    v.Field("gunFireTime", _gunFireTime);
    v.Field("gunFireCount", _gunFireCount);
    v.Field("gunDownTime", _gunDownTime);
}

bool NC_STACK_ypagun::TestTargetSector(const NC_STACK_ypabact * cel_unit) const
{
    vec3d vTgt = cel_unit->_position - _position;

    float dist = vTgt.length();

    if ( dist < 1.0 )
        return 1;

    if ( dist > 2400.0 )
        return 0;

    vTgt /= dist;

    if ( C_PI_2 - clp_acos( vTgt.dot(-_gunRott) ) > _gunMaxUp )
        return 0;

    if ( C_PI_2 - clp_acos( vTgt.dot(_gunRott) ) > _gunMaxDown )
        return 0;

    vec3d tmp = vTgt - _gunRott * vTgt.dot(_gunRott);

    float ln = tmp.length();

    if ( ln <= 0.1 )
        return 1;

    if ( clp_acos( tmp.dot(_gunBasis) / ln ) <= _gunMaxSide )
        return 1;

    return 0;
}

void NC_STACK_ypagun::SetGunBasis(const vec3d &_basis, bool directDown)
{
    vec3d basis = _basis;
    float ln = basis.length();

    if ( ln > 0.001 )
        basis /= ln;

    _gunBasis = basis;
    _rotation.SetZ( basis );

    if ( basis.y != 0.0 )
    {
        if ( basis.x != 0.0 || basis.z != 0.0 )
        {
            float invSlope = -1.0 / ( basis.y / basis.XZ().length() );

            _rotation.m11 = sqrt(POW2(invSlope) / (POW2(invSlope) + 1.0));

            if ( basis.x != 0.0 )
            {
                float xzLenSq = 1.0 - POW2(_rotation.m11);
                _rotation.m10 = sqrt( xzLenSq / (POW2(basis.z) / (POW2(basis.x)) + 1.0) );
                _rotation.m12 = sqrt( xzLenSq - POW2(_rotation.m10) );
            }
            else
            {
                float xzLenSq = 1.0 - POW2(_rotation.m11);
                _rotation.m12 = sqrt( xzLenSq / (POW2(basis.x) / (POW2(basis.z)) + 1.0) );
                _rotation.m10 = sqrt( xzLenSq - POW2(_rotation.m12) );
            }

            if ( basis.x < 0.0 )
                _rotation.m10 = -_rotation.m10;

            if ( basis.z < 0.0 )
                _rotation.m12 = -_rotation.m12;

            if ( basis.y > 0.0 )
            {
                _rotation.m10 = -_rotation.m10;
                _rotation.m12 = -_rotation.m12;
            }
        }
        else
        {
            _rotation.SetY( vec3d(0.0, 0.0, 1.0) );
        }
    }
    else
    {
        _rotation.SetY( vec3d(0.0, 1.0, 0.0) );
    }

    if ( directDown )
    {
        _gunFlags |= GUN_FLAGS_FALLDOWN;
        _rotation.SetY( -_rotation.AxisY() );
    }

    _rotation.SetX( _rotation.AxisY() * _rotation.AxisZ() );

    _gunRott = _rotation.AxisY();
}


vec3d NC_STACK_ypagun::RotateGunBasis(const vec3d &axis, float angle)
{
    _gunBasis = mat3x3(axis, -angle, MAT_FLAG_INV_SIN).Transform(_gunBasis);

    _rotation *= mat3x3(axis, angle, MAT_FLAG_INV_SIN);

    return _gunBasis;
}


void NC_STACK_ypagun::setGUN_sideAngle(int angl)
{
    _gunMaxSide = angl / 1000.0;
}

void NC_STACK_ypagun::setGUN_upAngle(int angl)
{
    _gunMaxUp = angl / 1000.0;
}

void NC_STACK_ypagun::setGUN_downAngle(int angl)
{
    _gunMaxDown = angl / 1000.0;
}

void NC_STACK_ypagun::setGUN_fireType(int tp)
{
    _gunType = tp;
}

void NC_STACK_ypagun::setGUN_fireTime(int time)
{
    _gunFireTime = time;
}

void NC_STACK_ypagun::setGUN_setGround(int grnd)
{
    if ( grnd )
        _gunFlags |= GUN_FLAGS_GROUND;
    else
        _gunFlags &= ~GUN_FLAGS_GROUND;
}

void NC_STACK_ypagun::setGUN_roboGun(int rbo)
{
    if ( rbo )
        _gunFlags |= GUN_FLAGS_ROBO;
    else
        _gunFlags &= ~GUN_FLAGS_ROBO;
}



int NC_STACK_ypagun::getGUN_sideAngle()
{
    return _gunMaxSide * 1000.0;
}

int NC_STACK_ypagun::getGUN_upAngle()
{
    return _gunMaxUp * 1000.0;
}

int NC_STACK_ypagun::getGUN_downAngle()
{
    return _gunMaxDown * 1000.0;
}

int NC_STACK_ypagun::getGUN_fireType()
{
    return _gunType;
}

int NC_STACK_ypagun::getGUN_fireTime()
{
    return _gunFireTime;
}

bool NC_STACK_ypagun::getGUN_setGround()
{
    return (_gunFlags & GUN_FLAGS_GROUND) != 0;
}

bool NC_STACK_ypagun::IsRoboGun()
{
    return (_gunFlags & GUN_FLAGS_ROBO) != 0;
}

