#include <inttypes.h>
#include <string.h>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "lstvw.h"
#include "font.h"
#include "env.h"

extern GuiList briefingTextList;

void BriefStartMapScaling(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    SFXEngine::SFXe.StopMusicTrack();
    if ( yw->_GameShell->missiontrack )
    {
        SFXEngine::SFXe.SetMusicTrack(yw->_GameShell->missiontrack, yw->_GameShell->missiontrack__adv.min_delay, yw->_GameShell->missiontrack__adv.max_delay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    brf->StartTime = brf->CurrTime;
    brf->Stage = TBriefengScreen::STAGE_SCALING;

    brf->MapBlitParams.pbitm = brf->MbmapImg->GetBitmap();

    brf->MapBlitStart.left = -0.4515625;
    brf->MapBlitStart.top = -0.324999988079;
    brf->MapBlitStart.right = brf->MapBlitStart.left;
    brf->MapBlitStart.bottom = brf->MapBlitStart.top;

    float aspectX, aspectY;

    if ( yw->_mapSize.x == yw->_mapSize.y )
    {
        aspectX = 1.0;
        aspectY = 1.0;
    }
    else if (yw->_mapSize.x < yw->_mapSize.y)
    {
        aspectY = 1.0;
        aspectX = (float)yw->_mapSize.x / (float)yw->_mapSize.y;
    }
    else if (yw->_mapSize.x > yw->_mapSize.y)
    {
        aspectX = 1.0;
        aspectY = (float)yw->_mapSize.y / (float)yw->_mapSize.x;
    }

    brf->MapBlitParams.srcRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);

    brf->MapBlitEnd.left = -0.4515625 - aspectX * 0.4828125;
    brf->MapBlitEnd.top = -0.325 - aspectY * 0.6083333333333334;
    brf->MapBlitEnd.right = brf->MapBlitEnd.left + aspectX * 0.965625;
    brf->MapBlitEnd.bottom = brf->MapBlitEnd.top + aspectY * 1.216666666666667;
    brf->MapBlitParams.dstRect = brf->MapBlitStart;
    brf->TextTime = 0;
    brf->PreTextTime = brf->CurrTime;
}

void BriefUpdateMapScaling(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elapsed = brf->CurrTime - brf->StartTime;

    if ( elapsed >= 600 )
    {
        brf->MapBlitParams.dstRect = brf->MapBlitEnd;
        brf->Stage = TBriefengScreen::STAGE_SCALEEND;

        GFX::Engine.SetCursor(6, 0);
    }
    else
    {
        float progress = elapsed * 0.0016666667;
        brf->MapBlitParams.dstRect.left = (brf->MapBlitEnd.left - brf->MapBlitStart.left) * progress + brf->MapBlitStart.left;
        brf->MapBlitParams.dstRect.top = (brf->MapBlitEnd.top - brf->MapBlitStart.top) * progress + brf->MapBlitStart.top;
        brf->MapBlitParams.dstRect.right = (brf->MapBlitEnd.right - brf->MapBlitStart.right) * progress + brf->MapBlitStart.right;
        brf->MapBlitParams.dstRect.bottom = (brf->MapBlitEnd.bottom - brf->MapBlitStart.bottom) * progress + brf->MapBlitStart.bottom;
    }
}

int yw_MBLoadSet(NC_STACK_ypaworld *yw, int setID)
{
    std::string oldRsrc = Common::Env.SetPrefix("rsrc", fmt::sprintf("data:set%d:", setID));

    if ( setID != yw->_setId && setID != 46 )
    {
        if ( yw->_setData )
        {
            ypa_log_out("yw_MBLoadSet(): killing set object %d\n", yw->_setId);
            yw->_setData->Delete();
            yw->_setData = NULL;
            yw->_setId = 0;
        }

        yw->_setData = load_set_base();
        if ( !yw->_setData )
        {
            ypa_log_out("yw_MBLoadSet(): loading set object %d failed\n", setID);
            Common::Env.SetPrefix("rsrc", oldRsrc);
            return 0;
        }
        
        yw->_setData->MakeVBO();

        yw->_setId = setID;
        ypa_log_out("yw_MBLoadSet(): loaded set object %d ok\n", setID);
    }

    if ( setID != 46 )
    {
        FSMgr::FileHandle *fil = uaOpenFileAlloc("rsrc:scripts/set.sdf", "r");
        if ( !fil )
        {
            ypa_log_out("Briefing: no set description file.\n");
            return 0;
        }

        int kid_id = 0;

        for( NC_STACK_base *& bs : yw->_setData->GetKidList() )
        {
            if ( kid_id == 0 )
            {
                if ( !SetupVehicleModels(yw, bs) )
                {
                    delete fil;
                    return 0;
                }
            }
            else if ( kid_id == 1 )
            {
                if ( !yw_parse_lego(yw, fil, bs) )
                {
                    delete fil;
                    return 0;
                }

                if ( !yw_parse_subSect(yw, fil) )
                {
                    delete fil;
                    return 0;
                }

                if ( !yw_parse_sektor(yw, fil) )
                {
                    delete fil;
                    return 0;
                }
            }
            else if ( kid_id == 2 )
            {
                if ( !SetupFillerModels(yw, bs) )
                {
                    delete fil;
                    return 0;
                }
            }

            kid_id++;
        }

        delete fil;
        
        yw->LoadOverrideModels();
    }

    Common::Env.SetPrefix("rsrc", oldRsrc);

    return 1;
}

void BriefLoadLevelSet(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->AddObjectsFlag = true;
    brf->TextTime = brf->CurrTime;
    brf->Objects.clear();

    if ( yw_MBLoadSet(yw, brf->Desc.SetID) )
    {
        brf->Stage = TBriefengScreen::STAGE_PLAYER_ST;

        GFX::Engine.SetCursor(1, 0);
    }
    else
    {
        brf->Stage = TBriefengScreen::STAGE_PLAYLEVEL;
    }
}

void BriefSetupPlayerHostStation(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->ElementsCount = 1;

    brf->Stage = TBriefengScreen::STAGE_PLAYER_RN;
    brf->StartTime = brf->CurrTime;

    brf->SelectedObjID = -1;
    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_YOU);
}

void NC_STACK_ypaworld::BriefingSetObject(const TBriefObject &obj, bool doAdd)
{
    _briefScreen.ViewingObjectStartTime = _briefScreen.CurrTime;
    
    _briefScreen.ViewingObject = obj;

    if ( doAdd )
        _briefScreen.Objects.push_back( obj );

    if ( _GameShell )
        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 11);
}

void BriefUpdatePlayerHostStation(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    if ( brf->CurrTime - brf->StartTime >= 2500 )
    {
        brf->Stage = TBriefengScreen::STAGE_PLAYER_END;
    }
    else if ( brf->ActiveElementID )
    {
        MapRobo &robo = brf->Desc.Robos[0];

        brf->ActiveElementID = 0;
        yw->BriefingSetObject( TBriefObject(TBriefObject::TYPE_VEHICLE, robo.VhclID, robo.Pos.x, robo.Pos.z,
                                           26, 128 + robo.Owner, 25,
                                           yw->GetVehicleName(robo.VhclID)), 
                               brf->AddObjectsFlag);
    }
}

void yw_BriefSetupKeySectors(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->ElementsCount = 0;
    brf->StartTime = brf->CurrTime;

    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_PRIM);

    for (const TMapGate &gate : yw->_levelInfo.Gates )
    {
        if ( gate.MbStatus != 1 )
            brf->ElementsCount += gate.KeySectors.size();
    }

    if ( brf->ElementsCount == 0 )
        brf->Stage = TBriefengScreen::STAGE_KEYS_END;
    else
        brf->Stage = TBriefengScreen::STAGE_KEYS_RN;
}

void yw_BriefUpdateKeySectors(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;

    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_KEYS_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        brf->ActiveElementID = elmID;
        int ksID = 0; /* Current key sector */

        for ( const TMapGate &gate : yw->_levelInfo.Gates )
        {
            if ( gate.MbStatus != 1 )
            {
                for (const TMapKeySector &ks : gate.KeySectors)
                {
                    if ( elmID == ksID)
                    {
                        vec2d ps = World::SectorIDToCenterPos2( ks.CellId );

                        uint8_t secType = yw->_lvlTypeMap(ks.CellId);

                        yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_SECTOR, secType, ps.x, ps.y, 26, 146, 25, 
                                                            Locale::Text::Brief(Locale::BRIEF_KEYSECT)),
                                               brf->AddObjectsFlag);
                    }

                    ksID++;
                }
            }
        }
    }
}

void BriefSetupTechUpgrades(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->ElementsCount = 0;
    brf->StartTime = brf->CurrTime;
    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_TECH);

    for ( const TMapGem &gem : yw->_techUpgrades)
    {
        if ( gem.MbStatus != 1 )
            brf->ElementsCount++;
    }

    brf->Stage = (brf->ElementsCount == 0) + TBriefengScreen::STAGE_TECH_RN;
}

void BriefUpdateTechUpgrades(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;
    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_TECH_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        brf->ActiveElementID = elmID;

        const TMapGem * pGem = &yw->_techUpgrades[0];

        for (const TMapGem &gem : yw->_techUpgrades)
        {
            pGem = &gem;

            if (gem.MbStatus != 1)
            {
                if (elmID == 0)
                    break;

                elmID--;
            }
        }

        vec2d ps = World::SectorIDToCenterPos2( pGem->CellId );
        
        int secType = yw->_buildProtos[pGem->BuildingID].SecType;

        yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_SECTOR, secType, ps.x, ps.y, 26, 144, 25, 
                                            Locale::Text::Brief(Locale::BRIEF_TECH2) ),  
                               brf->AddObjectsFlag);
    }
}

void BriefSetupEnemyHostStations(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->StartTime = brf->CurrTime;

    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_ENMYHS);
    brf->ElementsCount = 0;

    for (size_t i = 1; i < brf->Desc.Robos.size(); i++)
    {
        if (brf->Desc.Robos[i].MbStatus != 1)
            brf->ElementsCount++;
    }

    brf->Stage = TBriefengScreen::STAGE_ENMHS_RN + (brf->ElementsCount == 0);
}

void BriefUpdateEnemyHostStations(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;

    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_ENMHS_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        int remaining = elmID;
        brf->ActiveElementID = elmID;

        MapRobo *robo = &brf->Desc.Robos[1];

        for (size_t i = 1; i < brf->Desc.Robos.size(); i++)
        {
            robo = &brf->Desc.Robos[i];

            if (robo->MbStatus != 1)
            {
                if (remaining == 0)
                    break;

                remaining--;
            }
        }

        yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_VEHICLE, robo->VhclID, robo->Pos.x, robo->Pos.z, 26, 128 + robo->Owner, 25, 
                                            yw->GetVehicleName(robo->VhclID) ),  
                               brf->AddObjectsFlag);
    }
}

void BriefSetupEnemyForces(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->ElementsCount = 0;
    brf->StartTime = brf->CurrTime;

    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_ENMYFORCE);

    for ( const MapSquad &squad : brf->Desc.Squads )
    {
        if ( squad.Owner != brf->Desc.PlayerOwner && squad.MbStatus != 1)
            brf->ElementsCount++;
    }

    brf->Stage = (brf->ElementsCount == 0) + TBriefengScreen::STAGE_ENMFRC_RN;
}

void BriefUpdateEnemyForces(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;
    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_ENMFRC_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        brf->ActiveElementID = elmID;
        int remaining = elmID;

        const MapSquad *squad = NULL;

        for ( const MapSquad &it : brf->Desc.Squads )
        {
            squad = &it;

            if ( it.Owner != brf->Desc.PlayerOwner && it.MbStatus != 1)
            {
                if ( remaining == 0 )
                    break;
                remaining--;
            }
        }

        if ( squad )
        {
            std::string nm = fmt::sprintf("%d %s", squad->Count, yw->GetVehicleName(squad->VhclID));

            yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_VEHICLE, squad->VhclID, squad->X, squad->Z, 26, 136 + squad->Owner, 36, 
                                                nm ),  
                                   brf->AddObjectsFlag);
        }
    }
}

void BriefSetupFriendlyForces(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();
    
    brf->ActiveElementID = -1;
    brf->ElementsCount = 0;
    brf->Stage = TBriefengScreen::STAGE_BUDDY_RN;
    brf->ObjDescription = "FRIENDLY SUPPORT FORCES";

    brf->StartTime = brf->CurrTime;

    for ( const MapSquad &it : brf->Desc.Squads )
    {
        if ( it.Owner == brf->Desc.PlayerOwner && it.MbStatus != 1)
            brf->ElementsCount++;
    }

    brf->Stage = (brf->ElementsCount == 0) + TBriefengScreen::STAGE_BUDDY_RN;
}

void BriefUpdateFriendlyForces(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;
    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_BUDDY_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        int remaining = elmID;
        brf->ActiveElementID = elmID;

        const MapSquad *squad = NULL;

        for ( const MapSquad &it : brf->Desc.Squads )
        {
            squad = &it;

            if ( it.Owner == brf->Desc.PlayerOwner && it.MbStatus != 1)
            {
                if ( remaining == 0 )
                    break;

                remaining--;
            }
        }

        if ( squad )
        {
            std::string nm = fmt::sprintf("%d %s", squad->Count, yw->GetVehicleName(squad->VhclID));

            yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_VEHICLE, squad->VhclID, squad->X, squad->Z, 26, 136 + squad->Owner, 25, 
                                                nm ),  
                                   brf->AddObjectsFlag);
        }
    }
}

void BriefSetupGates(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    brf->ViewingObject = TBriefObject();

    brf->ActiveElementID = -1;
    brf->ElementsCount = 0;

    brf->Stage = TBriefengScreen::STAGE_GATE_RN;
    brf->StartTime = brf->CurrTime;

    brf->ObjDescription = Locale::Text::Brief(Locale::BRIEF_GATE);

    for ( const TMapGate &gate : yw->_levelInfo.Gates )
    {
        if (gate.MbStatus != 1)
            brf->ElementsCount++;
    }

    brf->Stage = (brf->ElementsCount == 0) + TBriefengScreen::STAGE_GATE_RN;
}

void BriefUpdateGates(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int elmID = (brf->CurrTime - brf->StartTime) / 2500;

    if ( elmID >= brf->ElementsCount )
    {
        brf->Stage = TBriefengScreen::STAGE_GATE_END;
    }
    else if ( elmID != brf->ActiveElementID )
    {
        brf->ActiveElementID = elmID;

        const TMapGate *pGate = &yw->_levelInfo.Gates[0];

        for ( const TMapGate &gate : yw->_levelInfo.Gates )
        {
            pGate = &gate;

            if (gate.MbStatus != 1)
            {
                if ( elmID == 0 )
                    break;

                elmID--;
            }
        }
        
        vec2d ps = World::SectorIDToCenterPos2( pGate->CellId );

        int secType = yw->_buildProtos[ pGate->ClosedBldID ].SecType;

        yw->BriefingSetObject( TBriefObject( TBriefObject::TYPE_SECTOR, secType, ps.x, ps.y, 26, 145, 25, 
                                            Locale::Text::Brief(Locale::BRIEF_GATE2) ),  
                               brf->AddObjectsFlag);
    }
}

void BriefSelectObjectUnderCursor(NC_STACK_ypaworld *yw, TInputState *inpt, TBriefengScreen *brf)
{
    int hoverID = -1;

    if ( !inpt->ClickInf.selected_btn )
    {
        float mx = (float)inpt->ClickInf.move.ScreenPos.x / (float)yw->_screenSize.x;
        float my = (float)inpt->ClickInf.move.ScreenPos.y / (float)yw->_screenSize.y;

        for (size_t i = 0; i < brf->Objects.size(); i++)
        {
            TBriefObject &obj = brf->Objects[i];

            float ndcX = obj.Pos.x / yw->_mapLength.x * brf->MapBlitEnd.Width() + brf->MapBlitEnd.left;
            float objX = (ndcX + 1.0) * 0.5;

            float ndcY = -obj.Pos.y / yw->_mapLength.y * brf->MapBlitEnd.Height() + brf->MapBlitEnd.top;
            float objY = 0.5 * (ndcY + 1.0);

            if ( objX + -0.025 < mx && objX + 0.025 > mx && objY + -0.025 < my && objY + 0.025 > my )
                hoverID = i;
        }

        if ( hoverID == -1 || hoverID == brf->SelectedObjID )
        {
            if ( hoverID != brf->SelectedObjID && brf->SelectedObjID != -1 )
            {
                brf->ViewingObject = TBriefObject();
                brf->SelectedObjID = -1;
            }
        }
        else
        {
            brf->SelectedObjID = hoverID;
            yw->BriefingSetObject( brf->Objects[hoverID], false);
        }
    }
}

void FormatBriefingText(NC_STACK_ypaworld *yw, TBriefengScreen *brf)
{
    briefingTextList.itemBlock.clear();

    if ( brf->TextTime > 0 )
    {
        int numLines = 1;
		size_t pos = brf->BriefingText.find_first_of("\n\r");
        while( pos != std::string::npos )
        {
        	pos = brf->BriefingText.find_first_of("\n\r", pos + 1);
            numLines++;
        }

        int lineTime = brf->CurrTime - brf->TextTime;
        int scaledTime = 100 * lineTime;

        briefingTextList.numEntries = numLines;

        briefingTextList.ItemsPreLayout(yw, &briefingTextList.itemBlock, 16, "   ");

        int lineIdx = 0;

        pos = 0;

        while ( 1 )
        {
            bool endline = false;

            size_t stpos = pos;
            pos = brf->BriefingText.find_first_of("\n\r", stpos);

            std::string line;

            if (pos == std::string::npos)
            {
                endline = true;
                line = brf->BriefingText.substr(stpos);
            }
            else
            {
                line = brf->BriefingText.substr(stpos, pos - stpos);
                pos++;
            }

            int percent = 0;

            if ( lineTime >= 500 )
            {
                percent = 100;
            }
            else
            {
                percent = scaledTime / 500;

                if ( percent < 0 )
                {
                    percent = 0;
                }
                else if ( percent > 100 )
                {
                    percent = 100;
                }
            }

            scaledTime -= 50000;
            lineTime -= 500;

            if ( briefingTextList.firstShownEntries <= lineIdx && briefingTextList.firstShownEntries + briefingTextList.shownEntries > lineIdx	&& percent > 0 )
            {
                FontUA::set_txtColor(&briefingTextList.itemBlock, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);

                FontUA::TextRelWidthItem(yw->_guiTiles[16], &briefingTextList.itemBlock, line.c_str(), percent, 4);

                FontUA::next_line(&briefingTextList.itemBlock);
            }

            lineIdx++;

            if ( endline )
            {
                briefingTextList.ItemsPostLayout(yw, &briefingTextList.itemBlock, 16, "   ");
                break;
            }
        }
    }
    else
    {
        int elapsed = brf->CurrTime - brf->PreTextTime;

        const std::string loadingText = Locale::Text::Advanced(Locale::ADV_LOADINGOBJ);

        int percent;

        if ( elapsed >= 600 )
            percent = 100;
        else
        {
            percent = 100 * elapsed / 600;

            if ( percent < 0 )
                percent = 0;
            else if ( percent > 100 )
                percent = 100;
        }

        if ( percent > 0 )
        {
            briefingTextList.ItemsPreLayout(yw, &briefingTextList.itemBlock, 16, "   ");

            FontUA::set_txtColor(&briefingTextList.itemBlock, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);

            FontUA::TextRelWidthItem(yw->_guiTiles[16], &briefingTextList.itemBlock, loadingText, percent, 4);

            briefingTextList.ItemsPostLayout(yw, &briefingTextList.itemBlock, 16, "   ");
        }
    }

    FontUA::set_end(&briefingTextList.itemBlock);
}

void BriefDrawObjectMarkers(NC_STACK_ypaworld *yw, TBriefengScreen *brf)
{
    CmdStream cmdStream;
    cmdStream.reserve(1024);

    int curTileSet = -1;

    for ( TBriefObject &obj : brf->Objects)
    {
        int visible = 1;

        if ( brf->ViewingObject && brf->ViewingObject == obj )
        {
            if ( (brf->CurrTime / 200) & 1 )
            {
                visible = 0;
            }
        }

        if ( visible )
        {
            if ( curTileSet != obj.TileSet)
            {
                curTileSet = obj.TileSet;
                FontUA::select_tileset(&cmdStream, obj.TileSet);
            }

            int halfSize = yw->_guiTiles[(int)obj.TileSet]->h / 2;

            float x = brf->MapBlitEnd.Width() * (obj.Pos.x / yw->_mapLength.x) + brf->MapBlitEnd.left;
            float y = brf->MapBlitEnd.Height() * (-obj.Pos.y / yw->_mapLength.y) + brf->MapBlitEnd.top;

            FontUA::set_center_xpos(&cmdStream, (x * (yw->_screenSize.x / 2)) - halfSize);
            FontUA::set_center_ypos(&cmdStream, (y * (yw->_screenSize.y / 2)) - halfSize);

            FontUA::store_u8(&cmdStream, obj.TileID);
        }
    }

    FontUA::set_end(&cmdStream);

    GFX::Engine.ProcessDrawSeq(cmdStream);
}

void BriefDrawLevelTitle(NC_STACK_ypaworld *yw)
{
    int w = yw->_screenSize.x / 2;
    int h = yw->_screenSize.y / 2;

    int xRight = w * 0.03125;
    int xLeft = w * -0.934375;
    int yTop = h * -0.9333333333333333;

    CmdStream buf;
    buf.reserve(260);

    FontUA::select_tileset(&buf, 16);
    FontUA::set_center_xpos(&buf, xLeft);
    FontUA::set_center_ypos(&buf, 4 + yTop);
    FontUA::set_txtColor(&buf, yw->_iniColors[66].r, yw->_iniColors[66].g, yw->_iniColors[66].b);

    const std::string levelName = yw->GetLevelName(yw->_levelInfo);

    FontUA::FormateCenteredSkipableItem(yw->_guiTiles[16], &buf, levelName, xRight - xLeft);
    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);
}

void BriefDrawViewedObjectCaption(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *inpt, int obj_id, char drawFlags)
{
    if ( brf->ViewingObject.ObjType )
    {
        int elapsed = brf->CurrTime - brf->ViewingObjectStartTime;
        float relY = -brf->ViewingObject.Pos.y / yw->_mapLength.y;
        float relX = brf->ViewingObject.Pos.x / yw->_mapLength.x;

        if ( elapsed > 50 )
        {
            if ( drawFlags & 1 )
            {
                Common::FLine horizLine;
                Common::FLine vertLine;

                vertLine.x1 = brf->MapBlitEnd.Width() * relX + brf->MapBlitEnd.left;
                vertLine.x2 = vertLine.x1;
                vertLine.y1 = brf->MapBlitEnd.Height() * relY + brf->MapBlitEnd.top;
                vertLine.y2 = brf->ViewingObjectRect.bottom;

                horizLine.y1 = vertLine.y2;
                horizLine.y2 = vertLine.y2;

                if ( obj_id >= 3 )
                {
                    horizLine.x1 = vertLine.x1;
                    horizLine.x2 = brf->ViewingObjectRect.right;
                }
                else
                {
                    horizLine.x1 = brf->ViewingObjectRect.left;
                    horizLine.x2 = vertLine.x1;
                }

                GFX::Engine.SetPen(GFX::Engine.Color(0, 160, 160));
                GFX::Engine.DrawLineNormalized(horizLine);

                GFX::Engine.SetPen(GFX::Engine.Color(0, 64, 64));
                GFX::Engine.DrawLineNormalized(vertLine);
            }
        }

        if ( elapsed > 50 && !brf->ViewingObject.Title.empty() )
        {
            if ( drawFlags & 2 )
            {
                int titleTime = elapsed - 50;
                int percent;

                if ( titleTime >= 500 )
                    percent = 100;
                else
                    percent = 100 * titleTime / 500;

                int xpos = ((brf->ViewingObjectRect.left + brf->ViewingObjectRect.right) / 2.0) * (yw->_screenSize.x / 2);
                int ypos = ((yw->_screenSize.y / 2) * brf->ViewingObjectRect.bottom - yw->_guiTiles[16]->h + -1.0);

                CmdStream cmdbuf;
                cmdbuf.reserve(128);

                FontUA::select_tileset(&cmdbuf, 16);
                FontUA::set_center_xpos(&cmdbuf, xpos);
                FontUA::set_center_ypos(&cmdbuf, ypos);
                FontUA::set_txtColor(&cmdbuf, yw->_iniColors[66].r, yw->_iniColors[66].g, yw->_iniColors[66].b);

                FontUA::TextRelWidthItem(yw->_guiTiles[16], &cmdbuf, brf->ViewingObject.Title.c_str(), percent, 16);

                FontUA::set_end(&cmdbuf);

                GFX::Engine.ProcessDrawSeq(cmdbuf);
            }
        }
    }
}

void UpdateBriefingScreen(NC_STACK_ypaworld *yw, UserData *usr, TInputState *inpt)
{
    TBriefengScreen *brf = &yw->_briefScreen;

    if ( yw->_briefScreen.Stage == TBriefengScreen::STAGE_MOVIE )
    {
        yw->PlayGameMovie(yw->_briefScreen.MovieStr);
        brf->Stage = TBriefengScreen::STAGE_LOADED;
    }
    else
    {

        switch ( yw->_briefScreen.TimerStatus )
        {
        case TBriefengScreen::TIMER_NORMAL:
            yw->_briefScreen.CurrTime += inpt->Period;
            break;

        case TBriefengScreen::TIMER_STOP:
            inpt->Period = 1;
            yw->_briefScreen.CurrTime += inpt->Period;
            break;

        case TBriefengScreen::TIMER_FAST:
            yw->_briefScreen.CurrTime += inpt->Period;

            if ( yw->_briefScreen.Stage == 30 )
            {
                yw->_briefScreen.Stage = 31;
                yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_NORMAL;
            }
            else if ( yw->_briefScreen.ActiveElementID == -1 )
            {
                yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_NORMAL;
            }
            else
            {
                switch (yw->_briefScreen.Stage)
                {
                case TBriefengScreen::STAGE_PLAYER_RN:
                case TBriefengScreen::STAGE_KEYS_RN:
                case TBriefengScreen::STAGE_TECH_RN:
                case TBriefengScreen::STAGE_ENMHS_RN:
                case TBriefengScreen::STAGE_ENMFRC_RN:
                case TBriefengScreen::STAGE_BUDDY_RN:
                case TBriefengScreen::STAGE_GATE_RN:
                    yw->_briefScreen.CurrTime = 2500 * (yw->_briefScreen.ActiveElementID + 1) + yw->_briefScreen.StartTime;
                    break;
                default:
                    break;
                }

                yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_NORMAL;
            }
            break;

        case TBriefengScreen::TIMER_RESTART:
            yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_NORMAL;
            yw->_briefScreen.Stage = 29;
            break;

        default:
            break;
        }

        switch ( yw->_briefScreen.Stage )
        {
        case TBriefengScreen::STAGE_LOADED:
            BriefStartMapScaling(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_SCALING:
            BriefUpdateMapScaling(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_SCALEEND:
            BriefLoadLevelSet(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_PLAYER_ST:
            BriefSetupPlayerHostStation(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_PLAYER_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdatePlayerHostStation(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_PLAYER_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_GATE_ST;
            break;
        case TBriefengScreen::STAGE_KEYS_ST:
            yw_BriefSetupKeySectors(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_KEYS_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                yw_BriefUpdateKeySectors(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_KEYS_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_TECH_ST;
            break;
        case TBriefengScreen::STAGE_TECH_ST:
            BriefSetupTechUpgrades(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_TECH_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdateTechUpgrades(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_TECH_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_ENMHS_ST;
            break;
        case TBriefengScreen::STAGE_ENMHS_ST:
            BriefSetupEnemyHostStations(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_ENMHS_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdateEnemyHostStations(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_ENMHS_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_ENMFRC_ST;
            break;
        case TBriefengScreen::STAGE_ENMFRC_ST:
            BriefSetupEnemyForces(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_ENMFRC_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdateEnemyForces(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_ENMFRC_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_BUDDY_ST;
            break;
        case TBriefengScreen::STAGE_BUDDY_ST:
            BriefSetupFriendlyForces(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_BUDDY_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdateFriendlyForces(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_BUDDY_END:
            yw->_briefScreen.AddObjectsFlag = false;
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_PLAYER_ST;
            break;
        case TBriefengScreen::STAGE_GATE_ST:
            BriefSetupGates(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_GATE_RN:
            if ( yw->_briefScreen.SelectedObjID == -1 )
                BriefUpdateGates(yw, inpt, brf);
            break;
        case TBriefengScreen::STAGE_GATE_END:
            yw->_briefScreen.Stage = TBriefengScreen::STAGE_KEYS_ST;
            break;
        default:
            break;
        }

        briefingTextList.InputHandle(yw, inpt);
        briefingTextList.Formate(yw);

        FormatBriefingText(yw, brf);

        BriefSelectObjectUnderCursor(yw, inpt, brf);

        if ( brf->BriefingMapImg )
        {
            GFX::TBitmapBlitParams backgroundBlit;

            backgroundBlit.pbitm = brf->BriefingMapImg->GetBitmap();

            backgroundBlit.srcRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);
            backgroundBlit.dstRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);

            GFX::Engine.DrawClippedBitmapNormalized(&backgroundBlit);
        }

        GFX::Engine.DrawClippedBitmapNormalized(&brf->MapBlitParams);

        GFX::Engine.draw2DandFlush();

        ypaworld_func158__DrawVehicle(yw, brf, inpt);

        BriefDrawObjectMarkers(yw, brf);
        BriefDrawLevelTitle(yw);

        GFX::Engine.ProcessDrawSeq(briefingTextList.cmdCommands, &briefingTextList.cmdInclude);

        BriefDrawViewedObjectCaption(yw, brf, inpt, 0, 2);
    }
}




// Debrif


void ClampRectToUnit(Common::FRect *rect)
{
    if ( rect->left > 1.0 )
        rect->left = 1.0;
    else if ( rect->left < -1.0 )
        rect->left = -1.0;

    if ( rect->top > 1.0 )
        rect->top = 1.0;
    else if ( rect->top < -1.0 )
        rect->top = -1.0;

    if ( rect->right > 1.0 )
        rect->right = 1.0;
    else if ( rect->right < -1.0 )
        rect->right = -1.0;

    if ( rect->bottom > 1.0 )
        rect->bottom = 1.0;
    else if ( rect->bottom < -1.0 )
        rect->bottom = -1.0;
}

void DebriefStartMapScaling(NC_STACK_ypaworld *yw, TInputState *struc, TBriefengScreen *brf)
{
    brf->StartTime = brf->CurrTime;
    brf->Stage = TBriefengScreen::STAGE_SCALING;

    brf->MapBlitParams.pbitm = brf->MbmapImg->GetBitmap();

    float aspectX, aspectY;

    if ( yw->_mapSize.x > yw->_mapSize.y )
    {
        aspectX = 1.0;
        aspectY = (float)yw->_mapSize.y / (float)yw->_mapSize.x;
    }
    else if ( yw->_mapSize.y > yw->_mapSize.x )
    {
        aspectY = 1.0;
        aspectX = (float)yw->_mapSize.x / (float)yw->_mapSize.y;
    }
    else
    {
        aspectX = 1.0;
        aspectY = 1.0;
    }

    float halfSectorH = 1.0 / (float)yw->_mapSize.y;
    float halfSectorW = 1.0 / (float)yw->_mapSize.x;

    Common::Point tmp;
    if ( brf->ZoomFromGate )
    {
        tmp = yw->_levelInfo.Gates[ yw->_levelInfo.GateCompleteID ].CellId;
    }
    else
    {
        tmp = {yw->_mapSize.x / 2, yw->_mapSize.y / 2};
    }

    float centerY = 2.0 * ((float)tmp.y / (float)yw->_mapSize.y) - 1.0;
    float centerX = 2.0 * ((float)tmp.x / (float)yw->_mapSize.x) - 1.0;

    brf->MapBlitStart.left = centerX - halfSectorW;
    brf->MapBlitStart.top = centerY - halfSectorH;
    brf->MapBlitStart.right = halfSectorW + centerX;
    brf->MapBlitStart.bottom = centerY + halfSectorH;
    ClampRectToUnit(&brf->MapBlitStart);

    brf->MapBlitEnd.left = -0.4515625 - aspectX * 0.4828125;
    brf->MapBlitEnd.top = -0.325 - aspectY * 0.6083333333333334;
    brf->MapBlitEnd.right = brf->MapBlitEnd.left + aspectX * 0.965625;
    brf->MapBlitEnd.bottom = brf->MapBlitEnd.top + aspectY * 1.216666666666667;
    ClampRectToUnit(&brf->MapBlitEnd);

    brf->MapBlitParams.srcRect = brf->MapBlitStart;
    brf->MapBlitParams.dstRect = brf->MapBlitEnd;

    if ( yw->_GameShell )
        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 11);
}

void DebriefUpdateMapScaling(NC_STACK_ypaworld *yw, TInputState *struc, TBriefengScreen *brf)
{
    int elapsed = brf->CurrTime - brf->StartTime;

    if ( elapsed >= 600 )
    {
        brf->MapBlitParams.srcRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        brf->MapBlitParams.dstRect = brf->MapBlitEnd;
        brf->Stage = TBriefengScreen::STAGE_SCALEEND;
    }
    else
    {
        float progress = elapsed / 600.0;
        brf->MapBlitParams.srcRect.left = brf->MapBlitStart.left + (-1.0 - brf->MapBlitStart.left) * progress;
        brf->MapBlitParams.srcRect.top = brf->MapBlitStart.top + (-1.0 - brf->MapBlitStart.top) * progress;
        brf->MapBlitParams.srcRect.right = brf->MapBlitStart.right + (1.0 - brf->MapBlitStart.right) * progress;
        brf->MapBlitParams.srcRect.bottom = brf->MapBlitStart.bottom + (1.0 - brf->MapBlitStart.bottom) * progress;
        brf->MapBlitParams.dstRect = brf->MapBlitEnd;
    }
}

void DebriefStartHistoryReplay(NC_STACK_ypaworld *yw, TInputState *struc, TBriefengScreen *brf)
{
    brf->StartTime = brf->CurrTime;
    brf->Stage = TBriefengScreen::STAGE_PLAYER_RN;
    brf->ReplayedTime = 0;
    brf->EventsReplayed = 0;

    brf->OwnMap = yw->_lvlPrimevalOwnMap;
    brf->TypMap = yw->_lvlPrimevalTypeMap;

    for (int i = 0; i < 8; i++)
        brf->StatsGlobal[i] = yw->_playersStats[i];

    brf->StatsIngame.fill(World::TPlayerStatus());
    
    brf->Upgrades.clear();
}

// A sector taken, marked for a while after
void yw_DebriefDrawConquest(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &e, uint32_t time)
{
    uint32_t dtime = time - e.Time;

    if ( dtime < 30000 )
    {
        if ( e.Owner )
        {
            if ( brf->VectorGfx[3] )
            {
                float sectorW = brf->MapBlitEnd.Width() / (float)yw->_mapSize.x;
                float sectorH = brf->MapBlitEnd.Height() / (float)yw->_mapSize.y;

                float posX = (float)e.SectorX * sectorW + brf->MapBlitEnd.left + sectorW * 0.5;
                float posY = (float)e.SectorY * sectorH + brf->MapBlitEnd.top + sectorH * 0.5;

                float sizeFactor = 1.0 - (float )dtime / 30000.0;

                if ( sizeFactor > 1.0 )
                    sizeFactor = 1.0;
                else if ( sizeFactor < 0.0)
                    sizeFactor = 0.0;

                float scaleX = sectorW * sizeFactor;
                float scaleY = sectorH * sizeFactor;

                yw_RenderVector2D(yw, brf->VectorGfx[3]->GetSkelet(), posX, posY, 1.0, 0.0, 0.0, 1.0, scaleX, scaleY,  yw->GetColor(e.Owner), NULL, NULL);
            }
        }
    }
}

// A unit killed, marked in its owner's colour for a while after
void yw_DebriefDrawKill(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &e, uint32_t time)
{
    uint32_t dtime = time - e.Time;

    if ( dtime < 120000 )
    {
        if ( brf->VectorGfx[1] )
        {
            float mapW = brf->MapBlitEnd.Width();
            float mapH = brf->MapBlitEnd.Height();

            float posX = mapW * (e.MapX / 256.0) + brf->MapBlitEnd.left;
            float posY = mapH * (e.MapY / 256.0) + brf->MapBlitEnd.top;

            float sizeFactor;

            if ( dtime >= 30000 )
            {
                sizeFactor = 0.1;
            }
            else
            {
                sizeFactor = 1.0 - dtime / 30000.0;

                if ( sizeFactor < 0.1 )
                    sizeFactor = 0.1;
                else if ( sizeFactor > 1.0 )
                    sizeFactor = 1.0;
            }

            float scaleX = ((mapW / (float)yw->_mapSize.x) / 2.0) * sizeFactor;
            float scaleY = ((mapH / (float)yw->_mapSize.y) / 2.0) * sizeFactor;

            yw_RenderVector2D(yw, brf->VectorGfx[1]->GetSkelet(), posX, posY, 1.0, 0.0, 0.0, 1.0, scaleX, scaleY, yw->GetColor(e.Owner), NULL, NULL);
        }
    }
}

// A unit made, growing for a while after
void yw_DebriefDrawCreation(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &e, uint32_t time)
{
    int dtime = time - e.Time;
    if ( dtime < 45000 )
    {
        if ( brf->VectorGfx[1] )
        {
            float mapW = brf->MapBlitEnd.Width();
            float mapH = brf->MapBlitEnd.Height();

            float posX = mapW * (e.MapX / 256.0) + brf->MapBlitEnd.left;
            float posY = mapH * (e.MapY / 256.0) + brf->MapBlitEnd.top;

            float sizeFactor = dtime / 45000.0;

            if ( sizeFactor > 1.0 )
                sizeFactor = 1.0;
            else if ( sizeFactor < 0.0 )
                sizeFactor = 0.0;

            float scaleX = ((mapW / (float)yw->_mapSize.x) / 8.0) * sizeFactor;
            float scaleY = ((mapH / (float)yw->_mapSize.y) / 8.0) * sizeFactor;

            yw_RenderVector2D(yw, brf->VectorGfx[1]->GetSkelet(), posX, posY, 1.0, 0.0, 0.0, 1.0, scaleX, scaleY, yw->GetColor(e.Owner), NULL, NULL);
        }
    }
}

void yw_DebriefAddTechUpgrade(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &e)
{
    if ( !yw->_gameWasNetGame || e.Owner == yw->_userOwnerIdWasInNetGame )
    {
        for (auto &u : brf->Upgrades)
        {
            if ( u.UpgradeType == e.UpgradeType && u.UpgradeVehicle == e.UpgradeVehicle &&
                 u.UpgradeWeapon == e.UpgradeWeapon && u.UpgradeBuilding == e.UpgradeBuilding )
                return;
        }

        //CHECKME if it needed to limit
        /*if ( brf->tp1_count >= 7 ) 
            brf->tp1_count = 6;*/
        
        brf->Upgrades.push_back(e);
    }
}

// An event the replay of the level's history has come to: counted, heard,
// and the map changed by it
void yw_DebriefEventReached(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &e)
{
    World::History::AddScore(e, brf->StatsIngame);

    int sound = -1;

    switch ( e.What )
    {
    case World::History::Event::CONQUEST:
        brf->OwnMap(e.SectorX, e.SectorY) = e.Owner;
        sound = 12;
        break;

    case World::History::Event::KILL:
        sound = 13;
        break;

    case World::History::Event::POWER_STATION:
        sound = 14;
        break;

    case World::History::Event::TECH_UPGRADE:
        yw_DebriefAddTechUpgrade(yw, brf, e);
        sound = 14;
        break;

    case World::History::Event::CREATION:
        break;
    }

    if ( sound >= 0 && yw->_GameShell )
        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, sound);
}

void yw_DebriefRenderSectorsOwners(NC_STACK_ypaworld *yw, TBriefengScreen *brf)
{
    float sectorW = brf->MapBlitEnd.Width() / (float)yw->_mapSize.x;
    float sectorH = brf->MapBlitEnd.Height() / (float)yw->_mapSize.y;

    float crossHalfW = sectorW / 10.0;
    float crossHalfH = sectorH / 10.0;

    float centerY = brf->MapBlitEnd.top + sectorH * 0.5;

    for (int yy = 0; yy < yw->_mapSize.y; yy++)
    {
        float centerX = brf->MapBlitEnd.left + sectorW * 0.5;
        
        uint8_t *ownmap = brf->OwnMap.Line(yy);

        for (int xx = 0; xx < yw->_mapSize.x; xx++)
        {
            int owner = *ownmap;

            if ( owner != 0 )
            {
                if ( yw->IsGamePlaySector( {xx, yy} ) )
                {
                    Common::FLine hLine;
                    Common::FLine vLine;

                    hLine.x1 = centerX - crossHalfW;
                    hLine.y1 = centerY;
                    hLine.x2 = crossHalfW + centerX;
                    hLine.y2 = centerY;

                    vLine.x1 = centerX;
                    vLine.y1 = centerY - crossHalfH;
                    vLine.x2 = centerX;
                    vLine.y2 = centerY + crossHalfH;

                    GFX::Engine.SetPen( yw->GetColor(owner) );
                    GFX::Engine.DrawLineNormalized(hLine);
                    GFX::Engine.DrawLineNormalized(vLine);
                }
            }

            centerX += sectorW;
            ownmap++;
        }

        centerY += sectorH;
    }
}


void DebriefDrawLevelTitle(NC_STACK_ypaworld *yw, TBriefengScreen *brf)
{
    int xLeft = (yw->_screenSize.x / 2) * -0.934375;
    int xRight = (yw->_screenSize.x / 2) * 0.03125;
    int yTop = (yw->_screenSize.y / 2) * -0.9333333333333333;

    const std::string levelName = yw->GetLevelName(yw->_levelInfo);

    CmdStream cmdBuff;
    cmdBuff.reserve(264);

    FontUA::select_tileset(&cmdBuff, 16);
    FontUA::set_center_xpos(&cmdBuff, xLeft);
    FontUA::set_center_ypos(&cmdBuff, yTop);

    FontUA::set_txtColor(&cmdBuff, yw->_iniColors[66].r, yw->_iniColors[66].g, yw->_iniColors[66].b);

    FontUA::FormateCenteredSkipableItem(yw->_guiTiles[16], &cmdBuff, levelName, xRight - xLeft);

    FontUA::set_end(&cmdBuff);

    GFX::Engine.ProcessDrawSeq(cmdBuff);
}

void yw_DebriefKillsTitleLine(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in, int tableWidth)
{
    FontUA::set_txtColor(in, yw->_iniColors[67].r, yw->_iniColors[67].g, yw->_iniColors[67].b);
    FontUA::ColumnItem elms[3];

    elms[0].txt = Locale::Text::Advanced(Locale::ADV_KILLSFACTION);
    elms[0].spaceChar = 32;
    elms[0].prefixChar = 0;
    elms[0].postfixChar = 0;
    elms[0].width = tableWidth * 0.4;
    elms[0].flags = 36;
    elms[0].fontID = 15;

    elms[1].txt = Locale::Text::Advanced(Locale::ADV_KILLSBYPLAYER);
    elms[1].fontID = 15;
    elms[1].width = tableWidth * 0.3;
    elms[1].spaceChar = 32;
    elms[1].flags = 36;
    elms[1].prefixChar = 0;
    elms[1].postfixChar = 0;

    elms[2].txt = Locale::Text::Advanced(Locale::ADV_KILLSALL);
    elms[2].width = tableWidth * 0.3;
    elms[2].fontID = 15;
    elms[2].spaceChar = 32;
    elms[2].flags = 36;
    elms[2].prefixChar = 0;
    elms[2].postfixChar = 0;

    FormateColumnItem(yw, in, 3, elms);
}

int CompareDebriefStatusDesc(const void *lhs, const void *rhs)
{
    return ((debrif_t1 *)rhs)->status - ((debrif_t1 *)lhs)->status;
}

void yw_DebriefKillsScore(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in, int tableWidth)
{
    int count = 0;

    debrif_t1 entries[8];

    for (int i = 0; i < 8; i++)
    {
        if ( (1 << i) & yw->_levelInfo.OwnerMask )
        {
            entries[count].owner = i;
            entries[count].status = brf->StatsIngame[i].DestroyedUnits;
            count++;
        }
    }

    qsort(entries, count, sizeof(debrif_t1), CompareDebriefStatusDesc);


    for (int i = 0; i < count; i++)
    {
        int clr_id;
        std::string who;

        switch ( entries[i].owner )
        {
        case 1:
            who = Locale::Text::Advanced(Locale::ADV_RESIST);
            clr_id = 1;
            break;

        case 2:
            who = Locale::Text::Advanced(Locale::ADV_SULG);
            clr_id = 2;
            break;

        case 3:
            who = Locale::Text::Advanced(Locale::ADV_MYKO);
            clr_id = 3;
            break;

        case 4:
            who = Locale::Text::Advanced(Locale::ADV_TAER);
            clr_id = 4;
            break;

        case 5:
            who = Locale::Text::Advanced(Locale::ADV_BLKSCT);
            clr_id = 5;
            break;

        case 6:
            who = Locale::Text::Advanced(Locale::ADV_GHOR);
            clr_id = 6;
            break;

        default:
            who = Locale::Text::Advanced(Locale::ADV_NEUTRAL);
            clr_id = 7;
            break;
        }

        FontUA::ColumnItem elms[3];

        FontUA::set_txtColor(in, yw->_iniColors[clr_id].r, yw->_iniColors[clr_id].g, yw->_iniColors[clr_id].b);

        elms[0].txt = who;
        elms[0].spaceChar = 32;
        elms[0].fontID = 15;
        elms[0].width = tableWidth * 0.4;
        elms[0].prefixChar = 0;
        elms[0].postfixChar = 0;
        elms[0].flags = 36;

        if ( yw->_gameWasNetGame || entries[i].owner == yw->_playerOwner )
            elms[1].txt = fmt::sprintf("%d", brf->StatsIngame[ entries[i].owner ].DestroyedByUser);
        else
            elms[1].txt = "-";
        elms[1].spaceChar = 32;
        elms[1].fontID = 15;
        elms[1].prefixChar = 0;
        elms[1].postfixChar = 0;
        elms[1].flags = 36;
        elms[1].width = tableWidth * 0.3;

        elms[2].txt = fmt::sprintf("%d", brf->StatsIngame[ entries[i].owner ].DestroyedUnits);
        elms[2].width = tableWidth * 0.3;
        elms[2].fontID = 15;
        elms[2].spaceChar = 32;
        elms[2].flags = 36;
        elms[2].prefixChar = 0;
        elms[2].postfixChar = 0;

        FormateColumnItem(yw, in, 3, elms);

        FontUA::next_line(in);
    }
}

void yw_DebriefMPlayScoreTitle(NC_STACK_ypaworld *yw, CmdStream *in, int tableWidth)
{
    FontUA::set_txtColor(in, yw->_iniColors[67].r, yw->_iniColors[67].g, yw->_iniColors[67].b);

    FontUA::ColumnItem elm;

    elm.txt = Locale::Text::Advanced(Locale::ADV_SCORE);
    elm.width = tableWidth;
    elm.spaceChar = 32;
    elm.fontID = 15;
    elm.prefixChar = 0;
    elm.postfixChar = 0;
    elm.flags = 36;

    FormateColumnItem(yw, in, 1, &elm);
}

void yw_DebriefMPlayScore(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in, int tableWidth)
{
    if ( yw->_gameWasNetGame )
    {
        yw_DebriefMPlayScoreTitle(yw, in, tableWidth);

        FontUA::next_line(in);

        debrif_t1 entries[8];
        int count = 0;

        for (int i = 0; i < 8; i++)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
            {
                entries[count].owner = i;
                entries[count].status = brf->StatsIngame[i].Score;
                count++;
            }
        }

        qsort(entries, count, sizeof(debrif_t1), CompareDebriefStatusDesc);

        for (int i = 0; i < count; i++)
        {
            int clr_id;
            std::string who;

            switch ( entries[i].owner )
            {
            case 1:
                who = Locale::Text::Advanced(Locale::ADV_RESIST);
                clr_id = 1;
                break;

            case 2:
                who = Locale::Text::Advanced(Locale::ADV_SULG);
                clr_id = 2;
                break;

            case 3:
                who = Locale::Text::Advanced(Locale::ADV_MYKO);
                clr_id = 3;
                break;

            case 4:
                who = Locale::Text::Advanced(Locale::ADV_TAER);
                clr_id = 4;
                break;

            case 5:
                who = Locale::Text::Advanced(Locale::ADV_BLKSCT);
                clr_id = 5;
                break;

            case 6:
                who = Locale::Text::Advanced(Locale::ADV_GHOR);
                clr_id = 6;
                break;

            default:
                who = Locale::Text::Advanced(Locale::ADV_NEUTRAL);
                clr_id = 7;
                break;
            }

            FontUA::set_txtColor(in, yw->_iniColors[clr_id].r, yw->_iniColors[clr_id].g, yw->_iniColors[clr_id].b);

            FontUA::ColumnItem columns[2];

            columns[0].txt = who;
            columns[0].spaceChar = 32;
            columns[0].fontID = 15;
            columns[0].prefixChar = 0;
            columns[0].postfixChar = 0;
            columns[0].flags = 36;
            columns[0].width = tableWidth * 0.5;

            columns[1].txt = fmt::sprintf("%d", brf->StatsIngame[ entries[i].owner ].Score);
            columns[1].width = tableWidth * 0.5;
            columns[1].fontID = 15;
            columns[1].spaceChar = 32;
            columns[1].flags = 36;
            columns[1].prefixChar = 0;
            columns[1].postfixChar = 0;

            FormateColumnItem(yw, in, 2, columns);

            FontUA::next_line(in);
        }
    }
    else
    {
        FontUA::set_txtColor(in, yw->_iniColors[67].r, yw->_iniColors[67].g, yw->_iniColors[67].b);

        FontUA::ColumnItem columns[2];

        columns[0].txt = Locale::Text::Advanced(Locale::ADV_SCOREMISSION);
        columns[0].spaceChar = 32;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].width = tableWidth * 0.7;
        columns[0].fontID = 15;
        columns[0].flags = 36;

        columns[1].txt = fmt::sprintf("%d", brf->StatsIngame[yw->_playerOwner].Score);
        columns[1].fontID = 15;
        columns[1].spaceChar = 32;
        columns[1].prefixChar = 0;
        columns[1].width = tableWidth * 0.3;
        columns[1].postfixChar = 0;
        columns[1].flags = 36;

        FormateColumnItem(yw, in, 2, columns);

        FontUA::next_line(in);

        columns[0].txt = Locale::Text::Advanced(Locale::ADV_SCOREOVERALL);
        columns[0].flags = 36;
        columns[0].spaceChar = 32;
        columns[0].width = tableWidth * 0.7;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].fontID = 15;

        

        columns[1].txt = fmt::sprintf("%d", brf->StatsIngame[yw->_playerOwner].Score + brf->StatsGlobal[yw->_playerOwner].Score);
        columns[1].fontID = 15;
        columns[1].spaceChar = 32;
        columns[1].postfixChar = 0;
        columns[1].width = tableWidth * 0.3;
        columns[1].prefixChar = 0;
        columns[1].flags = 36;

        FormateColumnItem(yw, in, 2, columns);

        FontUA::next_line(in);
    }
}

void yw_DebriefRenderTime(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in, int tableWidth)
{
    FontUA::set_txtColor(in, yw->_iniColors[67].r, yw->_iniColors[67].g, yw->_iniColors[67].b);

    if ( yw->_gameWasNetGame )
    {
        FontUA::ColumnItem columns[2];
        columns[0].txt = Locale::Text::Advanced(Locale::ADV_PLAYTIME);
        columns[0].spaceChar = 32;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].width = tableWidth * 0.7;
        columns[0].flags = 36;
        columns[0].fontID = 15;

        int seconds = brf->ReplayedTime / 1024;

        columns[1].txt = fmt::sprintf("%02d:%02d:%02d", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
        columns[1].fontID = 15;
        columns[1].spaceChar = 32;
        columns[1].prefixChar = 0;
        columns[1].postfixChar = 0;
        columns[1].flags = 36;
        columns[1].width = tableWidth * 0.3;

        FormateColumnItem(yw, in, 2, columns);
    }
    else
    {

        FontUA::ColumnItem columns[2];
        columns[0].txt = Locale::Text::Advanced(Locale::ADV_MISSIONTIME);
        columns[0].spaceChar = 32;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].width = tableWidth * 0.7;
        columns[0].flags = 36;
        columns[0].fontID = 15;

        int seconds = brf->ReplayedTime / 1024;

        columns[1].txt = fmt::sprintf("%02d:%02d:%02d", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
        columns[1].width = tableWidth * 0.3;
        columns[1].fontID = 15;
        columns[1].spaceChar = 32;
        columns[1].prefixChar = 0;
        columns[1].postfixChar = 0;
        columns[1].flags = 36;

        FormateColumnItem(yw, in, 2, columns);

        FontUA::next_line(in);

        columns[0].txt = Locale::Text::Advanced(Locale::ADV_OVERALLTIME);
        columns[0].flags = 36;
        columns[0].spaceChar = 32;
        columns[0].width = tableWidth * 0.7;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].fontID = 15;

        seconds = (brf->ReplayedTime + brf->StatsGlobal[yw->_playerOwner].ElapsedTime) / 1024;

        columns[1].txt = fmt::sprintf("%02d:%02d:%02d", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
        columns[1].fontID = 15;
        columns[1].spaceChar = 32;
        columns[1].width = tableWidth * 0.3;
        columns[1].prefixChar = 0;
        columns[1].postfixChar = 0;
        columns[1].flags = 36;

        FormateColumnItem(yw, in, 2, columns);
    }

    FontUA::next_line(in);
}

void yw_DebriefScoreTable(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in)
{
    FontUA::select_tileset(in, 15);
    FontUA::set_center_xpos(in, ((yw->_screenSize.x / 2) * 0.15) );
    FontUA::set_center_ypos(in, ((yw->_screenSize.y / 2) * -0.821) );

    int tableWidth = (yw->_screenSize.x / 2) * 0.796875;

    yw_DebriefKillsTitleLine(yw, brf, in, tableWidth);

    FontUA::next_line(in);

    yw_DebriefKillsScore(yw, brf, in, tableWidth);

    FontUA::next_line(in);

    yw_DebriefMPlayScore(yw, brf, in, tableWidth);

    FontUA::next_line(in);

    yw_DebriefRenderTime(yw, brf, in, tableWidth);
}

void yw_DebriefTechUpgradeLine(NC_STACK_ypaworld *yw, TBriefengScreen *brf, const World::History::Event &upg, CmdStream *in, int lineWidth)
{
    int lastWeapon = upg.UpgradeWeapon;
    int lastVhcl   = upg.UpgradeVehicle;
    int lastBuild  = upg.UpgradeBuilding;

    if ( lastVhcl == 0 && lastWeapon != 0 )
    {
        int i = 0;
        for (const World::TVhclProto &vhcl : yw->_vhclProtos)
        {
            if (vhcl.weapon == lastWeapon)
            {
                lastVhcl = i;
                break;
            }
            
            i++;
        }

        if ( !lastVhcl )
            return;
    }

    if ( lastWeapon == 0 && lastVhcl != 0 && lastVhcl < (int32_t)yw->_vhclProtos.size() )
    {
        lastWeapon = yw->_vhclProtos[ lastVhcl ].weapon;

        if ( lastWeapon == -1 )
            lastWeapon = 0;
    }

    World::TVhclProto *vhcl = NULL;
    World::TWeapProto *wpn  = NULL;
    World::TBuildingProto *bld = NULL;

    if ( lastVhcl && lastVhcl < (int32_t)yw->_vhclProtos.size() )
        vhcl = &yw->_vhclProtos.at(lastVhcl);

    if ( lastBuild && lastBuild < (int32_t)yw->_buildProtos.size() )
        bld = &yw->_buildProtos.at(lastBuild);

    if ( lastWeapon && lastWeapon < (int32_t)yw->_weaponProtos.size() )
        wpn = &yw->_weaponProtos.at(lastWeapon);

    std::string typeText = " ";
    std::string detailText = " ";
    std::string nameText = " ";

    if ( vhcl )
    {
        nameText = yw->GetVehicleName(*vhcl);
    }
    else if ( bld )
    {
        if ( yw->_gameWasNetGame )
            nameText = yw->GetBuildingName(*bld, true);
        else
            nameText = yw->GetBuildingName(*bld, false);
    }

    switch ( upg.UpgradeType )
    {
    case World::UPGRADE_WEAPON:
        if ( wpn )
        {
            if ( vhcl )
            {
                typeText = Locale::Text::Advanced(Locale::ADV_WEAPONUPG);

                if ( vhcl->num_weapons > 1 )
                    detailText = fmt::sprintf("(%d x%d)", wpn->energy / 100, vhcl->num_weapons);
                else
                    detailText = fmt::sprintf("(%d)", wpn->energy / 100);
            }
        }
        break;

    case World::UPGRADE_ARMOR:
        if ( vhcl )
        {
            typeText = Locale::Text::Advanced(Locale::ADV_ARMORUPG);

            detailText = fmt::sprintf("(%d%%)", vhcl->shield);
        }
        break;

    case World::UPGRADE_VEHICLE:
        if ( vhcl )
        {
            typeText = Locale::Text::Advanced(Locale::ADV_NEWVEHICLE);
        }
        break;

    case World::UPGRADE_BUILDING:
        if ( bld )
        {
            typeText = Locale::Text::Advanced(Locale::ADV_NEWBUILDING);
        }
        break;

    case World::UPGRADE_RADARE:
        if ( vhcl )
        {
            typeText = Locale::Text::Advanced(Locale::ADV_RADARUPG);
        }
        break;

    case World::UPGRADE_BLDVHCL:
        if ( vhcl )
        {
            if ( bld )
            {
                if ( yw->_gameWasNetGame )
                    detailText = yw->GetBuildingName(*bld, true);
                else
                    detailText = yw->GetBuildingName(*bld, false);

                typeText = Locale::Text::Advanced(Locale::ADV_COMBINEDUPG);
            }
        }
        break;

    case World::UPGRADE_GENERIC:
        typeText = Locale::Text::Advanced(Locale::ADV_GENERICUPG);
        break;

    default:
        break;
    }

    FontUA::ColumnItem elm[3];

    elm[0].txt = typeText;
    elm[0].spaceChar = 32;
    elm[0].flags = 36;
    elm[0].prefixChar = 0;
    elm[0].postfixChar = 0;
    elm[0].width = lineWidth * 0.48;
    elm[0].fontID = 15;

    elm[1].flags = 36;
    elm[1].fontID = 15;
    elm[1].spaceChar = 32;
    elm[1].prefixChar = 0;
    elm[1].postfixChar = 0;
    elm[1].txt = nameText;
    elm[1].width = lineWidth * 0.3;

    elm[2].flags = 36;
    elm[2].fontID = 15;
    elm[2].spaceChar = 32;
    elm[2].prefixChar = 0;
    elm[2].postfixChar = 0;
    elm[2].width = lineWidth * 0.22;
    elm[2].txt = detailText;

    FontUA::FormateColumnItem(yw, in, 3, elm);

    FontUA::next_line(in);
}

void yw_DebriefTechUpgradesTable(NC_STACK_ypaworld *yw, TBriefengScreen *brf, CmdStream *in)
{
    FontUA::select_tileset(in, 15);
    FontUA::set_center_xpos(in, ((yw->_screenSize.x / 2) * -0.9875) );
    FontUA::set_center_ypos(in, ((yw->_screenSize.y / 2) * 0.35) );

    FontUA::set_txtColor(in, yw->_iniColors[67].r, yw->_iniColors[67].g, yw->_iniColors[67].b);

    for ( const auto &upg : brf->Upgrades )
        yw_DebriefTechUpgradeLine(yw, brf, upg, in,  (yw->_screenSize.x / 2) * 0.984375 );
}

void yw_DebriefRunDebrief(NC_STACK_ypaworld *yw, TInputState *struc, TBriefengScreen *brf)
{
    CmdStream cmdbuf;
    cmdbuf.reserve(2048);
    
    uint32_t dtime = brf->CurrTime - brf->StartTime;

    yw_DebriefRenderSectorsOwners(yw, brf);

    yw_DebriefScoreTable(yw, brf, &cmdbuf);
    yw_DebriefTechUpgradesTable(yw, brf, &cmdbuf);

    FontUA::set_end(&cmdbuf);

    GFX::Engine.ProcessDrawSeq(cmdbuf);

    if ( brf->Stage == TBriefengScreen::STAGE_PLAYER_RN )
    {
        // The level's history replayed: what happened before the replay's
        // time, the newly come to counted and heard
        const std::vector<World::History::Event> &events = yw->_history;
        size_t reached = brf->EventsReplayed;

        while ( reached < events.size() && events[reached].Time < dtime )
        {
            yw_DebriefEventReached(yw, brf, events[reached]);
            brf->ReplayedTime = events[reached].Time;
            reached++;
        }

        brf->EventsReplayed = reached;

        for (size_t i = 0; i < reached; i++)
        {
            const World::History::Event &e = events[i];

            switch ( e.What )
            {
            case World::History::Event::CONQUEST:
                yw_DebriefDrawConquest(yw, brf, e, dtime);
                break;

            case World::History::Event::KILL:
                yw_DebriefDrawKill(yw, brf, e, dtime);
                break;

            case World::History::Event::CREATION:
                yw_DebriefDrawCreation(yw, brf, e, dtime);
                break;

            default:
                break;
            }
        }

        if ( reached == events.size() )
        {
            brf->Stage = TBriefengScreen::STAGE_PLAYER_END;
            if ( yw->_gameWasNetGame )
            {
                brf->StatsIngame = yw->_gameplayStats;
            }
        }
    }

    DebriefDrawLevelTitle(yw, brf);
}

void yw_debriefUpdate(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TBriefengScreen *brf = &yw->_briefScreen;

    if ( yw->_briefScreen.TimerStatus == TBriefengScreen::TIMER_NORMAL )
    {
        if ( brf->Stage == TBriefengScreen::STAGE_PLAYER_RN )
        {
            brf->CurrTime += 60 * inpt->Period;
        }
        else if ( brf->Stage != TBriefengScreen::STAGE_PLAYER_END )
        {
            brf->CurrTime += inpt->Period;
        }
    }
    else if ( yw->_briefScreen.TimerStatus == TBriefengScreen::TIMER_STOP )
    {
        inpt->Period = 1;
    }
    else if ( yw->_briefScreen.TimerStatus == TBriefengScreen::TIMER_RESTART )
    {
        brf->TimerStatus = TBriefengScreen::TIMER_NORMAL;
        brf->Stage = TBriefengScreen::STAGE_PLAYER_ST;
    }

    if ( brf->Stage != TBriefengScreen::STAGE_LOADED )
    {
        if ( brf->BriefingMapImg )
        {
            GFX::TBitmapBlitParams backgroundBlit;
            backgroundBlit.pbitm = brf->BriefingMapImg->GetBitmap();

            backgroundBlit.srcRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);
            backgroundBlit.dstRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);

            GFX::Engine.DrawClippedBitmapNormalized(&backgroundBlit);
        }

        GFX::Engine.DrawClippedBitmapNormalized(&brf->MapBlitParams);
    }

    switch ( brf->Stage )
    {
    case TBriefengScreen::STAGE_LOADED:
        DebriefStartMapScaling(yw, inpt, brf);
        break;

    case TBriefengScreen::STAGE_SCALING:
        DebriefUpdateMapScaling(yw, inpt, brf);
        break;

    case TBriefengScreen::STAGE_SCALEEND:
        brf->Stage = TBriefengScreen::STAGE_PLAYER_ST;
        break;

    case TBriefengScreen::STAGE_PLAYER_ST:
        DebriefStartHistoryReplay(yw, inpt, brf);
        break;

    case TBriefengScreen::STAGE_PLAYER_RN:
    case TBriefengScreen::STAGE_PLAYER_END:
        yw_DebriefRunDebrief(yw, inpt, brf);
        break;

    default:
        break;
    }

    //{
    //    yw->_briefScreen.Stage = TBriefengScreen::STAGE_CANCEL;
    //}
}
