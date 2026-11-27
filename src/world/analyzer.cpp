#include "analyzer.h"
#include "../yw.h"
#include "../yparobo.h"
#include "../yw_internal.h"
#include "locale/locale.h"

extern TBottomBar bzda;

namespace World
{

std::string GameAnalyzer::Analyze(NC_STACK_ypaworld *world)
{
    if (IsHSAttacked(world)) 
    {
        if (IsHSLowHP(world)) 
        {
            if (IsHasBackupPS(world)) 
                return Locale::Text::Advice(Locale::ADVICE_DEFENDHS);
            else 
                return Locale::Text::Advice(Locale::ADVICE_ESCAPETOPS);
        } 
        else 
        {
            return Locale::Text::Advice(Locale::ADVICE_SCREWED);
        }
    } 
    else if (IsAnySquadInFight(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_SENDFORCES);
    }
    else if (IsOwnPowerStation(world) && IsOnHighPowerStation(world) && IsPowerEffLow(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQPOWEREFF);
    } 
    else if (IsOwnPowerStation(world) && !IsOnHighPowerStation(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_BEAMTOSTRONG);
    } 
    else if (IsSpottedEnemyPS(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQPS);
    }
    else if (!IsOwnPowerStation(world) && IsCanBuildPowerStation(world)) 
    {
        return Locale::Text::Advice(Locale::ADVICE_BUILDPS);
    } 
    else if (IsSpottedTek(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQTECH);
    }
    else if (IsSpottedBeamGate(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQGATE);
    }
    else if (IsSpottedKeySector(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQKEYGATE);
    }
    else if (IsSpottedEnemyHS(world)) 
    {
        if (IsSpottedEnemyHSwithPS(world)) 
        {
            return Locale::Text::Advice(Locale::ADVICE_COLLECTTOPS);
        } 
        else 
        {
            return Locale::Text::Advice(Locale::ADVICE_COLLECTTOHS);
        }
    } 
    else if (IsFewSeeSectors(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_EXPLORE);
    }
    else if (IsFewOwnSectors(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_CONQLAND);
    }
    else if (IsReadyToBeam(world))
    {
        return Locale::Text::Advice(Locale::ADVICE_FORCESTOBEAM);
    }
    
    return Locale::Text::Advice(Locale::ADVICE_NOPROBLEM);
}

bool GameAnalyzer::IsHSAttacked(NC_STACK_ypaworld *world)
{
    NC_STACK_ypabact *hs = world->UserRobo();
            
    for ( int sY = -1; sY <= 1; sY++ )
    {
        for ( int sX = -1; sX <= 1; sX++ )
        {
            cellArea *s = world->GetSector( hs->_cellId + Common::Point(sX, sY) );
            if (s)
            {
                for (NC_STACK_ypabact *bact : s->unitsList)
                {
                    if (bact->_status != BACT_STATUS_CREATE
                     && bact->_status != BACT_STATUS_DEAD
                     && bact->_bact_type != BACT_TYPES_UFO
                     && bact->_owner != hs->_owner)
                        return true;
                }
            }
        }
    }
    return false;
}
    
bool GameAnalyzer::IsHSLowHP(NC_STACK_ypaworld *world)
{
    if (world->UserRobo()->_energy < world->UserRobo()->_energy_max / 8)
        return true;
    return false;
}
    
bool GameAnalyzer::IsHasBackupPS(NC_STACK_ypaworld *world)
{
    for(const auto it : world->_powerStations)
    {
        if (it.second.pCell)
        {
            if (it.second.pCell->owner == world->LocalOwner())
            {
                if ( Common::ABS(it.second.CellId.x - world->UserRobo()->_cellId.x) >= 4
                  || Common::ABS(it.second.CellId.y - world->UserRobo()->_cellId.y) >= 4)
                    return true;
            }
        }
    }
    return false;
}
    
bool GameAnalyzer::IsAnySquadInFight(NC_STACK_ypaworld *world)
{
    for (const MapScene::Unit &commander : world->_cmdrsRemap) 
    {
        if (commander.SecondaryType == BACT_TGT_TYPE_UNIT) 
            return true;
    }
    return false;
}
    
bool GameAnalyzer::IsOwnPowerStation(NC_STACK_ypaworld *world)
{
    for(const auto it : world->_powerStations)
    {
        if (it.second.pCell)
        {
            if (it.second.pCell->owner == world->LocalOwner())
                return true;
        }
    }
    return false;
}
    
bool GameAnalyzer::IsOnHighPowerStation(NC_STACK_ypaworld *world)
{
    const TPowerStationInfo *maxPwr = NULL;
    for(const auto it : world->_powerStations)
    {
        if (it.second.pCell)
        {
            if (it.second.pCell->owner == world->LocalOwner()
             && (!maxPwr || maxPwr->EffectivePower < it.second.EffectivePower) )
                maxPwr = &it.second;
        }
    }
    
    if (maxPwr && world->UserRobo()->_pSector == maxPwr->pCell)
        return true;
    
    return false;
}
    
bool GameAnalyzer::IsPowerEffLow(NC_STACK_ypaworld *world)
{
    if (world->_reloadRatioClamped[ world->UserRobo()->_owner ] < 0.9)
        return true;
    return false;
}
    
bool GameAnalyzer::IsSpottedEnemyPS(NC_STACK_ypaworld *world)
{
    for(const auto it : world->_powerStations)
    {
        if (it.second.pCell)
        {
            if (it.second.pCell->owner != world->LocalOwner() &&
                it.second.pCell->IsCanSee(world->UserRobo()->_owner) )
                return true;
        }
    }
    return false;
}

bool GameAnalyzer::IsCanBuildPowerStation(NC_STACK_ypaworld *world)
{
    int32_t enrg = ((NC_STACK_yparobo *)world->UserRobo())->getROBO_battVehicle();
    for(int i = 0; i < bzda.buildCount; i++)
    {
        TBuildingProto &proto = world->_buildProtos.at( bzda.buildProtoIds[i] );
        if (proto.ModelID == 1 && enrg >= proto.Energy)
            return true;            
    }
    return false;
}

bool GameAnalyzer::IsSpottedTek(NC_STACK_ypaworld *world)
{
    for (const TMapGem &gem : world->_techUpgrades)
    {
        cellArea *cell = world->GetSector(gem.CellId);
        if ( cell && cell->PurposeType == cellArea::PT_TECHUPGRADE )
        {
            if ( world->LocalOwner() != cell->owner 
              && cell->IsCanSee(world->UserRobo()->_owner) )
                return true;
        }
    }
    return false;
}
    
bool GameAnalyzer::IsSpottedBeamGate(NC_STACK_ypaworld *world)
{
    for (const TMapGate &gate : world->_levelInfo.Gates)
    {
        if ( gate.PCell->PurposeType == cellArea::PT_GATECLOSED )
        {
            if ( world->LocalOwner() != gate.PCell->owner 
              && gate.PCell->IsCanSee(world->UserRobo()->_owner) )
                return true;
        }
    }
    return false;
}

bool GameAnalyzer::IsSpottedKeySector(NC_STACK_ypaworld *world)
{
    for (const TMapGate &gate : world->_levelInfo.Gates)
    {
        for( const TMapKeySector &ks : gate.KeySectors )
        {
            if ( world->LocalOwner() != ks.PCell->owner 
              && ks.PCell->IsCanSee(world->UserRobo()->_owner) )
                return true;
        }
    }
    return false;
}

bool GameAnalyzer::IsSpottedEnemyHS(NC_STACK_ypaworld *world)
{
    for (const NC_STACK_ypabact *hs : world->_unitsList)
    {
        if ( hs != world->UserRobo()
         &&  hs->_bact_type == BACT_TYPES_ROBO
         &&  hs->_owner != world->LocalOwner()
         &&  hs->_status != BACT_STATUS_DEAD
         &&  hs->_pSector->IsCanSee( world->UserRobo()->_owner) )
        {
            return true;
        }
    }
    return false;
}

bool GameAnalyzer::IsSpottedEnemyHSwithPS(NC_STACK_ypaworld *world)
{
    for (const NC_STACK_ypabact *hs : world->_unitsList)
    {
        if ( hs != world->UserRobo()
         &&  hs->_bact_type == BACT_TYPES_ROBO
         &&  hs->_owner != world->LocalOwner()
         &&  hs->_status != BACT_STATUS_DEAD
         &&  hs->_pSector->IsCanSee( world->UserRobo()->_owner ) )
        {
            for(const auto it : world->_powerStations)
            {
                if (it.second.pCell && it.second.pCell->owner == hs->_owner 
                 && it.second.pCell->IsCanSee(world->UserRobo()->_owner))
                {
                    if (Common::ABS(it.second.CellId.x - hs->_cellId.x) < 3
                     || Common::ABS(it.second.CellId.y - hs->_cellId.y) < 3)
                        return true;
                }
            }
        }
    }
    return false;
}

bool GameAnalyzer::IsFewSeeSectors(NC_STACK_ypaworld *world)
{
    int32_t see = 0;
    for( const cellArea &cell : world->_cells )
    {
        if (cell.IsCanSee(world->UserRobo()->_owner))
            see++;
    }
    
    float prc = ((float)see) / ((float)(world->_mapSize.x - 2) * (world->_mapSize.y - 2));
    if (prc < 0.75)
        return true;
    return false;        
}

bool GameAnalyzer::IsFewOwnSectors(NC_STACK_ypaworld *world)
{
    if (world->_countSectorsPerOwner[world->UserRobo()->_owner] <= 0)
        return true;
    
    int32_t see = 0;
    for( const cellArea &cell : world->_cells )
    {
        if (cell.IsCanSee(world->UserRobo()->_owner))
            see++;
    }

    float prc = ((float)see) / ((float)world->_countSectorsPerOwner[world->UserRobo()->_owner]);
    if (prc < 0.75)
        return true;
    return false; 
}

bool GameAnalyzer::IsReadyToBeam(NC_STACK_ypaworld *world)
{
    for (const TMapGate &gate : world->_levelInfo.Gates)
    {
        if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
        {
            return true;
        }
    }
    return false;
}


}