#include <inttypes.h>
#include <string.h>
#include "includes.h"
#include "yw_internal.h"
#include "yw.h"
#include "yparobo.h"


int yw_write_callSign(NC_STACK_ypaworld *yw, const char *filename, const char *callsign)
{
    UserData *usr = yw->_GameShell;

    if ( !usr )
        return 0;

    if ( usr->UserName.empty() )
        return 0;

    FSMgr::FileHandle *fil = uaOpenFileAlloc( fmt::sprintf("save:%s/%s", usr->UserName, filename), "w");

    if ( !fil )
        return 0;

    fil->printf("%s", callsign);

    delete fil;

    return 1;
}

int yw_write_user(FSMgr::FileHandle *fil, UserData *usr)
{
    NC_STACK_ypaworld *yw = usr->p_YW;

    if ( usr->netPlayerName[0] >= ' ' )
        yw_write_callSign(yw, "callsign.def", usr->netPlayerName.c_str());

    fil->printf("new_user\n");

    for (int i = 0; i < 8; i++)
    {
        fil->printf("    playerstatus = %d_%d_%d_%d_%d_%d_%d_%d\n",
                i,
                yw->_playersStats[i].DestroyedUnits,
                yw->_playersStats[i].DestroyedByUser,
                yw->_playersStats[i].ElapsedTime,
                yw->_playersStats[i].SectorsTaked,
                yw->_playersStats[i].Score,
                yw->_playersStats[i].Power,
                yw->_playersStats[i].Upgrades);
    }

    fil->printf("    maxroboenergy = %d\n", usr->p_YW->_maxRoboEnergy);
    fil->printf("    maxreloadconst = %d\n", usr->p_YW->_maxReloadConst);
    fil->printf("    numbuddies    = %d\n", 128);
    fil->printf("    beamenergy    = %d\n", usr->p_YW->_beamEnergyCapacity);

    std::string jodie = "    jodiefoster   = ";

    for (int i = 0; i < 8; i++)
    {
        jodie += fmt::sprintf("%d", usr->p_YW->_levelInfo.JodieFoster[i]);

        if ( i < 7 )
            jodie += "_";
    }
    
    fil->puts(jodie);

    fil->printf("      ; the contact flags\n");
    fil->printf("end\n\n");

    return 1;
}

int yw_write_input(FSMgr::FileHandle *fil, UserData *usr)
{
    fil->printf("new_input\n");

    if ( usr->p_YW->_preferences & World::PREF_JOYDISABLE )
        fil->printf("    joystick = no\n");
    else
        fil->printf("    joystick = yes\n");

    if ( usr->altJoystickEnabled )
        fil->printf("    altjoystick = yes\n");
    else
        fil->printf("    altjoystick = no\n");

    if ( usr->p_YW->_preferences & World::PREF_FFDISABLE )
        fil->printf("    forcefeedback = no\n");
    else
        fil->printf("    forcefeedback = yes\n");

    for (size_t i = 1; i < usr->InputConfig.size(); i++)
    {
        if ( usr->InputConfig[i].Type == World::INPUT_BIND_TYPE_BUTTON )
        {
            fil->printf("    input.button[%d] = ", usr->InputConfig[i].KeyID);
            fil->printf("$");

            if ( usr->InputConfig[i].PKeyCode )
                fil->puts(Input::Engine.KeyNamesTable.at(usr->InputConfig[i].PKeyCode).Name);
            else
                fil->printf("nop");
        }
        else if ( usr->InputConfig[i].Type == World::INPUT_BIND_TYPE_SLIDER )
        {
            fil->printf("    input.slider[%d] = ", usr->InputConfig[i].KeyID);

            fil->printf("~#");
            fil->printf("$");

            if ( usr->InputConfig[i].NKeyCode != Input::KC_NONE )
            {
                fil->puts(Input::Engine.KeyNamesTable.at(usr->InputConfig[i].NKeyCode).Name);
            }
            else
            {
                ypa_log_out("Slider(neg) %s is not declared!\n", usr->InputConfigTitle[i].c_str());
                ypa_log_out("Use space-key for it\n");

                fil->printf("space");
            }

            fil->printf("_#");
            fil->printf("$");

            if ( usr->InputConfig[i].PKeyCode != Input::KC_NONE )
            {
                fil->puts(Input::Engine.KeyNamesTable.at(usr->InputConfig[i].PKeyCode).Name);
            }
            else
            {
                ypa_log_out("Slider(pos) %s is not declared!\n", usr->InputConfigTitle[i].c_str());
                ypa_log_out("Use space-key for it\n");

                fil->printf("space");
            }
        }
        else if ( usr->InputConfig[i].Type == World::INPUT_BIND_TYPE_HOTKEY )
        {
            fil->printf("    input.hotkey[%d] = ", usr->InputConfig[i].KeyID);

            if ( usr->InputConfig[i].PKeyCode != Input::KC_NONE )
                fil->puts(Input::Engine.KeyNamesTable.at(usr->InputConfig[i].PKeyCode).Name);
            else
                fil->printf("nop");
        }
        else
            continue;

        fil->printf("         ; ");
        fil->puts(usr->InputConfigTitle[i]);
        fil->printf("\n");
    }

    fil->printf("end\n\n");

    return 1;
}

int yw_write_sound(FSMgr::FileHandle *fil, UserData *usr)
{
    fil->printf("new_sound\n");
    fil->printf("    volume = %d\n", usr->soundVolume);
    fil->printf("    cdvolume = %d\n", usr->musicVolume);

    if ( usr->soundFlags & World::SF_INVERTLR )
        fil->printf("    invertlr = yes\n");
    else
        fil->printf("    invertlr = no\n");

    if ( usr->soundFlags & World::SF_CDSOUND )
        fil->printf("    cdsound = yes\n");
    else
        fil->printf("    cdsound = no\n");

    fil->printf("    channels = %d\n", SFXEngine::SFXe.GetChannels());

    fil->printf("end\n\n");

    return 1;
}

int yw_write_video(FSMgr::FileHandle *fil, UserData *usr)
{
    fil->printf("new_video\n");

    if ( usr->GFXFlags & World::GFX_FLAG_SKYRENDER )
        fil->printf("    heaven = yes\n");
    else
        fil->printf("    heaven = no\n");

    if ( usr->enemyIndicator )
        fil->printf("    enemyindicator = yes\n");
    else
        fil->printf("    enemyindicator = no\n");

    if ( usr->IsVsyncFlag() )
        fil->printf("    vsync = yes\n");
    else
        fil->printf("    vsync = no\n");

    fil->printf("    fxnumber = %d\n", usr->fxnumber);

    if ( GFX::Engine.GetTextureFiltering() )
        fil->printf("    filtering = yes\n");
    else
        fil->printf("    filtering = no\n");

    fil->printf("    maxfps = %d\n", GFX::Engine.GetFpsLimitSetting());

    if ( usr->p_YW->IsMoviesEnabled() )
        fil->printf("    movies = yes\n");
    else
        fil->printf("    movies = no\n");
    
    // The size is informational: the game renders at the window size.
    fil->printf("    ;#!gfxmode = %d_%d_%d\n", usr->p_YW->_screenSize.x, usr->p_YW->_screenSize.y,
                usr->IsWindowedFlag() ? 1 : 0);
    
    fil->printf("end\n\n");

    return 1;
}

int yw_write_game(FSMgr::FileHandle *fil, UserData *usr)
{
    fil->printf("new_game\n");
    fil->printf("    timeline = %d\n", usr->p_YW->GetAITimeline());
    fil->printf("end\n\n");

    return 1;
}

int yw_write_level_status(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw, int lvlid)
{
    //sprintf(buf, "\nbegin_levelstatus %ld\n", lvlid);
    fil->printf("\nbegin_levelstatus %d\n", lvlid);

    //sprintf(buf, "    status = %ld\n", yw->LevelNet->mapInfos[lvlid].field_0);
    fil->printf("    status = %d\n", yw->_globalMapRegions.MapRegions[lvlid].Status);

    fil->printf("end\n\n");

    return 1;
}

int yw_write_levels_statuses(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw)
{
    for (size_t i = 0; i < yw->_globalMapRegions.MapRegions.size(); i++)
    {
        if ( yw->_globalMapRegions.MapRegions[ i ].Status != TMapRegionInfo::STATUS_NONE )
        {
            if ( !yw_write_level_status(fil, yw, i))
                return 0;
        }
    }
    return 1;
}

int yw_write_buddies(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw)
{
    for (const TMapBuddy &buddy : yw->_levelInfo.Buddies )
    {
        fil->printf("\nbegin_buddy\n");

        //sprintf(buf, "    commandid = %ld\n", yw->field_2d90->buddies[buddy_id].commandid);
        fil->printf("    commandid = %d\n", buddy.CommandID);

        fil->printf("    type      = %d\n", buddy.Type);

        //sprintf(buf, "    energy    = %ld\n", yw->field_2d90->buddies[buddy_id].energy);
        fil->printf("    energy    = %d\n", buddy.Energy);

        fil->printf("end\n\n");
    }

    return 1;
}

int yw_write_status(TMFWinStatus *status, const char *field, FSMgr::FileHandle *file)
{
    file->printf("    %s = %d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d\n",
            field,
            (status->Valid ? 1 : 0),
            (status->IsOpen ? 1 : 0),
            status->Rect.x,
            status->Rect.y,
            status->Rect.w,
            status->Rect.h,
            status->Data[0],
            status->Data[1],
            status->Data[2],
            status->Data[3],
            status->Data[4],
            status->Data[5],
            status->Data[6],
            status->Data[7]);

    return 1;
}

int yw_write_shell(FSMgr::FileHandle *fil, UserData *usr)
{
    NC_STACK_ypaworld *yw = usr->p_YW;

    fil->printf("new_shell\n");

    if ( usr->default_lang_dll )
        fil->printf("    language = %s\n", usr->default_lang_dll->c_str());

    if ( yw->_shellConfIsParsed )
    {
        fil->printf("    finder = na_0_0_0_0\n");
        fil->printf("    log    = na_0_0_0_0\n");
        fil->printf("    energy = na_0_0_0_0\n");
        fil->printf("    message = na_0_0_0_0\n");
        fil->printf("    map    = na_0_0_0_0_0_0\n");

        yw_write_status(&yw->_roboMapStatus, "robo_map_status", fil);
        yw_write_status(&yw->_roboFinderStatus, "robo_finder_status", fil);
        yw_write_status(&yw->_vhclMapStatus, "vhcl_map_status", fil);
        yw_write_status(&yw->_vhclFinderStatus, "vhcl_finder_status", fil);
    }

    fil->printf("end\n\n");

    return 1;
}

int yw_write_item_modifers(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil)
{
    int i = 0;
    for (const World::TVhclProto &proto : yw->_vhclProtos)
    {
        if ( proto.model_id != BACT_TYPES_NOPE )
        {
            fil->printf("modify_vehicle %d\n", i);

            for (int j = 1; j < 8; j++)
            {
                if ( proto.disable_enable_bitmask & (1 << j) )
                    fil->printf("    enable         = %d\n", j);
                else
                    fil->printf("    disable        = %d\n", j);
            }

            fil->printf("    shield         = %d\n", proto.shield);
            fil->printf("    energy         = %d\n", proto.energy);
            fil->printf("    num_weapons    = %d\n", proto.num_weapons);
            fil->printf("    weapon         = %d\n", proto.weapon);
            fil->printf("    radar          = %d\n", proto.radar);
            fil->printf("    fire_x         = %4.2f\n", proto.fire_x);
            fil->printf("    fire_y         = %4.2f\n", proto.fire_y);
            fil->printf("    fire_z         = %4.2f\n", proto.fire_z);
            if (proto.hidden)
                fil->printf("    ;#!hidden      = %s\n", (proto.hidden ? "yes" : "no") );
            if (proto.unhideRadar > 0)
                fil->printf("    ;#!unhide_radar      = %d\n", proto.unhideRadar);
            fil->printf("end\n\n");
        }
        i++;
    }

    i = 0;
    for (const World::TWeapProto &proto : yw->_weaponProtos)
    {
        if ( proto.unitID )
        {
            fil->printf("modify_weapon %d\n", i);

            for (int j = 1; j < 8; j++)
            {
                if ( proto.enable_mask & (1 << j) )
                    fil->printf("    enable         = %d\n", j);
                else
                    fil->printf("    disable        = %d\n", j);
            }

            fil->printf("    shot_time      = %d\n", proto.shot_time);
            fil->printf("    shot_time_user = %d\n", proto.shot_time_user);
            fil->printf("    energy         = %d\n", proto.energy);
            fil->printf("end\n\n");
        }
        
        i++;
    }

    i = 0;
    for (const World::TBuildingProto &proto : yw->_buildProtos )
    {
        if ( proto.TypeIcon )
        {
            fil->printf("modify_building %d\n", i);

            for (int j = 1; j < 8; j++)
            {
                if ( proto.EnableMask & (1 << j) )
                    fil->printf("    enable         = %d\n", j);
                else
                    fil->printf("    disable        = %d\n", j);
            }

            fil->printf("end\n\n");
        }
        i++;
    }

    return 1;
}
