#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stack>
#include "yw.h"
#include "ypabact.h"
#include "yparobo.h"
#include "ypamissile.h"
#include "yw_net.h"

#include "log.h"


int ypabact_id = 1;


NC_STACK_ypabact::NC_STACK_ypabact()
: _kidList(this, GetKidRefNode, World::BLIST_KIDS)
{
    _wrldSize = vec2d();
    _wrldSectors = Common::Point();
    _bact_type = 0;
    _gid = 0;
    _vehicleID = 0;
    _bflags = 0;
    _commandID = 0;
    _host_station = NULL;
    _parent = NULL;
    
    _soundFlags = 0;
    _volume = 0;
    _pitch = 0;
    _pitch_max = 0.0;
    _energy = 0;
    _energy_max = 0;
    _reload_const = 0;
    _shield = 0;
    _radar = 0;
    _owner = 0;
    _aggr = 0;
    _status = BACT_STATUS_NOPE;
    _status_flg = 0;
    _primTtype = 0;
    _secndTtype = 0;
    _primT_cmdID = 0;
    _secndT_cmdID = 0;
    _primT.pbact = NULL;
    _secndT.pbact = NULL;
    _adist_sector = 0.0;
    _adist_bact = 0.0;
    _sdist_sector = 0.0;
    _sdist_bact = 0.0;
    _current_waypoint = 0;
    _waypoints_count = 0;
    _m_cmdID = 0;
    _m_owner = 0;
    _fe_cmdID = 0;
    _fe_time = 0;
    _mass = 0.0;
    _force = 0.0;
    _airconst = 0.0;
    _airconst_static = 0.0;
    _maxrot = 0.0;
    _viewer_horiz_angle = 0.0;
    _viewer_vert_angle = 0.0;
    _viewer_max_up = 0.0;
    _viewer_max_down = 0.0;
    _viewer_max_side = 0.0;
    _thraction = 0.0;
    _fly_dir_length = 0.0;
    _height = 0.0;
    _height_max_user = 0.0;

    _vp_active = 0;

    _vp_extra_mode = 0;

    _radius = 0.0;
    _viewer_radius = 0.0;
    _overeof = 0.0;
    _viewer_overeof = 0.0;
    _clock = 0;
    _AI_time1 = 0;
    _AI_time2 = 0;
    _search_time1 = 0;
    _search_time2 = 0;
    _scale_time = 0;
    _brkfr_time = 0;
    _brkfr_time2 = 0;
    _newtarget_time = 0;
    _assess_time = 0;
    _waitCol_time = 0;
    _slider_time = 0;
    _dead_time = 0;
    _beam_time = 0;
    _energy_time = 0;
    _weapon = 0;
    _weapon_flags = 0;
    _mgun = 0;
    _num_weapons = 0;
    _weapon_time = 0;
    _gun_angle = 0.0;
    _gun_angle_user = 0.0;
    _gun_leftright = 0.0;
    _gun_radius = 0.0;
    _gun_power = 0.0;
    _mgun_time = 0;
    _salve_counter = 0;
    _kill_after_shot = 0;
    _heading_speed = 0.0;
    _killer = NULL;
    _killer_owner = 0;
    _reb_count = 0;
    _atk_ret = 0;
    _lastFrmStamp = 0;
    _scale_start = 0.0;
    _scale_speed = 0.0;
    _scale_accel = 0.0;
    _scale_duration = 0;
    _scale_pos = 0;
    _scale_delay = 0;

    for (NC_STACK_base *& vp_fx : _vp_fx_models)
        vp_fx = NULL;


    _oflags = 0;
    _yls_time = 0;
    
    _world = NULL;
}

NC_STACK_ypabact::~NC_STACK_ypabact()
{
    Common::DeleteAndNull(&_current_vp);
}


size_t NC_STACK_ypabact::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
        return 0;

    _attackersList.clear();
    _kidList.clear();
    _missiles_list.clear();

    _gid = ypabact_id;
    _bact_type = BACT_TYPES_BACT;
//    ypabact.field_3DA = 0;
    _host_station = NULL;
    _viewer_rotation = mat3x3::Ident();
    _fly_dir = vec3d(0.0, 0.0, 0.0);
    _fly_dir_length = 0;
    _target_vec = vec3d(0.0, 0.0, 0.0);
    
    //_kidRef.bact = this;



    ypabact_id++;

    _rotation = _viewer_rotation;

    _mass = 400.0;
    _force = 5000.0;
    _airconst = 500.0;
    _maxrot = 0.5;
    _height = 150.0;
    _radius = 20.0;
    _viewer_radius = 40.0;
    _overeof = 10.0;
    _viewer_overeof = 40.0;
    _energy = 10000;
    _shield = 0;
    _heading_speed = 0.7;
    _yls_time = 3000;
    _aggr = 50;
    _energy_max = 10000;
//    ypabact.field_3CE = 0;
    _height_max_user = 1600.0;
    _gun_radius = 5.0;
    _gun_power = 4000.0;
    _adist_sector = 800.0;
    _adist_bact = 650.0;
    _sdist_sector = 200.0;
    _sdist_bact = 100.0;
    _oflags = BACT_OFLAG_EXACTCOLL;

    _world = stak.Get<NC_STACK_ypaworld *>(BACT_ATT_WORLD, NULL);// get ypaworld

    if ( _world )
    {
        for( auto& it : stak )
        {
            IDVPair &val = it.second;

            if ( !val.Skip )
            {
                switch (val.ID)
                {
                case BACT_ATT_VIEWER:
                {
                    uamessage_viewer viewMsg;

                    if ( val.Get<int32_t>() )
                    {
                        _world->SetViewerUnit(this); //Set current bact

                        _oflags |= BACT_OFLAG_VIEWER;

                        if ( _world->_isNetGame )
                            viewMsg.view = 1;

                        SFXEngine::SFXe.startSound(&_soundcarrier, 8);
                    }
                    else
                    {
                        _oflags &= ~BACT_OFLAG_VIEWER;

                        if ( _world->_isNetGame )
                            viewMsg.view = 0;

                        SFXEngine::SFXe.stopSound(&_soundcarrier, 8);
                    }

                    if ( _world->_isNetGame ) // Network message send routine?
                    {
                        viewMsg.msgID = UAMSG_VIEWER;
                        viewMsg.owner = _owner;
                        viewMsg.classID = _bact_type;
                        viewMsg.id = _gid;

                        if ( viewMsg.classID == BACT_TYPES_MISSLE )
                        {
                            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(this);
                            viewMsg.launcher = miss->GetLauncherBact()->_gid;
                        }

                        _world->NetBroadcastMessage(&viewMsg, sizeof(viewMsg), true);
                    }
                }
                break;

                case BACT_ATT_INPUTTING:
                    if ( val.Get<int32_t>() )
                    {
                        _oflags |= BACT_OFLAG_USERINPT;
                        _world->setYW_userVehicle(this);
                    }
                    else
                    {
                        _oflags &= ~BACT_OFLAG_USERINPT;
                    }
                    break;

                case BACT_ATT_EXACTCOLL:
                    setBACT_exactCollisions(val.Get<bool>());
                    break;

                case BACT_ATT_BACTCOLL:
                    setBACT_bactCollisions ( val.Get<bool>() );
                    break;

                case BACT_ATT_AIRCONST:
                    setBACT_airconst(val.Get<int32_t>());
                    break;

                case BACT_ATT_LANDINGONWAIT:
                    setBACT_landingOnWait ( val.Get<bool>() );
                    break;

                case BACT_ATT_YOURLS:
                    setBACT_yourLastSeconds(val.Get<int32_t>());
                    break;

                case BACT_ATT_VISPROT:
                    SetVP( val.Get<NC_STACK_base *>());
                    break;

                case BACT_ATT_AGGRESSION:
                    setBACT_aggression(val.Get<int32_t>());
                    break;

                case BACT_ATT_EXTRAVIEWER:
                    setBACT_extraViewer ( val.Get<bool>() );
                    break;

                case BACT_ATT_ALWAYSRENDER:
                    setBACT_alwaysRender ( val.Get<bool>() );
                    break;

                default:
                    break;
                }
            }
        }
    }

    _tForm.Pos = _position;

    _tForm.SclRot = _rotation;

    _status = BACT_STATUS_NORMAL;

    _wrldSectors = _world->GetMapSize();

    _wrldSize = World::SectorIDToPos2( _wrldSectors );

    return 1;
}

size_t NC_STACK_ypabact::Deinit()
{
    SFXEngine::SFXe.StopCarrier(&_soundcarrier);

    _status_flg |= BACT_STFLAG_CLEAN;

    if ( !(_status_flg & BACT_STFLAG_DEATH1) )
        Die();

    if ( _pSector )
        _cellRef.Detach();

    _kidRef.Detach();

    while (!_kidList.empty())
        _kidList.front()->Delete();

    while (!_missiles_list.empty())
    {
        _missiles_list.front()->Delete();
        _missiles_list.pop_front();        
    }

    return NC_STACK_nucleus::Deinit();
}


void NC_STACK_ypabact::CopyTargetOf(NC_STACK_ypabact *unit)
{
    NC_STACK_ypabact *v6 = NULL;

    _waypoints_count = 0;
    _m_cmdID = 0;
    _status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);

    int tgType;
    vec2d wTo;

    if ( unit->_status_flg & BACT_STFLAG_WAYPOINT )
    {
        if ( !unit->_m_cmdID )
        {
            int lastWp = unit->_waypoints_count - 1;

            wTo = unit->_waypoints[lastWp].XZ();

            tgType = BACT_TGT_TYPE_CELL;
        }
        else
        {
            v6 = _world->FindBactByCmdOwn(unit->_m_cmdID, unit->_m_owner);

            if ( v6 )
                tgType = BACT_TGT_TYPE_UNIT;
            else
                tgType = BACT_TGT_TYPE_NONE;
        }
    }
    else
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            v6 = unit->_primT.pbact;
            tgType = BACT_TGT_TYPE_UNIT;
        }
        else if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            wTo = unit->_primTpos.XZ();
            tgType = BACT_TGT_TYPE_CELL;
        }
        else
            tgType = BACT_TGT_TYPE_NONE;
    }

    if ( tgType == BACT_TGT_TYPE_NONE )
    {
        tgType = BACT_TGT_TYPE_UNIT;
        v6 = unit;
    }

    if ( _bact_type != BACT_TYPES_TANK && _bact_type != BACT_TYPES_CAR )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = tgType;
        targetMsg.priority = 0;

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            targetMsg.tgt.pbact = v6;
        }
        else
        {
            targetMsg.tgt_pos.x = wTo.x;
            targetMsg.tgt_pos.z = wTo.y;
        }

        SetTarget(&targetMsg);
    }
    else
    {
        TPathRequest pathReq;

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            pathReq.to = v6->_position.XZ();
        }
        else
        {
            pathReq.to = wTo;
        }

        pathReq.steps_cnt = 32;
        pathReq.from = _position.XZ();
        pathReq.tgt_type = 1;

        SetPath(&pathReq);

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            _m_cmdID = v6->_commandID;
            _m_owner = v6->_owner;
        }
    }
}

size_t NC_STACK_ypabact::SetParameters(IDVList &stak)
{
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

            case BACT_ATT_INPUTTING:
                setBACT_inputting(val.Get<bool>());
                break;

            case BACT_ATT_EXACTCOLL:
                setBACT_exactCollisions ( val.Get<bool>() );
                break;

            case BACT_ATT_BACTCOLL:
                setBACT_bactCollisions ( val.Get<bool>() );
                break;

            case BACT_ATT_AIRCONST:
                setBACT_airconst(val.Get<int32_t>());
                break;

            case BACT_ATT_LANDINGONWAIT:
                setBACT_landingOnWait ( val.Get<bool>() );
                break;

            case BACT_ATT_YOURLS:
                setBACT_yourLastSeconds(val.Get<int32_t>());
                break;

            case BACT_ATT_VISPROT:
                SetVP( val.Get<NC_STACK_base *>());
                break;

            case BACT_ATT_AGGRESSION:
                setBACT_aggression(val.Get<int32_t>());
                break;

            case BACT_ATT_EXTRAVIEWER:
                setBACT_extraViewer ( val.Get<bool>() );
                break;

            case BACT_ATT_ALWAYSRENDER:
                setBACT_alwaysRender ( val.Get<bool>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}


void NC_STACK_ypabact::FixSectorFall()
{
    TRayIntersection groundRay;
    groundRay.stPos = vec3d(_position.x, -30000.0, _position.z);
    groundRay.vect = vec3d(0.0, 50000.0, 0.0);
    groundRay.flags = 0;

    _world->IntersectRay(&groundRay);

    if ( groundRay.isect )
        _position.y = groundRay.isectPos.y - 50.0;
    else
        _position.y = _pSector->height  - 50.0;
}


void NC_STACK_ypabact::FixBeyondTheWorld()
{
    vec2d mv = World::SectorIDToPos2( _world->GetMapSize() );

    if ( _position.x > mv.x )
        _position.x = mv.x - World::CVSectorHalfLength;

    if ( _position.x < 0.0 )
        _position.x = World::CVSectorHalfLength;

    if ( _position.z < mv.y )
        _position.z = mv.y + World::CVSectorHalfLength;

    if ( _position.z > 0.0 )
        _position.z = -World::CVSectorHalfLength;

    FixSectorFall();
}

void ReleaseExpiredMissiles(NC_STACK_ypabact *bact)
{
    for (World::MissileList::iterator it = bact->_missiles_list.begin(); it != bact->_missiles_list.end(); )
    {
        NC_STACK_ypamissile *misl = *it;
        if ( misl->getBACT_yourLastSeconds() <= 0 )
        {
            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;

            misl->SetTarget(&targetMsg);

            misl->_parent = NULL;

            misl->Release();

            it = bact->_missiles_list.erase(it);
        }
        else
            it++;
    }
}


void NC_STACK_ypabact::Update(update_msg *arg)
{
    if ( _kidRef.IsListType(World::BLIST_CACHE) ) // Do not update units in dead list
        return;
        
    static TF::TForm3D bact_cam;
    TF::Engine.SetViewPoint(&bact_cam);

    TSectorInfo sect_info;
    sect_info.pos_x = _position.x;
    sect_info.pos_z = _position.z;

    if ( !_world->GetSectorInfo(&sect_info) )
    {
        FixBeyondTheWorld();

        sect_info.pos_x = _position.x;
        sect_info.pos_z = _position.z;

        _world->GetSectorInfo(&sect_info);
    }

    cellArea *oldcell = _pSector;

    _cellId = sect_info.CellId;

//    bact->pos_x_cntr = sect_info.pos_x_cntr;
//    bact->pos_y_cntr = sect_info.pos_y_cntr;

    _pSector = sect_info.pcell;

    if ( oldcell != sect_info.pcell ) // If cell changed
    {
        _cellRef.Detach();  //Remove unit from old cell
        _cellRef = _pSector->unitsList.push_back(this);  // Add unit to new cell
    }

    // Test if bact fall through sector
    if ( _pSector->height + 1000.0 < _position.y )
        FixSectorFall();

    NC_STACK_ypabact *roboHost = _world->getYW_userHostStation();

    if ( _pSector->PurposeType == cellArea::PT_GATEOPENED && _bact_type == BACT_TYPES_ROBO && this == roboHost )
        ((NC_STACK_yparobo *)roboHost)->UpdateGateBeamOut();

    if ( !(_status_flg & BACT_STFLAG_DEATH1) && _energy <= 0 && _bact_type != BACT_TYPES_MISSLE )
    {
        Die();

        if ( !IsDestroyed() )
        {
            setState_msg stateMsg;
            stateMsg.setFlags = 0;
            stateMsg.unsetFlags = 0;
            stateMsg.newStatus = BACT_STATUS_IDLE;

            SetState(&stateMsg);
        }

        _status = BACT_STATUS_DEAD;
        _status_flg &= ~BACT_STFLAG_LAND;
    }

    _clock += arg->frameTime;

    AI_layer1(arg);

    for( NC_STACK_ypamissile *misl : Utils::IterateListCopy<NC_STACK_ypamissile *>(_missiles_list))
        misl->Update(arg);

    ReleaseExpiredMissiles(this);

    if ( _oflags & BACT_OFLAG_VIEWER )
    {
        if ( _oflags & BACT_OFLAG_EXTRAVIEW )
            bact_cam.Pos = _position + _rotation.Transpose().Transform(_viewer_position);
        else
            bact_cam.Pos = _position;

        if ( _oflags & BACT_OFLAG_EXTRAVIEW )
            bact_cam.SclRot = _viewer_rotation;
        else
            bact_cam.SclRot = _rotation;

        GFX::Engine.matrixAspectCorrection(bact_cam.SclRot, false);
    }

    _tForm.Pos = _position;

    if ( _status_flg & BACT_STFLAG_SCALE )
        _tForm.SclRot = _rotation.Transpose() * mat3x3::Scale( _scale );
    else
        _tForm.SclRot = _rotation.Transpose();

    int numbid = arg->units_count;

    arg->units_count = 0;

    /** 
     * Because missiles can cause 'ModifyEnergy' and 'Die' methods of upper bact
     * in hierarchy Update->Update - it can remove all bacts from list in
     * iteration. So we just needs to get safe copy of list for modify without
     * worry of lists modify.
     **/
    for ( NC_STACK_ypabact *bnod : _kidList.safe_iter() )
    {
        bnod->Update(arg);

        arg->units_count++;
    }

    arg->units_count = numbid;

    _soundcarrier.Position = _position;

    if ( _oflags & BACT_OFLAG_VIEWER )
        _soundcarrier.Position += _rotation.AxisY() * 400.0;

    _soundcarrier.Vector = _fly_dir * _fly_dir_length;

    SFXEngine::SFXe.UpdateSoundCarrier(&_soundcarrier);
}

void NC_STACK_ypabact::Render(baseRender_msg *arg)
{
    if ( _current_vp )
    {
        if ( !(_status_flg & BACT_STFLAG_NORENDER) )
        {
            if ( !(_oflags & BACT_OFLAG_VIEWER) || _oflags & BACT_OFLAG_ALWAYSREND )
            {
                _current_vp->Bas->TForm().Pos = _tForm.Pos;
                _current_vp->Bas->TForm().SclRot = _tForm.SclRot;

                _current_vp->Bas->Render(arg, _current_vp);
            }
        }
    }

    for (int i = 0; i < 3; i++)
    {
        extra_vproto *bd = &_vp_extra[i];

        if ( bd->vp )
        {
            if ( bd->flags & EVPROTO_FLAG_ACTIVE )
            {
                bd->vp->Bas->TForm().Pos = bd->pos;

                if ( bd->flags & EVPROTO_FLAG_SCALE )
                    bd->vp->Bas->TForm().SclRot = bd->rotate.Transpose() * mat3x3::Scale( vec3d(bd->scale, bd->scale, bd->scale) );
                else
                    bd->vp->Bas->TForm().SclRot = bd->rotate.Transpose();

                bd->vp->Bas->Render(arg, bd->vp);
            }
        }
    }
}

void NC_STACK_ypabact::SetTarget(setTarget_msg *arg)
{
    _assess_time = 0;
    TSectorInfo cellInfo;
    
    constexpr float CurSectrLength = World::CVSectorLength + 10.0;

    if ( _status_flg & BACT_STFLAG_DEATH1 && arg->tgt_type == BACT_TGT_TYPE_UNIT )
    {
        ypa_log_out("ALARM!!! bact-settarget auf logische Leiche owner %d, class %d, prio %d\n", _owner, _bact_type, arg->priority);
    }
    else if ( arg->priority )
    {
        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
            _secndT.pbact->DeleteAttacker(this, 1);

        switch ( arg->tgt_type )
        {
        case BACT_TGT_TYPE_CELL:
        case BACT_TGT_TYPE_CELL_IND:
            _secndTtype = BACT_TGT_TYPE_CELL;

            cellInfo.pos_x = arg->tgt_pos.x;
            cellInfo.pos_z = arg->tgt_pos.z;

            if ( cellInfo.pos_x < CurSectrLength )
                cellInfo.pos_x = CurSectrLength;

            if ( cellInfo.pos_x > _wrldSize.x - CurSectrLength )
                cellInfo.pos_x = _wrldSize.x - CurSectrLength;

            if ( cellInfo.pos_z > -CurSectrLength )
                cellInfo.pos_z = -CurSectrLength;

            if ( cellInfo.pos_z < _wrldSize.y + CurSectrLength )
                cellInfo.pos_z = _wrldSize.y + CurSectrLength;

            if ( _world->GetSectorInfo(&cellInfo) )
            {
                _secndT.pcell = cellInfo.pcell;
                _sencdTpos.x = cellInfo.pos_x;
                _sencdTpos.z = cellInfo.pos_z;
                _sencdTpos.y = cellInfo.pcell->height;
            }
            else
            {
                _secndTtype = BACT_TGT_TYPE_NONE;
                _secndT.pcell = NULL;
            }
            break;

        case BACT_TGT_TYPE_UNIT:
        case BACT_TGT_TYPE_UNIT_IND:
            _secndT.pbact = arg->tgt.pbact;
            _secndTtype = BACT_TGT_TYPE_UNIT;

            if ( _secndT.pbact )
            {
                if ( _secndT.pbact->_status_flg & BACT_STFLAG_DEATH1 )
                {
                    ypa_log_out("totes vehicle als nebenziel, owner %d, class %d\n", arg->tgt.pbact->_owner, arg->tgt.pbact->_bact_type);
                    _secndTtype = BACT_TGT_TYPE_NONE;
                }
                else
                {
                    _sencdTpos = _secndT.pbact->_position;
                    _secndT.pbact->AddAttacker(this, 1);
                }
            }
            else
            {
                ypa_log_out("Yppsn\n");
                _secndTtype = BACT_TGT_TYPE_NONE;
            }
            break;

        case BACT_TGT_TYPE_FRMT:
            _secndTtype = BACT_TGT_TYPE_FRMT;
            _sencdTpos = arg->tgt_pos;
            break;

        case BACT_TGT_TYPE_NONE:
            _secndT.pbact = NULL;
            _secndTtype = BACT_TGT_TYPE_NONE;
            break;

        default:
            _secndTtype = arg->tgt_type;
            break;
        }
    }
    else
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
            _primT.pbact->DeleteAttacker(this, 0);

        switch ( arg->tgt_type )
        {
        case BACT_TGT_TYPE_CELL:
        case BACT_TGT_TYPE_CELL_IND:
            _primT_cmdID = 0;
            _primTtype = BACT_TGT_TYPE_CELL;

            cellInfo.pos_x = arg->tgt_pos.x;
            cellInfo.pos_z = arg->tgt_pos.z;

            if ( cellInfo.pos_x < CurSectrLength )
                cellInfo.pos_x = CurSectrLength;

            if ( cellInfo.pos_x > _wrldSize.x - CurSectrLength )
                cellInfo.pos_x = _wrldSize.x - CurSectrLength;

            if ( cellInfo.pos_z > -CurSectrLength )
                cellInfo.pos_z = -CurSectrLength;

            if ( cellInfo.pos_z < _wrldSize.y + CurSectrLength )
                cellInfo.pos_z = _wrldSize.y + CurSectrLength;

            if ( _world->GetSectorInfo(&cellInfo) )
            {
                _primT.pcell = cellInfo.pcell;
                _primTpos.x = cellInfo.pos_x;
                _primTpos.z = cellInfo.pos_z;
                _primTpos.y = cellInfo.pcell->height;
            }
            else
            {
                _primTtype = BACT_TGT_TYPE_NONE;
                _primT.pcell = NULL;
            }
            break;

        case BACT_TGT_TYPE_UNIT:
        case BACT_TGT_TYPE_UNIT_IND:
            _primT.pbact = arg->tgt.pbact;
            _primTtype = BACT_TGT_TYPE_UNIT;

            if ( _primT.pbact )
            {
                if ( _primT.pbact->_status_flg & BACT_STFLAG_DEATH1 )
                {
                    ypa_log_out("totes vehicle als hauptziel, owner %d, class %d - ich bin class %d\n", arg->tgt.pbact->_owner, arg->tgt.pbact->_bact_type, _bact_type);
                    _primTtype = BACT_TGT_TYPE_NONE;
                    return;
                }

                _primTpos = _primT.pbact->_position;

                _primT.pbact->AddAttacker(this, 0);

                _primT_cmdID = arg->tgt.pbact->_commandID;
            }
            else
            {
                ypa_log_out("PrimT. without a pointer\n");
                _primTtype = BACT_TGT_TYPE_NONE;
            }
            break;

        case BACT_TGT_TYPE_FRMT:
            _primTtype = BACT_TGT_TYPE_FRMT;
            _primT_cmdID = 0;
            _primTpos = arg->tgt_pos;
            break;

        case BACT_TGT_TYPE_NONE:
            _primT.pbact = NULL;
            _waypoints_count = 0;
            _primTtype = BACT_TGT_TYPE_NONE;
            _status_flg &= ~BACT_STFLAG_WAYPOINT;
            break;

        case BACT_TGT_TYPE_DRCT:
            _target_dir = arg->tgt_pos;
            _primTtype = BACT_TGT_TYPE_DRCT;
            _primT.pbact = NULL;
            _primT_cmdID = 0;
            break;

        default:
            _primTtype = arg->tgt_type;
            break;
        }

        if ( arg->tgt_type == BACT_TGT_TYPE_CELL || arg->tgt_type == BACT_TGT_TYPE_UNIT )
        {
            for ( NC_STACK_ypabact* &node : _kidList )
            {
                if ( node->_status != BACT_STATUS_DEAD)
                {
                    node->SetTarget(arg);
                    if ( !(_status_flg & BACT_STFLAG_WAYPOINT)  )
                        node->_status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
                }
            }
        }
    }
}

void NC_STACK_ypabact::AI_layer1(update_msg *arg)
{
    setTarget_msg targetMsg;

    if ( _mass == 1.0 )
    {
        if ( _status_flg & BACT_STFLAG_DEATH2 )
        {
            _yls_time -= arg->frameTime;

            if ( _yls_time < 0 )
                _world->MoveToDeadCache(this);
        }
        else
        {
            setState_msg stateMsg;
            stateMsg.newStatus = BACT_STATUS_NOPE;
            stateMsg.unsetFlags = 0;
            stateMsg.setFlags = BACT_STFLAG_DEATH2;

            SetState(&stateMsg);

            _yls_time = 6000;
        }
        return;
    }

    if ( _bact_type != BACT_TYPES_MISSLE )
        EnergyInteract(arg);

    if ( _status == BACT_STATUS_DEAD )
    {
        if ( _status_flg & BACT_STFLAG_LAND )
            _yls_time -= arg->frameTime;
    }

    _airconst = _airconst_static;

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    if ( _clock - _AI_time1 < 250 || 
         _primTtype == BACT_TGT_TYPE_DRCT || 
         _bact_type == BACT_TYPES_GUN || 
         _status == BACT_STATUS_DEAD || 
         _status == BACT_STATUS_BEAM ||
         _status == BACT_STATUS_CREATE )
    {
        AI_layer2(arg);
        return;
    }

    _AI_time1 = _clock;
    _target_vec = vec3d(0.0, 0.0, 0.0);

    if ( _clock - _brkfr_time > 5000 )
    {
        _brkfr_time = _clock;

        StuckFree(arg);
    }

    if ( _status == BACT_STATUS_NORMAL && _primTtype != BACT_TGT_TYPE_NONE )
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            _target_vec = _primT.pbact->_position - _position;

            if ( _primT.pbact->_status != BACT_STATUS_DEAD)
                _primTpos = _primT.pbact->_position;
        }
        else
        {
            _target_vec = _primTpos - _position;
        }

        if ( _target_vec.length() > 2000.0 )
            _target_vec.y = 0;

        if ( IsParentMyRobo() &&
             (_oflags & BACT_OFLAG_VIEWER) )
        {
            bool doFight = _target_vec.length() < 800.0;

            int unitId = 0;
            for (NC_STACK_ypabact* &node : _kidList)
            {
                if ( node->_status != BACT_STATUS_DEAD )
                {
                    if ( doFight )
                    {
                        targetMsg.tgt_type = _primTtype;
                        targetMsg.priority = 0;
                        targetMsg.tgt.pbact = _primT.pbact;
                        targetMsg.tgt_pos = _primTpos;
                    }
                    else
                    {
                        TFormationSlot formSlot;
                        formSlot.slot = unitId;
                        GetFormationPosition(&formSlot);

                        targetMsg.tgt_type = BACT_TGT_TYPE_FRMT;
                        targetMsg.priority = 0;
                        targetMsg.tgt_pos = formSlot.pos;
                    }

                    node->SetTarget(&targetMsg);
                }
                unitId++;
            }
        }
    }

    if ( _primTtype == BACT_TGT_TYPE_NONE)
    {
        if ( _host_station && _primT_cmdID )
        {
            targetMsg.priority = _primT_cmdID;

            if ( _host_station->GetTargetByCommandID(&targetMsg) )
            {
                targetMsg.priority = 0;
            }
            else
            {
                _primT_cmdID = 0;

                targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                targetMsg.priority = 0;
                targetMsg.tgt_pos = _primTpos;
            }

            SetTarget(&targetMsg);
        }
    }

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT && 
             _primT.pbact )
        {
            if ( !_primT.pbact->_pSector->IsCanSee(_owner) )
            {
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 0;

                SetTarget(&targetMsg);
            }
        }
    }
    else if ( _vp_active == 6 && _status == BACT_STATUS_NORMAL )
    {
        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_NORMAL;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;
        SetState(&stateMsg);
    }

    AI_layer2(arg);
}

void NC_STACK_ypabact::AI_layer2(update_msg *arg)
{
    constexpr float CurSectrLength = 1.05 * World::CVSectorLength;
    
    if ( (_clock - _AI_time2) < 250 
       || _owner == 0 
       || _secndTtype == BACT_TGT_TYPE_DRCT 
       || _status == BACT_STATUS_CREATE 
       || _status == BACT_STATUS_DEAD 
       || _status == BACT_STATUS_BEAM )
    {
        if ( _oflags & BACT_OFLAG_USERINPT )
            User_layer(arg);
        else
            AI_layer3(arg);
        
        return;
    }
    
    _AI_time2 = _clock;

    if ( _status_flg & BACT_STFLAG_ESCAPE && 
         _target_vec.XZ().length() > 3600.0 )
    {
        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
        targetMsg.priority = 1;

        for(NC_STACK_ypabact* &nod : _kidList)
            nod->SetTarget(&targetMsg);

        SetTarget(&targetMsg);

        if ( _oflags & BACT_OFLAG_USERINPT )
            User_layer(arg);
        else
            AI_layer3(arg);
        return;
    }

    NC_STACK_ypabact *wee = _world->getYW_userHostStation();

    if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
    {
        if ( _clock - _search_time1 > 500 )
        {
            _search_time1 = _clock;

            NC_STACK_ypabact *enemy = GetSectorTarget(_cellId);
            if ( enemy )
            {
                if ( enemy->_bact_type != BACT_TYPES_ROBO && IsParentMyRobo() && _host_station == wee && enemy->_commandID != _fe_cmdID && _clock - _fe_time > 45000 )
                {
                    bool isRoboGun = false;
                    if ( enemy->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( enemy );
                        isRoboGun = gun->IsRoboGun();
                    }

                    if ( !isRoboGun )
                    {
                        _fe_cmdID = enemy->_commandID;
                        _fe_time = _clock;

                        TRoboMessage roboMsg;
                        roboMsg.msgID = 7;
                        roboMsg.param1 = enemy->_commandID;
                        roboMsg.param2 = 0;
                        roboMsg.param3 = 0;
                        roboMsg.unit = this;
                        roboMsg.priority = 46;

                        _host_station->placeMessage(&roboMsg);
                    }
                }
            }

            if ( _status == BACT_STATUS_IDLE || 
                 (  _aggr >= 50 && 
                  !(_status_flg & BACT_STFLAG_ESCAPE) && 
                     (_secndTtype == BACT_TGT_TYPE_NONE || 
                      _secndTtype == BACT_TGT_TYPE_CELL || 
                      _secndTtype == BACT_TGT_TYPE_FRMT) ) )
            {
                if ( enemy )
                {
                    _secndT_cmdID = enemy->_commandID;

                    setTarget_msg targetMsg;
                    targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
                    targetMsg.priority = 1;
                    targetMsg.tgt.pbact = enemy;

                    SetTarget(&targetMsg);
                }

                if ( (_clock - _search_time2) > 2000 && 
                     _aggr == 75 && 
                    !(_oflags & BACT_OFLAG_VIEWER) && 
                     IsParentMyRobo() &&
                     (_secndTtype == BACT_TGT_TYPE_FRMT || 
                      _secndTtype == BACT_TGT_TYPE_NONE) )
                {
                    if (  _position.x > CurSectrLength && 
                          _position.x < _wrldSize.x + -CurSectrLength && 
                          _position.z < -CurSectrLength && 
                          _position.z > _wrldSize.y + CurSectrLength )
                    {
                        _search_time2 = _clock;

                        if ( _owner != _pSector->owner )
                        {
                            setTarget_msg targetMsg;
                            targetMsg.priority = 1;
                            targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                            targetMsg.tgt_pos.x = _position.x;
                            targetMsg.tgt_pos.z = _position.z;

                            SetTarget(&targetMsg);
                        }
                    }
                }

                if ( IsParentMyRobo() && _secndTtype == BACT_TGT_TYPE_CELL )
                {
                    for(NC_STACK_ypabact* &nod : _kidList)
                    {
                        if ( nod->_secndTtype == BACT_TGT_TYPE_NONE || nod->_secndTtype == BACT_TGT_TYPE_FRMT )
                        {
                            setTarget_msg targetMsg;
                            targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                            targetMsg.tgt_pos = _sencdTpos;
                            targetMsg.priority = 1;
                            nod->SetTarget(&targetMsg);
                        }
                    }
                }
            }
        }

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
            _target_vec = _secndT.pbact->_position - _position;
        else if ( _secndTtype == BACT_TGT_TYPE_CELL)
            _target_vec = _sencdTpos - _position;

        if ( _target_vec.length() > 2000.0 )
            _target_vec.y = 0;
    }

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( _secndTtype == BACT_TGT_TYPE_UNIT && _secndT.pbact )
        {
            if ( !_secndT.pbact->_pSector->IsCanSee(_owner) || 
                  (_position.XZ() - _secndT.pbact->_position.XZ()).length() > 2160.0 )
            {
                setTarget_msg targetMsg;
                targetMsg.priority = 1;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                SetTarget(&targetMsg);
            }
        }
    }
    
    if ( _oflags & BACT_OFLAG_USERINPT )
        User_layer(arg);
    else
        AI_layer3(arg);
}

void TiltThrustTowardTarget(NC_STACK_ypabact *bact, update_msg *arg)
{
    bact->_thraction = bact->_force;

    float dtSec = arg->frameTime / 1000.0;

    float top = -bact->_target_dir.y;

    if ( top == 1.0 )
        top = 0.99998999;

    if ( top == -1.0 )
        top = -0.99998999;

    float weight = bact->_mass * 9.80665;
    float thraction = bact->_thraction;

    if ( thraction == 0.0 )
        thraction = 0.1;

    float crossTerm = sqrt( (1.0 - POW2(top)) ) * (top * -0.5);

    float weightTerm = (1.0 - 0.25 * POW2(top) + 0.25 * POW2(top) * POW2(top)) * (POW2(weight) / POW2(thraction));

    float sinTilt = sqrt( (1.0 - weightTerm) ) + (weight * crossTerm / thraction);

    vec3d tmp;
    tmp.y = -cos( clp_asin(sinTilt) );

    if ( bact->_target_dir.z != 0.0 )
    {
        float zSq = (1.0 - POW2(tmp.y)) / (POW2(bact->_target_dir.x) / POW2(bact->_target_dir.z) + 1.0);

        if ( zSq < 0.0 )
            zSq = 0.0;

        tmp.z = sqrt(zSq);

        if ( bact->_target_dir.z < 0.0 )
            tmp.z = -tmp.z;
    }
    else
    {
        tmp.z = 0.0;
    }

    if ( bact->_target_dir.x != 0.0 )
    {
        float xSq = (1.0 - POW2(tmp.y)) / (POW2(bact->_target_dir.z) / POW2(bact->_target_dir.x) + 1.0);

        if ( xSq < 0.0 )
            xSq = 0.0;

        tmp.x = sqrt(xSq);

        if ( bact->_target_dir.x < 0.0 )
            tmp.x = -tmp.x;
    }
    else
    {
        tmp.x = 0.0;
    }

    vec3d vaxis = (-bact->_rotation.AxisY()) * tmp;;

    if ( vaxis.normalise() != 0.0 )
    {
        float maxrot = bact->_maxrot * dtSec;

        float angle = clp_acos( tmp.dot( -bact->_rotation.AxisY() ) );

        if ( angle > maxrot )
            angle = maxrot;

        if ( fabs(angle) > BACT_MIN_ANGLE )
            bact->_rotation *= mat3x3::AxisAngle(vaxis, angle);
    }
}

void TurnToFlyDirection(NC_STACK_ypabact *bact, int frameTime)
{
    if ( clp_acos(bact->_rotation.m11) > 0.003 && (bact->_fly_dir.z != 0.0 || bact->_fly_dir.x != 0.0) && bact->_fly_dir_length > 0.0 )
    {
        float dtSec = frameTime / 1000.0;

        vec2d flydir = bact->_fly_dir.XZ();
        vec2d axisZ = bact->_rotation.AxisZ().XZ();

        float tmpsq = flydir.length();        
        float cosAngle = 0.0;
        
        if ( isnormal(tmpsq) ) // Not NULL, NAN, INF
            cosAngle = flydir.dot(axisZ) / tmpsq;

        tmpsq = axisZ.length();
        
        if ( isnormal(tmpsq) ) // Not NULL, NAN, INF
            cosAngle /= tmpsq;
        else
            cosAngle = 0.0;

        float angle = clp_acos( cosAngle );

        float maxAngle = bact->_maxrot * dtSec * (fabs(angle) * 0.8 + 0.2);

        if ( angle > maxAngle )
            angle = maxAngle;

        if ( bact->_fly_dir.x * bact->_rotation.m22 - bact->_rotation.m20 * bact->_fly_dir.z < 0.0 )
            angle = -angle;

        bact->_rotation *= mat3x3::RotateY(-angle);
    }
}

void NC_STACK_ypabact::AI_layer3(update_msg *arg)
{
    float dtSec = arg->frameTime / 1000.0;

    float targetDist = _target_vec.length();

    if ( targetDist > 0.0 )
        _target_dir = _target_vec / targetDist;

    int isViewer = _oflags & BACT_OFLAG_VIEWER;
    int exactColl = _oflags & BACT_OFLAG_EXACTCOLL;

    int nearViewer = _world->IsNearViewer(this);

    int collRadius;

    if ( isViewer )
        collRadius = _viewer_radius;
    else
        collRadius = _radius;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    {
        if ( _oflags & BACT_OFLAG_BACTCOLL )
        {
            if ( (nearViewer || (_secndTtype == BACT_TGT_TYPE_NONE && targetDist < World::CVSectorLength)) && !(_status_flg & BACT_STFLAG_LAND) )
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

        TRayIntersection fwdRay;

        fwdRay.isect = false;
        fwdRay.stPos = _old_pos;
        fwdRay.vect = _position - _old_pos;
        fwdRay.vect.y = 0;

        float len = fwdRay.vect.length();

        if ( len > 0.0 )
            fwdRay.vect *= 300.0 / len;
        else
            fwdRay.vect = _rotation.AxisZ() * 300.0;

        fwdRay.isect = false;
        fwdRay.flags = 0;

        TRayIntersection leftRay;
        leftRay.isect = false;
        leftRay.flags = 0;

        if ( isViewer || (_status_flg & BACT_STFLAG_DODGE_RIGHT) || (nearViewer && exactColl) )
        {
            leftRay.stPos = _old_pos;
            leftRay.vect.x = fwdRay.vect.x * 0.93969 - fwdRay.vect.z * 0.34202;
            leftRay.vect.y = fwdRay.vect.y;
            leftRay.vect.z = fwdRay.vect.z * 0.93969 + fwdRay.vect.x * 0.34202;

            _world->IntersectRay(&leftRay);
        }

        TRayIntersection rightRay;
        rightRay.isect = false;
        rightRay.flags = 0;

        if ( isViewer || (_status_flg & BACT_STFLAG_DODGE_LEFT) || (nearViewer && exactColl) )
        {
            rightRay.stPos = _old_pos;
            rightRay.vect.x = fwdRay.vect.x * 0.93969 + fwdRay.vect.z * 0.34202;
            rightRay.vect.y = fwdRay.vect.y;
            rightRay.vect.z = fwdRay.vect.z * 0.93969 - fwdRay.vect.x * 0.34202;

            _world->IntersectRay(&rightRay);
        }

        if ( isViewer || !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) || (nearViewer && exactColl) )
            _world->IntersectRay(&fwdRay);

        int hitCount = 0;

        TCollisionNormal collNormal;
        collNormal.normal = vec3d(0.0, 0.0, 0.0);

        if ( fwdRay.isect )
        {
            if ( len + collRadius > fwdRay.tVal * 300.0 )
            {
                collNormal.normal = fwdRay.skel->polygons[fwdRay.polyID].Normal();
                hitCount++;
            }
        }

        if ( leftRay.isect )
        {
            if ( len + collRadius > leftRay.tVal * 300.0 )
            {
                collNormal.normal += leftRay.skel->polygons[leftRay.polyID].Normal();
                hitCount++;
            }
        }

        if ( rightRay.isect )
        {
            if ( len + collRadius > rightRay.tVal * 300.0 )
            {
                collNormal.normal += rightRay.skel->polygons[rightRay.polyID].Normal();
                hitCount++;
            }
        }

        if ( !fwdRay.isect && !leftRay.isect && !rightRay.isect )
        {
            _status_flg &= ~(BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT | BACT_STFLAG_MOVE);
            _status_flg |= BACT_STFLAG_MOVE;
        }

        if ( !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) )
        {

            if ( leftRay.isect == 1 && rightRay.isect == 1 )
            {
                if ( leftRay.tVal >= rightRay.tVal )
                    _status_flg |= BACT_STFLAG_DODGE_LEFT;
                else
                    _status_flg |= BACT_STFLAG_DODGE_RIGHT;
            }

            if ( leftRay.isect == 1 && !rightRay.isect )
                _status_flg |= BACT_STFLAG_DODGE_RIGHT;

            if ( !leftRay.isect && rightRay.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;

            if ( !leftRay.isect && !rightRay.isect && fwdRay.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;
        }

        float maxForce = _mass * 9.80665;

        if ( maxForce <= _force )
            maxForce = _force;

        float airDrag;

        if ( _airconst >= 10.0 )
            airDrag = _airconst;
        else
            airDrag = 10.0;


        TRayIntersection heightRay;

        heightRay.vect.x = (_fly_dir.x * 200.0 * _fly_dir_length) / (maxForce / airDrag);

        if ( heightRay.vect.x < -200.0 )
            heightRay.vect.x = -200.0;

        if ( heightRay.vect.x > 200.0 )
            heightRay.vect.x = 200.0;

        heightRay.vect.y = _height;

        heightRay.vect.z = (_fly_dir.z * 200.0 * _fly_dir_length) / (maxForce / airDrag);

        if ( heightRay.vect.z < -200.0 )
            heightRay.vect.z = -200.0;

        if ( heightRay.vect.z > 200.0 )
            heightRay.vect.z = 200.0;

        heightRay.stPos = _old_pos;
        heightRay.flags = 0;

        _world->IntersectRay(&heightRay);

        if ( heightRay.isect )
        {
            _target_dir.y = -(1.0 - heightRay.tVal);
        }
        else
        {
            NC_STACK_ypabact *userVehicle = _world->getYW_userVehicle();

            if ( ((_secndTtype != BACT_TGT_TYPE_UNIT || (userVehicle != _secndT.pbact && _secndT.pbact->_bact_type != BACT_TYPES_ROBO)) &&
                    (_primTtype != BACT_TGT_TYPE_UNIT || (userVehicle != _primT.pbact && _primT.pbact->_bact_type != BACT_TYPES_ROBO)))
                    || _target_dir.y >= -0.01 )
            {
                if ( _target_dir.y < 0.15 )
                    _target_dir.y = 0.15;
            }
        }

        if ( _status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT) )
            _target_dir.y = -0.2;

        if ( heightRay.isect )
        {
            if ( heightRay.tVal * _height < _radius && _fly_dir.y > 0.0 )
            {
                collNormal.normal += heightRay.skel->polygons[heightRay.polyID].Normal();

                hitCount++;
            }
        }

        if ( hitCount )
        {
            float hitCountF = hitCount;

            collNormal.normal /= hitCountF;

            Recoil(&collNormal);
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_LCRASH;
        }

        if ( _target_dir.y != 0.0 )
            _target_dir.normalise();

        float tmpsq = fwdRay.vect.XZ().length();
        if (isnormal(tmpsq)) // not NULL, NAN, INF
        {
            if ( _status_flg & BACT_STFLAG_DODGE_LEFT )
            {
                _target_dir.x = -fwdRay.vect.z / tmpsq;
                _target_dir.z = fwdRay.vect.x / tmpsq;
            }
            else if ( _status_flg & BACT_STFLAG_DODGE_RIGHT )
            {
                _target_dir.x = fwdRay.vect.z / tmpsq;
                _target_dir.z = -fwdRay.vect.x / tmpsq;
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

        TiltThrustTowardTarget(this, arg);

        /*if ( bact->status_flg & BACT_STFLAG_DODGE ) //Unused flag
            bact->fly_dir_length *= 0.95;*/

        _thraction = (0.85 - _target_dir.y) * _force;

        move_msg moveMsg;
        moveMsg.flag = 0;
        moveMsg.period = arg->frameTime / 1000.0;

        Move(&moveMsg);

        TurnToFlyDirection(this, arg->frameTime);

        TFightTarget fightTgt;

        fightTgt.fperiod = dtSec;
        fightTgt.g_time = _clock;

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        {
            fightTgt.target.pbact = _secndT.pbact;
            fightTgt.prio = 1;

            FightWithBact(&fightTgt);
        }
        else if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            fightTgt.pos = _sencdTpos;
            fightTgt.target.pcell = _secndT.pcell;
            fightTgt.prio = 1;

            FightWithSect(&fightTgt);
        }
        else if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            fightTgt.target.pbact = _primT.pbact;
            fightTgt.prio = 0;

            FightWithBact(&fightTgt);
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            fightTgt.pos = _primTpos;
            fightTgt.target.pcell = _primT.pcell;
            fightTgt.prio = 0;

            FightWithSect(&fightTgt);
        }
        else
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.setFlags = 0;

                SetState(&stateMsg);
            }

            _status_flg &= ~(BACT_STFLAG_FIGHT_P | BACT_STFLAG_FIGHT_S);
        }
    }
    break;

    case BACT_STATUS_DEAD:
        DeadTimeUpdate(arg);
        break;

    case BACT_STATUS_IDLE:

        if ( _clock - _newtarget_time > 500 )
        {
            _newtarget_time = _clock;

            TTargetAssess assess;
            assess.tgType = _secndTtype;
            assess.priority = 1;

            int secndResult = TargetAssess(&assess);

            assess.priority = 0;
            assess.tgType = _primTtype;
            int primResult = TargetAssess(&assess);

            if ( secndResult != TA_IGNORE || primResult != TA_IGNORE )
            {
                setTarget_msg targetMsg;

                if ( secndResult == TA_CANCEL )
                {
                    targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                    targetMsg.priority = 1;
                    SetTarget(&targetMsg);
                }

                if ( primResult == TA_CANCEL )
                {
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

        if ( _oflags & BACT_OFLAG_LANDONWAIT )
        {
            _thraction = 0;

            if ( _status_flg & BACT_STFLAG_LAND )
            {
                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_IDLE;
                SetState(&stateMsg);

                TRayIntersection groundRay;
                groundRay.stPos = _position;
                groundRay.vect = vec3d(0.0, _overeof + 50.0, 0.0);
                groundRay.flags = 0;

                _world->IntersectRay(&groundRay);

                if ( groundRay.isect )
                    _position.y = groundRay.isectPos.y - _overeof;
            }
            else
            {
                TCrashLandStep landStep;
                landStep.flags = 0;
                landStep.frameTime = arg->frameTime;

                CrashOrLand(&landStep);
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

void NC_STACK_ypabact::User_layer(update_msg *arg)
{
    _airconst = _airconst_static;

    float dtSec = arg->frameTime / 1000.0;

    if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
    {

        _old_pos = _position;

        if ( _oflags & BACT_OFLAG_BACTCOLL )
        {
            if ( !(_status_flg & BACT_STFLAG_LAND) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        float moveThreshold;

        if ( _status_flg & BACT_STFLAG_LAND )
            moveThreshold = 0.1;
        else
            moveThreshold = 1.0;

        setState_msg stateMsg;

        if ( moveThreshold <= fabs(_fly_dir_length) )
        {
            if ( !(_status_flg & BACT_STFLAG_FIRE) )
            {
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

            float probeLen;

            if ( _viewer_overeof <= _viewer_radius )
                probeLen = _viewer_radius;
            else
                probeLen = _viewer_overeof;

            groundRay.flags = 0;
            groundRay.vect.y = probeLen * 1.5;

            _world->IntersectRay(&groundRay);

            if ( groundRay.isect && _thraction <= _mass * 9.80665 )
            {
                _fly_dir_length = 0;
                _status_flg |= BACT_STFLAG_LAND;
                _position.y = groundRay.isectPos.y - _viewer_overeof;
                _thraction = _mass * 9.80665;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_LAND;
            }

            if ( _primTtype != BACT_TGT_TYPE_CELL || (_primTpos.XZ() - _position.XZ()).length() <= 800.0 )
            {
                if ( _status_flg & BACT_STFLAG_LAND )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        stateMsg.unsetFlags = 0;
                        stateMsg.setFlags = 0;
                        stateMsg.newStatus = BACT_STATUS_IDLE;
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
                        stateMsg.newStatus = BACT_STATUS_IDLE;
                        stateMsg.unsetFlags = 0;
                        stateMsg.setFlags = 0;
                        SetState(&stateMsg);
                    }
                }
            }
        }

        float pitchDelta = arg->inpt->Sliders[1] * _maxrot * dtSec;
        float yawDelta = -arg->inpt->Sliders[0] * _maxrot * dtSec;

        if ( (fabs(_fly_dir.y) > 0.98 || _fly_dir_length == 0.0) && _rotation.m11 > 0.996 && arg->inpt->Sliders[1] == 0.0 )
        {
            vec2d axisX = _rotation.AxisX().XZ();;

            if ( axisX.normalise() == 0.0 )
                ypa_log_out("Null on div occur %s:%d\n", __FILE__, __LINE__);

            vec2d axisZ = _rotation.AxisZ().XZ();

            if ( axisZ.normalise() == 0.0 )
                ypa_log_out("Null on div occur %s:%d\n", __FILE__, __LINE__);

            _rotation.SetX( vec3d::X0Z(axisX) );
            _rotation.SetY( vec3d(0.0, 1.0, 0.0) );
            _rotation.SetZ( vec3d::X0Z(axisZ) );
        }

//    float v84 = sqrt( POW2(bact->field_651.m20) + POW2(bact->field_651.m22) );
//    v84 /= sqrt( POW2(bact->field_651.m20) + POW2(bact->field_651.m21) + POW2(bact->field_651.m22) );
//
//    float v75 = v84;
//
//    if ( v84 > 1.0 )
//      v75 = 1.0;

        float rollAngle = clp_acos( _rotation.AxisX().XZ().length() / _rotation.AxisX().length() );

        if ( _rotation.m01 < 0.0 )
            rollAngle = -rollAngle;

        if ( fabs(rollAngle) < 0.01 )
            rollAngle = 0.0;

        float absRoll = fabs(rollAngle);

        float rollRate = _heading_speed * absRoll + _heading_speed * 0.25;

        float levelDelta = _maxrot * dtSec   *  rollRate;

        if ( levelDelta > absRoll )
            levelDelta = absRoll;

        if ( rollAngle > 0.0 )
            levelDelta = -levelDelta;

        float rollDelta = -yawDelta + levelDelta;

        if ( fabs(rollDelta + rollAngle) > 1.0 )
        {
            if ( rollDelta >= 0.0 )
                rollDelta = 1.0 - fabs(rollAngle);
            else
                rollDelta = fabs(rollAngle) - 1.0;
        }


        if ( fabs(rollDelta) > _maxrot * 2.0 * dtSec * rollRate )
        {
            if ( rollDelta < 0.0 )
                rollDelta = _maxrot * -2.0 * dtSec * rollRate;

            if ( rollDelta >= 0.0 )
                rollDelta = _maxrot * 2.0 * dtSec * rollRate;
        }

        if ( fabs(rollDelta) < 0.001 )
            rollDelta = 0.0;

        _rotation = mat3x3::RotateX(pitchDelta * 0.5) * _rotation; // local
        _rotation = mat3x3::RotateZ(rollDelta * 0.5) * _rotation; // local
        _rotation *= mat3x3::RotateY(yawDelta * 0.5); // global

        _thraction += _force * dtSec * 0.5 * arg->inpt->Sliders[2];

        if ( _thraction < 0.0 )
            _thraction = 0;

        if ( _thraction > _force )
            _thraction = _force;

        float savedThrust = _thraction;

        float altitude = _pSector->height - _position.y;
        float softCeiling = _height_max_user * 0.8;

        if ( altitude > softCeiling )
        {
            float netWeight = _mass * 9.80665 - _force;
            float ceilingBand = _height_max_user * 0.2;
            float thrustCap = (altitude - softCeiling) * netWeight / ceilingBand;

            if ( _thraction > thrustCap )
                _thraction = thrustCap;

            if ( _thraction < 0.0 )
                _thraction = 0;
        }

        TMissileLaunch missileLaunch;

        missileLaunch.tgType = BACT_TGT_TYPE_DRCT;
        missileLaunch.tgt_pos = _rotation.AxisZ();

        TAimQuery aimQuery;
        aimQuery.flags = 5;
        aimQuery.aim_dir = _rotation.AxisZ();

        if ( UserTargeting(&aimQuery) )
        {
            missileLaunch.target.pbact = aimQuery.ret_bact;
            missileLaunch.tgType = BACT_TGT_TYPE_UNIT;
        }

        if ( arg->inpt->Buttons.IsAny({0, 1}) )
        {
            missileLaunch.direction = vec3d(0.0, 0.0, 0.0);
            missileLaunch.weapon = _weapon;
            missileLaunch.g_time = _clock;

            if ( missileLaunch.g_time % 2 )
                missileLaunch.start_point.x = _fire_pos.x;
            else
                missileLaunch.start_point.x = -_fire_pos.x;

            missileLaunch.start_point.y = _fire_pos.y;
            missileLaunch.start_point.z = _fire_pos.z;
            missileLaunch.flags = (arg->inpt->Buttons.Is(1) ? 1 : 0) | 2;

            LaunchMissile(&missileLaunch);
        }

        if ( _mgun != -1 )
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                if ( !(arg->inpt->Buttons.Is(2)) )
                {
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
                    stateMsg.unsetFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.setFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }

                TMinigunShot mgShot;

                mgShot.dir = _rotation.AxisZ();
                mgShot.period = dtSec;
                mgShot.gTime = _clock;

                FireMinigun(&mgShot);
            }
        }

        if ( arg->inpt->Buttons.Is(3) )
        {
            HandBrake(arg);

            savedThrust = _thraction;
        }

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            move_msg moveMsg;
            moveMsg.flag = 0;
            moveMsg.period = dtSec;

            Move(&moveMsg);
        }
        else
        {
            vec3d contactNormal(0.0, 0.0, 0.0);

            TSphereContact contacts[10];

            for (int i = 3; i >= 0; i--)
            {
                move_msg moveMsg;
                moveMsg.flag = 0;
                moveMsg.period = dtSec;

                Move(&moveMsg);

                int collided = 0;

                TSphereCollisionQuery sphereQuery;
                sphereQuery.pos = _position;
                sphereQuery.dir = _fly_dir;
                sphereQuery.radius = 32.0;
                sphereQuery.collisions = contacts;
                sphereQuery.flags = 0;
                sphereQuery.coll_max = 10;

                _world->FindSphereCollisions(&sphereQuery);

                if ( sphereQuery.coll_count )
                {
                    contactNormal = vec3d(0.0, 0.0, 0.0);

                    for (int j = sphereQuery.coll_count - 1; j >= 0; j--)
                    {
                        TSphereContact *contact = &sphereQuery.collisions[ j ];

                        contactNormal += contact->normal;
                    }

                    TCollisionNormal collNormal;

                    float ln = contactNormal.length();
                    if ( ln != 0.0 )
                        collNormal.normal = contactNormal / ln;
                    else
                        collNormal.normal = _fly_dir;



                    Recoil(&collNormal);

                    collided = 1;
                }

                if ( !collided )
                {
                    TRayIntersection moveRay;
                    moveRay.stPos = _old_pos;
                    moveRay.vect = _position - _old_pos;
                    moveRay.flags = 0;

                    _world->IntersectRay(&moveRay);

                    if ( moveRay.isect )
                    {
                        TCollisionNormal collNormal;
                        collNormal.normal = moveRay.skel->polygons[moveRay.polyID].Normal();

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
                        ffEffect.pos_x = contactNormal.x * 10.0 + _position.x;
                        ffEffect.pos_z = contactNormal.z * 10.0 + _position.z;

                        _world->FFeedback_Play(&ffEffect);
                    }
                }
            }
        }

        _thraction = savedThrust;
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
}

void NC_STACK_ypabact::AddSubject(NC_STACK_ypabact *kid)
{
    newMaster_msg masterMsg;

    masterMsg.bact = this;
    masterMsg.list = &_kidList;

    kid->SetNewMaster(&masterMsg);
}

void NC_STACK_ypabact::SetNewMaster(newMaster_msg *arg)
{
    _kidRef.Detach();

    _kidRef = arg->list->push_front(this);
    
    _parent = arg->bact;
}

void NC_STACK_ypabact::Move(move_msg *arg)
{
    _old_pos = _position;

    float weight;

    if ( _status == BACT_STATUS_DEAD )
        weight = _mass * 39.2266;
    else
        weight = _mass * 9.80665;

    float thraction = 0.0;
    vec3d thrustDir(0.0, 0.0, 0.0);

    if ( !(arg->flag & 1) )
    {
        thrustDir = -_rotation.AxisY();

        thraction = _thraction;

        if ( _oflags & BACT_OFLAG_USERINPT )
        {
            thrustDir.x = fSign(thrustDir.x) * sqrt( fabs(thrustDir.x) );
            thrustDir.y = fSign(thrustDir.y) * thrustDir.y * thrustDir.y;
            thrustDir.z = fSign(thrustDir.z) * sqrt( fabs(thrustDir.z) );
        }
    }

    vec3d netForce = vec3d::OY(weight) + thrustDir * thraction - _fly_dir * (_fly_dir_length * _airconst);

    float len = netForce.length();

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( netForce.y >= 0.0 )
            netForce.y *= 3.0;
        else
            netForce.y *= 5.0;
    }

    if ( len > 0.0 )
    {
        //vec3d v42 = bact->fly_dir * bact->fly_dir_length + (v41 / len) * (len / bact->mass * arg->field_0);
        vec3d velocity = _fly_dir * _fly_dir_length + netForce * (arg->period / _mass);

        _fly_dir_length = velocity.length();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = velocity / _fly_dir_length;
    }

    if ( fabs(_fly_dir_length) > 0.1 )
        _position += _fly_dir * (_fly_dir_length * arg->period * 6.0);

    CorrectPositionInLevelBox(NULL);

    _soundcarrier.Sounds[0].Pitch = _pitch;
    _soundcarrier.Sounds[0].Volume = _volume;

    float maxPitch;
    if ( _pitch_max <= -0.8 )
        maxPitch = 1.2;
    else
        maxPitch = _pitch_max;

    float scaledSpeed = fabs(_fly_dir_length) * maxPitch;
    float sideForceSq = _force * _force - _mass * 100.0 * _mass;

    float pitchFactor = scaledSpeed / (sqrt(sideForceSq) / _airconst_static);

    if ( pitchFactor > maxPitch )
        pitchFactor = maxPitch;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch += (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * pitchFactor;
}

void NC_STACK_ypabact::FightWithBact(TFightTarget *arg)
{
    constexpr float CurSectrLen = 1.1 * World::CVSectorLength;
    
    arg->pos = arg->target.pbact->_position;

    vec3d toTarget = arg->target.pbact->_position - _position;
    float targetDist = toTarget.normalise();

    TTargetAssess assess;

    vec3d *foePos;
    bool isSecTarget = false;
    bool isPrimTarget = 0;

    if ( _secndT.pbact == arg->target.pbact )
    {
        foePos = &_secndT.pbact->_position;
        assess.priority = 1;
        isSecTarget = true;
    }
    else
    {
        foePos = &_primT.pbact->_position;
        assess.priority = 0;
        isPrimTarget = true;
    }

    NC_STACK_ypabact *userHost = _world->getYW_userHostStation();

    if ( _clock - _assess_time > 500 || _clock < 500 )
    {
        _assess_time = _clock;

        assess.tgType = BACT_TGT_TYPE_UNIT;
        _atk_ret = TargetAssess(&assess);
    }

    if ( _atk_ret == TA_FIGHT )
    {
        float foeDistance = ( foePos->XZ() - _position.XZ() ).length();

        if ( _status_flg & BACT_STFLAG_APPROACH )
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            if ( (_position.x < CurSectrLen || _position.z > -CurSectrLen || _position.x > _wrldSize.x - CurSectrLen || _position.z < _wrldSize.y + CurSectrLen) || _adist_bact < foeDistance )
            {
                _status_flg &= ~BACT_STFLAG_APPROACH;
            }
            else
            {
                _AI_time2 = _clock;
                _AI_time1 = _clock;
            }
        }
        else
        {
            if ( _sdist_bact <= foeDistance )
            {
                if ( _adist_sector <= foeDistance )
                    _status_flg &= ~BACT_STFLAG_ATTACK;
                else
                    _status_flg |= BACT_STFLAG_ATTACK;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_ATTACK;

                /*if ( bact->field_3D1 == 2 || (arg->g_time & 1 && bact->field_3D1 == 3) )
                {
                    bact->target_vec.x = bact->fly_dir.x;
                    bact->target_vec.z = bact->fly_dir.z;
                }
                else*/
                {
                    _target_vec.x = -_fly_dir.x;
                    _target_vec.z = -_fly_dir.z;
                }

                _AI_time2 = _clock;
                _AI_time1 = _clock;
                _status_flg |= BACT_STFLAG_APPROACH;
            }
        }
    }
    else
    {
        _status_flg &= ~(BACT_STFLAG_APPROACH | BACT_STFLAG_ATTACK);
    }

    switch( _atk_ret )
    {
        case TA_CANCEL:
        {
            if ( isPrimTarget )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                setTarget_msg targetMsg;
                targetMsg.priority = 0;
                targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                targetMsg.tgt_pos = _primTpos;

                SetTarget(&targetMsg);
            }

            if ( isSecTarget )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg targetMsg;
                targetMsg.priority = 1;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;

                SetTarget(&targetMsg);
            }

            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
        }
        break;
        
        case TA_MOVE:
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
        }
        break;
        
        case TA_FIGHT:
        {
            TFireCheck fireCheck;
            fireCheck.pos = arg->target.pbact->_position;
            fireCheck.tgType = 2;
            fireCheck.radius = arg->target.pbact->_radius;

            if ( CheckFireAI(&fireCheck) )
            {
                if ( isSecTarget )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                vec3d rotZ = _rotation.AxisZ();

                TMissileLaunch launch;

                launch.direction.x = rotZ.x;

                if ( _bact_type == BACT_TYPES_TANK )
                    launch.direction.y = toTarget.y;
                else
                    launch.direction.y = rotZ.y - _gun_angle;

                launch.direction.z = rotZ.z;
                launch.tgType = BACT_TGT_TYPE_UNIT;
                launch.target.pbact = arg->target.pbact;
                launch.tgt_pos = arg->pos;
                launch.weapon = _weapon;
                launch.g_time = _clock;

                if ( arg->g_time & 1 )
                    launch.start_point.x = _fire_pos.x;
                else
                    launch.start_point.x = -_fire_pos.x;

                launch.start_point.y = _fire_pos.y;
                launch.start_point.z = _fire_pos.z;
                launch.flags = 0;

                LaunchMissile(&launch);
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_ATTACK;
            }

            if ( targetDist < 1000.0 &&   _mgun != -1 &&   toTarget.dot(_rotation.AxisZ()) > 0.85 )
            {
                if ( isSecTarget )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                if ( !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg stateMsg;
                    stateMsg.unsetFlags = 0;
                    stateMsg.newStatus = BACT_STATUS_NOPE;
                    stateMsg.setFlags = BACT_STFLAG_FIRE;

                    SetState(&stateMsg);
                }

                TMinigunShot mgShot;

                mgShot.period = arg->fperiod;
                mgShot.gTime = _clock;
                mgShot.dir = _rotation.AxisZ();

                FireMinigun(&mgShot);
            }
            else if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }
        }
        break;
        
        case TA_IGNORE:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg stateMsg;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NOPE;
                stateMsg.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&stateMsg);
            }

            if ( _secndT.pbact == arg->target.pbact )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 1;

                SetTarget(&targetMsg);

                isSecTarget = 0;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                if ( (IsParentMyRobo() && _host_station == userHost) && _status != BACT_STATUS_IDLE && !(_status_flg & BACT_STFLAG_ESCAPE) )
                {
                    TRoboMessage roboMsg;
                    roboMsg.unit = this;
                    roboMsg.msgID = 1;
                    roboMsg.param3 = 0;
                    roboMsg.param2 = 0;
                    roboMsg.param1 = 0;
                    roboMsg.priority = 32;

                    _host_station->placeMessage(&roboMsg);
                }

                setState_msg stateMsg;
                stateMsg.unsetFlags = 0;
                stateMsg.setFlags = 0;
                stateMsg.newStatus = BACT_STATUS_NORMAL;

                SetState(&stateMsg);

                _status = BACT_STATUS_IDLE;
            }
        }
        break;
        
        default:
        break;
    }
}

void NC_STACK_ypabact::FightWithSect(TFightTarget *arg)
{
    constexpr float CurSectrLen = 1.1 * World::CVSectorLength;
    
    int isSecTarget = 0;
    int isPrimTarget = 0;

    vec3d *cellPos;

    TTargetAssess assess;

    if ( _secndT.pcell == arg->target.pcell )
    {
        cellPos = &_sencdTpos;
        isSecTarget = 1;

        assess.priority = 1;
    }
    else
    {
        cellPos = &_primTpos;
        isPrimTarget = 1;

        assess.priority = 0;
    }

    NC_STACK_ypabact *userHost = _world->getYW_userHostStation();

    int isUserLeader = IsParentMyRobo() && _host_station == userHost;

    float targetDist = (_position.XZ() - cellPos->XZ()).length();

    if ( _clock - _assess_time > 500 || _clock < 500 )
    {
        _assess_time = _clock;

        assess.tgType = BACT_TGT_TYPE_CELL;
        _atk_ret = TargetAssess(&assess);
    }

    if ( _atk_ret == TA_FIGHT )
    {
        float cellDistance = (cellPos->XZ() - _position.XZ()).length();

        if ( _status_flg & BACT_STFLAG_APPROACH )
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            if ( (_position.x < CurSectrLen || _position.z > -CurSectrLen || _position.x > _wrldSize.x - CurSectrLen || _position.z < _wrldSize.y + CurSectrLen) || _adist_sector < cellDistance )
            {
                _status_flg &= ~BACT_STFLAG_APPROACH;
            }
            else
            {
                _AI_time2 = _clock;
                _AI_time1 = _clock;
            }
        }
        else if ( _sdist_sector <= cellDistance )
        {
            if ( _adist_sector <= cellDistance )
                _status_flg &= ~BACT_STFLAG_ATTACK;
            else
                _status_flg |= BACT_STFLAG_ATTACK;
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            /*if ( bact->field_3D1 == 2 || (arg->g_time & 1 && bact->field_3D1 == 3) )
            {
                bact->target_vec.x = bact->fly_dir.x;
                bact->target_vec.z = bact->fly_dir.z;
                bact->target_vec.y = bact->fly_dir.y;
            }
            else*/
            {
                _target_vec = -_fly_dir;
            }

            _AI_time2 = _clock;
            _AI_time1 = _clock;

            _status_flg |= BACT_STFLAG_APPROACH;
        }
    }
    else
    {
        _status_flg &= ~(BACT_STFLAG_APPROACH | BACT_STFLAG_ATTACK);
    }

    if ( _status_flg & BACT_STFLAG_FIRE )
    {
        setState_msg stateMsg;
        stateMsg.unsetFlags = BACT_STFLAG_FIRE;
        stateMsg.setFlags = 0;
        stateMsg.newStatus = BACT_STATUS_NOPE;

        SetState(&stateMsg);
    }
    
    switch(_atk_ret)
    {
        case TA_CANCEL:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( isPrimTarget )
            {
                if ( isUserLeader )
                {
                    TRoboMessage roboMsg;
                    
                    Common::Point tmp = World::PositionToSectorID(_primTpos);

                    roboMsg.unit = this;
                    roboMsg.msgID = 4;
                    roboMsg.param1 = tmp.x;
                    roboMsg.param2 = tmp.y;
                    roboMsg.priority = 18;
                    roboMsg.param3 = 0;

                    _host_station->placeMessage(&roboMsg);
                }

                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
                targetMsg.tgt_pos.x = _position.x;
                targetMsg.tgt_pos.z = _position.z;
                targetMsg.priority = 0;

                SetTarget(&targetMsg);
            }

            if ( isSecTarget )
            {
                if ( isUserLeader )
                {
                    TRoboMessage roboMsg;
                    
                    Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                    roboMsg.unit = this;
                    roboMsg.msgID = 4;
                    roboMsg.param1 = tmp.x;
                    roboMsg.param3 = 0;
                    roboMsg.priority = 18;
                    roboMsg.param2 = tmp.y;

                    _host_station->placeMessage(&roboMsg);
                }

                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg targetMsg;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
                targetMsg.priority = 1;
                SetTarget(&targetMsg);
            }
        }
        break;
        
        case TA_MOVE:
        {
        }
        break;
        
        case TA_FIGHT:
        {
            if ( isPrimTarget )
            {
                if ( targetDist < World::CVSectorLength )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIGHT_P) && isUserLeader && _secndT.pcell != _primT.pcell )
                    {
                        TRoboMessage roboMsg;
                        
                        Common::Point tmp = World::PositionToSectorID(_primTpos);

                        roboMsg.msgID = 3;
                        roboMsg.param1 = tmp.x;
                        roboMsg.param2 = tmp.y;
                        roboMsg.unit = this;
                        roboMsg.param3 = 0;
                        roboMsg.priority = 20;

                        _host_station->placeMessage(&roboMsg);
                    }

                    _status_flg |= BACT_STFLAG_FIGHT_P;
                }

                GetBestSectorPart(&_primTpos);

                arg->pos = _primTpos;
            }

            if ( isSecTarget )
            {
                if ( targetDist < World::CVSectorLength )
                {
                    if ( isUserLeader && !(_status_flg & BACT_STFLAG_FIGHT_S) )
                    {
                        TRoboMessage roboMsg;
                        
                        Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                        roboMsg.msgID = 3;
                        roboMsg.param1 = tmp.x;
                        roboMsg.param2 = tmp.y;
                        roboMsg.unit = this;
                        roboMsg.param3 = 0;
                        roboMsg.priority = 20;

                        _host_station->placeMessage(&roboMsg);
                    }

                    _status_flg |= BACT_STFLAG_FIGHT_S;
                }

                GetBestSectorPart(&_sencdTpos);

                arg->pos = _sencdTpos;
            }

            TFireCheck fireCheck;
            fireCheck.tgType = 1;
            fireCheck.pos = arg->pos;

            if ( CheckFireAI(&fireCheck) )
            {
                vec3d tmp = _position + _fire_pos - arg->pos;

                float fireDist = tmp.length();

                if ( fireDist < 0.01 )
                    fireDist = 0.01;

                if ( isSecTarget )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                TMissileLaunch launch;

                launch.direction = -(_position + _fire_pos - arg->pos) / fireDist;
                launch.tgType = BACT_TGT_TYPE_CELL;
                launch.target.pbact = arg->target.pbact;
                launch.tgt_pos = arg->pos;
                launch.weapon = _weapon;
                launch.g_time = _clock;

                if ( arg->g_time & 1 )
                    launch.start_point.x = _fire_pos.x;
                else
                    launch.start_point.x = -_fire_pos.x;

                launch.start_point.y = _fire_pos.y;
                launch.start_point.z = _fire_pos.z;
                launch.flags = 0;

                LaunchMissile(&launch);
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_ATTACK;
            }
        }
        break;
        
        case TA_IGNORE:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( isSecTarget )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                if ( isUserLeader && _secndT.pcell != _primT.pcell )
                {
                    TRoboMessage roboMsg;
                    
                    Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                    roboMsg.msgID = 2;
                    roboMsg.param1 = tmp.x;
                    roboMsg.param2 = tmp.y;
                    roboMsg.param3 = 0;
                    roboMsg.priority = 22;
                    roboMsg.unit = this;

                    _host_station->placeMessage(&roboMsg);
                }

                setTarget_msg targetMsg;
                targetMsg.priority = 1;
                targetMsg.tgt_type = BACT_TGT_TYPE_NONE;

                SetTarget(&targetMsg);

                isSecTarget = 0;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                if ( isUserLeader && _status != BACT_STATUS_IDLE )
                {
                    TRoboMessage roboMsg;

                    roboMsg.param3 = 0;
                    roboMsg.param2 = 0;
                    roboMsg.param1 = 0;
                    roboMsg.unit = this;
                    roboMsg.msgID = 1;
                    roboMsg.priority = 32;

                    _host_station->placeMessage(&roboMsg);
                }

                _status = BACT_STATUS_IDLE;
            }
        }
        break;
        
        default:
            break;
    }
}

void NC_STACK_ypabact::CopyWaypointsStuff( NC_STACK_ypabact *bact)
{
    if ( bact->_status_flg & BACT_STFLAG_WAYPOINT )
    {
        for (int i = 0; i < 32; i++)
            _waypoints[i] = bact->_waypoints[i];

        _status_flg |= BACT_STFLAG_WAYPOINT;

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            _status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            _status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        _waypoints_count = bact->_waypoints_count;
        _current_waypoint = bact->_current_waypoint;
    }
}

void NC_STACK_ypabact::Die()
{
    if ( _status_flg & BACT_STFLAG_DEATH1 )
        return;
    
    int maxy = _world->getYW_mapSizeY();
    int maxx = _world->getYW_mapSizeX();

    uamessage_dead deadMsg;
    deadMsg.msgID = UAMSG_DEAD;
    deadMsg.owner = _owner;
    deadMsg.id = _gid;
    deadMsg.newParent = 0;
    deadMsg.landed = 0;
    deadMsg.classID = _bact_type;

    if ( _killer )
        deadMsg.killer = _killer->_gid;
    else
        deadMsg.killer = 0;

    deadMsg.killerOwner = _killer_owner;

    NC_STACK_ypabact *deputy = NULL;

    if (!_kidList.empty())
    {

        for (World::RefBactList::iterator it = _kidList.begin(); it != _kidList.end(); )
        {
            // Forward dereference iterator and next
            NC_STACK_ypabact *kid = *it;
            it++;

            if ( kid->_status == BACT_STATUS_DEAD )
            {
                if ( _parent )
                    _parent->AddSubject(kid);
                else
                    _world->AddToUnitsList(kid);                  

                kid->_status_flg |= BACT_STFLAG_NOMSG;
            }
            else
            {
                float kidLen = (kid->_position.XZ() - _position.XZ()).square();

                float deputyLen;

                if ( deputy )
                    deputyLen = (deputy->_position.XZ() - _position.XZ()).square();
                else
                    deputyLen = (POW2(maxx) + POW2(maxy)) * World::CVSectorLength * World::CVSectorLength;

                if ( kid->_bact_type == BACT_TYPES_UFO )
                    kidLen = (POW2(maxx) + POW2(maxy)) * World::CVSectorLength * World::CVSectorLength - 1000.0;

                if ( kidLen <= deputyLen )
                    deputy = kid;
            }
        }

        if ( deputy )
        {
            if ( _parent )
                _parent->AddSubject(deputy);
            else
                _world->AddToUnitsList(deputy);

            while ( !_kidList.empty() )
                deputy->AddSubject(_kidList.front());

            setTarget_msg targetMsg;
            targetMsg.tgt_pos = _primTpos;
            targetMsg.tgt.pbact = _primT.pbact;
            targetMsg.tgt_type = _primTtype;
            targetMsg.priority = 0;

            deputy->SetTarget(&targetMsg);

            deputy->CopyWaypointsStuff(this);

            deputy->_commandID = _commandID;
            deputy->_aggr = _aggr;

            if ( _world->_isNetGame )
            {
                if (_owner)
                    deadMsg.newParent = deputy->_gid;
            }
        }
        else
        {
            for(World::RefBactList::iterator kidXit = _kidList.begin(); kidXit != _kidList.end(); )
            {
                NC_STACK_ypabact *kidX = *kidXit;
                kidXit++;
                
                for ( World::RefBactList::iterator kidYit = kidX->_kidList.begin(); kidYit != kidX->_kidList.end(); )
                {
                    NC_STACK_ypabact *kidY = *kidYit;
                    kidYit++;

                    _world->AddToUnitsList(kidY);

                    if ( kidY->_status != BACT_STATUS_DEAD )
                        ypa_log_out("Scheisse, da hфngt noch ein Lebendiger unter der Leiche! owner %d, state %d, class %d\n", kidY->_owner, kidY->_status, _bact_type);
                }
                _world->AddToUnitsList(kidX);
            }
        }
    }

    NC_STACK_ypabact *userHost = _world->getYW_userHostStation();

    if ( !deputy && IsParentMyRobo()&& !(_status_flg & BACT_STFLAG_NOMSG) )
    {
        TRoboMessage roboMsg;

        if ( userHost == _host_station )
        {
            if ( _bact_type == BACT_TYPES_GUN )
            {
                if ( _weapon != -1 || -1 != _mgun )
                {
                    roboMsg.priority = 80;
                    roboMsg.msgID = 31;
                }
                else
                {
                    roboMsg.priority = 80;
                    roboMsg.msgID = 32;
                }

                roboMsg.param3 = 0;
                roboMsg.param2 = 0;
                roboMsg.param1 = 0;
                roboMsg.unit = this;

                _host_station->placeMessage(&roboMsg);
            }
            else
            {
                if ( !(_status_flg & BACT_STFLAG_CLEAN) )
                {
                    roboMsg.param1 = _commandID;
                    roboMsg.unit = this;
                    roboMsg.param3 = 0;
                    roboMsg.priority = 44;
                    roboMsg.param2 = 0;
                    roboMsg.msgID = 8;

                    _host_station->placeMessage(&roboMsg);
                }
            }
        }
        else
        {
            if ( _killer && userHost == _killer->_host_station )
            {
                roboMsg.msgID = 5;
                roboMsg.unit = _killer;
                roboMsg.param1 = _primT_cmdID;
                roboMsg.param3 = 0;
                roboMsg.param2 = 0;
                roboMsg.priority = 36;

                _host_station->placeMessage(&roboMsg);
            }
        }

    }

    CleanAttackersTarget();

    if ( _parent )
    {
        for (World::MissileList::iterator it = _missiles_list.begin(); it != _missiles_list.end(); it = _missiles_list.erase(it))
        {
            NC_STACK_ypamissile *miss = *it;

            _parent->_missiles_list.push_back(miss);
            miss->SetLauncherBact( _parent );
        }
    }
    else
    {
        for (World::MissileList::iterator it = _missiles_list.begin(); it != _missiles_list.end(); it = _missiles_list.erase(it))
        {
            NC_STACK_ypamissile *miss = *it;

            miss->ResetViewing();

            setState_msg stateMsg;
            stateMsg.newStatus = BACT_STATUS_DEAD;
            stateMsg.unsetFlags = 0;
            stateMsg.setFlags = 0;
            miss->SetStateInternal(&stateMsg);

            setTarget_msg targetMsg;
            targetMsg.tgt_type = BACT_TGT_TYPE_NONE;
            targetMsg.priority = 0;
            miss->SetTarget(&targetMsg);

            miss->_parent = NULL;

            _world->MoveToDeadCache(miss);
        }
    }
    

    if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        _secndT.pbact->DeleteAttacker(this, 1);

    if ( _primTtype == BACT_TGT_TYPE_UNIT )
        _primT.pbact->DeleteAttacker(this, 0);


    _secndTtype = BACT_TGT_TYPE_NONE;
    _primTtype = BACT_TGT_TYPE_NONE;

    _status = BACT_STATUS_DEAD;
    _commandID = 0;
    _status_flg |= BACT_STFLAG_DEATH1;
    _dead_time = _clock;

    if ( _status_flg & BACT_STFLAG_LAND )
    {
        if ( _vp_active == 1 || _vp_active == 6 )
        {
            setState_msg stateMsg;
            stateMsg.unsetFlags = 0;
            stateMsg.newStatus = BACT_STATUS_NOPE;
            stateMsg.setFlags = BACT_STFLAG_DEATH2;

            SetStateInternal(&stateMsg);

            if ( _world->_isNetGame )
            {
                if (_owner)
                    deadMsg.landed = 1;
            }
        }
    }

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( !(_oflags & BACT_OFLAG_VIEWER) )
        {
            if ( _parent )
                setBACT_inputting(false);
        }
    }

    if ( _world->_isNetGame )
    {
        if ( _owner )
        {
            if ( _bact_type != BACT_TYPES_ROBO )
                _world->NetBroadcastMessage(&deadMsg, sizeof(deadMsg), true);
        }
    }

    if ( _owner )
    {
        if ( !(_status_flg & BACT_STFLAG_CLEAN) )
            _world->HistoryAktKill(this);
    }
}

void NC_STACK_ypabact::SetState(setState_msg *arg)
{
    if ( (_bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR) && arg->newStatus == BACT_STATUS_DEAD )
    {
        setState_msg newarg;
        newarg.unsetFlags = 0;
        newarg.newStatus = BACT_STATUS_NOPE;
        newarg.setFlags = BACT_STFLAG_DEATH2;

        SetState(&newarg);
    }
    else
    {
        int changed = SetStateInternal(arg);

        if ( _world->_isNetGame )
        {
            if ( changed && _owner && _bact_type != BACT_TYPES_MISSLE )
            {
                uamessage_setState ssMsg;
                ssMsg.msgID = UAMSG_SETSTATE;
                ssMsg.owner = _owner;
                ssMsg.id = _gid;
                ssMsg.newStatus = arg->newStatus;
                ssMsg.setFlags = arg->setFlags;
                ssMsg.unsetFlags = arg->unsetFlags;
                ssMsg.classID = _bact_type;

                _world->NetBroadcastMessage(&ssMsg, sizeof(ssMsg), true);
            }
        }
    }
}

size_t NC_STACK_ypabact::LaunchMissile(TMissileLaunch *arg)
{
    NC_STACK_ypamissile *wobj = NULL;

    if ( arg->weapon == -1 )
        return 0;
    
    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(arg->weapon);

    if ( _weapon_time )
    {
        int shotDelay;

        if ( _oflags & BACT_OFLAG_USERINPT )
            shotDelay = wproto.shot_time_user;
        else
            shotDelay = wproto.shot_time;

        if ( wproto.salve_shots )
        {
            if ( wproto.salve_shots <= _salve_counter )
                shotDelay = wproto.salve_delay;
        }

        if ( arg->g_time - _weapon_time < shotDelay )
            return 0;
    }

    if ( _salve_counter < wproto.salve_shots )
        _salve_counter += 1;
    else
        _salve_counter = 1;

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        TFFeedbackEffect ffEffect;

        if ( wproto._weaponFlags & 2 )
            ffEffect.effects_type = 0;
        else if ( wproto._weaponFlags & 0x10 )
            ffEffect.effects_type = 1;
        else
            ffEffect.effects_type = 2;

        _world->FFeedback_Play(&ffEffect);
    }

    int shotCount;

    if ( _num_weapons <= 1 )
        shotCount = 1;
    else
        shotCount = _num_weapons;

    for (int i = 0; i < shotCount; i++)
    {
        float offsetX;

        if ( shotCount == 1 )
            offsetX = arg->start_point.x;
        else
        {
            float spreadX = fabs(arg->start_point.x);
            offsetX = (i * 2) * spreadX / (shotCount - 1) - spreadX;
        }

        TUnitSpawn spawn;
        spawn.vehicle_id = arg->weapon;
        spawn.pos = _position + _rotation.Transpose().Transform( vec3d(offsetX, arg->start_point.y, arg->start_point.z) );

        wobj = _world->CreateWeapon(&spawn);

        if ( !wobj )
            return 0;

        wobj->SetLauncherBact(this);

        wobj->SetStartHeight(spawn.pos.y);

        wobj->_owner = _owner;

        if ( _bact_type != BACT_TYPES_GUN )
            _energy -= wobj->_energy / 300;

        if ( arg->direction.x != 0.0 || arg->direction.y != 0.0 || arg->direction.z != 0.0 )
        {
            wobj->_fly_dir = arg->direction;
        }
        else
        {
            wobj->_fly_dir = _rotation.AxisZ();
        }

        wobj->_fly_dir_length = _fly_dir_length + wproto.start_speed;

        if ( !(wproto._weaponFlags & 0x12) )
            wobj->_fly_dir_length *= 0.2;

        wobj->_rotation.SetZ( wobj->_fly_dir );

        wobj->_rotation.SetX( _rotation.AxisX() );

        wobj->_rotation.SetY( wobj->_rotation.AxisZ() * wobj->_rotation.AxisX() );

        if ( i == 0 )
        {
            if ( arg->flags & 1 )
                wobj->_position = wobj->_position - wobj->_rotation.AxisZ() * 30.0;
        }

        /** Missiles will be stored in another list
         *  so kidref will be not attached to anything.
         *  Looks it's somehow related to mentioned problem with dead cache.
        **/
        
        wobj->_kidRef.Detach();
        wobj->_parent = NULL;

        _missiles_list.push_back(wobj);

        int missileType = wobj->GetMissileType();

        if ( missileType == 3 )
        {
            setTarget_msg targetMsg;

            targetMsg.tgt = arg->target;
            targetMsg.tgt_type = arg->tgType;
            targetMsg.priority = 0;
            targetMsg.tgt_pos = arg->tgt_pos;

            wobj->SetTarget(&targetMsg);

            if ( arg->flags & 2 )
            {
                if ( arg->tgType == BACT_TGT_TYPE_CELL )
                    wobj->_primTpos.y = arg->tgt_pos.y;
            }
        }

        uamessage_newWeapon wpnMsg;
        wpnMsg.targetPos = arg->tgt_pos;

        if ( missileType == 2 )
        {
            wobj->_primTtype = BACT_TGT_TYPE_DRCT;
            wobj->_target_dir = wobj->_fly_dir;
        }

        wobj->_host_station = _host_station;
        _weapon_time = arg->g_time;

        SFXEngine::SFXe.startSound(&wobj->_soundcarrier, 1);

        if ( _world->_isNetGame )
        {
            wobj->_gid |= _owner << 24;

            wpnMsg.msgID = UAMSG_NEWWEAPON;
            wpnMsg.owner = _owner;
            wpnMsg.id = wobj->_gid;
            wpnMsg.launcher = _gid;
            wpnMsg.type = arg->weapon;
            wpnMsg.pos = spawn.pos;
            wpnMsg.flags = 0;
            wpnMsg.dir = wobj->_fly_dir * wobj->_fly_dir_length;
            wpnMsg.targetType = wobj->_primTtype;

            if ( wobj->_primTtype == BACT_TGT_TYPE_UNIT )
            {
                wpnMsg.target = wobj->_primT.pbact->_gid;
                wpnMsg.targetOwner = wobj->_primT.pbact->_owner;
            }

            _world->NetBroadcastMessage(&wpnMsg, sizeof(wpnMsg), true);
        }

        if ( arg->flags & 1 )
        {
            if ( i == 0 )
            {
                if ( _oflags & BACT_OFLAG_VIEWER )
                {
                    setBACT_viewer(false);
                    wobj->setBACT_viewer(true);
                }
            }
        }
        
        if ( arg->flags & 4 )
            wobj->SetIgnoreBuilds(1);
            

        if ( arg->tgType != BACT_TGT_TYPE_UNIT )
        {
            int life_time_nt = wproto.life_time_nt;

            if ( life_time_nt )
                wobj->SetLifeTime(life_time_nt);
        }
    }

    if ( _kill_after_shot )
    {
        if ( _oflags & BACT_OFLAG_USERINPT )
        {
            setBACT_viewer(false);
            wobj->setBACT_viewer(true);
        }

        TEnergyChange energyChange;
        energyChange.unit = _parent;
        energyChange.energy = -2 * _energy_max;

        ModifyEnergy(&energyChange);
    }
    return 1;
}

size_t NC_STACK_ypabact::SetPosition(TUnitPlacement *arg)
{
    TSectorInfo sect_info;

    sect_info.pos_x = arg->pos.x;
    sect_info.pos_z = arg->pos.z;
    if (!_world->GetSectorInfo(&sect_info))
        return 0;

    if ( _pSector )
        _cellRef.Detach();

    _cellRef = sect_info.pcell->unitsList.push_back(this);

    _pSector = sect_info.pcell;
    _old_pos = arg->pos;
    _position = arg->pos;
    _cellId = sect_info.CellId;

    if ( !(arg->flags & 2) )
        CorrectPositionInLevelBox(NULL);

    return 1;
}

void NC_STACK_ypabact::GetSummary(TSummaryQuery *arg)
{
    for ( NC_STACK_ypabact* &node : _kidList )
        node->GetSummary(arg);

    if ( _status != BACT_STATUS_DEAD )
    {
        switch ( arg->enrg_type )
        {
        case 1:
            arg->enrg_sum += _energy;
            break;

        case 3:
            arg->enrg_sum++;
            break;

        case 2:
            arg->enrg_sum += _shield;
            break;

        case 4:
            arg->enrg_sum += _energy_max;
            break;

        case 5:
        {
            arg->enrg_sum += _attackersList.size();
        }
        break;

        default:
            break;
        }
    }
}

// Update bact energy
void NC_STACK_ypabact::EnergyInteract(update_msg *arg)
{
    if ( _status != BACT_STATUS_DEAD )
    {
        int elapsed = _clock - _energy_time;

        if ( elapsed >= 1500 )
        {
            _energy_time = _clock;

            TReloadRatios reloadRatios;
            reloadRatios.owner = _pSector->owner;

            _world->GetReloadRatios(&reloadRatios);

            float elapsedSec = elapsed / 1000.0;

            float denerg = _energy_max * elapsedSec * _pSector->energy_power * reloadRatios.ratioClamped / 7000.0;

            if ( _owner == _pSector->owner )
                _energy += denerg;
            else
                _energy -= denerg;

            if ( _energy < 0 )
                _energy = 0;

            if ( _energy > _energy_max )
                _energy = _energy_max;
        }
    }
}

void NC_STACK_ypabact::ApplyImpulse(TImpulse *arg)
{
    float massScale = 50.0 / _mass;
    float energyScale = arg->energ * 0.0004;

    vec3d pushDir = _position - arg->pos;

    float distance = pushDir.length();

    if ( distance <= _radius )
    {
        vec3d velocity = (arg->dir * (2.5 * arg->mass * arg->force) + _fly_dir * _mass * _fly_dir_length) / (_mass + arg->mass);

        _fly_dir_length = velocity.normalise();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = velocity;

        pushDir = arg->dir;

        distance = 1.0;
    }
    else
    {
        pushDir /= distance;

        vec3d velocity = _fly_dir * _fly_dir_length + (pushDir * massScale * energyScale) / distance;

        _fly_dir_length = velocity.normalise();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = velocity;
    }

    CorrectPositionInLevelBox(NULL);

    _status_flg &= ~BACT_STFLAG_LAND;

    float angle = massScale * 0.01 * energyScale / distance;

    float cos_len = pushDir.dot(_rotation.AxisZ());

    // cos(45) == 0.7071
    if ( fabs(cos_len) > 0.7071 )
    {
        if ( cos_len > 0.7071 )
            _rotation = mat3x3::RotateX(-angle) * _rotation;
        else
            _rotation = mat3x3::RotateX(angle) * _rotation;
    }
    else
    {
        if ( pushDir.XZ().cross( _rotation.AxisZ().XZ() ) >= 0.0 )
            _rotation = mat3x3::RotateZ(angle) * _rotation;
        else
            _rotation = mat3x3::RotateZ(-angle) * _rotation;
    }
}

void NC_STACK_ypabact::ModifyEnergy(TEnergyChange *arg)
{
    if (_world && (_oflags & BACT_OFLAG_VIEWER))
    {
        if (_world->getYW_invulnerable() && arg->energy > -1000000)
            return;
    }
    
    bool isNetGame = false;
    if (_world && _world->_isNetGame)
        isNetGame = true;
    
    bool friendlyFire = false;
    if (!arg->unit || _owner == arg->unit->_owner)
        friendlyFire = true;

    if ( isNetGame && friendlyFire == false )
    {
        uamessage_vhclEnergy vhclEnrgy;
        vhclEnrgy.msgID = UAMSG_VHCLENERGY;
        vhclEnrgy.owner = _owner;
        vhclEnrgy.id = _gid;
        vhclEnrgy.energy = arg->energy;

        if ( arg->unit )
        {
            vhclEnrgy.killer = arg->unit->_gid;
            vhclEnrgy.killerOwner = arg->unit->_owner;
        }
        else
        {
            vhclEnrgy.killer = 0;
            vhclEnrgy.killerOwner = 0;
        }

        _world->NetBroadcastMessage(&vhclEnrgy, sizeof(vhclEnrgy), true);
    }
    else
    {
        _energy += arg->energy;

        if ( _energy <= 0 )
        {
            if ( arg->unit )
                _killer_owner = arg->unit->_owner;
            else
                _killer_owner = 0;

            _killer = arg->unit;
            _status_flg &= ~BACT_STFLAG_LAND;

            setState_msg stateMsg;
            stateMsg.newStatus = BACT_STATUS_DEAD;
            stateMsg.unsetFlags = 0;
            stateMsg.setFlags = 0;

            SetState(&stateMsg);

            Die();
        }
    }
}

bool NC_STACK_ypabact::IsFastAlong(vec3d *arg)
{
    float tmp2 = arg->dot( _fly_dir * _fly_dir_length );

    if ( fabs(tmp2) > 15.0 )
        return true;

    return false;
}


void PushOffSideways(NC_STACK_ypabact *bact)
{
    if ( bact->_fly_dir.x < 0.0 )
        bact->_fly_dir.x -= 7.0;

    if ( bact->_fly_dir.z < 0.0 )
        bact->_fly_dir.z -= 7.0;

    if ( bact->_fly_dir.x >= 0.0 )
        bact->_fly_dir.x += 7.0;

    if ( bact->_fly_dir.z >= 0.0 )
        bact->_fly_dir.z += 7.0;

    if ( bact->_fly_dir_length < 15.0 )
        bact->_fly_dir_length = 15.0;

    float len = bact->_fly_dir.length();

    if ( len <= 0.001 )
        bact->_fly_dir = vec3d(0.0, 1.0, 0.0);
    else
        bact->_fly_dir /= len;
}

void AlignToGroundNormal(NC_STACK_ypabact *bact, const vec3d &vec)
{
    vec3d vaxis = bact->_rotation.AxisY() * vec;

    if ( vaxis.normalise() != 0.0 )
    {
        float angle = clp_acos( vec.dot(bact->_rotation.AxisY()) );

        if ( angle > 0.001 )
            bact->_rotation *= mat3x3::AxisAngle(vaxis, angle);
    }
}

void AnimateScaleFX(NC_STACK_ypabact *bact, int frameTime)
{
    bact->_status_flg |= BACT_STFLAG_SCALE;

    if ( bact->_scale_duration > bact->_scale_pos )
    {
        float spinAngle = bact->_maxrot * frameTime / 1000.0;


        bact->_scale_speed += bact->_scale_accel * frameTime / 1000.0;
        bact->_scale_start += bact->_scale_speed * (frameTime / 1000.0);

        bact->_scale = vec3d(bact->_scale_start);

        bact->_rotation = mat3x3::RotateY(spinAngle) * bact->_rotation;

        int fxModelCount = 0;
        for (int i = 0; i < 32; i++)
        {
            if ( bact->_vp_fx_models[i] )
                fxModelCount++;
        }

        if ( fxModelCount )
        {
            int fxIndex = bact->_scale_pos * fxModelCount / bact->_scale_duration;

            bact->SetVP(bact->_vp_fx_models[fxIndex]);
        }

        bact->_scale_pos += frameTime;
    }
    else
    {
        bact->_yls_time = -1;
        bact->Release();
    }
}

size_t NC_STACK_ypabact::CrashOrLand(TCrashLandStep *arg)
{
    TSphereContact contacts[10];

    int sphereHit = 0;

    if ( _status_flg & BACT_STFLAG_SEFFECT )
    {
        AnimateScaleFX(this, arg->frameTime);
    }
    else
    {
        float radius;
        float overEof;

        if ( _oflags & BACT_OFLAG_VIEWER )
        {
            radius = _viewer_radius;
            overEof = _viewer_overeof;
        }
        else
        {
            radius = _radius;
            overEof = _overeof;
        }

        if ( _bact_type == BACT_TYPES_ROBO )
            overEof = 60.0;

        vec3d vaxis = vec3d( -_rotation.m12, 0.0, _rotation.m10 );

        float ftime = arg->frameTime / 1000.0;

        if ( vaxis.normalise() > 0.001 && !(arg->flags & 1) )
        {
            float angle = clp_acos( _rotation.m11 );
            float maxrot = _maxrot * ftime;

            if ( angle > maxrot )
                angle = maxrot;

            if ( fabs(angle) > BACT_MIN_ANGLE )
            {
                _rotation *= mat3x3::AxisAngle(vaxis, angle);
            }
        }

        if ( arg->flags & 2 )
        {
            float spinAngle = fabs(_fly_dir_length) * ftime * 0.08;

            _rotation = mat3x3::RotateZ(spinAngle) * _rotation;
        }

        if ( !(_status_flg & BACT_STFLAG_LAND) )
        {
            if ( arg->flags & 1 )
                _airconst = 0;
            else
                _airconst = 500.0;

            for (int i = 0; i < 3; i++)
            {

                move_msg moveMsg;

                moveMsg.period = ftime;
                moveMsg.flag = 1;

                Move(&moveMsg);

                int bounced = 0;

                if ( _oflags & BACT_OFLAG_BACTCOLL )
                {
                    if ( CollisionWithBact(arg->frameTime) )
                    {
                        if ( _bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR )
                        {
                            PushOffSideways(this);
                            return 0;
                        }

                        return 0;
                    }
                }

                if ( _oflags & BACT_OFLAG_VIEWER )
                {
                    TSphereCollisionQuery sphereQuery;
                    sphereQuery.pos = _fly_dir * _fly_dir_length * ftime * 6.0 + _position;
                    sphereQuery.dir = _fly_dir;
                    sphereQuery.radius = radius;
                    sphereQuery.collisions = contacts;
                    sphereQuery.flags = 0;
                    sphereQuery.coll_max = 10;

                    _world->FindSphereCollisions(&sphereQuery);

                    if ( sphereQuery.coll_count )
                    {
                        int groundHit = 0;
                        sphereHit = 1;

                        vec3d normalSum;

                        for (int j = sphereQuery.coll_count - 1; j >= 0; j--)
                        {
                            TSphereContact *contact = &sphereQuery.collisions[ j ];

                            normalSum += contact->normal;

                            if ( normalSum.y > 0.6 )
                                groundHit = 1;
                        }

                        TCollisionNormal collNormal;
                        vec3d normal;

                        float lnn = normalSum.length();

                        if ( lnn != 0.0 )
                        {
                            collNormal.normal = normalSum / lnn;

                            normal = collNormal.normal;
                        }
                        else
                        {
                            normal = _fly_dir;
                            collNormal.normal = _fly_dir;
                        }

                        if ( arg->flags & 1 )
                        {
                            _energy -= fabs(_fly_dir_length) * 10.0;

                            if ( _energy <= 0 || (GetVP() == _vp_dead && _status == BACT_STATUS_DEAD) )
                            {
                                setState_msg stateMsg;
                                stateMsg.setFlags = BACT_STFLAG_DEATH2;
                                stateMsg.unsetFlags = 0;
                                stateMsg.newStatus = BACT_STATUS_NOPE;

                                SetState(&stateMsg);
                            }

                            if ( _oflags & BACT_OFLAG_USERINPT )
                            {
                                if ( fabs(_fly_dir_length) > 7.0 )
                                    SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                                TFFeedbackEffect ffEffect;

                                ffEffect.effects_type = 5;
                                ffEffect.strength = 1.0;
                                ffEffect.pos_x = normalSum.x * 10.0 + _position.x;
                                ffEffect.pos_z = normalSum.z * 10.0 + _position.z;

                                _world->FFeedback_Play(&ffEffect);
                            }

                            if ( normalSum.y >= 0.6 && groundHit )
                            {
                                _position.y = _old_pos.y;

                                _status_flg |= BACT_STFLAG_LAND;

                                _fly_dir_length *= _fly_dir.XZ().length();

                                AlignToGroundNormal(this, normal);

                                _reb_count = 0;
                            }
                            else
                            {
                                Recoil(&collNormal);

                                _reb_count++;

                                bounced = 1;

                                if ( _reb_count > 50 )
                                {
                                    _energy = -10000;
                                    _status_flg |= BACT_STFLAG_LAND;
                                }
                            }
                        }
                        else if ( normalSum.y < 0.6 )
                        {
                            Recoil(&collNormal);

                            bounced = 1;
                        }
                        else
                        {
                            _position.y = _old_pos.y;
                            _fly_dir_length = 0;
                            _reb_count = 0;
                            _status_flg |= BACT_STFLAG_LAND;
                        }
                    }
                }

                if ( !sphereHit )
                {
                    TRayIntersection ray;
                    ray.stPos = _old_pos;
                    ray.vect = _position - _old_pos + vec3d(0.0, overEof, 0.0);
                    ray.flags = 0;

                    _world->IntersectRay(&ray);

                    if ( ray.isect )
                    {
                        TCollisionNormal collNormal;

                        collNormal.normal = ray.skel->polygons[ray.polyID].Normal();

                        vec3d normal = collNormal.normal;

                        if ( arg->flags & 1 )
                        {
                            _energy -= fabs(_fly_dir_length) * 10.0;

                            if ( _energy <= 0 || (GetVP() == _vp_dead && _status == BACT_STATUS_DEAD) )
                            {
                                setState_msg stateMsg;
                                stateMsg.setFlags = BACT_STFLAG_DEATH2;
                                stateMsg.unsetFlags = 0;
                                stateMsg.newStatus = BACT_STATUS_NOPE;

                                SetState(&stateMsg);
                            }

                            if ( _oflags & BACT_OFLAG_USERINPT )
                            {
                                if ( fabs(_fly_dir_length) > 7.0 )
                                    SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                                TFFeedbackEffect ffEffect;

                                ffEffect.effects_type = 5;
                                ffEffect.strength = 1.0;
                                ffEffect.pos_x = normal.x * 10.0 + _position.x;
                                ffEffect.pos_z = normal.z * 10.0 + _position.z;

                                _world->FFeedback_Play(&ffEffect);
                            }

                            if ( ray.skel->polygons[ray.polyID].B < 0.6 )
                            {
                                Recoil(&collNormal);

                                _reb_count++;

                                bounced = 1;

                                if ( _reb_count > 50 )
                                {
                                    _energy = -10000;
                                    _status_flg |= BACT_STFLAG_LAND;
                                }
                            }
                            else
                            {
                                _position = ray.isectPos - vec3d(0.0, overEof, 0.0);

                                _status_flg |= BACT_STFLAG_LAND;

                                _fly_dir_length *= _fly_dir.XZ().length();

                                AlignToGroundNormal(this, normal);

                                _reb_count = 0;
                            }
                        }
                        else if ( ray.skel->polygons[ray.polyID].B < 0.6 )
                        {
                            Recoil(&collNormal);

                            bounced = 1;
                        }
                        else
                        {
                            _position.y = ray.isectPos.y - overEof;

                            _fly_dir_length = 0;
                            _reb_count = 0;
                            _status_flg |= BACT_STFLAG_LAND;
                        }
                    }
                }

                if ( !bounced ) // Alternative exit from loop
                    break;
            }

        }
        if ( _status_flg & BACT_STFLAG_LAND )
            return 1;
    }
    return 0;
}


void AbsorbPlasmaEnergy(NC_STACK_ypabact *bact, NC_STACK_ypabact *plasmaUnit)
{
    int plasmaDuration = (int)((float)plasmaUnit->_energy_max * 0.7);

    if ( plasmaDuration < 10000 )
        plasmaDuration = 10000;

    if ( plasmaDuration > 25000 )
        plasmaDuration = 25000;

    int energyGain = (float)plasmaUnit->_scale_time * 0.2 / (float)plasmaDuration * (float)plasmaUnit->_energy_max;

    if ( bact->_energy + energyGain > bact->_energy_max )
    {
        NC_STACK_yparobo *robo = bact->_host_station;

        int overflow = energyGain - (bact->_energy_max - bact->_energy);

        bact->_energy = bact->_energy_max;

        if ( robo->_energy + overflow > robo->_energy_max )
        {
            int stationOverflow = overflow - (robo->_energy_max - robo->_energy);

            robo->_energy = robo->_energy_max;

            if ( robo->_roboEnergyLife + stationOverflow >= robo->_energy_max )
            {
                robo->_roboEnergyMove += stationOverflow - (robo->_energy_max - robo->_roboEnergyLife);

                robo->_roboEnergyLife = robo->_energy_max;

                if ( robo->_roboEnergyMove > robo->_energy_max )
                    robo->_roboEnergyMove = robo->_energy_max;
            }
            else
            {
                robo->_roboEnergyLife += stationOverflow;
            }
        }
        else
        {
            robo->_energy += overflow;
        }
    }
    else
    {
        bact->_energy += energyGain;
    }
}

size_t NC_STACK_ypabact::CollisionWithBact(int arg)
{
    bool isViewer = getBACT_viewer();

    float trad;
    if ( isViewer )
        trad = _viewer_radius;
    else
        trad = _radius;


    int otherHasCollNodes = 0;

    World::TRoboCollSet *ownCollNodes = getBACT_collNodes();

    if ( _fly_dir_length == 0.0 )
        return 0;

    vec3d collisionCenter(0.0, 0.0, 0.0);

    int hitCount = 0;

    World::TRoboCollSet *collNodes;

    for ( NC_STACK_ypabact* &bnode : _pSector->unitsList )
    {
        int isPlasma = bnode->_status == BACT_STATUS_DEAD && (bnode->_vp_extra[0].flags & 1) && (_oflags & BACT_OFLAG_USERINPT) && bnode->_scale_time > 0 ;

        if ( bnode != this && bnode->_bact_type != BACT_TYPES_MISSLE && (!bnode->IsDestroyed() || isPlasma) )
        {

            collNodes = bnode->getBACT_collNodes();

            int collCount;

            if ( collNodes )
            {
                collCount = collNodes->roboColls.size();
                otherHasCollNodes = 1;
            }
            else
            {
                collCount = 1;
            }

            for (int i = collCount - 1; i >= 0; i--)
            {
                float ttrad;
                vec3d spherePos;

                if (!collNodes)
                {
                    ttrad = trad;
                    spherePos = bnode->_position;
                }
                else
                {
                    World::TRoboColl *roboColl = &collNodes->roboColls[i];
                    ttrad = roboColl->robo_coll_radius;

                    spherePos = bnode->_position + bnode->_rotation.Transpose().Transform(roboColl->coll_pos);

                    if ( ttrad < 0.01 )
                        continue;
                }

                if ( (_position - spherePos).length() <= trad + ttrad )
                {
                    if ( !isPlasma )
                    {
                        collisionCenter += spherePos;

                        hitCount++;
                    }
                    else
                    {
                        AbsorbPlasmaEnergy(this, bnode);

                        bnode->_scale_time = -1;

                        if ( _world->_GameShell )
                            SFXEngine::SFXe.startSound(&_world->_GameShell->samples1_info, World::SOUND_ID_PLASMA);

                        if ( _world->_isNetGame )
                        {
                            uamessage_endPlasma epMsg;
                            epMsg.msgID = UAMSG_ENDPLASMA;
                            epMsg.owner = bnode->_owner;
                            epMsg.id = bnode->_gid;

                            _world->NetBroadcastMessage(&epMsg, sizeof(epMsg), true);

                            if ( bnode->_owner != _owner )
                            {
                                bnode->_vp_extra[0].flags = 0;
                                bnode->_vp_extra[0].SetVP((NC_STACK_base::Instance *)NULL);// = NULL;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    if ( !hitCount || (ownCollNodes && !otherHasCollNodes) )
    {
        _status_flg &= ~BACT_STFLAG_BCRASH;
        return 0;
    }

    collisionCenter /= (double)hitCount;

    vec3d toCollisionCenter = collisionCenter - _position;

    float distance = toCollisionCenter.length();

    if ( distance < 0.0001)
        return 0;

    TCollisionNormal collNormal;
    collNormal.normal = toCollisionCenter / distance;

    // FIX MY MATH
    // toCollisionCenter should be normalised?
    // May be replace it with "dot < 0.0" ?
    // Because cos of 1.0...0 is 0..PI/2 and 0...-1.0 is PI/2..PI
    if ( clp_acos( toCollisionCenter.dot( _fly_dir ) ) > C_PI_2 )
        return 0;

    if ( !(_status_flg & BACT_STFLAG_BCRASH) )
    {
        if ( isViewer )
        {
            SFXEngine::SFXe.startSound(&_soundcarrier, 6);

            _status_flg |= BACT_STFLAG_BCRASH;

            TFFeedbackEffect ffEffect;
            ffEffect.strength = 1.0;
            ffEffect.pos_x = collisionCenter.x;
            ffEffect.pos_z = collisionCenter.z;
            ffEffect.effects_type = 5;

            _world->FFeedback_Play(&ffEffect);
        }
    }

    if ( fabs(_fly_dir_length) < 0.1 )
        _fly_dir_length = 1.0;

    Recoil(&collNormal);

    _target_vec = _fly_dir;

    _AI_time1 = _clock;
    _AI_time2 = _clock;

    return 1;
}

void NC_STACK_ypabact::Recoil(TCollisionNormal *arg)
{
    if ( !(_status_flg & BACT_STFLAG_LAND) )
    {
        if ( _fly_dir.dot(arg->normal) >= 0.0 )
        {
            if ( _fly_dir_length != 0.0 )
            {
                _position = _old_pos;

                float reflectLen = _fly_dir.dot(arg->normal) * 2.0;

                _fly_dir -= arg->normal * reflectLen;

                _fly_dir_length *= 25.0 / (fabs(_fly_dir_length) + 10.0);
            }
        }
    }
}


bool NC_STACK_ypabact::IsAnyKidWithoutSecondUnitTarget() const
{
    for ( NC_STACK_ypabact* node : _kidList )
    {
        if ( node->_secndTtype != BACT_TGT_TYPE_UNIT )
            return true;
    }
    return false;
}

NC_STACK_ypabact * NC_STACK_ypabact::GetEnemyCandidateInSector(const cellArea &cell, float *radius, char *job) const
{
    NC_STACK_ypabact *lastSelectedUnit = NULL;

    const World::TVhclProto &proto = _world->GetVhclProtos().at( _vehicleID );

    for( NC_STACK_ypabact* cel_unit : cell.unitsList )
    {
        // Do not target missile or dead
        if ( cel_unit->_bact_type == BACT_TYPES_MISSLE ||
             cel_unit->_status == BACT_STATUS_DEAD )
            continue;
        
        // Do not target same fraction unit or owner == 0
        if ( cel_unit->_owner == _owner || cel_unit->_owner == World::OWNER_0 )
            continue;
            
        int jobLevel;
        
        switch ( cel_unit->_bact_type )
        {
        case BACT_TYPES_BACT:
            jobLevel = proto.job_fighthelicopter;
            break;

        case BACT_TYPES_TANK:
        case BACT_TYPES_CAR:
            jobLevel = proto.job_fighttank;
            break;

        case BACT_TYPES_FLYER:
        case BACT_TYPES_UFO:
            jobLevel = proto.job_fightflyer;
            break;

        case BACT_TYPES_ROBO:
            jobLevel = proto.job_fightrobo;
            break;

        default:
            jobLevel = 5;
            break;
        }

        // do not target if job for this unit is less of previous
        if ( jobLevel < *job )
            continue;
        
        float radivs = (_position - cel_unit->_position).length();
        
        // do not target if distance more than for old selected unit
        if ( radivs > *radius && !cel_unit->getBACT_viewer() )
            continue;

        // If own unit is not gun or robo do additional checks for distance
        if ( _bact_type != BACT_TYPES_GUN && _bact_type != BACT_TYPES_ROBO )
        {
            vec3d ownTargetPos;
            bool isLeader;

            if ( IsParentMyRobo() )
            {
                if ( _primTtype == BACT_TGT_TYPE_CELL )
                {
                    ownTargetPos = _primTpos;

                }
                else if ( _primTtype == BACT_TGT_TYPE_UNIT )
                {
                    ownTargetPos = _primT.pbact->_position;
                }
                else
                {
                    ownTargetPos = _position;
                }

                isLeader = true;
            }
            else
            {
                if ( _parent->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    ownTargetPos = _parent->_primTpos;
                }
                else if ( _parent->_primTtype == BACT_TGT_TYPE_UNIT )
                {
                    ownTargetPos = _parent->_primT.pbact->_position;
                }
                else
                {
                    ownTargetPos = _position;
                }

                isLeader = false;
            }

            // if primary/secondary squad target distance is more than 3 sector length
            // do additional check
            if ( (ownTargetPos.XZ() - _position.XZ()).length() > World::CVUnitFarSecDist )
            {
                int countOwnAttackers = 0;

                for ( const TBactAttacker &ainf : cel_unit->_attackersList )
                {
                    if ( ainf.attacker->_secndTtype == BACT_TGT_TYPE_UNIT &&
                         ainf.attacker->_secndT.pbact == cel_unit && 
                         ainf.attacker->_owner == _owner )
                        countOwnAttackers++;

                    if ( countOwnAttackers > 1 ) // if more than 1 do break already
                        break;
                }

                // If current unit already attacked by more than 1 another units - skip it
                if ( countOwnAttackers > 1 )
                    continue;
                
                // If we is leader and if some of us kids do not has second target unit
                // let's skip this unit and leave it for targeting by kid
                if (isLeader && IsAnyKidWithoutSecondUnitTarget() )
                    continue;
            }
        }
        
        // If test of sector below of unit is OK then make it current candidate
        // and do tests for next units in this sector
        if ( TestTargetSector(cel_unit) )
        {
            *radius = radivs;
            *job = jobLevel;
            lastSelectedUnit = cel_unit;
        }
    }

    return lastSelectedUnit;
}

NC_STACK_ypabact * NC_STACK_ypabact::GetSectorTarget(Common::Point CellId) const
{
    NC_STACK_ypabact *enemy = NULL;

    if ( _world->IsSector(CellId) )
    {
        float rad = 1800.0;
        char job = 0;

        for (int x = -1; x < 2; x++)
        {
            for (int y = -1; y < 2; y++)
            {
                Common::Point pt = CellId + Common::Point(x, y);
                NC_STACK_ypabact *unit = GetEnemyCandidateInSector( _world->SectorAt(pt), &rad, &job);

                if ( unit ) enemy = unit;
            }
        }
    }
    return enemy;
}

void NC_STACK_ypabact::GetBestSectorPart(vec3d *arg)
{
    TSectorInfo sectorInfo;
    sectorInfo.pos_x = arg->x;
    sectorInfo.pos_z = arg->z;

    _world->GetSectorInfo(&sectorInfo);

    vec2d ttmp = World::SectorIDToCenterPos2( sectorInfo.CellId );

    arg->x = ttmp.x;
    arg->z = ttmp.y;

    if ( sectorInfo.pcell->SectorType != 1 )
    {
        int bestHealth = 0;

        for (int y = 0; y < 3; y++)
        {
            for (int x = 0; x < 3; x++)
            {
                if ( sectorInfo.pcell->buildings_health.At(x, y) > bestHealth )
                {
                    arg->z = 300.0 * (-1 + y) + ttmp.y;
                    arg->x = 300.0 * (-1 + x) + ttmp.x;

                    bestHealth = sectorInfo.pcell->buildings_health.At(x, y);
                }
            }
        }
    }
}

void NC_STACK_ypabact::GetForcesRatio(TForcesRatio *arg)
{
    TSectorInfo sectorInfo;

    arg->ownEnergy = 0;
    arg->enemyEnergy = 0;

    if ( arg->flags & 1 )
    {
        sectorInfo.pos_x = _position.x;
        sectorInfo.pos_z = _position.z;
    }
    else
    {
        sectorInfo.pos_x = arg->pos.x;
        sectorInfo.pos_z = arg->pos.z;
    }

    if ( _world->GetSectorInfo(&sectorInfo) )
    {
        cellArea *cell = sectorInfo.pcell;
        Common::Point pt = cell->CellId;

        if ( sectorInfo.CellId.x != 0 && sectorInfo.CellId.y != 0 )
        {
            // left-up
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.y )
        {
            // up
            cellArea &tcell = _world->SectorAt(pt.x, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.x < _wrldSectors.x - 1 && sectorInfo.CellId.y )
        {
            // right-up
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.x )
        {
            // left
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        // center
        if ( cell->IsCanSee(_owner) )
        {
            for (NC_STACK_ypabact* &cl_unit : cell->unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
        }

        if ( sectorInfo.CellId.x < _wrldSectors.x - 1 )
        {
            // right
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y);

            if ( tcell.IsCanSee(_owner) )
            {
               for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.x != 0 && sectorInfo.CellId.y < _wrldSectors.y - 1 )
        {
            // left-down
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.y < _wrldSectors.y - 1  )
        {
            // down
            cellArea &tcell = _world->SectorAt(pt.x, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( sectorInfo.CellId.x < _wrldSectors.x - 1 && sectorInfo.CellId.y < _wrldSectors.y - 1 )
        {
            // down-right
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD && 
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) && 
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->ownEnergy += cl_unit->_energy;
                            else
                                arg->enemyEnergy += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( !(arg->flags & 2) )
        {
            int buildingHealth = 0;

            if ( cell->SectorType == 1 )
            {
                buildingHealth = cell->buildings_health.At(0, 0);
            }
            else
            {
                for (auto helth : cell->buildings_health)
                    buildingHealth += helth;

                buildingHealth /= cell->buildings_health.size();
            }

            if ( cell->owner == _owner )
            {
                if ( arg->flags & 4 )
                    arg->ownEnergy += buildingHealth * 120;
            }
            else
            {
                arg->enemyEnergy += buildingHealth * 120;
            }
        }
    }
}

void NC_STACK_ypabact::GetFormationPosition(TFormationSlot *arg)
{
    vec3d forward = _rotation.AxisZ().X0Z();
    forward.normalise();

    arg->pos = _position - forward * ( (arg->slot / 3 + 1) * 150.0 );

    int column = arg->slot % 3;

    if ( column == 0 )
    {
        arg->pos.x += 100.0 * forward.z;
        arg->pos.z += -100.0 * forward.x;
    }
    else if ( column == 2 )
    {
        arg->pos.x += -100.0 * forward.z;
        arg->pos.z += 100.0 * forward.x;
    }

    // With y = 0
    //arg->pos2 = vec3d::X0Z( arg->pos1.XZ() - _position.XZ() );
}

// Reset
void NC_STACK_ypabact::Renew()
{
    _oflags = BACT_OFLAG_EXACTCOLL;
    _status_flg = 0;
    _host_station = NULL;
    _yls_time = 3000;
    _primTtype = BACT_TGT_TYPE_NONE;

    _secndTtype = BACT_TGT_TYPE_NONE;
    _primT_cmdID = 0;

    _wrldSectors = _world->GetMapSize();

    _wrldSize = World::SectorIDToPos2( _wrldSectors );

    _commandID = 0;
//    bact->field_3D1 = 1;
    _killer = NULL;
    _brkfr_time = 0;
    _brkfr_time2 = 0;
    _mpos.x = 0;
    _mpos.y = 0;
    _mpos.z = 0;
    _gun_leftright = 0.0;
    _scale_time = 0;
    _clock = 0;
    _AI_time1 = 0;
    _AI_time2 = 0;
//    bact->field_921 = 0;
//    bact->field_925 = 0;
    _search_time1 = 0;
    _search_time2 = 0;
    _slider_time = 0;
//    bact->field_951 = 0;
    _mgun_time = 0;
    _weapon_time = 0;
    _newtarget_time = 0;
    _assess_time = 0;
    _scale_pos = 0;
    _scale_delay = 0;
    _beam_time = 0;
    _energy_time = 0;
    _fe_time = -45000;
    _salve_counter = 0;
    _kill_after_shot = 0;
    
    Common::DeleteAndNull(&_current_vp);

    _vp_active = 0;
    _volume = 0; //_soundcarrier.Sounds[0].Volume;
    _pitch = 0; //_soundcarrier.Sounds[0].Pitch;

    _m_cmdID = 0;
    _gun_angle_user = _gun_angle;
    _oflags |= BACT_OFLAG_LANDONWAIT;

    for (World::DestFX &x : _destroyFX)
        x = World::DestFX();
    
    _extDestroyFX.clear();

    for (extra_vproto &vp : _vp_extra)
        vp = extra_vproto();
    
    _current_waypoint = 0;

    _attackersList.clear();
    _kidList.clear();
    _missiles_list.clear();
}

void NC_STACK_ypabact::HandBrake(update_msg *arg)
{
    // TODO(bug): 9.77665 here; gravity is 9.80665 everywhere else.
    _thraction = _mass * 9.77665;

    float ftime = arg->frameTime * 0.001;

    vec3d vaxis = _rotation.AxisY() * vec3d(0.0, 1.0, 0.0);

    if ( vaxis.normalise() > 0.001 )
    {
        float angle = clp_acos( _rotation.AxisY().dot( vec3d(0.0, 1.0, 0.0) ) );
        float maxrot = _maxrot * ftime;

        // TODO(bug): Multiplies two angles. Probably meant to clamp to v11, the
        // most it may turn this step.
        if ( angle > maxrot )
            angle = (angle * 1.5) * maxrot;

        if ( fabs(angle) <= 0.0015 )
        {
            _rotation.SetY( vec3d(0.0, 1.0, 0.0) );

            vec3d axisX = _rotation.AxisX().X0Z();
            axisX.normalise();

            _rotation.SetX( axisX );

            vec3d axisZ = _rotation.AxisZ().X0Z();
            axisZ.normalise();

            _rotation.SetZ( axisZ );

            if ( fabs(_fly_dir_length) < 0.1 )
            {
                _fly_dir = vec3d(0.0, 1.0, 0.0);

                _fly_dir_length = 0;
            }
        }
        else
        {
            _rotation *= mat3x3::AxisAngle(vaxis, angle);
        }
    }

    _fly_dir_length *= 0.8;
}

void NC_STACK_ypabact::CreationTimeUpdate(update_msg *arg)
{
    _scale_time -= arg->frameTime;

    float ftime = arg->frameTime / 1000.0;

    if ( _scale_time > 0 )
    {
        _status_flg |= BACT_STFLAG_SCALE;

        if ( _scale_time < 0 )
            _scale = vec3d(1.0);
        else
            _scale = vec3d( 0.9 / ((float)_scale_time / 1000.0 + 0.9) + 0.1 );

        _rotation = mat3x3::RotateY( 2.5 / _scale.x * ftime ) * _rotation;
    }
    else
    {
        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_NORMAL;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;

        SetState(&stateMsg);

        _status_flg &= ~BACT_STFLAG_SCALE;

        TUnitPlacement placement;

        placement.pos = _position;
        placement.flags = 0;

        SetPosition(&placement);

        NC_STACK_ypabact *userRobo = _world->getYW_userHostStation();

        if ( _host_station == userRobo )
        {

            if ( IsParentMyRobo() )
            {
                TRoboMessage roboMsg;
                roboMsg.unit = this;
                roboMsg.msgID = 14;
                roboMsg.param1 = 0;
                roboMsg.param2 = 0;
                roboMsg.param3 = 0;
                roboMsg.priority = 26;

                _host_station->placeMessage(&roboMsg);
            }
        }

        if ( _host_station )
        {
            if ( _bact_type != BACT_TYPES_GUN )
            {
                _fly_dir = placement.pos - _host_station->_position;

                float fly_len = _fly_dir.length();

                if ( fly_len > 0.001 )
                    _fly_dir /= fly_len;

                _fly_dir_length = 20.0;
            }
        }
    }
}

size_t NC_STACK_ypabact::IsDestroyed()
{
    return (GetVP() == _vp_dead || GetVP() == _vp_genesis || GetVP() == _vp_megadeth) && _status == BACT_STATUS_DEAD;
}

size_t NC_STACK_ypabact::CheckFireAI(TFireCheck *arg)
{
    vec3d tmp;

    if ( arg->tgType == 2 )
        tmp = arg->pos - _position;
    else
        tmp = arg->pos.X0Z() - _position.X0Z() + vec3d::OY(_height);

    float len = tmp.normalise();

    if ( len == 0.0 )
        return 0;

    World::TWeapProto *v8 = NULL;

    int weaponKind;

    if ( _weapon != -1 )
    {
        v8 = &_world->GetWeaponsProtos().at( _weapon );


        if ( v8->_weaponFlags & 1 )
            weaponKind = v8->_weaponFlags & 0xFE;
        else
            v8 = NULL;
    }

    if ( !v8 )
    {
        if ( _mgun == -1 )
            return 0;

        weaponKind = 2;
    }

    if ( arg->tgType == 2 )
    {
        float hitRadius;

        if ( v8 )
        {

            float combinedRadius = arg->radius * 0.8 + v8->radius;

            if ( combinedRadius >= 40.0 )
            {
                hitRadius = combinedRadius;
            }
            else
            {
                // TODO(bug): Odd minimum radius; the minigun branch below uses 40.0. Check
                // against the original.
                hitRadius = 3.0625;
            }
        }
        else
        {
            float targetRadius = arg->radius * 0.8;

            if ( targetRadius >= 40.0 )
                hitRadius = targetRadius;
            else
                hitRadius = 40.0;
        }

        if ( weaponKind )
        {
            if ( weaponKind == 16 )
            {
                if ( len < World::CVSectorLength && tmp.XZ().dot( _rotation.AxisZ().XZ() ) > 0.93 )
                    return 1;
            }
            else
            {
                vec3d tmp2 = tmp * _rotation.AxisZ();

                if ( len < World::CVSectorLength && (tmp.dot( _rotation.AxisZ() ) > 0.0) && hitRadius / len > tmp2.length() )
                    return 1;
            }
        }
        else
        {
            if ( (arg->pos.XZ() - _position.XZ()).length() < hitRadius && arg->pos.y > _position.y )
                return 1;
        }
    }
    else if ( v8 )
    {
        if ( weaponKind )
        {
            if ( weaponKind == 16 )
            {
                if ( len < World::CVSectorLength && tmp.XZ().dot( _rotation.AxisZ().XZ() ) > 0.91 )
                    return 1;
            }
            else if ( len < World::CVSectorLength && tmp.dot( _rotation.AxisZ() ) > 0.91 )
            {
                return 1;
            }
        }
        else
        {
            if ( (arg->pos.XZ() - _position.XZ()).length() < v8->radius )
                return 1;
        }
    }
    return 0;
}

void NC_STACK_ypabact::MarkSectorsForView()
{
    /* Missle does not have kids, if else it's a BUG or must be another missle */
    if ( _bact_type == BACT_TYPES_MISSLE )
        return;
 
    /* Unit already dead also must do not have any kids */
    if ( _status == BACT_STATUS_DEAD || _status == BACT_STATUS_CREATE )
        return;
    
    if ( !_parent || _cellId != _parent->_cellId || 
        (_radar > _parent->_radar || _unhideRadar > _parent->_unhideRadar) )
    {
        if ( _owner < 8 )
        {
            for (int i = -_radar; i <= _radar; i++)
            {
                int yy = _cellId.y + i;

                if ( _radar == 1 )
                {
                    if ( yy > 0 && yy < _wrldSectors.y - 1 )
                    {
                        if (_unhideRadar > 0)
                        {
                            if ( _cellId.x > 1 )
                            {
                                _world->SectorAt(_cellId.x - 1, yy).AddToViewMask(_owner);
                                _world->SectorAt(_cellId.x - 1, yy).AddUnhideMask(_owner);
                            }

                            _world->SectorAt(_cellId.x, yy).AddToViewMask(_owner);
                            _world->SectorAt(_cellId.x, yy).AddUnhideMask(_owner);

                            if ( _cellId.x + 1 < _wrldSectors.x - 1 )
                            {
                                _world->SectorAt(_cellId.x + 1, yy).AddToViewMask(_owner);
                                _world->SectorAt(_cellId.x + 1, yy).AddUnhideMask(_owner);
                            }
                        }
                        else
                        {
                            if ( _cellId.x > 1 )
                                _world->SectorAt(_cellId.x - 1, yy).AddToViewMask(_owner);

                            _world->SectorAt(_cellId.x, yy).AddToViewMask(_owner);

                            if ( _cellId.x + 1 < _wrldSectors.x - 1 )
                                _world->SectorAt(_cellId.x + 1, yy).AddToViewMask(_owner);
                        }
                    }
                }
                else
                {
                    float vtmp = POW2((float)_radar) - POW2((float)i);

                    if (vtmp < 0.0)
                        vtmp = 0.0;

                    int tmp = dround( sqrt(vtmp) );

                    if (_unhideRadar > 0 && Common::ABS(i) < _unhideRadar)
                    {
                        for (int j = -tmp; j <= tmp; j++)
                        {
                            Common::Point d(_cellId.x + j, yy);

                            if ( _world->IsGamePlaySector(d) )
                            {
                                _world->SectorAt(d).AddToViewMask(_owner);
                                if (Common::ABS(j) < _unhideRadar)
                                    _world->SectorAt(d).AddUnhideMask(_owner);
                            }
                        }
                    }
                    else
                    {
                        for (int j = -tmp; j <= tmp; j++)
                        {
                            Common::Point d(_cellId.x + j, yy);

                            if ( _world->IsGamePlaySector(d) )
                                _world->SectorAt(d).AddToViewMask(_owner);
                        }
                    }
                }
            }
        }
    }

    for( NC_STACK_ypabact* &kid : _kidList )
        kid->MarkSectorsForView();
}

void NC_STACK_ypabact::StuckFree(update_msg *arg)
{
//    if ( bact->field_93D > 0 )
//        bact->field_93D -= arg->field_4;

//    if ( bact->field_93D < 0 )
//        bact->field_93D = 0;

    if ( _bflags & BACT_OFLAG_BACTCOLL )
    {
//        if ( !bact->field_93D )
        _oflags |= BACT_OFLAG_BACTCOLL;
    }

    if ( _status != BACT_STATUS_NORMAL || _oflags & BACT_OFLAG_USERINPT )
    {
        _mpos = _position;
        _brkfr_time2 = _clock;
    }
    else
    {
        vec3d tmp = _mpos - _position;

        if (tmp.length() >= 12.0)
        {
            _mpos = _position;
            _brkfr_time2 = _clock;
        }
        else
        {
            if ( _oflags & BACT_OFLAG_BACTCOLL )
                _bflags |= BACT_OFLAG_BACTCOLL;

            if ( _clock - _brkfr_time2 > 10000 )
            {
                if ( (_bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR) && !(_status_flg & BACT_STFLAG_ATTACK) )
                {
                    _old_pos = _position;

                    _position += -_rotation.AxisZ() * 10.0;

                    CorrectPositionInLevelBox(NULL);

                    _rotation = mat3x3::RotateY(0.1) * _rotation;

                    TRayIntersection ray;
                    ray.stPos = _old_pos;
                    ray.vect = _position - _old_pos;
                    ray.flags = 1;

                    _world->IntersectRay(&ray);

                    if ( ray.isect )
                    {
                        _position = ray.isectPos - vec3d::OY(5.0);
                    }
                }
            }
        }
    }
}

size_t NC_STACK_ypabact::FireMinigun(TMinigunShot *arg)
{
    int isNetGame = 0;

    if ( _world->_isNetGame )
        isNetGame = 1;

    if ( _mgun == -1 )
        return 0;

    NC_STACK_ypabact *v108 = NULL;
    float nearestDist = 0.0;
    float hitRadius = 0.0;
    vec3d nearestPos;

    TSectorInfo sectorInfo;
    sectorInfo.pos_x = _position.x;
    sectorInfo.pos_z = _position.z;

    vec2d tmp = _position.XZ() + arg->dir.XZ() * 1000.0;

    _world->GetSectorInfo(&sectorInfo);

    cellArea *pCells[3];
    pCells[0] = sectorInfo.pcell;

    sectorInfo.pos_x = tmp.x;
    sectorInfo.pos_z = tmp.y;
    _world->GetSectorInfo(&sectorInfo);

    pCells[2] = sectorInfo.pcell;

    if ( sectorInfo.pcell == pCells[0] )
    {
        pCells[1] = pCells[0];
    }
    else
    {
        vec2d tmp2 = _position.XZ() + (tmp - _position.XZ()) * 0.5;
        sectorInfo.pos_x = tmp2.x;
        sectorInfo.pos_z = tmp2.y;

        _world->GetSectorInfo(&sectorInfo);

        pCells[1] = sectorInfo.pcell;
    }

    int isRoboGun = 0;
    if ( _bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( this );
        int onRobo = gun->IsRoboGun();

        if ( onRobo )
            isRoboGun = 1;
    }
    else
    {
        _energy -= _gun_power * arg->period / 300.0;
    }

    for(int i = 0; i < 3; i++)
    {
        if ( i <= 0 || pCells[ i ] != pCells[ i - 1 ] )
        {
            for ( NC_STACK_ypabact* &cellUnit : pCells[ i ]->unitsList )
            {
                if ( cellUnit != this && cellUnit->_bact_type != BACT_TYPES_MISSLE && cellUnit->_status != BACT_STATUS_DEAD )
                {
                    int targetIsRoboGun = 0;
                    if (cellUnit->_bact_type == BACT_TYPES_GUN)
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( cellUnit );
                        targetIsRoboGun = gun->IsRoboGun();
                    }

                    if ( cellUnit->_bact_type != BACT_TYPES_GUN || cellUnit->_shield > 100 || !targetIsRoboGun )
                    {
                        if ( (_oflags & BACT_OFLAG_USERINPT || cellUnit->_owner != _owner) && (!isRoboGun || cellUnit != _host_station) )
                        {

                            World::TRoboCollSet *collNodes = cellUnit->getBACT_collNodes();

                            int collCount;
                            if ( collNodes )
                                collCount = collNodes->roboColls.size();
                            else
                                collCount = 1;

                            int damaged = 0;

                            for (int j = collCount - 1; j >= 0; j-- )
                            {
                                vec3d spherePos;
                                float sphereRadius;

                                if ( collNodes )
                                {
                                    spherePos = cellUnit->_position + cellUnit->_rotation.Transpose().Transform( collNodes->roboColls[j].coll_pos );

                                    sphereRadius = collNodes->roboColls[j].robo_coll_radius;
                                }
                                else
                                {
                                    spherePos = cellUnit->_position;

                                    sphereRadius = cellUnit->_radius;
                                }

                                if ( !collNodes || sphereRadius >= 0.01 )
                                {
                                    hitRadius = sphereRadius;

                                    vec3d toSphere = spherePos - _old_pos;

                                    if ( toSphere.dot( _rotation.AxisZ() ) >= 0.3 )
                                    {
                                        vec3d perpVec = arg->dir * toSphere;

                                        float dist = toSphere.length();
                                        float perpDist = perpVec.length();

                                        float hitDist = sphereRadius + _gun_radius;

                                        if ( hitDist > perpDist )
                                        {
                                            if ( sqrt( POW2(perpDist) + 1000000.0 ) > dist )
                                            {
                                                if ( !damaged )
                                                {
                                                    int energ;

                                                    if ( cellUnit->getBACT_inputting() || cellUnit->getBACT_viewer() )
                                                    {
                                                        float damage = (_gun_power * arg->period) * (100.0 - (float)cellUnit->_shield);
                                                        energ = (damage * 0.004);
                                                    }
                                                    else
                                                    {

                                                        float damage = (_gun_power * arg->period) * (100.0 - (float)cellUnit->_shield);
                                                        energ = damage / 100;
                                                    }

                                                    TEnergyChange energyMsg;
                                                    energyMsg.unit = this;
                                                    energyMsg.energy = -energ;

                                                    if ( energ )
                                                        cellUnit->ModifyEnergy(&energyMsg);
                                                }

                                                damaged = 1;

                                                if ( !v108 || nearestDist > dist )
                                                {
                                                    v108 = cellUnit;
                                                    nearestDist = dist;

                                                    nearestPos = cellUnit->_position;
                                                }
                                            }
                                        }
                                    }

                                }

                            }


                        }
                    }
                }
            }
        }
    }

    int userControlled = getBACT_inputting();

    if ( (userControlled || _world->IsNearViewer(this)) && !isNetGame )
    {
        int shotInterval;

        if ( userControlled )
        {
            int shotTime = _world->GetWeaponsProtos().at(_mgun).shot_time_user;
            float frameMs = arg->period * 1000.0;

            if ( shotTime <= frameMs )
                shotInterval = frameMs;
            else
                shotInterval = shotTime;
        }
        else
        {
            int shotTime = _world->GetWeaponsProtos().at(_mgun).shot_time;
            float frameMs = arg->period * 1000.0;

            if ( shotTime <= frameMs )
                shotInterval = frameMs;
            else
                shotInterval = shotTime;
        }

        if ( arg->gTime - _mgun_time > shotInterval )
        {
            _mgun_time = arg->gTime;

            int hasImpact = 0;
            int hitWorld = 0;

            TRayIntersection ray;

            vec3d impactPos;

            if ( v108 )
            {
                hasImpact = 1;
                hitWorld = 0;

                if (isnormal(nearestDist)) // Not NULL, NAN, INF
                    impactPos = nearestPos - (nearestPos - _position) * (hitRadius * 0.7) / nearestDist;
                else
                    impactPos = nearestPos;
            }
            else
            {
                ray.stPos = _position;
                ray.vect = arg->dir * 1000.0;
                ray.flags = 0;

                _world->IntersectLongRay(&ray);

                if ( ray.isect )
                {
                    impactPos = ray.isectPos;

                    hitWorld = 1;
                    hasImpact = 1;
                }
                else
                {
                    hasImpact = 0;
                }
            }

            if ( hasImpact )
            {
                TUnitSpawn spawn;
                spawn.pos = impactPos;
                spawn.vehicle_id = _mgun;

                NC_STACK_ypamissile *gunFireBact = _world->CreateWeapon(&spawn);

                if ( gunFireBact )
                {
                    gunFireBact->_owner = _owner;

                    gunFireBact->_kidRef.Detach();
                    gunFireBact->_parent = NULL;

                    _missiles_list.push_back(gunFireBact);

                    setState_msg stateMsg;
                    stateMsg.newStatus = BACT_STATUS_DEAD;
                    stateMsg.setFlags = 0;
                    stateMsg.unsetFlags = 0;

                    gunFireBact->SetStateInternal(&stateMsg);

                    if ( hitWorld )
                    {
                        stateMsg.setFlags = BACT_STFLAG_DEATH2;
                        stateMsg.newStatus = BACT_STATUS_NOPE;
                        stateMsg.unsetFlags = 0;
                        gunFireBact->SetStateInternal(&stateMsg);

                        gunFireBact->AlignMissileByNormal( ray.skel->polygons[ ray.polyID ].Normal() );
                    }
                }
            }
        }
    }
    return 1;
}


void NC_STACK_ypabact::UpdateVisor(NC_STACK_ypabact *bact2, int noWeaponSight)
{
    THudVisor hudi;

    float sightX;
    float sightY;

    if ( bact2 )
    {
        vec3d toTarget = bact2->_position - _position;

        mat3x3 corrected = _rotation;
        GFX::Engine.matrixAspectCorrection(corrected, false);

        vec3d viewPos = corrected.Transform(toTarget);

        if ( viewPos.z != 0.0 )
        {
            sightX = viewPos.x / viewPos.z;
            sightY = viewPos.y / viewPos.z;
        }
        else
        {
            sightY = 0.0;
            sightX = 0.0;
        }

        hudi.target = bact2;
    }
    else
    {
        sightX = -_gun_leftright;
        sightY = -_gun_angle_user;

        hudi.target = NULL;
    }

    if ( _mgun == -1 )
    {
        hudi.mgunSight = 0;
    }
    else
    {
        hudi.mgunSight = 1;
        hudi.mgunX = -_gun_leftright;
        hudi.mgunY = -_gun_angle_user;
    }

    if ( _weapon == -1 || noWeaponSight )
    {
        hudi.weaponSight = 0;
    }
    else
    {
        if ( _weapon_flags & 4 )
        {
            hudi.weaponSight = 4;
            hudi.weaponX = sightX;
            hudi.weaponY = sightY;
        }
        else
        {
            if ( (_weapon_flags & 4) || !(_weapon_flags & 2) )
                hudi.weaponSight = 2;
            else
                hudi.weaponSight = 3;

            hudi.weaponX = -_gun_leftright;
            hudi.weaponY = -_gun_angle_user;
        }
    }

    _world->SetHudVisor(&hudi);
}

size_t NC_STACK_ypabact::UserTargeting(TAimQuery *arg)
{
    NC_STACK_ypabact *targeto = 0;
    float bestAimDist = 0.0;

    float weaponRadius;

    if ( _weapon == -1 )
        weaponRadius = 0.0;
    else
        weaponRadius = _world->GetWeaponsProtos().at(_weapon).radius;

    int noWeaponSight = !(_weapon_flags & 2) && !(_weapon_flags & 0x10);

    if ( _weapon != -1 && !noWeaponSight )
    {
        TSectorInfo sectorInfo;
        sectorInfo.pos_x = _position.x;
        sectorInfo.pos_z = _position.z;

        _world->GetSectorInfo(&sectorInfo);

        vec2d tmp = _rotation.AxisZ().XZ() * World::CVSectorLength + _position.XZ();

        cellArea *pCells[3];

        pCells[0] = sectorInfo.pcell;

        sectorInfo.pos_x = tmp.x;
        sectorInfo.pos_z = tmp.y;

        _world->GetSectorInfo(&sectorInfo);

        pCells[2] = sectorInfo.pcell;

        if ( sectorInfo.pcell == pCells[0] )
        {
            pCells[1] = pCells[0];
        }
        else
        {
            vec2d tmp2 = _position.XZ() + (tmp - _position.XZ()) * 0.5;
            sectorInfo.pos_x = tmp2.x;
            sectorInfo.pos_z = tmp2.y;

            _world->GetSectorInfo(&sectorInfo);

            pCells[1] = sectorInfo.pcell;
        }

        for (int i = 0; i < 3; i++)
        {
            if ( i <= 0 || pCells[i] != pCells[i - 1] )
            {
                if ( pCells[i] )
                {
                    for ( NC_STACK_ypabact* &bct : pCells[i]->unitsList )
                    {
                        if ( bct != this )
                        {
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_status != BACT_STATUS_DEAD )
                            {
                                int isRoboGun = 0;
                                if (bct->_bact_type == BACT_TYPES_GUN)
                                {
                                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                                    isRoboGun = gun->IsRoboGun();
                                }

                                if ( !isRoboGun )
                                {
                                    if ( arg->flags & 2 || bct->_owner != _owner )
                                    {
                                        if ( arg->flags & 1 || bct->_owner == _owner || !bct->_owner )
                                        {
                                            if ( arg->flags & 4 || bct->_owner )
                                            {
                                                vec3d mv = bct->_position - _old_pos;

                                                if ( mv.dot( _rotation.AxisZ() ) >= 0.0 )
                                                {
                                                    float mv_len = mv.length();

                                                    vec3d mvd = arg->aim_dir * mv;

                                                    float aimRadius = mv_len * 1000.0 * 0.0005 + 20.0;
                                                    float mvd_len = mvd.length();

                                                    if ( ((mvd_len < aimRadius && (_weapon_flags & 4)) || (bct->_radius + weaponRadius > mvd_len && !(_weapon_flags & 4)) )
                                                            && mv_len < 2000.0
                                                            && (bestAimDist > mvd_len || !targeto) )
                                                    {
                                                        targeto = bct;
                                                        bestAimDist = mvd_len;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }
    }

    if ( targeto )
    {
        UpdateVisor(targeto, noWeaponSight);

        setTarget_msg targetMsg;
        targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
        targetMsg.priority = 1;
        targetMsg.tgt.pbact = targeto;

        SetTarget(&targetMsg);

        arg->ret_bact = targeto;
        return 1;
    }

    UpdateVisor(NULL, noWeaponSight);
    arg->ret_bact = NULL;

    return 0;
}

void NC_STACK_ypabact::HandleVisChildrens(int *arg)
{
    NC_STACK_base *vps[] {
    _vp_normal,
    _vp_dead,
    _vp_fire,
    _vp_genesis,
    _vp_wait,
    _vp_megadeth};

    for ( NC_STACK_base *vp : vps )
    {
        for( NC_STACK_base *kd : vp->GetKidList())
        {
            if ( *arg == 1 )
            {
                kd->SetParentFollow(true);

                kd->SetPosition( kd->GetPos() - _position );
            }
            else if ( *arg == 2 )
            {
                kd->SetParentFollow(true);

                kd->SetPosition( kd->GetPos() + _position );
            }
        }
    }
}

bool NC_STACK_ypabact::GetFightMotivation(float *arg)
{
    if ( _aggr == 100 )
        return true;

    if ( _aggr == 0 )
        return false;

    TSummaryQuery summary;
    summary.enrg_sum = 0;
    summary.enrg_type = 1;

    GetSummary(&summary);

    float forceRatio = summary.enrg_sum;

    summary.enrg_sum = 0;
    summary.enrg_type = 4;

    GetSummary(&summary);

    if (summary.enrg_sum == 0) // Possible devision by zero
        summary.enrg_sum = 1;

    forceRatio = forceRatio / (float)summary.enrg_sum;

    if ( arg )
        *arg = forceRatio;

    if ( (_status_flg & BACT_STFLAG_ESCAPE) && forceRatio > 0.5 )
    {
        return true;
    }
    else if ( forceRatio > 0.2 )
    {
        return true;
    }
    return false;
}

NC_STACK_ypabact *FindKidNearestTarget(NC_STACK_ypabact *bact)
{
    vec3d targetPos;

    if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
        targetPos = bact->_primTpos;
    else if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
        targetPos = bact->_primT.pbact->_position;
    else
        return NULL;

    float bestDist = 215040.0;

    NC_STACK_ypabact *new_leader = NULL;

    for ( NC_STACK_ypabact* &kid_unit : bact->_kidList )
    {
        if ( kid_unit->_status != BACT_STATUS_DEAD )
        {
            int userControlled = kid_unit->getBACT_inputting();

            if ( !userControlled )
            {

                float dist = (targetPos.XZ() - kid_unit->_position.XZ()).length();

                if ( !new_leader || (kid_unit->_bact_type != BACT_TYPES_UFO && bestDist > dist) || (new_leader->_bact_type == BACT_TYPES_UFO && (kid_unit->_bact_type != BACT_TYPES_UFO || bestDist > dist )) )
                {
                    new_leader = kid_unit;
                    bestDist = dist;
                }
            }
        }
    }
    return new_leader;
}

NC_STACK_ypabact *FindHealthiestNearbyKid(NC_STACK_ypabact *bact)
{
    float tmp = 0.0;
    NC_STACK_ypabact *new_leader = NULL;

    for ( NC_STACK_ypabact* &kid_unit : bact->_kidList )
    {
        if ( kid_unit->_status != BACT_STATUS_DEAD )
        {
            float score;
            if ( kid_unit->_bact_type == BACT_TYPES_UFO )
            {
                score = 0.0;
            }
            else
            {
                float closeness = 1.0 - ( (bact->_position.XZ() - kid_unit->_position.XZ()).length() / 110400.0);
                score = (float)kid_unit->_energy / (float)kid_unit->_energy_max + closeness;
            }

            if ( !new_leader || tmp < score )
            {
                new_leader = kid_unit;
                tmp = score;
            }
        }
    }

    return new_leader;
}

NC_STACK_ypabact *PassGroupToNewLeader(NC_STACK_ypabact *bact, int nearestToTarget)
{
    if ( !bact->_kidList.empty() )
    {
        NC_STACK_ypabact *new_leader = NULL;

        if (nearestToTarget)
            new_leader = FindKidNearestTarget(bact);
        else
            new_leader = FindHealthiestNearbyKid(bact);

        if (!new_leader)
            return NULL;

        if (new_leader->_bact_type != BACT_TYPES_UFO || bact->_bact_type == BACT_TYPES_UFO)
        {
            bact->_host_station->AddSubject(new_leader);

            new_leader->CopyTargetOf(bact);

            for ( World::RefBactList::iterator it = bact->_kidList.begin(); it != bact->_kidList.end(); )
            {
                NC_STACK_ypabact *kid_unit = *it;
                it++;

                new_leader->AddSubject(kid_unit);

                kid_unit->CopyTargetOf(new_leader);
            }
            new_leader->_commandID = bact->_commandID;
            return new_leader;
        }

    }
    return NULL;
}

void NC_STACK_ypabact::NetBroadcastReorder(NC_STACK_ypabact *bact2, int mode)
{
    if ( _world->_isNetGame )
    {
        static uamessage_reorder ordMsg;

        ordMsg.comm = bact2->_gid;
        ordMsg.num = 0;
        ordMsg.commID = bact2->_commandID;

        for ( NC_STACK_ypabact* &bct : bact2->_kidList )
        {
            if ( ordMsg.num < 500 )
            {
                ordMsg.units[ordMsg.num] = bct->_gid;
                ordMsg.num++;
            }
        }

        ordMsg.owner = _owner;
        ordMsg.sz = (char *)&ordMsg.units[ordMsg.num] - (char *)&ordMsg;
        ordMsg.mode = mode;
        ordMsg.msgID = UAMSG_REORDER;

        _world->NetBroadcastMessage(&ordMsg, ordMsg.sz, true);
    }
}

void NC_STACK_ypabact::ReorganizeGroup(TReorgOrder *arg)
{
    switch ( arg->mode )
    {
    case 1:
        if ( arg->unit )
        {
            if ( arg->unit->_status == BACT_STATUS_DEAD )
            {
                ypa_log_out("ORG_NEWCHIEF: Dead master\n");
            }
            else if ( arg->unit != _parent && arg->unit != this )
            {
                _commandID = arg->unit->_commandID;
                _aggr = arg->unit->_aggr;

                arg->unit->AddSubject(this);

                while ( !_kidList.empty() )
                {
                    NC_STACK_ypabact *kid = _kidList.front();

                    kid->_aggr = arg->unit->_aggr;
                    kid->_commandID = arg->unit->_commandID;

                    arg->unit->AddSubject(kid);
                    
                    kid->CopyTargetOf(arg->unit);
                }

                CopyTargetOf(arg->unit);
                NetBroadcastReorder(arg->unit, 1);
            }
        }
        break;

    case 2:
        if ( _host_station != _parent || arg->unit != this )
        {
            if ( _status == BACT_STATUS_DEAD )
            {
                ypa_log_out("ORG_BECOMECHIEF dead vehicle\n");
            }
            else
            {
                if ( _host_station != _parent && _host_station )
                    _host_station->AddSubject(this);

                if ( arg->unit )
                {
                    CopyTargetOf(arg->unit);

                    _aggr = arg->unit->_aggr;
                    _commandID = arg->unit->_commandID;

                    AddSubject(arg->unit);

                    while ( !arg->unit->_kidList.empty() )
                    {
                        NC_STACK_ypabact *kid = arg->unit->_kidList.front();
                        
                        AddSubject(kid);
                        kid->_aggr = arg->unit->_aggr;

                        kid->CopyTargetOf(this);
                    }

                    _commandID = arg->unit->_commandID;
                    NetBroadcastReorder(this, 2);
                }
                else
                {
                    if ( _host_station != _parent && _host_station )
                    {
                        int nextCommandId = _host_station->getROBO_commCount();

                        _commandID = nextCommandId;

                        _host_station->setROBO_commCount(nextCommandId + 1);
                    }
                    NetBroadcastReorder(this, 2);
                }
            }
        }
        break;

    case 3:
        if ( _status == BACT_STATUS_DEAD )
        {
            ypa_log_out("ORG_NEWCOMMAND: dead vehicle\n");
        }
        else if (_host_station)
        {

            if ( _host_station == _parent )
            {
                NC_STACK_ypabact *newLeader = PassGroupToNewLeader(this, 0);

                if ( newLeader )
                    NetBroadcastReorder(newLeader, 13);
            }
            else
            {
                _host_station->AddSubject(this);
            }

            int nextCommandId = _host_station->getROBO_commCount();
            _commandID = nextCommandId;

            if (_world->_isNetGame)
                _commandID |= _owner << 24;

            _host_station->setROBO_commCount(nextCommandId + 1);
            NetBroadcastReorder(this, 3);
        }
        break;

    case 4:
        if ( arg->unit->IsParentMyRobo() )
        {
            NC_STACK_ypabact *newLeader = PassGroupToNewLeader(arg->unit, 0);

            if ( newLeader )
                NetBroadcastReorder(newLeader, 14);
        }

        AddSubject(arg->unit);

        arg->unit->_commandID = _commandID;

        arg->unit->CopyTargetOf(this);
        NetBroadcastReorder(this, 4);
        break;

    case 6:
    {
        int userControlled = getBACT_inputting();

        if ( !userControlled )
        {
            NC_STACK_ypabact *newLeader = PassGroupToNewLeader(this, 1);

            if ( newLeader )
            {
                newLeader->AddSubject(this);
                newLeader->_commandID = _commandID;

                NetBroadcastReorder(newLeader, 6);
            }
        }
    }
    break;

    default:
        break;
    }
}

void NC_STACK_ypabact::DoTargetWaypoint()
{
    if ( ( _position.XZ() - _primTpos.XZ() ).length() >= 300.0 )
        return;

    if ( !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
    {
        _current_waypoint++;

        setTarget_msg targetMsg;

        if ( _waypoints_count > 1 )
        {
            targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
            targetMsg.priority = 0;
            targetMsg.tgt_pos = _waypoints[ _current_waypoint ];

            SetTarget(&targetMsg);
        }

        if ( _current_waypoint >= _waypoints_count - 1 )
        {
            if ( _m_cmdID )
            {
                NC_STACK_ypabact *targetUnit = _world->FindBactByCmdOwn(_m_cmdID, _m_owner);

                if ( targetUnit )
                {
                    if ( targetUnit->_pSector->IsCanSee(_owner) )
                    {
                        targetMsg.tgt.pbact = targetUnit;
                        targetMsg.tgt_type = BACT_TGT_TYPE_UNIT;
                        targetMsg.priority = 0;

                        SetTarget(&targetMsg);
                    }
                }
            }

            _m_owner = 0;
            _m_cmdID = 0;
            _status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
        }
    }
    else
    {

        _current_waypoint++;

        int waypointIdx = _current_waypoint;

        if ( _current_waypoint >= _waypoints_count )
        {
            _current_waypoint = 0;
            waypointIdx = 0;
        }

        setTarget_msg targetMsg;

        targetMsg.tgt_type = BACT_TGT_TYPE_CELL_IND;
        targetMsg.priority = 0;
        targetMsg.tgt_pos = _waypoints[ waypointIdx ];

        SetTarget(&targetMsg);
    }
}

size_t NC_STACK_ypabact::TargetAssess(TTargetAssess *arg)
{
    bool primTgtDone = false;
    bool primTgtNear = false;

    if ( arg->tgType == BACT_TGT_TYPE_FRMT 
         && 
        (_primTtype == BACT_TGT_TYPE_FRMT || _secndTtype == BACT_TGT_TYPE_FRMT) )
        return TA_MOVE;

    if ( arg->tgType == BACT_TGT_TYPE_NONE)
        return TA_IGNORE;

    if ( _primTtype == BACT_TGT_TYPE_CELL )
    {
        if ( (_position.XZ() - _primTpos.XZ()).length() < 1800.0 )
            primTgtNear = true;

        if ( _owner == _primT.pcell->owner )
            primTgtDone = true;
    }

    if ( _primTtype == BACT_TGT_TYPE_UNIT )
    {
        if ( (_position.XZ() - _primT.pbact->_position.XZ()).length() < 1800.0 )
            primTgtNear = true;

        if ( _owner == _primT.pbact->_owner )
            primTgtDone = true;
    }

    if ( arg->tgType == BACT_TGT_TYPE_UNIT )
    {
        NC_STACK_ypabact *enemy = NULL;
        bool isSecTgt = false;
        int aggr = 0;

        if ( arg->priority == 1 )
        {
            enemy = _secndT.pbact;
            isSecTgt = true;
            aggr = 50;
        }
        else if ( arg->priority == 0)
        {
            enemy = _primT.pbact;
            isSecTgt = false;
            aggr = 25;
        }

        if ( enemy )
        {
            float enemyDistance = (enemy->_position.XZ() - _position.XZ()).length();

            if ( !enemy->_pSector->IsCanSee(_owner) )
                return TA_CANCEL;

            if ( _aggr >= 100 )
            {
                if ( isSecTgt && enemyDistance > 2160.0 )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( enemy->_owner == 0 || enemy->_owner == _owner )
            {
                if ( enemyDistance < 300.0 )
                    return TA_IGNORE;

                return TA_MOVE;
            }

            if ( _status_flg & BACT_STFLAG_ESCAPE )
            {
                if ( !primTgtNear )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( _aggr < aggr )
            {
                if ( primTgtNear && primTgtDone )
                    return TA_FIGHT;

                return TA_CANCEL;
            }
            
            if ( !isSecTgt || _bact_type == BACT_TYPES_GUN )
                return TA_FIGHT;

            if ( enemyDistance > 2160.0 )
                return TA_CANCEL;

            vec3d tgtPos;

            if ( IsParentMyRobo() )
            {

                if ( _primTtype == BACT_TGT_TYPE_CELL )
                    tgtPos = _primTpos;
                else if ( _primTtype == BACT_TGT_TYPE_UNIT )
                    tgtPos = _primT.pbact->_position;
                else
                    tgtPos = _position;
            }
            else if ( _parent )
            {
                if ( _parent->_primTtype == BACT_TGT_TYPE_CELL )
                    tgtPos = _parent->_primTpos;
                else if ( _parent->_primTtype == BACT_TGT_TYPE_UNIT )
                    tgtPos = _parent->_primT.pbact->_position;
                else
                    tgtPos = _position;
            }

            if ( (tgtPos.XZ() - _position.XZ()).length() > 3600.0 )
            {
                int allyAttackers = 0;

                for( const TBactAttacker &ainf : _secndT.pbact->_attackersList )
                {
                    if ( ainf.attacker->_secndTtype == BACT_TGT_TYPE_UNIT &&
                         ainf.attacker->_secndT.pbact == _secndT.pbact &&
                         ainf.attacker->_owner == _owner )
                        allyAttackers++;

                    if ( allyAttackers > 2 )
                        break;
                }

                if ( allyAttackers > 2 )
                    return TA_CANCEL;
            }
            return TA_FIGHT;
        }
    }
    else if ( arg->tgType == BACT_TGT_TYPE_CELL )
    {
        cellArea *pCell = NULL;
        vec2d cellPos;
        bool isSecTgt = false;
        int aggr = 0;

        if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            pCell = _secndT.pcell;
            cellPos = _sencdTpos.XZ();

            aggr = 75;
            isSecTgt = true;
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            pCell = _primT.pcell;
            cellPos = _primTpos.XZ();

            aggr = 25;
            isSecTgt = false;
        }

        if ( (_status_flg & BACT_STFLAG_WAYPOINT) && !isSecTgt )
        {
            DoTargetWaypoint();
            return TA_MOVE;
        }

        if ( !pCell )
            return TA_IGNORE;

        int cellEnergy = pCell->GetEnergy();

        float cellDistance = (_position.XZ() - cellPos).length();

        if ( _aggr >= 100 )
        {
            if ( cellEnergy <= 0 && pCell->owner == _owner )
            {
                if ( cellDistance < 300.0 )
                    return TA_IGNORE;

                return TA_MOVE;
            }

            return TA_FIGHT;
        }

        if ( cellDistance >= 300.0 )
        {
            if ( _owner != pCell->owner )
            {
                if ( (_status_flg & BACT_STFLAG_ESCAPE) || _aggr < aggr )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( isSecTgt )
                return TA_CANCEL;

            return TA_MOVE;
        }

        if ( _owner == pCell->owner )
        {
            if ( isSecTgt )
                return TA_CANCEL;

            return TA_IGNORE;
        }

        if ( (_status_flg & BACT_STFLAG_ESCAPE) || _aggr < aggr )
            return TA_CANCEL;

        return TA_FIGHT;
    }

    return TA_IGNORE;
}

void NC_STACK_ypabact::BeamingTimeUpdate(update_msg *arg)
{
    float growFraction = 0.66;

    if ( _scale_delay <= 0 )
    {
        if ( _scale_time >= 1980.0 )
        {
            if ( _scale_time >= 3000 )
            {
                _world->BeamUnitThroughGate(this);

                _status_flg |= BACT_STFLAG_CLEAN;

                Die();

                if ( _oflags & BACT_OFLAG_USERINPT )
                    _status_flg |= BACT_STFLAG_NORENDER;
                else
                    Release();

                _status_flg &= ~BACT_STFLAG_SCALE;
            }
            else
            {
                _status_flg |= BACT_STFLAG_SCALE;

                _scale = vec3d(1.0, 30.0, 1.0) - vec3d::OY( (_scale_time - 1980.0) * 30.0 / 1020.0 );
            }
        }
        else
        {
            if ( GetVP() != _vp_genesis )
            {
                setState_msg stateMsg;
                stateMsg.newStatus = BACT_STATUS_BEAM;
                stateMsg.setFlags = 0;
                stateMsg.unsetFlags = 0;

                SetState(&stateMsg);
            }

            _status_flg |= BACT_STFLAG_SCALE;

            _scale = vec3d(1.0, 0.0, 1.0) + vec3d::OY( (30 * _scale_time)/ (growFraction * 3000.0) );
        }

        _scale_time += arg->frameTime;
    }
    else
    {
        _scale_delay -= arg->frameTime;
    }
}

void NC_STACK_ypabact::StartDestFX(const World::DestFX &fx)
{
    TUnitSpawn spawn;

    spawn.pos = _position;
    spawn.vehicle_id = fx.ModelID;

    if ( _radius > 31.0 )    // 31.0
    {
        float len = fx.Pos.length();

        if ( len > 0.1 )
        {
            vec3d pos = fx.Pos / len * _radius;

            spawn.pos += _rotation.Transform(pos);
        }
    }

    NC_STACK_ypabact *bah = _world->CreateVehicle(&spawn);

    if ( bah )
    {
        _world->AddToUnitsList(bah);

        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_DEAD;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;

        bah->SetStateInternal(&stateMsg);

        bah->_fly_dir = _rotation.Transform(fx.Pos);

        if ( fx.Accel )
            bah->_fly_dir += _fly_dir * _fly_dir_length;

        float len = bah->_fly_dir.length();

        if ( len > 0.001 )
        {
            bah->_fly_dir /= len;
            bah->_fly_dir_length = len;
        }

    }
}

void NC_STACK_ypabact::StartDestFXByType(uint8_t type)
{
    if ( _world->IsNearViewer(this) )
    {
        size_t fxCount = _world->getYW_destroyFX();

        if (fxCount > _destroyFX.size())
            fxCount = _destroyFX.size();

        for (size_t i = 0; i < fxCount; i++)
        {
            if ( _destroyFX[i].ModelID )
            {
                const World::DestFX &fx = _destroyFX[i];

                if ( fx.Type == type )
                    StartDestFX(fx);
            }
        }
        
        for (const World::DestFX &x : _extDestroyFX)
        {
            if (x.ModelID != 0 && x.Type == type)
                StartDestFX(x);
        }
    }
}

void NC_STACK_ypabact::CorrectPositionOnLand()
{
    float radius;
    if ( _viewer_radius >= 32.0 )
        radius = _viewer_radius;
    else
        radius = 32.0;

    TSphereContact coltmp[10];

    TSphereCollisionQuery sphereQuery;
    sphereQuery.pos = _position;
    sphereQuery.dir = _rotation.AxisX();
    sphereQuery.coll_max = 10;
    sphereQuery.radius = radius;
    sphereQuery.flags = 0;
    sphereQuery.collisions = coltmp;

    _world->FindSphereCollisions(&sphereQuery);

    vec3d tmp(0.0, 0.0, 0.0);

    float trad = 0.0;

    for (int i = sphereQuery.coll_count - 1; i >= 0; i-- )
    {
        TSphereContact *clsn = &sphereQuery.collisions[i];

        if ( clsn->normal.y < 0.6 )
        {
            vec3d tmp2 = _position - clsn->pos1;

            tmp += clsn->normal;

            float penetration = radius - tmp2.length();

            if ( trad == 0.0 || trad < penetration )
                trad = penetration;
        }
    }

    if ( _viewer_radius >= 32.0 )
        radius = _viewer_radius;
    else
        radius = 32.0;

    sphereQuery.pos = _position;
    sphereQuery.dir = -_rotation.AxisX();
    sphereQuery.coll_max = 10;
    sphereQuery.radius = radius;
    sphereQuery.flags = 0;
    sphereQuery.collisions = coltmp;

    _world->FindSphereCollisions(&sphereQuery);

    for (int i = sphereQuery.coll_count - 1; i >= 0; i-- )
    {
        TSphereContact *clsn = &sphereQuery.collisions[i];

        if ( clsn->normal.y < 0.6 )
        {
            vec3d tmp2 = _position - clsn->pos1;

            tmp += clsn->normal;

            float penetration = radius - tmp2.length();

            if ( trad == 0.0 || trad < penetration )
                trad = penetration;
        }
    }

    float normalLen = tmp.length();

    if ( normalLen > 0.0001 )
        tmp /= normalLen;

    _position -= tmp * trad;
}


void NC_STACK_ypabact::CorrectPositionInLevelBox(void *)
{
    int clamped = 0;
    
    constexpr float CurSectrLen = World::CVSectorLength + 10.0;

    if ( _position.x > _wrldSize.x - CurSectrLen )
    {
        clamped = 1;
        _position.x = _wrldSize.x - CurSectrLen;
    }

    if ( _position.x < CurSectrLen )
    {
        clamped = 1;
        _position.x = CurSectrLen;
    }

    if ( _position.z > -CurSectrLen )
    {
        clamped = 1;
        _position.z = -CurSectrLen;
    }

    if ( _position.z < _wrldSize.y + CurSectrLen )
    {
        clamped = 1;
        _position.z = _wrldSize.y + CurSectrLen;
    }

    if ( _oflags & BACT_OFLAG_VIEWER )
    {
        if ( clamped )
        {
            if ( _bact_type != BACT_TYPES_TANK && _bact_type != BACT_TYPES_CAR )
            {
                TRayIntersection ray;

                ray.stPos = _position - vec3d::OY(100.0);

                ray.vect = vec3d::OY(_viewer_overeof + 100.0);
                ray.flags = 0;

                _world->IntersectRay(&ray);

                if ( ray.isect )
                    _position.y = ray.isectPos.y - _viewer_overeof;
            }
        }
    }
}

void ypabact_NetUpdate_VPHACKS(NC_STACK_ypabact *bact, update_msg *upd)
{
    if ( bact->_vp_extra_mode == 1 )
    {
        int engy = bact->_energy_max * 0.7;

        if ( engy < 10000 )
            engy = 10000;

        if ( engy > 25000 )
            engy = 25000;

        ShrinkAndSpinPlasma(bact, engy, upd->frameTime, 0.75);
        bact->_scale_time -= upd->frameTime;

        if ( bact->_scale_time < 0 )
            bact->_vp_extra[0].SetVP((NC_STACK_base::Instance *)NULL);
    }

    if ( bact->_vp_extra_mode == 2 )
    {
        NC_STACK_yparobo *roboo = dynamic_cast<NC_STACK_yparobo *>(bact);

        if (roboo)
        {
            roboo->_roboBeamTimePre -= upd->frameTime;
            if ( roboo->_roboBeamTimePre <= 0 )
            {
                roboo->_roboBeamTimePre = 0;
                SFXEngine::SFXe.startSound(&bact->_soundcarrier, 10);

                roboo->_roboState &= ~NC_STACK_yparobo::ROBOSTATE_MOVE;
                bact->_vp_extra[0].flags = 0;
                bact->_vp_extra[1].flags = 0;
            }
            else
            {
                if ( roboo->_roboBeamFXTime <= 0 )
                {
                    if ( bact->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
                    {
                        roboo->_roboBeamFXTime = roboo->_roboBeamTimePre / 10;
                        bact->_vp_extra[0].flags &= ~EVPROTO_FLAG_ACTIVE;
                    }
                    else
                    {
                        roboo->_roboBeamFXTime = (1500 - roboo->_roboBeamTimePre) / 10;
                        bact->_vp_extra[0].pos = bact->_position;
                        bact->_vp_extra[0].rotate = bact->_rotation;;
                        bact->_vp_extra[0].flags = 3;
                        bact->_vp_extra[0].scale = 1.25;
                        bact->_vp_extra[0].SetVP(bact->_vp_genesis);
                    }

                    if ( roboo->_vp_extra[1].flags & EVPROTO_FLAG_ACTIVE )
                    {
                        roboo->_roboBeamFXTime = roboo->_roboBeamTimePre / 10;
                        bact->_vp_extra[1].flags &= ~EVPROTO_FLAG_ACTIVE;
                    }
                    else
                    {
                        roboo->_roboBeamFXTime = (1500 - roboo->_roboBeamTimePre) / 10;
                        bact->_vp_extra[1].pos = roboo->_roboBeamPos;
                        bact->_vp_extra[1].rotate = bact->_rotation;
                        bact->_vp_extra[1].flags = 1;
                        bact->_vp_extra[1].SetVP(bact->_vp_genesis);
                    }
                }
                roboo->_roboBeamFXTime -= upd->frameTime;
            }

        }
    }
}

void NC_STACK_ypabact::NetUpdate(update_msg *upd)
{
    ypabact_NetUpdate_VPHACKS(this, upd);

    TSectorInfo sectorInfo;
    sectorInfo.pos_x = _position.x;
    sectorInfo.pos_z = _position.z;
    if ( !_world->GetSectorInfo(&sectorInfo) )
    {
        FixBeyondTheWorld();

        sectorInfo.pos_x = _position.x;
        sectorInfo.pos_z = _position.z;
        _world->GetSectorInfo(&sectorInfo);
    }

    cellArea *oldSect = _pSector;

    _cellId = sectorInfo.CellId;
    _pSector = sectorInfo.pcell;

    if ( oldSect != sectorInfo.pcell )
    {
        _cellRef.Detach();
        _cellRef = _pSector->unitsList.push_back(this);
    }

    _clock += upd->frameTime;

    NetPredictMotion(upd);

    for ( NC_STACK_ypamissile* misl : Utils::IterateListCopy<NC_STACK_ypamissile *>(_missiles_list) )
    {
        misl->SetLauncherBact(this);
        misl->Update(upd);
    }

    ReleaseExpiredMissiles(this);

    _tForm.Pos = _position;

    if ( _status_flg & BACT_STFLAG_SCALE )
        _tForm.SclRot = _rotation.Transpose() * mat3x3::Scale(_scale);
    else
        _tForm.SclRot = _rotation.Transpose();

    int units_cnt = upd->units_count;

    upd->units_count = 0;

    for (NC_STACK_ypabact* bct : _kidList.safe_iter())
    {
        bct->NetUpdate(upd);
        upd->units_count++;
    }

    upd->units_count = units_cnt;

    _soundcarrier.Position = _position;
    _soundcarrier.Vector = _fly_dir * _fly_dir_length;

    SFXEngine::SFXe.UpdateSoundCarrier(&_soundcarrier);
}

void NC_STACK_ypabact::NetPredictMotion(update_msg *upd)
{
    if (_world->_netInterpolate)
        NetInterpolateMotion(upd);
    else
        NetExtrapolateMotion(upd);
}

void NC_STACK_ypabact::Release()
{
    if ( _owner )
    {
        if ( _world->_isNetGame )
        {
            if ( _bact_type != BACT_TYPES_MISSLE )
            {
                uamessage_destroyVhcl destrMsg;

                destrMsg.msgID = UAMSG_DESTROYVHCL;
                destrMsg.owner = _owner;
                destrMsg.id = _gid;
                destrMsg.type = _bact_type;

                _world->NetBroadcastMessage(&destrMsg, sizeof(destrMsg), true);
            }
        }
    }

    _world->MoveToDeadCache(this);
}

size_t NC_STACK_ypabact::SetStateInternal(setState_msg *arg)
{
    int result = 0;

    if ( arg->newStatus )
        _status = arg->newStatus;

    if ( arg->setFlags )
        _status_flg |= arg->setFlags;

    if ( arg->unsetFlags )
        _status_flg &= ~arg->unsetFlags;

    if ( arg->newStatus == BACT_STATUS_DEAD && (_vp_active != 2 && _vp_active != 3) )
    {
        _energy = -10000;

        SetVP(_vp_dead);

        _vp_active = 2;

        if ( _soundFlags & 2 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                TFFeedbackEffect ffEffect;
                ffEffect.effects_type = 4;

                _world->FFeedback_Play(&ffEffect);
            }

            SFXEngine::SFXe.stopSound(&_soundcarrier, 1);
            _soundFlags &= ~2;
        }

        if ( _oflags & BACT_OFLAG_USERINPT )
            SFXEngine::SFXe.stopSound(&_soundcarrier, 8);

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 0);
        }

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
        }

        SFXEngine::SFXe.startSound(&_soundcarrier, 7);

        _soundFlags |= 0x80;

        StartDestFXByType(World::DestFX::FX_DEATH);

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_NORMAL && 1 != _vp_active )
    {
        SetVP(_vp_normal);

        _vp_active = 1;

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 1) )
        {
            _soundFlags |= 1;
            SFXEngine::SFXe.startSound(&_soundcarrier, 0);
        }

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_BEAM && 5 != _vp_active )
    {
        _vp_active = 5;
        SetVP(_vp_genesis);

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 0x200) )
        {
            _soundFlags |= 0x200;
            SFXEngine::SFXe.startSound(&_soundcarrier, 9);
        }

        StartDestFXByType(World::DestFX::FX_BEAM);

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_IDLE && _vp_active != 6 )
    {
        SetVP(_vp_wait);
        _vp_active = 6;

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 0);
        }

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 3);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 4) )
        {
            _soundFlags |= 4;
            SFXEngine::SFXe.startSound(&_soundcarrier, 2);
        }

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_CREATE && 4 != _vp_active )
    {
        _vp_active = arg->newStatus;
        SetVP(_vp_genesis);

        if ( _soundFlags & 2 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                TFFeedbackEffect ffEffect;
                ffEffect.effects_type = 4;

                _world->FFeedback_Play(&ffEffect);
            }

            SFXEngine::SFXe.stopSound(&_soundcarrier, 1);
            _soundFlags &= ~2;
        }

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 0);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.stopSound(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 8) )
        {
            _soundFlags |= 8;
            SFXEngine::SFXe.startSound(&_soundcarrier, 3);
        }

        StartDestFXByType(World::DestFX::FX_CREATE);

        result = 1;
    }

    if ( arg->unsetFlags == BACT_STFLAG_FIRE && _vp_active == 7 )
    {
        if ( _oflags & BACT_OFLAG_USERINPT )
        {
            TFFeedbackEffect ffEffect;
            ffEffect.effects_type = 4;
            _world->FFeedback_Play(&ffEffect);
        }

        SetVP(_vp_normal);
        _vp_active = 1;

        SFXEngine::SFXe.stopSound(&_soundcarrier, 1);

        _soundFlags &= ~2;

        result = 1;
    }

    if ( arg->unsetFlags == BACT_STFLAG_DEATH2 && _vp_active == 3 )
    {
        _vp_active = 1;
        SetVP(_vp_normal);

        result = 1;
    }

    if ( arg->setFlags == BACT_STFLAG_FIRE && _vp_active != 7 )
    {
        _vp_active = 7;
        SetVP(_vp_fire);

        if ( !(_soundFlags & 2) )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                TFFeedbackEffect ffEffect;
                ffEffect.effects_type = 3;
                _world->FFeedback_Play(&ffEffect);
            }

            _soundFlags |= 2;
            SFXEngine::SFXe.startSound(&_soundcarrier, 1);
        }
        result = 1;
    }

    if ( arg->setFlags == BACT_STFLAG_DEATH2 )
    {
        _status = BACT_STATUS_DEAD;

        if ( _vp_active != 3 )
        {
            SetVP(_vp_megadeth);
            _vp_active = 3;

            if ( _soundFlags & 2 )
            {
                if ( _oflags & BACT_OFLAG_USERINPT )
                {
                    TFFeedbackEffect ffEffect;
                    ffEffect.effects_type = 4;
                    _world->FFeedback_Play(&ffEffect);
                }

                SFXEngine::SFXe.stopSound(&_soundcarrier, 1);
                _soundFlags &= ~2;
            }

            if ( _oflags & BACT_OFLAG_USERINPT )
                SFXEngine::SFXe.stopSound(&_soundcarrier, 8);

            if ( _soundFlags & 1 )
            {
                _soundFlags &= ~2;
                SFXEngine::SFXe.stopSound(&_soundcarrier, 0);
            }

            if ( _soundFlags & 8 )
            {
                _soundFlags &= ~8;
                SFXEngine::SFXe.stopSound(&_soundcarrier, 3);
            }

            if ( _soundFlags & 4 )
            {
                _soundFlags &= ~4;
                SFXEngine::SFXe.stopSound(&_soundcarrier, 2);
            }

            if ( _soundFlags & 0x80 )
            {
                _soundFlags &= ~0x80;
                SFXEngine::SFXe.stopSound(&_soundcarrier, 7);
            }

            SFXEngine::SFXe.startSound(&_soundcarrier, 4);

            StartDestFXByType(World::DestFX::FX_MEGADETH);

            _fly_dir_length = 0;

            result = 1;
        }
    }
    return result;
}

void NC_STACK_ypabact::ChangeSectorEnergy(TSectorDamage *arg)
{
    arg->OwnerID = World::OWNER_RECALC;

    _world->DamageSectorBuilding(arg);

    TSectorInfo sectorInfo;
    sectorInfo.pos_x = arg->pos.x;
    sectorInfo.pos_z = arg->pos.z;

    int sectorOwner;

    if ( _world->GetSectorInfo(&sectorInfo) )
        sectorOwner = sectorInfo.pcell->owner;
    else
        sectorOwner = 0;

    if ( _world->_isNetGame )
    {
        uamessage_sectorEnergy seMsg;
        seMsg.msgID = UAMSG_SECTORENERGY;
        seMsg.owner = _owner;
        seMsg.pos = arg->pos;
        seMsg.energy = arg->energy;
        seMsg.sectOwner = sectorOwner;

        if ( arg->unit )
            seMsg.whoHit = arg->unit->_gid;
        else
            seMsg.whoHit = 0;

        _world->NetBroadcastMessage(&seMsg, sizeof(seMsg), true);
    }
}

void ShrinkAndSpinPlasma(NC_STACK_ypabact *bact, int plasmaDuration, int frameTime, float maxScale)
{
    if (plasmaDuration == 0)
        plasmaDuration = 1;

    bact->_vp_extra[0].scale = sqrt( (float)bact->_scale_time / (float)plasmaDuration ) * maxScale;

    if ( bact->_vp_extra[0].scale < 0.0 )
        bact->_vp_extra[0].scale = 0;

    bact->_vp_extra[0].rotate = mat3x3::RotateY(bact->_maxrot * 2.0 * (float)frameTime * 0.001) * bact->_vp_extra[0].rotate;
}

void NC_STACK_ypabact::DeadTimeUpdate(update_msg *arg)
{
    if ( _status_flg & BACT_STFLAG_LAND || (_clock - _dead_time > 5000 && _status_flg & BACT_STFLAG_DEATH1 ) )
    {
        if ( !(_status_flg & BACT_STFLAG_DEATH2) )
        {
            setState_msg stateMsg;
            stateMsg.newStatus = BACT_STATUS_NOPE;
            stateMsg.unsetFlags = 0;
            stateMsg.setFlags = BACT_STFLAG_DEATH2;

            SetState(&stateMsg);
        }

        _status_flg |= BACT_STFLAG_LAND;

        if ( _owner && _bact_type != BACT_TYPES_MISSLE && _vp_genesis )
        {
            int plasmaDuration = _energy_max * 0.7;

            if ( plasmaDuration < 10000 )
                plasmaDuration = 10000;

            if ( plasmaDuration > 25000 )
                plasmaDuration = 25000;

            if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
            {
                _scale_time -= arg->frameTime;

                if ( _scale_time <= 0 )
                {
                    _vp_extra[0].SetVP((NC_STACK_base::Instance *)NULL);

                    if ( _yls_time <= 0 )
                    {

                        if ( _oflags & BACT_OFLAG_USERINPT )
                            _status_flg |= BACT_STFLAG_NORENDER;
                        else
                            Release();

                    }
                }
                else
                {
                    ShrinkAndSpinPlasma(this, plasmaDuration, arg->frameTime, 0.75);

                    if ( _yls_time <= 0 )
                        _status_flg |= BACT_STFLAG_NORENDER;
                }
            }
            else
            {
                _scale_time = plasmaDuration;
                _vp_extra[0].scale = 0.75;
                _vp_extra[0].pos = _position;
                _vp_extra[0].rotate = _rotation;
                _vp_extra[0].SetVP(_vp_genesis);
                _vp_extra[0].flags |= (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);

                if ( _world->_isNetGame )
                {
                    uamessage_startPlasma splMsg;
                    splMsg.msgID = UAMSG_STARTPLASMA;
                    splMsg.owner = _owner;
                    splMsg.scale = 0.75;
                    splMsg.time = plasmaDuration;
                    splMsg.id = _gid;
                    splMsg.pos = _position;
                    splMsg.dir = _rotation;

                    _world->NetBroadcastMessage(&splMsg, sizeof(splMsg), true);
                }
            }
        }
        else if ( _yls_time <= 0 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
                _status_flg |= BACT_STFLAG_NORENDER;
            else
                Release();
        }
    }
    else
    {
        TCrashLandStep crashStep;
        crashStep.flags = 3;
        crashStep.frameTime = arg->frameTime;

        CrashOrLand(&crashStep);
    }
}

void NC_STACK_ypabact::NetInterpolateMotion(update_msg *upd)
{
    float ftime = upd->frameTime * 0.001;

    if ( 0.001 * (upd->gTime - _lastFrmStamp) > 0.0 )
    {
        // Interpolate rotation
        _rotation += _netDRot * ftime;

        vec3d axis = _rotation.AxisX();

        if (axis.normalise() > 0.0001)
            _rotation.SetX( axis );
        else
            _rotation.SetX( vec3d::OX(1.0) );

        axis = _rotation.AxisY();

        if (axis.normalise() > 0.0001)
            _rotation.SetY( axis );
        else
            _rotation.SetY( vec3d::OY(1.0) );

        // Get "90 - angle" between interpolated X and Y
        float as = C_PI_2 - clp_acos( _rotation.AxisX().dot( _rotation.AxisY() ));

        // Calculate correction axis
        vec3d axs = _rotation.AxisX() * _rotation.AxisY();

        // FIX MY MATH ?
        // axs must be 1.0 normalised?

        // Rotate Y axis for 90" between X and Y
        vec3d newY = mat3x3::AxisAngle(axs, -as).Transform( _rotation.AxisY() );

        _rotation.SetY( newY );

        _rotation.SetZ( _rotation.AxisX() * _rotation.AxisY() );

        _position += _fly_dir * (_fly_dir_length * ftime * 6.0);

        CorrectPositionInLevelBox(NULL);
    }
}

void NC_STACK_ypabact::NetExtrapolateMotion(update_msg *upd)
{
    float ftime = upd->frameTime * 0.001;
    float stupd = (upd->gTime - _lastFrmStamp) * 0.001;

    if ( stupd > 0.0 )
    {
        _rotation += _netDRot * ftime;

        vec3d axis = _rotation.AxisX();

        if (axis.normalise() > 0.0001)
            _rotation.SetX( axis );
        else
            _rotation.SetX( vec3d::OX(1.0) );

        axis = _rotation.AxisY();

        if (axis.normalise() > 0.0001)
            _rotation.SetY( axis );
        else
            _rotation.SetY( vec3d::OY(1.0) );

        axis = _rotation.AxisZ();

        if (axis.normalise() > 0.0001)
            _rotation.SetZ( axis );
        else
            _rotation.SetZ( vec3d::OZ(1.0) );

        vec3d spd = _fly_dir * _fly_dir_length + _netDSpeed * stupd;

        bool hgun = false;
        if (_bact_type == BACT_TYPES_GUN)
        {
            NC_STACK_ypagun *guno = dynamic_cast<NC_STACK_ypagun *>(this);
            if (guno)
            {
                hgun = guno->IsRoboGun();
            }
        }

        // TODO(multiplayer): Probably a decompile slip. This replaces the
        // position with one step's movement, so a remote unit would jump near
        // the map origin; NetInterpolateMotion moves with "+=". Check against
        // the original before relying on network play.
        if (_bact_type != BACT_TYPES_GUN || hgun)
            _position = spd * ftime * 6.0;

        CorrectPositionInLevelBox(NULL);

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            TRayIntersection ray;
            ray.stPos = _position;
            ray.vect = _rotation.AxisY() * 200.0;
            ray.flags = 0;

            _world->IntersectRay(&ray);

            if ( ray.isect )
                _position = ray.isectPos - _rotation.AxisY() * _overeof;
        }
    }
}

size_t NC_STACK_ypabact::PathFinder(TPathRequest *arg)
{
    //path find for ground units (tank & car)
    int maxsteps = arg->steps_cnt;

    for (cellArea &cll : _world->_cells)
    {
        cll.pf_flags = 0;
        cll.cost_to_this = 0;
        cll.cost_to_target = 0;
        cll.pf_treeup= NULL;
    }

    cellArea *target_pcell = _world->GetSector( World::PositionToSectorID( arg->to ) );
    cellArea *start_pcell = _world->GetSector( World::PositionToSectorID( arg->from ) );

    if ( target_pcell == start_pcell )
    {
        arg->steps_cnt = 1;
        arg->waypoints[0].x = arg->to.x;
        arg->waypoints[0].z = arg->to.y;
        return 1;
    }

    std::list<cellArea *> openList;

    start_pcell->pf_flags |= CELL_PFLAGS_IN_CLST;

    cellArea *current_pcell = start_pcell;

    int startDistX = Common::ABS(target_pcell->CellId.x - current_pcell->CellId.x);
    int startDistZ = Common::ABS(target_pcell->CellId.y - current_pcell->CellId.y);

    float sq2 = sqrt(2.0);

    current_pcell->cost_to_target = Common::MIN(startDistX, startDistZ) * sq2 + Common::ABS(startDistX - startDistZ);
    current_pcell->cost_to_this = 0;
    while ( 1 )
    {

        for(int dx = -1; dx <= 1; dx++)
        {
            for(int dz = -1; dz <= 1; dz++)
            {
                if ( dx == 0.0 && dz == 0.0 )
                    continue;

                Common::Point currentSec = current_pcell->CellId;
                Common::Point t = currentSec + Common::Point(dx, dz);

                if ( _world->IsGamePlaySector(t) )
                {
                    cellArea *cell_tzx = _world->GetSector(t);

                    if ( cell_tzx->pf_flags & CELL_PFLAGS_IN_CLST )
                        continue;

                    if ( cell_tzx->addit_cost >= 100 )
                        continue;

                    if (fabs(current_pcell->height - cell_tzx->height) >= 500.0 )
                        continue;

                    if (cell_tzx->SectorType == 1 && cell_tzx != target_pcell)
                    {
                        int32_t hlth = _world->GetLegoBld(cell_tzx, 0, 0);

                        if (_world->_legoArray[hlth].UseCollisionSkelet != _world->_legoArray[hlth].CollisionSkelet)
                            continue;
                    }

                    if ( dx != 0 && dz != 0)
                    {
                        cellArea *cell_tz = _world->GetSector(currentSec.x, t.y);
                        cellArea *cell_tx = _world->GetSector(t.x, currentSec.y);

                        if ( fabs(current_pcell->height - cell_tzx->height) > 300.0
                                || fabs(current_pcell->height - cell_tz->height) > 300.0
                                || fabs(current_pcell->height - cell_tx->height) > 300.0
                                || fabs(cell_tz->height - cell_tx->height) > 300.0
                                || fabs(cell_tzx->height - cell_tz->height) > 300.0
                                || fabs(cell_tzx->height - cell_tx->height) > 300.0)
                            continue;
                    }

                    float new_cost_to_this = sqrt(POW2(dx) + POW2(dz)) + cell_tzx->addit_cost + current_pcell->cost_to_this;

                    int distX = Common::ABS(target_pcell->CellId.x - t.x);
                    int distZ = Common::ABS(target_pcell->CellId.y - t.y);

                    float new_cost_to_target = Common::MIN(distX, distZ) * sq2 + Common::ABS(distX - distZ);

                    if ( (cell_tzx->pf_flags & CELL_PFLAGS_IN_OLST)
                            && new_cost_to_this + new_cost_to_target > cell_tzx->cost_to_this + cell_tzx->cost_to_target )
                        continue;

                    cell_tzx->cost_to_this = new_cost_to_this;
                    cell_tzx->cost_to_target = new_cost_to_target;

                    if ( !(cell_tzx->pf_flags & CELL_PFLAGS_IN_OLST) )
                        openList.push_back(cell_tzx);

                    cell_tzx->pf_treeup = current_pcell;
                    cell_tzx->pf_flags |= CELL_PFLAGS_IN_OLST;
                }
            }
        }



        if ( openList.empty() )
        {
            arg->steps_cnt = 0;
            return 0;
        }

        std::list<cellArea *>::iterator it = openList.begin();
        
        std::list<cellArea *>::iterator selected = it;
        float selected_value = (*selected)->cost_to_this + (*selected)->cost_to_target;
        
        for(it++; it != openList.end(); it++)
        {
            float totalCost = (*it)->cost_to_this + (*it)->cost_to_target;

            if ( totalCost < selected_value )
            {
                selected = it;
                selected_value = totalCost;
            }
        }
        
        current_pcell = *selected;

        openList.erase(selected); // Remove OLIST

        current_pcell->pf_flags &= ~CELL_PFLAGS_IN_OLST;
        current_pcell->pf_flags |= CELL_PFLAGS_IN_CLST;

        if ( current_pcell == target_pcell )
            break;
    }

    std::stack<cellArea *> pathCells;

    cellArea *iter_cell = target_pcell;

    while(iter_cell)
    {
        pathCells.push(iter_cell);
        iter_cell = iter_cell->pf_treeup;
    }

    cellArea *curcell = pathCells.top();
    pathCells.pop();
    
    cellArea *nextcell = pathCells.top();

    int dirX = nextcell->CellId.x - curcell->CellId.x;
    int dirZ = nextcell->CellId.y - curcell->CellId.y;

    int step_id = 0;

    while ( !pathCells.empty() )
    {
        if ( maxsteps <= 1 || nextcell == target_pcell)
        {
            arg->waypoints[ step_id ].x = arg->to.x;
            arg->waypoints[ step_id ].z = arg->to.y;
            break;
        }

        curcell = nextcell;
        
        pathCells.pop();
        nextcell = pathCells.top();

        if ( nextcell->CellId.x - curcell->CellId.x != dirX || nextcell->CellId.y - curcell->CellId.y != dirZ )
        {
            float tx, tz;

            if ( Common::ABS(dirX) < Common::ABS(dirZ) )
            {
                if ( dirX > 0 )
                {
                    tz = 0.0;
                    tx = -200.0;
                }
                else
                {
                    tz = 0.0;
                    tx = 200.0;
                }
            }
            else
            {
                if ( dirZ > 0 )
                {
                    tz = 200.0;
                    tx = 0.0;
                }
                else
                {
                    tz = -200.0;
                    tx = 0.0;
                }
            }

            dirX = nextcell->CellId.x - curcell->CellId.x;
            dirZ = nextcell->CellId.y - curcell->CellId.y;
            
            arg->waypoints[ step_id ] = World::SectorIDToCenterPos3(curcell->CellId) + vec3d(tx, 0.0, tz);
            maxsteps--;
            step_id++;
        }
    }

    arg->steps_cnt = step_id + 1;
    return 1;
}

void NC_STACK_ypabact::SetKidsPath(int beginWp)
{
    for (NC_STACK_ypabact* &kidunit : _kidList)
    {
        kidunit->_waypoints_count = _waypoints_count;
        kidunit->_current_waypoint = beginWp;

        kidunit->_status_flg |= BACT_STFLAG_WAYPOINT;

        if ( _status_flg & BACT_STFLAG_WAYPOINTCCL )
            kidunit->_status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            kidunit->_status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        for (int i = 0; i < 32; i++)
        {
            kidunit->_waypoints[i] = _waypoints[i];
        }
    }
}

size_t NC_STACK_ypabact::SetPath(TPathRequest *arg)
{
    // path find caller for ground squads
    int maxsteps = arg->steps_cnt;

    // TODO(cleanup): The same as tgt_type != 1.
    if ( arg->tgt_type >= 2 || arg->tgt_type != 1 )
        return 0; //may be 1   CHECK IT

    if ( !PathFinder(arg) )
        return 0;

    setTarget_msg targetMsg;
    if ( arg->steps_cnt <= 1 )
    {
        targetMsg.tgt_pos.x = arg->to.x;
        targetMsg.tgt_pos.z = arg->to.y;
    }
    else
    {
        for (int i = 0; i < arg->steps_cnt; i++)
        {
            _waypoints[i] = arg->waypoints[i];
        }

        _status_flg |= BACT_STFLAG_WAYPOINT;

        _current_waypoint = 0;
        _waypoints_count = arg->steps_cnt;

        SetKidsPath(0);

        targetMsg.tgt_pos.x = arg->waypoints[0].x;
        targetMsg.tgt_pos.z = arg->waypoints[0].z;
    }

    targetMsg.tgt_type = BACT_TGT_TYPE_CELL;
    targetMsg.priority = 0;
    SetTarget(&targetMsg);

    for (NC_STACK_ypabact* &kidunit : _kidList)
    {
        if ( (kidunit->_bact_type == BACT_TYPES_CAR || kidunit->_bact_type == BACT_TYPES_TANK) && _pSector != kidunit->_pSector )
        {
            TPathRequest kidPath;
            kidPath.steps_cnt = maxsteps;
            kidPath.from = kidunit->_position.XZ();
            kidPath.to = arg->to;
            kidPath.tgt_type = arg->tgt_type;

            kidunit->SetPath(&kidPath);
        }
    }

    return 1;
}



void NC_STACK_ypabact::setBACT_viewer(bool vwr)
{
    uamessage_viewer viewMsg;

    if ( vwr )
    {
        if (_world->_viewerBact)
        {
            if ( _world->_viewerBact->_bact_type != BACT_TYPES_MISSLE )
                _salve_counter = 0;
        }

        _world->SetViewerUnit(this); //Set current bact

        _oflags |= BACT_OFLAG_VIEWER;

        if ( _world->_isNetGame )
            viewMsg.view = 1;

        if ( _bact_type == BACT_TYPES_BACT && !(_status_flg & BACT_STFLAG_LAND) && _status == BACT_STATUS_NORMAL )
            _thraction = _force;

        SFXEngine::SFXe.startSound(&_soundcarrier, 8);
    }
    else
    {
        _oflags &= ~BACT_OFLAG_VIEWER;

        if ( _world->_isNetGame )
            viewMsg.view = 0;

        SFXEngine::SFXe.stopSound(&_soundcarrier, 8);

        if ( _bact_type != BACT_TYPES_MISSLE && _bact_type != BACT_TYPES_ROBO && _status != BACT_STATUS_DEAD )
        {
            if ( _host_station == _parent )
            {
                if ( !(_status_flg & BACT_STFLAG_WAYPOINT) || !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
                {
                    for (NC_STACK_ypabact* &node : _kidList)
                        node->CopyTargetOf(this);
                }
            }
            else
            {
                if ( !(_status_flg & BACT_STFLAG_WAYPOINT) || !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
                    CopyTargetOf(_parent);
            }
        }
    }

    if ( _world->_isNetGame ) // Network message send routine?
    {
        viewMsg.msgID = UAMSG_VIEWER;
        viewMsg.owner = _owner;
        viewMsg.classID = _bact_type;
        viewMsg.id = _gid;

        if ( viewMsg.classID == BACT_TYPES_MISSLE )
        {
            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(this);
            viewMsg.launcher = miss->GetLauncherBact()->_gid;
        }

        _world->NetBroadcastMessage(&viewMsg, sizeof(viewMsg), true);
    }
}

void NC_STACK_ypabact::setBACT_inputting(bool inpt)
{
    if ( inpt )
    {
        _oflags |= BACT_OFLAG_USERINPT;
        _world->setYW_userVehicle(this);

        if ( _bact_type != BACT_TYPES_GUN )
            CorrectPositionOnLand();
    }
    else
    {
        _oflags &= ~BACT_OFLAG_USERINPT;
    }
}

void NC_STACK_ypabact::setBACT_exactCollisions(bool col)
{
    if ( col )
        _oflags |= BACT_OFLAG_EXACTCOLL;
    else
        _oflags &= ~BACT_OFLAG_EXACTCOLL;
}

void NC_STACK_ypabact::setBACT_bactCollisions(bool col)
{
    if ( col )
        _oflags |= BACT_OFLAG_BACTCOLL;
    else
        _oflags &= ~BACT_OFLAG_BACTCOLL;
}

void NC_STACK_ypabact::setBACT_airconst(int air)
{
    _airconst = air;
    _airconst_static = air;
}

void NC_STACK_ypabact::setBACT_landingOnWait(bool lnding)
{
    if ( lnding )
        _oflags |= BACT_OFLAG_LANDONWAIT;
    else
        _oflags &= ~BACT_OFLAG_LANDONWAIT;
}

void NC_STACK_ypabact::setBACT_yourLastSeconds(int ls)
{
    _yls_time = ls;
}

void NC_STACK_ypabact::SetVP(NC_STACK_base *vp)
{
    Common::DeleteAndNull(&_current_vp);
    if (vp)
        _current_vp = vp->GenRenderInstance();
}

void NC_STACK_ypabact::setBACT_aggression(int aggr)
{
    _aggr = aggr;
    
    for (NC_STACK_ypabact* &nod : _kidList)
        nod->_aggr = aggr;
}

void NC_STACK_ypabact::setBACT_extraViewer(bool vwr)
{
    if ( vwr )
        _oflags |= BACT_OFLAG_EXTRAVIEW;
    else
        _oflags &= ~BACT_OFLAG_EXTRAVIEW;
}

void NC_STACK_ypabact::setBACT_alwaysRender(bool rndr)
{
    if ( rndr )
        _oflags |= BACT_OFLAG_ALWAYSREND;
    else
        _oflags &= ~BACT_OFLAG_ALWAYSREND;
}





bool NC_STACK_ypabact::IsNeedsWaypoints() const
{
    if (IsGroundUnit())
        return true;
    
    for (NC_STACK_ypabact* const &unit : _kidList)
    {
        if (unit->IsGroundUnit())
            return true;
    }
    
    return false;
}

void NC_STACK_ypabact::CleanAttackersTarget()
{
    for(auto it = _attackersList.begin();
        it != _attackersList.end();
        it = _attackersList.erase(it))
    {
        NC_STACK_ypabact *attacker = it->attacker;

        if ( attacker->_primTtype == BACT_TGT_TYPE_UNIT &&
             attacker->_primT.pbact == this )
        {
            attacker->_primT.pbact = NULL;
            attacker->_primTtype = BACT_TGT_TYPE_NONE;
            attacker->_assess_time = 0;
        }
        
        if ( attacker->_secndTtype == BACT_TGT_TYPE_UNIT && 
             attacker->_secndT.pbact == this )
        {
            attacker->_secndT.pbact = NULL;
            attacker->_secndTtype = BACT_TGT_TYPE_NONE;
            attacker->_assess_time = 0;
        }
    }
}

void NC_STACK_ypabact::DeleteAttacker(NC_STACK_ypabact *bact, int tgtType)
{
    _attackersList.remove( TBactAttacker(tgtType, bact) );
}

void NC_STACK_ypabact::AddAttacker(NC_STACK_ypabact *bact, int tgtType)
{
    _attackersList.push_back(TBactAttacker(tgtType, bact));
}

bool NC_STACK_ypabact::IsParentMyRobo() const
{
    return (_host_station) && (_parent) && (_host_station == _parent);
}

void NC_STACK_ypabact::ChangeEscapeFlag(bool escape)
{
    if ( escape )
        _status_flg |= BACT_STFLAG_ESCAPE;
    else
        _status_flg &= ~BACT_STFLAG_ESCAPE;

    // May be do it in recursion?
    for( NC_STACK_ypabact* &node : _kidList ) 
    {
        if ( escape )
            node->_status_flg |= BACT_STFLAG_ESCAPE;
        else
            node->_status_flg &= ~BACT_STFLAG_ESCAPE;
    }
}

bool NC_STACK_ypabact::IsHidden() const 
{
    if (_hidden)
        return true;
    
    if (_world && _world->IsHidden(_owner))
        return true;
    
    return false;
}

bool NC_STACK_ypabact::IsHiddenFor(uint8_t owner) const
{
    if (owner == _owner) // Own unit can be seen
        return false;
    
    if (_pSector && _pSector->IsUnhideFor(owner))
        return false;
    
    if (_hidden)
        return true;
    
    if (_world && _world->IsHidden(_owner))
        return true;
    
    return false;
}