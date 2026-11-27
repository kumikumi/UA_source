#include <inttypes.h>
#include <string.h>
#include <math.h>
#include "env.h"
#include "includes.h"
#include "yw_internal.h"
#include "yw.h"
#include "yw_net.h"
#include "windp.h"
#include "loaders.h"

#include "yparobo.h"
#include "font.h"
#include "system/inivals.h"
#include "system/perf.h"



NC_STACK_bitmap * loadDisk_screen(NC_STACK_ypaworld *yw)
{
    SFXEngine::SFXe.StopMusicTrack();

    const char *fileName;

    if ( yw->_screenSize.x <= 360 )
        fileName = "disk320.ilbm";
    else if ( yw->_screenSize.x > 600 )
        fileName = "disk640.ilbm";
    else
        fileName = "disk512.ilbm";

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:mc2res");

    NC_STACK_bitmap *disk = Utils::ProxyLoadImage({
        {NC_STACK_rsrc::RSRC_ATT_NAME, std::string(fileName)},
        {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});

    Common::Env.SetPrefix("rsrc", oldRsrc);

    return disk;
}

void draw_splashScreen(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen)
{
    if ( splashScreen )
    {
        GFX::TBitmapBlitParams blit;

        blit.pbitm = splashScreen->GetBitmap();

        blit.srcRect  = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        blit.dstRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);

        GFX::Engine.SetCursor(6, 0);

        GFX::Engine.BeginFrame();
        GFX::Engine.DrawBitmapNormalized(&blit);
        GFX::Engine.EndFrame();

        GFX::Engine.BeginFrame();
        GFX::Engine.DrawBitmapNormalized(&blit);
        GFX::Engine.EndFrame();
    }
}

void drawSplashScreenWithTOD(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen, const std::string &text)
{
    if ( splashScreen )
    {
        GFX::TBitmapBlitParams blit;

        blit.pbitm = splashScreen->GetBitmap();

        blit.srcRect  = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        blit.dstRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);

        GFX::Engine.SetCursor(6, 0);

        GFX::Engine.BeginFrame();
        GFX::Engine.DrawBitmapNormalized(&blit);
        splashScreen_OutText(yw, text, yw->_screenSize.x / 7, yw->_screenSize.y / 5);
        GFX::Engine.EndFrame();

        GFX::Engine.BeginFrame();
        GFX::Engine.DrawBitmapNormalized(&blit);
        splashScreen_OutText(yw, text, yw->_screenSize.x / 7, yw->_screenSize.y / 5);
        GFX::Engine.EndFrame();
    }
}

void NC_STACK_ypaworld::PowerStationErase(cellArea *cell)
{
    if (!cell) return;
    
    auto it = _powerStations.find(cell->Id);
    if (it == _powerStations.end())
    {
        /* Oh no, power station info for this sector does not exist.
           So just clear sector purpose field if it was PowerStation*/
        if (cell->PurposeType == cellArea::PT_POWERSTATION)
        {
            cell->PurposeIndex = 0;
            cell->PurposeType = cellArea::PT_NONE;
        }
        
        return;
    }
    
    _powerStations.erase(it);
    
    cell->PurposeIndex = 0;
    cell->PurposeType = cellArea::PT_NONE;
    
    _lvlBuildingsMap(cell->CellId) = 0;
}

void LoadLevelPalette(NC_STACK_ypaworld *yw, TLevelDescription *mapp)
{
    if (!mapp->Palettes.empty())
    {
        if (!mapp->Palettes[0].empty())
        {
            NC_STACK_bitmap *ilbm = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, mapp->Palettes[0]},
                {NC_STACK_bitmap::BMD_ATT_HAS_COLORMAP, (int32_t)1}});

            if (ilbm)
            {
                GFX::Engine.SetPalette(*ilbm->getBMD_palette());
                ilbm->Delete();
            }
            else
            {
                ypa_log_out("WARNING: slot #%d [%s] init failed!\n", 0, mapp->Palettes[0].c_str());
            }
        }
    }
}

int NC_STACK_ypaworld::LevelCommonLoader(TLevelDescription *mapp, int levelID, const std::string &description)
{
    int ok = 0;

    *mapp = TLevelDescription();

    _gameplayStats.fill( World::TPlayerStatus() );

    _timeStamp = 0;
    _msgTimestampHSReturn = 0;
    _msgTimestampEnemySector = 0;
    _msgTimestampGates = 0;
    _msgTimestampPSUnderAtk = 0;
    _lastRoboMessages.fill(TLastRoboMessage());
    _lastEnemyHostFoundMsgTime.fill(0);
    _framesElapsed = 0;

    _nextUnitId = World::CVFirstUnitId;
    _nextCommandId = 1;

    _levelInfo.LevelID = levelID;
    _levelDescription = description;
    _levelInfo.State = TLevelInfo::STATE_PLAYING;
    _levelInfo.OwnerMask = 0;
    _levelInfo.UserMask = 0;

    ResetControls();
    _localPlayer->HostStation = NULL;
    _localPlayer->Unit = NULL;
    _localPlayer->Controls = NULL;
    _playerHSDestroyed = false;
    _vehicleSectorRatio = 0;
    _beamEnergyCurrent = 0;
    _invulnerable = 0;

    _levelInfo.Gates.clear();
    _levelInfo.SuperItems.clear();

    _techUpgrades.clear();
    
    _netEvent = TNetGameEvent();
    _countUnitsPerOwner.fill(0);

    _dbgTotalSquadCountMax = 0;
    _dbgTotalVehicleCountMax = 0;
    _dbgTotalFlakCountMax = 0;
    _dbgTotalWeaponCountMax = 0;
    _dbgTotalRoboCountMax = 0;
    
    _playerOwner = 0;
    
    /* Set hidden fractions to default world's*/
    _hiddenFractions = _worldHiddenFractions;

    UpdateScreenSize();

    NC_STACK_bitmap *diskScreenImage = loadDisk_screen(this);

    if ( diskScreenImage )
        draw_splashScreen(this, diskScreenImage);


    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:fonts");

    int fontsLoaded = load_fonts_and_icons();

    Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( !fontsLoaded )
        return 0;

    int tod = loadTOD(this, "tod.def");

    int next_tod = tod + 1;
    
    if ( next_tod + 2490 > 2512 )
        next_tod = 0;

    writeTOD(this, "tod.def", next_tod);

    if ( diskScreenImage )
    {
        drawSplashScreenWithTOD(this, diskScreenImage, Locale::Text::ToD(tod, " "));
        diskScreenImage->Delete();
    }

    _profileFramesCount = 0;
    for (int i = 0; i < PFID_MAX; i++)
    {
        _profileVals[i] = 0;
        _profileMax[i] = 0;
        _profileMin[i] = 100000;
        _profileTotal[i] = 0;
    }

    _history.Close();

    audio_volume = SFXEngine::SFXe.getMasterVolume();

    _voiceMessage.Reset();

    Common::Env.SetPrefix("rsrc", "data:");

    if ( ParseLevelDescription(mapp, levelID, _levelDescription) && mapp->IsOk() )
    {       
        _tutorialHints.Start(mapp->EventLoopID);

        _energyAccumMap.Clear();
        _nextPSForUpdate = 0;

        _inBuildProcess.clear();

        Common::Env.SetPrefix("rsrc", fmt::sprintf("data:set%d", mapp->SetID));

        LoadLevelPalette(this, mapp);

        if ( yw_LoadSet(mapp->SetID) )
        {
            if ( yw_loadSky(this, mapp->SkyStr) )
                ok = 1;
        }
    }

    FFeedback_Init();

    return ok;
}

bool NC_STACK_ypaworld::LoadTypeMap(const std::string &mapName)
{
    if ( _lvlTypeMap.IsNull() )
        _lvlTypeMap = World::LoadMapDataFromImage(mapName);
    
    if ( _lvlTypeMap.IsNull() )
        return false;

    SetMapSize(_lvlTypeMap.Size());

    int32_t id = 0;
    for(int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            
            TSectorDesc *sectp = &_secTypeArray[ _lvlTypeMap( cell.CellId ) ];

            cell.type_id = _lvlTypeMap( cell.CellId );
            cell.SectorType = sectp->SectorType;
            cell.energy_power = 0;

            if ( sectp->SectorType == 1)
            {
                cell.buildings_health.At(0, 0) = sectp->SubSectors.At(0, 0)->StartHealth;
            }
            else
            {
                for (int bldY = 0; bldY < 3; bldY++)
                    for (int bldX = 0; bldX < 3; bldX++)
                        cell.buildings_health.At(bldX, bldY) = sectp->SubSectors.At(bldX, bldY)->StartHealth;
            }
            
            id++;
        }
    }
    return true;
}

bool NC_STACK_ypaworld::LoadOwnerMap(const std::string &mapName)
{
    _countSectorsPerOwner.fill(0);

    if ( _lvlOwnMap.IsNull() )
        _lvlOwnMap = World::LoadMapDataFromImage(mapName);
    
    if ( _lvlOwnMap.IsNull() )
        return false;

    if ( _lvlOwnMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlOwnMap.Width(), _lvlOwnMap.Height(), _mapSize.x, _mapSize.y);
        _lvlOwnMap.Clear();
        return false;
    }


    for (uint32_t yy = 0; yy < _lvlOwnMap.Height(); yy++)
    {
        for (uint32_t xx = 0; xx < _lvlOwnMap.Width(); xx++)
        {
            Common::Point cellId(xx, yy);
            if ( IsGamePlaySector( cellId ) )
            {
                _cells(cellId).owner = _lvlOwnMap(cellId);
                _countSectorsPerOwner[ _lvlOwnMap(cellId) ]++;
            }
            else
            {
                _cells(cellId).owner = 0;
                _countSectorsPerOwner[ 0 ]++;
            }
        }
    }

    return true;
}

bool NC_STACK_ypaworld::LoadHightMap(const std::string &mapName)
{
    if ( _lvlHeightMap.IsNull() )
        _lvlHeightMap = World::LoadMapDataFromImage(mapName);
    
    if ( _lvlHeightMap.IsNull() )
        return false;

    if ( _lvlHeightMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlHeightMap.Width(), _lvlHeightMap.Height(), _mapSize.x, _mapSize.y);
        _lvlHeightMap.Clear();
        return false;
    }

    for (int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            cell.height = (-100.0) * _lvlHeightMap( cell.CellId );
            cell.CenterPos = World::SectorIDToCenterPos3( cell.CellId );
            cell.CenterPos.y = cell.height;
        }
    }

    for (int y = 1; y < _mapSize.y; y++)
    {
        for (int x = 1; x < _mapSize.x; x++)
        {
            _cells(x, y).averg_height = (_cells(x    ,     y).height +
                                         _cells(x - 1,     y).height +
                                         _cells(x - 1, y - 1).height +
                                         _cells(x    , y - 1).height ) / 4.0;
        }
    }

    return true;
}

bool NC_STACK_ypaworld::yw_createRobos(const std::vector<MapRobo> &Robos)
{
    _levelInfo.OwnerMask = 0;
    _levelInfo.UserMask = 2;
    
    bool first = true;

    for ( const MapRobo &roboInf : Robos)
    {
        TRayIntersection ray;
        ray.stPos = vec3d::X0Z(roboInf.Pos) - vec3d::OY(30000.0);
        ray.vect = vec3d::OY(50000.0);
        ray.flags = 0;

        TUnitSpawn spawn;
        spawn.vehicle_id = roboInf.VhclID;
        spawn.pos = roboInf.Pos;

        IntersectRay(&ray);

        if ( ray.isect )
            spawn.pos.y += ray.isectPos.y;

        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>( CreateVehicle(&spawn) );

        if ( robo )
        {
            int reloadConst = 0;

            AddToUnitsList(robo);

            int energy;

            if ( !first )
            {
                energy = roboInf.Energy;
            }
            else
            {
                energy = roboInf.Energy / 4;

                if ( energy < _maxRoboEnergy )
                {
                    energy = _maxRoboEnergy;
                    reloadConst = _maxReloadConst;
                }

            }

            robo->_owner = roboInf.Owner;
            robo->_energy = energy;
            robo->_energy_max = energy;

            if ( !reloadConst )
            {
                if ( roboInf.ReloadConst )
                    reloadConst = roboInf.ReloadConst;
                else
                    reloadConst = robo->_energy_max;
            }

            robo->_reload_const = reloadConst;

            robo->setBACT_bactCollisions(1);
            robo->setROBO_fillMode(15);
            robo->setROBO_battVehicle(energy);
            robo->setROBO_battBeam(energy);

            _levelInfo.OwnerMask |= 1 << roboInf.Owner;

            robo->setROBO_epConquer(roboInf.ConBudget);
            robo->setROBO_epDefense(roboInf.DefBudget);
            robo->setROBO_epRadar(roboInf.RadBudget);
            robo->setROBO_epPower(roboInf.PowBudget);
            robo->setROBO_epSafety(roboInf.SafBudget);
            robo->setROBO_epChangeplace(roboInf.CplBudget);
            robo->setROBO_epRobo(roboInf.RobBudget);
            robo->setROBO_epReconnoitre(roboInf.RecBudget);
            robo->setROBO_viewAngle(roboInf.ViewAngle);
            robo->setROBO_safDelay(roboInf.SafDelay);
            robo->setROBO_powDelay(roboInf.PowDelay);
            robo->setROBO_cplDelay(roboInf.CplDelay);
            robo->setROBO_radDelay(roboInf.RadDelay);
            robo->setROBO_defDelay(roboInf.DefDelay);
            robo->setROBO_conDelay(roboInf.ConDelay);
            robo->setROBO_recDelay(roboInf.RecDelay);
            robo->setROBO_robDelay(roboInf.RobDelay);

            if ( first )
            {
                robo->setBACT_viewer(true, _localPlayer);
                robo->setBACT_inputting(true, _localPlayer);
            }
        }
        
        first = false;
    }
    return true;
}

bool NC_STACK_ypaworld::LoadBlgMap(const std::string &mapName)
{
    if ( _lvlBuildingsMap.IsNull() )
        _lvlBuildingsMap = World::LoadMapDataFromImage(mapName);
    
    if ( _lvlBuildingsMap.IsNull() )
        return false;

    if ( _lvlBuildingsMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlBuildingsMap.Width(), _lvlBuildingsMap.Height(), _mapSize.x, _mapSize.y);
        _lvlBuildingsMap.Clear();
        return false;
    }

    for ( int y = 0; y < _mapSize.y; y++)
    {
        for ( int x = 0; x < _mapSize.x; x++)
        {
            int blg = _lvlBuildingsMap(x, y);
            cellArea &cell = _cells(x, y);
            
            if (blg && cell.owner)
            {
                TBuildingPlacement placement;

                placement.owner = cell.owner;
                placement.blg_ID = blg;
                placement.instant = 1;
                placement.flags = 0;
                placement.CellId = cell.CellId;

                CreateBuilding(&placement);
            }
        }
    }

    return true;
}

void NC_STACK_ypaworld::yw_InitSquads(const std::vector<MapSquad> &squads)
{
    size_t i = 0;
    for ( const MapSquad &squad : squads )
    {
        NC_STACK_yparobo *robo = NULL;

        for( NC_STACK_ypabact *unit : _unitsList )
        {
            if ( unit->_bact_type == BACT_TYPES_ROBO && unit->_owner == squad.Owner)
            {
                robo = dynamic_cast<NC_STACK_yparobo *>(unit);
                break;
            }
        }

        if ( !robo )
        {
            ypa_log_out("WARNING: yw_InitSquads(): no host robo for squad[%d], owner %d!\n", i, squad.Owner);
        }
        else
        {
            vec3d squadPos;

            TRayIntersection ray;
            ray.stPos.x = squad.X;
            ray.stPos.y = -50000.0;
            ray.stPos.z = squad.Z;
            ray.vect = vec3d::OY(100000.0);
            ray.flags = 0;
            IntersectRay(&ray);

            if ( ray.isect )
                squadPos = ray.isectPos - vec3d::OY(50.0);
            else
            {
                TSectorInfo sect_info;
                sect_info.pos_x = squad.X;
                sect_info.pos_z = squad.Z;

                if ( !GetSectorInfo(&sect_info) )
                {
                    ypa_log_out("yw_InitSquads(): no valid position for squad[%d]!\n", i);
                    return;
                }

                squadPos.x = squad.X;
                squadPos.y = sect_info.pcell->height;
                squadPos.z = squad.Z;
            }
            // Create squad by robo method
            robo->MakeSquad( std::vector<int>(squad.Count, squad.VhclID), squadPos, squad.Useable); // yparobo_func133
        }
        
        i++;
    }
}

void NC_STACK_ypaworld::InitBuddies()
{
    if ( !_levelInfo.Buddies.empty() )
    {
        int squad_sn = 0;
        
        std::vector<TMapBuddy> buds = _levelInfo.Buddies;
        while ( 1 )
        {
            std::vector<int> VhclIDS;
            int wrkID = -1;
            for (std::vector<TMapBuddy>::iterator it = buds.begin(); it != buds.end(); )
            {
                if (wrkID == -1 || wrkID == it->CommandID )
                {
                    wrkID = it->CommandID;
                    VhclIDS.push_back(it->Type);
                    it = buds.erase(it);
                }
                else
                    it++;
            }

            if ( wrkID == -1 )
                break;

            vec3d squadPos =    UserRobo()->_position +  
                                vec3d(  sin(squad_sn * 1.745) * 500.0,
                                        0.0,
                                        cos(squad_sn * 1.745) * 500.0 );

            TRayIntersection ray;
            ray.stPos = squadPos.X0Z() + vec3d(0.5, -50000.0, 0.75);
            ray.vect = vec3d::OY(100000.0);
            ray.flags = 0;

            IntersectRay(&ray);

            if ( ray.isect )
                squadPos.y = ray.isectPos.y + -100.0;

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(UserRobo());

            robo->MakeSquad(VhclIDS, squadPos, true); //robo 133 method

            squad_sn++;
        }
    }
}

void NC_STACK_ypaworld::yw_InitTechUpgradeBuildings()
{
    _upgradeTimeStamp = 0;
    _upgradeBuildId = 0;
    _upgradeVehicleId = 0;
    _upgradeWeaponId = 0;
    _upgradeId = -1;

    for (size_t i = 0; i < _techUpgrades.size(); i++)
    {
        TMapGem &gem = _techUpgrades[i];
        cellArea &cell = _cells(gem.CellId);

        if (gem.BuildingID)
        {
            if ( cell.PurposeType != cellArea::PT_BUILDINGS || gem.BuildingID != cell.PurposeIndex )
            {
                TBuildingPlacement placement;
                placement.owner = cell.owner;
                placement.blg_ID = gem.BuildingID;
                placement.CellId = gem.CellId;
                placement.instant = 1;
                placement.flags = 0;

                CreateBuilding(&placement);
            }
        }

        cell.PurposeType = cellArea::PT_TECHUPGRADE;
        cell.PurposeIndex = i;
    }
}

// The sectors of the gates and bombs and of their key sectors
void NC_STACK_ypaworld::LinkGatesAndSuperItems()
{
    for (TMapGate &gate : _levelInfo.Gates)
    {
        gate.PCell = &_cells(gate.CellId);

        for (TMapKeySector &ks : gate.KeySectors)
        {
            if ( IsGamePlaySector( ks.CellId ) )
                ks.PCell = &_cells(ks.CellId);
        }
    }

    for (TMapSuperItem &sitem : _levelInfo.SuperItems)
    {
        sitem.PCell = &_cells(sitem.CellId);

        for ( TMapKeySector &ks : sitem.KeySectors )
        {
            if ( IsGamePlaySector( ks.CellId ) )
                ks.PCell = &_cells(ks.CellId);
        }
    }
}

void NC_STACK_ypaworld::InitGates()
{
    LinkGatesAndSuperItems();

    for (size_t i = 0; i < _levelInfo.Gates.size(); i++)
    {
        TMapGate &gate = _levelInfo.Gates[i];

        TBuildingPlacement placement;
        placement.owner = gate.PCell->owner;
        placement.blg_ID = gate.ClosedBldID;
        placement.instant = 1;
        placement.CellId = gate.CellId;
        placement.flags = 0;

        CreateBuilding(&placement);

        gate.PCell->PurposeType = cellArea::PT_GATECLOSED;
        gate.PCell->PurposeIndex = i;
    }
}

void NC_STACK_ypaworld::InitSuperItems()
{
    LinkGatesAndSuperItems();

    for ( size_t i = 0; i < _levelInfo.SuperItems.size(); i++ )
    {
        TMapSuperItem &sitem = _levelInfo.SuperItems[i];

        TBuildingPlacement placement;
        placement.owner = sitem.PCell->owner;
        placement.blg_ID = sitem.InactiveBldID;
        placement.instant = 1;
        placement.CellId = sitem.CellId;
        placement.flags = 0;

        CreateBuilding(&placement);

        sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
        sitem.PCell->PurposeIndex = i;

        sitem.ActiveTime = 0;
        sitem.TriggerTime = 0;
        sitem.ActivateOwner = 0;
        sitem.State = TMapSuperItem::STATE_INACTIVE;
    }
}

void NC_STACK_ypaworld::UpdatePowerEnergy()
{
    // Apply power to sectors and clean power matrix for next compute iteration.

    for (int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            EnergyAccum &accum = _energyAccumMap(x, y);

            accum.Owner = cell.owner;
            cell.energy_power = accum.Energy; // Apply power to cell
            accum.Energy = 0; // Clean matrix's power
        }
    }

    _nextPSForUpdate = 0; // Next power station for recompute power is first
}


void NC_STACK_ypaworld::CellSetOwner(cellArea *cell, uint8_t owner)
{
    if ( cell->owner != owner )
    {
        ScoreEvent( World::History::Event::Conquest(cell->CellId.x, cell->CellId.y, owner) );

        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
            ScoreEvent( World::History::Event::PowerStation(cell->CellId.x, cell->CellId.y, owner) );

        _countSectorsPerOwner[cell->owner]--;
        _countSectorsPerOwner[owner]++;

        cell->owner = owner;
    }
}

void NC_STACK_ypaworld::CellSetNewOwner(cellArea *cell, NC_STACK_ypabact *attacker, int newOwner)
{
    int energon[World::FRACTION_MAXCOUNT];

    if ( newOwner < World::OWNER_UNKNOW )
    {
        newOwner = cell->owner;

        for( int &e : energon )
            e = 0;

        for ( NC_STACK_ypabact* &nod : cell->unitsList )
            energon[nod->_owner] += nod->_energy;

        energon[0] = 0;

        for (int i = 0; i < World::FRACTION_MAXCOUNT; i++)
        {
            if ( energon[i] > energon[newOwner] )
                newOwner = i;
        }
    }

    if ( cell->owner != newOwner )
    {
        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
        {
            if ( UserRobo()->_owner == newOwner )
            {
                if ( attacker )
                {
                    TInfoMessage capturedMsg;
                    capturedMsg.unit = attacker;
                    capturedMsg.Priority = 78;
                    capturedMsg.MsgID = 45;

                    ShowMessage(&capturedMsg);
                }
            }
            else if ( cell->owner == UserRobo()->_owner )
            {
                TInfoMessage lostMsg;
                lostMsg.unit = NULL;
                lostMsg.Priority = 78;
                lostMsg.MsgID = 67;

                ShowMessage(&lostMsg);
            }
        }
        else
        {
            for ( const TMapGate &gate : _levelInfo.Gates )
            {
                for ( const TMapKeySector &ks : gate.KeySectors )
                {
                    if ( cell == ks.PCell )
                    {
                        if ( UserRobo()->_owner == newOwner )
                        {
                            TInfoMessage keyCapturedMsg;
                            keyCapturedMsg.unit = NULL;
                            keyCapturedMsg.Priority = 80;
                            keyCapturedMsg.MsgID = 82;

                            ShowMessage(&keyCapturedMsg);
                        }
                        else if ( UserRobo()->_owner == cell->owner )
                        {
                            TInfoMessage keyLostMsg;
                            keyLostMsg.unit = NULL;
                            keyLostMsg.Priority = 80;
                            keyLostMsg.MsgID = 81;

                            ShowMessage(&keyLostMsg);
                        }
                    }
                }
            }
        }
    }

    CellSetOwner(cell, newOwner);
}

void NC_STACK_ypaworld::CellCheckHealth(cellArea *cell, int newOwner, NC_STACK_ypabact *attacker)
{
    if ( cell->IsGamePlaySector() )
    {
        int helth = 0;

        for (auto h : cell->buildings_health)
            helth += h;

        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
        {
            if ( helth )
            {
                auto psIt = _powerStations.find(cell->Id);
                if (psIt != _powerStations.end())
                {
                    TPowerStationInfo &psInf = psIt->second;
                    int effPower = (helth * psInf.Power) / 255 ;

                    if ( effPower < 0 )
                        effPower = 0;
                    else if ( effPower > 255 )
                        effPower = 255;

                    psInf.EffectivePower = effPower;
                }
            }
            else
            {
                PowerStationErase(cell);
            }
        }

        /* Fix for vanilla game bug that recalc sector owner after loading
         * saved game. */
        if (newOwner != World::OWNER_NOCHANGE)
        {
            if ( cell->SectorType == 1 )
            {
                if ( helth < 224 )
                    CellSetNewOwner(cell, attacker, newOwner);
            }
            else if ( helth < 1728 )
            {
                CellSetNewOwner(cell, attacker, newOwner);
            }
        }
    }
    else
    {
        CellSetOwner(cell, 0);
    }
}



TSectorCollision NC_STACK_ypaworld::GetSectorCollision(int _dx, int _dz, int _dxx, int _dzz, int flags)
{
    int effFlags = flags;
    
    TSectorCollision tmp;
    tmp.sklt = NULL;
    tmp.Flags = 0;
    tmp.CollisionType = 0;
    
    

    if ( _dxx > 0 && _dxx < 4 * _mapSize.x - 1 && _dzz > 0  &&  _dzz < 4 * _mapSize.y - 1)
    {
        tmp.Cell = Common::Point(_dxx / 4, _dzz / 4);
        
        cellArea &cell = _cells(tmp.Cell);

        if ( _dxx % 4 && _dzz % 4 )
        {
            tmp.CollisionType = 1;

            int bldY, bldX;

            if ( cell.SectorType == 1 )
            {
                bldY = 0;
                bldX = 0;

                if ( (_dxx / 4) == (_dx / 4)  &&  (_dz / 4) == (_dzz / 4) )
                    effFlags = flags & ~1;

                tmp.pos = World::SectorIDToCenterPos3(tmp.Cell);
                tmp.pos.y = cell.height;
            }
            else
            {
                bldX = (_dxx % 4) - 1;
                bldY = 2 - ((_dzz % 4) - 1);

                if ( _dxx == _dx && _dzz == _dz )
                    effFlags = flags & ~1;

                tmp.pos.z = -(_dzz * 300.0);
                tmp.pos.x = _dxx * 300.0;
                tmp.pos.y = cell.height;
            }

            tmp.Flags = effFlags;

            int model_id = GetLegoBld(&cell, bldX, bldY);

            if ( effFlags & 1 )
                tmp.sklt = _legoArray[model_id].UseCollisionSkelet;
            else
                tmp.sklt = _legoArray[model_id].CollisionSkelet;
        }
        else
        {
            tmp.pos.y = 0;
            tmp.pos.x = _dxx * 300.0;
            tmp.pos.z = -(_dzz * 300.0);

            if ( _dxx == _dx && _dzz == _dz )
                effFlags = flags & ~1;

            tmp.Flags = effFlags;

            if ( _dxx % 4 == 0 && _dzz % 4 == 0)
            {
                tmp.sklt = _fillerCross;
                tmp.CollisionType = 4;
            }
            else if ( _dxx % 4 == 0 && _dzz % 4 != 0 )
            {
                tmp.sklt = _fillerSide;
                tmp.CollisionType = 2;
            }
            else if ( _dxx % 4 != 0 && _dzz % 4 == 0 )
            {
                tmp.sklt = _fillerSide;
                tmp.CollisionType = 3;
            }
        }

        if ( tmp.CollisionType && !tmp.sklt )
        {
            ypa_log_out("yw_GetSklt: WARNING, not CZT_INVALID, but Sklt NULL!\n");

            const char *typeName = "UNKNOWN";

            switch ( tmp.CollisionType )
            {
            case 4:
                typeName = "czt_cross_slurp";
                break;
            case 2:
                typeName = "czt_vside_slurp";
                break;
            case 3:
                typeName = "czt_hside_slurp";
                break;
            case 1:
                typeName = "czt_lego";
                break;
            default:
                break;
            }
            ypa_log_out("    Type=%s, sec_x=%d, sec_y=%d.\n", typeName, tmp.Cell.x, tmp.Cell.y);

            tmp.CollisionType = 0;
        }
    }
    
    return tmp;
}

// TODO(cleanup): Duplicates CalcPolygonPlane in sklt.cpp.
void RecalcCollisionPlane(UAskeleton::Data *arg, int id)
{
    UAskeleton::Polygon &tr = arg->polygons[id];

    int vtx1 = arg->polygons[id].v[0];
    int vtx2 = arg->polygons[id].v[1];
    int vtx3 = arg->polygons[id].v[2];

    vec3d tmp  = arg->POO[vtx2] - arg->POO[vtx1];
    vec3d tmp2 = arg->POO[vtx3] - arg->POO[vtx2];

    vec3d tmp3 = tmp * tmp2;
    tmp3.normalise();

    tr.A = tmp3.x;
    tr.B = tmp3.y;
    tr.C = tmp3.z;

    tr.D = -tmp3.dot( arg->POO[vtx1] );
}

void NC_STACK_ypaworld::FitFillerCollisionHeights(TSectorCollision &arg)
{
    if ( arg.CollisionType == 2 )
    {
        cellArea &cur = _cells(arg.Cell);
        cellArea &left = _cells(arg.Cell + Common::Point(-1, 0));

        if ( !(arg.Flags & 1) || fabs( (int)(cur.height - left.height)) < 500.0 )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = left.height;
            skel->POO[1].y = cur.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = left.height;

            RecalcCollisionPlane(skel, 0);
        }
        else
        {
            arg.sklt = _colsubSkeleton;

            if ( cur.height > left.height )
                arg.pos.y = cur.height;
            else
                arg.pos.y = left.height;
        }
    }
    else if ( arg.CollisionType == 3 )
    {
        cellArea &cur = _cells(arg.Cell);
        cellArea &up = _cells(arg.Cell + Common::Point(0, -1));

        if ( !(arg.Flags & 1) || fabs( (int)(cur.height - up.height)) < 500.0 )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = up.height;
            skel->POO[1].y = up.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = cur.height;

            RecalcCollisionPlane(skel, 0);
        }
        else
        {
            arg.sklt = _colsubSkeleton;

            if ( cur.height > up.height )
                arg.pos.y = cur.height;
            else
                arg.pos.y = up.height;
        }
    }
    else if ( arg.CollisionType == 4 )
    {

        int kk = 0;

        cellArea &cur = _cells(arg.Cell);
        cellArea &left = _cells(arg.Cell + Common::Point(-1, 0));
        cellArea &up = _cells(arg.Cell + Common::Point(0, -1));
        cellArea &leftup = _cells(arg.Cell + Common::Point(-1, -1));

        if ( arg.Flags & 1 )
        {
            float cs = cur.height;
            float ls = left.height;
            float us = up.height;
            float lus = leftup.height;

            float minCurLeft, minHeight, maxCurLeft, maxHeight;

            if ( cs >= ls )
                minCurLeft = ls;
            else
                minCurLeft = cs;

            if ( us >= lus )
                minHeight = lus;
            else
                minHeight = us;

            if ( minCurLeft < minHeight )
                minHeight = minCurLeft;

            if ( cs <= ls )
                maxCurLeft = ls;
            else
                maxCurLeft = cs;

            if ( us <= lus )
                maxHeight = lus;
            else
                maxHeight = us;

            if ( maxCurLeft > maxHeight )
                maxHeight = maxCurLeft;

            if ( fabs( (int)(maxHeight - minHeight) ) > 300.0 )
            {
                arg.sklt = _colsubSkeleton;
                arg.pos.y = maxHeight;
                kk = 1;
            }
        }
        if ( !kk )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = leftup.height;
            skel->POO[1].y = up.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = left.height;
            skel->POO[4].y = cur.averg_height;

            RecalcCollisionPlane(skel, 0);
            RecalcCollisionPlane(skel, 1);
            RecalcCollisionPlane(skel, 2);
            RecalcCollisionPlane(skel, 3);
        }
    }
}

int IsPointInSkeletonPolygon(const vec3d &v, int id, NC_STACK_skeleton *skeleton)
{
    UAskeleton::Data *sklt = skeleton->GetSkelet();
    
    int inside = 0;

    const UAskeleton::Polygon &tr = sklt->polygons[id];
    float nX = fabs(tr.A);
    float nY = fabs(tr.B);
    float nZ = fabs(tr.C);

    float maxAx = (nX <= nY ? nY : nX );
    maxAx = (maxAx <= nZ ? nZ : maxAx);

    if ( maxAx == nX )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.z <= v.z && v.z < cur.z) ||
                    (cur.z <= v.z && v.z < prv.z) ) &&
                    prv.y + (cur.y - prv.y) * (v.z - prv.z) / (cur.z - prv.z) > v.y )
            {
                inside = inside == 0;
            }

            prev = i;
        }
    }
    else if ( maxAx == nY )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.z <= v.z && v.z < cur.z) ||
                    (cur.z <= v.z && v.z < prv.z) ) &&
                    prv.x + (cur.x - prv.x) * (v.z - prv.z) / (cur.z - prv.z) > v.x )
            {
                inside = inside == 0;
            }

            prev = i;
        }
    }
    else if ( maxAx == nZ )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.y <= v.y && v.y < cur.y) ||
                    (cur.y <= v.y && v.y < prv.y) ) &&
                    prv.x + (cur.x - prv.x) * (v.y - prv.y) / (cur.y - prv.y) > v.x )
            {
                inside = inside == 0;
            }

            prev = i;
        }
    }
    return inside;
}

void NC_STACK_ypaworld::IntersectRaySkeleton(TRayIntersection *arg, const TSectorCollision &loc)
{
    UAskeleton::Data *skel = loc.sklt->GetSkelet();
    for ( size_t i = 0; i < skel->polygons.size(); i++)
    {
        UAskeleton::Polygon &tr = skel->polygons[i];
        vec3d norm = tr.Normal();

        float normDotDir = norm.dot(arg->vect);
        if ( normDotDir > 0.0 )
        {
            float t = -(norm.dot( arg->stPos ) + tr.D) / normDotDir;
            if ( t > 0.0 && t <= 1.0 && t < arg->tVal )
            {
                vec3d px = arg->vect * t + arg->stPos;

                if ( IsPointInSkeletonPolygon(px, i, loc.sklt) )
                {
                    arg->isect = 1;
                    arg->tVal = t;
                    arg->isectPos = loc.pos + px;
                    arg->polyID = i;
                    arg->skel = loc.sklt->GetSkelet();
                }
            }
        }
    }
}


void NC_STACK_ypaworld::FFeedback_VehicleChanged()
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }

    if ( UserUnit() )
    {
        _ffTimeStamp = _timeStamp;

        Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_ALL);

        int effectType;
        float massMax;
        float rotMax;
        float rotMin;
        float massMin;
        float damperMin;
        float periodMin;
        float magnitudeMax;
        float periodMax;
        float magnitudeMin;
        float damperMax;


        switch ( UserUnit()->_bact_type )
        {
        case BACT_TYPES_BACT:
            effectType = Input::FF_TYPE_HELIENGINE;
            massMin = 300.0;
            massMax = 800.0;
            rotMin = 1.0;
            rotMax = 2.0;
            damperMax = 1.0;
            periodMin = 0.0;
            damperMin = 0.7;
            magnitudeMin = 0.3;
            periodMax = 1.0;
            magnitudeMax = 0.5;
            break;

        case BACT_TYPES_TANK:
        case BACT_TYPES_CAR:
            effectType = Input::FF_TYPE_TANKENGINE;
            massMin = 200.0;
            massMax = 500.0;
            rotMin = 0.6;
            rotMax = 1.0;
            damperMax = 1.0;
            periodMax = 1.0;
            damperMin = 0.1;
            magnitudeMin = 0.3;
            magnitudeMax = 0.4;
            periodMin = 0.0;
            break;

        case BACT_TYPES_FLYER:
            effectType = Input::FF_TYPE_JETENGINE;
            massMin = 200.0;
            massMax = 500.0;
            rotMin = 1.0;
            rotMax = 2.0;
            damperMax = 1.0;
            periodMin = 0.0;
            damperMin = 0.1;
            magnitudeMin = 0.3;
            periodMax = 1.0;
            magnitudeMax = 0.75;
            break;

        default:
            effectType = -1;
            break;
        }

        if ( effectType != -1 )
        {
            float massFactor = (UserUnit()->_mass - massMin) / (massMax - massMin);
            float rotFactor = (UserUnit()->_maxrot - rotMin) / (rotMax - rotMin);

            float magnitude = (magnitudeMax - magnitudeMin) * massFactor + magnitudeMin;
            float period = (periodMin - periodMax) * massFactor + periodMax;

            float damper = (damperMin - damperMax) * rotFactor + damperMax;

            if ( magnitude < magnitudeMin)
                magnitude = magnitudeMin;
            else if (magnitude > magnitudeMax)
                magnitude = magnitudeMax;

            if ( period < periodMin )
                period = periodMin;
            else if ( period > periodMax)
                period = periodMax;

            if ( damper < damperMin)
                damper = damperMin;
            else if ( damper > damperMax)
                damper = damperMax;

            _ffEffectType = effectType;
            _ffPeriod = period;
            _ffMagnitude = magnitude;

            Input::Engine.ForceFeedback(Input::FF_STATE_START, effectType, magnitude, period);
            Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_ROTDAMPER, damper);
        }
    }
}




// The class of each unit type, BACT_TYPES_*
static const std::array<const std::string, 11> unit_classes_names
{
    "dummy.class",      // 0
    "ypabact.class",    // 1
    "ypatank.class",    // 2
    "yparobo.class",    // 3
    "ypamissile.class", // 4
    "ypazepp.class",    // 5
    "ypaflyer.class",   // 6
    "ypaufo.class",     // 7
    "ypacar.class",     // 8
    "ypagun.class",     // 9
    "ypahovercraft.class" //10
};

NC_STACK_ypabact *NC_STACK_ypaworld::yw_createUnit( int model_id)
{
    NC_STACK_ypabact *bacto = NULL;

    // Reuse a dead unit of the same class: Renew starts it as a new one.
    // Except host stations, which Renew doesn't start over in full: they
    // are few, and new ones come only with the level.
    if ( model_id != BACT_TYPES_ROBO )
    {
        for ( NC_STACK_ypabact * &unit : _deadCacheList )
        {
            if (unit->_bact_type == model_id)
            {
                bacto = unit;
                break;
            }
        }
    }

    // Take the unit out of the cache right away, like a newly created one.
    // Setting the new unit's state can create further units (destruction and
    // creation effects) before the caller puts it into a list, and those must
    // not get this same object again: it would be reinitialized while its own
    // effects are being started.
    if ( bacto )
        bacto->_kidRef.Detach();

    if ( !bacto )
    {
        bacto = Nucleus::CTFInit<NC_STACK_ypabact>(unit_classes_names[model_id], 
            {{NC_STACK_ypabact::BACT_ATT_WORLD, this}} );

        if ( !bacto )
            return NULL;
    }

    bacto->Renew();

    bacto->_gid = _nextUnitId;
    _nextUnitId++;

    return bacto;
}

NC_STACK_ypabact *NC_STACK_ypaworld::NewUnitObject(int type)
{
    if ( type <= 0 || type >= (int)unit_classes_names.size() )
        return NULL;

    return yw_createUnit(type);
}

NC_STACK_ypabact *NC_STACK_ypaworld::MakeUnitObject(int type)
{
    if ( type <= 0 || type >= (int)unit_classes_names.size() )
        return NULL;

    return Nucleus::CTFInit<NC_STACK_ypabact>(unit_classes_names[type], {{NC_STACK_ypabact::BACT_ATT_WORLD, this}} );
}




void NC_STACK_ypaworld::CheckSectorVisibility(Common::Point sect, TRenderingSector *sct, const World::Frame &frame, baseRender_msg *rndrParams)
{
    sct->Visible = 0;
    sct->InMap = 0;

    size_t index = sect.y * _mapSize.x + sect.x;

    if ( IsSector( sect ) && frame.Sectors && index < frame.Sectors->size() )
    {
        sct->InMap = 1;
        sct->Cell = &_cells( sect );
        sct->Drawn = &frame.Sectors->at(index);
        sct->Construction = frame.FindConstruction(index);

        vec3d pos = World::SectorIDToCenterPos3( sect );
        pos.y = sct->Cell->height;

        _beeBox->SetPosition(pos);

        if ( _beeBox->Render(rndrParams, NULL) )
        {
            sct->Visible = 1;
        }
    }
}

// What a model's drawing keeps from one frame to the next, made anew for
// another model
static NC_STACK_base::Instance *Drawing(std::unique_ptr<NC_STACK_base::Instance> &drawing, NC_STACK_base *model)
{
    if ( !drawing || drawing->Bas != model )
        drawing.reset(model->GenRenderInstance());

    return drawing.get();
}

void NC_STACK_ypaworld::RenderSector(TRenderingSector *sct, const World::Frame &frame, baseRender_msg *rndrParams)
{
    if ( sct->Visible )
    {
        const World::Frame::Sector &drawn = *sct->Drawn;
        const World::Frame::Construction *construction = sct->Construction;
        int32_t typeId = DrawnSectorType(drawn, construction);

        // A building being built grows
        bool constructing = construction && construction->End > 0;
        vec3d scel = vec3d::OY(constructing ? (float)construction->Time / (float)construction->End : 1.0);

        int subOffset, subCount;

        if ( _secTypeArray[ typeId ].SectorType == 1 )
        {
            subOffset = 0;
            subCount = 1;
        }
        else
        {
            subOffset = -1;
            subCount = 3;
        }

        size_t cellIndex = sct->Cell->CellId.y * _mapSize.x + sct->Cell->CellId.x;

        if ( _buildingDrawings.size() < _cells.size() * 9 )
            _buildingDrawings.resize(_cells.size() * 9);

        for (int zz = 0; zz < subCount; zz++)
        {
            for (int xx = 0; xx < subCount; xx++)
            {
                vec3d pos = sct->Cell->CenterPos + vec3d((subOffset + xx) * 300.0, 0.0, (subOffset + zz) * 300.0);
                std::unique_ptr<NC_STACK_base::Instance> &drawing = _buildingDrawings[cellIndex * 9 + xx + zz * 3];
                TSubSectorDesc *ssec = _secTypeArray[ typeId ].SubSectors.At(xx, zz);

                if ( constructing )
                {
                    NC_STACK_base *bld = _legoArray[ ssec->HPModels[0] ].Base;

                    bld->SetStatic(false);

                    bld->SetScale(scel, NC_STACK_base::UF_Y); //Scale only Y
                    bld->SetPosition(pos);

                    bld->Render(rndrParams, Drawing(drawing, bld));

                    bld->SetStatic(true);
                }
                else
                {
                    int16_t health = drawn.Health[xx + zz * 3];
                    NC_STACK_base *bld = _legoArray[ ssec->HPModels[ _buildHealthModelId[ health ] ] ].Base;

                    bld->SetPosition(pos);

                    bld->Render(rndrParams, Drawing(drawing, bld));
                }
            }
        }
    }

    size_t cellIndex = sct->Cell->CellId.y * _mapSize.x + sct->Cell->CellId.x;

    for (int32_t i = _drawnUnitFirst.at(cellIndex); i >= 0; i = _drawnUnitNext[i])
    {
        const World::Frame::Unit &unit = frame.Units[i];

        if ( sct->Visible || unit.Type == BACT_TYPES_ROBO )
        {
            RenderUnit(frame, unit, rndrParams);
            System::Perf.Count(System::PerfStats::CNT_UNITS);
        }
    }
}

void NC_STACK_ypaworld::RenderUnit(const World::Frame &frame, const World::Frame::Unit &unit, baseRender_msg *arg)
{
    TUnitDrawing &drawing = _unitDrawings[unit.Id];
    NC_STACK_base *model = UnitModel(unit.Type, unit.Vehicle, unit.Look);

    // Not the unit the player sees from, but a host station's deck
    if ( model && !(unit.StatusFlags & BACT_STFLAG_NORENDER) &&
         (unit.Id != frame.Viewer || unit.Type == BACT_TYPES_ROBO) )
    {
        model->TForm().Pos = vec3d(unit.Position);

        if ( unit.StatusFlags & BACT_STFLAG_SCALE )
            model->TForm().SclRot = mat3x3(unit.Rotation).Transpose() * mat3x3::Scale( vec3d(unit.Scale) );
        else
            model->TForm().SclRot = mat3x3(unit.Rotation).Transpose();

        model->Render(arg, Drawing(drawing.Model, model));
    }

    auto it = std::lower_bound(frame.Extras.begin(), frame.Extras.end(), unit.Id,
                               [](const World::Frame::Extra &e, uint32_t id) { return e.Unit < id; });

    for (; it != frame.Extras.end() && it->Unit == unit.Id; it++)
    {
        NC_STACK_base *extra = UnitModel(unit.Type, unit.Vehicle, it->Model);
        if ( !extra || it->Slot >= 3 )
            continue;

        extra->TForm().Pos = vec3d(it->Position);

        if ( it->Flags & EVPROTO_FLAG_SCALE )
            extra->TForm().SclRot = mat3x3(it->Rotation).Transpose() * mat3x3::Scale( vec3d(it->Scale, it->Scale, it->Scale) );
        else
            extra->TForm().SclRot = mat3x3(it->Rotation).Transpose();

        extra->Render(arg, Drawing(drawing.Extras[it->Slot], extra));
    }
}

void NC_STACK_ypaworld::yw_renderSky(const vec3d &viewerPos, baseRender_msg *rndr_params)
{
    if ( _skyObject )
    {
        float savedMaxZ = rndr_params->maxZ;
        uint32_t flags = rndr_params->flags;
        bool fadeOut = rndr_params->fadeOut;

        _skyObject->SetPosition( viewerPos + vec3d::OY(_skyHeight) );

        rndr_params->maxZ = 32000.0;
        rndr_params->flags = GFX::RFLAGS_SKY | GFX::RFLAGS_COMPUTED_COLOR;
        rndr_params->fadeOut = false; // Things fade into the sky, not the sky itself

        _skyObject->Render(rndr_params, NULL);

        rndr_params->maxZ = savedMaxZ;
        rndr_params->flags = flags;
        rndr_params->fadeOut = fadeOut;
    }
}


bool NC_STACK_ypaworld::IsVisibleMapPos(vec2d pos, const Common::Point &viewerSector)
{
    Common::Point pt( ((pos.x + 150) / 300) / 4, ((-pos.y + 150) / 300) / 4 );

    if ( !IsGamePlaySector( pt ) )
        return false;

    int radius = std::min(GetViewDistance().SectorRadius, GetMapSectorRadius());

    Common::Point dist = viewerSector.AbsDistance( pt );
    if ( dist.x + dist.y <= radius )
        return true;

    return false;
}

void NC_STACK_ypaworld::RenderSuperWave(vec2d pos, vec2d fromPos, const Common::Point &viewerSector, baseRender_msg *arg)
{
    if ( !_stoudsonWaveVehicleId )
        return;
    
    if ( pos.x > 0.0 && pos.y < 0.0 && pos.x < _mapLength.x && -_mapLength.y < pos.y )
    {
        if ( IsVisibleMapPos(pos, viewerSector) )
        {
            int vpId = _vhclProtos[_stoudsonWaveVehicleId].vp_normal;

            NC_STACK_base *wall_base = _vhclModels.at(vpId);

            if ( wall_base )
            {
                float groundY = 0.0;

                int subX = (pos.x + 150) / 300;
                int subY = (-pos.y + 150) / 300;

                if ( (subX & 3) && (subY & 3) )
                {
                    groundY = _cells((subX / 4), (subY / 4)).height;
                }
                else
                {
                    TRayIntersection ray;
                    ray.vect = vec3d::OY(50000.0);
                    ray.stPos.x = pos.x;
                    ray.stPos.y = -25000.0;
                    ray.stPos.z = pos.y;
                    ray.flags = 0;

                    IntersectRay(&ray);

                    if ( ray.isect )
                    {
                        groundY = ray.isectPos.y;
                    }
                }


                wall_base->TForm().Pos = vec3d(pos.x, groundY, pos.y);

                vec2d delt = pos - fromPos;
                delt.normalise();

                wall_base->TForm().SclRot =  mat3x3(delt.y,   0, -delt.x,
                                                      0, 1.0,    0,
                                                    delt.x, 0.0,  delt.y);

                wall_base->Render(arg, NULL);
            }
        }
    }
}


void NC_STACK_ypaworld::RenderSuperItems(const World::Frame &frame, const Common::Point &viewerSector, baseRender_msg *arg)
{
    // Render super items
    for (size_t i = 0; i < frame.SuperItems.size() && i < _levelInfo.SuperItems.size(); i++)
    {
        const World::Frame::SuperItem &sitem = frame.SuperItems[i];

        if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
        {
            vec2d pos = World::SectorIDToCenterPos2( _levelInfo.SuperItems[i].CellId );

            float maxRadius = sqrt( POW2(_mapLength.x) + POW2(_mapLength.y) );

            if ( sitem.Radius > 300 && sitem.Radius < maxRadius )
            {
                float wallCount = (2 * sitem.Radius) * C_PI / 300.0;

                if ( wallCount > 2.0 )
                {
                    float angleStep = 6.283 / wallCount;

                    for (float j = 0.0; j < 6.283; j = j + angleStep )
                    {
                        vec2d wallpos = vec2d(cos(j), sin(j)) * sitem.Radius + pos;
                        RenderSuperWave(wallpos, pos, viewerSector, arg);
                    }
                }
            }
        }
    }
}

void NC_STACK_ypaworld::PrepareFiller(cellArea *sct, cellArea *sct2, int32_t type, int32_t type2, float startCornerHeight, float endCornerHeight, bool vertical, TCellFillerCh *out, bool force)
{
    int x = _secTypeArray[ type ].SurfaceType;
    int y = _secTypeArray[ type2 ].SurfaceType;
    
    if (!force && (out->Id1 == x && out->Id2 == y && 
        out->Heights[0] == sct->height && out->Heights[1] == sct2->height && 
        out->Heights[2] == endCornerHeight && out->Heights[3] == startCornerHeight))
        return;

    NC_STACK_base *bs;
    if (vertical)
        bs = _fillersVertical(x, y);
    else
        bs = _fillersHorizontal(x, y);
    
    UAskeleton::Data *skel = bs->GetSkeleton()->GetSkelet();
    
    vec2d pos = World::SectorIDToCenterPos2( sct2->CellId );

    bs->SetPosition( vec3d::X0Z( pos ), NC_STACK_base::UF_XZ );

    for (int i = 0; i < 4; i++)
        skel->POO[i].y = sct->height;

    for (int i = 4; i < 8; i++)
        skel->POO[i].y = sct2->height;

    skel->POO[8].y = endCornerHeight;
    skel->POO[9].y = startCornerHeight;
    
    bs->RecalcInternal(true);
    bs->MakeCoordsCache();
    
    out->FreeVBO();
    
    bs->MakeCache(out);
    
    out->MakeVBO();
    
    out->Id1 = x;
    out->Id2 = y;
    out->Heights[0] = sct->height;
    out->Heights[1] = sct2->height;
    out->Heights[2] = endCornerHeight;
    out->Heights[3] = startCornerHeight;
}


void NC_STACK_ypaworld::PrepareAllFillers()
{
    for (int i = 0; i < _mapSize.x - 1; i++)
    {
        for (int j = 0; j < _mapSize.y - 2; j++)
        {
            cellArea *sct = &_cells(i, j);
            cellArea *sct2 = &_cells(i, j + 1);
            
            float h;
            if (i == _mapSize.x - 1)
                h = sct2->averg_height;
            else
                h = _cells(i + 1, j + 1).averg_height;
            
            PrepareFiller(sct, sct2, sct->type_id, sct2->type_id, sct2->averg_height, h, false, &_cellsHFCache(i, j), true);
        }
    }
    
    for (int i = 0; i < _mapSize.x - 2; i++)
    {
        for (int j = 0; j < _mapSize.y - 1; j++)
        {
            cellArea *sct = &_cells(i, j);
            cellArea *sct2 = &_cells(i + 1, j);
            
            float h;
            if (i == _mapSize.x - 1)
                h = sct2->averg_height;
            else
                h = _cells(i + 1, j + 1).averg_height;
            
            PrepareFiller(sct, sct2, sct->type_id, sct2->type_id, sct2->averg_height, h, true, &_cellsVFCache(i, j), true);
        }
    }
}




// The sectors around the viewer in this frame, one row and column more than
// the view's diameter: RenderFillers reads one past the visible ones.
static Common::PlaneVector<TRenderingSector> rendering_sectors;

void NC_STACK_ypaworld::RenderFillers(baseRender_msg *arg, int diameter)
{
    //Render landscape linking parts
    for (int i = 0; i < diameter; i++)
    {
        for (int j = 0; j < diameter - 1; j++)
        {
            TRenderingSector &sct = rendering_sectors(j, i);
            TRenderingSector &sct2 = rendering_sectors(j + 1, i);
            
            if (sct.InMap == 1 && sct2.InMap == 1 && (sct.Visible == 1 || sct2.Visible == 1))
            {
                float h;
                if (rendering_sectors(j + 1, i + 1).InMap == 1)
                    h = rendering_sectors(j + 1, i + 1).Cell->averg_height;
                else
                    h = sct2.Cell->averg_height;
                    
                TCellFillerCh &filler = _cellsVFCache( sct2.Cell->CellId.x - 1, sct2.Cell->CellId.y );
                PrepareFiller(sct.Cell, sct2.Cell, DrawnSectorType(*sct.Drawn, sct.Construction), DrawnSectorType(*sct2.Drawn, sct2.Construction),
                              sct2.Cell->averg_height, h, true, &filler);
                filler.Render(arg);
            }
        }
    }

    for (int i = 0; i < diameter - 1; i++)
    {
        for (int j = 0; j < diameter; j++)
        {
            TRenderingSector &sct = rendering_sectors(j, i);
            TRenderingSector &sct2 = rendering_sectors(j, i + 1);
            
            if (sct.InMap == 1 && sct2.InMap == 1 && (sct.Visible == 1 || sct2.Visible == 1))
            {
                float h;
                if (rendering_sectors(j + 1, i + 1).InMap == 1)
                    h = rendering_sectors(j + 1, i + 1).Cell->averg_height;
                else
                    h = sct2.Cell->averg_height;
                    
                TCellFillerCh &filler = _cellsHFCache( sct2.Cell->CellId.x, sct2.Cell->CellId.y - 1 );
                PrepareFiller(sct.Cell, sct2.Cell, DrawnSectorType(*sct.Drawn, sct.Construction), DrawnSectorType(*sct2.Drawn, sct2.Construction),
                              sct2.Cell->averg_height, h, false, &filler);
                filler.Render(arg);
            }
        }
    }
}

// Draws a frame of the world, with the interface over it as the game goes
// on, and with particles: not when going back.
void NC_STACK_ypaworld::RenderGame(TUpdateArgs *arg, const World::Frame &frame, bool withInterface, bool particles)
{
    uint64_t sceneStart = System::PerfStats::Now();

    PlaceDrawnCamera(frame);
    _drawnCamera.CalcGlobal();

    // The sectors drawn and the sky are placed around the viewer
    vec3d viewerPos = vec3d(frame.CameraPosition);

    if ( _cameraOverridden )
        viewerPos = _cameraOverridePos;
    else if ( const World::Frame::Unit *viewer = frame.FindUnit(frame.Viewer) )
        viewerPos = vec3d(viewer->Position);

    Common::Point viewerSector = World::PositionToSectorID(viewerPos);

    baseRender_msg rndrs;

    rndrs.flags = 0;
    rndrs.frameTime = arg->DTime;
    rndrs.globTime = frame.Time;
    rndrs.adeCount = 0;
    rndrs.emitParticles = particles;

    rndrs.minZ = 1.0;

    TViewDistance view = GetViewDistance();

    // However far the view reaches, the map ends. The far plane stays well
    // behind the clip distance, as objects that start before it are drawn
    // whole, and it never comes closer than the 8192 it always was.
    int radius = std::min(view.SectorRadius, GetMapSectorRadius());
    float farPlane = std::min(view.ClipDistance, GetMapDiagonal()) + 4 * World::CVSectorLength;
    farPlane = std::max(farPlane, 8192.0f);

    rndrs.maxZ = std::min(view.ClipDistance, farPlane);

    GFX::Engine.setFrustumClip(1.0, farPlane);

    if ( _viewDistanceSet )
    {
        rndrs.fogOverride = true;
        rndrs.noFog = (view.FogEnd == TViewDistance::UNLIMITED);
        rndrs.fogStart = view.FogStart;
        rndrs.fogLength = view.FogEnd - view.FogStart;
    }

    rndrs.fadeOut = (view.FadeOutEnd != TViewDistance::UNLIMITED);
    rndrs.fadeOutStart = view.FadeOutStart;
    rndrs.fadeOutLength = view.FadeOutEnd - view.FadeOutStart;

    int diameter = 2 * radius + 1;

    // Clear the whole grid, so that sectors off the map or outside the
    // diamond drawn below don't keep entries from an earlier frame.
    rendering_sectors.Resize(diameter + 1, diameter + 1);

    for (TRenderingSector &sct : rendering_sectors)
    {
        sct.InMap = 0;
        sct.Visible = 0;
    }

    // The frame's units by the sector they're in, each sector's in the
    // order of their ids
    _drawnUnitFirst.assign(_cells.size(), -1);
    _drawnUnitNext.assign(frame.Units.size(), -1);

    for (int32_t i = (int32_t)frame.Units.size() - 1; i >= 0; i--)
    {
        Common::Point sector = World::PositionToSectorID(frame.Units[i].Position.x, frame.Units[i].Position.z);

        if ( IsSector(sector) )
        {
            size_t index = sector.y * _mapSize.x + sector.x;
            _drawnUnitNext[i] = _drawnUnitFirst[index];
            _drawnUnitFirst[index] = i;
        }
    }

    for (int i = 0; i <= radius; i++)
    {
        int rowOffset = radius - i;

        for (int j = -i; j <= i; j++)
        {
            TRenderingSector *sct = &rendering_sectors(radius + j, radius - rowOffset);

            CheckSectorVisibility( viewerSector + Common::Point(j, -rowOffset), sct, frame, &rndrs);

            if ( sct->InMap )
                RenderSector(sct, frame, &rndrs);

        }

        if ( -rowOffset != rowOffset )
        {
            for (int j = -i; j <= i; j++)
            {
                TRenderingSector *sct = &rendering_sectors(radius + j, radius + rowOffset);

                CheckSectorVisibility( viewerSector + Common::Point(j, rowOffset), sct, frame, &rndrs);

                if ( sct->InMap )
                    RenderSector(sct, frame, &rndrs);
            }
        }
    }

    
    RenderSuperItems(frame, viewerSector, &rndrs);

    RenderFillers(&rndrs, diameter);

    arg->AdeCount = rndrs.adeCount;

    _polysCount = rndrs.adeCount;
    _polysDraw = 7777;


    TObjectRenderParams rrg;
    rrg.timeStamp = frame.Time;
    rrg.frameTime = arg->DTime;
    rrg.minZ = 1.0;
    rrg.maxZ = rndrs.maxZ;
    rrg.ViewTForm = TF::Engine.GetViewPoint();
    rrg.OwnerTForm = NULL;
    rrg.flags = 0;


    ParticleSystem().UpdateRender(&rrg, arg->DTime);

    GFX::Engine.BeginScene();
    
    if ( _skyRender )
        yw_renderSky(viewerPos, &rndrs);

    System::Perf.AddTime(System::PerfStats::SEC_SCENE, System::PerfStats::Now() - sceneStart);

    GFX::Engine.Rasterize();

    GFX::Engine.EndScene();

    // TODO(bug): The interface's markers, map icons and lines are placed from
    // the units as the last step left them, not from the frame, so they can
    // lag the world drawn by up to one step.
    if ( withInterface )
    {
        System::PerfStats::Timer perf(System::PerfStats::SEC_GUI);
        uint32_t tpm = profiler_begin();
        DrawGameInterface(this);
        _profileVals[PFID_NEWGUITIME] = profiler_end(tpm);
    }
}


void NC_STACK_ypaworld::ResetAccumMap()
{
    for( int y = 0; y < _mapSize.y; y++ )
    {
        for( int x = 0; x < _mapSize.x; x++ )
        {
            EnergyAccum &accum = _energyAccumMap(x, y);
            accum.Energy = 0;
            accum.Owner = _cells(x, y).owner;
        }
    }

    _nextPSForUpdate = 0;
}

void NC_STACK_ypaworld::SetupPowerStationInfo(cellArea *cell, int power)
{
    if (!cell) return;
    
    TPowerStationInfo &ps = _powerStations[cell->Id];

    ps.CellId = cell->CellId;
    ps.Power = power;
    ps.EffectivePower = power;
    ps.pCell = cell;
    
    cell->PurposeType = cellArea::PT_POWERSTATION;
    cell->PurposeIndex = 0;
    
    ResetAccumMap();
}


void NC_STACK_ypaworld::PlaceBuilding(const Common::Point &cellId, int ownerid2, int blg_id, int skipGuns)
{
    uamessage_bldVhcl bvMsg;

    cellArea &cell = _cells(cellId);
    World::TBuildingProto *bld = &_buildProtos[ blg_id ];
    TSectorDesc *sectp = &_secTypeArray[ bld->SecType ];

    int isLocal = 1;

    NC_STACK_yparobo *robo = NULL;

    if ( cell.IsGamePlaySector() )
    {
        _lvlBuildingsMap(cellId) = blg_id;
        _lvlTypeMap(cellId) = bld->SecType;

        cell.type_id = bld->SecType;
        cell.energy_power = 0;
        cell.PurposeType = cellArea::PT_BUILDINGS;
        cell.SectorType = sectp->SectorType;
        cell.PurposeIndex = blg_id;

        int subCount;

        if ( sectp->SectorType == 1 )
        {
            cell.buildings_health.fill(0);
            
            subCount = 1;
        }
        else
        {
            subCount = 3;
        }

        for (int yy = 0; yy < subCount; yy++)
        {
            for (int xx = 0; xx < subCount; xx++)
                cell.buildings_health.At(xx, yy) = sectp->SubSectors.At(xx, yy)->StartHealth;
        }

        if ( bld->ModelID == 1 )
            SetupPowerStationInfo(&cell, bld->Power);

        CellSetOwner(&cell, ownerid2);

        for( NC_STACK_ypabact * &unit: _unitsList )
        {
            if (unit->_bact_type == BACT_TYPES_ROBO && unit->_owner == ownerid2)
            {
                robo = dynamic_cast<NC_STACK_yparobo *>(unit);
                break;
            }
        }

        if ( _isNetGame )
        {
            if ( robo != UserRobo() )
                isLocal = 0;
        }

        if ( !skipGuns )
        {
            if ( robo && robo->_status != BACT_STATUS_DEAD && isLocal )
            {
                NC_STACK_ypagun *commander = NULL;

                // TODO(bug): The guns take the id after the next one and leave
                // it as the next, so the next new squad gets the same id
                _nextCommandId++;
                int commandId = _nextCommandId;

                for ( size_t i = 0; i < bld->Guns.size(); i++)
                {
                    World::TBuildingProto::TGun &GunProto = bld->Guns[i];
                    
                    if ( !GunProto.VhclID )
                        break;

                    TUnitSpawn spawn;
                    spawn.vehicle_id = GunProto.VhclID;
                    spawn.pos = GunProto.Pos + World::SectorIDToCenterPos3( cellId );

                    NC_STACK_ypabact *gun_obj = CreateVehicle(&spawn);
                    NC_STACK_ypagun *gunn = dynamic_cast<NC_STACK_ypagun *>(gun_obj);

                    if ( gun_obj )
                    {
                        gun_obj->_owner = ownerid2;

                        if (gunn)
                            gunn->SetGunBasis(GunProto.Dir, false);

                        setState_msg stateMsg;
                        stateMsg.newStatus = BACT_STATUS_CREATE;
                        stateMsg.unsetFlags = 0;
                        stateMsg.setFlags = 0;

                        gun_obj->SetStateInternal(&stateMsg);

                        gun_obj->_scale_time = 500;
                        gun_obj->_scale = vec3d(1.0, 1.0, 1.0);

                        gun_obj->_host_station = robo;
                        gun_obj->_commandID = commandId;

                        if ( _isNetGame && i < 8 )
                        {
                            gun_obj->_gid |= ownerid2 << 24;

                            bvMsg.vhcl[i].id = gun_obj->_gid;
                            bvMsg.vhcl[i].base = GunProto.Dir;
                            bvMsg.vhcl[i].pos = gun_obj->_position;
                            bvMsg.vhcl[i].protoID = GunProto.VhclID;
                        }

                        if ( commander )
                        {
                            commander->AddSubject(gun_obj);
                        }
                        else
                        {
                            commander = gunn;

                            robo->AddSubject(gun_obj);
                        }
                    }
                }

                if ( _isNetGame )
                {
                    bvMsg.msgID = UAMSG_BUILDINGVHCL;
                    bvMsg.tstamp = _timeStamp;
                    bvMsg.owner = ownerid2;

                    NetBroadcastMessage(&bvMsg, sizeof(bvMsg), true);
                }
            }
        }
    }
}


void NC_STACK_ypaworld::DestroyAllGunsInSector(cellArea *cell)
{
    /*
     * Destroy all GUN units in sector     
     */
    
    // Safe iterator, because it will call ModifyEnergy->Die for units
    for ( NC_STACK_ypabact* itUnit : cell->unitsList.safe_iter() )
    {
        int destroy = 0;

        if ( _isNetGame )
        {
            // In netgame only destroy own units
            if ( itUnit->_owner == UserUnit()->_owner )
            {
                if ( itUnit->_status != BACT_STATUS_DEAD && itUnit->_status != BACT_STATUS_BEAM && itUnit->_status != BACT_STATUS_CREATE )
                {
                    if ( itUnit->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( itUnit );

                        if (!gun->IsRoboGun())
                            destroy = 1;
                    }
                }
            }
        }
        else
        {
            if ( itUnit->_status != BACT_STATUS_DEAD && itUnit->_status != BACT_STATUS_BEAM && itUnit->_status != BACT_STATUS_CREATE )
            {
                if ( itUnit->_bact_type == BACT_TYPES_GUN )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( itUnit );

                    if (!gun->IsRoboGun())
                        destroy = 1;
                }
            }
        }

        if ( destroy )
        {
            TEnergyChange energyChange;
            energyChange.energy = -22000000;
            energyChange.unit = NULL;

            itUnit->ModifyEnergy(&energyChange);
        }
    }
}

bool NC_STACK_ypaworld::BuildingConstructBegin(cellArea *cell, uint8_t buildID, int owner, int cTime)
{
    if (!cell)
        return false;
    
    if (cell->IsBorder())
        return false;
    
    auto it = _inBuildProcess.find(cell->Id);
    if (it != _inBuildProcess.end())
        return false;
    
    TConstructInfo &bldProc = _inBuildProcess[cell->Id];
    bldProc.CellID = cell->CellId;
    bldProc.Time = 0;
    bldProc.EndTime = cTime;
    bldProc.BuildID = buildID;
    bldProc.Owner = owner;
    
    cell->PurposeType = cellArea::PT_CONSTRUCTING;
    
    for (NC_STACK_ypabact * &unit : _unitsList)
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && owner == unit->_owner )
        {
            SFXEngine::SFXe.startSound(&unit->_soundcarrier, 11);
            break;
        }
    }
    
    return true;
}

int StepTowardPolygonCentroid(UAskeleton::Data *skl, int id, const vec3d &v, float r, vec3d &out)
{
    UAskeleton::Polygon &pg = skl->polygons[id];
    vec3d tmp(0.0, 0.0, 0.0);

    for (int i = 0; i < pg.num_vertices; i++)
    {
        int16_t idd = pg.v[i];
        tmp += static_cast<vec3d> (skl->POO[ idd ]);
    }

    vec3d tmp2 = tmp / pg.num_vertices - v;

    float dist = tmp2.length();

    if ( dist <= r )
        return 0;

    out = tmp2 * (r / dist) + v;

    return 1;
}

void NC_STACK_ypaworld::CollideSphereWithSkeleton(TSphereCollisionQuery *arg, const TSectorCollision &sectorColl)
{
    for (size_t i = 0; i < sectorColl.sklt->GetSkelet()->polygons.size(); i++)
    {
        const UAskeleton::Polygon &tria = sectorColl.sklt->GetSkelet()->polygons[i];

        vec3d t0 = tria.Normal();

        float normDotDir = t0.dot( arg->dir );

        if ( normDotDir > 0.0 )
        {
            float t = -( t0.dot( arg->pos ) + tria.D) / ( t0.dot( t0 ) * arg->radius);

            if ( t > 0.0 && t <= 1.0 )
            {
                vec3d tx = arg->pos + t0 * (arg->radius * t);

                int hit = 0;

                vec3d probePos;

                if ( StepTowardPolygonCentroid(sectorColl.sklt->GetSkelet(), i, tx, arg->radius, probePos) )
                {
                    if ( IsPointInSkeletonPolygon(probePos, i, sectorColl.sklt) )
                        hit = 1;
                }
                else
                    hit = 1;

                if ( hit )
                {
                    if ( arg->coll_count < arg->coll_max )
                    {
                        int pos = arg->coll_count;

                        arg->collisions[pos].pos1 = sectorColl.pos + tx;
                        arg->collisions[pos].normal = tria.Normal();

                        arg->coll_count++;
                    }
                }
            }
        }
    }
}

NC_STACK_ypabact * NC_STACK_ypaworld::FindBactByCmdOwn(uint32_t commandID, char owner)
{
    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        if ( robo->_bact_type == BACT_TYPES_ROBO && robo->_owner == owner)
        {
            if ( robo->_commandID == commandID )
            {
                if ( robo->_status == BACT_STATUS_DEAD )
                    return NULL;
                else
                    return robo;
            }
            else
            {
                for ( NC_STACK_ypabact * &unit : robo->_kidList )
                {
                    if ( unit->_commandID == commandID )
                    {
                        if ( unit->_status == BACT_STATUS_DEAD )
                            return NULL;
                        else
                            return unit;
                    }
                }
            }
        }
    }

    return NULL;
}


void NC_STACK_ypaworld::BuildingConstructUpdate(int dtime)
{
    for(auto it  = _inBuildProcess.begin(); it != _inBuildProcess.end(); )
    {
        TConstructInfo &bldProc = it->second;
        bldProc.Time += dtime;

        if ( bldProc.Time >= bldProc.EndTime )
        {
            cellArea &rCell = _cells( bldProc.CellID );
            rCell.PurposeType = cellArea::PT_NONE;
            rCell.PurposeIndex = 0;

            PlaceBuilding(bldProc.CellID, bldProc.Owner, bldProc.BuildID, 0);

            if ( bldProc.Owner == UserRobo()->_owner )
            {
                if ( _buildProtos[ bldProc.BuildID ].ModelID )
                {
                    TInfoMessage msg;

                    msg.unit = UserRobo();
                    msg.Priority = 65;

                    switch( _buildProtos[ bldProc.BuildID ].ModelID )
                    { 
                        case 1:
                        msg.MsgID = 36;
                        break;
                        
                        case 2:
                        msg.MsgID = 38;
                        break;
                    
                        case 3:
                        msg.MsgID = 37;
                        break;
                        
                        default:
                        msg.MsgID = 0;
                        break;
                    }

                    ShowMessage(&msg);
                }
            }
            it = _inBuildProcess.erase(it);
        }
        else
            ++it;
    }
}

bool NC_STACK_ypaworld::IsAnyBuildingProcess(int owner) const
{
    for(const auto &it : _inBuildProcess)
    {
        if (it.second.Owner == owner)
            return true;
    }
    return false;
}

void NC_STACK_ypaworld::CountUnitsPerOwner()
{
    for(int i = 0; i < 8; i++)
        _countUnitsPerOwner[i] = 0;

    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        if ( robo->_bact_type == BACT_TYPES_ROBO && robo->_status != BACT_STATUS_DEAD && robo->_status != BACT_STATUS_BEAM )
        {
            for( NC_STACK_ypabact * &comnd : robo->_kidList )
            {
                if ( comnd->_status != BACT_STATUS_DEAD && comnd->_status != BACT_STATUS_BEAM )
                {
                    bool isRoboGun = false;

                    if ( comnd->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( comnd );
                        isRoboGun = gun->IsRoboGun();
                    }

                    if ( !isRoboGun )
                    {
                        _countUnitsPerOwner[ comnd->_owner ]++;

                        for( NC_STACK_ypabact * &unit : comnd->_kidList )
                        {
                            if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM )
                                _countUnitsPerOwner[ unit->_owner ]++;
                        }
                    }
                }
            }
        }
    }
}


// How much of what the power under its host stations gives a faction gets:
// its sectors against half the power of its power stations, 0 without any,
// and over 1 when they don't power that many sectors
float NC_STACK_ypaworld::ReloadRatio(int sectors, int stationPower)
{
    stationPower /= 2;

    if ( stationPower <= 0 )
        return 0.0;

    return (float)std::max(sectors, 0) / (float)stationPower;
}

void UpdateReloadRatios(NC_STACK_ypaworld *yw)
{
    int ownerPower[8];

    for (int i = 0; i < 8; i++)
        ownerPower[i] = 0;

    for (const auto &ps : yw->_powerStations)
    {
        if (ps.second.pCell)
            ownerPower[ ps.second.pCell->owner ] += ps.second.EffectivePower;
    }

    yw->CountUnitsPerOwner();

    for (int i = 0; i < 8; i++)
    {
        yw->_reloadRatioClamped[i] = NC_STACK_ypaworld::ReloadRatio(yw->_countSectorsPerOwner[i], ownerPower[i]);
        yw->_reloadRatioPositive[i] = yw->_reloadRatioClamped[i];

        if ( yw->_isNetGame && yw->_levelUnitLimitType == 1 )
        {
            // TODO(bug): Every owner i is penalised by the local player's unit count, and
            // penalty is an int, so it stays 0 until it reaches 1 and then wipes
            // out the 0..1 reload ratio.
            int unitsOverLimit = yw->_countUnitsPerOwner[yw->UserRobo()->_owner] - yw->_levelUnitLimit;

            if ( unitsOverLimit > 0 )
            {
                int penalty = (float)yw->_levelUnitLimitArg * 0.01 * (float)unitsOverLimit;

                yw->_reloadRatioClamped[i] -= penalty;
                yw->_reloadRatioPositive[i] -= penalty;
            }
        }

        yw->_reloadRatioClamped[i] = std::min(std::max(yw->_reloadRatioClamped[i], 0.0f), 1.0f);
        yw->_reloadRatioPositive[i] = std::max(yw->_reloadRatioPositive[i], 0.0f);
    }
}


void NC_STACK_ypaworld::RecalcSectorsPowerForPS(const TPowerStationInfo &ps)
{
    int pwrTmp = ps.EffectivePower;
    int powsCount = 0;

    while (pwrTmp > 0)
    {
        pwrTmp >>= 1;
        powsCount++;
    }

    int sdx = -powsCount;
    int edx = powsCount + 1;

    int sdy = -powsCount;
    int edy = powsCount + 1;

    if ( ps.CellId.x + sdx < 1 )
        sdx = 1 - ps.CellId.x;

    if ( ps.CellId.y + sdy < 1 )
        sdy = 1 - ps.CellId.y;

    if ( ps.CellId.x + edx >= _mapSize.x )
        edx = _mapSize.x - ps.CellId.x - 1;

    if ( ps.CellId.y + edy >= _mapSize.y )
        edy = _mapSize.y - ps.CellId.y - 1;

    for (int dy = sdy; dy < edy; dy++)
    {
        for (int dx = sdx; dx < edx; dx++)
        {
            int cellPower = ps.EffectivePower  >>  _sqrtTable(abs(dx), abs(dy));

            EnergyAccum &accum = _energyAccumMap(dx + ps.CellId.x, dy + ps.CellId.y);

            if ( accum.Owner == ps.pCell->owner )
            {
                accum.Energy += cellPower; // Add power to this cell

                if ( accum.Energy > 255 )
                    accum.Energy = 255;
            }

        }
    }
}

void NC_STACK_ypaworld::DoSectorsEnergyRecalc()
{
    // Recompute power on sectors
    if ( !_powerStations.empty() ) // If we have powerstations
    {
        auto itPs = _powerStations.lower_bound(_nextPSForUpdate);
        
        if (itPs == _powerStations.end()) // If we reach end of power stations list, apply power to sectors
        {
            UpdatePowerEnergy(); // Apply power to sectors and clean power matrix for next compute iteration.
        }
        else
        {
            if ( itPs->second.EffectivePower ) // if this power station has power
                RecalcSectorsPowerForPS(itPs->second); // Add power to power matrix

            _nextPSForUpdate = itPs->first + 1; // go to next station in next update loop
        }
    }
}


void NC_STACK_ypaworld::ActivateSuperItem(int id, int resume)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.State = TMapSuperItem::STATE_ACTIVE;
    sitem.TriggerTime = 0;
    sitem.ActivateOwner = sitem.PCell->owner;

    if ( !resume )
    {
        sitem.ActiveTime = _timeStamp;
        sitem.LastTenSec = 0;
        sitem.LastSec = 0;
        sitem.CountDown = sitem.TimerValue;
    }

    TBuildingPlacement placement;
    placement.owner = sitem.PCell->owner;
    placement.blg_ID = sitem.ActiveBldID;
    placement.instant = 1;
    placement.CellId = sitem.CellId;
    placement.flags = 0;

    CreateBuilding(&placement);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    TInfoMessage msg;
    msg.unit = 0;
    msg.Priority = 94;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBACT);
        msg.MsgID = 70;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEACT);
        msg.MsgID = 74;
    }
    else
    {
        msg.MsgID = 0;
        msg.txt = "Cant happen.";
    }

    ShowMessage(&msg);
}

void NC_STACK_ypaworld::StopSuperItem(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.State = TMapSuperItem::STATE_STOPPED;

    TBuildingPlacement placement;
    placement.owner = sitem.PCell->owner;
    placement.blg_ID = sitem.InactiveBldID;
    placement.instant = 1;
    placement.CellId = sitem.CellId;
    placement.flags = 0;

    CreateBuilding(&placement);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    TInfoMessage msg;
    msg.unit = 0;
    msg.Priority = 93;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBFROZ);
        msg.MsgID = 72;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEFROZ);
        msg.MsgID = 76;
    }
    else
    {
        msg.MsgID = 0;
        msg.txt = "Cant happen.";
    }

    ShowMessage(&msg);
}

void NC_STACK_ypaworld::TriggerSuperItem(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    sitem.State = TMapSuperItem::STATE_TRIGGED;
    sitem.TriggerTime = _timeStamp;

    TBuildingPlacement placement;
    placement.owner = sitem.PCell->owner;
    placement.blg_ID = sitem.TriggerBldID;
    placement.instant = 1;
    placement.CellId = sitem.CellId;
    placement.flags = 0;

    CreateBuilding(&placement);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    sitem.LastRadius = 0;

    TInfoMessage msg;
    msg.Priority = 95;
    msg.unit = 0;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBTRIG);
        msg.MsgID = 71;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVETRIG);
        msg.MsgID = 75;
    }
    else
    {
        msg.MsgID = 0;
        msg.txt = "Cant happen.";
    }

    ShowMessage(&msg);
}


int HandlePause(NC_STACK_ypaworld *yw, TUpdateArgs *arg)
{
    if ( yw->_isNetGame )
        return 0;

    if ( !yw->_gamePaused )
    {
        if ( arg->Input->HotKeyID == 32 || arg->Input->KbdLastHit == Input::KC_PAUSE )
        {
            yw->_gamePaused = true;
            yw->_gamePausedTimeStamp = arg->TimeStamp;
        }
        return 0;
    }

    if ( arg->Input->KbdLastHit != Input::KC_NONE )
    {
        yw->_gamePaused = false;
        arg->TimeStamp = yw->_gamePausedTimeStamp;
    }
    else
    {
        GFX::Engine.BeginFrame();

        /*yw->_win3d->setRSTR_BGpen(0);

        yw->_win3d->raster_func192(NULL);*/

        vec3d listenerPos = yw->_viewerPosition + vec3d::OY(50000.0);

        SFXEngine::SFXe.BeginSoundFrame(1, listenerPos, vec3d(0.0), mat3x3::Ident());

        if ( arg->TimeStamp / 500 & 1 )
        {
            const std::string pausedText = Locale::Text::Common(Locale::CMN_PAUSED);

            CmdStream drawCmds;
            drawCmds.reserve(256);

            FontUA::select_tileset(&drawCmds, 15);

            FontUA::set_xpos(&drawCmds, 0);
            FontUA::set_center_ypos(&drawCmds, -yw->_fontH / 2);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &drawCmds, pausedText, yw->_screenSize.x);

            FontUA::set_end(&drawCmds);

            GFX::Engine.ProcessDrawSeq(drawCmds);
        }

        SFXEngine::SFXe.EndSoundFrame();

        GFX::Engine.EndFrame();
    }
    return 1;
}


void UpdatePlayerInRoboGun(NC_STACK_ypaworld *yw)
{
    yw->_playerInHSGun = false;

    if ( yw->UserRobo() != yw->UserUnit() )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->UserRobo());

        for (World::TRoboGun &gun : robo->GetGuns())
        {
            if ( yw->UserUnit() == gun.gun_obj )
                yw->_playerInHSGun = true;
        }
    }
}


void UpdateGates(NC_STACK_ypaworld *yw)
{
    for ( size_t i = 0; i < yw->_levelInfo.Gates.size(); i++ )
    {
        const TMapGate &gate = yw->_levelInfo.Gates[i];
        int gateState = cellArea::PT_GATEOPENED;

        if ( gate.PCell->owner == yw->UserRobo()->_owner )
        {
            for( const TMapKeySector &ks : gate.KeySectors )
            {
                if (ks.PCell)
                {
                    if (ks.PCell->owner != yw->UserRobo()->_owner)
                    {
                        gateState = cellArea::PT_GATECLOSED;
                        break;
                    }
                }
            }
        }
        else
        {
            gateState = cellArea::PT_GATECLOSED;
        }

        if ( gate.PCell->PurposeType != gateState )
        {
            TBuildingPlacement placement;
            placement.owner = gate.PCell->owner;
            placement.instant = 1;
            placement.CellId = gate.CellId;
            placement.flags = 0;

            if ( gateState == cellArea::PT_GATEOPENED )
            {
                placement.blg_ID = gate.OpenBldID;
            }
            else
            {
                placement.blg_ID = gate.ClosedBldID;

                TInfoMessage closedMsg;
                closedMsg.unit = 0;
                closedMsg.Priority = 65;
                closedMsg.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATECLOSE);
                closedMsg.MsgID = 24;

                yw->ShowMessage(&closedMsg);
            }

            yw->CreateBuilding(&placement);

            gate.PCell->PurposeType = gateState;
            gate.PCell->PurposeIndex = i;
        }

        if ( gateState == cellArea::PT_GATEOPENED )
        {
            int energ = 0;

            for ( NC_STACK_ypabact* &unit : gate.PCell->unitsList )
            {
                if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM )
                {
                    if ( unit->_bact_type != BACT_TYPES_ROBO && unit->_bact_type != BACT_TYPES_MISSLE && unit->_bact_type != BACT_TYPES_GUN )
                        energ += (unit->_energy_max + 99) / 100;
                }
            }

            if ( energ <= yw->_beamEnergyCapacity )
            {
                if ( yw->_timeStamp - yw->_msgTimestampGates > 60000 )
                {
                    TInfoMessage openMsg;
                    openMsg.unit = 0;
                    openMsg.Priority = 49;
                    openMsg.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATEOPEN);
                    openMsg.MsgID = 23;

                    yw->ShowMessage(&openMsg);
                    yw->_msgTimestampGates = yw->_timeStamp;
                }
            }
            else
            {
                if ( yw->_timeStamp - yw->_msgTimestampGates > 40000 )
                {
                    TInfoMessage fullMsg;
                    fullMsg.unit = 0;
                    fullMsg.Priority = 10;
                    fullMsg.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATEFULL);
                    fullMsg.MsgID = 46;

                    yw->ShowMessage(&fullMsg);
                    yw->_msgTimestampGates = yw->_timeStamp;
                }
            }
        }
    }
}


bool NC_STACK_ypaworld::IsSuperItemHeldBy(int id, int owner)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( sitem.PCell->owner != owner )
        return false;

    if ( sitem.KeySectors.empty() )
        return true;

    for ( const TMapKeySector &ks : sitem.KeySectors )
    {
        if ( ks.PCell->owner != owner )
            return false;
    }
    return true;
}

bool NC_STACK_ypaworld::OwnerHasHostStation(int owner)
{
    for ( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && owner == unit->_owner )
            return true;
    }

    return false;
}

void NC_STACK_ypaworld::DeactivateSuperItem(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.State = TMapSuperItem::STATE_INACTIVE;
    sitem.ActiveTime = 0;
    sitem.TriggerTime = 0;
    sitem.ActivateOwner = 0;
    sitem.CountDown = 0;

    TBuildingPlacement placement;
    placement.owner = sitem.PCell->owner;
    placement.blg_ID = sitem.InactiveBldID;
    placement.instant = 1;
    placement.CellId = sitem.CellId;
    placement.flags = 0;

    CreateBuilding(&placement);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    TInfoMessage msg;
    msg.unit = NULL;
    msg.Priority = 92;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBDEACT);
        msg.MsgID = 73;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEDEACT);
        msg.MsgID = 77;
    }
    else
    {
        msg.MsgID = 0;
        msg.txt = "Cant happen.";
    }

    ShowMessage(&msg);
}

void NC_STACK_ypaworld::UpdateActiveSuperItem(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    for( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && sitem.ActivateOwner == unit->_owner )
        {
            if ( IsSuperItemHeldBy(id, sitem.ActivateOwner) )
            {
                if ( sitem.CountDown > 0 )
                    sitem.CountDown = sitem.CountDown - _frameTime;
                else
                    TriggerSuperItem(id);
            }
            else if ( !sitem.KeySectors.empty() )
            {
                StopSuperItem(id);
            }
            else
            {
                ActivateSuperItem(id, 0);
            }
            return;
        }
    }

    DeactivateSuperItem(id);
}


void PlaySuperItemCountdownSound(NC_STACK_ypaworld *yw, int id)
{
    if ( yw->_GameShell )
    {
        TMapSuperItem &sitem = yw->_levelInfo.SuperItems[id];

        int secLeft = sitem.CountDown / 1024;

        if ( secLeft < 10 && secLeft != sitem.LastSec )
        {
            SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            sitem.LastSec = secLeft;
        }

        int tenSecLeft = secLeft / 10;
        if ( tenSecLeft != sitem.LastTenSec )
        {
            SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            sitem.LastTenSec = tenSecLeft;
        }
    }
}

bool NC_STACK_ypaworld::IsSuperItemLostBy(int id, int owner)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( sitem.PCell->owner == owner )
        return false;

    if ( sitem.KeySectors.empty() )
        return true;

    for ( const TMapKeySector &ks : sitem.KeySectors )
    {
        if (ks.PCell->owner == owner)
            return false;
    }

    return true;
}

void NC_STACK_ypaworld::UpdateStoppedSuperItem(int id)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    for ( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && sitem.ActivateOwner == unit->_owner )
        {
            if ( IsSuperItemHeldBy(id, sitem.ActivateOwner) )
                ActivateSuperItem(id, 1);
            else if ( IsSuperItemLostBy(id, sitem.ActivateOwner) )
                DeactivateSuperItem(id);
            return;
        }
    }

    DeactivateSuperItem(id);
}

void NC_STACK_ypaworld::KillUnitsInSuperBombRadius(uint8_t activate, float centerX, float centerZ, float radius)
{
    for(cellArea &cell : _cells)
    {
        for( NC_STACK_ypabact* &bct : cell.unitsList )
        {
            int affected = 1;

            if ( _isNetGame )
            {
                if ( bct->_owner != UserUnit()->_owner || bct->_owner == activate || bct->_status == BACT_STATUS_DEAD )
                    affected = 0;
            }
            else if ( bct->_owner == activate || bct->_status == BACT_STATUS_DEAD )
            {
                affected = 0;
            }

            if ( affected )
            {
                float dx = centerX - bct->_position.x;
                float dz = centerZ - bct->_position.z;

                if ( sqrt(POW2(dx) + POW2(dz)) < radius )
                {
                    TEnergyChange energyChange;
                    energyChange.energy = -22000000;
                    energyChange.unit = NULL;
                    // The bomb's owner gets the kill, and with it the sectors of a host station
                    energyChange.killerOwner = activate;

                    bct->ModifyEnergy(&energyChange);
                }
            }
        }

    }
}

void NC_STACK_ypaworld::UpdateSuperBombShockwave(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.CurrentRadius = (_timeStamp - sitem.TriggerTime) * World::CVSectorLength / 2400.0;

    vec2d tmp = World::SectorIDToCenterPos2( sitem.CellId );

    float maxRadius = sqrt(POW2(_mapLength.x) + POW2(_mapLength.y));

    if ( sitem.CurrentRadius > 300 && sitem.CurrentRadius - sitem.LastRadius > 200 && sitem.CurrentRadius < maxRadius )
    {
        float pointCount = (2 * sitem.CurrentRadius) * C_PI / 150.0;

        sitem.LastRadius = sitem.CurrentRadius;

        if ( pointCount > 2.0 )
        {
            for (float angle = 0.0; angle < 6.283 ; angle += 6.283 / pointCount )
            {
                float radius = sitem.CurrentRadius;
                float px = cos(angle) * radius + tmp.x;
                float pz = sin(angle) * radius + tmp.y;

                if ( px > 600.0 && pz < -600.0 && px < _mapLength.x - 600.0 && pz > -(_mapLength.y - 600.0) )
                {
                    int savedFxLimit = _fxLimit;

                    _fxLimit = 2;

                    TSectorDamage damage;
                    damage.pos.x = px;
                    damage.pos.y = sitem.PCell->height;
                    damage.pos.z = pz;
                    damage.energy = 200000;
                    damage.OwnerID = sitem.ActivateOwner;
                    damage.unit = 0;

                    DamageSectorBuilding(&damage);

                    _fxLimit = savedFxLimit;
                }
            }
        }
    }

    KillUnitsInSuperBombRadius(sitem.ActivateOwner, tmp.x, tmp.y, sitem.CurrentRadius);
}

void NC_STACK_ypaworld::UpdateTriggeredSuperItem(int id)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( !IsSuperItemLostBy(id, sitem.ActivateOwner) && OwnerHasHostStation(sitem.ActivateOwner) )
    {
        if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
            UpdateSuperBombShockwave(id);
    }
    else
    {
        DeactivateSuperItem(id);
    }
}

void NC_STACK_ypaworld::UpdateSuperItems()
{
    for (size_t i = 0; i < _levelInfo.SuperItems.size(); i++)
    {
        const TMapSuperItem &sitem = _levelInfo.SuperItems[i];

        if ( sitem.Type != 0 )
        {
            switch ( sitem.State )
            {
            case TMapSuperItem::STATE_INACTIVE:
                if ( IsSuperItemHeldBy(i, sitem.PCell->owner) )
                {
                    if ( OwnerHasHostStation(sitem.PCell->owner) )
                        ActivateSuperItem(i, 0);
                }
                break;

            case TMapSuperItem::STATE_ACTIVE:
                UpdateActiveSuperItem(i);
                PlaySuperItemCountdownSound(this, i);
                break;

            case TMapSuperItem::STATE_STOPPED:
                UpdateStoppedSuperItem(i);
                break;

            case TMapSuperItem::STATE_TRIGGED:
                UpdateTriggeredSuperItem(i);
                break;

            default:
                break;
            }
        }
    }
}

void NC_STACK_ypaworld::VoiceMessageCalcPositionToUnit()
{
    if ( _voiceMessage.Unit == UserRobo() )
    {
        _voiceMessage.Carrier.Position = UserUnit()->_position;
    }
    else
    {
        vec3d tmp = _voiceMessage.Unit->_position - UserUnit()->_position;

        float dist = tmp.length();

        if ( dist > 0.0 )
            tmp *= (100.0 / dist);

        _voiceMessage.Carrier.Position = UserUnit()->_position + tmp;
    }
}

void NC_STACK_ypaworld::VoiceMessageUpdate()
{
    if ( _voiceMessage.Priority >= 0 )
    {
        if ( _voiceMessage.Unit->_status != BACT_STATUS_DEAD )
        {
            VoiceMessageCalcPositionToUnit();

            _voiceMessage.Carrier.Vector = UserUnit()->_fly_dir * UserUnit()->_fly_dir_length;
        }

        if ( _voiceMessage.Carrier.Sounds[0].IsEnabled() )
        {
            SFXEngine::SFXe.UpdateSoundCarrier(&_voiceMessage.Carrier);
        }
        else
            _voiceMessage.Reset();
    }
}

void CheckEnemySectorEntered(NC_STACK_ypaworld *yw)
{
    if ( yw->UserUnit()->_pSector->owner != yw->UserRobo()->_owner )
    {
        if ( yw->UserUnit()->_pSector->owner )
        {
            if ( yw->_ownerOldCellUserUnit == yw->UserRobo()->_owner || !yw->_ownerOldCellUserUnit )
            {
                if ( yw->_timeStamp - yw->_msgTimestampEnemySector > 10000 )
                {
                    TInfoMessage msg;
                    msg.unit = yw->UserUnit();
                    msg.Priority = 24;
                    msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_ESECTENTER);
                    msg.MsgID = 22;

                    yw->ShowMessage(&msg);
                }

                yw->_msgTimestampEnemySector = yw->_timeStamp;
            }
        }
    }
}

void NC_STACK_ypaworld::ProfileCalcValues()
{
    _profileFramesCount++;

    if ( _profileFramesCount >= 5 )
    {
        if ( _profileVals[PFID_FPS] > 200 )
            _profileVals[PFID_FPS] = 0;

        for (size_t i = 0; i < PFID_MAX; i++)
        {
            if ( _profileVals[i] != 0 )
            {
                if ( _profileVals[i] < _profileMin[i] )
                    _profileMin[i] = _profileVals[i];

                if ( _profileVals[i] > _profileMax[i] )
                    _profileMax[i] = _profileVals[i];

                _profileTotal[i] += _profileVals[i];
            }
        }
    }
}

void NC_STACK_ypaworld::EnableLevelPasses()
{
    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        TMapGate &gate = _levelInfo.Gates[ _levelInfo.GateCompleteID ];

        _globalMapRegions.MapRegions[ _levelInfo.LevelID ].Status = TMapRegionInfo::STATUS_COMPLETED;

        for (int lvl : gate.PassToLevels)
        {
            if ( _globalMapRegions.MapRegions[ lvl ].Status == TMapRegionInfo::STATUS_DISABLED )
                _globalMapRegions.MapRegions[ lvl ].Status = TMapRegionInfo::STATUS_ENABLED;
        }
    }
}

void NC_STACK_ypaworld::NetReleaseMissiles(NC_STACK_ypabact *bact)
{
    while(!bact->_missiles_list.empty())
    {
        NC_STACK_ypamissile *misl = bact->_missiles_list.front();
        bact->_missiles_list.pop_front();
        
        if ( misl->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            misl->_primT.pbact->DeleteAttacker(misl, 0);
            misl->_primTtype = BACT_TGT_TYPE_NONE;
        }

        misl->CleanAttackersTarget();

        misl->_parent = NULL;

        MoveToDeadCache(misl);

        misl->_status_flg |= BACT_STFLAG_DEATH1;
    }
}

void NC_STACK_ypaworld::DetachFromRoboGunSlot(NC_STACK_ypabact *bct)
{
    if ( bct->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

        if ( gun->IsRoboGun() )
        {
            for (World::TRoboGun &pgun : bct->_host_station->GetGuns())
            {
                if ( bct == pgun.gun_obj )
                    pgun.gun_obj = NULL;
            }
        }
    }
}

void NC_STACK_ypaworld::NetRemove(NC_STACK_ypabact *bct)
{
    while(!bct->_kidList.empty())
    {
        NC_STACK_ypabact *cmnder = bct->_kidList.front();

        while ( !cmnder->_kidList.empty() )
        {
            NC_STACK_ypabact *slave = cmnder->_kidList.front();

            NetReleaseMissiles(slave);
            slave->CleanAttackersTarget();
            DetachFromRoboGunSlot(slave);

            slave->_status_flg |= BACT_STFLAG_DEATH1;
            slave->_status = BACT_STATUS_DEAD;

            MoveToDeadCache(slave);
        }

        NetReleaseMissiles(cmnder);
        cmnder->CleanAttackersTarget();
        DetachFromRoboGunSlot(cmnder);

        cmnder->_status_flg |= BACT_STFLAG_DEATH1;
        cmnder->_status = BACT_STATUS_DEAD;

        MoveToDeadCache(cmnder);
    }

    if ( bct->_bact_type == BACT_TYPES_ROBO )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bct);

        for (World::TRoboGun &gun : robo->GetGuns())
            gun.gun_obj = NULL;
    }

    bct->CleanAttackersTarget();
    NetReleaseMissiles(bct);

    bct->_status = BACT_STATUS_DEAD;

    MoveToDeadCache(bct);

    bct->_status_flg |= BACT_STFLAG_DEATH1;
}

void NC_STACK_ypaworld::ProtosFreeSounds()
{
    SFXEngine::SFXe.StopPlayingSounds();

    for (World::TVhclProto &vhcl : _vhclProtos)
    {
        for (World::TVhclSound &sfx : vhcl.sndFX)
            sfx.ClearSounds();
    }

    for (World::TWeapProto &wep : _weaponProtos)
    {
        for (World::TVhclSound &fx : wep.sndFXes)
            fx.ClearSounds();
    }

    for (World::TBuildingProto &proto : _buildProtos)
        proto.SndFX.ClearSounds();
}


void NC_STACK_ypaworld::FFeedback_Update()
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }

    if ( UserUnit() )
    {
        if ( _timeStamp - _ffTimeStamp > 250 )
        {
            _ffTimeStamp = _timeStamp;

            if ( UserUnit()->_status == BACT_STATUS_DEAD )
            {
                Input::Engine.ForceFeedback(Input::FF_STATE_UPDATE, _ffEffectType);
            }
            else
            {
                if ( _ffEffectType != -1 )
                {
                    float thrustSq = POW2(UserUnit()->_force) - POW2(UserUnit()->_mass) * 100.0;

                    if (thrustSq < 0.0)
                        thrustSq = 0.0;

                    float speedRatio = fabs(UserUnit()->_fly_dir_length) / ( sqrt(thrustSq) / UserUnit()->_airconst_static );
                    if ( speedRatio >= 1.0 )
                        speedRatio = 1.0;
                    else if (speedRatio < 0.0)
                        speedRatio = 0.0;

                    Input::Engine.ForceFeedback(Input::FF_STATE_UPDATE, _ffEffectType,
                                                        _ffMagnitude, _ffPeriod * speedRatio);
                }
            }
        }

        TSoundSource *top = SFXEngine::SFXe.SndGetTopShake();
        if ( top )
        {
            if ( top->StartTime == SFXEngine::SFXe.currentTime )
            {
                float p1 = top->ShkMag;
                if ( p1 > 1.0 )
                    p1 = 1.0;

                vec3d tmp = top->PCarrier->Position - UserUnit()->_position;

                float p2 = top->PShkFx->time;
                float p3 = UserUnit()->_rotation.AxisX().dot( tmp );
                float p4 = -UserUnit()->_rotation.AxisZ().dot( tmp );

                if ( p2 > 0.0 )
                {
                    Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_SHAKE,
                                                        p1, p2, p3, p4);
                }
            }
        }
    }
}

void HandleSnapshotKeys(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( inpt->KbdLastHit == Input::KC_NUMMUL && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
    {
        TakeScreenshot(yw);

        TInfoMessage info_msg;
        info_msg.txt = "Screenshot saved.";
        info_msg.unit = NULL;
        info_msg.Priority = 100;
        info_msg.MsgID = 0;

        yw->ShowMessage(&info_msg);
    }


    if ( yw->_screenShotSeq )
    {
        if ( inpt->KbdLastHit == Input::KC_NUMDIV && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
        {
            yw->_screenShotSeq = false;

            TInfoMessage info_msg;
            info_msg.txt = "Screenshotting: stopped.";
            info_msg.unit = NULL;
            info_msg.Priority = 100;
            info_msg.MsgID = 0;

            yw->ShowMessage(&info_msg);
        }

        GFX::Engine.SaveScreenshot( fmt::sprintf("env:snaps/s%d_%04d", yw->_screenShotSeqId, yw->_screenShotSeqFrame) );
        
        yw->_screenShotSeqFrame++;
    }
    else if ( inpt->KbdLastHit == Input::KC_NUMDIV && (inpt->ClickInf.flag & 0x100 || yw->_easyCheatKeys) )
    {
        yw->_screenShotSeqFrame = 0;
        yw->_screenShotSeq = true;
        yw->_screenShotSeqId++;

        TInfoMessage info_msg;
        info_msg.txt = "Screenshotting: started.";
        info_msg.unit = NULL;
        info_msg.Priority = 100;
        info_msg.MsgID = 0;

        yw->ShowMessage(&info_msg);
    }

}

void DebugInfoPrintf(NC_STACK_ypaworld *yw, CmdStream *in, char *buf, const char *fmt, ...)
{
    FontUA::copy_position(in);

    va_list va;
    va_start(va, fmt);

    vsprintf(buf, fmt, va);

    va_end(va);

    FontUA::add_txt(in, yw->_screenSize.x, 1, buf);
}

void NC_STACK_ypaworld::debug_count_units()
{
    for (int i = 0; i < 8; i++)
    {
        _dbgSquadCounter[i] = 0;
        _dbgVehicleCounter[i] = 0;
        _dbgFlakCounter[i] = 0;
        _dbgRoboCounter[i] = 0;
        _dbgWeaponCounter[i] = 0;
    }

    _dbgTotalSquadCount = 0;
    _dbgTotalVehicleCount = 0;
    _dbgTotalFlakCount = 0;
    _dbgTotalWeaponCount = 0;
    _dbgTotalRoboCount = 0;

    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        _dbgRoboCounter[ robo->_owner ]++;

        if ( robo->_owner )
        {
            for ( NC_STACK_ypabact * &commander : robo->_kidList )
            {
                bool isFlak = false;

                if ( commander->_bact_type == BACT_TYPES_GUN )
                {
                    isFlak = true;
                    _dbgFlakCounter[ commander->_owner ]++;
                }
                else
                {
                    _dbgSquadCounter[ commander->_owner ]++;
                    _dbgVehicleCounter[ commander->_owner ]++;
                }

                _dbgWeaponCounter[ commander->_owner ] += commander->_missiles_list.size();

                for ( NC_STACK_ypabact * &unit : commander->_kidList )
                {
                    if ( isFlak )
                        _dbgFlakCounter[ unit->_owner ]++;
                    else
                        _dbgVehicleCounter[ commander->_owner ]++;


                    _dbgWeaponCounter[ commander->_owner ] += unit->_missiles_list.size();
                }
            }
        }
    }

    for (int i = 0; i < 8; i++)
    {
        _dbgTotalSquadCount  += _dbgSquadCounter[i];
        _dbgTotalVehicleCount += _dbgVehicleCounter[i];
        _dbgTotalFlakCount  += _dbgFlakCounter[i];
        _dbgTotalWeaponCount  += _dbgWeaponCounter[i];
        _dbgTotalRoboCount += _dbgRoboCounter[i];
    }

    if ( _dbgTotalSquadCount > _dbgTotalSquadCountMax )
        _dbgTotalSquadCountMax = _dbgTotalSquadCount;

    if ( _dbgTotalVehicleCount > _dbgTotalVehicleCountMax )
        _dbgTotalVehicleCountMax = _dbgTotalVehicleCount;

    if ( _dbgTotalFlakCount > _dbgTotalFlakCountMax )
        _dbgTotalFlakCountMax = _dbgTotalFlakCount;

    if ( _dbgTotalWeaponCount > _dbgTotalWeaponCountMax )
        _dbgTotalWeaponCountMax = _dbgTotalWeaponCount;

    if ( _dbgTotalRoboCount > _dbgTotalRoboCountMax )
        _dbgTotalRoboCountMax = _dbgTotalRoboCount;
}

void NC_STACK_ypaworld::debug_info_draw(TInputState *inpt)
{
    if ( _showDebugMode == 0)
    {
        if ( IsCheatKeyHit(inpt, Input::KC_F9) )
            _showDebugMode++;
    }
    else
    {

        CmdStream dbg_txt;
        dbg_txt.reserve(4096);
        char buf_sprintf[2048];

        FontUA::select_tileset(&dbg_txt, 15);
        FontUA::set_xpos(&dbg_txt, 8);
        FontUA::set_ypos(&dbg_txt, 16);

        int exitDebug = 0;

        if ( _showDebugMode == 1 )
        {
            debug_count_units();

            if ( !_buildDate.empty() )
            {
                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "build id: %s", _buildDate.c_str());

                FontUA::next_line(&dbg_txt);
            }

            int this_time = _timeStamp / 1024;
            int all_time;

            if ( _isNetGame )
                all_time = 0;
            else
                all_time = (_timeStamp + _playersStats[1].ElapsedTime) / 1024;

            DebugInfoPrintf(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "time: (this: %02d:%02d:%02d) (all: %02d:%02d:%02d)",
                      this_time / 60 / 60,
                      this_time / 60 % 60,
                      this_time % 60,
                      all_time / 60 / 60,
                      all_time / 60 % 60,
                      all_time % 60 );

            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof all: %d", _profileVals[PFID_FRAMETIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof fprint: %d", _profileVals[PFID_MARKTIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof gui: %d", _profileVals[PFID_GUITIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof ai: %d", _profileVals[PFID_UPDATETIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof rend: %d", _profileVals[PFID_RENDERTIME]);
            FontUA::next_line(&dbg_txt);
            
            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof 2d rend: %d", _profileVals[PFID_NEWGUITIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "prof net: %d", _profileVals[PFID_NETTIME]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "sec type/wtype: %d/%d", UserUnit()->_pSector->type_id, UserUnit()->_pSector->PurposeType);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "beam energy: %d", _beamEnergyCapacity);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "num sqd: %d,%d", _dbgTotalSquadCount, _dbgTotalSquadCountMax);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "num vhcl: %d,%d", _dbgTotalVehicleCount, _dbgTotalVehicleCountMax);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "num flk: %d,%d", _dbgTotalFlakCount, _dbgTotalFlakCountMax);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "num robo: %d,%d", _dbgTotalRoboCount, _dbgTotalRoboCountMax);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "num wpn: %d,%d", _dbgTotalWeaponCount, _dbgTotalWeaponCountMax);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "reload const: %d", UserRobo()->_reload_const);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "num all vhcl: %d,%d,%d,%d,%d,%d,%d,%d",
                      _countUnitsPerOwner[0],
                      _countUnitsPerOwner[1],
                      _countUnitsPerOwner[2],
                      _countUnitsPerOwner[3],
                      _countUnitsPerOwner[4],
                      _countUnitsPerOwner[5],
                      _countUnitsPerOwner[6],
                      _countUnitsPerOwner[7]);
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "rld ratio: %8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f",
                      _reloadRatioPositive[0],
                      _reloadRatioPositive[1],
                      _reloadRatioPositive[2],
                      _reloadRatioPositive[3],
                      _reloadRatioPositive[4],
                      _reloadRatioPositive[5],
                      _reloadRatioPositive[6],
                      _reloadRatioPositive[7]);
            FontUA::next_line(&dbg_txt);

            if ( _invulnerable )
                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "invulnerable: %s", "YES");
            else
                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "invulnerable: %s", "NO");
            FontUA::next_line(&dbg_txt);
        }
        else if ( _showDebugMode == 2 )
        {
            if ( _GameShell && _isNetGame )
            {
                FontUA::next_line(&dbg_txt);
                FontUA::next_line(&dbg_txt);

                for (UserData::TNetPlayerData &pl : _GameShell->netPlayers)
                {
                    if ( pl.Status )
                    {
                        const char *factionName;
                        const char *statusName;

                        switch ( pl.Owner )
                        {
                        case 1:
                            factionName = "Resistance";
                            break;

                        case 3:
                            factionName = "Mykonier  ";
                            break;

                        case 4:
                            factionName = "Taerkasten";
                            break;

                        case 6:
                            factionName = "Ghorkov   ";
                            break;

                        default:
                            factionName = "Hae?!     ";
                            break;
                        }

                        switch ( pl.Status )
                        {
                        case 1:
                            statusName = "OK";
                            break;

                        case 2:
                            statusName = "makes trouble";
                            break;

                        case 3:
                            statusName = "left the game";
                            break;

                        case 4:
                            statusName = "Removed";
                            break;

                        default:
                            statusName = "Hae?!     ";
                            break;
                        }

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "%s status: %s latency: %d", factionName, statusName, pl.Latency);

                        FontUA::next_line(&dbg_txt);
                    }
                }

                FontUA::next_line(&dbg_txt);

                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "net send: %d bytes/sec", _GameShell->netsend_speed);
                FontUA::next_line(&dbg_txt);

                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "net rcv: %d bytes/sec", _GameShell->netrecv_speed);
                FontUA::next_line(&dbg_txt);

                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "packet: %d bytes", _GameShell->net_packet_size);
                FontUA::next_line(&dbg_txt);

                if ( _netInfoOverkill )
                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "WARNING: INFO OVERKILL");

                FontUA::next_line(&dbg_txt);

                if ( _netDriver )
                {
                    int netStats[6];
                    _netDriver->GetStats(netStats);

                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "round trip: %d ms", netStats[0]);
                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "packet loss: %d%%", netStats[1]);
                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "packets sent: %d", netStats[2]);
                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "packets received: %d", netStats[3]);
                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "kb sent: %d", netStats[4]);
                    FontUA::next_line(&dbg_txt);

                    DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "kb received: %d", netStats[5]);
                    FontUA::next_line(&dbg_txt);
                }
            }
            else
            {
                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "not a network game");
                FontUA::next_line(&dbg_txt);
            }
        }
        else if ( _showDebugMode == 3 )
        {
            for (int i = 0; i < 17; i++)
            {
                DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "slider[%d] = %f", i, inpt->Sliders[i]);
                FontUA::next_line(&dbg_txt);
            }

            std::string buf;

            for (int i = 0; i < 32; i++)
            {
                if ( inpt->Buttons.Is(i) )
                    buf += 'O';
                else
                    buf += '_';
            }

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, buf.c_str());
            FontUA::next_line(&dbg_txt);

            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "keycode = %d", inpt->KbdLastDown);
            FontUA::next_line(&dbg_txt);
        }
        else
        {
            int found = 0;
            int roboNum = 0;

            for ( NC_STACK_ypabact * &bact : _unitsList )
            {
                if (found)
                    break;

                if ( bact->_bact_type == BACT_TYPES_ROBO && bact->_owner != 1 ) // FIXME owner
                {
                    roboNum++;

                    if ( _showDebugMode - 3 <= roboNum )
                    {
                        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bact);

                        found = 1;

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "robo owner %d with energy %d / %d / %d / %d", robo->_owner, robo->_energy, robo->_roboBuildSpare, robo->_roboVehicleSpare, robo->_energy_max);
                        FontUA::next_line(&dbg_txt);

                        const char *buildJob;
                        const char *vhclJob;

                        switch (robo->_roboBuildingDuty)
                        {
                        case 0x10:
                            buildJob = "radar";
                            break;

                        case 0x20:
                            buildJob = "powerstation";
                            break;

                        case 0x100:
                            buildJob = "flak";
                            break;

                        case 0x20000:
                            buildJob = "location";
                            break;

                        default:
                            buildJob = "nothing";
                            break;
                        }

                        switch (robo->_roboVehicleDuty)
                        {
                        case 0:
                            vhclJob = "nothing";
                            break;

                        case 0x40:
                            vhclJob = "conquer";
                            break;

                        case 0x80:
                            vhclJob = "defense";
                            break;

                        case 0x80000:
                            vhclJob = "recon";
                            break;

                        case 0x200000:
                            vhclJob = "robo";
                            break;

                        default:
                            vhclJob = "powerstation";
                            break;
                        }

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "    do build job   >%s<   and vhcl job   >%s<", buildJob, vhclJob);
                        FontUA::next_line(&dbg_txt);

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "    wait power %d, radar %d, flak %d, location %d",
                                         robo->_roboPowerDelay / 1000,
                                         robo->_roboRadarDelay / 1000,
                                         robo->_roboSafetyDelay / 1000,
                                         robo->_roboPositionDelay / 1000);

                        FontUA::next_line(&dbg_txt);

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "    wait conquer %d, defense %d, recon %d, robo %d",
                                         robo->_roboConqDelay / 1000,
                                         robo->_roboEnemyDelay / 1000,
                                         robo->_roboExploreDelay / 1000,
                                         robo->_roboDangerDelay / 1000);
                        FontUA::next_line(&dbg_txt);

                        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "    values  ");
                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboPowerDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "power -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "power %d, ", robo->_roboPowerValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboRadarDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "radar -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "radar %d, ", robo->_roboRadarValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboSafetyDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "flak -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "flak %d, ", robo->_roboSafetyValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboPositionDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "power -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "power %d, ", robo->_roboPositionValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboEnemyDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "defense -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "defense %d, ", robo->_roboEnemyValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboConqDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "conquer -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "conquer %d, ", robo->_roboConqValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboExploreDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "recon -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "recon %d, ", robo->_roboExploreValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboDangerDelay > 0 )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "robo -1, ");
                        else
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "robo %d, ", robo->_roboDangerValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboState & NC_STACK_yparobo::ROBOSTATE_DOCKINUSE )
                            DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "dock energy %d time %d", robo->_roboDockEnerg, robo->_roboDockTime);
                    }
                }
            }

            if ( !found )
                exitDebug = 1;
        }


        FontUA::next_line(&dbg_txt);

        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "fps: %d", _FPS);
        FontUA::next_line(&dbg_txt);

        DebugInfoPrintf(this, &dbg_txt, buf_sprintf, "polys: %d,%d", _polysCount, _polysDraw);
        FontUA::next_line(&dbg_txt);

        FontUA::set_end(&dbg_txt);

        GFX::Engine.ProcessDrawSeq(dbg_txt);


        if ( exitDebug )
        {
            _showDebugMode = 0;
        }
        else
        {
            if ( IsCheatKeyHit(inpt, Input::KC_F9) )
                _showDebugMode++;
        }
    }
}

static const int PerfOverlayLines = 5;

void NC_STACK_ypaworld::DrawPerfOverlay(TInputState *inpt)
{
    // F10 goes from nothing to the numbers, then the numbers with the view
    // distance window, and back to nothing. The window is there with a
    // level's interface, or in the replay viewer.
    bool haveWindow = _guiLoaded || _replay;

    if ( inpt->KbdLastHit == Input::KC_F10 )
    {
        if ( !_showPerfOverlay )
        {
            _showPerfOverlay = true;
        }
        else if ( haveWindow && _viewDistanceWindow.IsClosed() )
        {
            TileMap *font = GFX::Engine.GetTileset(15);
            _viewDistanceWindow.Open(this, 16 + (PerfOverlayLines + 1) * font->h);
        }
        else
        {
            _showPerfOverlay = false;

            if ( haveWindow )
                _viewDistanceWindow.Close(this);
        }
    }

    if ( !_showPerfOverlay )
        return;

    using PS = System::PerfStats;
    const PS::Summary &perf = System::Perf.GetSummary();

    const std::array<std::string, PerfOverlayLines> lines = {
        fmt::sprintf("%.0f fps   %.2f ms   worst %.2f ms", perf.Fps, perf.FrameMs, perf.WorstMs),

        fmt::sprintf("sim %.2f   scene %.2f   draw %.2f   gui %.2f   present %.2f   wait %.2f   idle %.2f ms",
                     perf.SectionMs[PS::SEC_SIM], perf.SectionMs[PS::SEC_SCENE],
                     perf.SectionMs[PS::SEC_DRAW], perf.SectionMs[PS::SEC_GUI],
                     perf.SectionMs[PS::SEC_PRESENT], perf.SectionMs[PS::SEC_WAIT],
                     perf.SectionMs[PS::SEC_IDLE]),

        fmt::sprintf("%d draw calls (%d particles), %d triangles, %d texture changes",
                     perf.Counters[PS::CNT_DRAWCALLS], perf.Counters[PS::CNT_PARTICLES],
                     perf.Counters[PS::CNT_TRIANGLES], perf.Counters[PS::CNT_TEXCHANGES]),

        fmt::sprintf("%d units drawn", perf.Counters[PS::CNT_UNITS]),

        GFX::Engine.GetRendererInfo()
    };

    CmdStream txt;
    FontUA::select_tileset(&txt, 15);
    FontUA::set_xpos(&txt, 0);
    FontUA::set_ypos(&txt, 16);

    for (const std::string &line : lines)
    {
        // Flag 2 right-aligns the text in its block, which ends 8 pixels
        // from the right edge of the screen.
        FontUA::copy_position(&txt);
        FontUA::add_txt(&txt, _screenSize.x - 8, 1 | 2, line);
        FontUA::next_line(&txt);
    }

    FontUA::set_end(&txt);

    GFX::Engine.ProcessDrawSeq(txt);
}



void NC_STACK_ypaworld::ScoreKill(NC_STACK_ypabact *bact)
{
    if ( bact->_killer )
    {
        ScoreEvent( World::History::Event::Kill(bact->_owner, bact->_killer->_owner,
                                                bact->_killer->getBACT_viewer() || (bact->_killer->_status_flg & BACT_STFLAG_ISVIEW),
                                                bact->getBACT_viewer() || (bact->_status_flg & BACT_STFLAG_ISVIEW),
                                                bact->_bact_type == BACT_TYPES_ROBO,
                                                bact->_position.x * 256.0 / bact->_wrldSize.x, bact->_position.z * 256.0 / bact->_wrldSize.y) );
    }
}
