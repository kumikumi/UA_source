#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "log.h"
#include "yw.h"
#include "yparobo.h"

#include "yw_net.h"
#include "fmtlib/printf.h"

#include <math.h>
#include "world/state.h"


const std::array<Common::Point, 8> NearDxy 
{{
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
    { 1,  0}, {-1, 1}, {0,  1}, { 1, 1}
}};

size_t NC_STACK_yparobo::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _roboEpConquer = 30;
    _roboEpDefense = 90;
    _roboEpRadar = 30;
    _roboEpPower = 50;
    _roboEpSafety = 50;
    _roboEpRecon = 80;
    _roboEpChangePlace = 100;
    _roboWFlags = 0;
    _roboFillMode = 15;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {

            case ROBO_ATT_EP_CONQUER:
                setROBO_epConquer(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_DEFENSE:
                setROBO_epDefense(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RADAR:
                setROBO_epRadar(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_POWER:
                setROBO_epPower(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_SAFETY:
                setROBO_epSafety(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_VEHICLE:
                setROBO_battVehicle(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_BEAM:
                setROBO_battBeam(val.Get<int32_t>());
                break;

            case ROBO_ATT_FILL_MODE:
                setROBO_fillMode(val.Get<int32_t>());
                break;

            case ROBO_ATT_WAIT_SWAY:
                setROBO_waitSway( val.Get<int32_t>() );
                break;

            case ROBO_ATT_WAIT_ROTATE:
                setROBO_waitRotate( val.Get<int32_t>() );
                break;

            case ROBO_ATT_EP_CHANGEPLACE:
                setROBO_epChangeplace(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RECONNOITRE:
                setROBO_epReconnoitre(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_ROBO:
                setROBO_epRobo(val.Get<int32_t>());
                break;

            case ROBO_ATT_VIEWANGLE:
                setROBO_viewAngle(val.Get<int32_t>());
                break;

            case ROBO_ATT_SAFDELAY:
                setROBO_safDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_POWDELAY:
                setROBO_powDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RADDELAY:
                setROBO_radDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CPLDELAY:
                setROBO_cplDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_DEFDELAY:
                setROBO_defDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CONDELAY:
                setROBO_conDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_ROBDELAY:
                setROBO_robDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RECDELAY:
                setROBO_recDelay(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    _bact_type = BACT_TYPES_ROBO;

    return 1;
}

size_t NC_STACK_yparobo::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_yparobo::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {

            case ROBO_ATT_PROTO:
                setROBO_proto(val.Get<World::TRoboProto *>());
                break;

            case ROBO_ATT_EP_CONQUER:
                setROBO_epConquer(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_DEFENSE:
                setROBO_epDefense(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RADAR:
                setROBO_epRadar(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_POWER:
                setROBO_epPower(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_SAFETY:
                setROBO_epSafety(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_VEHICLE:
                setROBO_battVehicle(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_BEAM:
                setROBO_battBeam(val.Get<int32_t>());
                break;

            case ROBO_ATT_FILL_MODE:
                setROBO_fillMode(val.Get<int32_t>());
                break;

            case ROBO_ATT_WAIT_SWAY:
                setROBO_waitSway( val.Get<int32_t>() );
                break;

            case ROBO_ATT_WAIT_ROTATE:
                setROBO_waitRotate( val.Get<int32_t>() );
                break;

            case ROBO_ATT_EP_CHANGEPLACE:
                setROBO_epChangeplace(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RECONNOITRE:
                setROBO_epReconnoitre(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_ROBO:
                setROBO_epRobo(val.Get<int32_t>());
                break;

            case ROBO_ATT_VIEWANGLE:
                setROBO_viewAngle(val.Get<int32_t>());
                break;

            case ROBO_ATT_SAFDELAY:
                setROBO_safDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_POWDELAY:
                setROBO_powDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RADDELAY:
                setROBO_radDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CPLDELAY:
                setROBO_cplDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_DEFDELAY:
                setROBO_defDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CONDELAY:
                setROBO_conDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_ROBDELAY:
                setROBO_robDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RECDELAY:
                setROBO_recDelay(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_yparobo::AI_layer1(update_msg *arg)
{
    if ( _status == BACT_STATUS_DEAD )
    {
        int lastSeconds = getBACT_yourLastSeconds();
        lastSeconds -= arg->frameTime;
        setBACT_yourLastSeconds(lastSeconds);
    }

    _airconst = _airconst_static;

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
            gun.gun_obj->_owner = _owner;
    }

    EnergyInteract(arg);

    if ( _primTtype == BACT_TGT_TYPE_CELL )
        _target_vec = _primTpos - _position;
    else if ( _primTtype != BACT_TGT_TYPE_UNIT )
        _target_vec = vec3d(0.0, 0.0, 0.0);

    int inputting = getBACT_inputting();

    if ( inputting )
        UserUpdate(arg);
    else
        AI_layer3(arg);
}


void NC_STACK_yparobo::DispatchDockedForce(NC_STACK_ypabact *unit)
{
    if ( _roboState & ROBOSTATE_SETDOCKTARGET )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = _roboDockTargetType;
        targetMsg.priority = 0;
        targetMsg.tgt.pbact = NULL; // A unit target is found again by its squad's id
        targetMsg.tgt_pos.x = _roboDockTargetPos.x;
        targetMsg.tgt_pos.z = _roboDockTargetPos.z;

        if ( targetMsg.tgt_type == BACT_TGT_TYPE_UNIT )
        {
            targetMsg.priority = _roboDockTargetCommandID;

            if ( GetTargetByCommandID(&targetMsg) )
            {
                if ( !targetMsg.tgt.pbact->_pSector->IsCanSee(_owner) )
                {
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                    targetMsg.tgt_pos.x = _roboDockTargetPos.x;
                    targetMsg.tgt_pos.z = _roboDockTargetPos.z;

                    _roboDockAggr = 50;
                }
            }
            else
            {
                targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                targetMsg.tgt_pos.x = unit->_position.x;
                targetMsg.tgt_pos.z = unit->_position.z;
            }
            targetMsg.priority = 0;
        }

        if ( unit->_status != BACT_STATUS_DEAD )
        {
            TPathRequest pathReq;
            pathReq.from = unit->_position.XZ();

            if ( targetMsg.tgt_type == BACT_TGT_TYPE_UNIT )
            {
                pathReq.to = targetMsg.tgt.pbact->_position.XZ();
            }
            else
            {
                pathReq.to = targetMsg.tgt_pos.XZ();
            }

            pathReq.tgt_type = 1;
            pathReq.steps_cnt = 32;

            if ( unit->IsNeedsWaypoints() && unit->PathFinder(&pathReq) )
            {
                pathReq.steps_cnt = 32;

                unit->SetPath(&pathReq);

                if ( targetMsg.tgt_type == BACT_TGT_TYPE_UNIT )
                {
                    unit->_m_cmdID = targetMsg.tgt.pbact->_commandID;
                    unit->_m_owner = targetMsg.tgt.pbact->_owner;
                }
            }
            else
            {
                unit->SetTarget(&targetMsg);
            }
        }

        _roboState &= ~ROBOSTATE_SETDOCKTARGET;
    }

    unit->setBACT_aggression(_roboDockAggr);
    _roboDockCnt = 0;
    _roboDockUser = 0;
    _roboDockTime = 2000;
    _roboState &= ~ROBOSTATE_USEVHCLPARTS;
}

void NC_STACK_yparobo::InitForce(NC_STACK_ypabact *unit)
{
    if ( _roboDockTime > 0 )
        return;
    
    if ( _roboDockEnerg <= 0 )
    {
        DispatchDockedForce(unit);
        return;
    }

    int availableEnergy = _energy - _energy_max * 0.2;

    if ( _roboState & ROBOSTATE_USEVHCLPARTS )
        availableEnergy += _roboVehicleSpare;

    TSummaryQuery unitCountQuery;
    unitCountQuery.enrg_type = 3;
    unitCountQuery.enrg_sum = 0;

    GetSummary(&unitCountQuery);

    NC_STACK_ypabact *newUnit = NULL;

    int unitCost = (unitCountQuery.enrg_sum * 0.05 + 1.0) * (float)unit->_energy_max;
    
    if ( availableEnergy > (signed int)unitCost )
    {
        TUnitSpawn spawn;
        spawn.vehicle_id = unit->_vehicleID;
        spawn.pos = _position + _roboDockPos;

        newUnit = _world->CreateVehicle(&spawn);
    }

    if ( !newUnit )
    {
        DispatchDockedForce(unit);
        return;
    }
    
    newUnit->_status = BACT_STATUS_CREATE;
    newUnit->_owner = _owner;
    newUnit->_commandID = unit->_commandID;
    newUnit->_host_station = this;

    int bactCollisions = getBACT_bactCollisions();
    newUnit->setBACT_bactCollisions(bactCollisions);

    unit->AddSubject(newUnit);

    setTarget_msg targetMsg;
    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
    targetMsg.priority = 0;
    targetMsg.tgt_pos.x = _position.x + _rotation.m20 * World::CVSectorLength * 0.5;
    targetMsg.tgt_pos.z = _position.z + _rotation.m22 * World::CVSectorLength * 0.5;
    newUnit->SetTarget(&targetMsg);

    if ( _world->_isNetGame )
    {
        newUnit->_gid |= newUnit->_owner << 24;

        uamessage_newVhcl nvMsg;
        nvMsg.msgID = UAMSG_NEWVHCL;
        nvMsg.owner = newUnit->_owner;
        nvMsg.pos = newUnit->_position;
        nvMsg.type = newUnit->_vehicleID;
        nvMsg.parent = newUnit->_parent->_gid;
        nvMsg.id = newUnit->_gid;
        nvMsg.kind = 2;
        nvMsg.commandID = unit->_commandID;

        _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
    }

    if ( _roboState & ROBOSTATE_USEVHCLPARTS )
    {
        if ( unitCost > _roboVehicleSpare )
        {
            _energy -= unitCost - _roboVehicleSpare;
            _roboVehicleSpare = 0;
        }
        else
        {
            _roboVehicleSpare -= unitCost;
        }
    }
    else
    {
        _energy -= unitCost;
    }

    _roboDockEnerg -= newUnit->_energy;

    setState_msg stateMsg;
    stateMsg.setFlags = 0;
    stateMsg.unsetFlags = 0;
    stateMsg.newStatus = BACT_STATUS_CREATE;
    newUnit->SetState(&stateMsg);

    newUnit->_scale_time = newUnit->_energy_max * 0.2;
    _roboDockTime = newUnit->_scale_time + 2000;
    _roboDockCnt++;
}


int NC_STACK_yparobo::SendSquadToPosition(NC_STACK_ypabact *bact, float x, float z)
{
    int hasGroundUnits = 0;
    if ( bact->_bact_type == BACT_TYPES_TANK || bact->_bact_type == BACT_TYPES_CAR )
    {
        hasGroundUnits = 1;
    }
    else
    {
        for ( NC_STACK_ypabact* &node : bact->_kidList )
        {
            if (node->_bact_type == BACT_TYPES_TANK || node->_bact_type == BACT_TYPES_CAR)
            {
                hasGroundUnits = 1;
                break;
            }
        }
    }

    if ( hasGroundUnits )
    {
        TPathRequest pathReq;
        pathReq.steps_cnt = 32;
        pathReq.from = bact->_position.XZ();
        pathReq.to = vec2d(x, z);
        pathReq.tgt_type = 1;
        if ( !bact->SetPath(&pathReq) )
            return 0;
    }
    else
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_pos.x = x;
        targetMsg.tgt_pos.z = z;
        targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
        targetMsg.priority = 0;
        bact->SetTarget(&targetMsg);
    }
    return 1;
}

int NC_STACK_yparobo::SendSquadToUnit(NC_STACK_ypabact *bact, NC_STACK_ypabact *bact2)
{
    int hasGroundUnits = 0;
    if ( bact->_bact_type == BACT_TYPES_TANK || bact->_bact_type == BACT_TYPES_CAR )
    {
        hasGroundUnits = 1;
    }
    else
    {
        for ( NC_STACK_ypabact* &node : bact->_kidList )
        {
            if (node->_bact_type == BACT_TYPES_TANK || node->_bact_type == BACT_TYPES_CAR)
            {
                hasGroundUnits = 1;
                break;
            }
        }
    }

    if ( hasGroundUnits )
    {
        TPathRequest pathReq;
        pathReq.steps_cnt = 32;
        pathReq.from = bact->_position.XZ();
        pathReq.to = bact2->_position.XZ();
        pathReq.tgt_type = 1;

        if ( !bact->PathFinder(&pathReq) )
            return 0;

        if ( pathReq.steps_cnt > 1 )
        {
            pathReq.steps_cnt = 32;
            bact->SetPath(&pathReq);

            bact->_m_cmdID = bact2->_commandID;
            bact->_m_owner = bact2->_owner;
            return 1;
        }
    }

    setTarget_msg targetMsg;
    targetMsg.tgt.pbact = bact2;
    targetMsg.priority = 0;
    targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
    bact->SetTarget(&targetMsg);
    return 1;
}

void NC_STACK_yparobo::SendSquadAgainstEnemyRobo(NC_STACK_ypabact *unit1, NC_STACK_ypabact *unit2)
{
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            Common::Point pt = unit1->_cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                cellArea &pcell = _world->SectorAt(pt);

                if ( pcell.owner == unit1->_owner && pcell.PurposeType == cellArea::PT_POWERSTATION )
                {
                    vec2d xy = World::SectorIDToCenterPos2(pt);
                    SendSquadToPosition(unit2, xy.x, xy.y);

                    unit2->setBACT_aggression(25);
                    return;
                }

                if ( pcell.IsCanSee(unit2->_owner) )
                {
                    for( NC_STACK_ypabact* &cellUnit : pcell.unitsList )
                    {
                        if ( cellUnit->_bact_type == BACT_TYPES_GUN && unit1->_owner == cellUnit->_owner && cellUnit->_status != BACT_STATUS_DEAD )
                        {
                            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( cellUnit );

                            if ( !gun->IsRoboGun() )
                            {
                                if ( cellUnit->IsCommander() )
                                    SendSquadToUnit(unit2, cellUnit);
                                else if (cellUnit->_parent)
                                    SendSquadToUnit(unit2, cellUnit->_parent);

                                unit2->setBACT_aggression(25);
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    if ( unit1->_pSector->IsCanSee(unit2->_owner) )
    {
        SendSquadToUnit(unit2, unit1);

        unit2->setBACT_aggression(25);
    }
    else
    {
        vec2d xy = World::SectorIDToCenterPos2( unit1->_cellId );

        SendSquadToPosition(unit2, xy.x, xy.y);

        unit2->setBACT_aggression(50);
    }
}

void NC_STACK_yparobo::UpdateRoboAttackSquad(NC_STACK_ypabact *unit, TRoboAttackSlot *rbt)
{
    NC_STACK_ypabact *foundBact = NULL;
    
    if (unit->_parent->_kidRef)
    {
        for(NC_STACK_ypabact* &nod : *unit->_parent->_kidRef.PList())
        {
            if ( nod->_commandID == rbt->targetCommandID )
            {
                foundBact = nod;
                break;
            }
        }
    }

    if ( foundBact )
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            if ( unit->_primT.pbact->_bact_type == BACT_TYPES_ROBO )
                SendSquadAgainstEnemyRobo(foundBact, unit);
        }
        else if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            if (unit->_primT.pcell->owner == unit->_owner || foundBact->_pSector == unit->_primT.pcell)
                SendSquadAgainstEnemyRobo(foundBact, unit);
        }
    }
    else
    {
        rbt->targetCommandID = 0;
        rbt->squadCommandID = 0;
    }
}

void NC_STACK_yparobo::doBeamUpdate(int frameTime)
{
    _roboBeamTimePre -= frameTime;

    if ( _roboBeamTimePre > 0 )
    {
        if ( _roboBeamFXTime <= 0 )
        {
            int onTime = (1500 - _roboBeamTimePre) / 10;
            int offTime = _roboBeamTimePre / 10;

            if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
            {
                _roboBeamFXTime = offTime;
                _vp_extra[0].flags &= ~EVPROTO_FLAG_ACTIVE;
            }
            else
            {
                _roboBeamFXTime = onTime;

                _vp_extra[0].pos = _position;
                _vp_extra[0].rotate = _rotation;
                _vp_extra[0].flags = (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);
                _vp_extra[0].scale = 1.0;
                _vp_extra[0].model = _vp_genesis;
            }

            if ( _vp_extra[1].flags & EVPROTO_FLAG_ACTIVE )
            {
                _roboBeamFXTime = offTime;
                _vp_extra[1].flags &= ~EVPROTO_FLAG_ACTIVE;
            }
            else
            {
                _roboBeamFXTime = onTime;

                _vp_extra[1].pos = _roboBeamPos;
                _vp_extra[1].rotate = _rotation;
                _vp_extra[1].flags = EVPROTO_FLAG_ACTIVE;
                _vp_extra[1].model = _vp_genesis;
            }
        }

        _roboBeamFXTime -= frameTime;
    }
    else
    {
        _roboBeamTimePre = 0;

        vec3d tmp = _position;

        tmp.y = _roboYPos;

        TUnitPlacement placement;
        placement.pos = _roboBeamPos;
        placement.flags = 0;

        SetPosition(&placement);

        if ( _world->_isNetGame )
        {
            _status_flg |= BACT_STFLAG_DSETTED;

            for (NC_STACK_ypabact* &node : _kidList)
            {
                if ( node->_bact_type == BACT_TYPES_GUN )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( node );
                    if ( gun->IsRoboGun() )
                        node->_status_flg |= BACT_STFLAG_DSETTED;
                }
            }
        }

        _old_pos = tmp;
        _old_pos.y = tmp.y;

        SFXEngine::SFXe.startSound(&_soundcarrier, 10);

        _roboState &= ~ROBOSTATE_MOVE;
        _vp_extra[0].flags = 0;
        _vp_extra[1].flags = 0;

        if ( _world->_isNetGame )
        {
            uamessage_endBeam ebMsg;
            ebMsg.msgID = UAMSG_ENDBEAM;
            ebMsg.owner = _owner;
            ebMsg.id = _gid;

            _world->NetBroadcastMessage(&ebMsg, sizeof(ebMsg), true);
        }
    }
}

void NC_STACK_yparobo::RotateGunsAroundAxis(const vec3d &vaxis, float angle)
{
    for (World::TRoboGun &gun : _roboGuns)
    {
        if ( gun.gun_obj )
        {
            NC_STACK_ypagun *gungun = dynamic_cast<NC_STACK_ypagun *>(gun.gun_obj);

            if (gungun)
                gun.dir = gungun->RotateGunBasis(vaxis, -angle);
        }
    }
}

void NC_STACK_yparobo::TurnRoboAboutOwnUpAxis(float angle)
{
    _rotation = mat3x3::RotateY(angle) * _rotation;

    RotateGunsAroundAxis(_rotation.AxisY(), angle);

    _roboColls.lastTurnAngle = angle;
}

void NC_STACK_yparobo::TurnRoboAboutWorldUpAxis(float angle)
{
    _rotation = mat3x3::RotateY(angle) * _rotation;

    RotateGunsAroundAxis(vec3d::OY(1.0), angle);

    _roboColls.lastTurnAngle = angle;
}

size_t NC_STACK_yparobo::checkCollisions(float dtime)
{
    bool isViewer = getBACT_viewer();
    int inputting = getBACT_inputting();
    int exactCollisions = getBACT_exactCollisions();

    if ( _position == _old_pos )
        return 0;

    TCollisionNormal avgNormal;
    avgNormal.normal = vec3d(0.0, 0.0, 0.0);

    int hitCount = 0;

    TRayIntersection ray;

    for (const World::TRoboColl &rcoll : _roboColls.roboColls)
    {
        if ( !inputting || _primTtype )
        {
            vec3d tmp = rcoll.prev_coll_pos;
            vec3d t = _position + _rotation.Transform( rcoll.coll_pos );

            ray.stPos = tmp;
            ray.flags = 0;

            float moveDist = (tmp.XZ() - t.XZ()).length();

            if ( moveDist <= 1.0 )
                ray.vect = _fly_dir * 300.0;
            else
                ray.vect = vec3d::X0Z( (t.XZ() - tmp.XZ()) * 300.0 / moveDist );

            _world->IntersectRay(&ray);


            if ( ray.isect )
            {
                if ( ray.tVal * 300.0 >= rcoll.robo_coll_radius + moveDist)
                {
                    _status_flg &= ~BACT_STFLAG_LCRASH;
                    _status_flg |= BACT_STFLAG_UPWRD;
                    return 0;
                }
                else if ( rcoll.robo_coll_radius > 0.01 )
                {
                    TCollisionNormal collNormal;
                    collNormal.normal = ray.skel->polygons[ ray.polyID ].Normal();

                    Recoil(&collNormal);

                    if ( fabs(_roboColls.lastTurnAngle) > 0.0 )
                    {
                        TurnRoboAboutOwnUpAxis(-_roboColls.lastTurnAngle);

                        _roboColls.lastTurnAngle = 0;
                    }

                    TSectorInfo sectInfo;
                    sectInfo.pos_x = ray.isectPos.x;
                    sectInfo.pos_z = ray.isectPos.z;

                    _world->GetSectorInfo(&sectInfo);

                    if ( inputting || sectInfo.pcell->PurposeType == cellArea::PT_NONE )
                    {
                        TSectorDamage sectorDamage;
                        sectorDamage.pos = ray.isectPos;
                        sectorDamage.unit = NULL;
                        sectorDamage.OwnerID = World::OWNER_RECALC;
                        sectorDamage.energy = moveDist * 15000.0 / dtime;

                        ChangeSectorEnergy(&sectorDamage);
                    }

                    _fly_dir_length *= 0.4;
                    _status_flg |= BACT_STFLAG_UPWRD;

                    return 1;
                }
            }
        }


        int nearViewer = _world->IsNearViewer(this);

        if ( (isViewer || (nearViewer && exactCollisions)) && rcoll.robo_coll_radius > 0.01 )
        {
            vec3d tmp = rcoll.prev_coll_pos;
            vec3d t = _position + _rotation.Transform( rcoll.coll_pos );

            TSphereCollisionQuery sphereQuery;
            sphereQuery.pos = tmp;

            float moveDist = (tmp.XZ() - t.XZ()).length();

            if ( moveDist <= 1.0 )
                sphereQuery.dir = _fly_dir;
            else
            {
                sphereQuery.dir = vec3d::X0Z( (t.XZ() - tmp.XZ()) ) / moveDist;

                // TYPO??
                /*arg137.pos2.x = 1.0 / v83 * (t.x - tmp.x);
                arg137.pos2.y = 0;
                arg137.pos2.z = 1.0 / v83 * (t.x - tmp.x); */
            }

            TSphereContact contacts[32];

            sphereQuery.radius = rcoll.robo_coll_radius;
            sphereQuery.collisions = contacts;
            sphereQuery.flags = 0;
            sphereQuery.coll_max = 32;

            _world->FindSphereCollisions(&sphereQuery);

            if ( sphereQuery.coll_count )
            {
                hitCount++;

                float crashEnergy = moveDist * 15000.0;

                vec3d ttmp(0.0, 0.0, 0.0);

                for (int j = sphereQuery.coll_count - 1; j >= 0; j--)
                {
                    TSphereContact *clzn = &sphereQuery.collisions[j];

                    ttmp += clzn->normal;

                    TSectorInfo sectInfo;
                    sectInfo.pos_x = clzn->pos1.x;
                    sectInfo.pos_z = clzn->pos1.z;

                    _world->GetSectorInfo(&sectInfo);

                    if ( inputting || sectInfo.pcell->PurposeType == cellArea::PT_NONE )
                    {
                        TSectorDamage sectorDamage;
                        sectorDamage.pos = clzn->pos1;
                        sectorDamage.unit = NULL;
                        sectorDamage.OwnerID = World::OWNER_RECALC;
                        sectorDamage.energy = crashEnergy / dtime;

                        ChangeSectorEnergy(&sectorDamage);
                    }
                }

                float normalLen = ttmp.length();

                if ( normalLen != 0.0 )
                    avgNormal.normal += ttmp / normalLen;
                else
                    avgNormal.normal += _fly_dir;
            }

            // TODO(bug): Inside the per-point loop, so it returns at the first point with sphere
            // contacts: hitCount is always 1 and nothing is averaged. Probably meant
            // to run after the loop. The first decompile has the same.
            if ( hitCount )
            {
                avgNormal.normal /= (float)hitCount;

                _position = _old_pos;


                Recoil(&avgNormal);

                if ( fabs(_roboColls.lastTurnAngle) > 0.0 )
                {
                    TurnRoboAboutOwnUpAxis(-_roboColls.lastTurnAngle);
                    _roboColls.lastTurnAngle = 0;
                }

                _fly_dir_length *= 0.4;
                _status_flg |= BACT_STFLAG_UPWRD;

                return 1;
            }
        }
    }

    _status_flg &= ~BACT_STFLAG_LCRASH;

    ray.stPos = _position;
    ray.vect = vec3d::OY( _height * 1.5 ) + _fly_dir.X0Z() * 100.0;
    ray.flags = 0;

    _world->IntersectRay(&ray);

    if ( ray.isect )
    {
        if ( ray.tVal < 0.66 )
        {
            _target_dir.y = -1.0;
            _status_flg |= BACT_STFLAG_UPWRD;
        }
    }
    else
    {
        _target_dir.y = 1.0;
        _status_flg &= ~BACT_STFLAG_UPWRD;
    }

    _roboColls.lastTurnAngle = 0;

    return 0;
}

void NC_STACK_yparobo::wallow(update_msg *arg)
{
    _position.y = sin(arg->gTime * C_PI / 3000.0) * 25.0 + _roboYPos;

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
        {
            TUnitPlacement placement;
            placement.pos = _position + _rotation.Transpose().Transform(gun.pos);
            placement.flags = 4;

            gun.gun_obj->SetPosition(&placement);
        }
    }
}

void NC_STACK_yparobo::UpdateCollisionPointPositions()
{
    for (World::TRoboColl &rcoll : _roboColls.roboColls)
        rcoll.prev_coll_pos = _position + _rotation.Transform( rcoll.coll_pos );
}

bool NC_STACK_yparobo::IsNotUserControlled()
{
    NC_STACK_ypabact *yw_1b7c = _world->UserUnit();

    if ( yw_1b7c == this )
        return false;

    for (World::TRoboGun &gun : _roboGuns)
        if (gun.gun_obj && gun.gun_obj == yw_1b7c)
            return false;

    return true;
}

void NC_STACK_yparobo::AI_doMove(update_msg *arg)
{
    move_msg moveMsg;

    UpdateCollisionPointPositions();

    float dtime = arg->frameTime / 1000.0;

    _roboFlotage = _mass * 9.80665;

    if ( _target_dir.y < -0.7 )
        _roboFlotage = _mass * 3.0 * 9.80665;

    if ( _target_dir.y > 0.7 )
        _roboFlotage = _mass * 0.91 * 9.80665;

    float targetDist;
    if ( _primTtype )
        targetDist = _target_vec.XZ().length();
    else
        targetDist = 0.0;

    if ( targetDist >= 100.0 )
    {
        if ( _status_flg & BACT_STFLAG_UPWRD )
        {
            _airconst *= 3.0;

            _roboFlotage = _mass * 2.5 * 9.80665;
            _thraction = 0;

            float damping = 1.0 - dtime * 4.0;
            if ( damping < 0.0 )
                damping = 0.1;

            _fly_dir_length *= damping;
        }
        else if ( _primTtype )
        {
            _thraction = _force;
        }
        else
        {
            _thraction = 0;
        }

        if ( !(_status_flg & BACT_STFLAG_UPWRD) )
        {
            float forwardLen = _rotation.AxisZ().XZ().length();
            float targetDirLen = _target_dir.XZ().length();

            if ( forwardLen <= 0.0001 || targetDirLen <= 0.0001 )
            {
                _slider_time = 0;
                _status_flg &= ~BACT_STFLAG_XLEFT;
            }
            else
            {
                float cosAngle = _target_dir.XZ().dot( _rotation.AxisZ().XZ() ) / (targetDirLen * forwardLen);

                float turnAngle = clp_acos(cosAngle);

                float absAngle = fabs(turnAngle);

                if ( absAngle >= 0.001 )
                {
                    float mxx = dtime * _maxrot;

                    if ( absAngle > mxx )
                        turnAngle = mxx;

                    if ( _rotation.AxisZ().XZ().cross( _target_dir.XZ() ) < 0.0 )
                        turnAngle = -turnAngle;

                    float turnRamp;

                    if ( turnAngle <= 0.0 )
                    {
                        if ( !(_status_flg & BACT_STFLAG_XLEFT) || !_slider_time )
                            _slider_time = arg->gTime;

                        turnRamp = (arg->gTime - _slider_time) * 0.9 * 0.001 + 0.1;
                        _status_flg |= BACT_STFLAG_XLEFT;
                    }
                    else
                    {
                        if ( _status_flg & BACT_STFLAG_XLEFT || !_slider_time )
                            _slider_time = arg->gTime;

                        turnRamp = (arg->gTime - _slider_time) * 0.9 * 0.001 + 0.1;
                        _status_flg &= ~BACT_STFLAG_XLEFT;
                    }

                    if ( turnRamp > 1.0 )
                        turnRamp = 1.0;

                    turnAngle *= turnRamp;

                    TurnRoboAboutWorldUpAxis(turnAngle);
                }
            }
        }

        moveMsg.period = dtime;
        moveMsg.flag = 0;
        moveMsg.thrustDir = _target_vec.X0Z() / targetDist;
    }
    else
    {
        _thraction = 0;

        if ( fabs(_fly_dir.x * _fly_dir_length) <= 0.1 && fabs(_fly_dir.z * _fly_dir_length) <= 0.1 )
        {
            if ( !(_roboState & ROBOSTATE_BUILDRADAR) &&
                    !(_roboState & ROBOSTATE_BUILDPOWER) &&
                    !(_roboState & ROBOSTATE_BUILDSAFE) )
            {
                bool notUserControlled = IsNotUserControlled();

                if ( notUserControlled && _roboWFlags & 1 )
                {
                    float liftMass;
                    if ( _status_flg & BACT_STFLAG_UPWRD )
                        liftMass = _mass * 1.5;
                    else
                        liftMass = _mass * 0.82;

                    _roboFlotage = liftMass * 9.80665;
                }
                else
                {
                    if ( fabs(_target_dir.y) < 0.7 )
                        _fly_dir_length *= 0.35;
                }

                if ( notUserControlled && _roboWFlags & 2 )
                {
                    TurnRoboAboutOwnUpAxis(_maxrot * 0.3 * dtime);
                }
            }
        }
        else
        {
            _airconst *= 3.0;
            _fly_dir_length *= 0.8;

            if ( fabs(_fly_dir.x * _fly_dir_length) < 0.1 && fabs(_fly_dir.z * _fly_dir_length) < 0.1 )
            {
                _fly_dir.z = 0;
                _fly_dir.x = 0;

                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 0;
                SetTarget(&targetMsg);
            }
        }
        moveMsg.thrustDir = vec3d(0.0, 0.0, 0.0);
        moveMsg.flag = 0;
        moveMsg.period = dtime;
    }

    Move(&moveMsg);
}

void NC_STACK_yparobo::ClearSquadSecondaryTargets(NC_STACK_ypabact *bact)
{
    setTarget_msg targetMsg;
    targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
    targetMsg.priority = 1;

    bact->SetTarget(&targetMsg);

    for( NC_STACK_ypabact* &node : bact->_kidList )
        node->SetTarget(&targetMsg);
}

void NC_STACK_yparobo::CopyWaypointsToSquad(NC_STACK_ypabact *bact)
{
    for( NC_STACK_ypabact* &node : bact->_kidList )
    {
        node->_waypoints_count = bact->_waypoints_count;

        if ( node->_waypoints_count == 1 )
            node->_current_waypoint = 0;

        node->_status_flg |= BACT_STFLAG_WAYPOINT;

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            node->_status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            node->_status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        for (int i = 0; i < 32; i++)
            node->_waypoints[i] = bact->_waypoints[i];
    }
}



int NC_STACK_yparobo::IsCommanderClosestToTarget(NC_STACK_ypabact *bact)
{
    vec3d tmp;
    if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
    {
        tmp = bact->_primT.pbact->_position;
    }
    else if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
    {
        tmp = bact->_primTpos;
    }
    else
        return 1;

    vec3d tmp2 = bact->_position - tmp;

    float commanderDist = tmp2.length();

    for( NC_STACK_ypabact* &node : bact->_kidList )
    {
        vec3d tmp3 = node->_position - tmp;

        if ( tmp3.length() < commanderDist )
            return 0;
    }
    return 1;
}

void NC_STACK_yparobo::doUserCommands(TUserOrder *arg)
{
    setTarget_msg targetMsg;
    TUnitSpawn spawn;
    setState_msg stateMsg;
    TBuildingPlacement placement;

    switch ( arg->user_action )
    {
    case World::DOACTION_ADD_UNIT1:
        if ( arg->energy <= _roboEnergyLife )
        {
            spawn.pos = arg->target_point;
            spawn.vehicle_id = arg->protoID;

            NC_STACK_ypabact *newbact = _world->CreateVehicle(&spawn);

            if ( newbact )
            {
                targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                targetMsg.priority = 0;
                targetMsg.tgt_pos.x = (spawn.pos.x - _position.x) * 4.0 + _position.x;
                targetMsg.tgt_pos.y = spawn.pos.y;
                targetMsg.tgt_pos.z = (spawn.pos.z - _position.z) * 4.0 + _position.z;

                newbact->SetTarget(&targetMsg);

                stateMsg.newStatus = BACT_STATUS_CREATE;
                stateMsg.setFlags = 0;
                stateMsg.unsetFlags = 0;

                newbact->SetState(&stateMsg);

                newbact->_scale_time = newbact->_energy_max * 0.2;
                newbact->_commandID = _world->_nextCommandId;
                _world->_nextCommandId++;

                newbact->_owner = _owner;
                AddSubject(newbact);

                if ( _world->_isNetGame )
                {
                    newbact->_gid |= newbact->_owner << 24;
                    newbact->_commandID |= newbact->_owner << 24;

                    uamessage_newVhcl nvMsg;
                    nvMsg.msgID = UAMSG_NEWVHCL;
                    nvMsg.owner = newbact->_owner;
                    nvMsg.pos = newbact->_position;
                    nvMsg.type = newbact->_vehicleID;
                    nvMsg.parent = newbact->_parent->_gid;
                    nvMsg.id = newbact->_gid;
                    nvMsg.kind = 1;
                    nvMsg.commandID = newbact->_commandID;

                    _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
                }

                if ( newbact->_bact_type == BACT_TYPES_UFO )
                    newbact->setBACT_aggression(25);
                else
                    newbact->setBACT_aggression(60);

                int bactCollisions = getBACT_bactCollisions();
                newbact->setBACT_bactCollisions(bactCollisions);

                newbact->_host_station = this;
                _roboEnergyLife -= arg->energy;
                
                _world->SetCmdrIdToSelect(newbact->_commandID); // Select it for add next units
            }
        }
        break;

    case World::DOACTION_ADD_UNIT2:
        if ( arg->energy <= _roboEnergyLife )
        {
            spawn.pos = arg->target_point;
            spawn.vehicle_id = arg->protoID;
            NC_STACK_ypabact *newbact2 = _world->CreateVehicle(&spawn);

            if ( newbact2 )
            {
                stateMsg.newStatus = BACT_STATUS_CREATE;
                stateMsg.setFlags = 0;
                stateMsg.unsetFlags = 0;
                newbact2->SetState(&stateMsg);

                newbact2->_scale_time = newbact2->_energy_max * 0.2;
                newbact2->_owner = _owner;
                newbact2->_commandID = arg->selectBact->_commandID;

                arg->selectBact->AddSubject(newbact2);

                newbact2->_aggr = arg->selectBact->_aggr;

                if ( _world->_isNetGame )
                {
                    newbact2->_gid |= newbact2->_owner << 24;

                    uamessage_newVhcl nvMsg;
                    nvMsg.msgID = UAMSG_NEWVHCL;
                    nvMsg.owner = newbact2->_owner;
                    nvMsg.pos = newbact2->_position;
                    nvMsg.type = newbact2->_vehicleID;
                    nvMsg.parent = newbact2->_parent->_gid;
                    nvMsg.id = newbact2->_gid;
                    nvMsg.kind = 2;
                    nvMsg.commandID = newbact2->_commandID;

                    _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
                }

                int bactCollisions = getBACT_bactCollisions();
                newbact2->setBACT_bactCollisions(bactCollisions);

                newbact2->_host_station = this;
                _roboEnergyLife -= arg->energy;

                newbact2->CopyTargetOf(arg->selectBact);
            }
        }
        break;

    case World::DOACTION_6:
        if ( arg->energy <= _roboEnergyLife )
        {
            placement.owner = _owner;
            placement.blg_ID = arg->protoID;
            placement.instant = 0;
            placement.CellId = arg->target_Sect->CellId;
            placement.flags = 0;
            if ( _world->CreateBuilding(&placement) )
            {
                _roboEnergyLife -= arg->energy;

                if ( _world->_isNetGame )
                {
                    uamessage_startBuild sbMsg;
                    sbMsg.msgID = UAMSG_STARTBUILD;
                    sbMsg.owner = _owner;
                    sbMsg.bproto = arg->protoID;
                    sbMsg.sec_x = arg->target_Sect->CellId.x;
                    sbMsg.sec_y = arg->target_Sect->CellId.y;

                    _world->NetBroadcastMessage(&sbMsg, sizeof(sbMsg), true);
                }
            }
        }
        break;

    case World::DOACTION_10:
        if ( !(_roboState & ROBOSTATE_MOVE) )
        {
            SFXEngine::SFXe.startSound(&_soundcarrier, 9);

            _roboState |= ROBOSTATE_MOVE;
            _roboEnergyMove -= arg->energy;
            _roboBeamPos = arg->target_point;
            _roboBeamTimePre = 1500;
            if ( _world->_isNetGame )
            {
                uamessage_startBeam stbMsg;
                stbMsg.msgID = UAMSG_STARTBEAM;
                stbMsg.owner = _owner;
                stbMsg.id = _gid;
                stbMsg.pos = _roboBeamPos;

                _world->NetBroadcastMessage(&stbMsg, sizeof(stbMsg), true);
            }
        }
        break;

    // Squads' orders, which need no host station
    case World::DOACTION_2:
    case World::DOACTION_WP_BEGIN:
    case World::DOACTION_WP_ADD:
    case World::DOACTION_WP_CYCLE:
        _world->OrderSquad(_owner, arg);
        break;

    default:
        break;
    }
}

void NC_STACK_yparobo::HandleUserCommands(TUserOrder *arg)
{
    if ( _kidRef.IsListType(World::BLIST_CACHE) ) // Do not update units in dead list
        return;
    
    if (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2))
        return;
    
    if ( _status != BACT_STATUS_NORMAL)
        return;

    if ( getBACT_inputting() || IsPlayerRobo() )
        doUserCommands(arg);
}

int NC_STACK_yparobo::FindBestBldRadar()
{
    int id = -1;
    int power = 0;
    int i = 0;
    
    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 2 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }
        
        i++;
    }

    return id;
}

void NC_STACK_yparobo::GetBuildSiteApproachTarget(int sectID, setTarget_msg *target)
{
    Common::Point pt(sectID % _wrldSectors.x, sectID / _wrldSectors.x);

    if ( _cellId == pt )
    {
        if ( pt.x >= 2 )
            pt.x--;
        else
            pt.x++;
    }
    else
    {
        if ( pt.x > _cellId.x )
            pt.x--;
        if ( pt.x < _cellId.x )
            pt.x++;

        if ( pt.y > _cellId.y )
            pt.y--;
        if ( pt.y < _cellId.y )
            pt.y++;
    }

    target->tgt_type = BACT_TGT_TYPE_CELL;
    target->priority = 0;
    target->tgt_pos = World::SectorIDToCenterPos3( pt );
}

void NC_STACK_yparobo::buildRadar(update_msg *arg)
{
    Common::Point pt( _roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x );

    float dtime = arg->frameTime * 0.001;

    if ( _world->SectorAt(_roboBuildingCellID).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
        targetMsg.priority = 0;

        SetTarget(&targetMsg);
        _roboBuildingDuty = 0;
    }
    else
    {
        int build_id = FindBestBldRadar();

        if ( build_id == -1 )
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;

            SetTarget(&targetMsg);
            _roboBuildingDuty = 0;
        }
        else
        {
            Common::Point dist = _cellId.AbsDistance( pt );
            if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
            {
                setTarget_msg targetMsg;
                GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);

                targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&targetMsg);
            }
            else if ( dist ) // Not null distance (1 sector distance)
            {
                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 0;
                SetTarget(&targetMsg);

                vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                t.normalise();

                float cosAngle = t.dot( _rotation.AxisZ().XZ() );

                if ( cosAngle <= 0.9 )
                {
                    float turnAngle = clp_acos(cosAngle);

                    if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                        turnAngle = -turnAngle;

                    if ( turnAngle < -_maxrot * dtime )
                        turnAngle = -_maxrot * dtime;

                    if ( turnAngle > _maxrot * dtime )
                        turnAngle = _maxrot * dtime;

                    TurnRoboAboutWorldUpAxis(turnAngle);
                }
                else
                {
                    TBuildingPlacement placement;

                    placement.owner = _owner;
                    placement.blg_ID = build_id;
                    placement.CellId = pt;
                    placement.instant = 0;
                    placement.flags = 0;

                    if ( _world->CreateBuilding(&placement) )
                    {
                        const World::TBuildingProto &proto = _world->GetBuildProtos().at(build_id);

                        if ( _roboBuildSpare >= proto.Energy )
                        {
                            _roboBuildSpare -= proto.Energy;
                        }
                        else
                        {
                            _energy -= proto.Energy - _roboBuildSpare;
                            _roboBuildSpare = 0;
                        }

                        _roboBuildingDuty = 0;

                        _roboRadarDelay = _roboTimeScale * (100 - _roboEpRadar) / 100;
                    }
                    else
                    {
                        setTarget_msg abortTargetMsg;
                        abortTargetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                        abortTargetMsg.priority = 0;

                        SetTarget(&abortTargetMsg);

                        _roboBuildingDuty = 0;
                    }
                }
            }
            else
            {
                setTarget_msg targetMsg;

                GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);

                targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&targetMsg);
            }
        }
    }
}

int NC_STACK_yparobo::FindBestBldPowerStation()
{
    int id = -1;
    int power = 0;
    int i = 0;

    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 1 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }
        i++;
    }

    return id;
}

void NC_STACK_yparobo::buildPower(update_msg *arg)
{
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);

    float dtime = arg->frameTime * 0.001;

    if ( _world->SectorAt( _roboBuildingCellID ).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
        targetMsg.priority = 0;
        SetTarget(&targetMsg);

        _roboBuildingDuty = 0;
    }
    else
    {
        TReloadRatios reloadRatios;
        reloadRatios.owner = _owner;
        _world->GetReloadRatios(&reloadRatios);

        if ( _world->SectorAt( _roboBuildingCellID ).energy_power != 255 && ( reloadRatios.ratioClamped >= 0.9 || reloadRatios.ratioClamped <= 0.001) )
        {
            int bldid = FindBestBldPowerStation();

            if ( bldid == -1 )
            {
                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 0;
                SetTarget(&targetMsg);
                _roboBuildingDuty = 0;
            }
            else
            {
                Common::Point dist = _cellId.AbsDistance( pt );
                if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
                {
                    setTarget_msg targetMsg;
                    GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
                    SetTarget(&targetMsg);
                }
                else if ( dist ) // Not null distance (1 sector distance)
                {
                    setTarget_msg targetMsg;
                    targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                    targetMsg.priority = 0;
                    SetTarget(&targetMsg);

                    vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                    t.normalise();

                    float cosAngle = t.dot( _rotation.AxisZ().XZ() );

                    if ( cosAngle <= 0.9 )
                    {
                        float turnAngle = clp_acos(cosAngle);

                        if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                            turnAngle = -turnAngle;

                        if ( turnAngle < -_maxrot * dtime )
                            turnAngle = -_maxrot * dtime;

                        if ( turnAngle > _maxrot * dtime )
                            turnAngle = _maxrot * dtime;

                        TurnRoboAboutWorldUpAxis(turnAngle);
                    }
                    else
                    {
                        TBuildingPlacement placement;
                        placement.owner = _owner;
                        placement.blg_ID = bldid;
                        placement.CellId = pt;
                        placement.instant = 0;
                        placement.flags = 0;

                        if ( _world->CreateBuilding(&placement) )
                        {
                            const World::TBuildingProto &proto = _world->GetBuildProtos().at(bldid);

                            if ( _roboBuildSpare >= proto.Energy )
                            {
                                _roboBuildSpare -= proto.Energy;
                            }
                            else
                            {
                                _energy -= proto.Energy - _roboBuildSpare;
                                _roboBuildSpare = 0;
                            }

                            _roboBuildingDuty = 0;

                            _roboPowerDelay = _roboTimeScale * (100 - _roboEpPower) / 100;
                        }
                        else
                        {
                            setTarget_msg abortTargetMsg;
                            abortTargetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                            abortTargetMsg.priority = 0;
                            SetTarget(&abortTargetMsg);
                            _roboBuildingDuty = 0;
                        }
                    }
                }
                else
                {
                    setTarget_msg targetMsg;
                    GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
                    SetTarget(&targetMsg);
                }
            }
        }
        else
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;
            SetTarget(&targetMsg);
            _roboBuildingDuty = 0;
        }
    }
}

int NC_STACK_yparobo::FindBestBldDefenceCenter()
{
    int id = -1;
    int power = 0;
    int i = 0;

    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 3 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }
        i++;
    }

    return id;
}


void NC_STACK_yparobo::buildSafe(update_msg *arg)
{
    // Copy of sub4_sub0 with minor changes
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);

    float dtime = arg->frameTime * 0.001;

    if ( _world->SectorAt( _roboBuildingCellID ).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
        targetMsg.priority = 0;

        SetTarget(&targetMsg);
        _roboBuildingDuty = 0;
    }
    else
    {
        int build_id = FindBestBldDefenceCenter();

        if ( build_id == -1 )
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;

            SetTarget(&targetMsg);
            _roboBuildingDuty = 0;
        }
        else
        {
            Common::Point dist = _cellId.AbsDistance( pt );
            if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
            {
                setTarget_msg targetMsg;
                GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);

                targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&targetMsg);
            }
            else if ( dist ) // Not null distance (1 sector distance)
            {
                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 0;
                SetTarget(&targetMsg);

                vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                t.normalise();

                float cosAngle = t.dot( _rotation.AxisZ().XZ() );

                if ( cosAngle <= 0.9 )
                {
                    float turnAngle = clp_acos(cosAngle);

                    if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                        turnAngle = -turnAngle;

                    if ( turnAngle < -_maxrot * dtime )
                        turnAngle = -_maxrot * dtime;

                    if ( turnAngle > _maxrot * dtime )
                        turnAngle = _maxrot * dtime;

                    TurnRoboAboutWorldUpAxis(turnAngle);
                }
                else
                {
                    TBuildingPlacement placement;

                    placement.owner = _owner;
                    placement.blg_ID = build_id;
                    placement.CellId = pt;
                    placement.instant = 0;
                    placement.flags = 0;

                    if ( _world->CreateBuilding(&placement) )
                    {
                        const World::TBuildingProto &proto = _world->GetBuildProtos().at(build_id);

                        if ( _roboBuildSpare >= proto.Energy )
                        {
                            _roboBuildSpare -= proto.Energy;
                        }
                        else
                        {
                            _energy -= proto.Energy - _roboBuildSpare;
                            _roboBuildSpare = 0;
                        }

                        _roboBuildingDuty = 0;

                        _roboSafetyDelay = _roboTimeScale * (100 - _roboEpSafety) / 100;
                    }
                    else
                    {
                        setTarget_msg abortTargetMsg;
                        abortTargetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                        abortTargetMsg.priority = 0;

                        SetTarget(&abortTargetMsg);

                        _roboBuildingDuty = 0;
                    }
                }
            }
            else
            {
                setTarget_msg targetMsg;

                GetBuildSiteApproachTarget(_roboBuildingCellID, &targetMsg);

                targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&targetMsg);
            }

        }
    }
}

void NC_STACK_yparobo::changePlace()
{
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);
    
    if ( _world->IsGamePlaySector(pt) )
    {
        float cellDist = _cellId.LengthTo<float>( pt );

        if ( cellDist >= 0.1 )
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
            targetMsg.priority = 0;
            targetMsg.tgt_pos = World::SectorIDToCenterPos3( pt );

            SetTarget(&targetMsg);
        }
        else
        {
            _roboSafetyCellID = 0;
            _roboSafetyValue = 0;
            _roboSafetyCellIndex = 0;
            _roboPositionCellID = 0;
            _roboRadarCellID = 0;
            _roboPowerCellID = 0;
            _roboPositionValue = 0;
            _roboRadarValue = 0;
            _roboPowerValue = 0;
            _roboPositionCellIndex = 0;
            _roboRadarCellIndex = 0;
            _roboPowerCellIndex = 0;

            _roboBuildingDuty = 0;

            _roboState &= ~(ROBOSTATE_READYRADAR | ROBOSTATE_READYPOWER
                                 | ROBOSTATE_READYSAFE | ROBOSTATE_FOUNDPLACE);

            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;
            SetTarget(&targetMsg);

            _roboPositionDelay = _roboTimeScale * (100 - _roboEpChangePlace) / 100;
        }
    }
    else
    {
        _roboBuildingDuty = 0;
    }
}


NC_STACK_ypabact *NC_STACK_yparobo::AllocForce(robo_loct1 *arg)
{
    int useDock = 0;
    float costFactor = 0.0;

    int bestProtoId = -1;
    int secondProtoId = -1;
    int thirdProtoId = -1;

    int bestEnergyDiff = 10000000;
    int idleCount = 0;
    int idleNearby = 0;
    int bestRating = -2;
    float bestTargetDist = 153600.0;
    NC_STACK_ypabact *selectedCommander = NULL;

    for (NC_STACK_ypabact* &node : _kidList)
    {
        //Unused flag
        if ( node->_status == BACT_STATUS_IDLE && /*!( node->status_flg & BACT_STFLAG_AKKU ) &&*/ !( node->_status_flg & BACT_STFLAG_UNUSE ) && !( node->_status_flg & BACT_STFLAG_ESCAPE ) &&  node->_bact_type != BACT_TYPES_GUN )
        {
            idleCount++;

            int rating = _world->TestVehicle(node->_vehicleID, arg->job);

            Common::Point tmp = _cellId.AbsDistance( node->_cellId );
            if ( tmp.x < 2 && tmp.y < 2 )
                idleNearby = 1;

            if ( rating > -1 && rating >= bestRating )
            {

                TSummaryQuery squadEnergyQuery;
                squadEnergyQuery.enrg_type = 1;
                squadEnergyQuery.enrg_sum = 0;
                node->GetSummary(&squadEnergyQuery);

                int energyDiff = abs(squadEnergyQuery.enrg_sum - arg->energ);
                int isBetter = 0;

                int energyRatio;

                if ( energyDiff )
                    energyRatio = arg->energ / energyDiff;
                else
                    energyRatio = 6;

                float targetDist = (node->_position.XZ() - arg->tgt_pos.XZ()).length();

                if ( energyRatio <= 5 )
                {
                    if ( energyDiff < bestEnergyDiff )
                    {
                        isBetter = 1;
                    }
                }
                else if ( targetDist < bestTargetDist )
                {
                    isBetter = 1;
                }

                if ( rating > bestRating || (rating == bestRating && isBetter) )
                {
                    bestEnergyDiff = energyDiff;
                    bestTargetDist = targetDist;
                    selectedCommander = node;
                    bestRating = rating;
                }

            }
        }
    }

    if ( !(_roboState & ROBOSTATE_DOCKINUSE) )
    {
        TSummaryQuery unitCountQuery;
        unitCountQuery.enrg_sum = 0;
        unitCountQuery.enrg_type = 3;

        GetSummary(&unitCountQuery);

        bestProtoId = -1;
        secondProtoId = -1;
        thirdProtoId = -1;

        costFactor = unitCountQuery.enrg_sum * 0.05 + 1.0;

        int i = 0;
        for ( const World::TVhclProto &proto : _world->GetVhclProtos() )
        {
            if ( proto.model_id != BACT_TYPES_NOPE )
            {
                float reserveEnergy = _energy_max * 0.2;
                int availableEnergy = _energy - reserveEnergy;

                if ( _roboState & ROBOSTATE_BUILDROBO )
                    availableEnergy += _roboVehicleSpare;

                if ( (1 << _owner) & proto.disable_enable_bitmask )
                {
                    if ( proto.model_id != BACT_TYPES_ROBO && proto.model_id != BACT_TYPES_GUN && proto.model_id != BACT_TYPES_MISSLE )
                    {
                        if ( availableEnergy > ((float)proto.energy * costFactor) )
                        {
                            int rating = _world->TestVehicle(i, arg->job);

                            int preferNew = 0;
                            if ( idleCount < (_wrldSectors.x + _wrldSectors.y) / 5 )
                                preferNew = 1;

                            if ( _energy < ((float)_energy_max * 0.9) )
                                preferNew = 0;

                            // TODO(cleanup): Always false: the enclosing block only runs when ROBOSTATE_DOCKINUSE is
                            // clear.
                            if ( _roboState & ROBOSTATE_DOCKINUSE )
                                preferNew = 0;

                            if ( idleNearby )
                                preferNew = 0;

                            if ( rating > -1 && (rating > bestRating || (rating == bestRating && preferNew)) )
                            {
                                bestRating = rating;
                                thirdProtoId = secondProtoId;
                                secondProtoId = bestProtoId;
                                bestProtoId = i;
                            }
                        }
                    }
                }
            }
            
            i++;
        }
    }

    //there must be switch, but it's originally deleted
    //arg->field_34;

    if ( bestProtoId == -1 && !selectedCommander )
        return NULL;

    if ( bestProtoId == -1 )
    {
        if ( !(_roboState & ROBOSTATE_DOCKINUSE) )
        {
            TSummaryQuery squadEnergyQuery;
            squadEnergyQuery.enrg_type = 1;
            squadEnergyQuery.enrg_sum = 0;

            selectedCommander->GetSummary(&squadEnergyQuery);

            if ( squadEnergyQuery.enrg_sum < arg->energ )
            {
                _roboDockUser = selectedCommander->_commandID;
                _roboDockEnerg = arg->energ - squadEnergyQuery.enrg_sum;
                useDock = 1;
            }
        }
    }
    else
    {
        int roll = _clock % 10;
        int protoId;

        if ( roll <= 8 || thirdProtoId == -1 )
        {
            if ( roll <= 5 || secondProtoId == -1 )
                protoId = bestProtoId;
            else
                protoId = secondProtoId;
        }
        else
        {
            protoId = thirdProtoId;
        }

        TUnitSpawn spawn;
        spawn.vehicle_id = protoId;
        spawn.pos = _position + _roboDockPos;

        NC_STACK_ypabact *newUnit = _world->CreateVehicle(&spawn);

        if ( !newUnit )
            return NULL;

        newUnit->_commandID = _world->_nextCommandId;
        _world->_nextCommandId++;

        newUnit->_owner = _owner;
        newUnit->_host_station = this;

        int bactCollisions = getBACT_bactCollisions();
        newUnit->setBACT_bactCollisions(bactCollisions);

        AddSubject(newUnit);
        if ( _world->_isNetGame )
        {
            newUnit->_gid |= newUnit->_owner << 24;
            newUnit->_commandID |= newUnit->_owner << 24;

            uamessage_newVhcl nvMsg;

            nvMsg.msgID = UAMSG_NEWVHCL;
            nvMsg.owner = newUnit->_owner;
            nvMsg.pos = newUnit->_position;
            nvMsg.type = newUnit->_vehicleID;
            nvMsg.parent = newUnit->_parent->_gid;
            nvMsg.id = newUnit->_gid;
            nvMsg.kind = 1;
            nvMsg.commandID = newUnit->_commandID;

            _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
        }

        _roboState |= ROBOSTATE_DOCKINUSE;

        newUnit->setBACT_aggression(60);

        _roboDockUser = newUnit->_commandID;
        _roboDockEnerg = arg->energ - newUnit->_energy;

        newUnit->_scale_time = (float)newUnit->_energy_max * 0.2;

        _roboDockTime = newUnit->_scale_time + 2000;

        newUnit->ChangeEscapeFlag(false);

        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_CREATE;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;

        newUnit->SetState(&stateMsg);


        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
        targetMsg.priority = 0;
        targetMsg.tgt_pos.x = _rotation.m20 * World::CVSectorLength * 0.5 + _position.x;
        targetMsg.tgt_pos.z = _rotation.m22 * World::CVSectorLength * 0.5 + _position.z;

        newUnit->SetTarget(&targetMsg);

        int unitCost = (float)newUnit->_energy_max * costFactor;

        if ( _roboState & ROBOSTATE_BUILDROBO )
        {
            if ( unitCost > _roboVehicleSpare )
            {
                _energy -= unitCost - _roboVehicleSpare;
                _roboVehicleSpare = 0;
            }
            else
            {
                _roboVehicleSpare -= unitCost;
            }
        }
        else
        {
            _energy -= unitCost;
        }

        selectedCommander = newUnit;
        useDock = 1;
    }

    if ( useDock )
    {
        _roboState |= (ROBOSTATE_SETDOCKTARGET | ROBOSTATE_DOCKINUSE);
        _roboDockTargetType = arg->tgType;
        _roboDockTargetCommandID = arg->command_id;

        _roboDockTargetPos = arg->tgt_pos;
        _roboDockAggr = arg->aggr;

        if ( arg->tgType == BACT_TGT_TYPE_UNIT )
            _roboDockTargetPos = arg->tgt_bact->_position;
    }
    else
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_pos = arg->tgt_pos;
        targetMsg.priority = 0;
        targetMsg.tgt.pbact = arg->tgt_bact;
        targetMsg.tgt_type = arg->tgType;

        selectedCommander->SetTarget(&targetMsg);

        selectedCommander->setBACT_aggression(arg->aggr);
    }

    return selectedCommander;
}



void NC_STACK_yparobo::buildConquer()
{
    TForcesRatio forcesRatio;
    forcesRatio.flags = 2;
    forcesRatio.pos = World::SectorIDToCenterPos3( {_roboVehicleCellID % _wrldSectors.x, _roboVehicleCellID / _wrldSectors.x} );

    GetForcesRatio(&forcesRatio);

    int neededEnergy = (forcesRatio.enemyEnergy - forcesRatio.ownEnergy) * 0.5;

    if ( neededEnergy < 15000 )
        neededEnergy = 15000;

    TSectorInfo sectInfo;
    sectInfo.pos_x = forcesRatio.pos.x;
    sectInfo.pos_z = forcesRatio.pos.z;

    _world->GetSectorInfo(&sectInfo);

    int enrg = 0;

    for ( NC_STACK_ypabact* &cell_unit : sectInfo.pcell->unitsList )
    {
        if ( _owner != cell_unit->_owner && cell_unit->_bact_type == BACT_TYPES_GUN && (cell_unit->_weapon != -1 || cell_unit->_mgun != -1) )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( cell_unit );
            int isRoboGun = gun->IsRoboGun();

            if ( !isRoboGun )
            {
                for (NC_STACK_ypabact* &bctt : _world->_unitsList)
                {
                    if ( bctt->_bact_type == BACT_TYPES_ROBO && bctt->_status != BACT_STATUS_DEAD && cell_unit->_owner == bctt->_owner )
                    {
                        int hostEnergy = bctt->_energy_max / 2;

                        if ( hostEnergy < 120000 )
                            hostEnergy = 120000;

                        enrg = hostEnergy;
                        break;
                    }
                }
            }
        }

        if ( enrg )
            break;
    }

    if ( enrg > neededEnergy )
        neededEnergy = enrg;

    TRayIntersection ray;
    ray.stPos = _position + _roboDockPos - vec3d::OY(100.0);
    ray.vect = vec3d::OY(20000.0);
    ray.flags = 0;

    _world->IntersectRay(&ray);

    if ( _pSector->height - ray.isectPos.y >= 50.0 )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_pos.x = _position.x + 200.0;
        targetMsg.tgt_pos.z = _position.z + 300.0;
        targetMsg.priority = 0;
        targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

        SetTarget(&targetMsg);
    }
    else
    {
        robo_loct1 loct = robo_loct1(); // Not every path fills every field

        loct.distance = (forcesRatio.pos.XZ() - _position.XZ()).length();
        loct.tgt_pos = forcesRatio.pos.X0Z();

        loct.tgt_bact = NULL;

        if ( neededEnergy <= 20000 )
            loct.energ = 20000;
        else
            loct.energ = neededEnergy;

        loct.tgType = BACT_TGT_TYPE_CELL;

        if ( _roboVehicleCell->PurposeType == cellArea::PT_POWERSTATION )
            loct.aggr = 49;
        else
            loct.aggr = 60;

        loct.forbidden = 1;
        loct.want = 32;
        loct.good = 130;
        loct.bad = 0;
        loct.job = 6;

        AllocForce(&loct);

        _roboConqDelay = _roboTimeScale * (100 - _roboEpConquer) / 100;

        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildDefense()
{
    setTarget_msg targetMsg;
    targetMsg.priority = _roboVehicleCommandID;

    if ( GetTargetByCommandID(&targetMsg) )
    {
        TSummaryQuery squadEnergyQuery;
        squadEnergyQuery.enrg_sum = 0;
        squadEnergyQuery.enrg_type = 1;

        targetMsg.tgt.pbact->GetSummary(&squadEnergyQuery);

        int neededEnergy = squadEnergyQuery.enrg_sum / 2;

        if ( neededEnergy < 15000 )
            neededEnergy = 15000;

        if ( targetMsg.tgt.pbact->_bact_type == BACT_TYPES_GUN && (targetMsg.tgt.pbact->_weapon != -1 || targetMsg.tgt.pbact->_mgun != -1) )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( targetMsg.tgt.pbact );
            int isRoboGun = gun->IsRoboGun();

            if ( !isRoboGun )
            {
                for (NC_STACK_ypabact* &node : _world->_unitsList)
                {
                    if ( node->_bact_type == BACT_TYPES_ROBO && node->_status != BACT_STATUS_DEAD && targetMsg.tgt.pbact->_owner == node->_owner )
                    {
                        int hostEnergy = node->_energy_max / 2;

                        if ( hostEnergy < 120000 )
                            hostEnergy = 120000;

                        neededEnergy = hostEnergy;
                        break;
                    }
                }
            }
        }

        TRayIntersection ray;
        ray.stPos = _position + _roboDockPos - vec3d::OY(100.0);
        ray.vect = vec3d::OY(20000.0);
        ray.flags = 0;

        _world->IntersectRay(&ray);

        if ( _pSector->height - ray.isectPos.y >= 50.0 )
        {
            targetMsg.tgt_pos.x = _position.x + 200.0;
            targetMsg.tgt_pos.z = _position.z + 300.0;
            targetMsg.priority = 0;
            targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;

            SetTarget(&targetMsg);
        }
        else
        {
            robo_loct1 loct = robo_loct1(); // Not every path fills every field
            loct.distance = (targetMsg.tgt.pbact->_position.XZ() - _position.XZ()).length();
            loct.tgt_pos = targetMsg.tgt.pbact->_position;

            int cappedEnergy = neededEnergy;
            if ( cappedEnergy >= 40000 )
                cappedEnergy = 40000;

            loct.energ = cappedEnergy;
            loct.tgType = BACT_TGT_TYPE_UNIT;
            loct.tgt_bact = targetMsg.tgt.pbact;
            loct.command_id = _roboVehicleCommandID;
            loct.aggr = 60;

            if ( targetMsg.tgt.pbact->_bact_type != BACT_TYPES_TANK && targetMsg.tgt.pbact->_bact_type != BACT_TYPES_CAR && targetMsg.tgt.pbact->_bact_type != BACT_TYPES_HOVER )
            {
                loct.bad = 2;
                loct.want = 32;
                loct.good = 852;
                loct.forbidden = 129;
            }
            else
            {
                loct.want = 32;
                loct.good = 578;
                loct.bad = 256;
                loct.forbidden = 1;
            }

            switch ( targetMsg.tgt.pbact->_bact_type )
            {
            case BACT_TYPES_TANK:
            case BACT_TYPES_CAR:
            case BACT_TYPES_HOVER:
                loct.job = 2;
                break;

            case BACT_TYPES_FLYER:
            case BACT_TYPES_UFO:
                loct.job = 3;
                break;

            default:
                loct.job = 4;
                break;
            }

            AllocForce(&loct);

            _roboEnemyDelay = _roboTimeScale * (100 - _roboEpDefense) / 100;

            _roboVehicleDuty = 0;
        }
    }
    else
    {
        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildRobo()
{
    setTarget_msg targetMsg;
    targetMsg.priority = _roboVehicleCommandID;
    if ( GetTargetByCommandID(&targetMsg) )
    {
        TRayIntersection ray;
        ray.stPos = _position + _roboDockPos - vec3d::OY(100.0);
        ray.vect = vec3d::OY(20000.0);
        ray.flags = 0;

        _world->IntersectRay(&ray);

        if ( _pSector->height - ray.isectPos.y >= 50.0 )
        {
            targetMsg.tgt_pos.x = _position.x + 200.0;
            targetMsg.tgt_pos.z = _position.z + 300.0;
            targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
            targetMsg.priority = 0;
            SetTarget(&targetMsg);
        }
        else
        {
            robo_loct1 loct = robo_loct1(); // Not every path fills every field
            loct.distance = (targetMsg.tgt.pbact->_position.XZ() - _position.XZ()).length();
            loct.tgt_pos = targetMsg.tgt.pbact->_position;

            int forceEnergy = targetMsg.tgt.pbact->_energy_max / 2;
            if ( forceEnergy < 120000 )
                forceEnergy = 120000;

            loct.energ = forceEnergy;
            loct.tgType = BACT_TGT_TYPE_UNIT;
            loct.tgt_bact = targetMsg.tgt.pbact;
            loct.command_id = _roboVehicleCommandID;
            loct.aggr = 60;
            loct.want = 32;
            loct.good = 852;
            loct.bad = 2;
            loct.forbidden = 1;
            loct.job = 1;
            _roboState |= ROBOSTATE_BUILDROBO;

            NC_STACK_ypabact *force = AllocForce(&loct);
            if ( force )
            {
                for ( TRoboAttackSlot &ataker : _roboAttackers )
                {
                    if ( !ataker.squadCommandID )
                    {
                        ataker.squadCommandID = force->_commandID;
                        ataker.targetCommandID = targetMsg.tgt.pbact->_commandID;
                        break;
                    }
                }
            }

            _roboDangerDelay = _roboTimeScale * (100 - _roboEpRobo) / 100;

            _roboVehicleDuty = 0;
        }
    }
    else
    {
        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildReconnoitre()
{
    TRayIntersection ray;
    ray.stPos = _position + _roboDockPos - vec3d::OY(100.0);
    ray.vect = vec3d::OY(20000.0);
    ray.flags = 0;

    _world->IntersectRay(&ray);

    if ( _pSector->height - ray.isectPos.y >= 50.0 )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_pos.x = _position.x + 200.0;
        targetMsg.tgt_pos.z = _position.z + 300.0;
        targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
        targetMsg.priority = 0;

        SetTarget(&targetMsg);
    }
    else
    {
        robo_loct1 loct = robo_loct1(); // Not every path fills every field
        loct.tgt_pos = World::SectorIDToCenterPos3( {_roboVehicleCellID % _wrldSectors.x, _roboVehicleCellID / _wrldSectors.x} );

        loct.energ = 10;
        loct.aggr = 25;
        loct.forbidden = 0;
        loct.want = 1024;
        loct.good = 9;
        loct.job = 5;
        loct.tgType = BACT_TGT_TYPE_CELL;
        loct.bad = 32;
        loct.distance = (loct.tgt_pos.XZ() - _position.XZ()).length();
        loct.tgt_bact = NULL;

        NC_STACK_ypabact *force = AllocForce(&loct);

        if ( force )
            force->_aggr = 25;

        _roboExploreDelay = _roboTimeScale * (100 - _roboEpRecon) / 100;

        _roboVehicleDuty = 0;
    }
}


void NC_STACK_yparobo::AI_doWish(update_msg *arg)
{
    if ( !_roboBuildingDuty )
    {
        int found_time = 0;
        int found = 0;

        if ( _roboState & ROBOSTATE_READYPOWER )
        {
            if ( _roboPowerValue )
            {
                if ( -_roboPowerDelay > found_time )
                {
                    found = ROBOSTATE_BUILDPOWER;
                    found_time = -_roboPowerDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYRADAR )
        {
            if ( _roboRadarValue )
            {
                if ( -_roboRadarDelay > found_time )
                {
                    found = ROBOSTATE_BUILDRADAR;
                    found_time = -_roboRadarDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYSAFE )
        {
            if ( _roboSafetyValue )
            {
                if ( -_roboSafetyDelay > found_time )
                {
                    found = ROBOSTATE_BUILDSAFE;
                    found_time = -_roboSafetyDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_FOUNDPLACE )
        {
            if ( _roboPositionValue )
            {
                if ( -_roboPositionDelay > found_time )
                {
                    found = ROBOSTATE_CHANGEPLACE;
                    found_time = -_roboPositionDelay;
                }
            }
        }

        if ( found_time > 0 )
        {
            if ( found == ROBOSTATE_BUILDRADAR )
            {
                _roboRadarValue = 0;
                _roboRadarCellIndex = 0;
                _roboBuildingDuty = ROBOSTATE_BUILDRADAR;
                _roboBuildingCellID = _roboRadarCellID;
                _roboBuildingCell = _roboRadarCell;
                _roboState &= ~ROBOSTATE_READYRADAR;
            }
            else if ( found == ROBOSTATE_BUILDPOWER )
            {
                _roboPowerValue = 0;
                _roboPowerCellIndex = 0;
                _roboBuildingDuty = ROBOSTATE_BUILDPOWER;
                _roboBuildingCellID = _roboPowerCellID;
                _roboState &= ~ROBOSTATE_READYPOWER;
                _roboBuildingCell = _roboPowerCell;
            }
            else if ( found == ROBOSTATE_BUILDSAFE )
            {
                _roboSafetyValue = 0;
                _roboSafetyCellIndex = 0;
                _roboBuildingCellID = _roboSafetyCellID;
                _roboBuildingCell = _roboSafetyCell;
                _roboBuildingDuty = ROBOSTATE_BUILDSAFE;
                _roboState &= ~ROBOSTATE_READYSAFE;
            }
            else if ( found == ROBOSTATE_CHANGEPLACE )
            {
                _roboPositionValue = 0;
                _roboPositionCellIndex = 0;
                _roboBuildingCellID = _roboPositionCellID;
                _roboBuildingCell = _roboPositionCell;
                _roboBuildingDuty = ROBOSTATE_CHANGEPLACE;
                _roboState &= ~ROBOSTATE_FOUNDPLACE;
            }
        }
    }

    if ( !_roboVehicleDuty && !(_roboState & ROBOSTATE_DOCKINUSE) )
    {
        int found_time = 0;
        int found = 0;

        if ( _roboState & ROBOSTATE_READYDEFENSE )
        {
            if ( _roboEnemyValue )
            {
                if ( -_roboEnemyDelay > found_time )
                {
                    found = ROBOSTATE_BUILDDEFENSE;
                    found_time = -_roboEnemyDelay;
                }
            }
        }

        if ( _roboState & ROBOSTATE_READYCONQUER )
        {
            if ( _roboConqValue )
            {
                if ( -_roboConqDelay > found_time )
                {
                    found = ROBOSTATE_BUILDCONQUER;
                    found_time = -_roboConqDelay;
                }
            }
        }

        if ( _roboState & ROBOSTATE_READYRECON )
        {
            if ( _roboExploreValue )
            {
                if ( -_roboExploreDelay > found_time )
                {
                    found = ROBOSTATE_BUILDRECON;
                    found_time = -_roboExploreDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYROBO )
        {
            if ( _roboDangerValue )
            {
                if ( -_roboDangerDelay > found_time )
                {
                    found = ROBOSTATE_BUILDROBO;
                    found_time = -_roboDangerDelay;
                }
            }
        }

        if ( found_time > 0 )
        {
            if ( found == ROBOSTATE_BUILDCONQUER )
            {
                _roboConqValue = 0;
                _roboConqCellIndex = 0;
                _roboVehicleCellID = _roboConqCellID;
                _roboVehicleCell = _roboConqCell;
                _roboVehicleDuty = ROBOSTATE_BUILDCONQUER;
                _roboState &= ~ROBOSTATE_READYCONQUER;
            }
            else if ( found == ROBOSTATE_BUILDDEFENSE )
            {
                _roboVehicleDuty = ROBOSTATE_BUILDDEFENSE;
                _roboEnemyValue = 0;
                _roboVehicleCommandID = _roboEnemyCommandID;
                _roboState &= ~ROBOSTATE_READYDEFENSE;
                _roboVehicleCellID = _roboEnemyCellID;
                _roboVehicleCell = _roboEnemyCell;
            }
            else if ( found == ROBOSTATE_BUILDRECON )
            {
                _roboVehicleDuty = ROBOSTATE_BUILDRECON;
                _roboExploreValue = 0;
                _roboExploreCellIndex = 0;
                _roboVehicleCellID = _roboExploreCellID;
                _roboVehicleCell = _roboExploreCell;
                _roboState &= ~ROBOSTATE_READYRECON;
            }
            else if ( found == ROBOSTATE_BUILDROBO )
            {
                _roboDangerValue = 0;
                _roboVehicleCommandID = _roboDangerCommandID;
                _roboVehicleCellID = _roboDangerCellID;
                _roboVehicleCell = _roboDangerCell;
                _roboVehicleDuty = ROBOSTATE_BUILDROBO;
                _roboState &= ~ROBOSTATE_READYROBO;
            }
        }
    }

    switch (_roboBuildingDuty)
    {
    case ROBOSTATE_BUILDRADAR:
        buildRadar(arg);
        break;

    case ROBOSTATE_BUILDPOWER:
        buildPower(arg);
        break;

    case ROBOSTATE_BUILDSAFE:
        buildSafe(arg);
        break;

    case ROBOSTATE_CHANGEPLACE:
        changePlace();
        break;

    default:
        break;
    }

    switch (_roboVehicleDuty)
    {
    case ROBOSTATE_BUILDCONQUER:
        buildConquer();
        break;

    case ROBOSTATE_BUILDDEFENSE:
        buildDefense();
        break;

    case ROBOSTATE_BUILDRECON:
        buildReconnoitre();
        break;

    case ROBOSTATE_BUILDROBO:
        buildRobo();
        break;

    default:
        break;
    }
}

void NC_STACK_yparobo::AI_service(update_msg *arg)
{
    if ( _roboDockUser )
    {
        int dockUserFound = 0;
        for( NC_STACK_ypabact* &node : _kidList )
        {
            if (_roboDockUser == node->_commandID)
            {
                dockUserFound = 1;
                break;
            }
        }

        if ( !dockUserFound )
        {
            _roboDockTime = 0;
            _roboDockUser = 0;
            _roboState &= ~ROBOSTATE_DOCKINUSE;
        }
    }

    if ( _roboDockTime > 0 )
    {
        _roboDockTime -= arg->frameTime;
        if ( _roboDockTime <= 0 )
        {
            _roboDockTime = 0;

            if ( !_roboDockUser )
                _roboState &= ~ROBOSTATE_DOCKINUSE;
        }
    }

    if ( _roboDockTime <= 0 && !_roboDockUser )
        _roboState &= ~ROBOSTATE_DOCKINUSE;
}

void NC_STACK_yparobo::AI_solveTrouble(update_msg *arg)
{
    if ( (float)_energy / (float)_energy_max < 0.2 && (arg->gTime - _roboTestEnemyTime) > 20000 )
    {
        _roboTestEnemyTime = arg->gTime;

        int energyCritical = (float)_energy / (float)_energy_max < 0.1;

        for (NC_STACK_ypabact* &unit : _kidList )
        {
            if ( unit->_status == BACT_STATUS_NORMAL || unit->_status == BACT_STATUS_IDLE )
            {
                if ( unit->_bact_type != BACT_TYPES_GUN )
                {
                    int recall;

                    if ( energyCritical )
                    {
                        recall = 1;
                        unit->_status_flg &= ~BACT_STFLAG_UNUSE;
                    }
                    else
                    {
                        recall = !(unit->_status_flg & BACT_STFLAG_UNUSE) && !unit->_secndTtype;
                    }

                    float dx = unit->_position.x - _position.x;
                    float dz = unit->_position.z - _position.z;

                    if ( (dx * dx + dz * dz) < 4320000.0 && unit->_primTtype == BACT_TGT_TYPE_UNIT && this == unit->_primT.pbact )
                    {
                        if ( energyCritical )
                        {
                            unit->setBACT_aggression(99);
                            unit->_status_flg &= ~BACT_STFLAG_ESCAPE;
                        }
                        else
                        {
                            unit->setBACT_aggression(60);
                        }
                    }

                    if ( recall )
                    {
                        setTarget_msg targetMsg;

                        targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
                        targetMsg.priority = 0;
                        targetMsg.tgt.pbact = this;

                        unit->SetTarget(&targetMsg);
                    }
                }
            }
        }
    }
}

void NC_STACK_yparobo::AI_clearAttackerSlots()
{
    if ( _clock - _roboAttackersClearTime > 4000 )
    {
        _roboAttackersClearTime = _clock;
        for ( TRoboAttackSlot &ataker : _roboAttackers )
        {
            if ( ataker.squadCommandID )
            {
                int squadFound = 0;
                for (NC_STACK_ypabact* &node : _kidList )
                {
                    if (node->_commandID == ataker.squadCommandID)
                    {
                        squadFound = 1;
                        break;
                    }
                }

                if ( !squadFound )
                {
                    ataker.squadCommandID = 0;
                    ataker.targetCommandID = 0;
                }
            }
        }
    }
}

void NC_STACK_yparobo::searchEnemyRobo()
{
    for (NC_STACK_ypabact* &unit : _world->_unitsList)
    {
        if ( unit->_status != BACT_STATUS_DEAD &&
             unit != this && unit->_bact_type == BACT_TYPES_ROBO )
        {
            if ( unit->_pSector->IsCanSee(_owner) )
            {
                NC_STACK_ypabact *sel = NULL;
                float selfl = 0.0;

                for (NC_STACK_ypabact* &kbct : _kidList )
                {
                    if (kbct->_status != BACT_STATUS_DEAD)
                    {
                        float distSq = (kbct->_position.XZ() - unit->_position.XZ()).square();

                        if ( !sel || distSq < selfl )
                        {
                            sel = kbct;
                            selfl = distSq;
                        }
                    }
                }

                if ( sel )
                {
                    TRoboMessage msg;

                    msg.msgID = 6;
                    msg.param1 = unit->_owner;
                    msg.priority = 48;
                    msg.unit = sel;
                    msg.param2 = 0;
                    msg.param3 = 0;

                    _world->PlaceMessage(&msg);
                }
            }
        }
    }
}

void NC_STACK_yparobo::usersRoboEnergyCheck()
{
    float energyRatio = (float)_energy / (float)_energy_max;

    if ( energyRatio < 0.2 )
    {
        if ( energyRatio < 0.07 )
            energyRatio = 0.01;

        TRoboMessage msg;
        msg.msgID = 40;
        msg.param2 = 0;
        msg.param3 = 0;
        msg.priority = 98;
        msg.param1 = (energyRatio * 60000.0 * 5.0);
        msg.unit = this;

        _world->PlaceMessage(&msg);
    }
}

void NC_STACK_yparobo::checkCommander()
{
    bool userRobo =  (this == _world->UserRobo());

    float energyRatio;

    for ( NC_STACK_ypabact* &commander : _kidList )
    {
        if ( commander->_status == BACT_STATUS_DEAD )
            continue;

        if ( _roboDockUser == commander->_commandID && _roboDockUser )
        {
            InitForce(commander);
            continue;
        }
        
        if ( commander->_bact_type == BACT_TYPES_GUN )
            continue;

        if ( commander->_status_flg & BACT_STFLAG_ESCAPE )
        {
            if ( commander->GetFightMotivation(&energyRatio) )
            {
                if ( commander->_owner == commander->_pSector->owner && commander->_pSector->energy_power )
                {
                    setTarget_msg targetMsg;
                    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                    targetMsg.priority = 0;
                    targetMsg.tgt_pos = commander->_position;

                    commander->SetTarget(&targetMsg);
                }

                commander->ChangeEscapeFlag(false);

                if ( userRobo )
                {
                    TRoboMessage msg;
                    msg.param1 = commander->_commandID;
                    msg.msgID = 10;
                    msg.param3 = 0;
                    msg.param2 = 0;
                    msg.unit = commander;
                    msg.priority = 28;

                    _world->PlaceMessage(&msg);
                }
            }
            continue;
        }

        if ( commander->GetFightMotivation(&energyRatio) )
        {
            if ( energyRatio < 0.5 && userRobo )
            {
                TSummaryQuery summary;
                summary.enrg_sum = 0;
                summary.enrg_type = 3;

                commander->GetSummary(&summary);

                int squadSize = summary.enrg_sum;

                summary.enrg_sum = 0;
                summary.enrg_type = 5;
                commander->GetSummary(&summary);

                if ( squadSize < summary.enrg_sum )
                {
                    TRoboMessage msg;
                    msg.param1 = 20000;
                    msg.param3 = 0;
                    msg.param2 = 0;
                    msg.msgID = 18;
                    msg.unit = commander;
                    msg.priority = 42;

                    _world->PlaceMessage(&msg);
                }
            }

            if ( !userRobo )
            {
                if ( _clock - _roboAttackersTime > 1000 )
                {
                    _roboAttackersTime = _clock;

                    for ( TRoboAttackSlot &ataker : _roboAttackers )
                    {
                        if ( ataker.squadCommandID == commander->_commandID)
                        {
                            UpdateRoboAttackSquad(commander, &ataker);
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            commander->ChangeEscapeFlag(true);


            commander->_status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
            commander->_waypoints_count = 0;
            commander->_status_flg &= ~BACT_STFLAG_UNUSE;


            for (NC_STACK_ypabact* &nod : commander->_kidList)
                nod->_status_flg &= ~BACT_STFLAG_UNUSE;

            if ( commander->IsNeedsWaypoints() )
            {
                TPathRequest pathRequest;
                pathRequest.steps_cnt = 32;
                pathRequest.from = commander->_position.XZ();
                pathRequest.to = _position.XZ();
                pathRequest.tgt_type = 1;

                if ( ! commander->PathFinder(&pathRequest) )
                    // TODO(bug): Leaves the commander loop, so one failed path skips every remaining
                    // commander this frame. Probably meant continue. The first decompile has
                    // the same.
                    break;

                if ( pathRequest.steps_cnt > 1 )
                {
                    pathRequest.steps_cnt = 32;
                    commander->SetPath(&pathRequest);

                    commander->_m_cmdID = _commandID;
                    commander->_m_owner = _owner;
                }
                else
                {
                    setTarget_msg targetMsg;
                    targetMsg.tgt.pbact = this;
                    targetMsg.priority = 0;
                    targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
                    commander->SetTarget(&targetMsg);
                }
            }
            else
            {
                setTarget_msg targetMsg;
                targetMsg.tgt.pbact = this;
                targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
                targetMsg.priority = 0;
                commander->SetTarget(&targetMsg);
            }

            if ( userRobo )
            {
                TRoboMessage msg;
                msg.param1 = commander->_commandID;
                msg.param3 = 0;
                msg.param2 = 0;
                msg.msgID = 9;
                msg.unit = commander;
                msg.priority = 40;

                _world->PlaceMessage(&msg);

                if ( _world->_isNetGame  )
                {
                    NC_STACK_ypabact *enemy = commander->GetSectorTarget(commander->_cellId);
                    if ( enemy )
                    {
                        uamessage_logmsg logMsg;
                        logMsg.msgID = UAMSG_LOGMSG;
                        logMsg.owner = _owner;
                        logMsg.sender = enemy->_gid;
                        logMsg.senderOwner = enemy->_owner;
                        logMsg.pr1 = 0;
                        logMsg.pr2 = 0;
                        logMsg.pr3 = 0;
                        logMsg.pri = 34;
                        logMsg.id = 19;

                        _world->NetBroadcastMessage(&logMsg, sizeof(logMsg), true);
                    }
                }
            }
            else
            {
                for ( const TBactAttacker &ainf : commander->_attackersList )
                {
                    if (_world->LocalOwner() == ainf.attacker->_owner)
                    {                     
                        NC_STACK_ypabact *realbact = ainf.attacker;                  
                        if ( realbact->_bact_type == BACT_TYPES_MISSLE ) //If missile
                        {
                            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(realbact);
                            realbact = miss->GetLauncherBact(); //Get emitter bact
                        }

                        if ( realbact->_host_station != realbact->_parent &&
                             realbact->_parent )
                            realbact = realbact->_parent;

                        TRoboMessage msg;
                        msg.param1 = 0;
                        msg.param3 = 0;
                        msg.param2 = 0;
                        msg.msgID = 19;
                        msg.unit = realbact;
                        msg.priority = 34;
                        _world->PlaceMessage(&msg);
                        break;
                    }
                }
            }
        }
    }
}

void NC_STACK_yparobo::checkDanger()
{
    int enemyNear = 0;
    int enemyCommandId;

    for(int i = -1; i <= 1; i++)
    {
        for(int j = -1; j <= 1; j++)
        {
            Common::Point pt = _cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                for ( NC_STACK_ypabact* &unit : _world->SectorAt(pt).unitsList )
                {
                    // TODO(bug): Only enemies with both a weapon and a machine gun count, so units with
                    // just one are ignored. Probably meant ||, as in the gun checks. The
                    // first decompile has the same.
                    if (unit->_owner != _owner && unit->_status != BACT_STATUS_DEAD && unit->_weapon != -1 && unit->_mgun != -1)
                    {
                        enemyNear = 1;
                        enemyCommandId = unit->_commandID;
                        break;
                    }
                }
            }

        }
    }

    if ( enemyNear )
    {
        if ( !(_roboState & ROBOSTATE_DANGER) && _roboState & ROBOSTATE_PLAYERROBO )
        {
            TRoboMessage msg;
            msg.msgID = 13;
            msg.param1 = enemyCommandId;
            msg.param3 = 0;
            msg.param2 = 0;
            msg.unit = this;
            msg.priority = 99;

            _world->PlaceMessage(&msg);
        }

        _roboState |= ROBOSTATE_DANGER;
    }
    else
    {
        _roboState &= ~ROBOSTATE_DANGER;
    }
}

int NC_STACK_yparobo::RateSectorForRadar(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

//  if ( cell->field_2E != 1 )
//  {
//      for (int i = 0; i < 3; i++)
//      {
//          for (int j = 0; j < 3; j++)
//          {
//
//          }
//      }
//  }

    if ( cell.owner != _owner )
        return -1;

    if ( cell.PurposeType != cellArea::PT_NONE )
        return -1;

    float distance = _cellId.LengthTo<float>( sc );

    float rating = ( distance <= 0.01 ? 500.0 : 1000.0 / distance );

    int hasUnseenNeighbor = 0;

    for (Common::Point ds : NearDxy)
    {
        if ( !_world->SectorAt(sc + ds).IsCanSee(_owner) )
        {
            hasUnseenNeighbor = 1;
            rating += 10.0;
        }
    }

    if (  !hasUnseenNeighbor || _world->IsSectorBorder(sc, 2) )
        return -1;

    return rating;
}

int NC_STACK_yparobo::RateSectorForRecon(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( cell.IsCanSee(_owner) )
        return -1;

    float rating = _cellId.LengthTo<float>( sc );

    if ( rating <= 0.01 )
        rating = 0.0;

    for (Common::Point ds : NearDxy)
    {
        if ( !_world->SectorAt(sc + ds).IsCanSee(_owner) )
        {
            rating += 5.0;
        }
    }

    return rating;
}

int NC_STACK_yparobo::RateSectorForFlak(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

//  if ( cell->field_2E != 1 )
//  {
//      for (int i = 0; i < 3; i++)
//      {
//          for (int j = 0; j < 3; j++)
//          {
//
//          }
//      }
//  }

    if ( cell.owner != _owner || cell.PurposeType != cellArea::PT_NONE )
        return -1;

    float distance = _cellId.LengthTo<float>( sc );

    float rating;

    if ( distance <= 0.01 )
        rating = 500.0;
    else
        rating = 1000.0 / distance;

    for (Common::Point ds : NearDxy)
    {
        cellArea &cll = _world->SectorAt(sc + ds);

        if ( cll.PurposeType != cellArea::PT_NONE && _owner == cll.owner)
            rating += 5.0;

    }

    return rating;
}

int NC_STACK_yparobo::RateSectorForPowerStation(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

    if ( cell.energy_power >= 255 )
        return -1;

    if ( cell.PurposeType != cellArea::PT_NONE )
        return -1;

//  if ( cell->field_2E != 1 )
//  {
//      for (int i = 0; i < 3; i++)
//      {
//          for (int j = 0; j < 3; j++)
//          {
//
//          }
//      }
//  }

    if ( cell.owner != _owner )
        return -1;

    float distance = _cellId.LengthTo<float>( sc );
    float distanceRating = distance <= 0.01 ? 500.0 : 1000.0 / distance;

    if ( distance > 8.0 )
        return -1;

    float rating = distanceRating + (255 - cell.energy_power) / 3.0;

    for (Common::Point ds : NearDxy)
    {
        cellArea &cll = _world->SectorAt(sc + ds);

        if ( cll.owner == _owner )
        {
            int neighborEnergy = cll.GetEnergy();
            rating += (255 - neighborEnergy) * 0.05 + 10.0;
        }

        if ( cll.PurposeType == cellArea::PT_POWERSTATION )
            rating *= 0.7;
    }

    rating -= 5.0 * cell.unitsList.size();

    return rating;
}

int32_t NC_STACK_yparobo::RateSectorForConquer(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);
    
    if ( cell.owner == _owner )
        return -1;

    float distance = _cellId.LengthTo<float>( sc );
    int32_t val = 0;

    if ( distance <= 0.01 )
        val = 1500;
    else
        val = 1000.0 / distance;

    if ( cell.IsCanSee(_owner) )
    {
        if ( cell.PurposeType != cellArea::PT_NONE )
            val = 20;
        if ( cell.PurposeType == cellArea::PT_GATEOPENED )
            val += 40;
        if ( cell.PurposeType == cellArea::PT_POWERSTATION )
            val += 50;
    }

    for ( Common::Point ds : NearDxy )
    {
        Common::Point pt = sc + ds;

        if ( _world->IsGamePlaySector(pt))
        {
            if ( _world->SectorAt(pt).owner == _owner )
                val += 3;
        }
    }

    if ( cell.IsCanSee(_owner) )
    {
        for ( NC_STACK_ypabact* &bct : cell.unitsList )
        {
            if ( bct->_owner != _owner && bct->_owner)
            {
                if ( bct->_bact_type != BACT_TYPES_MISSLE)
                {
                    if ( bct->_bact_type == BACT_TYPES_ROBO)
                        val += 100;
                    else
                        val += 5;
                }
            }
        }
    }

    return val;
}

int NC_STACK_yparobo::CountOwnFlakSectors()
{
    int count = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.PurposeType == cellArea::PT_BUILDINGS )
        {
            for ( NC_STACK_ypabact* &bct : cll.unitsList )
            {
                if ( bct->_bact_type == BACT_TYPES_GUN && bct->_status != BACT_STATUS_DEAD && _owner == bct->_owner )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                    int isRoboGun = gun->IsRoboGun();

                    if ( isRoboGun == 0 && ( bct->_weapon != -1 || bct->_mgun != -1 ) )
                    {
                        count++;
                        break;
                    }
                }
            }
        }
    }

    return count;
}

int NC_STACK_yparobo::CountOwnRadarSectors()
{
    int count = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.PurposeType == cellArea::PT_BUILDINGS )
        {
            for (NC_STACK_ypabact* &bct : cll.unitsList )
            {
                if ( bct->_bact_type == BACT_TYPES_GUN && bct->_status != BACT_STATUS_DEAD && _owner == bct->_owner )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                    int isRoboGun = gun->IsRoboGun();

                    if ( isRoboGun == 0 && bct->_weapon == -1 && bct->_mgun == -1 )
                    {
                        count++;
                        break;
                    }
                }
            }
        }
    }

    return count;
}

int NC_STACK_yparobo::CountOwnedSectors()
{
    int count = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.owner == _owner )
            count++;
    }

    return count;
}

int NC_STACK_yparobo::CountUnseenSectors()
{
    int count = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( !cll.IsCanSee(_owner) )
            count++;
    }

    return count;
}

int NC_STACK_yparobo::IsEnemyRoboNear()
{
    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_bact_type == BACT_TYPES_ROBO)
            {
                if ( _owner != node->_owner )
                {
                    Common::Point dist = _cellId.AbsDistance( node->_cellId );
                    if ( dist.x < 3 && dist.y < 3 )
                        return 1;
                }
            }
        }
    }

    return 0;
}

int NC_STACK_yparobo::IsEnemyUnitNear()
{
    for (int i = -2; i <= 2; i++)
    {
        for (int j = -2; j <= 2; j++)
        {
            Common::Point pt = _cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                for ( NC_STACK_ypabact* &bct : _world->SectorAt(pt).unitsList )
                {
                    if ( bct->_owner )
                    {
                        if ( bct->_owner != _owner )
                        {
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_ROBO && bct->_status != BACT_STATUS_DEAD )
                                return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}


float NC_STACK_yparobo::RateEnemyTarget(NC_STACK_ypabact *bact)
{
    if ( bact->_status_flg & BACT_STFLAG_DEATH1 )
        return -1.0;

    float rating = _cellId.LengthTo<float>( bact->_cellId );

    if (rating != 0.0)
        rating = 100.0 / rating;
    else
        rating = 200.0;

    if ( bact->_pSector->owner == _owner )
        rating += 100.0;

    return rating;
}

int NC_STACK_yparobo::FindDefenseTarget(int *commandId, Common::Point *pCellId)
{
    float bestRating = -0.5;

    int bestCommandId = 0;
    
    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_owner != _owner  &&  node->_owner  &&  node->_status != BACT_STATUS_DEAD )
            {
                Common::Point dist = _cellId.AbsDistance( node->_cellId );
                if ( dist.x <= 2 && dist.y <= 2 )
                {
                    *commandId = node->_commandID;
                    *pCellId = node->_cellId;

                    return 500;
                }

                for(NC_STACK_ypabact* &ndbct : node->_kidList)
                {

                    if ( (ndbct->_gid & 0xF) == _roboEnemyDiv )
                    {
                        int isRoboGun = 0;

                        if ( ndbct->_bact_type == BACT_TYPES_GUN )
                        {
                            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( ndbct );
                            isRoboGun = gun->IsRoboGun();
                        }

                        if ( ndbct->_pSector->IsCanSee(_owner) )
                        {
                            if ( ndbct->_status != BACT_STATUS_DEAD && !isRoboGun )
                            {
                                float tmp = RateEnemyTarget(ndbct);
                                if ( tmp > bestRating )
                                {
                                    bestRating = tmp;
                                    bestCommandId = ndbct->_commandID;
                                    *pCellId = ndbct->_cellId;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    _roboEnemyDiv++;

    if ( _roboEnemyDiv > 15 )
        _roboEnemyDiv = 0;

    if ( bestRating < 0.0 )
    {
        *commandId = 0;
        return -1;
    }

    *commandId = bestCommandId;
    return bestRating;
}

int NC_STACK_yparobo::FindEnemyRoboTarget(int *commandId, Common::Point *pCellId)
{
    float bestRating = -0.5;
    int bestCommandId = 0;

    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_owner != _owner && node->_owner && node->_status != BACT_STATUS_DEAD )
            {
                if ( node->_pSector->IsCanSee(_owner) )
                {
                    float rating = RateEnemyTarget(node);
                    if ( rating > bestRating )
                    {
                        bestCommandId = node->_commandID;
                        bestRating = rating;
                        *pCellId = node->_cellId;
                    }
                }
            }
        }
    }

    if ( bestRating < 0.0 )
    {
        *commandId = 0;
        return -1;
    }

    *commandId = bestCommandId;
    return bestRating;
}


int NC_STACK_yparobo::RateAreaControl(const Common::Point &cellId)
{
    float controlRating = 0.0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            cellArea &cll = _world->SectorAt( cellId + Common::Point(i, j) );

            TReloadRatios reloadRatios;
            reloadRatios.owner = cll.owner;

            _world->GetReloadRatios(&reloadRatios);

            float cellWeight;

            if ( i || j )
                cellWeight = 1.0;
            else
                cellWeight = 3.0;

            if ( _owner == cll.owner )
                controlRating += cll.energy_power * reloadRatios.ratioClamped + cellWeight;
            else
                controlRating -= cll.energy_power * reloadRatios.ratioClamped + cellWeight;

            for ( NC_STACK_ypabact* &bct : cll.unitsList )
            {
                float unitWeight = 0;

                if ( bct->_bact_type == BACT_TYPES_ROBO && bct != this )
                {
                    unitWeight = 5.0;
                }
                else
                {
                    if ( bct->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

                        if ( !gun->IsRoboGun() )
                            unitWeight = 1.0;
                    }
                }

                if ( unitWeight > 0.0 )
                {
                    if ( bct->_owner == _owner )
                        controlRating += unitWeight;
                    else
                        controlRating -= unitWeight;
                }
            }
        }
    }

    return controlRating;
}

int NC_STACK_yparobo::RateSectorForRelocation()
{
    Common::Point pt( _roboPositionCellIndex % _wrldSectors.x, _roboPositionCellIndex / _wrldSectors.x);

    int currentRating = RateAreaControl(_cellId);
    int targetRating = RateAreaControl( pt );

    float distance = _cellId.LengthTo<float>( pt );

    if ( distance > 0.0 && currentRating < targetRating )
        targetRating = ((float)targetRating * (1.0 - distance * 0.8 / 91.0));

    if ( targetRating <= currentRating )
        return -1;

    return targetRating;
}

void NC_STACK_yparobo::AI_checkWorld(update_msg *arg)
{
    int doSearch = 1;
    _roboConqDelay -= arg->frameTime;
    if ( _roboConqDelay > 0 )
        doSearch = 0;

    TCommanderSearch commanderSearch;

    if ( _roboEpConquer && doSearch )
    {
        int scanCount = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for (int i = scanCount; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboConqCellIndex )
            {
                _roboConqCellIndex = 0;
                _roboState |= ROBOSTATE_READYCONQUER;
                return;
            }

            Common::Point pt (_roboConqCellIndex % _wrldSectors.x, _roboConqCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int conquerRating = RateSectorForConquer(pt);
                if ( conquerRating > _roboConqValue )
                {
                    commanderSearch.flags = 2;
                    commanderSearch.tgType = BACT_TGT_TYPE_CELL;
                    commanderSearch.tgt_pos = World::SectorIDToCenterPos3(pt);

                    FindMatchingCommander(&commanderSearch);

                    if ( !commanderSearch.found_comm )
                    {
                        _roboConqValue = conquerRating;
                        _roboConqCellID = _roboConqCellIndex;
                        _roboConqCell = &_world->SectorAt( _roboConqCellIndex );
                        _roboConqTime = arg->gTime;
                    }
                }
            }

            _roboConqCellIndex++;
        }
    }
    else
    {
        _roboConqCellIndex = 0;
        _roboConqValue = 0;
        _roboState &= ~ROBOSTATE_READYCONQUER;
    }

    doSearch = 1;
    _roboEnemyDelay -= arg->frameTime;

    if ( _roboEnemyDelay > 0 )
        doSearch = 0;

    if ( IsEnemyUnitNear() )
        doSearch = 1;

    if ( _roboEpDefense && doSearch )
    {
        Common::Point cellId;
        int enemyCommandId;

        int defenseRating = FindDefenseTarget(&enemyCommandId, &cellId);
        if ( defenseRating > _roboEnemyValue )
        {
            // TODO(bug): tgt is never set (tgt_pos is left from the conquer scan), but
            // FindMatchingCommander reads tgt when prim_comm_id is 0. Same problem as
            // the robo search below.
            commanderSearch.flags = 2;
            commanderSearch.tgType = BACT_TGT_TYPE_UNIT;
            commanderSearch.prim_comm_id = enemyCommandId;
            FindMatchingCommander(&commanderSearch);

            if ( !commanderSearch.found_comm )
            {
                _roboEnemyValue = defenseRating;
                _roboEnemyCommandID = enemyCommandId;
                _roboEnemyTime = arg->gTime;
                _roboEnemyCellID = cellId.y * _wrldSectors.x + cellId.x;
            }
        }
        _roboState |= ROBOSTATE_READYDEFENSE;
    }
    else
    {
        _roboEnemyValue = 0;
        _roboState &= ~ROBOSTATE_READYDEFENSE;
    }

    doSearch = 1;
    _roboDangerDelay -= arg->frameTime;
    if ( _roboDangerDelay > 0 )
        doSearch = 0;

    if ( IsEnemyRoboNear() )
        doSearch = 1;

    if ( _roboEpRobo && doSearch )
    {
        Common::Point cellId;
        int enemyCommandId;
        int enemyRoboRating = FindEnemyRoboTarget(&enemyCommandId, &cellId);

        if ( enemyRoboRating > _roboDangerValue )
        {
            TCommanderSearch roboSearch;
            // TODO(bug): tgt_pos and tgt are left unset, but FindMatchingCommander turns
            // tgt_pos into a sector for unit searches too, and reads tgt when
            // prim_comm_id is 0.
            roboSearch.flags = 2;
            roboSearch.tgType = BACT_TGT_TYPE_UNIT;
            roboSearch.prim_comm_id = enemyCommandId;

            FindMatchingCommander(&roboSearch);

            if ( !roboSearch.found_comm )
            {
                _roboDangerValue = enemyRoboRating;
                _roboDangerCommandID = enemyCommandId;
                _roboDangerTime = arg->gTime;
                _roboDangerCellID = _wrldSectors.x * cellId.y + cellId.x;
            }
        }
        _roboState |= ROBOSTATE_READYROBO;
    }
    else
    {
        _roboDangerValue = 0;
        _roboState &= ~ROBOSTATE_READYROBO;
    }

    doSearch = 1;
    _roboPositionDelay -= arg->frameTime;
    if ( _roboPositionDelay > 0 )
        doSearch = 0;

    if ( _roboEpChangePlace && doSearch )
    {
        int scanCount = (_wrldSectors.x + _wrldSectors.y) / 10 + 1;

        for (int i = scanCount; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboPositionCellIndex )
            {
                _roboPositionCellIndex = 0;
                _roboState |= ROBOSTATE_FOUNDPLACE;
                return;
            }

            if ( _world->IsGamePlaySector( {_roboPositionCellIndex % _wrldSectors.x, _roboPositionCellIndex / _wrldSectors.x} ) )
            {
                int relocationRating = RateSectorForRelocation();
                if ( relocationRating > _roboPositionValue )
                {
                    _roboPositionValue = relocationRating;
                    _roboPositionCellID = _roboPositionCellIndex;
                    _roboPositionTime = arg->gTime;
                }
            }
            _roboPositionCellIndex++;
        }
    }
    else
    {
        _roboPositionValue = 0;
        _roboPositionCellIndex = 0;
        _roboState &= ~ROBOSTATE_FOUNDPLACE;
    }

    TReloadRatios reloadRatios;
    reloadRatios.owner = _owner;
    _world->GetReloadRatios(&reloadRatios);

    doSearch = 1;
    _roboPowerDelay -= arg->frameTime;
    if ( _roboPowerDelay > 0 )
        doSearch = 0;

    if ( reloadRatios.ratioClamped == 0.0 )
    {
        if ( _roboEpPower )
            doSearch = 1;
    }

    float powerRatioThreshold;
    if ( _roboEpPower )
        powerRatioThreshold = 100.0 / (float)_roboEpPower * 0.9;
    else
        powerRatioThreshold = 0.89999998;

    if ( _roboEpPower && (reloadRatios.ratioPositive > powerRatioThreshold || reloadRatios.ratioClamped < 0.0001) && doSearch )
    {
        int scanCount = (_wrldSectors.x + _wrldSectors.y) / 20 + 1;
        for (int i = scanCount; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboPowerCellIndex )
            {
                _roboPowerCellIndex = 0;
                _roboState |= ROBOSTATE_READYPOWER;
                return;
            }
            
            Common::Point pt(_roboPowerCellIndex % _wrldSectors.x, _roboPowerCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int powerRating = RateSectorForPowerStation(pt);
                if ( powerRating > _roboPowerValue )
                {
                    _roboPowerValue = powerRating;
                    _roboPowerCell = &_world->SectorAt( _roboPowerCellIndex );
                    _roboPowerCellID = _roboPowerCellIndex;
                    _roboPowerTime = arg->gTime;
                }
            }
            _roboPowerCellIndex++;
        }
    }
    else
    {
        _roboPowerValue = 0;
        _roboPowerCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYPOWER;
    }

    doSearch = 1;
    _roboRadarDelay -= arg->frameTime;
    if ( _roboRadarDelay > 0 )
        doSearch = 0;

    if ( _roboEpRadar && (CountUnseenSectors() / 64 > CountOwnRadarSectors()) && doSearch )
    {
        int scanCount = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for(int i = scanCount; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboRadarCellIndex )
            {
                _roboRadarCellIndex = 0;
                _roboState |= ROBOSTATE_READYRADAR;
                return;
            }
            
            Common::Point pt(_roboRadarCellIndex % _wrldSectors.x, _roboRadarCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int radarRating = RateSectorForRadar(pt);

                if ( radarRating > _roboRadarValue )
                {
                    _roboRadarValue = radarRating;
                    _roboRadarCell = &_world->SectorAt( _roboRadarCellIndex );
                    _roboRadarCellID = _roboRadarCellIndex;
                    _roboRadarTime = arg->gTime;
                }
            }
            _roboRadarCellIndex++;
        }
    }
    else
    {
        _roboRadarCellIndex = 0;
        _roboRadarValue = 0;
        _roboState &= ~ROBOSTATE_READYRADAR;
    }


    doSearch = 1;
    _roboSafetyDelay -= arg->frameTime;
    if ( _roboSafetyDelay > 0 )
        doSearch = 0;

    int sectorsPerFlak;
    if ( _roboEpSafety )
        sectorsPerFlak = 10 * (100 / _roboEpSafety);
    else
        sectorsPerFlak = 10;

    if ( _roboEpSafety && (CountOwnedSectors() / sectorsPerFlak) >= CountOwnFlakSectors() && doSearch )
    {
        int scanCount = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for(int i = scanCount; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboSafetyCellIndex )
            {
                _roboSafetyCellIndex = 0;
                _roboState |= ROBOSTATE_READYSAFE;
                return;
            }
            
            Common::Point pt(_roboSafetyCellIndex % _wrldSectors.x, _roboSafetyCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int flakRating = RateSectorForFlak(pt);

                if ( flakRating > _roboSafetyValue )
                {
                    _roboSafetyValue = flakRating;
                    _roboSafetyCell = &_world->SectorAt( _roboSafetyCellIndex );
                    _roboSafetyCellID = _roboSafetyCellIndex;
                    _roboSafetyTime = arg->gTime;
                }
            }
            _roboSafetyCellIndex++;
        }
    }
    else
    {
        _roboSafetyValue = 0;
        _roboSafetyCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYSAFE;
    }


    doSearch = 1;
    _roboExploreDelay -= arg->frameTime;
    if ( _roboExploreDelay > 0 )
        doSearch = 0;

    if ( _roboEpRecon && doSearch )
    {
        int scanCount = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for (int i = scanCount; i > 0; i-- )
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboExploreCellIndex )
            {
                _roboExploreCellIndex = 0;
                _roboState |= ROBOSTATE_READYRECON;
                return;
            }

            Common::Point pt(_roboExploreCellIndex % _wrldSectors.x, _roboExploreCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int reconRating = RateSectorForRecon(pt);

                if ( reconRating > _roboExploreValue )
                {
                    TCommanderSearch reconSearch;
                    reconSearch.tgType = BACT_TGT_TYPE_CELL;
                    reconSearch.flags = 2;
                    reconSearch.tgt_pos = World::SectorIDToCenterPos3(pt);

                    FindMatchingCommander(&reconSearch);

                    if ( !reconSearch.found_comm )
                    {
                        _roboExploreValue = reconRating;
                        _roboExploreCellID = _roboExploreCellIndex;
                        _roboExploreCell = &_world->SectorAt( _roboExploreCellIndex );
                        _roboExploreTime = arg->gTime;
                    }
                }
            }
            _roboExploreCellIndex++;
        }
    }
    else
    {
        _roboExploreValue = 0;
        _roboExploreCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYRECON;
    }
}

void NC_STACK_yparobo::AI_layer3(update_msg *arg)
{
    int bactCollisions = getBACT_bactCollisions();

    float tmp = _target_vec.length();

    if ( tmp <= 0.0 )
        _target_dir = vec3d(0.0, 0.0, 0.0);
    else
        _target_dir = _target_vec / tmp;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _roboState & ROBOSTATE_MOVE )
            doBeamUpdate(arg->frameTime);

        if ( !bactCollisions || ! CollisionWithBact(arg->frameTime) )
        {
            checkCollisions(arg->frameTime * 0.001);

            if ( IsPlayerRobo() )
                wallow(arg);
            else
                AI_doMove(arg);

            if ( IsPlayerRobo() )
            {
                searchEnemyRobo();
                usersRoboEnergyCheck();
                //doUserCommands(arg);
            }

            if ( !IsPlayerRobo() )
            {
                AI_checkWorld(arg);
                AI_doWish(arg);
                AI_service(arg);
                AI_clearAttackerSlots();
            }

            checkCommander();

            checkDanger();

            if ( _roboState & ROBOSTATE_DANGER && !IsPlayerRobo() )
                AI_solveTrouble(arg);
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
}

void NC_STACK_yparobo::UpdateViewerLookAngles(update_msg *arg)
{
    TInputState *inpt = arg->inpt;
    float frameSec = arg->frameTime / 1000.0;


    if ( inpt->Sliders[1] < -0.001 )
    {
        _viewer_vert_angle += inpt->Sliders[1] * _maxrot * frameSec * 2.0;

        if ( _viewer_vert_angle > _viewer_max_up )
            _viewer_vert_angle = _viewer_max_up;

        if ( _viewer_vert_angle < -_viewer_max_down )
            _viewer_vert_angle = -_viewer_max_down;
    }
    else if (inpt->Sliders[1] > 0.001)
    {
        _viewer_vert_angle += inpt->Sliders[1] * _maxrot * frameSec * 2.0;

        if ( _viewer_vert_angle > _viewer_max_up )
            _viewer_vert_angle = _viewer_max_up;

        if ( _viewer_vert_angle < -_viewer_max_down )
            _viewer_vert_angle = -_viewer_max_down;
    }

    if ( inpt->Sliders[0] < -0.001 )
    {
        _viewer_horiz_angle -= inpt->Sliders[0] * _maxrot * frameSec * 2.0;

        if ( _viewer_max_side < 3.15 )
        {
            if ( _viewer_horiz_angle > _viewer_max_side )
                _viewer_horiz_angle = _viewer_max_side;

            if ( _viewer_horiz_angle < -_viewer_max_side )
                _viewer_horiz_angle = -_viewer_max_side;
        }
    }
    else if (inpt->Sliders[0] > 0.001)
    {
        _viewer_horiz_angle -= inpt->Sliders[0] * _maxrot * frameSec * 2.0;

        if ( _viewer_max_side < 3.15 )
        {
            if ( _viewer_horiz_angle > _viewer_max_side )
                _viewer_horiz_angle = _viewer_max_side;

            if ( _viewer_horiz_angle < -_viewer_max_side )
                _viewer_horiz_angle = -_viewer_max_side;
        }
    }
}

void NC_STACK_yparobo::User_layer(update_msg *arg)
{
    int bactCollisions = getBACT_bactCollisions();

    float targetDist = _target_vec.length();

    if ( targetDist <= 0.0 )
        _target_dir = vec3d(0.0, 0.0, 0.0);
    else
        _target_dir = _target_vec / targetDist;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _roboState & ROBOSTATE_MOVE )
            doBeamUpdate(arg->frameTime);

        if ( !bactCollisions || !CollisionWithBact(arg->frameTime) )
        {
            checkCommander();
            checkDanger();
            searchEnemyRobo();
            usersRoboEnergyCheck();

            if ( arg->inpt->Buttons.Is(3) )
                HandBrake(arg);

            //doUserCommands(arg);
            UpdateViewerLookAngles(arg);
            wallow(arg);

            _viewer_rotation = _rotation;

            _viewer_rotation = mat3x3::RotateY(_viewer_horiz_angle) * _viewer_rotation;
            _viewer_rotation = mat3x3::RotateX(_viewer_vert_angle) * _viewer_rotation;
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
    else if ( _status == BACT_STATUS_IDLE )
    {
        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_NORMAL;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;

        SetState(&stateMsg);

        _status_flg &= ~BACT_STFLAG_LAND;
    }
}

void NC_STACK_yparobo::Move(move_msg *arg)
{
    _old_pos = _position;

    vec3d gravity;
    if ( _status == BACT_STATUS_DEAD )
        gravity = vec3d::OY(_mass * 39.2266);
    else
        gravity = vec3d::OY(_mass * 9.80665);


    vec3d buoyancy(0.0, 0.0, 0.0);
    if ( !(arg->flag & 1) )
        buoyancy = vec3d::OY(-_roboFlotage);

    vec3d thrust(0.0, 0.0, 0.0);
    if ( !(arg->flag & 1) )
        thrust = arg->thrustDir * _thraction;

    vec3d totalForce = gravity + thrust - _fly_dir * (_fly_dir_length * _airconst) + buoyancy;

    if ( totalForce.length() > 0.0 )
    {
        vec3d velocity = _fly_dir * _fly_dir_length + totalForce * (arg->period / _mass);

        _fly_dir_length = velocity.normalise();
        _fly_dir = velocity;
    }

    if ( fabs(_fly_dir_length) > 0.1 )
        _position += _fly_dir * (_fly_dir_length * arg->period * 6.0);

    CorrectPositionInLevelBox(NULL);

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
        {
            TUnitPlacement gunPlacement;
            gunPlacement.pos = _position + _rotation.Transpose().Transform( gun.pos );
            gunPlacement.flags = 4;

            gun.gun_obj->SetPosition(&gunPlacement);
        }
    }

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    float pitchFactor = fabs(_fly_dir_length) / (_force / _airconst_static) * 1.4;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch = (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * pitchFactor;
}

void NC_STACK_yparobo::Die()
{
    int lastSeconds = getBACT_yourLastSeconds();

    uamessage_hostDie hdMsg;

    if ( !(_status_flg & BACT_STFLAG_DEATH1) )
    {
        hdMsg.msgID = UAMSG_HOSTDIE;
        hdMsg.owner = _owner;

        if (_killer)
            hdMsg.killer = _killer->_gid;
        else
            hdMsg.killer = 0;

        hdMsg.killerOwner = _killer_owner;

        if ( _killer_owner )
        {
            if ( !(_status_flg & BACT_STFLAG_CLEAN) )
            {
                TRoboDestroyed roboDestroyed;
                roboDestroyed.killerOwner = _killer_owner;
                roboDestroyed.bact = this;

                _world->HandleRoboDestroyed(&roboDestroyed); //Reown sectors for new owner
            }
        }

        // Only a faction's last host station takes its units along. Before
        // that, its squads and the faction's guns on the ground go to the
        // nearest one left, and so do its players.
        NC_STACK_yparobo *heir = (_status_flg & BACT_STFLAG_CLEAN) ? NULL : NearestFellowHostStation();

        if ( heir )
        {
            std::vector<NC_STACK_ypabact *> moving;

            for ( NC_STACK_ypabact *kid : _kidList )
            {
                NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(kid);

                if ( kid->_status != BACT_STATUS_DEAD && (!gun || !gun->IsRoboGun()) )
                    moving.push_back(kid);
            }

            for ( NC_STACK_ypabact *kid : moving )
            {
                heir->AddSubject(kid);
                kid->_host_station = heir;

                for ( NC_STACK_ypabact *unit : kid->_kidList )
                    unit->_host_station = heir;
            }

            _world->HostStationTakesOver(this, heir);
        }

        for (World::RefBactList::iterator kdIt = _kidList.begin(); kdIt != _kidList.end();)
        {
            NC_STACK_ypabact *node = *kdIt;
            kdIt++;

            for (World::RefBactList::iterator sbIt = node->_kidList.begin(); sbIt != node->_kidList.end();)
            {
                NC_STACK_ypabact *subnode = *sbIt;
                sbIt++;

                subnode->_killer = _killer;

                if ( _status_flg & BACT_STFLAG_CLEAN )
                    subnode->_status_flg |= BACT_STFLAG_CLEAN;

                subnode->Die();

                subnode->_status_flg &= ~BACT_STFLAG_LAND;

                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_CREATE;
                subnode->SetStateInternal(&stateMsg);

                subnode->_status = BACT_STATUS_DEAD;

                SFXEngine::SFXe.stopSound(&subnode->_soundcarrier, 3);
                SFXEngine::SFXe.startSound(&subnode->_soundcarrier, 7);

                subnode->_soundFlags &= 0xFFFFFFF7;
                subnode->_soundFlags |= 0x80;

                subnode->setBACT_yourLastSeconds(lastSeconds);
            }

            node->_killer = _killer;

            if ( _status_flg & BACT_STFLAG_CLEAN )
                node->_status_flg |= BACT_STFLAG_CLEAN;

            node->Die();

            node->_status_flg &= ~BACT_STFLAG_LAND;

            setState_msg stateMsg;
            stateMsg.unsetFlags = 0;
            stateMsg.setFlags = 0;
            stateMsg.newStatus = BACT_STATUS_CREATE;
            node->SetStateInternal(&stateMsg);

            node->_status = BACT_STATUS_DEAD;

            SFXEngine::SFXe.stopSound(&node->_soundcarrier, 3);
            SFXEngine::SFXe.startSound(&node->_soundcarrier, 7);

            node->_soundFlags &= 0xFFFFFFF7;
            node->_soundFlags |= 0x80;

            node->setBACT_yourLastSeconds(lastSeconds);
        }

        NC_STACK_ypabact *userHost = _world->UserRobo();

        // Only a host station destroyed is reported, not one cleaned away
        // with the rest of the level's units, as when it ends or goes back
        if ( _status_flg & BACT_STFLAG_CLEAN )
        {
        }
        else if ( this == userHost )
        {
            TRoboMessage msg;
            msg.msgID = 12;
            msg.param1 = 0;
            msg.param3 = 0;
            msg.param2 = 0;
            msg.unit = this;
            msg.priority = 100;

            _world->PlaceMessage(&msg);
        }
        else
        {
            TRoboMessage msg;
            msg.msgID = 11;
            msg.param1 = _owner;
            msg.param3 = 0;
            msg.param2 = 0;
            msg.unit = _killer;
            msg.priority = 50;

            _world->PlaceMessage(&msg);
        }

        NC_STACK_ypabact::Die();

        _vp_extra[1].flags = 0;

        if ( _world->_isNetGame )
        {
            if ( _owner )
            {
                _world->NetBroadcastMessage(&hdMsg, sizeof(hdMsg), true);

                if ( this == _world->UserRobo() )
                {
                    World::Player *local = _world->LocalPlayer();

                    _world->UserUnit()->setBACT_inputting(false, local);
                    _world->UserUnit()->setBACT_viewer(false, local);

                    setBACT_inputting(true, local);
                    setBACT_viewer(true, local);
                }
            }
        }
    }
}

size_t NC_STACK_yparobo::SetPosition(TUnitPlacement *arg)
{
    int placed = NC_STACK_ypabact::SetPosition(arg);
    if ( !placed )
        return 0;

    _roboFlotage = _mass * 9.80665;

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
        {
            TUnitPlacement gunPlacement;
            gunPlacement.pos = _position + _rotation.Transpose().Transform(gun.pos);
            gunPlacement.flags = 4;

            gun.gun_obj->SetPosition(&gunPlacement);
        }
    }

    _roboYPos = _position.y;

    return 1;
}

// Update robo energys
void NC_STACK_yparobo::EnergyInteract(update_msg *arg)
{
    float reloadConst = _reload_const;
    float elapsedSec = (_clock - _energy_time) / 1000.0;

    if ( _status != BACT_STATUS_DEAD && elapsedSec >= 0.25 )
    {
        int oldEnergy = _energy;

        int oldLifeEnergy = _roboEnergyLife;
        int oldMoveEnergy = _roboEnergyMove;

        _energy_time = _clock;

        TReloadRatios reloadRatios;
        reloadRatios.owner = _pSector->owner;

        _world->GetReloadRatios(&reloadRatios);

        float sectorPower = _pSector->energy_power;

        float energyGain = reloadConst * elapsedSec * sectorPower * reloadRatios.ratioClamped;

        if ( IsPlayerRobo() )
        {
            int poolCount = 0;

            float gain = energyGain / 6000.0;

            if ( _pSector->owner )
            {
                if ( _pSector->owner != _owner )
                    gain = -gain;
            }
            else
            {
                gain = 0.0;
            }

            if ( _roboFillMode & 1 || gain < 0.0 )
                poolCount++;

            if ( _roboFillMode & 4 )
                poolCount++;

            if ( _roboFillMode & 8 )
                poolCount++;

            _roboEnergyReloadPS = reloadConst * sectorPower / 6000.0;

            if ( poolCount > 0 )
            {
                int energySum = 0;
                int raiseStep = 0;
                int lowerStep = 0;

                gain = gain / (float)poolCount;

                if ( _roboFillMode & 1 || gain < 0.0 )
                    _energy += gain;

                if ( _roboFillMode & 4 )
                    _roboEnergyLife += gain;

                if ( _roboFillMode & 8 )
                    _roboEnergyMove += gain;

                float balanceAmount = elapsedSec * reloadConst / 30.0;

                if ( _roboFillMode & 1 )
                    energySum = _energy;

                if ( _roboFillMode & 4 )
                    energySum += _roboEnergyLife;

                if ( _roboFillMode & 8 )
                    energySum += _roboEnergyMove;

                int average = energySum / poolCount;

                if ( _roboFillMode & 1 )
                {
                    if ( average > _energy )
                        raiseStep++;
                    else if ( average < _energy )
                        lowerStep++;
                }

                if ( _roboFillMode & 4 )
                {
                    if ( average > _roboEnergyLife )
                        raiseStep++;
                    else if ( average < _roboEnergyLife )
                        lowerStep++;
                }

                if ( _roboFillMode & 8 )
                {
                    if ( average > _roboEnergyMove )
                        raiseStep++;
                    else if ( average < _roboEnergyMove )
                        lowerStep++;
                }

                if ( raiseStep )
                    raiseStep = (int)balanceAmount / raiseStep;

                if ( lowerStep )
                    lowerStep = (int)balanceAmount / lowerStep;

                if ( _roboFillMode & 1 )
                {
                    if ( _energy <= average )
                    {
                        _energy += raiseStep;

                        if ( _energy >= average )
                            _energy = average;
                    }
                    else
                    {
                        _energy -= lowerStep;
                        if ( _energy <= average )
                            _energy = average;
                    }
                }

                if ( _roboFillMode & 4 )
                {
                    if ( _roboEnergyLife <= average )
                    {
                        _roboEnergyLife += raiseStep;
                        if ( _roboEnergyLife >= average)
                            _roboEnergyLife = average;
                    }
                    else
                    {
                        _roboEnergyLife -= lowerStep;
                        if ( _roboEnergyLife <= average )
                            _roboEnergyLife = average;
                    }
                }

                if ( _roboFillMode & 8 )
                {
                    if ( average >= _roboEnergyMove )
                    {
                        _roboEnergyMove += raiseStep;
                        if ( _roboEnergyMove >= average)
                            _roboEnergyMove = average;
                    }
                    else
                    {
                        _roboEnergyMove -= lowerStep;
                        if ( _roboEnergyMove <= average )
                            _roboEnergyMove = average;
                    }
                }

            }
        }
        else
        {
            _roboEnergyReloadPS = reloadConst * sectorPower / 7000.0;
            float gain = energyGain / 7000.0;
            if ( _owner == _pSector->owner )
            {
                _roboBuildSpare += gain * 0.15;
                _roboVehicleSpare += gain * 0.7;
                _energy += gain * 0.7;
            }
            else
            {
                _energy -= gain;
            }
        }

        if ( _energy <= 0 )
        {
            _energy = 0;

            // Drained dry on another faction's sector: the next update kills
            // this host station, and the sector's owner gets the kill, and
            // with it this host station's sectors
            if ( _pSector->owner != _owner )
                _killer_owner = _pSector->owner;
        }

        if ( _roboEnergyLife < 0 )
            _roboEnergyLife = 0;

        if ( _roboEnergyMove < 0 )
            _roboEnergyMove = 0;

        if ( _roboBuildSpare < 0 )
            _roboBuildSpare = 0;

        if ( _roboVehicleSpare < 0 )
            _roboVehicleSpare = 0;

        if ( _energy_max < _energy )
            _energy = _energy_max;

        if ( _energy_max < _roboEnergyLife )
            _roboEnergyLife = _energy_max;

        if ( _energy_max < _roboEnergyMove )
            _roboEnergyMove = _energy_max;

        if ( _energy_max < _roboBuildSpare )
            _roboBuildSpare = _energy_max;

        if ( _energy_max < _roboVehicleSpare )
            _roboVehicleSpare = _energy_max;

        _roboEnergyLossFlags = 0;
        _roboEnergyLoadFlags = 0;

        if ( oldEnergy < _energy )
            _roboEnergyLoadFlags |= 1;
        else if ( oldEnergy > _energy )
            _roboEnergyLossFlags |= 1;

        if ( oldLifeEnergy < _roboEnergyLife )
            _roboEnergyLoadFlags |= 4;
        else if ( oldLifeEnergy > _roboEnergyLife )
            _roboEnergyLossFlags |= 4;

        if ( oldMoveEnergy < _roboEnergyMove )
            _roboEnergyLoadFlags |= 8;
        else if ( oldMoveEnergy > _roboEnergyMove )
            _roboEnergyLossFlags |= 8;
    }
}

void NC_STACK_yparobo::Renew()
{
    NC_STACK_ypabact::Renew();

    _roboRadarValue = 0;
    _roboEnemyValue = 0;
    _roboPowerValue = 0;
    _roboSafetyValue = 0;
    _roboConqValue = 0;
    _roboPositionValue = 0;
    _roboDangerValue = 0;
    _roboExploreValue = 0;
    _roboPowerCellIndex = 0;
    _roboRadarCellIndex = 0;
    _roboSafetyCellIndex = 0;
    _roboConqCellIndex = 0;
    _roboRadarDelay = 0;
    _roboEnemyDelay = 0;
    _roboPowerDelay = 0;
    _roboSafetyDelay = 0;
    _roboConqDelay = 0;
    _roboDangerDelay = 0;
    _roboExploreDelay = 0;
    _roboPositionDelay = 0;
    _roboBuildSpare = 0;

    _roboDockEnerg = 0;
    _roboDockCnt = 0;
    _roboDockTime = 0;
    _roboDockTargetType = BACT_TGT_TYPE_NONE;
    _roboDockAggr = 0;
    _roboDockTargetCommandID = 0;
    _roboDockTargetPos.x = 0;
    _roboDockTargetPos.y = 0;
    _roboDockTargetPos.z = 0;
    _roboTestEnemyTime = 0;
    _roboBeamTimePre = 0;
    _roboAttackersClearTime = 0;
    _roboAttackersTime = 0;
    _roboState = 0;

    _roboDockTime = 0;

    _roboGuns.clear();
    
    _roboAttackers.fill( TRoboAttackSlot() );

    _commandID = _world->_nextCommandId;

    _world->_nextCommandId++;

    setBACT_yourLastSeconds(3000);

    _roboTimeScale = _world->GetAITimeline();
}

void NC_STACK_yparobo::VisitView(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitView(v);

    // Its batteries and what it takes in, as the energy panel shows them
    v.Field("energyLife", _roboEnergyLife, World::State::VIEW);
    v.Field("energyMove", _roboEnergyMove, World::State::VIEW);
    v.Field("fillMode", _roboFillMode, World::State::VIEW);
    v.Field("energyLoadFlags", _roboEnergyLoadFlags, World::State::VIEW);
    v.Field("energyLossFlags", _roboEnergyLossFlags, World::State::VIEW);
    v.Field("energyReloadPS", _roboEnergyReloadPS, World::State::VIEW);
    v.Field("buildSpare", _roboBuildSpare, World::State::VIEW);
    v.Field("vehicleSpare", _roboVehicleSpare, World::State::VIEW);
}

void NC_STACK_yparobo::VisitInternal(World::State::Visitor &v)
{
    NC_STACK_ypabact::VisitInternal(v);

    v.Field("flotage", _roboFlotage);
    v.Field("collTurnAngle", _roboColls.lastTurnAngle);

    // The prototype's collision points and guns, which reading gives the
    // unit first, with what changes of them
    std::vector<vec3d> collPrev;
    for ( const World::TRoboColl &c : _roboColls.roboColls )
        collPrev.push_back(c.prev_coll_pos);

    v.Field("collPrevPos", collPrev);

    if ( v.Reads(World::State::INTERNAL) )
    {
        for (size_t i = 0; i < collPrev.size() && i < _roboColls.roboColls.size(); i++)
            _roboColls.roboColls[i].prev_coll_pos = collPrev[i];
    }

    std::vector<vec3d> gunDirs;
    std::vector<NC_STACK_ypabact *> guns;
    for ( const World::TRoboGun &gun : _roboGuns )
    {
        gunDirs.push_back(gun.dir);
        guns.push_back(gun.gun_obj);
    }

    v.Field("gunDirs", gunDirs);
    v.Units("guns", guns);

    if ( v.Reads(World::State::INTERNAL) )
    {
        for (size_t i = 0; i < _roboGuns.size(); i++)
        {
            if ( i < gunDirs.size() )
                _roboGuns[i].dir = gunDirs[i];
            _roboGuns[i].gun_obj = i < guns.size() ? guns[i] : NULL;
        }
    }

    v.Field("yPos", _roboYPos);
    v.Field("wFlags", _roboWFlags);
    v.Field("state", _roboState);
    v.Field("timeScale", _roboTimeScale);

    v.Field("epConquer", _roboEpConquer);
    v.Field("epRadar", _roboEpRadar);
    v.Field("epPower", _roboEpPower);
    v.Field("epDefense", _roboEpDefense);
    v.Field("epSafety", _roboEpSafety);
    v.Field("epRecon", _roboEpRecon);
    v.Field("epChangePlace", _roboEpChangePlace);
    v.Field("epRobo", _roboEpRobo);

    v.Field("dockEnergy", _roboDockEnerg);
    v.Field("dockCount", _roboDockCnt);
    v.Field("dockUser", _roboDockUser);
    v.Field("dockTime", _roboDockTime);
    v.Field("dockTargetPos", _roboDockTargetPos);
    v.Field("dockTargetType", _roboDockTargetType);
    v.Field("dockAggression", _roboDockAggr);
    v.Field("dockTargetCommand", _roboDockTargetCommandID);
    v.Field("dockPos", _roboDockPos);

    v.Field("radarValue", _roboRadarValue);
    v.Sector("radarCell", _roboRadarCell);
    v.Field("radarCellId", _roboRadarCellID);
    v.Field("radarCellIndex", _roboRadarCellIndex);
    v.Field("radarTime", _roboRadarTime);
    v.Field("radarDelay", _roboRadarDelay);
    v.Field("safetyValue", _roboSafetyValue);
    v.Sector("safetyCell", _roboSafetyCell);
    v.Field("safetyCellId", _roboSafetyCellID);
    v.Field("safetyCellIndex", _roboSafetyCellIndex);
    v.Field("safetyTime", _roboSafetyTime);
    v.Field("safetyDelay", _roboSafetyDelay);
    v.Field("powerValue", _roboPowerValue);
    v.Sector("powerCell", _roboPowerCell);
    v.Field("powerCellId", _roboPowerCellID);
    v.Field("powerCellIndex", _roboPowerCellIndex);
    v.Field("powerTime", _roboPowerTime);
    v.Field("powerDelay", _roboPowerDelay);
    v.Field("conqValue", _roboConqValue);
    v.Sector("conqCell", _roboConqCell);
    v.Field("conqCellId", _roboConqCellID);
    v.Field("conqCellIndex", _roboConqCellIndex);
    v.Field("conqTime", _roboConqTime);
    v.Field("conqDelay", _roboConqDelay);
    v.Field("positionValue", _roboPositionValue);
    v.Sector("positionCell", _roboPositionCell);
    v.Field("positionCellId", _roboPositionCellID);
    v.Field("positionCellIndex", _roboPositionCellIndex);
    v.Field("positionTime", _roboPositionTime);
    v.Field("positionDelay", _roboPositionDelay);
    v.Field("exploreValue", _roboExploreValue);
    v.Sector("exploreCell", _roboExploreCell);
    v.Field("exploreCellId", _roboExploreCellID);
    v.Field("exploreCellIndex", _roboExploreCellIndex);
    v.Field("exploreTime", _roboExploreTime);
    v.Field("exploreDelay", _roboExploreDelay);

    v.Field("enemyValue", _roboEnemyValue);
    v.Sector("enemyCell", _roboEnemyCell);
    v.Field("enemyCellId", _roboEnemyCellID);
    v.Field("enemyCommand", _roboEnemyCommandID);
    v.Field("enemyTime", _roboEnemyTime);
    v.Field("enemyDiv", _roboEnemyDiv);
    v.Field("enemyDelay", _roboEnemyDelay);

    v.Field("dangerValue", _roboDangerValue);
    v.Sector("dangerCell", _roboDangerCell);
    v.Field("dangerCellId", _roboDangerCellID);
    v.Field("dangerCommand", _roboDangerCommandID);
    v.Field("dangerTime", _roboDangerTime);
    v.Field("dangerDelay", _roboDangerDelay);

    v.Field("vehicleCellId", _roboVehicleCellID);
    v.Sector("vehicleCell", _roboVehicleCell);
    v.Field("vehicleDuty", _roboVehicleDuty);
    v.Field("vehicleCommand", _roboVehicleCommandID);
    v.Field("buildingCellId", _roboBuildingCellID);
    v.Sector("buildingCell", _roboBuildingCell);
    v.Field("buildingDuty", _roboBuildingDuty);
    v.Field("testEnemyTime", _roboTestEnemyTime);

    v.Field("beamTimePre", _roboBeamTimePre);
    v.Field("beamPos", _roboBeamPos);
    v.Field("beamFXTime", _roboBeamFXTime);

    static_assert(sizeof(TRoboAttackSlot) == 8, "two command ids");
    v.Field("attackSlots", &_roboAttackers[0].squadCommandID, _roboAttackers.size() * 2);
    v.Field("attackersTime", _roboAttackersTime);
    v.Field("attackersClearTime", _roboAttackersClearTime);
}

void NC_STACK_yparobo::HandBrake(update_msg *)
{
    _fly_dir_length *= 0.8;
    _thraction = 0;
    _roboFlotage = _mass * 9.80665;
}

void NC_STACK_yparobo::CorrectPositionOnLand()
{
}

void NC_STACK_yparobo::DeadTimeUpdate(update_msg *arg)
{
    int lastSeconds = getBACT_yourLastSeconds();

    if ( !(_status_flg & BACT_STFLAG_DEATH2) )
    {
        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_NOPE;
        stateMsg.unsetFlags = 0;
        stateMsg.setFlags = BACT_STFLAG_DEATH2;
        SetState(&stateMsg);
    }

    _status_flg |= BACT_STFLAG_LAND;

    if ( _owner == 0 || _vp_genesis == NULL )
    {
        if ( lastSeconds > 0 )
            return;

        int isInputting = getBACT_inputting();

        if ( isInputting )
            _status_flg |= BACT_STFLAG_NORENDER;
        else
            Release();

        return;
    }

    int plasmaTime = (float)_energy_max * 0.7;
    if ( plasmaTime < 10000 )
        plasmaTime = 10000;
    if ( plasmaTime > 25000 )
        plasmaTime = 25000;

    if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
    {
        _scale_time -= arg->frameTime;

        if ( _scale_time <= 0 )
        {
            _vp_extra[0].model = NULL;

            if ( lastSeconds > 0 )
                return;

            int isInputting = getBACT_inputting();

            if ( isInputting )
                _status_flg |= BACT_STFLAG_NORENDER;
            else
                Release();

            return;
        }

        _vp_extra[0].scale = sqrt((float)_scale_time / (float)plasmaTime) * 0.75;

        if ( _vp_extra[0].scale < 0.0 )
            _vp_extra[0].scale = 0;

        float spinAngle = _maxrot * 2.0 * ((float)arg->frameTime / 1000.0);

        _vp_extra[0].rotate = mat3x3::RotateY(spinAngle) * _vp_extra[0].rotate;

        if ( lastSeconds <= 0 )
            _status_flg |= BACT_STFLAG_NORENDER;
    }
    else
    {
        _scale_time = plasmaTime;
        _vp_extra[0].scale = 0.75;
        _vp_extra[0].pos = _position;
        _vp_extra[0].rotate = _rotation;
        _vp_extra[0].model = _vp_genesis;
        _vp_extra[0].flags |= (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);

        if ( _world->_isNetGame )
        {
            uamessage_startPlasma sbMsg;
            sbMsg.msgID = UAMSG_STARTPLASMA;
            sbMsg.owner = _owner;
            sbMsg.scale = 0.75;
            sbMsg.time = plasmaTime;
            sbMsg.id = _gid;
            sbMsg.pos = _position;
            sbMsg.dir = _rotation;

            _world->NetBroadcastMessage(&sbMsg, sizeof(sbMsg), true);
        }
    }
}

void NC_STACK_yparobo::FindMatchingCommander(TCommanderSearch *arg)
{
    arg->found_comm = NULL;

    for ( NC_STACK_ypabact* &node : _kidList )
    {
        if ( !(node->_status_flg & BACT_STFLAG_DEATH1) )
        {
            int idOk;

            if ( arg->flags & 1 )
            {
                if ( arg->comm_id == node->_commandID )
                {
                    arg->found_comm = node;
                    idOk = 1;
                }
                else
                {
                    idOk = 0;
                }
            }
            else
            {
                idOk = 1;
            }

            int targetOk;
            if ( arg->flags & 2 )
            {
                if ( node->_primTtype == arg->tgType || _roboDockTargetType == arg->tgType )
                {
                    Common::Point tgsec = World::PositionToSectorID(arg->tgt_pos);
                    Common::Point rbtg;

                    uint32_t targetCmdId;
                    if ( node->_commandID == _roboDockUser && _roboDockUser )
                    {
                        rbtg = World::PositionToSectorID( _roboDockTargetPos );
                        targetCmdId = _roboDockTargetCommandID;
                    }
                    else
                    {
                        rbtg = World::PositionToSectorID( node->_primTpos );
                        targetCmdId = node->_primT_cmdID;
                    }

                    if ( (arg->tgType == BACT_TGT_TYPE_UNIT && !arg->prim_comm_id && arg->tgt.pbact == node->_primT.pbact)
                            || (arg->tgType == BACT_TGT_TYPE_UNIT && arg->prim_comm_id != 0 && targetCmdId == arg->prim_comm_id)
                            || (arg->tgType == BACT_TGT_TYPE_CELL && rbtg == tgsec) )
                    {
                        arg->found_comm = node;
                    }
                }
                // TODO(bug): v21 stays 0 even when this commander matches, so with flag 2
                // the loop never stops early and the last match wins, not the first.
                targetOk = 0;
            }
            else
            {
                targetOk = 1;
            }

            int distanceOk;
            if ( !(arg->flags & 4) )
            {
                distanceOk = 1;
            }
            else
            {
                if ( node->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    float tx, ty;

                    if ( node->_commandID == _roboDockUser && _roboDockUser )
                    {
                        tx = _roboDockTargetPos.x;
                        ty = _roboDockTargetPos.z;
                    }
                    else
                    {
                        tx = node->_primTpos.x;
                        ty = node->_primTpos.z;
                    }
                    if ( POW2(arg->distance) > POW2(tx - arg->tgt_pos.x) + POW2(ty - arg->tgt_pos.z) )
                    {
                        arg->found_comm = node;

                        distanceOk = 1;
                    }
                    else
                    {
                        distanceOk = 0;
                    }
                }
                else
                {
                    distanceOk = 0;
                }
            }

            if ( idOk && targetOk && distanceOk )
                break;
        }
    }
}

// TODO(cleanup): setTarget_msg::priority carries a command ID here.
size_t NC_STACK_yparobo::GetTargetByCommandID(setTarget_msg *arg)
{
    if ( !arg->priority )
    {
        arg->tgt_type = BACT_TGT_TYPE_NONE;
        return 0;
    }

    if (_kidRef) //FIXME Think it's must be yw->_unitsList
    {
        for (NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if (arg->priority == node->_commandID)
            {
                if ( node->_status != BACT_STATUS_DEAD )
                {
                    arg->priority = 0;
                    arg->tgt_type = BACT_TGT_TYPE_UNIT;
                    arg->tgt.pbact = node;
                    return 1;
                }
                else
                {
                    arg->priority = 0;
                    arg->tgt_type = BACT_TGT_TYPE_NONE;
                    return 0;
                }
            }

            for (NC_STACK_ypabact* &subnode : node->_kidList)
            {
                if (arg->priority == subnode->_commandID)
                {
                    if ( subnode->_status != BACT_STATUS_DEAD )
                    {
                        arg->priority = 0;
                        arg->tgt_type = BACT_TGT_TYPE_UNIT;
                        arg->tgt.pbact = subnode;
                        return 1;
                    }
                    else
                    {
                        arg->priority = 0;
                        arg->tgt_type = BACT_TGT_TYPE_NONE;
                        return 0;
                    }
                }
            }
        }
    }

    arg->tgt_type = BACT_TGT_TYPE_NONE;
    arg->priority = 0;
    return 0;
}

// Create squad for robo
void NC_STACK_yparobo::UpdateGateBeamOut()
{
    if ( _beam_time )
    {
        if ( _clock - _beam_time > 2000 )
        {
            bool isNotViewBeaming = false;

            for ( NC_STACK_ypabact* &sectorUnit : _pSector->unitsList )
            {
                if ( sectorUnit->_status == BACT_STATUS_BEAM && sectorUnit->_owner == _owner && !sectorUnit->getBACT_viewer() )
                {
                    isNotViewBeaming = true;
                    break;
                }
            }

            if ( !isNotViewBeaming )
            {
                _world->BeamUnitThroughGate(this);

                vec3d tt = _position;

                _position = _old_pos;
                _roboYPos = _old_pos.y;

                _world->SaveWonState();

                _position = tt;
                
                _status_flg |= BACT_STFLAG_CLEAN;

                Die();
            }

        }
    }
    else
    {

        _position = World::SectorIDToCenterPos3( _cellId );

        for (NC_STACK_ypabact* &unit : _kidList )
        {
            unit->_status_flg |= BACT_STFLAG_CLEAN;

            for (NC_STACK_ypabact* &sub_unit : unit->_kidList )
                sub_unit->_status_flg |= BACT_STFLAG_CLEAN;
        }

        _world->_levelInfo.Buddies.clear();

        int beamDelay = 0;

        for ( NC_STACK_ypabact* &bct : _pSector->unitsList )
        {
            if  ( bct->_status != BACT_STATUS_DEAD )
            {
                if ( bct->_bact_type != BACT_TYPES_ROBO && bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_GUN && bct->_owner == _owner )
                {
                    vec2d temp = World::SectorIDToCenterPos2( _cellId );
                    float dx = fabs(bct->_position.x - temp.x);
                    float dz = fabs(bct->_position.z - temp.y);

                    // TODO(bug): || accepts units within 450 of the centre on either axis, so only the
                    // sector's corners are left out. Probably meant &&. The first decompile
                    // has the same.
                    if ( dx <= 450.0 || dz <= 450.0 )
                    {
                        bct->_status = BACT_STATUS_BEAM;

                        bct->_scale_time = 0;

                        bct->_scale_delay = beamDelay + 1000;
                        beamDelay += 400;
                    }
                }
            }
        }

        _beam_time = _clock;
    }
}



void NC_STACK_yparobo::setBACT_inputting(bool inpt, World::Player *player)
{
    NC_STACK_ypabact::setBACT_inputting(inpt, player);

    if ( inpt && _world->ControllerOf(this) == player )
        _world->SetPlayerHostStation(player, this);
}

NC_STACK_yparobo *NC_STACK_yparobo::NearestFellowHostStation() const
{
    NC_STACK_yparobo *nearest = NULL;
    float nearestDist = 0.0;

    for ( NC_STACK_ypabact *unit : _world->_unitsList )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(unit);

        if ( !robo || robo == this || robo->_owner != _owner || robo->_status == BACT_STATUS_DEAD ||
             (robo->_status_flg & BACT_STFLAG_DEATH1) )
            continue;

        float dist = (robo->_position - _position).length();

        if ( !nearest || dist < nearestDist )
        {
            nearest = robo;
            nearestDist = dist;
        }
    }

    return nearest;
}

void NC_STACK_yparobo::SetPlayerRun(bool run)
{
    if ( run )
    {
        _roboState |= ROBOSTATE_PLAYERROBO;
        return;
    }

    _roboState &= ~ROBOSTATE_PLAYERROBO;

    if ( _roboEpConquer || _roboEpDefense || _roboEpRecon || _roboEpRobo ||
         _roboEpPower || _roboEpRadar || _roboEpSafety || _roboEpChangePlace )
        return;

    // The medians of the campaign's AI host stations
    _roboEpConquer = 96;
    _roboEpDefense = 90;
    _roboEpRecon = 90;
    _roboEpRobo = 90;
    _roboEpChangePlace = 10;
    setROBO_robDelay(2000);
}

void NC_STACK_yparobo::setROBO_proto(World::TRoboProto *proto)
{
    for (World::TRoboGun &gun : _roboGuns)
    {
        if ( gun.gun_obj )
        {
            if ( !( gun.gun_obj->_status_flg & BACT_STFLAG_DEATH1 ) )
                gun.gun_obj->Die();

            gun.gun_obj->Release();

            gun.gun_obj = NULL;
        }
    }

    _roboGuns = proto->guns;
    
    for (World::TRoboGun &gun : _roboGuns)
    {
        TUnitSpawn gun_req;

        gun_req.pos = _position + _rotation.Transpose().Transform(gun.pos);

        gun_req.vehicle_id = gun.robo_gun_type;

        NC_STACK_ypabact *gun_obj = _world->CreateVehicle(&gun_req);
        NC_STACK_ypagun *gungun = dynamic_cast<NC_STACK_ypagun *>(gun_obj);

        gun.gun_obj = gun_obj;

        if ( gun_obj )
        {
            if (gungun)
            {
                gungun->SetGunBasis(gun.dir, false);

                gungun->setGUN_roboGun(1);
            }

            gun_obj->_owner = _owner;
            gun_obj->_commandID = _world->_nextCommandId;
            gun_obj->_host_station = this;

            _world->_nextCommandId++;

            if ( _world->_isNetGame )
            {
                gun_obj->_gid |= gun_obj->_owner << 24;
                gun_obj->_commandID |= gun_obj->_owner << 24;
            }

            gun_obj->_aggr = 60;

            AddSubject(gun_obj);
        }
        else
        {
            ypa_log_out("Unable to create Robo-Gun\n");
        }
    }

    _roboDockPos = proto->dock;
    _viewer_position = proto->viewer;
    _viewer_max_up = proto->robo_viewer_max_up;
    _viewer_max_down = proto->robo_viewer_max_down;
    _viewer_max_side = proto->robo_viewer_max_side;

    setBACT_extraViewer(true);
    setBACT_alwaysRender(true);

    _roboColls = proto->coll;
}

void NC_STACK_yparobo::setROBO_epConquer(int ep)
{
    _roboEpConquer = ep;
}

void NC_STACK_yparobo::setROBO_epDefense(int ep)
{
    _roboEpDefense = ep;
}

void NC_STACK_yparobo::setROBO_epRadar(int ep)
{
    _roboEpRadar = ep;
}

void NC_STACK_yparobo::setROBO_epPower(int ep)
{
    _roboEpPower = ep;
}

void NC_STACK_yparobo::setROBO_epSafety(int ep)
{
    _roboEpSafety = ep;
}

void NC_STACK_yparobo::setROBO_battVehicle(int batt)
{
    _roboEnergyLife = batt;
}

void NC_STACK_yparobo::setROBO_battBeam(int batt)
{
    _roboEnergyMove = batt;
}

void NC_STACK_yparobo::setROBO_fillMode(int fil)
{
    _roboFillMode = fil;
}

void NC_STACK_yparobo::setROBO_waitSway(int wait)
{
    if ( wait )
        _roboWFlags |= 1;
    else
        _roboWFlags &= ~1;
}

void NC_STACK_yparobo::setROBO_waitRotate(int wait)
{
    if ( wait )
        _roboWFlags |= 2;
    else
        _roboWFlags &= ~2;
}

void NC_STACK_yparobo::setROBO_epChangeplace(int ep)
{
    _roboEpChangePlace = ep;
}

void NC_STACK_yparobo::setROBO_epReconnoitre(int ep)
{
    _roboEpRecon = ep;
}

void NC_STACK_yparobo::setROBO_epRobo(int ep)
{
    _roboEpRobo = ep;
}

void NC_STACK_yparobo::setROBO_viewAngle(int angl)
{
    _viewer_rotation = mat3x3::Ident();
    _viewer_horiz_angle = angl * C_PI_180;
    _viewer_rotation = mat3x3::RotateY(_viewer_horiz_angle) * _viewer_rotation;
}

void NC_STACK_yparobo::setROBO_safDelay(int delay)
{
    _roboSafetyDelay = delay;
}

void NC_STACK_yparobo::setROBO_powDelay(int delay)
{
    _roboPowerDelay = delay;
}

void NC_STACK_yparobo::setROBO_radDelay(int delay)
{
    _roboRadarDelay = delay;
}

void NC_STACK_yparobo::setROBO_cplDelay(int delay)
{
    _roboPositionDelay = delay;
}

void NC_STACK_yparobo::setROBO_defDelay(int delay)
{
    _roboEnemyDelay = delay;
}

void NC_STACK_yparobo::setROBO_conDelay(int delay)
{
    _roboConqDelay = delay;
}

void NC_STACK_yparobo::setROBO_robDelay(int delay)
{
    _roboDangerDelay = delay;
}

void NC_STACK_yparobo::setROBO_recDelay(int delay)
{
    _roboExploreDelay = delay;
}




int NC_STACK_yparobo::getROBO_epConquer()
{
    return _roboEpConquer;
}

int NC_STACK_yparobo::getROBO_epDefense()
{
    return _roboEpDefense;
}

int NC_STACK_yparobo::getROBO_epRadar()
{
    return _roboEpRadar;
}

int NC_STACK_yparobo::getROBO_epPower()
{
    return _roboEpPower;
}

int NC_STACK_yparobo::getROBO_epSafety()
{
    return _roboEpSafety;
}

int NC_STACK_yparobo::getROBO_battVehicle()
{
    return _roboEnergyLife;
}

int NC_STACK_yparobo::getROBO_battBeam()
{
    return _roboEnergyMove;
}

int NC_STACK_yparobo::getROBO_fillMode()
{
    return _roboFillMode;
}

std::vector<World::TRoboGun> &NC_STACK_yparobo::GetGuns()
{
    return _roboGuns;
}

int NC_STACK_yparobo::getROBO_epChangeplace()
{
    return _roboEpChangePlace;
}

int NC_STACK_yparobo::getROBO_epReconnoitre()
{
    return _roboEpRecon;
}

int NC_STACK_yparobo::getROBO_epRobo()
{
    return _roboEpRobo;
}

int NC_STACK_yparobo::getROBO_roboState()
{
    return _roboState;
}

int NC_STACK_yparobo::getROBO_safDelay()
{
    return _roboSafetyDelay;
}

int NC_STACK_yparobo::getROBO_powDelay()
{
    return _roboPowerDelay;
}

int NC_STACK_yparobo::getROBO_radDelay()
{
    return _roboRadarDelay;
}

int NC_STACK_yparobo::getROBO_cplDelay()
{
    return _roboPositionDelay;
}

int NC_STACK_yparobo::getROBO_defDelay()
{
    return _roboEnemyDelay;
}

int NC_STACK_yparobo::getROBO_conDelay()
{
    return _roboConqDelay;
}

int NC_STACK_yparobo::getROBO_robDelay()
{
    return _roboDangerDelay;
}

int NC_STACK_yparobo::getROBO_recDelay()
{
    return _roboExploreDelay;
}

int NC_STACK_yparobo::getROBO_loadFlags()
{
    return _roboEnergyLoadFlags;
}

int NC_STACK_yparobo::getROBO_lossFlags()
{
    return _roboEnergyLossFlags;
}

int NC_STACK_yparobo::getROBO_absReload()
{
    return _roboEnergyReloadPS;
}

bool NC_STACK_yparobo::IsPlayerRobo() const
{
    return (_roboState & ROBOSTATE_PLAYERROBO) != 0;
}



NC_STACK_yparobo::NC_STACK_yparobo()
{
    _roboFlotage = 0.;
    _roboColls = World::TRoboCollSet();
    _roboYPos = 0.;

    _roboWFlags = 0;
    _roboState = 0;
    _roboTimeScale = 0;

    _roboEpConquer = 0;
    _roboEpRadar = 0;
    _roboEpPower = 0;
    _roboEpDefense = 0;
    _roboEpSafety = 0;
    _roboEpRecon = 0;
    _roboEpChangePlace = 0;
    _roboEpRobo = 0;

    _roboDockEnerg = 0;
    _roboDockCnt = 0;
    _roboDockUser = 0;
    _roboDockTime = 0;
    _roboDockTargetPos = vec3d();
    _roboDockTargetType = 0;
    _roboDockAggr = 0;
    _roboDockTargetCommandID = 0;
    _roboDockPos = vec3d();

    _roboRadarValue = 0;
    _roboRadarCell = NULL;
    _roboRadarCellID = 0;  //For AI
    _roboRadarCellIndex = 0;
    _roboRadarTime = 0;
    _roboRadarDelay = 0; //??

    _roboSafetyValue = 0;
    _roboSafetyCell = NULL;  //For AI
    _roboSafetyCellID = 0;  //For AI
    _roboSafetyCellIndex = 0;
    _roboSafetyTime = 0;
    _roboSafetyDelay = 0; //??

    //Power
    _roboPowerValue = 0;
    _roboPowerCell = NULL;  //For AI
    _roboPowerCellID = 0;  //For AI
    _roboPowerCellIndex = 0;
    _roboPowerTime = 0;
    _roboPowerDelay = 0; //??

    //Enemy
    _roboEnemyValue = 0;
    _roboEnemyCell = NULL;  //For AI
    _roboEnemyCellID = 0;  //For AI
    _roboEnemyCommandID = 0;  //For AI
    _roboEnemyTime = 0;
    _roboEnemyDiv = 0;
    _roboEnemyDelay = 0; //??

    _roboConqValue = 0;
    _roboConqCell = NULL;  //For AI
    _roboConqCellID = 0;  //For AI
    _roboConqCellIndex = 0;
    _roboConqTime = 0;
    _roboConqDelay = 0; //??

    _roboPositionValue = 0;
    _roboPositionCell = NULL;  //For AI
    _roboPositionCellID = 0;  //For AI
    _roboPositionCellIndex = 0;  //For AI
    _roboPositionTime = 0;
    _roboPositionDelay = 0; //??
    _roboExploreValue = 0;
    _roboExploreCell = NULL;  //For AI
    _roboExploreCellID = 0;  //For AI
    _roboExploreCellIndex = 0;  //For AI
    _roboExploreTime = 0;
    _roboExploreDelay = 0; //??
    _roboDangerValue = 0;
    _roboDangerCell = NULL;  //For AI
    _roboDangerCellID = 0;  //For AI
    _roboDangerCommandID = 0;  //For AI
    _roboDangerTime = 0;
    _roboDangerDelay = 0; //??
    _roboVehicleCellID = 0;  //For AI
    _roboVehicleCell = NULL;  //For AI

    _roboVehicleDuty = 0;  //For AI
    _roboVehicleCommandID = 0;  //For AI
    _roboBuildingCellID = 0;  //For AI
    _roboBuildingCell = NULL;  //For AI
    _roboBuildingDuty = 0;  //For AI

    _roboTestEnemyTime = 0;

    _roboGuns.clear();

    _roboEnergyLife = 0; //??

    _roboEnergyMove = 0; //??

    _roboFillMode = 0;
    _roboEnergyLoadFlags = 0;
    _roboEnergyLossFlags = 0;

    _roboEnergyReloadPS = 0;
    _roboBuildSpare = 0;
    _roboVehicleSpare = 0;
    _roboBeamTimePre = 0;
    _roboBeamPos = vec3d();
    _roboBeamFXTime = 0;

    for (auto &t : _roboAttackers)
    {
        t.squadCommandID = 0;
        t.targetCommandID = 0;
    }

    _roboAttackersTime = 0;
    _roboAttackersClearTime = 0;
}