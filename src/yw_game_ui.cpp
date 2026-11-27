#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <iterator>
#include "env.h"
#include "includes.h"
#include "yw_internal.h"
#include "yw.h"
#include "yw_mapscene.h"
#include "yw_net.h"

#include "font.h"

#include "yparobo.h"
#include "ypagun.h"
#include "windp.h"
#include "world/analyzer.h"





////////////////////////////////////////
GuiBase *confirmDialogOwner = NULL;
std::string confirmDialogText;

////////////////////////////////////////

int confirmDialogState;




/////////////Asking//////////////////
GuiList lstvw2;

int confirmButtonWidth;
int confirmButtonGap;
int confirmDialogMarginX;



////////// exit menu/////////
GuiList exit_menu;

int exitMenuPendingAction;
int exitMenuEntryHeight;
int exitMenuMarginX;
int exitMenuButtonPadY;
int exitMenuButtonWidth;



////////////////////////////////////////


squadMan squadron_manager;


/////////// INFO LOG //////////////

yw_infolog info_log;





//////////// Map //////////////

int mapSelStartX;
int mapSelStartY;
int mapSelEndX;
int mapSelEndY;

float radarRotM01 = 0.0;
float radarRotM10 = 0.0;
float radarRotM11 = 0.0;

float mapSelWorldMinX;
float mapSelWorldMaxX;
float mapSelWorldMinZ;
float mapSelWorldMaxZ;

tehMap robo_map;

NC_STACK_ypaworld *protoSortWorld; // For sort func


///////// up panel ///////////
energPanel up_panel;




///////// down panel /////////
GuiList gui_lstvw;

TBottomBar bzda;

////////////////////////////////////////
////////////////////////////////////////





void NC_STACK_ypaworld::OpenConfirmDialog(const std::string &text, GuiBase *lstvw)
{
    GuiWinOpen(&lstvw2);
    GuiWinToFront(&lstvw2);

    confirmDialogText = text;
    confirmDialogOwner = lstvw;
    confirmDialogState = 3;
}

void NC_STACK_ypaworld::ConfirmOpenHelp(const std::string &url)
{
    if ( !_isNetGame )
    {
        OpenConfirmDialog( Locale::Text::Advanced(Locale::ADV_RLYHELP) , &exit_menu);

        exitMenuPendingAction = 13;

        _helpURL.clear();
        _helpSavedURL = url;
    }
}


void create_squad_man(NC_STACK_ypaworld *yw)
{
    auto &fnt0 = yw->_guiTiles[0]->map;
    auto &fnt24 = yw->_guiTiles[24]->map;
    auto &fnt28 = yw->_guiTiles[28]->map;

    int f0c32_w = fnt0[32].w;
    int aggrIconsX = f0c32_w + fnt0[123].w;

    int lowerBorder = yw->_fontBorderH + yw->_fontH;
    int upperBorder = yw->_fontBorderH;

    int unitCountX = f0c32_w + aggrIconsX + 5 * fnt24[49].w;
    int entryWidth = 4 * fnt0[65].w + unitCountX + fnt28[97].w + f0c32_w + 8;

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_FINDER);
    args.resizeable = true;
    args.numEntries = 0;
    args.shownEntries = 12;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 24;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = entryWidth;
    args.minEntryWidth = entryWidth;
    args.maxEntryWidth = 32000;
    args.enabled = true;
    args.upperVborder = upperBorder;
    args.lowerVborder = lowerBorder;
    args.closeChar = 73;

    if ( squadron_manager.Init(yw, args) )
    {
        squadron_manager.iconWidth = fnt28[65].w;
        squadron_manager.cmdrIconX = fnt28[64].w + fnt28[97].w + yw->_fontBorderW;
        squadron_manager.unitCountX = unitCountX;
        squadron_manager.aggrIconsX = aggrIconsX;
        squadron_manager.kidIconsX = fnt28[64].w + squadron_manager.iconWidth + squadron_manager.cmdrIconX;
    }
}



// The player's faction has no units left; false when they play none
static bool PlayerDefeated(const MapScene &scene)
{
    uint8_t owner = scene.PlayerOwner();
    return owner > 0 && owner < 8 && scene.Defeated(owner);
}

// The unit the player looks from; Id 0 for none
static MapScene::Unit UserUnitOf(NC_STACK_ypaworld *yw)
{
    MapScene::Unit user;

    if ( !yw->UiScene().UserUnit(user) )
        user = MapScene::Unit();

    return user;
}

void UpdateBottomBarModes(NC_STACK_ypaworld *yw)
{
    const MapScene &scene = yw->UiScene();

    if ( scene.UserUnitId() )
    {
        if ( scene.UserUnitId() == scene.UserRoboId() || yw->_playerInHSGun )
        {
            bzda.enabledModes = 0x29;

            if ( bzda.vhclCount )
            {
                bzda.enabledModes = 0x2B;

                if ( yw->_activeCmdrRemapIndex != -1 )
                    bzda.enabledModes = 0x2F;
            }

            if ( bzda.buildCount )
                bzda.enabledModes |= 0x10;

            bzda.barEnabled = 1;
        }
        else
        {
            bzda.barEnabled = 1;
            bzda.enabledModes = 41;
        }

        // Beaming takes a host station
        if ( !scene.UserRoboId() )
            bzda.enabledModes &= ~0x20;

        if ( PlayerDefeated(scene) )
            bzda.enabledModes = 0;
    }
    else
    {
        bzda.barEnabled = 0;
        bzda.enabledModes = 0;
    }

    bzda.activeMode &= bzda.enabledModes;
    bzda.inputAttached &= bzda.barEnabled;

    if ( !bzda.activeMode )
        bzda.activeMode = bzda.enabledModes & 1;
}

void CreateEnergyPanel(NC_STACK_ypaworld *yw)
{
    up_panel = energPanel();

    TileMap *f30 = yw->_guiTiles[30];

    up_panel.panelHeight = f30->h;
    up_panel.iconWidth = f30->map[65].w;
    up_panel.flags = 0;
    up_panel.gaugeWidth = up_panel.iconWidth + f30->map[84].w;
    up_panel.buttons.resize(4);


    int freeWidth = yw->_screenSize.x - 4 * (up_panel.iconWidth + f30->map[84].w);

    up_panel.gaugeSpacing = freeWidth / 4;
    up_panel.gaugeStartX = freeWidth / 8;
    up_panel.barGap = 2;
    up_panel.barMaxLen = up_panel.gaugeWidth - up_panel.iconWidth - 4;

    up_panel.x = 0;
    up_panel.y = 0;
    up_panel.w = yw->_screenSize.x;
    up_panel.h = up_panel.panelHeight;

    for (int i = 0; i < 4; i++)
        up_panel.buttons[i] = ButtonBox(up_panel.gaugeStartX + (up_panel.gaugeWidth + up_panel.gaugeSpacing) * i,    0,
                                        up_panel.gaugeWidth,   up_panel.panelHeight);

    up_panel.cmdCommands.reserve(512);
    FontUA::set_end(&up_panel.cmdCommands); // Nothing to draw until the first update

    Input::Engine.AddClickBoxFront(&up_panel);
}


Common::Point RoboMapWorldToScreen(vec2d in)
{
    if ( robo_map.stateFlags & 0x100 )
    {
        float dx = in.x - robo_map.viewCenterX;
        float dy = in.y - robo_map.viewCenterZ;

        in.x = robo_map.viewCenterX + robo_map.radarRotM00 * dx + radarRotM01 * dy;
        in.y = robo_map.viewCenterZ + radarRotM10 * dx + radarRotM11 * dy;
    }

    return Common::Point( robo_map.viewScreenX + dround(in.x / robo_map.unitsPerPixelX) - robo_map.scrollPixelX
                        , robo_map.viewScreenY + dround(-in.y / robo_map.unitsPerPixelY) - robo_map.scrollPixelY );
}

void RoboMapDrawWorldLine(float x1, float y1, float x2, float y2, SDL_Color color)
{
    GFX::Engine.SetPen(color);
    GFX::Engine.DrawClippedLineCentered( Common::Line( RoboMapWorldToScreen({x1, y1}), RoboMapWorldToScreen({x2, y2}) )  );
}


void RoboMapDrawIcon(CmdStream *cur, float x, float y, char glyph, int width, int height)
{
    Common::Point tmp = RoboMapWorldToScreen( {x, y} );

    int left = tmp.x - width / 2 - robo_map.viewScreenX;
    int top = tmp.y - height / 2 - robo_map.viewScreenY;
    int right = left + width;
    int bottom = top + height;

    if ( left < robo_map.viewWidth && top < robo_map.viewHeight && right > 0 && bottom > 0 )
    {
        int clipLeft;

        if ( left >= 0 )
        {
            clipLeft = 0;
        }
        else
        {
            clipLeft = -left;
            left = 0;
        }

        int clipTop;

        if ( top >= 0 )
        {
            clipTop = 0;
        }
        else
        {
            clipTop = -top;
            top = 0;
        }

        int visibleWidth;

        if ( right <= robo_map.viewWidth )
            visibleWidth = 0;
        else
            visibleWidth = robo_map.viewWidth - left;

        int visibleHeight;

        if ( bottom <= robo_map.viewHeight )
            visibleHeight = 0;
        else
            visibleHeight = robo_map.viewHeight - top;

        FontUA::set_center_xpos(cur, robo_map.viewScreenX + left);
        FontUA::set_center_ypos(cur, robo_map.viewScreenY + top);

        if ( clipTop )
        {
            FontUA::set_yoff(cur, clipTop);
        }
        else if ( visibleHeight )
        {
            FontUA::set_yheight(cur, visibleHeight);
        }

        if ( clipLeft )
        {
            FontUA::set_xoff(cur, clipLeft);
        }
        else if ( visibleWidth )
        {
            FontUA::set_xwidth(cur, visibleWidth);
        }

        FontUA::store_u8(cur, glyph);
    }
}

void RoboMapDrawOrders(NC_STACK_ypaworld *yw, const MapScene &scene, const MapScene::Unit &bact)
{
    if ( bact.Id != scene.UserRoboId() && bact.HasParent && !bact.Commander )
    {
        if ( bact.SecondaryType == BACT_TGT_TYPE_UNIT )
            RoboMapDrawWorldLine(bact.Position.x, bact.Position.z, bact.SecondaryPos.x, bact.SecondaryPos.z, yw->GetColor(10));
        else
            RoboMapDrawWorldLine(bact.Position.x, bact.Position.z, bact.ParentPos.x, bact.ParentPos.z, yw->GetColor(11));

        return;
    }

    if ( bact.SecondaryType == BACT_TGT_TYPE_UNIT )
    {
        RoboMapDrawWorldLine(bact.Position.x, bact.Position.z, bact.SecondaryPos.x, bact.SecondaryPos.z,  yw->GetColor(10));
        return;
    }

    if ( bact.SecondaryType == BACT_TGT_TYPE_CELL )
    {
        RoboMapDrawWorldLine(bact.Position.x, bact.Position.z, bact.SecondaryPos.x, bact.SecondaryPos.z, yw->GetColor(10));
    }
    else
    {
        if ( bact.PrimaryType == BACT_TGT_TYPE_CELL || bact.PrimaryType == BACT_TGT_TYPE_UNIT )
            RoboMapDrawWorldLine(bact.Position.x, bact.Position.z, bact.PrimaryPos.x, bact.PrimaryPos.z, yw->GetColor(9));

        if ( (bact.StatusFlags & BACT_STFLAG_WAYPOINT) && bact.Waypoints )
        {
            const vec3d *waypoints = bact.Waypoints;

            if ( bact.StatusFlags & BACT_STFLAG_WAYPOINTCCL )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = 1; i < bact.WaypointCount; i++)
                    RoboMapDrawWorldLine(waypoints[i - 1].x, waypoints[i - 1].z, waypoints[i].x, waypoints[i].z, clr);

                RoboMapDrawWorldLine(waypoints[bact.WaypointCount - 1].x, waypoints[bact.WaypointCount - 1].z, waypoints[0].x, waypoints[0].z, clr);
            }
            else if ( bact.WaypointCount > 0 )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = bact.CurrentWaypoint; i < bact.WaypointCount - 1; i++)
                    RoboMapDrawWorldLine(waypoints[i].x, waypoints[i].z, waypoints[i + 1].x, waypoints[i + 1].z, clr);
            }
        }
    }
}

void  RoboMapDrawRoboAIDebug(NC_STACK_ypaworld *yw)
{
    for( NC_STACK_ypabact* &bact : yw->_unitsList )
    {
        if ( bact->_bact_type == BACT_TYPES_ROBO && bact != yw->UserRobo() )
        {
            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bact);

            for (int i = 0; i < 8; i++)
            {
                int cellId = 0;

                switch ( i )
                {
                case 0:
                    if ( robo->_roboRadarValue > 0 )
                        cellId = robo->_roboRadarCellID;
                    break;

                case 1:
                    if ( robo->_roboSafetyValue > 0 )
                        cellId = robo->_roboSafetyCellID;
                    break;

                case 2:
                    if ( robo->_roboPowerValue > 0 )
                        cellId = robo->_roboPowerCellID;
                    break;

                case 3:
                    if ( robo->_roboEnemyValue > 0 )
                        cellId = robo->_roboEnemyCellID;
                    break;

                case 4:
                    if ( robo->_roboConqValue > 0 )
                        cellId = robo->_roboConqCellID;
                    break;

                case 5:
                    if ( robo->_roboPositionValue > 0 )
                        cellId = robo->_roboPositionCellID;
                    break;

                case 6:
                    if ( robo->_roboExploreValue > 0 )
                        cellId = robo->_roboExploreCellID;
                    break;

                case 7:
                    if ( robo->_roboDangerValue > 0 )
                        cellId = robo->_roboDangerCellID;
                    break;

                default:
                    break;
                }

                if ( cellId )
                {
                    vec2d secC = World::SectorIDToCenterPos2( {cellId % yw->_mapSize.x, cellId / yw->_mapSize.x} );

                    RoboMapDrawWorldLine(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(i));
                }
            }


            if ( robo->_roboBuildingDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboBuildingCellID % yw->_mapSize.x, robo->_roboBuildingCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                    RoboMapDrawWorldLine(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(0));
            }

            if ( robo->_roboVehicleDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboVehicleCellID % yw->_mapSize.x, robo->_roboVehicleCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                {
                    RoboMapDrawWorldLine(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(7));
                }
            }
        }
    }
}

void RoboMapDrawLines(NC_STACK_ypaworld *yw)
{
    const MapScene &scene = *robo_map.Scene;
    MapScene::Unit user;
    bool hasUser = scene.UserUnit(user);

    if ( robo_map.stateFlags & 0x100 )
    {
        if ( hasUser && scene.UserRoboId() != user.Id && user.HasParent )
        {
            RoboMapDrawWorldLine(user.Position.x, user.Position.z, user.ParentPos.x, user.ParentPos.z, yw->GetColor(11));

            if ( user.Commander && (user.PrimaryType == BACT_TGT_TYPE_CELL || user.PrimaryType == BACT_TGT_TYPE_UNIT) )
                RoboMapDrawWorldLine(user.Position.x, user.Position.z, user.PrimaryPos.x, user.PrimaryPos.z, yw->GetColor(9));
        }
    }
    else
    {
        if ( robo_map.layerFlags & 4 )
        {
            scene.ForEachCommander([yw, &scene](const MapScene::Unit &comm)
            {
                if ( comm.Status != BACT_STATUS_DEAD )
                    RoboMapDrawOrders(yw, scene, comm);

                scene.ForEachKid(comm, [yw, &scene](const MapScene::Unit &kid)
                {
                    if ( kid.Status != BACT_STATUS_DEAD )
                        RoboMapDrawOrders(yw, scene, kid);
                });
            });
        }
        else
        {
            MapScene::Unit selected;

            if ( scene.SelectedCommander(selected) && selected.Status != BACT_STATUS_DEAD )
                RoboMapDrawOrders(yw, scene, selected);
        }

        if ( hasUser )
        {
            RoboMapDrawWorldLine(user.Position.x, 0.0, user.Position.x, -yw->_mapLength.y, yw->GetColor(13));
            RoboMapDrawWorldLine(0.0, user.Position.z, yw->_mapLength.x, user.Position.z, yw->GetColor(13));
        }

        if ( robo_map.stateFlags & 0x200 )
        {
            int wh = yw->_screenSize.x / 2;
            int hh = yw->_screenSize.y / 2;

            GFX::Engine.SetPen( yw->GetColor(12) );
            
            GFX::Engine.DrawClippedLineCentered( {mapSelStartX - wh,
                                         mapSelStartY - hh,
                                         mapSelEndX - wh,
                                         mapSelStartY - hh} );
            GFX::Engine.DrawClippedLineCentered( {mapSelEndX - wh,
                                         mapSelStartY - hh,
                                         mapSelEndX - wh,
                                         mapSelEndY - hh} );
            GFX::Engine.DrawClippedLineCentered( {mapSelEndX - wh,
                                         mapSelEndY - hh,
                                         mapSelStartX - wh,
                                         mapSelEndY - hh} );
            GFX::Engine.DrawClippedLineCentered( {mapSelStartX - wh,
                                         mapSelEndY - hh,
                                         mapSelStartX - wh,
                                         mapSelStartY - hh} );
        }
    }

    if ( yw->_showDebugMode && scene.Interactive() )
        RoboMapDrawRoboAIDebug(yw);
}


void RoboMapDrawIconStrip(CmdStream *cur, float x, float y, char glyph, int width, int height)
{
    Common::Point tmp = RoboMapWorldToScreen( {x, y} );

    int left = tmp.x - width / 2 - robo_map.viewScreenX;
    // TODO(bug): Centres vertically by the width (a4); RoboMapDrawIcon uses the
    // height (a5). Same result for square icons.
    int top = tmp.y - width / 2 - robo_map.viewScreenY;
    int right = left + width;
    int bottom = top + height;

    if ( left < robo_map.viewWidth && top < robo_map.viewHeight && right > 0 && bottom > 0 )
    {
        int clipLeft;
        if ( left >= 0 )
        {
            clipLeft = 0;
        }
        else
        {
            clipLeft = -left;
            left = 0;
        }

        int clipTop;
        if ( top >= 0 )
        {
            clipTop = 0;
        }
        else
        {
            clipTop = -top;
            top = 0;
        }

        int visibleWidth;
        if ( right <= robo_map.viewWidth )
            visibleWidth = 0;
        else
            visibleWidth = robo_map.viewWidth - left;

        int visibleHeight;
        if ( bottom <= robo_map.viewHeight )
            visibleHeight = height;
        else
            visibleHeight = robo_map.viewHeight - top;

        FontUA::set_center_xpos(cur, robo_map.viewScreenX + left);
        FontUA::set_center_ypos(cur, robo_map.viewScreenY + top);


        if ( clipTop )
            FontUA::set_yoff(cur, clipTop);

        if ( visibleHeight )
            FontUA::set_yheight(cur, visibleHeight);

        if ( clipLeft )
        {
            FontUA::set_xoff(cur, clipLeft);
        }
        else if ( visibleWidth )
        {
            FontUA::set_xwidth(cur, visibleWidth);
        }

        FontUA::store_u8(cur, glyph);
    }
}


void RoboMapDrawSpecialSectors(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int iconSize, tilesetId;

    switch ( robo_map.zoomLevel )
    {
    case 0:
        tilesetId = 16;
        iconSize = 4;
        break;

    case 1:
        tilesetId = 17;
        iconSize = 8;
        break;

    case 2:
        tilesetId = 18;
        iconSize = 16;
        break;

    case 3:
        tilesetId = 19;
        iconSize = 32;
        break;

    case 4:
        tilesetId = 20;
        iconSize = 64;
        break;

    default:
        break;
    }

    const MapScene &scene = *robo_map.Scene;
    uint8_t player = scene.PlayerOwner();

    if ( iconSize >= 8 )
    {
        if ( robo_map.layerFlags & 1 )
        {
            FontUA::select_tileset(cur, tilesetId);

            scene.ForEachPowerStation([&](const Common::Point &cellId, int power)
            {
                if ( scene.CanSee(cellId, player) )
                {
                    if ( power > 0 )
                    {
                        int energyGlyph = 0;

                        vec2d tmp = World::SectorIDToCenterPos2(cellId);

                        if ( power <= 32)
                            energyGlyph = 0x80;
                        else if ( power <= 64)
                            energyGlyph = 0x81;
                        else if ( power <= 96)
                            energyGlyph = 0x82;
                        else if ( power <= 128)
                            energyGlyph = 0x83;
                        else if ( power <= 160)
                            energyGlyph = 0x84;
                        else if ( power <= 192)
                            energyGlyph = 0x85;
                        else if ( power <= 224)
                            energyGlyph = 0x86;
                        else if ( power <= 256)
                            energyGlyph = 0x87;

                        RoboMapDrawIcon(cur, tmp.x, tmp.y, 0x89, iconSize, iconSize);
                        RoboMapDrawIconStrip(cur, tmp.x, tmp.y, energyGlyph, iconSize, iconSize / 8);
                    }
                }
            });

            for ( const TMapGem &gem : yw->_techUpgrades )
            {
                if ( scene.CanSee(gem.CellId, player) )
                {
                    int showGem = 1;

                    if ( scene.Purpose(gem.CellId) == cellArea::PT_TECHUPGRADE )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                            showGem = 0;
                    }

                    if ( showGem )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gem.CellId );
                        RoboMapDrawIcon(cur, tmp.x, tmp.y, 0x88, iconSize, iconSize);
                    }
                }
            }

            for ( const TMapGate &gate : yw->_levelInfo.Gates )
            {
                if ( scene.Purpose(gate.CellId) == cellArea::PT_GATECLOSED )
                {
                    if ( scene.CanSee(gate.CellId, player) )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                        RoboMapDrawIcon(cur, tmp.x, tmp.y, 0x93, iconSize, iconSize);
                    }

                    for ( const TMapKeySector &ks : gate.KeySectors )
                    {
                        if ( ks.PCell )
                        {
                            if ( scene.CanSee(ks.CellId, player) )
                            {
                                if ( scene.Owner(ks.CellId) == player || yw->_timeStamp / 300 & 1 )
                                {
                                    vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                    RoboMapDrawIcon(cur, tmp.x, tmp.y, 0x8A, iconSize, iconSize);
                                }
                            }
                        }
                    }
                }
                else if ( scene.Purpose(gate.CellId) == cellArea::PT_GATEOPENED )
                {
                    if ( scene.CanSee(gate.CellId, player) )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                        {
                            vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                            RoboMapDrawIcon(cur, tmp.x, tmp.y, 0x94, iconSize, iconSize);
                        }
                    }
                }
            }

            for (size_t itemIndex = 0; itemIndex < yw->_levelInfo.SuperItems.size(); itemIndex++)
            {
                const TMapSuperItem &sitem = yw->_levelInfo.SuperItems[itemIndex];
                int itemState = scene.SuperItemState(itemIndex);
                int keyGlyph = 0;
                int itemGlyph = 0;

                if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
                {
                    keyGlyph = 0x8E;

                    if ( itemState == TMapSuperItem::STATE_INACTIVE )
                        itemGlyph = 0x8B;
                    else if ( itemState == TMapSuperItem::STATE_ACTIVE ||
                              itemState == TMapSuperItem::STATE_STOPPED )
                        itemGlyph = 0x8C;
                    else if ( itemState == TMapSuperItem::STATE_TRIGGED )
                        itemGlyph = 0x8D;
                }
                else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
                {
                    keyGlyph = 0x92;

                    if ( itemState == TMapSuperItem::STATE_INACTIVE )
                        itemGlyph = 0x8F;
                    else if ( itemState == TMapSuperItem::STATE_ACTIVE ||
                              itemState == TMapSuperItem::STATE_STOPPED )
                        itemGlyph = 0x90;
                    else if ( itemState == TMapSuperItem::STATE_TRIGGED )
                        itemGlyph = 0x91;
                }

                if ( itemGlyph )
                {
                    int showItem = 0;

                    if ( scene.CanSee(sitem.CellId, player) )
                    {
                        if ( scene.Owner(sitem.CellId) == player || yw->_timeStamp / 300 & 1 )
                            showItem = 1;
                    }

                    if ( showItem )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( sitem.CellId );
                        RoboMapDrawIcon(cur, tmp.x, tmp.y, itemGlyph, iconSize, iconSize);
                    }

                    if ( keyGlyph )
                    {
                        for ( const TMapKeySector &ks : sitem.KeySectors )
                        {
                            if ( ks.PCell )
                            {
                                if ( scene.CanSee(ks.CellId, player) )
                                {
                                    if ( scene.Owner(ks.CellId) == player || yw->_timeStamp / 500 & 1 )
                                    {
                                        vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                        // TODO(bug): Key sectors get itemGlyph; keyGlyph (0x8E/0x92) is only used as a flag.
                                        // Gate key sectors have their own glyph (0x8A). The first decompile has
                                        // the same, so this may be in the original.
                                        RoboMapDrawIcon(cur, tmp.x, tmp.y, itemGlyph, iconSize, iconSize);
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

int MapUnitGlyph(NC_STACK_ypaworld *yw, uint8_t type, uint8_t owner, int vehicle, int zoomLevel, bool typeIcons)
{
    if ( typeIcons )
    {
        if ( type == BACT_TYPES_MISSLE )
            return owner + 1;
        else
            return yw->_vhclProtos[vehicle].type_icon;
    }

    switch ( type )
    {
    case BACT_TYPES_BACT:
        return owner + 9;

    case BACT_TYPES_ROBO:
        switch ( zoomLevel )
        {
        case 0:
        case 1:
            return owner + 0x30;

        case 2:
            return owner + 0x28;

        case 3:
            return owner + 0x20;

        default:
            return owner + 0x18;
        }

    case BACT_TYPES_MISSLE:
        return owner + 1;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        return owner + 0x19;

    case BACT_TYPES_FLYER:
        return owner + 0x11;

    case BACT_TYPES_UFO:
        return owner + 0x21;

    case BACT_TYPES_GUN:
        return owner + 0x29;

    default:
        return 0x41;
    }
}

uint8_t EnergyGlyph(int energy, int energyMax)
{
    float energyRatio = energyMax > 0 ? (float)energy / (float)energyMax : 0.0;

    if ( energyRatio > 0.75 )
        return 0x83;
    else if ( energyRatio > 0.5 )
        return 0x82;
    else if ( energyRatio > 0.25 )
        return 0x81;
    else
        return 0x80;
}

void RoboMapDrawUnit(NC_STACK_ypaworld *yw, CmdStream *cur, int height, int width, const MapScene::Unit &bact, int useTypeIcons)
{
    const MapScene &scene = *robo_map.Scene;

    if ( bact.Status != BACT_STATUS_DEAD && !bact.Hidden )
    {
        int glyph = MapUnitGlyph(yw, bact.Type, bact.Owner, bact.Vehicle, robo_map.zoomLevel, useTypeIcons);

        RoboMapDrawIcon(cur, bact.Position.x, bact.Position.z, glyph, height, width);

        if ( bact.Type != BACT_TYPES_MISSLE && robo_map.zoomLevel > 2 )
        {
            SDL_Color clr = yw->GetColor(bact.Owner);

            int drawBase = 0;
            float dirX, dirZ;

            float leftSide;
            float rightSide;
            float leftDepth;
            float tipDepth;
            float rightDepth;

            if ( bact.Id == scene.UserRoboId() )
            {
                dirX = bact.LookRotation.m02;
                dirZ = bact.LookRotation.m22;

                if ( bact.Id == scene.UserUnitId() )
                {
                    leftSide = -380.0;
                    rightSide = 380.0;

                    leftDepth = -300.0;
                    tipDepth = 600.0;
                    rightDepth = -300.0;

                    drawBase = 1;

                    clr = yw->GetColor(13);
                }
                else
                {
                    leftSide = -85.0;
                    rightSide = 85.0;

                    leftDepth = 0.0;
                    tipDepth = 700.0;
                    rightDepth = 0.0;
                }
            }
            else if ( bact.Id == scene.UserUnitId() )
            {
                dirX = scene.ViewerRotation().m02;
                dirZ = scene.ViewerRotation().m22;

                leftSide = -190.0;
                rightSide = 190.0;

                leftDepth = -200.0;
                tipDepth = 400.0;
                rightDepth = -200.0;

                drawBase = 1;

                clr = yw->GetColor(13);
            }
            else
            {
                dirX = bact.Rotation.m02;
                dirZ = bact.Rotation.m22;

                leftSide = -40.0;
                rightSide = 40.0;

                leftDepth = 0.0;
                tipDepth = 350.0;
                rightDepth = 0.0;
            }

            float dirLen = sqrt(POW2(dirX) + POW2(dirZ));
            float normX, normZ;

            if ( dirLen <= 0.0 )
            {
                normZ = 1.0;
                normX = 0.0;
            }
            else
            {
                normX = dirX / dirLen;
                normZ = dirZ / dirLen;
            }

            float negNormX = -normX;

            float leftX = normZ * leftSide + negNormX * leftDepth + bact.Position.x;
            float leftZ = normX * leftSide + normZ * leftDepth + bact.Position.z;
            float tipX = normZ * 0.0 + negNormX * tipDepth + bact.Position.x;
            float tipZ = normX * 0.0 + normZ * tipDepth + bact.Position.z;

            RoboMapDrawWorldLine(leftX, leftZ, tipX, tipZ, clr);

            float rightX = normZ * rightSide + negNormX * rightDepth + bact.Position.x;
            float rightZ = normX * rightSide + normZ * rightDepth + bact.Position.z;

            RoboMapDrawWorldLine(rightX, rightZ, tipX, tipZ, clr);

            if ( drawBase )
                RoboMapDrawWorldLine(leftX, leftZ, rightX, rightZ, clr); // Also draw base of view triangle
        }

        if ( bact.Type != BACT_TYPES_MISSLE && bact.Type != BACT_TYPES_ROBO && bact.Owner == scene.PlayerOwner() )
        {
            if ( robo_map.layerFlags & 4 && robo_map.zoomLevel > 2 )
                RoboMapDrawIcon(cur, bact.Position.x, bact.Position.z, EnergyGlyph(bact.Energy, bact.EnergyMax), height, width);
        }
    }
}


NC_STACK_ypabact * GetSquadLeader(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return NULL;

    if ( !bact->_parent )
        return NULL;

    if ( bact->_parent != bact->_host_station )
        return bact->_parent;

    return bact;
}

bool MapScene::CanSee(const Common::Point &sector, uint8_t owner) const
{
    return (cellArea::ViewMask(owner) & ViewMask(sector)) != 0;
}

// The map's scene of the game itself: the world as it is now, as the player
// sees it
MapScene::Unit LiveMapUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    MapScene::Unit u;
    u.Id = bact->_gid;
    u.Type = bact->_bact_type;
    u.Owner = bact->_owner;
    u.Status = bact->_status;
    u.StatusFlags = bact->_status_flg;
    u.Vehicle = bact->_vehicleID;
    u.Position = bact->_position;
    u.Sector = bact->_cellId;
    u.Rotation = bact->_rotation;
    u.LookRotation = bact->_viewer_rotation;
    u.Energy = bact->_energy;
    u.EnergyMax = bact->_energy_max;
    u.Radius = bact->_radius;
    u.Height = bact->_height;
    u.Hidden = bact->IsHiddenFor( yw->GetPlayerOwner() );
    u.RoboGun = bact->_bact_type == BACT_TYPES_GUN && static_cast<NC_STACK_ypagun *>(bact)->IsRoboGun();
    u.Commander = bact->IsCommander();
    u.CommandId = bact->_commandID;
    u.Aggression = bact->_aggr;

    if ( bact->_parent )
    {
        u.HasParent = true;
        u.Parent = bact->_parent->_gid;
        u.ParentPos = bact->_parent->_position;
    }

    u.PrimaryType = bact->_primTtype;

    if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
    {
        u.PrimaryPos = bact->_primTpos;

        if ( bact->_primT.pcell )
            u.PrimarySector = bact->_primT.pcell->CellId;
    }
    else if ( bact->_primTtype == BACT_TGT_TYPE_UNIT && bact->_primT.pbact )
    {
        u.PrimaryPos = bact->_primT.pbact->_position;
    }

    u.SecondaryType = bact->_secndTtype;

    if ( bact->_secndTtype == BACT_TGT_TYPE_CELL )
        u.SecondaryPos = bact->_sencdTpos;
    else if ( bact->_secndTtype == BACT_TGT_TYPE_UNIT && bact->_secndT.pbact )
        u.SecondaryPos = bact->_secndT.pbact->_position;

    u.Waypoints = bact->_waypoints;
    u.WaypointCount = bact->_waypoints_count;
    u.CurrentWaypoint = bact->_current_waypoint;
    u.Bact = bact;
    return u;
}

class LiveMapScene : public MapScene
{
public:
    NC_STACK_ypaworld *World = NULL;

    int32_t ViewMask(const Common::Point &sector) const override { return World->_cells(sector).view_mask; }
    uint8_t Owner(const Common::Point &sector) const override { return World->_cells(sector).owner; }
    int32_t Type(const Common::Point &sector) const override { return World->_cells(sector).type_id; }
    uint8_t SimpleType(const Common::Point &sector) const override { return World->_cells(sector).SectorType; }
    const int16_t *Health(const Common::Point &sector) const override { return World->_cells(sector).buildings_health.data(); }
    uint8_t Purpose(const Common::Point &sector) const override { return World->_cells(sector).PurposeType; }

    void ForEachUnitIn(const Common::Point &sector, const UnitFunc &func) const override
    {
        for ( NC_STACK_ypabact *bact : World->_cells(sector).unitsList )
            func(LiveMapUnit(World, bact));
    }

    void ForEachKid(const Unit &unit, const UnitFunc &func) const override
    {
        if ( unit.Bact )
        {
            for ( NC_STACK_ypabact *kid : unit.Bact->_kidList )
                func(LiveMapUnit(World, kid));
        }
    }

    bool SquadLeader(const Unit &unit, Unit &leader) const override
    {
        NC_STACK_ypabact *bact = GetSquadLeader(unit.Bact);

        if ( !bact )
            return false;

        leader = LiveMapUnit(World, bact);
        return true;
    }

    bool Find(uint32_t id, Unit &out) const override
    {
        NC_STACK_ypabact *bact = World->FindUnit(id);

        if ( bact )
            out = LiveMapUnit(World, bact);

        return bact != NULL;
    }

    Unit Fresh(const Unit &unit) const override
    {
        return unit.Bact ? LiveMapUnit(World, unit.Bact) : unit;
    }

    void ForEachSquad(uint8_t owner, const UnitFunc &func) const override
    {
        World->ForEachCommander(owner, [&](NC_STACK_ypabact *commander)
        {
            func(LiveMapUnit(World, commander));
        });
    }

    void ForEachHostStation(uint8_t owner, const UnitFunc &func) const override
    {
        std::vector<NC_STACK_ypabact *> hosts;

        for ( NC_STACK_ypabact *unit : World->_unitsList )
        {
            if ( unit->_bact_type == BACT_TYPES_ROBO && unit->_owner == owner && unit->_status != BACT_STATUS_DEAD &&
                 !(unit->_status_flg & BACT_STFLAG_DEATH1) )
                hosts.push_back(unit);
        }

        std::sort(hosts.begin(), hosts.end(), [](NC_STACK_ypabact *a, NC_STACK_ypabact *b) { return a->_gid < b->_gid; });

        for ( NC_STACK_ypabact *host : hosts )
            func(LiveMapUnit(World, host));
    }

    void ForEachRoboGun(const Unit &host, const UnitFunc &func) const override
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(host.Bact);

        if ( !robo )
            return;

        for ( World::TRoboGun &gun : robo->GetGuns() )
        {
            if ( gun.gun_obj )
                func(LiveMapUnit(World, gun.gun_obj));
        }
    }

    bool Defeated(uint8_t owner) const override
    {
        return !(World->FactionsWithUnits() & (1u << owner));
    }

    bool Constructing(uint8_t owner) const override
    {
        return World->IsAnyBuildingProcess(owner);
    }

    bool HostStationBatteries(uint32_t id, Batteries &out) const override
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(World->FindUnit(id));

        if ( !robo )
            return false;

        out.FillMode = robo->getROBO_fillMode();
        out.Main = robo->_energy;
        out.Max = robo->_energy_max;
        out.Create = robo->getROBO_battVehicle();
        out.Beam = robo->getROBO_battBeam();
        out.LoadFlags = robo->getROBO_loadFlags();
        out.LossFlags = robo->getROBO_lossFlags();
        out.SectorOwner = robo->_pSector->owner;
        out.SectorPower = robo->_pSector->energy_power;
        out.AbsReload = robo->getROBO_absReload();

        TReloadRatios ratios;
        ratios.owner = out.SectorOwner;
        World->GetReloadRatios(&ratios);
        out.ReloadRatio = ratios.ratioClamped;
        return true;
    }

    uint8_t PlayerOwner() const override { return World->LocalOwner(); }
    uint32_t UserRoboId() const override { return World->UserRobo() ? World->UserRobo()->_gid : 0; }
    uint32_t UserUnitId() const override { return World->UserUnit() ? World->UserUnit()->_gid : 0; }

    bool UserRobo(Unit &out) const override
    {
        if ( World->UserRobo() )
            out = LiveMapUnit(World, World->UserRobo());

        return World->UserRobo() != NULL;
    }

    bool UserUnit(Unit &out) const override
    {
        if ( World->UserUnit() )
            out = LiveMapUnit(World, World->UserUnit());

        return World->UserUnit() != NULL;
    }

    bool ViewerPosition(vec3d &out) const override
    {
        if ( World->UserUnit() )
            out = World->UserUnit()->_position;

        return World->UserUnit() != NULL;
    }

    mat3x3 ViewerRotation() const override { return World->_viewerRotation; }

    // The game marks it by what a click would do
    bool Hovered(Unit &out) const override { return false; }

    void ForEachCommander(const UnitFunc &func) const override
    {
        for ( const Unit &comm : World->_cmdrsRemap )
            func(Fresh(comm));
    }

    bool SelectedCommander(Unit &out) const override
    {
        if ( World->_activeCmdrRemapIndex == -1 )
            return false;

        out = Fresh(World->_cmdrsRemap[World->_activeCmdrRemapIndex]);
        return true;
    }

    void ForEachPowerStation(const std::function<void(const Common::Point &, int)> &func) const override
    {
        for ( const auto &ps : World->_powerStations )
        {
            if ( ps.second.pCell )
                func(ps.second.CellId, ps.second.EffectivePower);
        }
    }

    int SuperItemState(size_t index) const override { return World->_levelInfo.SuperItems[index].State; }

    bool Interactive() const override { return true; }
};

static LiveMapScene &LiveScene(NC_STACK_ypaworld *yw)
{
    static LiveMapScene scene;
    scene.World = yw;
    return scene;
}

const MapScene &NC_STACK_ypaworld::UiScene()
{
    return _uiScene ? *_uiScene : LiveScene(this);
}

const MapScene &NC_STACK_ypaworld::WorldScene()
{
    return LiveScene(this);
}

bool NC_STACK_ypaworld::UnitOnRay(const MapScene &scene, const vec3d &pos, const vec3d &vect, uint32_t ignore, uint32_t ignoreToo,
                                  MapScene::Unit &hit, float &distance)
{
    bool found = false;

    int fixX = pos.x / 300.0 * 16384.0;
    int fixZ = pos.z / 300.0 * 16384.0;

    float absDx = fabs(vect.x);
    float absDz = fabs(vect.z);

    distance = vect.length();

    if ( distance <= 0.0 )
        return false;

    int stepX, stepZ, remaining;

    if ( absDx != 0.0 || absDz != 0.0 )
    {
        if ( absDx <= absDz )
        {
            stepX = (absDx * 16384.0 / absDz);

            if ( vect.x < 0.0 )
                stepX = -stepX;

            if ( vect.z >= 0.0 )
                stepZ = 16384;
            else
                stepZ = -16384;

            remaining = absDz;
        }
        else
        {
            if ( vect.x >= 0.0 )
                stepX = 16384;
            else
                stepX = -16384;

            stepZ = (absDz * 16384.0 / absDx);

            if ( vect.z < 0.0 )
                stepZ = -stepZ;

            remaining = absDx;
        }
    }
    else
    {
        stepZ = 0;
        remaining = 0;
        stepX = 0;
    }

    vec3d dir = vect / distance;

    // Sector by sector along it, the nearest unit in the first that has one
    while ( remaining >= 0 )
    {
        int secY = -fixZ >> 16;
        int secX = fixX >> 16;

        if ( IsSector( {secX, secY} ) )
        {
            scene.ForEachUnitIn({secX, secY}, [&](const MapScene::Unit &unit)
            {
                if ( unit.Id == ignore || unit.Status == BACT_STATUS_DEAD || (ignoreToo && unit.Id == ignoreToo) )
                    return;

                vec3d toUnit = unit.Position - pos;
                vec3d cross = dir * toUnit;

                if ( cross.length() < unit.Radius )
                {
                    float dist = toUnit.length();

                    if ( dir.dot(toUnit) / dist > 0.0 )
                    {
                        if ( distance > dist - unit.Radius )
                        {
                            hit = unit;
                            found = true;
                            distance = dist - unit.Radius;
                        }
                    }
                }
            });
        }

        if ( found )
            break;

        do
        {
            fixX += stepX;
            fixZ += stepZ;
            remaining -= 300;
        }
        while ( (fixX >> 16) == secX && (-fixZ >> 16) == secY && remaining >= 0 );
    }

    return found;
}

void NC_STACK_ypaworld::IssueOrder(const World::PlayerOrder &order)
{
    if ( _orderSink )
    {
        // The squad a host station makes comes with a later frame: the
        // interface selects it then
        if ( order.Type == World::PlayerOrder::COMMAND && order.Action == World::DOACTION_ADD_UNIT1 )
        {
            const MapScene &scene = UiScene();

            _playSelectNewSquad = true;
            _playSquadsBefore.clear();

            scene.ForEachSquad(scene.PlayerOwner(), [this](const MapScene::Unit &commander)
            {
                _playSquadsBefore.push_back(commander.Id);
            });
        }

        _orderSink(order);
        return;
    }

    ApplyPlayerOrder(_localPlayer, order);

    // The interface's units as the order left them
    const MapScene &scene = UiScene();

    for ( MapScene::Unit &commander : _cmdrsRemap )
        commander = scene.Fresh(commander);

    if ( _lastMsgSender.Id )
        _lastMsgSender = scene.Fresh(_lastMsgSender);
}

void RoboMapDrawSquadIcons(NC_STACK_ypaworld *yw, CmdStream *cur, const MapScene::Unit &bact, int tilesetId, int glyph, int width, int height)
{
    FontUA::select_tileset(cur, tilesetId);

    RoboMapDrawIcon(cur, bact.Position.x, bact.Position.z, glyph, width, height);

    robo_map.Scene->ForEachKid(bact, [&](const MapScene::Unit &kid)
    {
        if ( kid.Status != BACT_STATUS_DEAD )
            RoboMapDrawIcon(cur, kid.Position.x, kid.Position.z, glyph, width, height);
    });
}

int NC_STACK_ypaworld::SectorBeamEnergy(const Common::Point &sector)
{
    int enrg = 0;

    for( NC_STACK_ypabact* &unit : _cells(sector).unitsList )
    {
        if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM )
        {
            if ( unit->_bact_type != BACT_TYPES_ROBO && unit->_bact_type != BACT_TYPES_MISSLE && unit->_bact_type != BACT_TYPES_GUN )
                enrg += (unit->_energy_max + 99) / 100;
        }
    }

    return enrg;
}

void RoboMapDrawSectorLabel(NC_STACK_ypaworld *yw, const std::string &labl, int tileset_id, CmdStream *cur, int sectorX, int sectorY)
{
    int labelWidth = yw->_guiTiles[tileset_id]->GetWidth(labl);

    Common::Point tmp = RoboMapWorldToScreen( World::SectorIDToPos2( {sectorX, sectorY} ) );

    int labelX = tmp.x - robo_map.viewScreenX;
    int labelY = tmp.y - robo_map.viewScreenY;

    if ( tmp.x - robo_map.viewScreenX > 0 && labelY > 0 && tmp.x - robo_map.viewScreenX + labelWidth < robo_map.viewWidth && tmp.y - robo_map.viewScreenY + yw->_guiTiles[tileset_id]->h < robo_map.viewHeight )
    {
        FontUA::select_tileset(cur, tileset_id);
        FontUA::set_center_xpos(cur, robo_map.viewScreenX + labelX);
        FontUA::set_center_ypos(cur, robo_map.viewScreenY + labelY);

        for ( uint8_t c : labl )
            FontUA::store_s8(cur, c);
    }
}

// The unit a click would select: a host station, or the squad the unit is
// in
static void RoboMapDrawHovered(NC_STACK_ypaworld *yw, CmdStream *cur, const MapScene::Unit &unit, int roboHoverGlyph, int markerTileset,
                               int markerWidth, int markerHeight)
{
    if ( unit.Type == BACT_TYPES_ROBO )
    {
        int roboIconSize = yw->_guiTiles[1]->h;

        FontUA::select_tileset(cur, 1);

        RoboMapDrawIcon(cur, unit.Position.x, unit.Position.z, roboHoverGlyph, roboIconSize, roboIconSize);
    }
    else
    {
        MapScene::Unit leader;

        if ( robo_map.Scene->SquadLeader(unit, leader) )
            RoboMapDrawSquadIcons(yw, cur, leader, markerTileset, 0x85, markerWidth, markerHeight);
    }
}

void RoboMapDrawMarkers(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int sectorIconSize;
    int markerHeight;
    int markerWidth;
    int markerTileset;
    int sectorTileset;
    int roboHoverGlyph;
    int roboSelectGlyph;

    switch ( robo_map.zoomLevel )
    {
    case 0:
        sectorIconSize = 4;
        markerTileset = 59;
        sectorTileset = 16;
        markerHeight = yw->_guiTiles[59]->h;
        markerWidth = yw->_guiTiles[59]->map[1].w;
        roboHoverGlyph = 23;
        roboSelectGlyph = 19;
        break;

    case 1:
        markerTileset = 59;
        sectorIconSize = 8;
        sectorTileset = 17;
        markerHeight = yw->_guiTiles[59]->h;
        markerWidth = yw->_guiTiles[59]->map[1].w;
        roboHoverGlyph = 23;
        roboSelectGlyph = 19;
        break;

    case 2:
        sectorIconSize = 16;
        markerTileset = 60;
        sectorTileset = 18;
        markerHeight = yw->_guiTiles[60]->h;
        markerWidth = yw->_guiTiles[60]->map[1].w;
        roboHoverGlyph = 22;
        roboSelectGlyph = 18;
        break;

    case 3:
        sectorTileset = 19;
        sectorIconSize = 32;
        markerTileset = 61;
        markerHeight = yw->_guiTiles[61]->h;
        markerWidth = yw->_guiTiles[61]->map[1].w;
        roboHoverGlyph = 21;
        roboSelectGlyph = 17;
        break;

    case 4:
        sectorTileset = 20;
        sectorIconSize = 64;
        markerTileset = 28;
        markerHeight = yw->_guiTiles[28]->h;
        markerWidth = yw->_guiTiles[28]->map[1].w;
        roboHoverGlyph = 20;
        roboSelectGlyph = 16;
        break;
    }


    const MapScene &scene = *robo_map.Scene;

    // Watching a replay, the unit under the pointer is marked as the game
    // marks the one a click would select
    MapScene::Unit hovered;

    if ( !scene.Interactive() && scene.Hovered(hovered) )
        RoboMapDrawHovered(yw, cur, hovered, roboHoverGlyph, markerTileset, markerWidth, markerHeight);

    switch ( scene.Interactive() ? yw->_doAction : World::DOACTION_0 )
    {
    case World::DOACTION_2:
        if ( !(robo_map.stateFlags & 0x200) )
        {
            if ( yw->_guiActFlags & 0x20 )
            {
                MapScene::Unit target = scene.Fresh(yw->_bactOnMouse);

                FontUA::select_tileset(cur, markerTileset);

                RoboMapDrawIcon(cur, target.Position.x, target.Position.z, 0x86, markerWidth, markerHeight);
            }
            else if ( yw->_guiActFlags & 0x10 && yw->_mouseGrabbed == false)
            {
                uint8_t player = scene.PlayerOwner();

                if ( scene.Owner(yw->_cellOnMouse) == player && scene.CanSee(yw->_cellOnMouse, player) )
                {
                    FontUA::select_tileset(cur, markerTileset);

                    RoboMapDrawIcon(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, markerWidth, markerHeight);
                }
                else
                {
                    const cellArea &cell = yw->_cells(yw->_cellOnMouse);

                    FontUA::select_tileset(cur, sectorTileset);

                    RoboMapDrawIcon(cur, cell.CenterPos.x, cell.CenterPos.z, 66, sectorIconSize, sectorIconSize);
                }
            }
        }
        break;

    case World::DOACTION_5:
    {
        MapScene::Unit target = scene.Fresh(yw->_bactOnMouse);

        FontUA::select_tileset(cur, markerTileset);

        RoboMapDrawIcon(cur, target.Position.x, target.Position.z, 0x85, markerWidth, markerHeight);
        break;
    }

    case World::DOACTION_6:
    {
        const cellArea &cell = yw->_cells(yw->_cellOnMouse);

        FontUA::select_tileset(cur, sectorTileset);

        RoboMapDrawIcon(cur, cell.CenterPos.x, cell.CenterPos.z, 65, sectorIconSize, sectorIconSize);
        break;
    }

    case World::DOACTION_8:
        if ( !(robo_map.stateFlags & 0x200) )
        {
            if ( yw->_bactOnMouse.Id )
                RoboMapDrawHovered(yw, cur, scene.Fresh(yw->_bactOnMouse), roboHoverGlyph, markerTileset, markerWidth, markerHeight);
        }
        break;

    case World::DOACTION_10:
        FontUA::select_tileset(cur, markerTileset);

        RoboMapDrawIcon(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, markerWidth, markerHeight);
        break;

    default:
        break;
    }

    MapScene::Unit commander;

    MapScene::Unit robo;

    if ( scene.Interactive() && (bzda.activeMode & 0x20) && scene.UserRobo(robo) )
    {
        FontUA::select_tileset(cur, 1);

        int roboIconSize = yw->_guiTiles[1]->h;

        RoboMapDrawIcon(cur, robo.Position.x, robo.Position.z, roboSelectGlyph, roboIconSize, roboIconSize);
    }
    else if ( scene.SelectedCommander(commander) )
    {
        RoboMapDrawSquadIcons(yw, cur, commander, markerTileset, 0x84, markerWidth, markerHeight);

        if ( commander.PrimaryType == BACT_TGT_TYPE_CELL )
        {
            if ( scene.Owner(commander.PrimarySector) != scene.PlayerOwner() )
            {
                vec2d tmp = World::SectorIDToCenterPos2( World::PositionToSectorID(commander.PrimaryPos) );

                FontUA::select_tileset(cur, sectorTileset);

                RoboMapDrawIcon(cur, tmp.x, tmp.y, 66, sectorIconSize, sectorIconSize);

                FontUA::select_tileset(cur, markerTileset);

                RoboMapDrawIcon(cur, commander.PrimaryPos.x, commander.PrimaryPos.z, 0x88, markerWidth, markerHeight);
            }
            else
            {
                FontUA::select_tileset(cur, markerTileset);

                RoboMapDrawIcon(cur, commander.PrimaryPos.x, commander.PrimaryPos.z, 0x88, markerWidth, markerHeight);
            }
        }
        else if ( commander.PrimaryType == BACT_TGT_TYPE_UNIT )
        {
            FontUA::select_tileset(cur, markerTileset);

            RoboMapDrawIcon(cur, commander.PrimaryPos.x, commander.PrimaryPos.z, 0x86, markerWidth, markerHeight);
        }

        if ( (commander.StatusFlags & BACT_STFLAG_WAYPOINT) && commander.Waypoints )
        {
            if ( commander.WaypointCount > 0 )
            {
                int firstWaypoint;

                if ( commander.StatusFlags & BACT_STFLAG_WAYPOINTCCL )
                    firstWaypoint = 0;
                else
                    firstWaypoint = commander.CurrentWaypoint;

                for (int i = firstWaypoint; i < commander.WaypointCount; i++)
                {
                    FontUA::select_tileset(cur, markerTileset);

                    RoboMapDrawIcon(cur, commander.Waypoints[i].x, commander.Waypoints[i].z, 0x88, markerWidth, markerHeight);
                }
            }
        }
    }

    MapScene::Unit user;

    if ( scene.UserUnit(user) )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            FontUA::select_tileset(cur, markerTileset);

            RoboMapDrawIcon(cur, user.Position.x, user.Position.z, 0x89, markerWidth, markerHeight);
        }
    }

    // What only the game has: who reported last, and what the gates can
    // beam
    if ( !scene.Interactive() )
        return;

    if ( yw->_lastMsgSender.Id )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            MapScene::Unit sender = scene.Fresh(yw->_lastMsgSender);

            FontUA::select_tileset(cur, markerTileset);

            RoboMapDrawIcon(cur, sender.Position.x, sender.Position.z, 0x8A, markerWidth, markerHeight);
        }
    }

    for ( const TMapGate &gate : yw->_levelInfo.Gates )
    {
        if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
        {
            int beamEnergy = yw->SectorBeamEnergy(gate.CellId);

            int showLabel = 1;

            if ( 2 * yw->_beamEnergyCapacity < 2 * beamEnergy )
            {
                showLabel = (yw->_timeStamp / 300) & 1; // FLASHING GATES TITLE
            }

            if ( showLabel )
                RoboMapDrawSectorLabel(yw,  fmt::sprintf("%d / %d", 2 * beamEnergy, 2 * yw->_beamEnergyCapacity), 0x1F, cur, gate.CellId.x, gate.CellId.y);
        }
    }
}

void RoboMapDrawToolbar(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    FontUA::select_tileset(cur, 10);
    FontUA::set_center_xpos(cur, robo_map.x + robo_map.toolBarOffsetX + robo_map.borderLeft - (yw->_screenSize.x / 2));
    FontUA::set_center_ypos(cur, robo_map.y + robo_map.h - (robo_map.toolBtnHeight + robo_map.borderBottom) - (yw->_screenSize.y / 2));

    if ( robo_map.layerFlags & 1 )
        FontUA::store_u8(cur, 97);
    else
        FontUA::store_u8(cur, 65);

    if ( robo_map.layerFlags & 2 )
        FontUA::store_u8(cur, 98);
    else
        FontUA::store_u8(cur, 66);

    if ( robo_map.layerFlags & 4 )
        FontUA::store_u8(cur, 100);
    else
        FontUA::store_u8(cur, 68);

    if ( robo_map.lockViewer == 1 )
        FontUA::store_u8(cur, 102);
    else
        FontUA::store_u8(cur, 70);

    if ( robo_map.stateFlags & 0x40 )
        FontUA::store_u8(cur, 106);
    else
        FontUA::store_u8(cur, 74);

    if ( robo_map.stateFlags & 0x80 )
        FontUA::store_u8(cur, 107);
    else
        FontUA::store_u8(cur, 75);
}

void RoboMapDrawOverlay(NC_STACK_ypaworld *yw)
{
    int drawnCount = 0;

    int sectorShift = robo_map.zoomLevel + 2;

    int useTypeIcons = 0;
    int setid = 59;
    switch ( robo_map.zoomLevel )
    {
    case 0:
    case 1:
        setid = 59;
        break;

    case 2:
        setid = 60;
        break;

    case 3:
        setid = 61;
        break;

    case 4:
        useTypeIcons = 1;
        setid = 28;
        break;
    }

    int height = yw->_guiTiles[setid]->map[128].w;
    int width = yw->_guiTiles[setid]->h;

    robo_map.viewWidth = robo_map.w - robo_map.borderTotalW;
    robo_map.viewHeight = robo_map.h - robo_map.borderTotalH;

    robo_map.viewScreenX = robo_map.x + robo_map.borderLeft - (yw->_screenSize.x / 2);
    robo_map.viewScreenY = robo_map.y + robo_map.borderTop - (yw->_screenSize.y / 2);

    Common::Rect rect;
    rect.left = robo_map.viewScreenX;
    rect.right = robo_map.w - robo_map.borderTotalW + robo_map.viewScreenX - 1;
    rect.top = robo_map.y + robo_map.borderTop - (yw->_screenSize.y / 2);
    rect.bottom = robo_map.h - robo_map.borderTotalH + rect.top - 1;

    robo_map.scrollPixelX = robo_map.viewCenterX / robo_map.unitsPerPixelX - (robo_map.w - robo_map.borderTotalW) / 2;
    robo_map.scrollPixelY = -(robo_map.viewCenterZ / robo_map.unitsPerPixelY + (robo_map.h - robo_map.borderTotalH) / 2);

    GFX::Engine.SetClipCentered(rect);

    int firstCol = robo_map.scrollPixelX >> sectorShift;
    int firstRow = robo_map.scrollPixelY >> sectorShift;
    int lastCol = (robo_map.viewWidth + robo_map.scrollPixelX) >> sectorShift;
    int vii = (robo_map.viewHeight + robo_map.scrollPixelY) >> sectorShift;

    if ( firstCol < 1 )
        firstCol = 1;

    if ( firstRow < 1 )
        firstRow = 1;

    if ( lastCol >= yw->_mapSize.x )
        lastCol = yw->_mapSize.x - 1;

    if ( vii >= yw->_mapSize.y )
        vii = yw->_mapSize.y - 1;

    // A buffer of its own: t1_cmdbuf_3 holds the sectors, which only the
    // simulation step rebuilds. Reusing it here made frames without a step
    // draw this overlay in place of the sectors, so the grid flickered.
    robo_map.overlayCmds.clear();
    
    RoboMapDrawSpecialSectors(yw, &robo_map.overlayCmds);

    FontUA::select_tileset(&robo_map.overlayCmds, setid);

    RoboMapDrawLines(yw);

    const MapScene &scene = *robo_map.Scene;

    for (int row = firstRow; row <= vii; row++)
    {
        for (int j = firstCol; j <= lastCol; j++ )
        {
            if ( drawnCount < 2048 )
            {
                if ( robo_map.MapViewMask & scene.ViewMask({j, row}) )
                {
                    scene.ForEachUnitIn({j, row}, [&](const MapScene::Unit &bct)
                    {
                        if ( bct.Type == BACT_TYPES_ROBO )
                        {
                            FontUA::select_tileset(&robo_map.overlayCmds, 1);

                            RoboMapDrawUnit(yw, &robo_map.overlayCmds, yw->_guiTiles[1]->h, yw->_guiTiles[1]->map[24].w, bct, 0);

                            FontUA::select_tileset(&robo_map.overlayCmds, setid);

                            drawnCount++;
                        }
                        else if ( bct.Type == BACT_TYPES_GUN )
                        {
                            if ( !bct.RoboGun || bct.Id == scene.UserUnitId() )
                            {
                                RoboMapDrawUnit(yw, &robo_map.overlayCmds, height, width, bct, useTypeIcons);
                                drawnCount++;
                            }
                        }
                        else
                        {
                            RoboMapDrawUnit(yw, &robo_map.overlayCmds, height, width, bct, useTypeIcons);
                            drawnCount++;
                        }
                    });

                }
            }
        }
    }

    if ( robo_map.stateFlags & 0x200 )
    {
        for (int l = firstRow; l <= vii; l++ )
        {
            for (int m = firstCol; m <= lastCol; m++ )
            {
                if ( drawnCount < 2048 )
                {
                    if ( robo_map.MapViewMask & scene.ViewMask({m, l}) )
                    {
                        scene.ForEachUnitIn({m, l}, [&](const MapScene::Unit &bct)
                        {
                            if ( bct.Owner == scene.PlayerOwner()
                                    && bct.Position.x > mapSelWorldMinX
                                    && bct.Position.z > mapSelWorldMinZ
                                    && bct.Position.x < mapSelWorldMaxX
                                    && bct.Position.z < mapSelWorldMaxZ )
                            {
                                if ( bct.Type != BACT_TYPES_MISSLE && bct.Type != BACT_TYPES_ROBO && bct.Type != BACT_TYPES_GUN )
                                {
                                    if ( bct.Status != BACT_STATUS_CREATE && bct.Status != BACT_STATUS_BEAM && bct.Status != BACT_STATUS_DEAD )
                                    {
                                        RoboMapDrawIcon(&robo_map.overlayCmds, bct.Position.x, bct.Position.z, 0x85, height, width);
                                        drawnCount++;
                                    }
                                }
                            }
                        });
                    }
                }
            }
        }
    }
    RoboMapDrawMarkers(yw, &robo_map.overlayCmds);
    RoboMapDrawToolbar(yw, &robo_map.overlayCmds);
    FontUA::set_end(&robo_map.overlayCmds);

    GFX::Engine.ProcessDrawSeq(robo_map.overlayCmds);
}

void RoboMapCalcScrollThumbs(NC_STACK_ypaworld *yw)
{
    float viewSpanZ = (robo_map.h - robo_map.borderTotalH) * robo_map.unitsPerPixelY;
    float viewSpanX = (robo_map.w - robo_map.borderTotalW) * robo_map.unitsPerPixelX;

    float mapSpanX = (yw->_mapSize.x * World::CVSectorLength);
    int trackLen = robo_map.w - robo_map.borderRight;

    int hThumbPos = ((robo_map.viewCenterX - viewSpanX * 0.5) * trackLen) / mapSpanX;
    int thumbSize = trackLen * viewSpanX / mapSpanX;



    if ( thumbSize >= robo_map.minThumbSize )
    {
        if ( thumbSize > trackLen )
            thumbSize = trackLen;
    }
    else
    {
        hThumbPos -= (robo_map.minThumbSize - thumbSize) >> 1;
        thumbSize = robo_map.minThumbSize;
    }

    if ( hThumbPos < 0 )
        hThumbPos = 0;

    if ( hThumbPos + thumbSize > trackLen )
        hThumbPos = trackLen - thumbSize;

    robo_map.hScrollLength = trackLen;
    robo_map.hThumbPos = hThumbPos;
    robo_map.hThumbSize = thumbSize;




    float viewTop = -(robo_map.viewCenterZ + viewSpanZ * 0.5);
    float mapSpanZ = (yw->_mapSize.y * World::CVSectorLength);

    trackLen = robo_map.h - robo_map.borderTotalH;
    thumbSize = (trackLen * viewSpanZ / mapSpanZ);

    int vThumbPos = ((trackLen * viewTop) / mapSpanZ);


    if ( thumbSize >= robo_map.minThumbSize )
    {
        if ( thumbSize > trackLen )
            thumbSize = trackLen;
    }
    else
    {
        vThumbPos -= (robo_map.minThumbSize - thumbSize) >> 1;
        thumbSize = robo_map.minThumbSize;
    }

    if ( vThumbPos < 0 )
        vThumbPos = 0;

    if ( thumbSize + vThumbPos > trackLen )
        vThumbPos = trackLen - thumbSize;

    robo_map.vScrollLength = trackLen;
    robo_map.vThumbPos = vThumbPos;
    robo_map.vThumbSize = thumbSize;
}

void RoboMapLayoutFrame(NC_STACK_ypaworld *yw)
{
    int winX = robo_map.x - (yw->_screenSize.x / 2);
    int winY = robo_map.y - (yw->_screenSize.y / 2);
    int winW = robo_map.w;
    int winH = robo_map.h;

    robo_map.buttons[0] = ButtonBox(robo_map.w - yw->_fontDefCloseW,     0,
                                    yw->_fontDefCloseW,    yw->_fontH);

    robo_map.buttons[2] = ButtonBox(robo_map.w - 2 * yw->_fontDefCloseW, 0,
                                    yw->_fontDefCloseW,    yw->_fontH);

    robo_map.buttons[1] = ButtonBox(0, 0,
                                    robo_map.buttons[2].x - robo_map.sizeBtnWidth, yw->_fontH);

    robo_map.buttons[9] = ButtonBox(robo_map.buttons[2].x - robo_map.sizeBtnWidth, 0,
                                    robo_map.sizeBtnWidth,       yw->_fontH);

    int btnX = robo_map.toolBarOffsetX + robo_map.borderLeft;

    for (int i = 3; i < 9; i++)
    {
        robo_map.buttons[i] = ButtonBox(btnX,  winH - robo_map.borderBottom - robo_map.toolBtnHeight,
                                        robo_map.toolBtnWidth,  robo_map.toolBtnHeight);

        btnX += robo_map.toolBtnWidth;
    }

    int postfixChar;

    if ( robo_map.stateFlags & 0x20 )
        postfixChar = 's'; //default fon
    else
        postfixChar = 'q'; //default fon (
    
    robo_map.cmdCommands.clear();

    GuiBase::FormateTitle(yw, winX, winY, winW, Locale::Text::WinName(Locale::WINNAME_MAP), &robo_map.cmdCommands, postfixChar, robo_map.flags);

    FontUA::next_line(&robo_map.cmdCommands);
    FontUA::reset_tileset(&robo_map.cmdCommands, 13);

    FontUA::store_u8(&robo_map.cmdCommands, 65);

    FontUA::next_line(&robo_map.cmdCommands);
    FontUA::reset_tileset(&robo_map.cmdCommands, 12);

    int sideHeight = winH - robo_map.borderTotalH - 1;
    while (sideHeight > yw->_fontVScrollH)
    {
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);

        sideHeight -= yw->_fontVScrollH;
    }

    if ( sideHeight > 1 )
    {
        FontUA::set_yheight(&robo_map.cmdCommands, sideHeight - 1);
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);
    }

    FontUA::reset_tileset(&robo_map.cmdCommands, 13);
    FontUA::store_u8(&robo_map.cmdCommands, 66);
    FontUA::next_line(&robo_map.cmdCommands);

    FontUA::include(&robo_map.cmdCommands, 0);
    FontUA::include(&robo_map.cmdCommands, 1);
    FontUA::include(&robo_map.cmdCommands, 2);
    FontUA::set_end(&robo_map.cmdCommands);
}

void RoboMapLayoutHScroll(NC_STACK_ypaworld *yw)
{
    int barX = robo_map.x - (yw->_screenSize.x / 2);
    int barY = robo_map.y + robo_map.h - robo_map.borderBottom - (yw->_screenSize.y / 2);

    robo_map.buttons[13] = ButtonBox(0, robo_map.h - robo_map.borderBottom,
                                     robo_map.hThumbPos, robo_map.borderBottom);

    robo_map.buttons[14] = ButtonBox(robo_map.hThumbPos, robo_map.h - robo_map.borderBottom,
                                     robo_map.hThumbSize, robo_map.borderBottom);

    robo_map.buttons[16] = ButtonBox(robo_map.w - robo_map.borderRight,  robo_map.h - robo_map.borderBottom,
                                     robo_map.borderRight,  robo_map.borderBottom);

    robo_map.buttons[15] = ButtonBox(robo_map.hThumbSize + robo_map.hThumbPos,   robo_map.h - robo_map.borderBottom,
                                     robo_map.hScrollLength - robo_map.hThumbPos - robo_map.hThumbSize,   robo_map.borderBottom);

    robo_map.t1_cmdbuf_2.clear();

    FontUA::select_tileset(&robo_map.t1_cmdbuf_2, 11);
    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_2, barX);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_2, barY);

    int thumbPos = robo_map.hThumbPos;

    if ( thumbPos > 0 )
    {
        FontUA::store_u8(&robo_map.t1_cmdbuf_2, 65);
        if ( thumbPos > 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, thumbPos);
            FontUA::store_u8(&robo_map.t1_cmdbuf_2, 66);
        }
    }

    FontUA::store_u8(&robo_map.t1_cmdbuf_2, 68);

    FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.hThumbSize + thumbPos - 1);

    FontUA::store_u8(&robo_map.t1_cmdbuf_2, 69);
    FontUA::store_u8(&robo_map.t1_cmdbuf_2, 70);


    int thumbEnd = thumbPos + robo_map.hThumbSize;
    if ( thumbEnd < robo_map.hScrollLength )
    {
        if ( thumbEnd < robo_map.hScrollLength - 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.hScrollLength - 1);

            FontUA::store_u8(&robo_map.t1_cmdbuf_2, 66);
        }
        FontUA::store_u8(&robo_map.t1_cmdbuf_2, 67);
    }

    FontUA::store_u8(&robo_map.t1_cmdbuf_2, 71);

    FontUA::set_end(&robo_map.t1_cmdbuf_2);
}

void RoboMapLayoutVScroll(NC_STACK_ypaworld *yw)
{
    int barX = robo_map.x + robo_map.w - robo_map.borderRight - (yw->_screenSize.x / 2);
    int barY = robo_map.y + robo_map.borderTop - (yw->_screenSize.y / 2);

    robo_map.buttons[10] = ButtonBox(robo_map.w - robo_map.borderRight,    robo_map.borderTop,
                                     robo_map.borderRight,   robo_map.vThumbPos);

    robo_map.buttons[11] = ButtonBox(robo_map.w - robo_map.borderRight,    robo_map.vThumbPos + robo_map.borderTop,
                                     robo_map.borderRight,   robo_map.vThumbSize);

    robo_map.buttons[12] = ButtonBox(robo_map.w - robo_map.borderRight,    robo_map.vThumbSize + robo_map.vThumbPos + robo_map.borderTop,
                                     robo_map.borderRight,   robo_map.vScrollLength - robo_map.vThumbPos - robo_map.vThumbSize);

    robo_map.t1_cmdbuf_1.clear();

    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_1, barX);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_1, barY);


    if ( robo_map.vThumbPos > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        FontUA::store_u8(&robo_map.t1_cmdbuf_1, 67);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        int aboveHeight = robo_map.vThumbPos - 1;

        while (aboveHeight >= yw->_fontVScrollH )
        {
            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 66);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            aboveHeight -= yw->_fontVScrollH;
        }

        if ( aboveHeight > 0 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, aboveHeight);
            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 66);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
    }

    if ( robo_map.vThumbSize > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);
        FontUA::store_u8(&robo_map.t1_cmdbuf_1, 69);

        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);


        int thumbHeight = robo_map.vThumbSize - 1;

        while ( thumbHeight > yw->_fontVScrollH )
        {
            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 67);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            thumbHeight -= yw->_fontVScrollH;
        }

        if ( thumbHeight > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, thumbHeight - 1);
            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 67);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        FontUA::store_u8(&robo_map.t1_cmdbuf_1, 70);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }

    int belowHeight = robo_map.vScrollLength - robo_map.vThumbPos - robo_map.vThumbSize;

    if ( belowHeight > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        while ( belowHeight > yw->_fontVScrollH )
        {
            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 66);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            belowHeight -= yw->_fontVScrollH;
        }

        if ( belowHeight > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, belowHeight - 1);

            FontUA::store_u8(&robo_map.t1_cmdbuf_1, 66);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        FontUA::store_u8(&robo_map.t1_cmdbuf_1, 68);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }
    FontUA::set_end(&robo_map.t1_cmdbuf_1);
}



void RoboMapLayoutTileLayer(NC_STACK_ypaworld *yw, int tileSize, CmdStream *cmdbuf, int left, int top, int right, int bottom, int tilesetId, mapFunc ffunc, int xpos, int ypos)
{
    int tileShift;

    if ( tileSize == 4)
        tileShift = 2;
    else if ( tileSize == 8)
        tileShift = 3;
    else if ( tileSize == 16 )
        tileShift = 4;
    else if ( tileSize == 32 )
        tileShift = 5;
    else if ( tileSize == 64 )
        tileShift = 6;
    else
    {
        ypa_log_out("yw_maprnd.c/yw_RMapLego(): wrong size\n");

        FontUA::set_end(cmdbuf);
        return ;
    }

    FontUA::set_center_xpos(cmdbuf, xpos);
    FontUA::set_center_ypos(cmdbuf, ypos);

    int msk = tileSize - 1;

    FontUA::reset_tileset(cmdbuf, tilesetId);

    int row = top;

    while (row < bottom)
    {
        int gapWidth = 0;
        int col = left;
        int tileY = row >> tileShift;

        if ( msk & row )
        {
            FontUA::set_yoff(cmdbuf, row & msk);

            row += tileSize - (msk & row);
        }
        else
        {
            if ( bottom - row >= tileSize )
            {
                row += tileSize;
            }
            else
            {
                FontUA::set_yheight(cmdbuf, bottom - row);

                row = bottom;
            }
        }

        if ( col & msk )
        {
            int tileOffset = msk & col;

            int firstTile = ffunc(yw, col >> tileShift, tileY) & 0xFF;

            if ( firstTile )
            {
                FontUA::set_xoff(cmdbuf, tileOffset);
                FontUA::store_u8(cmdbuf, firstTile);

                gapWidth = 0;
            }
            else
            {
                gapWidth = tileSize - tileOffset;

                FontUA::add_xpos(cmdbuf, gapWidth);
            }

            col += tileSize - tileOffset;
        }

        while ( col < right - tileSize )
        {
            int tile = ffunc(yw, col >> tileShift, tileY) & 0xFF;

            if ( tile )
            {
                FontUA::store_u8(cmdbuf, tile);

                gapWidth = 0;
            }
            else
            {
                if ( gapWidth <= 0 )
                {
                    gapWidth = tileSize;

                    FontUA::add_xpos(cmdbuf, gapWidth);
                }
                else
                {
                    gapWidth += tileSize;

                    //pcur -= 2; //HACKY rewrite
                    cmdbuf->pop_back();
                    cmdbuf->pop_back();
                    FontUA::store_s16(cmdbuf, gapWidth);
                }
            }

            col += tileSize;
        }

        if ( right - col > 0 )
        {
            int lastTile = ffunc(yw, col >> tileShift, tileY) & 0xFF;
            if ( lastTile )
            {
                FontUA::set_xwidth(cmdbuf, right - col);
                FontUA::store_u8(cmdbuf, lastTile);
            }
        }
        FontUA::next_line(cmdbuf);

    }
}



int RoboMapFogTile(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        if ( (robo_map.layerFlags & 3) && (robo_map.MapViewMask & robo_map.Scene->ViewMask({x, y})) )
            return 0;
        else
            return 9;
    }
    else
        return 9;
}

int RoboMapOwnerTile(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        const MapScene &scene = *robo_map.Scene;

        if ( robo_map.MapViewMask & scene.ViewMask({x, y}) )
        {
            uint8_t owner = scene.Owner({x, y});

            if ( !owner )
                return 8;
            else
                return owner;
        }
    }

    return 0;
}

int RoboMapSectorTypeTile(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        const MapScene &scene = *robo_map.Scene;

        if ( robo_map.MapViewMask & scene.ViewMask({x, y}) )
            return yw->_secTypeArray[scene.Type({x, y})].GUIElementID;
    }

    return 0;
}

int RoboMapLegoTile(NC_STACK_ypaworld *yw, int x, int y)
{
    Common::Point sector(x / 4, y / 4);

    if ( !yw->IsSector( sector ) )
        return 0;

    const MapScene &scene = *robo_map.Scene;

    if ( !(robo_map.MapViewMask & scene.ViewMask(sector)) )
        return 0;

    return MapLegoTile(yw, x, y, scene.Type(sector), scene.SimpleType(sector), scene.Health(sector));
}

int MapLegoTile(NC_STACK_ypaworld *yw, int x, int y, int32_t typeId, int sectorType, const int16_t *health)
{
    int secX = x / 4;
    int subX = x & 3;
    int secY = y / 4;
    int subY = y & 3;

    // The lego of a building on it, as GetLegoBld has it
    auto legoAt = [yw, typeId, health](int bldX, int bldY) -> TLego &
    {
        TSubSectorDesc *ssec = yw->_secTypeArray[ typeId ].SubSectors.At(bldX, bldY);
        return yw->_legoArray[ ssec->HPModels[ yw->_buildHealthModelId[ health[bldX + 3 * bldY] ] ] ];
    };

    if ( subX && subY )
    {
        if ( sectorType == 1 )
        {
            TLego *lego = &legoAt(0, 0);
            int glyph = (16 * (subY - 1) + subX - 1 + lego->GUIElementID) & 0xFF;
            return glyph;
        }
        else
        {
            TLego *lego = &legoAt(subX - 1, 2 - (subY - 1));
            return lego->GUIElementID;
        }
    }
    else
    {
        cellArea &cell = yw->_cells(secX, secY);

        if ( secX < 1 || secY < 1 )
            return 1;

        int edgeFlags = 0;

        cellArea *leftCell;
        cellArea *topCell;
        cellArea *topLeftCell;

        if ( subX )
        {
            leftCell = &cell;
            topCell = &yw->_cells(secX, secY - 1);
            topLeftCell = &yw->_cells(secX, secY - 1);
        }
        else if ( subY )
        {
            topLeftCell = &yw->_cells(secX - 1, secY);
            leftCell = &yw->_cells(secX - 1, secY);
            topCell = &cell;
        }
        else
        {
            leftCell = &yw->_cells(secX - 1, secY);
            topCell = &yw->_cells(secX, secY - 1);
            topLeftCell = &yw->_cells(secX - 1, secY - 1);
        }


        if ( fabs(topLeftCell->height - topCell->height) >= 500.0 )
            edgeFlags = 1;

        if ( fabs(leftCell->height - cell.height) >= 500.0 )
            edgeFlags |= 2;

        if ( fabs(topLeftCell->height - leftCell->height) >= 500.0 )
            edgeFlags |= 4;

        if ( fabs(topCell->height - cell.height) >= 500.0 )
            edgeFlags |= 8;

        if ( edgeFlags == 12 )
        {
            if ( topLeftCell->height <= leftCell->height )
                edgeFlags = 0xFD;
            else
                edgeFlags = 0xFC;
        }
        else if ( edgeFlags == 3 )
        {
            if ( topLeftCell->height <= topCell->height )
                edgeFlags = 0xFF;
            else
                edgeFlags = 0xFE;
        }

        int edgeGlyph = (edgeFlags - 16) & 0xFF;
        return (edgeGlyph << 8) | edgeGlyph;
    }
}

void RoboMapLayoutTerrainLayers(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int xpos, int ypos, int w, int h)
{
    float centerX = robo_map.viewCenterX / robo_map.unitsPerPixelX;
    float centerY = -robo_map.viewCenterZ / robo_map.unitsPerPixelY;
    int left = centerX - (w / 2);
    int right = w + left;
    int top = centerY - (h / 2);
    int bottom = h + top;

    RoboMapLayoutTileLayer(yw, 4 << robo_map.zoomLevel, cmdbuf, left, top, right, bottom, robo_map.zoomLevel + 50, RoboMapFogTile, xpos, ypos);

    if ( robo_map.layerFlags & 2 )
        RoboMapLayoutTileLayer(yw, 4 << robo_map.zoomLevel, cmdbuf, left, top, right, bottom, robo_map.zoomLevel + 50, RoboMapOwnerTile, xpos, ypos);

    if ( robo_map.layerFlags & 1 )
    {
        switch ( robo_map.zoomLevel )
        {
        case 0:
            RoboMapLayoutTileLayer(yw, 4, cmdbuf, left, top, right, bottom, 43, RoboMapSectorTypeTile, xpos, ypos);
            break;

        case 1:
            RoboMapLayoutTileLayer(yw, 8, cmdbuf, left, top, right, bottom, 44, RoboMapSectorTypeTile, xpos, ypos);
            break;

        case 2:
            RoboMapLayoutTileLayer(yw, 4, cmdbuf, left + 2, top + 2, right + 2, bottom + 2, 42, RoboMapLegoTile, xpos, ypos);
            break;

        case 3:
            RoboMapLayoutTileLayer(yw, 8, cmdbuf, left + 4, top + 4, right + 4, bottom + 4, 41, RoboMapLegoTile, xpos, ypos);
            break;

        case 4:
            RoboMapLayoutTileLayer(yw, 16, cmdbuf, left + 8, top + 8, right + 8, bottom + 8, 40, RoboMapLegoTile, xpos, ypos);
            break;
        }
    }
}

void RoboMapLayoutTerrain(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int x, int y)
{
    RoboMapLayoutTerrainLayers(yw, cmdbuf, x, y, robo_map.w - robo_map.borderTotalW, robo_map.h - robo_map.borderTotalH);
    FontUA::set_end(cmdbuf);
}

void RoboMapClampView(NC_STACK_ypaworld *yw, int pixelW, int pixelH)
{
    float mapSpanX = yw->_mapSize.x * World::CVSectorLength;
    float mapSpanZ = yw->_mapSize.y * World::CVSectorLength;

    float viewSpanX = pixelW * robo_map.unitsPerPixelX;
    float halfSpanX = viewSpanX * 0.5;

    if ( viewSpanX <= mapSpanX )
    {
        if ( robo_map.viewCenterX - halfSpanX >= 0.0 )
        {
            if ( robo_map.viewCenterX + halfSpanX > mapSpanX )
                robo_map.viewCenterX = mapSpanX - halfSpanX;
        }
        else
        {
            robo_map.viewCenterX = halfSpanX;
        }
    }
    else
    {
        robo_map.viewCenterX = mapSpanX * 0.5;
    }

    float viewSpanZ = pixelH * robo_map.unitsPerPixelY;
    float halfSpanZ = viewSpanZ * 0.5;

    if ( viewSpanZ > mapSpanZ )
    {
        robo_map.viewCenterZ = -mapSpanZ * 0.5;
    }
    else
    {
        if ( robo_map.viewCenterZ + halfSpanZ > 0.0 )
        {
            robo_map.viewCenterZ = -viewSpanZ * 0.5;
        }
        else
        {
            if ( robo_map.viewCenterZ - halfSpanZ < -mapSpanZ )
                robo_map.viewCenterZ = -mapSpanZ + halfSpanZ;
        }
    }
}

void MapRoboUpdateMapInt(NC_STACK_ypaworld *yw)
{
    robo_map.buttons[17] = ButtonBox(robo_map.borderLeft,
                                     robo_map.borderTop,
                                     robo_map.w - robo_map.borderTotalW,
                                     robo_map.h - robo_map.borderTotalH);

    int viewX = robo_map.x + robo_map.borderLeft - (yw->_screenSize.x / 2);
    int viewY = robo_map.y + robo_map.borderTop - (yw->_screenSize.y / 2);

    robo_map.t1_cmdbuf_3.clear();
    RoboMapLayoutTerrain(yw, &robo_map.t1_cmdbuf_3, viewX, viewY);
}

void  CreateRoboMap(NC_STACK_ypaworld *yw, const MapScene *scene)
{
    robo_map.Scene = scene ? scene : &LiveScene(yw);

    robo_map.t1_cmdbuf_1.reserve(256);
    robo_map.t1_cmdbuf_2.reserve(256);
    robo_map.t1_cmdbuf_3.reserve(32768);
    robo_map.overlayCmds.reserve(32768);

    robo_map.flags = (GuiBase::FLAG_WITH_HELP | GuiBase::FLAG_CLOSED | GuiBase::FLAG_WITH_CLOSE | GuiBase::FLAG_WITH_DRAGBAR);
    robo_map.minThumbSize = 8;
    robo_map.toolBtnWidth = yw->_guiTiles[10]->map[65].w;
    robo_map.toolBtnHeight = yw->_guiTiles[10]->h;
    robo_map.toolBarOffsetX = 4;
    robo_map.sizeBtnWidth = yw->_guiTiles[0]->map[113].w;
    robo_map.borderTop = yw->_fontH;
    robo_map.borderBottom = yw->_fontHScrollH;
    robo_map.borderLeft = yw->_guiTiles[12]->map[65].w;
    robo_map.borderRight = yw->_fontVBScrollW;
    robo_map.borderTotalW = robo_map.borderRight + robo_map.borderLeft;
    robo_map.borderTotalH = robo_map.borderBottom + robo_map.borderTop;
    robo_map.minWidth = robo_map.borderRight + 7 * robo_map.toolBtnWidth + robo_map.borderLeft + 8;
    robo_map.minHeight = robo_map.borderBottom + robo_map.borderTop + 96;
    robo_map.maxWidth = 32000;
    robo_map.maxHeight = 32000;
    robo_map.x = 0;
    robo_map.y = yw->_upScreenBorder;
    robo_map.w = 2 * yw->_screenSize.x / 3;
    robo_map.h = 2 * yw->_screenSize.y / 3;
    robo_map.zoomLevel = 4;
    robo_map.layerFlags = 7;
    robo_map.buttons.resize(18);

    robo_map.cmdCommands.reserve(512);
    robo_map.cmdInclude = {&robo_map.t1_cmdbuf_1, &robo_map.t1_cmdbuf_2, &robo_map.t1_cmdbuf_3};

    robo_map.postDraw = RoboMapDrawOverlay;

    MapScene::Unit userRobo;

    if ( robo_map.Scene->UserRobo(userRobo) )
    {
        robo_map.viewCenterX = userRobo.Position.x;
        robo_map.viewCenterZ = userRobo.Position.z;
    }
    else
    {
        robo_map.viewCenterZ = -39000.0;
        robo_map.viewCenterX = 39000.0;
    }

    switch ( robo_map.zoomLevel )
    {
    case 0:
        robo_map.unitsPerPixelY = 300.0;
        robo_map.unitsPerPixelX = 300.0;
        break;

    case 1:
        robo_map.unitsPerPixelY = 150.0;
        robo_map.unitsPerPixelX = 150.0;
        break;

    case 2:
        robo_map.unitsPerPixelY = 75.0;
        robo_map.unitsPerPixelX = 75.0;
        break;

    case 3:
        robo_map.unitsPerPixelY = 37.5;
        robo_map.unitsPerPixelX = 37.5;
        break;

    case 4:
        robo_map.unitsPerPixelY = 18.75;
        robo_map.unitsPerPixelX = 18.75;
        break;
    }

    robo_map.lockViewer = 0;
    robo_map.stateFlags = 0;
    robo_map.MapViewMask = cellArea::ViewMask(1);

    robo_map.radarLeft = HudRadarLeft;
    robo_map.radarTop = HudRadarTop;
    robo_map.radarRight = HudRadarRight;
    robo_map.radarBottom = HudRadarBottom;

    robo_map.altX = yw->_screenSize.x - robo_map.minWidth;
    robo_map.altY = yw->_screenSize.y - yw->_downScreenBorder - robo_map.minHeight;
    robo_map.altW = robo_map.minWidth;
    robo_map.altH = robo_map.minHeight;

    RoboMapCalcScrollThumbs(yw);
    RoboMapLayoutFrame(yw);
    RoboMapLayoutHScroll(yw);
    RoboMapLayoutVScroll(yw);
    MapRoboUpdateMapInt(yw);
    RoboMapClampView(yw, robo_map.hScrollLength - robo_map.borderLeft, robo_map.vScrollLength);
}


void ValidateWinStatus(NC_STACK_ypaworld *yw, TMFWinStatus *status)
{
    if ( status->Valid )
    {
        if ( status->Rect.y < yw->_upScreenBorder
                || status->Rect.x + status->Rect.w > yw->_screenSize.x
                || status->Rect.y + status->Rect.h > yw->_screenSize.y - yw->_downScreenBorder )
        {
            status->Valid = false;
        }
    }

    if ( status->Valid )
    {
        // Data[3..6] hold the window's other (size button) rect.
        if ( status->Data[4] < yw->_upScreenBorder
                || status->Data[3] + status->Data[5] > yw->_screenSize.x
                || status->Data[6] + status->Data[4] > yw->_screenSize.y - yw->_downScreenBorder )
            status->Valid = false;
    }
}

void RoboMapZoom(NC_STACK_ypaworld *yw, int zoomDir)
{
    if ( zoomDir == 1 )
        robo_map.zoomLevel++;
    else if ( zoomDir == 2 )
        robo_map.zoomLevel--;


    // TODO(cleanup): Zooming is clamped to 1..4, so the level 0 cases here and in
    // the map drawing only run if a saved status holds 0.
    if ( robo_map.zoomLevel > 4 )
        robo_map.zoomLevel = 4;
    else if ( robo_map.zoomLevel < 1 )
        robo_map.zoomLevel = 1;

    switch ( robo_map.zoomLevel )
    {
    case 0:
        robo_map.unitsPerPixelY = 300.0;
        robo_map.unitsPerPixelX = 300.0;
        break;

    case 1:
        robo_map.unitsPerPixelY = 150.0;
        robo_map.unitsPerPixelX = 150.0;
        break;

    case 2:
        robo_map.unitsPerPixelY = 75.0;
        robo_map.unitsPerPixelX = 75.0;
        break;

    case 3:
        robo_map.unitsPerPixelY = 37.5;
        robo_map.unitsPerPixelX = 37.5;
        break;

    case 4:
        robo_map.unitsPerPixelY = 18.75;
        robo_map.unitsPerPixelX = 18.75;
        break;
    }
}

// Positions the bottom bar and its button groups along the screen's bottom edge.
void SetBottomBarLayout(NC_STACK_ypaworld *yw)
{
    int spareWidth = yw->_screenSize.x - 11 * yw->_iconOrderW;
    bzda.groupGap = spareWidth / 4;

    int halfListWidth;
    if ( yw->_screenSize.x >= 512 )
        halfListWidth = 7 * yw->_guiTiles[0]->map[87].w;
    else
        halfListWidth = 9 * yw->_guiTiles[0]->map[87].w;

    bzda.buyListWidth = 2 * halfListWidth;
    bzda.createGroupX = bzda.groupGap / 2;
    bzda.windowGroupX = yw->_iconOrderW + bzda.groupGap + bzda.groupGap / 2;
    bzda.unitGroupX = 2 * yw->_iconOrderW + bzda.windowGroupX + bzda.groupGap;
    bzda.toolGroupX = 5 * yw->_iconOrderW + bzda.unitGroupX + bzda.groupGap;

    bzda.x = 0;
    bzda.y = yw->_screenSize.y - yw->_iconOrderH;
    bzda.w = yw->_screenSize.x;
    bzda.h = yw->_iconOrderH;
}

int CreateBottomBar(NC_STACK_ypaworld *yw)
{
    bzda = TBottomBar();

    SetBottomBarLayout(yw);

    bzda.buttonsY = 0;
    bzda.flags = 0;
    bzda.buttons.resize(11);

    for (int i = 0; i < 11; i++)
        bzda.buttons[i] = ButtonBox();

    bzda.cmdCommands.reserve(1088); 
    bzda.inputAttached = 0;

    UpdateBottomBarModes(yw);

    bzda.selVhclIdx = 0;
    bzda.selGunIdx = 0;
    bzda.selBuildIdx = 0;
    bzda.selBuyListIdx = 0;

    if ( bzda.barEnabled & 1 )
    {
        if ( !(bzda.inputAttached & 1) )
        {
            bzda.inputAttached |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }
    }

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = 8;
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 16;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = bzda.buyListWidth;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    if ( !gui_lstvw.Init(yw, args) )
        return 0;


    ValidateWinStatus(yw, &yw->_roboMapStatus);
    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_roboFinderStatus.Rect.x + yw->_roboFinderStatus.Rect.w > yw->_screenSize.x
                || yw->_roboFinderStatus.Rect.h + yw->_roboFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_roboFinderStatus.Valid = false;
        }
    }

    ValidateWinStatus(yw, &yw->_vhclMapStatus);
    if ( yw->_vhclFinderStatus.Valid )
    {
        if ( yw->_vhclFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_vhclFinderStatus.Rect.w + yw->_vhclFinderStatus.Rect.x > yw->_screenSize.x
                || yw->_vhclFinderStatus.Rect.h + yw->_vhclFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_vhclFinderStatus.Valid = false;
        }
    }

    if ( !yw->_roboMapStatus.Data[5] || !yw->_roboMapStatus.Data[6] )
        yw->_roboMapStatus.Valid = false;

    if ( !yw->_vhclMapStatus.Data[5] || !yw->_vhclMapStatus.Data[6] )
        yw->_vhclMapStatus.Valid = false;

    if ( yw->_roboMapStatus.Valid )
    {
        if ( yw->_roboMapStatus.IsOpen )
        {
            yw->GuiWinOpen(&robo_map);
            yw->GuiWinToFront(&robo_map);
        }
        else
            yw->GuiWinClose(&robo_map);

        robo_map.x = yw->_roboMapStatus.Rect.x;
        robo_map.y = yw->_roboMapStatus.Rect.y;
        robo_map.w = yw->_roboMapStatus.Rect.w;
        robo_map.h = yw->_roboMapStatus.Rect.h;
        robo_map.layerFlags = yw->_roboMapStatus.Data[0];
        robo_map.lockViewer = yw->_roboMapStatus.Data[1];
        robo_map.zoomLevel = yw->_roboMapStatus.Data[2];
        robo_map.altX = yw->_roboMapStatus.Data[3];
        robo_map.altY = yw->_roboMapStatus.Data[4];
        robo_map.altW = yw->_roboMapStatus.Data[5];
        robo_map.altH = yw->_roboMapStatus.Data[6];

        RoboMapZoom(yw, 0);
    }


    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.IsOpen )
        {
            yw->GuiWinOpen(&squadron_manager);
            yw->GuiWinToFront(&squadron_manager);
        }
        else
            yw->GuiWinClose(&squadron_manager);

        squadron_manager.x = yw->_roboFinderStatus.Rect.x;
        squadron_manager.y = yw->_roboFinderStatus.Rect.y;
        squadron_manager.w = yw->_roboFinderStatus.Rect.w;
        squadron_manager.h = yw->_roboFinderStatus.Rect.h;

        squadron_manager.SetRect(yw, -2, -2);
    }

    return 1;
}

void NC_STACK_ypaworld::FFeedback_Init()
{
    _ffTimeStamp = 0;
    _ffEffectType = -1;
    _ffPeriod = 0;
    _ffMagnitude = 0;
}



void create_info_log(NC_STACK_ypaworld *yw)
{
    info_log = yw_infolog();

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_LOG);
    args.resizeable = true;
    args.numEntries = 1;
    args.shownEntries = 6;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 24;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = 200;
    args.maxEntryWidth = 32000;
    args.minEntryWidth = 2 * (5 * yw->_guiTiles[0]->map[48].w);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.closeChar = 74;

    if ( info_log.Init(yw, args) )
    {
        if ( yw->_timeStamp )
        {
            std::string tmp = fmt::sprintf("<%02d:%02d:%02d>", (yw->_timeStamp >> 10) / 60 / 60 % 24, (yw->_timeStamp >> 10) / 60 % 60, (yw->_timeStamp >> 10) % 60);
            std::string tmp2 = Locale::Text::Common(Locale::CMN_MISNCONT);
            snprintf(info_log.msgs[0].txt, sizeof(info_log.msgs[0].txt), tmp2.c_str(), tmp.c_str());
        }
        else
        {
            std::string tmp = Locale::Text::Common(Locale::CMN_WELCOME);
            strcpy(info_log.msgs[0].txt, tmp.c_str());
        }

        info_log.msgs[0].timestamp = yw->_timeStamp + 1;
        info_log.last_sender_gid = 0;
        info_log.last_msg_time = yw->_timeStamp;
        info_log.last_msg_id = 0;
        info_log.last_msg_prio = 127;
        info_log.time_col_width = 6 * yw->_guiTiles[0]->map[48].w + 2 * yw->_guiTiles[0]->map[58].w + 12;
    }
}




void create_exit_menu(NC_STACK_ypaworld *yw)
{
    exitMenuEntryHeight = (yw->_fontH / 2) + yw->_fontH;
    exitMenuButtonPadY = yw->_fontH / 4;

    int tmp = yw->_guiTiles[0]->GetWidth("WWWWWWW");

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_EXIT);
    args.resizeable = false;
    args.numEntries = 5;
    args.shownEntries = 5;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 5;
    args.minShownEntries = 5;
    args.withIcon = false;
    args.entryHeight = exitMenuEntryHeight;
    args.entryWidth = (2 * tmp + 16);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;

    if ( exit_menu.Init(yw, args) )
    {
        int firstBtnY = yw->_fontH + exit_menu.upperVborder + exitMenuButtonPadY;

        exitMenuMarginX = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        exitMenuButtonWidth = exit_menu.entryWidth - 2 * exitMenuMarginX;

        for (int i = 8; i < 13; i++)
        {
            exit_menu.buttons[i] = ButtonBox(exitMenuMarginX, (i - 8) * exitMenuEntryHeight + firstBtnY,
                                             exitMenuButtonWidth, yw->_fontH);
        }
    }
}


void CreateConfirmWindow(NC_STACK_ypaworld *yw)
{
    int entryWidth = yw->_guiTiles[0]->GetWidth("WWWWWWW") * 3.5 + 16;

    GuiList::tInit args;
    args.title = " ";
    args.resizeable = false;
    args.numEntries = 3;
    args.shownEntries = 3;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 3;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = entryWidth;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.withHelp = false;

    if ( lstvw2.Init(yw, args) )
    {
        int buttonY = yw->_fontH + lstvw2.upperVborder + 2 * lstvw2.entryHeight;

        confirmDialogMarginX = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        confirmButtonWidth = (lstvw2.entryWidth - 2 * confirmDialogMarginX) / 3;

        lstvw2.buttons.resize(10);

        lstvw2.buttons[8] = ButtonBox(confirmDialogMarginX, buttonY, confirmButtonWidth, lstvw2.entryHeight);

        lstvw2.buttons[9] = ButtonBox(  lstvw2.entryWidth - confirmDialogMarginX - confirmButtonWidth,
                                        buttonY,
                                        confirmButtonWidth,
                                        lstvw2.entryHeight);

        confirmButtonGap = lstvw2.buttons[9].x - (confirmButtonWidth + lstvw2.buttons[8].x);

        confirmDialogState = 0;
    }
}


// Sizes the HUD's vector graphics relative to the screen.
void SetHudLayout(NC_STACK_ypaworld *yw)
{
    THudState *wis = &yw->_hud;

    float CW, CH;
    GFX::Engine.getAspectCorrection(CW, CH, false);

    if ( yw->_screenSize.x >= 512 )
    {
        wis->bar_segments = 8;
        wis->label_width = 28;
        wis->line_height = (float)yw->_guiTiles[0]->h / ((float)yw->_screenSize.y * CH);
        wis->info_height = wis->line_height * 14.0;
        wis->value_width = 28;
    }
    else
    {
        wis->bar_segments = 6;
        wis->label_width = 18;
        wis->line_height = (float)yw->_guiTiles[0]->h * 1.5 / ((float)yw->_screenSize.y * CH);
        wis->info_height = wis->line_height * 18.0;
        wis->value_width = 18;
    }

    wis->color_fx = 0;
    wis->bar_width = (float)(yw->_guiTiles[51]->map[1].w * wis->bar_segments + wis->value_width + wis->label_width) / ((float)yw->_screenSize.x * CW);

    wis->color_fx = 1;
}

void LoadHud(NC_STACK_ypaworld *yw)
{
    const std::array<std::string, 14> hudSkeletonFiles
    {
        "wireless/bar.sklt",
        "wireless/compass.sklt",
        "wireless/frame.sklt",
        "wireless/arrow.sklt",
        "wireless/mgvis.sklt",
        "wireless/grenvis1.sklt",
        "wireless/rockvis1.sklt",
        "wireless/missvis1.sklt",
        "wireless/bombvis1.sklt",
        "wireless/grenvis2.sklt",
        "wireless/rockvis2.sklt",
        "wireless/missvis2.sklt",
        "wireless/bombvis2.sklt",
        "wireless/triangle.sklt"
    };

    THudState *wis = &yw->_hud;

    *wis = THudState();

    wis->visible = 1;

    Common::Env.SetPrefix("rsrc", "data:");

    for (int i = 0; i < 14; i++)
    {
        wis->sklts[i] = Nucleus::CInit<NC_STACK_sklt>({{NC_STACK_rsrc::RSRC_ATT_NAME, hudSkeletonFiles.at(i)}});

        if ( wis->sklts[i] )
            wis->sklts_intern[i] = wis->sklts[i]->GetSkelet();
    }

    SetHudLayout(yw);
}

void FreeExitMenu(NC_STACK_ypaworld *yw)
{
    exit_menu.Free();
}

void FreeInfoLog(NC_STACK_ypaworld *yw)
{
    info_log.Free();
}

void FreeSquadManager(NC_STACK_ypaworld *yw)
{
    squadron_manager.Free();
}

void FreeBottomBar(NC_STACK_ypaworld *yw)
{
    yw->GuiWinClose( &gui_lstvw );

    if ( bzda.inputAttached & 1 )
    {
        bzda.inputAttached &= 0xFFFFFFFE;
        Input::Engine.RemClickBox(&bzda);
    }

    gui_lstvw.Free();
}

void FreeEnergyPanel()
{
    Input::Engine.RemClickBox(&up_panel);
}

void FreeHud(NC_STACK_ypaworld *yw)
{
    for (int i = 0; i < 14; i++)
    {
        if (yw->_hud.sklts[i])
        {
            yw->_hud.sklts[i]->Delete();
            yw->_hud.sklts[i] = NULL;
            yw->_hud.sklts_intern[i] = NULL;
        }
    }
}

void FreeConfirmWindow(NC_STACK_ypaworld *yw)
{
    lstvw2.Free();
}

void NC_STACK_ypaworld::FFeedback_StopAll()
{
    Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_ALL);
}

int GUI_Open(NC_STACK_ypaworld *yw, const MapScene *scene)
{
    if ( yw->_guiLoaded )
        return yw->_guiLoaded;

    yw->_guiDragging = false;
    yw->_guiDragElement = NULL;
    yw->_guiDragDefaultMouse = false;
    yw->_mouseCursorHidden = false;
    yw->_prevMousePos = Common::Point();
    yw->_bactPrevClicked = MapScene::Unit();

    LoadHud(yw);
    CreateEnergyPanel(yw);
    CreateRoboMap(yw, scene);
    create_squad_man(yw);
    create_info_log(yw);
    create_exit_menu(yw);
    CreateConfirmWindow(yw);
    CreateNetGameWindow(yw);
    yw->_viewDistanceWindow.Create(yw);

    yw->GuiWinAttach(&robo_map);
    yw->GuiWinAttach(&squadron_manager);
    yw->GuiWinAttach(&info_log);
    yw->GuiWinAttach(&exit_menu);
    yw->GuiWinAttach(&lstvw2);
    yw->GuiWinAttach(&netgame_wnd);
    yw->GuiWinAttach(&yw->_viewDistanceWindow);

    if ( !CreateBottomBar(yw) )
    {
        yw->GuiWinDetach(&lstvw2);
        yw->GuiWinDetach(&exit_menu);
        yw->GuiWinDetach(&info_log);
        yw->GuiWinDetach(&squadron_manager);
        yw->GuiWinDetach(&robo_map);
        yw->GuiWinDetach(&netgame_wnd);
        yw->GuiWinDetach(&yw->_viewDistanceWindow);

        yw->_viewDistanceWindow.Free();
        FreeExitMenu(yw);
        FreeInfoLog(yw);
        FreeSquadManager(yw);
        return 0;
    }

    yw->_guiLoaded = true;

    if ( yw->_preferences & World::PREF_CDMUSICDISABLE )
    {
        SFXEngine::SFXe.SetMusicTrack(yw->_levelInfo.MusicTrack, yw->_levelInfo.MusicTrackMinDelay, yw->_levelInfo.MusicTrackMaxDelay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    return 1;
}


void DrawBottomBar()
{
    if ( bzda.IsOpen() )
    {
        GFX::Engine.ProcessDrawSeq(bzda.cmdCommands, &bzda.cmdInclude);

        if ( gui_lstvw.IsOpen() )
            GFX::Engine.ProcessDrawSeq(gui_lstvw.cmdCommands, &gui_lstvw.cmdInclude);
    }
}


// The open windows, the front one last
void NC_STACK_ypaworld::DrawGuiWindows()
{
    for(GuiBaseList::iterator it = _guiActive.begin(); it != _guiActive.end(); it++)
    {
        GuiBase *lstnode = *it;
        if ( !(lstnode->flags & GuiBase::FLAG_CLOSED) )
        {
            if ( lstnode->flags & GuiBase::FLAG_ICONIFED )
            {
                GFX::Engine.ProcessDrawSeq(lstnode->iconString);
            }
            else
            {
                GFX::Engine.ProcessDrawSeq(lstnode->cmdCommands, &lstnode->cmdInclude);

                if ( lstnode->postDraw )
                    lstnode->postDraw(this);
            }
        }
    }
}

void DrawGameInterface(NC_STACK_ypaworld *yw)
{
    if ( UserUnitOf(yw).Type != BACT_TYPES_MISSLE )
        DrawHud(yw);

    DrawGameWindows(yw);
}

// The interface but the HUD: the windows, the messages and the bars
void DrawGameWindows(NC_STACK_ypaworld *yw)
{
    MapScene::Unit user = UserUnitOf(yw);

    if ( user.Type != BACT_TYPES_MISSLE )
    {
        yw->DrawGuiWindows();

        if ( user.Status != BACT_STATUS_DEAD )
        {
            DrawGameMessages(yw);
            DrawGameStatus(yw);
        }
        draw_tooltip(yw);
        GFX::Engine.ProcessDrawSeq(up_panel.cmdCommands, &up_panel.cmdInclude);
        DrawBottomBar();
    }
}


void buy_list_update_sub(NC_STACK_ypaworld *yw, int selected, GuiList *lstvw, CmdStream *cur, char icon, const std::string &name, int cost)
{
    int innerWidth = lstvw->entryWidth - 2 * yw->_fontBorderW;

    FontUA::ColumnItem columns[3];

    int fontId;
    int prefixChar;
    int spaceChar;
    int postfixChar;

    if ( selected )
    {
        FontUA::set_txtColor(cur, yw->_iniColors[62].r, yw->_iniColors[62].g, yw->_iniColors[62].b);

        fontId = 9;
        prefixChar = 98;
        spaceChar = 99;
        postfixChar = 100;
    }
    else
    {
        FontUA::set_txtColor(cur, yw->_iniColors[61].r, yw->_iniColors[61].g, yw->_iniColors[61].b);

        fontId = 0;
        spaceChar = 102;
        prefixChar = 102;
        postfixChar = 102;
    }

    int digitWidth = yw->_guiTiles[fontId]->map[48].w;

    columns[0].txt = icon;
    columns[0].fontID = 28;
    columns[0].width = squadron_manager.iconWidth;
    columns[0].prefixChar = 0;
    columns[0].postfixChar = 0;
    columns[0].spaceChar = 64;
    columns[0].flags = 4;

    columns[1].fontID = fontId;
    columns[1].spaceChar = spaceChar;
    columns[1].prefixChar = prefixChar;
    columns[1].txt = name;
    columns[1].flags = 37;
    columns[1].width = innerWidth - squadron_manager.iconWidth - 5 * digitWidth;
    columns[1].postfixChar = 0;

    columns[2].txt = fmt::sprintf("%d", cost);
    columns[2].fontID = fontId;
    columns[2].spaceChar = spaceChar;
    columns[2].postfixChar = postfixChar;
    columns[2].width = 5 * digitWidth;
    columns[2].flags = 42;
    columns[2].prefixChar = 0;

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, fontId);

    FontUA::op10(cur, squadron_manager.iconWidth);

    FontUA::store_u8(cur, spaceChar);

    FontUA::add_xpos(cur, -squadron_manager.iconWidth);

    FormateColumnItem(yw, cur, 3, columns);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

float NC_STACK_ypaworld::GetUnitLimitCostFactor()
{
    return UnitLimitCostFactor(_playerOwner);
}

float NC_STACK_ypaworld::UnitLimitCostFactor(uint8_t owner)
{
    float factor = 1.0;

    if ( _isNetGame )
    {
        if ( _levelUnitLimitType == 2 )
        {
            int excess = _countUnitsPerOwner[owner] - _levelUnitLimit;
            if ( excess > 0 )
                factor = (_levelUnitLimitArg * excess) * 0.01 + 1.0;
        }
    }

    return factor;
}

int NC_STACK_ypaworld::OrderCost(uint8_t owner, int action, int proto)
{
    switch ( action )
    {
    case World::DOACTION_ADD_UNIT1:
    case World::DOACTION_ADD_UNIT2:
        return dround(UnitLimitCostFactor(owner) * (2 * _vhclProtos.at(proto).energy));

    case World::DOACTION_6:
        return dround(UnitLimitCostFactor(owner) * _buildProtos.at(proto).Energy);

    default:
        return 0;
    }
}

void BottomBarLayoutBuyList(NC_STACK_ypaworld *yw)
{
    gui_lstvw.itemBlock.clear();
    Input::Engine.RemClickBox(&gui_lstvw);
    Input::Engine.AddClickBoxFront(&gui_lstvw);

    gui_lstvw.numEntries = bzda.buyListCount;
    gui_lstvw.selectedEntry = bzda.selBuyListIdx;

    if ( gui_lstvw.maxShownEntries >= bzda.buyListCount )
        gui_lstvw.shownEntries = bzda.buyListCount;
    else
        gui_lstvw.shownEntries = gui_lstvw.maxShownEntries;

    if ( gui_lstvw.firstShownEntries + gui_lstvw.shownEntries > gui_lstvw.numEntries )
        gui_lstvw.firstShownEntries = gui_lstvw.numEntries - gui_lstvw.shownEntries;

    gui_lstvw.SetRect(yw, -2, -2);

    gui_lstvw.x = bzda.createGroupX + bzda.x;
    gui_lstvw.y = bzda.buttonsY + bzda.y - gui_lstvw.h;

    gui_lstvw.ItemsPreLayout(yw, &gui_lstvw.itemBlock, 0, "uvw");

    for (int i = 0; i < gui_lstvw.shownEntries; i++ )
    {
        int selected = 0;
        int entryIdx = i + gui_lstvw.firstShownEntries;

        if ( bzda.buyList[ entryIdx ].kind == 1 )
        {
            int vhclId = bzda.buyList[ entryIdx ].protoId;

            const World::TVhclProto &proto = yw->_vhclProtos[vhclId];

            int cost = dround(yw->GetUnitLimitCostFactor() * 2 * proto.energy / 100.0);

            const std::string name = yw->GetVehicleName(proto);

            if ( entryIdx == gui_lstvw.selectedEntry )
                selected = 1;

            buy_list_update_sub(yw, selected, &gui_lstvw, &gui_lstvw.itemBlock, proto.type_icon, name, cost);
        }
        else if ( bzda.buyList[ entryIdx ].kind == 2 )
        {
            int buildId = bzda.buyList[ entryIdx ].protoId;
            const World::TBuildingProto &buildProto = yw->_buildProtos[buildId];

            int cost = dround(yw->GetUnitLimitCostFactor() * buildProto.Energy / 100.0);
            
            std::string name;
            if ( yw->_isNetGame )
                name = yw->GetBuildingName(buildProto, true);
            else
                name = yw->GetBuildingName(buildProto, false);
                
            if ( entryIdx == gui_lstvw.selectedEntry )
                selected = 1;

            buy_list_update_sub(yw, selected, &gui_lstvw, &gui_lstvw.itemBlock, buildProto.TypeIcon, name, cost);
        }
    }

    gui_lstvw.ItemsPostLayout(yw, &gui_lstvw.itemBlock, 0, "xyz");

    FontUA::set_end(&gui_lstvw.itemBlock);
}


void gui_update_create_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( UserUnitOf(yw).Status == BACT_STATUS_DEAD )
    {
        bzda.buttons[0] = ButtonBox(); //create_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 67);

        yw->GuiWinClose( &gui_lstvw );
    }
    else
    {
        bzda.buttons[0] = ButtonBox(bzda.createGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH);

        if ( !(bzda.enabledModes & 0x16) )
        {
            bzda.buttons[0] = ButtonBox(); //create_btn

            FontUA::select_tileset(cur, 23);
            FontUA::store_u8(cur, 67);
        }
        else
        {
            if ( bzda.activeMode & 0x16 )
            {
                if ( gui_lstvw.IsOpen() )
                    BottomBarLayoutBuyList(yw);

                FontUA::select_tileset(cur, 22);
            }
            else
            {
                FontUA::select_tileset(cur, 21);
            }

            int buyMode = bzda.activeMode & 0x16;

            if (buyMode == 0x10)
                FontUA::store_u8(cur, 70);
            else if (buyMode == 4)
                FontUA::store_u8(cur, 68);
            else if (buyMode == 2)
                FontUA::store_u8(cur, 67);
            else
                FontUA::store_u8(cur, 67);
        }
    }
}

void gui_update_map_squad_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( UserUnitOf(yw).Status == BACT_STATUS_DEAD )
    {
        FontUA::select_tileset(cur, 23);

        FontUA::store_u8(cur, 72);
        FontUA::store_u8(cur, 73);

        bzda.buttons[2] = ButtonBox(); //map_btn
        bzda.buttons[3] = ButtonBox(); //squad_btn

        yw->GuiWinClose( &robo_map );
        yw->GuiWinClose( &squadron_manager );
        yw->GuiWinClose( &info_log );
    }
    else
    {
        int btnX = bzda.windowGroupX;

        for (int i = 2; i < 4; i++)
        {
            bzda.buttons[i] = ButtonBox(btnX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH);

            btnX += yw->_iconOrderW;
        }

        if ( robo_map.IsClosed() )
            FontUA::select_tileset(cur, 21);
        else
            FontUA::select_tileset(cur, 22);

        FontUA::store_u8(cur, 72);

        if ( squadron_manager.IsClosed() )
            FontUA::select_tileset(cur, 21);
        else
            FontUA::select_tileset(cur, 22);

        FontUA::store_u8(cur, 73);
    }
}

void gui_update_player_panel(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    const MapScene &scene = yw->UiScene();
    MapScene::Unit user = UserUnitOf(yw);
    bool inHostStation = scene.UserRoboId() == scene.UserUnitId();
    int enabledBtns = 0;

    if ( inHostStation )
    {
        if ( !yw->_cmdrsRemap.empty() )
            enabledBtns = 0x80;

        if ( bzda.gunCount )
            enabledBtns |= 0x40;
    }
    else if ( user.Status == BACT_STATUS_DEAD )
    {
        enabledBtns = 0x90;
    }
    else if ( yw->_playerInHSGun )
    {
        enabledBtns = 0x90;

        if ( bzda.gunCount > 1 )
            enabledBtns = 0xD0;
    }
    else if ( user.Commander )
    {
        enabledBtns = 0x10;

        if ( user.Type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                enabledBtns = 0x90;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            enabledBtns = 0x90;

        bool hasKids = false;
        scene.ForEachKid(user, [&hasKids](const MapScene::Unit &) { hasKids = true; });

        if ( hasKids )
            enabledBtns |= 0x40;
    }
    else
    {
        enabledBtns = 0x70;

        if ( user.Type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                enabledBtns = 0xF0;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            enabledBtns = 0xF0;
    }

    // Back to the host station takes one
    if ( !scene.UserRoboId() )
        enabledBtns &= ~0x10;

    if ( PlayerDefeated(scene) )
        enabledBtns = 0;

    if ( bzda.enabledModes & 8 )
    {
        bzda.buttons[1] = ButtonBox(bzda.unitGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //into_vhcl_btn

        if ( bzda.activeMode & 8 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 69);
    }
    else
    {
        bzda.buttons[1] = ButtonBox();

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 69);
    }


    if ( enabledBtns & 0x10 )
    {
        bzda.buttons[4] = ButtonBox(yw->_iconOrderW + bzda.unitGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //to_host_btn

        if ( bzda.pressedBtns & 0x10 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 81);
    }
    else
    {
        bzda.buttons[4] = ButtonBox(); //to_host_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 81);
    }

    if ( enabledBtns & 0x20 )
    {
        bzda.buttons[5] = ButtonBox(bzda.unitGroupX + 2 * yw->_iconOrderW, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //to_leader_btn

        if ( bzda.pressedBtns & 0x20 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 82);
    }
    else
    {
        bzda.buttons[5] = ButtonBox(); // to_leader_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 82);
    }

    if ( enabledBtns & 0x40 )
    {
        if ( !bzda.gunCount || (!inHostStation && !yw->_playerInHSGun) )
        {
            bzda.buttons[6] = ButtonBox(bzda.unitGroupX + 3 * yw->_iconOrderW, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.pressedBtns & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 83);
        }
        else
        {
            bzda.buttons[6] = ButtonBox(bzda.unitGroupX + 3 * yw->_iconOrderW, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.pressedBtns & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 66);
        }
    }
    else
    {
        bzda.buttons[6] = ButtonBox(); // turrets_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 83);
    }

    if ( enabledBtns & 0x80 )
    {
        bzda.buttons[7] = ButtonBox(4 * yw->_iconOrderW + bzda.unitGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); //next_squad_btn

        if ( bzda.pressedBtns & 0x80 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 84);
    }
    else
    {
        bzda.buttons[7] = ButtonBox(); //next_squad_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 84);
    }
}

void gui_update_tools(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    // The analyzer goes by the host station
    if ( UserUnitOf(yw).Status == BACT_STATUS_DEAD || !yw->UiScene().UserRoboId() )
    {
        bzda.buttons[8] = ButtonBox(); // analyzer_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 63);
    }
    else
    {
        bzda.buttons[8] = ButtonBox(bzda.toolGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); // analyzer_btn

        if ( bzda.pressedBtns & 0x100 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 63);
    }

    bzda.buttons[9] = ButtonBox(yw->_iconOrderW + bzda.toolGroupX, bzda.buttonsY, yw->_iconOrderW, yw->_iconOrderH); // help_btn

    if ( bzda.pressedBtns & 0x200 )
        FontUA::select_tileset(cur, 22);
    else
        FontUA::select_tileset(cur, 21);

    FontUA::store_u8(cur, 76);

    bzda.buttons[10] = ButtonBox(bzda.toolGroupX + 2 * yw->_iconOrderW,  bzda.buttonsY,  yw->_iconOrderW,  yw->_iconOrderH); // menu_btn

    if ( bzda.pressedBtns & 0x400 )
        FontUA::select_tileset(cur, 22);
    else
        FontUA::select_tileset(cur, 21);

    FontUA::store_u8(cur, 85);
}

void BottomBarDrawRatingRow(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, const std::string &label, int rating, int rowWidth)
{
    int iconsX, labelWidth;

    if ( yw->_screenSize.x >= 512 )
    {
        labelWidth = rowWidth - 72;
        iconsX = x + rowWidth - 72 + 16;
    }
    else
    {
        labelWidth = rowWidth - 56;
        iconsX = x + rowWidth - 56 + 8;
    }

    FontUA::select_tileset(cur, 15);

    FontUA::set_xpos(cur, x);
    FontUA::set_ypos(cur, y);

    int heightDiff = yw->_fontH - yw->_guiTiles[51]->h;

    FontUA::ColumnItem arg;
    arg.txt = label;
    arg.width = labelWidth;
    arg.fontID = 15;
    arg.prefixChar = 0;
    arg.spaceChar = 32;
    arg.postfixChar = 0;
    arg.flags = 40;

    FormateTextAlignedClippedString(yw, cur, &arg);

    FontUA::select_tileset(cur, 51);

    FontUA::set_xpos(cur, iconsX);
    FontUA::set_ypos(cur, (heightDiff / 2) + y);

    for (int i = 0; i < rating; i++)
        FontUA::store_u8(cur, yw->UiScene().PlayerOwner());
}

void BottomBarDrawVehicleRatings(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( bzda.activeMode & 6 )
    {
        if ( bzda.selVhclIdx != -1 && !yw->_playerInHSGun )
        {
            World::TVhclProto *proto = &yw->_vhclProtos[ bzda.vhclProtoIds[ bzda.selVhclIdx ] ];
            int tankRating = proto->job_fighttank / 2;
            int flyerRating = proto->job_fightflyer / 2;
            int heliRating = proto->job_fighthelicopter / 2;
            int conquerRating = proto->job_conquer / 2;
            int roboRating = proto->job_fightrobo / 2;
            int reconRating = proto->job_reconnoitre / 2;

            if ( !tankRating )
                tankRating = 1;

            if ( !roboRating )
                roboRating = 1;

            if ( !flyerRating )
                flyerRating = 1;

            if ( !heliRating )
                heliRating = 1;

            if ( !conquerRating )
                conquerRating = 1;

            if ( !reconRating )
                reconRating = 1;

            int rowX = 4 * yw->_screenSize.x / 7;
            int rowWidth = yw->_screenSize.x - rowX;

            int rowY = -(yw->_downScreenBorder + 7 * yw->_fontH);

            FontUA::set_txtColor(cur, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_VSROBO), roboRating, rowWidth);

            rowY += yw->_fontH;

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_VSTANK), tankRating, rowWidth);

            rowY += yw->_fontH;

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_VSPLANE), flyerRating, rowWidth);

            rowY += yw->_fontH;

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_VSHELI), heliRating, rowWidth);

            rowY += yw->_fontH;

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_CAPTURE), conquerRating, rowWidth);

            rowY += yw->_fontH;

            BottomBarDrawRatingRow(yw, cur, rowX, rowY,  Locale::Text::Advanced(Locale::ADV_RECON), reconRating, rowWidth);
        }
    }
}


void BottomBarUpdateLayout(NC_STACK_ypaworld *yw)
{
    UpdateBottomBarModes(yw);

    bzda.cmdCommands.clear();

    if ( bzda.barEnabled & 1 )
    {
        int barX = bzda.x - (yw->_screenSize.x / 2);
        int barY = bzda.y - (yw->_screenSize.y / 2);

        if ( !(bzda.inputAttached & 1) )
        {
            bzda.inputAttached |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }

        Input::Engine.RemClickBox(&bzda);
        Input::Engine.AddClickBoxFront(&bzda);

        FontUA::select_tileset(&bzda.cmdCommands, 21);
        FontUA::set_center_xpos(&bzda.cmdCommands, barX);
        FontUA::set_center_ypos(&bzda.cmdCommands, barY);


        if ( bzda.createGroupX > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.createGroupX);

        gui_update_create_btn(yw, &bzda.cmdCommands);

        if ( bzda.groupGap > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.groupGap);

        gui_update_map_squad_btn(yw, &bzda.cmdCommands);

        if ( bzda.groupGap > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.groupGap);

        gui_update_player_panel(yw, &bzda.cmdCommands);

        if ( bzda.groupGap > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.groupGap);

        gui_update_tools(yw, &bzda.cmdCommands);

        if ( gui_lstvw.IsOpen() )
            BottomBarDrawVehicleRatings(yw, &bzda.cmdCommands);
    }

    FontUA::set_end(&bzda.cmdCommands);

    if ( gui_lstvw.IsOpen() )
        gui_lstvw.Formate(yw);
}


bool FindPreviousUserUnit(NC_STACK_ypaworld *yw, MapScene::Unit &out)
{
    if ( !yw->_prevUnitId )
        return false;

    const MapScene &scene = yw->UiScene();

    if ( scene.UserRoboId() && yw->_prevUnitId == scene.UserRoboId() )
        return scene.UserRobo(out);

    bool found = false;

    scene.ForEachSquad(scene.PlayerOwner(), [&](const MapScene::Unit &comm)
    {
        if ( found || comm.Status == BACT_STATUS_DEAD || comm.Status == BACT_STATUS_CREATE || comm.Status == BACT_STATUS_BEAM )
            return;

        if ( yw->_prevUnitId == comm.Id )
        {
            out = comm;
            found = true;
        }

        scene.ForEachKid(comm, [&](const MapScene::Unit &unit)
        {
            if ( !found && unit.Status != BACT_STATUS_DEAD && unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_BEAM && yw->_prevUnitId == unit.Id )
            {
                out = unit;
                found = true;
            }
        });
    });

    return found;
}

void MakeUserUnitCommander(NC_STACK_ypaworld *yw)
{
    MapScene::Unit user = UserUnitOf(yw);

    if ( user.Id && !user.Commander )
    {
        if ( user.Type != BACT_TYPES_ROBO && user.Type != BACT_TYPES_MISSLE && user.Type != BACT_TYPES_GUN )
        {
            World::PlayerOrder order;
            order.Type = World::PlayerOrder::REORGANIZE;
            order.Unit = user.Id;
            order.Value = 2;
            order.Target = user.HasParent ? user.Parent : 0;
            yw->IssueOrder(order);
        }
    }
}

int BottomBarHandleHotkeys(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    switch ( inpt->HotKeyID )
    {
    case 0:
        if ( bzda.enabledModes & 1 )
            bzda.activeMode = 1;
        break;

    case 1:
        if ( yw->UiScene().UserUnitId() == yw->UiScene().UserRoboId() )
        {
            if ( bzda.buttons[6] ) //turrets_btn
            {
                winpt->selected_btnID = 6;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            }
        }
        break;

    case 2:
        if ( bzda.enabledModes & 2 )
        {
            if ( bzda.activeMode & 0x16 )
            {
                bzda.activeMode = bzda.enabledModes & 1;
            }
            else
            {
                if ( !(bzda.activeMode & 2) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                if ( bzda.vhclCount <= bzda.selBuyListIdx )
                    bzda.activeMode = 16;
                else
                    bzda.activeMode = 2;

                return 0;
            }
        }
        break;

    case 3:
        if ( bzda.enabledModes & 4 )
        {
            if ( bzda.activeMode & 0x16 )
            {
                bzda.activeMode = bzda.enabledModes & 1;
            }
            else
            {
                if ( !(bzda.activeMode & 4) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                bzda.activeMode = 4;
                return 0;
            }
        }
        break;

    case 4:
        if ( bzda.enabledModes & 8 )
        {
            winpt->selected_btn = &bzda;
            winpt->selected_btnID = 1;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 7:
        if ( bzda.enabledModes & 0x20 )
        {
            if ( bzda.activeMode & 0x20 )
                bzda.activeMode = bzda.enabledModes & 1;
            else
                bzda.activeMode = 32;
        }
        break;

    case 8:
        if ( UserUnitOf(yw).Status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 2;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 9:
        if ( UserUnitOf(yw).Status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 3;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 20:
        if ( bzda.buttons[7] ) //next_squad_btn
        {
            winpt->selected_btnID = 7;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 21:
        if ( bzda.buttons[4] ) //to_host_btn
        {
            winpt->selected_btnID = 4;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 22:
        if ( bzda.buttons[6] ) //turrets_btn
        {
            winpt->selected_btnID = 6;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 23:
        if ( bzda.buttons[5] ) //to_leader_btn
        {
            winpt->selected_btnID = 5;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 24:
        if ( PlayerDefeated(yw->UiScene()) || bzda.activeMode & 1 )
        {
            // The menu, from the host station or without one; from a unit,
            // back to the host station
            const MapScene &scene = yw->UiScene();

            if ( scene.UserRoboId() == scene.UserUnitId() || !scene.UserRoboId() || PlayerDefeated(scene) )
            {
                if ( bzda.buttons[10] ) //menu_btn
                {
                    winpt->selected_btnID = 10;
                    winpt->selected_btn = &bzda;
                    winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                }
            }
            else
            {
                winpt->selected_btnID = 4;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                inpt->HotKeyID = -1;
            }
        }
        else
        {
            bzda.activeMode = 1;
        }
        break;

    case 25:
        if ( yw->UiScene().UserUnitId() != yw->UiScene().UserRoboId() )
        {
            if ( yw->_hud.visible )
                yw->_hud.visible = 0;
            else
                yw->_hud.visible = 1;
        }
        break;

    case 27:
        if ( info_log.IsClosed() )
        {
            yw->GuiWinOpen(&info_log);
            yw->GuiWinToFront(&info_log);
        }
        else
            yw->GuiWinClose(&info_log);
        break;

    case 28:
        if ( bzda.activeMode == 2 || bzda.activeMode == 4 || bzda.activeMode == 16 )
        {
            bzda.selBuyListIdx++;
            if ( bzda.selBuyListIdx >= bzda.buyListCount )
                bzda.selBuyListIdx = 0;

            if ( bzda.selBuyListIdx >= bzda.vhclCount )
            {
                bzda.selBuildIdx = bzda.selBuyListIdx - bzda.vhclCount;
                bzda.activeMode = 16;
                bzda.selVhclIdx = -1;
            }
            else
            {
                bzda.selVhclIdx = bzda.selBuyListIdx;
                bzda.selBuildIdx = -1;

                if ( !(bzda.activeMode & 4) )
                    bzda.activeMode = 2;
            }
        }
        return 0;

    case 29:
        if ( bzda.activeMode == 2 || bzda.activeMode == 4 || bzda.activeMode == 16 )
        {
            bzda.selBuyListIdx--;

            if ( bzda.selBuyListIdx < 0 )
                bzda.selBuyListIdx = bzda.buyListCount - 1;

            if ( bzda.selBuyListIdx >= bzda.vhclCount )
            {
                bzda.selBuildIdx = bzda.selBuyListIdx - bzda.vhclCount;
                bzda.selVhclIdx = -1;
                bzda.activeMode = 16;
            }
            else
            {
                bzda.selVhclIdx = bzda.selBuyListIdx;
                bzda.selBuildIdx = -1;

                if ( !(bzda.activeMode & 4) )
                    bzda.activeMode = 2;
            }
        }
        return 0;

    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
        if ( yw->_activeCmdrRemapIndex != -1 )
        {
            int aggression;

            switch ( inpt->HotKeyID )
            {
            case 38:
                aggression = 0;
                break;

            case 39:
                aggression = 25;
                break;

            case 40:
                aggression = 50;
                break;

            case 41:
                aggression = 75;
                break;

            case 42:
                aggression = 100;
                break;
            }

            World::PlayerOrder order;
            order.Type = World::PlayerOrder::AGGRESSION;
            order.Unit = yw->_cmdrsRemap[ yw->_activeCmdrRemapIndex ].Id;
            order.Value = aggression;
            yw->IssueOrder(order);

            if ( yw->_GameShell )
                SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
        }
        break;

    case 43:
        yw->ConfirmOpenHelp(Locale::Text::Help(Locale::HELP_GAMEPLAY));
        break;

    case 46:
        if ( UserUnitOf(yw).Status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 8;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    default:
        break;
    }

    return 1;
}

bool NC_STACK_ypaworld::GetDefaultCmdr(MapScene::Unit &out)
{
    if ( _activeCmdrRemapIndex != -1 )
    {
        out = _cmdrsRemap.at( _activeCmdrRemapIndex );
        return true;
    }

    if ( !_cmdrsRemap.empty() )
    {
        out = _cmdrsRemap.front();
        return true;
    }
    
    return UiScene().UserRobo(out);
}

bool NC_STACK_ypaworld::GetNextCmdrByCmdId(uint32_t cmdrId, MapScene::Unit &out)
{
    for (auto it = _cmdrsRemap.begin(); it != _cmdrsRemap.end(); ++it)
    {
        if ( cmdrId == it->CommandId )
        {
            ++it;
            out = it != _cmdrsRemap.end() ? *it : _cmdrsRemap.front();
            return true;
        }
    }
    
    return GetDefaultCmdr(out);
}

bool NC_STACK_ypaworld::GetNextSquad(MapScene::Unit &out)
{
    const MapScene &scene = UiScene();
    MapScene::Unit user;

    if ( scene.UserUnitId() == scene.UserRoboId() || _playerInHSGun || !scene.UserUnit(user) )
        return GetDefaultCmdr(out);
    
    if ( user.Status == BACT_STATUS_DEAD )
    {
        for (const MapScene::Unit &comm : _cmdrsRemap)
        {
            if ( _vehicleTakenCommandId == comm.CommandId )
            {
                out = comm;
                return true;
            }
        }

        return GetDefaultCmdr(out);
    }
    
    if ( user.Type == BACT_TYPES_GUN )
        return GetDefaultCmdr(out);

    return GetNextCmdrByCmdId(_vehicleTakenCommandId, out);
}

void StoreWinStatus(NC_STACK_ypaworld *yw)
{
    if ( UserUnitOf(yw).Status != BACT_STATUS_DEAD )
    {
        if ( yw->UiScene().UserUnitId() == yw->UiScene().UserRoboId() )
        {
            yw->_roboMapStatus.Valid = true;
            yw->_roboMapStatus.IsOpen = robo_map.IsOpen();
            yw->_roboMapStatus.Rect.x = robo_map.x;
            yw->_roboMapStatus.Rect.y = robo_map.y;
            yw->_roboMapStatus.Rect.w = robo_map.w;
            yw->_roboMapStatus.Rect.h = robo_map.h;

            yw->_roboMapStatus.Data[0] = robo_map.layerFlags;
            yw->_roboMapStatus.Data[1] = robo_map.lockViewer;
            yw->_roboMapStatus.Data[2] = robo_map.zoomLevel;
            yw->_roboMapStatus.Data[3] = robo_map.altX;
            yw->_roboMapStatus.Data[4] = robo_map.altY;
            yw->_roboMapStatus.Data[5] = robo_map.altW;
            yw->_roboMapStatus.Data[6] = robo_map.altH;

            yw->_roboFinderStatus.Valid = true;
            yw->_roboFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_roboFinderStatus.Rect.x = squadron_manager.x;
            yw->_roboFinderStatus.Rect.y = squadron_manager.y;
            yw->_roboFinderStatus.Rect.w = squadron_manager.w;
            yw->_roboFinderStatus.Rect.h = squadron_manager.h;
        }
        else
        {
            yw->_vhclMapStatus.Valid = true;
            yw->_vhclMapStatus.IsOpen = robo_map.IsOpen();
            yw->_vhclMapStatus.Rect.x = robo_map.x;
            yw->_vhclMapStatus.Rect.y = robo_map.y;
            yw->_vhclMapStatus.Rect.w = robo_map.w;
            yw->_vhclMapStatus.Rect.h = robo_map.h;

            yw->_vhclMapStatus.Data[0] = robo_map.layerFlags;
            yw->_vhclMapStatus.Data[1] = robo_map.lockViewer;
            yw->_vhclMapStatus.Data[2] = robo_map.zoomLevel;
            yw->_vhclMapStatus.Data[3] = robo_map.altX;
            yw->_vhclMapStatus.Data[4] = robo_map.altY;
            yw->_vhclMapStatus.Data[5] = robo_map.altW;
            yw->_vhclMapStatus.Data[6] = robo_map.altH;

            yw->_vhclFinderStatus.Valid = true;
            yw->_vhclFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_vhclFinderStatus.Rect.x = squadron_manager.x;
            yw->_vhclFinderStatus.Rect.y = squadron_manager.y;
            yw->_vhclFinderStatus.Rect.w = squadron_manager.w;
            yw->_vhclFinderStatus.Rect.h = squadron_manager.h;
        }
    }
}

// The host station's gun to get in next, as bzda.gunIds has them; out of
// the last, back in the host station
bool GetNextHostStationGun(NC_STACK_ypaworld *yw, MapScene::Unit &out)
{
    const MapScene &scene = yw->UiScene();

    if ( bzda.selGunIdx != -1 )
    {
        if ( yw->_playerInHSGun )
        {
            bzda.selGunIdx++;

            if ( bzda.selGunIdx >= bzda.gunCount )
            {
                bzda.selGunIdx = 0;
                yw->_playerInHSGun = false;
                return scene.UserRobo(out);
            }
        }
        else
        {
            bzda.selGunIdx = 0;
        }

        yw->_playerInHSGun = true;
        return scene.Find(bzda.gunIds[ bzda.selGunIdx ], out);
    }
    return false;
}

// The next unit of the squad the player is in: from its commander the
// first, from a unit the one after it or, after the last, the commander
bool GetNextSquadMember(NC_STACK_ypaworld *yw, MapScene::Unit &out)
{
    const MapScene &scene = yw->UiScene();
    MapScene::Unit user = UserUnitOf(yw);
    bool found = false;

    auto usable = [](const MapScene::Unit &unit)
    {
        return unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_DEAD && unit.Status != BACT_STATUS_BEAM;
    };

    if ( user.Commander )
    {
        scene.ForEachKid(user, [&](const MapScene::Unit &unit)
        {
            if ( !found && usable(unit) )
            {
                out = unit;
                found = true;
            }
        });
    }
    else
    {
        // Those after it in its squad's list
        MapScene::Unit commander;
        bool after = false;

        if ( user.HasParent && scene.Find(user.Parent, commander) )
        {
            scene.ForEachKid(commander, [&](const MapScene::Unit &unit)
            {
                if ( !found && after && usable(unit) )
                {
                    out = unit;
                    found = true;
                }

                if ( unit.Id == user.Id )
                    after = true;
            });
        }
    }

    if ( found )
        return true;

    if ( !user.Commander && user.HasParent )
        return scene.Find(user.Parent, out);

    return false;
}

int ConfirmDialogGetState()
{
    if ( confirmDialogState == 3 )
    {
        if ( lstvw2.IsClosed() )
            confirmDialogState = 2;
    }
    return confirmDialogState;
}

void OpenExitMenu(NC_STACK_ypaworld *yw)
{
    if ( ConfirmDialogGetState() != 3 )
    {
        yw->GuiWinOpen( &exit_menu );
        exit_menu.Attach(yw->_guiActive);
    }
}

void CloseExitMenu(NC_STACK_ypaworld *yw)
{
    if ( ConfirmDialogGetState() != 3 )
        yw->GuiWinClose( &exit_menu );
}

// Asks whether to leave the training mission; yes acts as the exit menu's
// leave button
void OpenExitTrainingDialog(NC_STACK_ypaworld *yw)
{
    if ( ConfirmDialogGetState() != 3 )
    {
        exitMenuPendingAction = 8;
        yw->OpenConfirmDialog( Locale::Text::Advanced(Locale::ADV_EXITRAINING) , &exit_menu);
    }
}

void NC_STACK_ypaworld::SituationAnalyzer()
{
    // It goes by the host station
    if ( !UserRobo() )
        return;

    TInfoMessage msg;
    msg.txt = World::GameAnalyzer::Analyze(this);
    msg.unit = 0;
    msg.Priority = 100;
    msg.MsgID = 0;

    ShowMessage(&msg);
}

void UpdateBuyList()
{
    bzda.buyListCount = 0;

    for (int i = 0; i < bzda.vhclCount; i++)
    {
        bzda.buyList[i].kind = 1;
        bzda.buyList[i].protoId = bzda.vhclProtoIds[i];
    }

    bzda.buyListCount += bzda.vhclCount;

    if ( !(bzda.activeMode & 4) )
    {
        for (int i = 0; i < bzda.buildCount; i++)
        {
            bzda.buyList[i + bzda.vhclCount].kind = 2;
            bzda.buyList[i + bzda.vhclCount].protoId = bzda.buildProtoIds[i];
        }

        bzda.buyListCount += bzda.buildCount;
    }

    if ( bzda.buyListCount )
    {
        if ( bzda.buyListCount == 1 )
        {
            bzda.selBuyListIdx = 0;
        }
        else if ( bzda.buyListCount <= bzda.selBuyListIdx )
        {
            bzda.selBuyListIdx = bzda.buyListCount - 1;
        }
    }
    else
    {
        bzda.selBuyListIdx = -1;
    }
}

void NotifyReturnToHostStation(NC_STACK_ypaworld *yw)
{
    if ( yw->_timeStamp - yw->_msgTimestampHSReturn > 35000 )
    {
        MapScene::Unit robo;

        TInfoMessage msg;
        msg.unit = yw->UiScene().UserRobo(robo) ? robo.Bact : NULL;
        msg.Priority = 10;
        msg.MsgID = 39;

        yw->ShowMessage(&msg);
    }

    yw->_msgTimestampHSReturn = yw->_timeStamp;
}

// The player gets in a unit of theirs, from the one they're in
void SwitchControlToUnit(NC_STACK_ypaworld *yw, const MapScene::Unit &unit)
{
    const MapScene &scene = yw->UiScene();

    if ( unit.Id != scene.UserUnitId() )
    {
        if ( unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_DEAD && unit.Status != BACT_STATUS_BEAM )
        {
            yw->_hud.switch_time = yw->_timeStamp;

            World::PlayerOrder order;
            order.Type = World::PlayerOrder::GET_IN;
            order.Unit = unit.Id;
            yw->IssueOrder(order);

            yw->GuiWinClose( &gui_lstvw );

            if ( bzda.enabledModes & 1 )
            {
                if ( bzda.activeMode == 8 )
                    bzda.activeMode = 1;
            }

            if ( unit.Id == scene.UserRoboId() )
            {
                yw->_mouseGrabbed = false;
                NotifyReturnToHostStation(yw);
            }
            else
            {
                TInfoMessage msg;
                msg.unit = scene.Fresh(unit).Bact;
                msg.Priority = 33;
                msg.MsgID = 17;

                yw->ShowMessage(&msg);
            }

            yw->_guiDragDefaultMouse = false;
        }
    }
}



void  BottomBarHandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;
    const MapScene &scene = yw->UiScene();

    // The unit to get in, if any
    MapScene::Unit getIn;
    bool switching = false;

    bzda.pressedBtns = 0;

    int passToList = 1;
    if ( bzda.barEnabled & 1 )
    {
        switch ( inpt->HotKeyID )
        {
            case 31:
            {
                if ( yw->GetLastMsgSender(getIn) )
                    switching = true;
            }
            break;

            case 44:
                switching = FindPreviousUserUnit(yw, getIn);
                break;

            case 45:
                MakeUserUnitCommander(yw);
                break;

            default:
                if ( inpt->HotKeyID >= 0 )
                    passToList = BottomBarHandleHotkeys(yw, inpt);

                break;
        }

        if ( UserUnitOf(yw).Status == BACT_STATUS_DEAD )
        {
            if ( yw->_fireBtnDownHappen )
            {
                switching = yw->GetNextSquad(getIn);
                inpt->Buttons.UnSet(0);
            }
        }

        if ( gui_lstvw.IsOpen() )
        {
            if ( yw->_kbdLastKeyHit == Input::KC_ESCAPE || yw->_kbdLastKeyHit == Input::KC_RETURN )
            {
                yw->GuiWinClose(&gui_lstvw);

                inpt->KbdLastDown = Input::KC_NONE;
                inpt->HotKeyID = -1;
                inpt->KbdLastHit = Input::KC_NONE;
            }
        }

        // TODO(cleanup): stateFlags bit 8 is set and cleared here but never read.
        if ( scene.UserUnitId() == scene.UserRoboId() )
        {
            if ( robo_map.stateFlags & 8 )
                robo_map.stateFlags &= 0xFFFFFFF7;
        }
        else if ( !(robo_map.stateFlags & 8) )
        {
            robo_map.stateFlags |= 8;
        }

        StoreWinStatus(yw);

        if ( winpt->selected_btn == &bzda )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( yw->_GameShell )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
                }
            }

            switch ( winpt->selected_btnID )
            {
            case 0:
                if ( !(winpt->flag & TClickBoxInf::FLAG_BTN_DOWN) )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
                    {
                        if ( bzda.activeMode & 0x16 )
                        {
                            bzda.activeMode = bzda.enabledModes & 1;
                            yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                        }
                        else
                        {

                            if ( !(bzda.activeMode & 4) || gui_lstvw.IsClosed() )
                            {
                                yw->GuiWinOpen( &gui_lstvw );
                                gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                            }
                            else
                            {
                                yw->GuiWinClose( &gui_lstvw );
                            }

                            passToList = 0;
                            bzda.activeMode = 4;
                        }
                    }
                    else
                        yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);

                }
                else if ( bzda.activeMode & 0x16 )
                {
                    bzda.activeMode = bzda.enabledModes & 1;

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                else
                {
                    if ( !(bzda.activeMode & 2) || gui_lstvw.IsClosed() )
                    {
                        yw->GuiWinOpen( &gui_lstvw );
                        gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                    }
                    else
                    {
                        yw->GuiWinClose( &gui_lstvw );
                    }

                    if ( bzda.vhclCount <= bzda.selBuyListIdx )
                        bzda.activeMode = 16;
                    else
                        bzda.activeMode = 2;

                    passToList = 0;
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                break;

            case 1: // Jump into vehicle
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( bzda.activeMode & 8 )
                        bzda.activeMode = bzda.enabledModes & 1;
                    else
                        bzda.activeMode = 8;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_TAKECONTROL, 4);
                break;

            case 2: //MAP
            case 3: //SQUAD
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( winpt->selected_btnID == 2 )
                    {
                        if (robo_map.IsClosed())
                        {
                            yw->GuiWinOpen(&robo_map);
                            yw->GuiWinToFront(&robo_map);
                        }
                        else
                            yw->GuiWinClose(&robo_map);
                    }
                    else if ( winpt->selected_btnID == 3 )
                    {
                        if (squadron_manager.IsClosed())
                        {
                            yw->GuiWinOpen(&squadron_manager);
                            yw->GuiWinToFront(&squadron_manager);
                        }
                        else
                            yw->GuiWinClose(&squadron_manager);
                    }
                }

                if ( winpt->selected_btnID == 2 )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_MAPWND, 8);
                else if ( winpt->selected_btnID == 3 )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_FINDERWND, 9);

                break;

            case 4: //Back to host station
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x10;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    switching = scene.UserRobo(getIn);

                    if ( yw->_playerInHSGun )
                        bzda.activeMode = 1;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOHS, 21);
                break;

            case 5:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x20;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    MapScene::Unit user = UserUnitOf(yw);
                    switching = user.HasParent && scene.Find(user.Parent, getIn);
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOCMDR, 23);
                break;

            case 6: // Cycle through units in squad
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x40;

                if ( scene.UserRoboId() == scene.UserUnitId() || yw->_playerInHSGun )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        switching = GetNextHostStationGun(yw, getIn);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOINTOGUNS, 22);
                }
                else
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        switching = GetNextSquadMember(yw, getIn);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTUNIT, 22);
                }
                break;

            case 7: //Cycle through squad leader
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x80;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    switching = yw->GetNextSquad(getIn);

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTCMDR, 20);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    yw->SituationAnalyzer();

                yw->SetShowingTooltipWithHotkey(Locale::TIP_ANALYZER, 46);
                break;

            case 9:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x200;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    yw->ConfirmOpenHelp(Locale::Text::Help(Locale::HELP_GAMEPLAY));
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_ONLINEHELP, 43);
                break;

            case 10:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.pressedBtns |= 0x400;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    if ( exit_menu.IsClosed() )
                        OpenExitMenu(yw);
                    else
                        CloseExitMenu(yw);
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_PAUSECANCEL, 24);
                break;

            default:
                break;
            }
        }

        if ( bzda.activeMode & 0x29 )
            yw->GuiWinClose(&gui_lstvw);

        if ( passToList )
        {
            gui_lstvw.InputHandle(yw, inpt);
            if ( gui_lstvw.IsOpen() )
            {
                if ( bzda.activeMode == 2 || bzda.activeMode == 4 || bzda.activeMode == 16)
                {
                    bzda.selBuyListIdx = gui_lstvw.selectedEntry;

                    if ( gui_lstvw.selectedEntry >= bzda.vhclCount )
                    {
                        bzda.selBuildIdx = gui_lstvw.selectedEntry - bzda.vhclCount;
                        bzda.selVhclIdx = -1;
                        bzda.activeMode = 16;
                    }
                    else
                    {
                        bzda.selBuildIdx = -1;
                        bzda.selVhclIdx = gui_lstvw.selectedEntry;

                        if ( !(bzda.activeMode & 4) )
                            bzda.activeMode = 2;
                    }
                }
            }
        }
    }

    if ( switching && getIn.Id )
        SwitchControlToUnit(yw, getIn);

    UpdateBuyList();
    BottomBarUpdateLayout(yw);
}







void EnergyPanelDrawGauge(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, uint8_t icon, uint8_t icon2, uint8_t icon3, float fillRatio, float highlightRatio, const std::string &label)
{
    int fillLen = (fillRatio * up_panel.barMaxLen);
    int highlightLen = (highlightRatio * up_panel.barMaxLen);

    if ( !fillLen && fillRatio > 0.0 )
        fillLen = 1;

    if ( !highlightLen && highlightRatio > 0.0 )
        highlightLen = 1;

    FontUA::select_tileset(cur, 30);
    FontUA::set_center_xpos(cur, x);
    FontUA::set_center_ypos(cur, y);

    FontUA::store_u8(cur, icon);

    FontUA::store_u8(cur, 84);

    if ( fillLen > 0 || highlightLen > 0 )
    {
        int barX = up_panel.barGap + up_panel.iconWidth + x;

        FontUA::set_center_xpos(cur, barX);

        if ( fillLen > 0 )
        {
            if ( fillLen > 1 )
                FontUA::op11(cur, fillLen);

            FontUA::store_u8(cur, icon2);
        }

        if ( highlightLen > fillLen )
        {
            if ( highlightLen > 1 )
                FontUA::op11(cur, highlightLen);

            FontUA::store_u8(cur, icon3);
        }
    }

    if ( !label.empty() )
    {
        FontUA::select_tileset(cur, 31);

        FontUA::set_center_xpos(cur, x + up_panel.iconWidth + 4);
        FontUA::set_ypos(cur, 0);

        cur->insert(cur->end(), label.begin(), label.end());

        //pcur += a8.size();
    }
}

TReloadGauge ReloadGauge(uint8_t owner, uint8_t sectorOwner, int sectorPower, float sectorRatio, int absReload)
{
    TReloadGauge gauge;
    gauge.SectorOwner = sectorOwner;
    gauge.SectorPower = sectorPower;
    gauge.Ratio = sectorRatio;
    gauge.Amount = 10 * ( absReload * sectorRatio ) / 100;

    int effectivePower = sectorPower * sectorRatio;

    if ( owner == sectorOwner )
        gauge.State = effectivePower != 0;
    else if ( effectivePower )
        gauge.State = -1;
    else
        gauge.State = 0;

    if ( gauge.State >= 0 )
    {
        if ( gauge.Amount )
            gauge.Label = fmt::sprintf("+%d   (%d%%)", gauge.Amount, (int)(sectorRatio * 100.0) );
        else
            gauge.Label = fmt::sprintf("%d", 0);
    }
    else
    {
        gauge.Label = fmt::sprintf("%d", -gauge.Amount);
    }

    return gauge;
}

void EnergyPanelDrawReloadGauge(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, const TReloadGauge &gauge)
{
    int loadState = gauge.State;
    int sectorPower = gauge.SectorPower;
    float reloadRatio = gauge.Ratio;

    int icon;

    if ( loadState == -1 )
        icon = 82;
    else if ( loadState == 0 )
        icon = 83;
    else if ( loadState == 1 )
        icon = 81;

    int ownerGlyph = 0;
    switch ( gauge.SectorOwner )
    {
    case 1:
        ownerGlyph = 49;
        break;

    case 2:
        ownerGlyph = 50;
        break;

    case 3:
        ownerGlyph = 51;
        break;

    case 4:
        ownerGlyph = 52;
        break;

    case 5:
        ownerGlyph = 53;
        break;

    case 6:
        ownerGlyph = 54;
        break;

    case 7:
        ownerGlyph = 55;
        break;

    default:
        ownerGlyph = 0;
        break;
    }

    float fillRatio, powerRatio;

    if ( sectorPower <= 0 )
    {
        powerRatio = 0.0;
        fillRatio = 0.0;
    }
    else
    {
        powerRatio = sectorPower / 255.0;
        fillRatio = reloadRatio * powerRatio;
    }

    EnergyPanelDrawGauge(yw, cur, x, y, icon, ownerGlyph, 57, fillRatio, powerRatio, gauge.Label);
}

void EnergyPanelDrawMainGauge(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char fillMode, int loadState, int energy, int maxEnergy)
{
    int icon;
    if ( fillMode & 1 )
    {
        if ( loadState == -1 )
        {
            icon = 66;
        }
        else if ( loadState == 0 )
        {
            icon = 67;
        }
        else if ( loadState == 1 )
        {
            icon = 65;
        }
    }
    else
    {
        icon = 68;
    }

    float fillRatio = (float)energy / (float)maxEnergy;
    std::string label = fmt::sprintf("%d", energy / 100);

    int barGlyph;

    if ( maxEnergy / 5 <= energy )
    {
        barGlyph = 56;
    }
    else
    {
        barGlyph = 54;

        if ( yw->_timeStamp / 300 & 1 )
            label = "";
    }

    EnergyPanelDrawGauge(yw, cur, x, y, icon, barGlyph, 0, fillRatio, 0.0, label);
}


void EnergyPanelDrawCreateGauge(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char fillMode, int loadState, int energy, int maxEnergy)
{
    int icon;
    if ( fillMode & 4 )
    {
        if ( loadState == -1 )
            icon = 70;
        else if ( loadState == 0 )
            icon = 71;
        else if ( loadState == 1 )
            icon = 69;
    }
    else
    {
        icon = 72;
    }

    float capacity = (float)maxEnergy;
    int costGlyph = 57;
    float costRatio = 0.0;
    float fillRatio = (float)energy / capacity;

    if ( bzda.activeMode & 0x16 )
    {
        if ( bzda.selBuyListIdx != -1 )
            costRatio = yw->_actionEnergyCost / capacity;
    }

    if ( costRatio <= fillRatio )
    {
        float currentRatio = fillRatio;
        fillRatio -= costRatio;
        costRatio = currentRatio;
    }
    else
    {
        costRatio = fillRatio;
        costGlyph = 54;
        fillRatio = 0.0;
    }

    EnergyPanelDrawGauge(yw, cur, x, y, icon, 56, costGlyph, fillRatio, costRatio,  fmt::sprintf("%d", energy / 100) );
}

void EnergyPanelDrawBeamGauge(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char fillMode, int loadState, int energy, int maxEnergy)
{
    int icon;
    if ( fillMode & 8 )
    {
        if ( loadState == -1 )
            icon = 78;
        else if ( loadState == 0 )
            icon = 79;
        else if ( loadState == 1 )
            icon = 77;
    }
    else
    {
        icon = 80;
    }

    float capacity = (float)maxEnergy;
    int costGlyph = 57;
    float costRatio = 0.0;
    float fillRatio = (float)energy / capacity;

    if ( bzda.activeMode & 0x20 )
        costRatio = yw->_actionEnergyCost / capacity;

    if ( costRatio <= fillRatio )
    {
        float currentRatio = fillRatio;
        fillRatio -= costRatio;
        costRatio = currentRatio;
    }
    else
    {
        costRatio = fillRatio;
        costGlyph = 54;
        fillRatio = 0.0;
    }

    EnergyPanelDrawGauge(yw, cur, x, y, icon, 56, costGlyph, fillRatio, costRatio,  fmt::sprintf("%d", energy / 100) );
}

void EnergyPanelUpdateLayout(NC_STACK_ypaworld *yw)
{
    up_panel.cmdCommands.clear();

    // The host station's energy
    const MapScene &scene = yw->UiScene();
    MapScene::Unit robo;
    MapScene::Batteries batteries;

    if ( scene.UserRobo(robo) && robo.Status != BACT_STATUS_DEAD && scene.HostStationBatteries(robo.Id, batteries) )
    {
        int panelX = up_panel.x - yw->_screenSize.x / 2;
        int y = up_panel.y - yw->_screenSize.y / 2;
        int gaugeStep = up_panel.gaugeSpacing + up_panel.gaugeWidth;

        int fillMode = batteries.FillMode;
        int vehicleBattery = batteries.Create;
        int beamBattery = batteries.Beam;
        int loadFlags = batteries.LoadFlags;
        int lossFlags = batteries.LossFlags;

        int maxEnergy = batteries.Max;

        TReloadGauge reload = ReloadGauge(robo.Owner, batteries.SectorOwner, batteries.SectorPower, batteries.ReloadRatio,
                                          batteries.AbsReload);

        int mainState, createState, beamState;

        if ( loadFlags & 1 )
            mainState = 1;
        else if ( lossFlags & 1 )
            mainState = -1;
        else
            mainState = 0;

        if ( loadFlags & 4 )
            createState = 1;
        else if ( lossFlags & 4 )
            createState = -1;
        else
            createState = 0;

        if ( loadFlags & 8 )
            beamState = 1;
        else if ( lossFlags & 8 )
            beamState = -1;
        else
            beamState = 0;

        Input::Engine.RemClickBox(&up_panel);
        Input::Engine.AddClickBoxFront(&up_panel);

        // TODO(cleanup): The & 0xFFFF is a 16-bit leftover from the decompile.
        int x = (up_panel.gaugeStartX & 0xFFFF) + panelX;

        EnergyPanelDrawReloadGauge(yw, &up_panel.cmdCommands, x, y, reload);

        int mainX = gaugeStep + x;
        EnergyPanelDrawMainGauge(yw, &up_panel.cmdCommands, mainX, y, fillMode, mainState, batteries.Main, maxEnergy);

        int createX = gaugeStep + mainX;
        EnergyPanelDrawCreateGauge(yw, &up_panel.cmdCommands, createX, y, fillMode, createState, vehicleBattery, maxEnergy);
        EnergyPanelDrawBeamGauge(yw, &up_panel.cmdCommands, gaugeStep + createX, y, fillMode, beamState, beamBattery, maxEnergy);
    }

    FontUA::set_end(&up_panel.cmdCommands);
}

void EnergyPanelHandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( winpt->selected_btn == &up_panel )
    {
        if ( yw->_GameShell )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                    SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            }
        }

        const MapScene &scene = yw->UiScene();
        MapScene::Unit robo;
        MapScene::Batteries batteries;

        if ( (winpt->flag & TClickBoxInf::FLAG_BTN_UP) && scene.UserRobo(robo) && scene.HostStationBatteries(robo.Id, batteries) )
        {
            int fillMode = batteries.FillMode;

            if ( winpt->selected_btnID == 1 )
                fillMode ^= 1;
            else if ( winpt->selected_btnID == 2 )
                fillMode ^= 4;
            else if ( winpt->selected_btnID == 3 )
                fillMode ^= 8;

            World::PlayerOrder order;
            order.Type = World::PlayerOrder::FILL_MODE;
            order.Unit = robo.Id;
            order.Value = fillMode;
            yw->IssueOrder(order);
        }

        switch ( winpt->selected_btnID )
        {
        case 0:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_RELOAD);
            break;

        case 1:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_MAIN);
            break;

        case 2:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_CREATION);
            break;

        case 3:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_TELEPORT);
            break;

        default:
            break;
        }
    }

    EnergyPanelUpdateLayout(yw);
}


char GetVehicleTypeIcon(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    return VehicleTypeIcon(yw, bact->_vehicleID);
}

char VehicleTypeIcon(NC_STACK_ypaworld *yw, int vehicle)
{
    uint8_t icon = vehicle >= 0 && vehicle < (int)yw->_vhclProtos.size() ? yw->_vhclProtos[ vehicle ].type_icon : 0;

    if ( !icon )
        icon = 65;

    return icon;
}

void SquadManagerDrawDragIcon(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y)
{
    if ( x > 0 && y > 0 )
    {
        if ( x + squadron_manager.iconWidth < yw->_screenSize.x )
        {
            if ( y + yw->_fontH < yw->_screenSize.y )
            {
                FontUA::select_tileset(cur, 28);

                FontUA::set_center_xpos(cur, x - (yw->_screenSize.x / 2));
                FontUA::set_center_ypos(cur, y - (yw->_screenSize.y / 2));

                FontUA::store_u8(cur, VehicleTypeIcon(yw, squadron_manager.dragUnit.Vehicle));
            }
        }
    }
}

void SquadManagerLayoutEmptyLine(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 123);

    FontUA::op17(cur, squadron_manager.entryWidth - yw->_fontBorderW);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}


void LayoutClippedTileString(TileMap *tyle, CmdStream *cur, const std::string &st, int maxWidth, char padChar)
{
    int wdth = 0;
    int char_num = 0;
    int neg_wdth = 0;

    for (size_t i = 0; i < st.size() && neg_wdth == 0; i++ )
    {
        uint8_t chr = (uint8_t)st[i];

        wdth += tyle->map[chr].w;

        if ( wdth <= maxWidth )
            char_num++;
        else
            neg_wdth = tyle->map[chr].w - (wdth - maxWidth);
    }

    for (int i = 0; i < char_num; i++)
        cur->push_back( st[i] );


    if ( neg_wdth <= 0 )
    {
        if ( wdth < maxWidth )
        {
            int padWidth = maxWidth - wdth;

            while( padWidth > 0 )
            {
                if ( padWidth <= 255 )
                    FontUA::op10(cur, padWidth);
                else
                    FontUA::op10(cur, 255);

                padWidth -= 255;

                FontUA::store_u8(cur, padChar);
            }
        }
    }
    else
    {
        FontUA::set_xwidth(cur, neg_wdth);

        FontUA::store_u8(cur, st[char_num]);
    }
}

// The host stations' icons: the one the player is in, or the one last
// heard from, blinks
std::string SquadManagerHostStationIcons(NC_STACK_ypaworld *yw)
{
    std::string icons;

    uint32_t userUnit = yw->UiScene().UserUnitId();

    for ( const MapScene::Unit &host : squadron_manager.hostStations )
    {
        if ( host.Id == userUnit && yw->_timeStamp / 300 & 1 )
            icons += (char)33;
        else if ( info_log.last_sender_gid == host.Id && yw->_timeStamp - info_log.msgs[info_log.last_msg_idx].timestamp < 10000 && yw->_timeStamp / 300 & 1 )
            icons += (char)34;
        else
            icons += VehicleTypeIcon(yw, host.Vehicle);
    }

    return icons;
}

void SquadManagerLayoutHostStations(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    std::string iconStr = SquadManagerHostStationIcons(yw);

    int bgTileset;

    if ( bzda.activeMode & 0x20 )
        bgTileset = 9;
    else
        bgTileset = 0;

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, bgTileset);
    FontUA::op17(cur, squadron_manager.entryWidth - yw->_fontBorderW);
    FontUA::store_u8(cur, 32);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 125);
    FontUA::add_xpos(cur, -(squadron_manager.entryWidth - 2 * yw->_fontBorderW + 1));

    FontUA::select_tileset(cur, 28);

    LayoutClippedTileString(yw->_guiTiles[28], cur, iconStr, squadron_manager.entryWidth - 2 * yw->_fontBorderW, 64);

    FontUA::next_line(cur);
}

char GetSquadStatusIcon(const MapScene::Unit &unit)
{
    return SquadStatusIcon(unit.Status, unit.StatusFlags, unit.PrimaryType, unit.SecondaryType);
}

char SquadStatusIcon(uint8_t status, int32_t statusFlags, uint8_t primaryType, uint8_t secondaryType)
{
    if ( status == BACT_STATUS_NORMAL )
    {
        if ( statusFlags & BACT_STFLAG_ESCAPE )
        {
            return 102;
        }
        else if ( secondaryType )
        {
            return 99;
        }
        else if ( primaryType )
        {
            return 98;
        }
    }
    else if ( status == BACT_STATUS_IDLE )
    {
        if ( statusFlags & BACT_STFLAG_ESCAPE )
            return 102;
    }

    return 97;
}

void LayoutSquadRow(NC_STACK_ypaworld *yw, CmdStream *cur, int entryWidth, char status, const std::string &units,
                    bool chosen, int hoveredX, uint8_t hoveredEnergy)
{
    std::string icons{status, '@'};

    if ( !units.empty() )
    {
        icons += units[0];
        icons += '@';
        icons.append(units, 1, std::string::npos);
    }

    int inner = entryWidth - 2 * yw->_fontBorderW;

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, chosen ? 9 : 0);
    FontUA::op17(cur, entryWidth - yw->_fontBorderW);
    FontUA::store_u8(cur, 32);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 125);

    FontUA::add_xpos(cur, -(inner + 1));

    FontUA::select_tileset(cur, 28);

    LayoutClippedTileString(yw->_guiTiles[28], cur, icons, inner, 64);

    if ( hoveredX && entryWidth - yw->_guiTiles[28]->map[65].w - yw->_fontBorderW > hoveredX )
    {
        FontUA::add_xpos(cur, -inner);
        FontUA::add_xpos(cur, hoveredX);

        FontUA::store_u8(cur, hoveredEnergy);
    }

    FontUA::next_line(cur);
}

void SquadManagerLayoutSquad(NC_STACK_ypaworld *yw, const MapScene::Unit &commander, CmdStream *cur)
{
    const MapScene &scene = yw->UiScene();
    const MapScene::Unit *hoverUnit = NULL;

    if ( yw->_guiActFlags & 0x20 )
        hoverUnit = &yw->_bactOnMouse;

    int ttt = (yw->_timeStamp / 300) & 1;
    uint32_t userUnit = scene.UserUnitId();

    // The one steered and the last to report blink
    auto icon = [yw, ttt, userUnit](const MapScene::Unit &unit)
    {
        if ( unit.Id == userUnit && ttt )
            return '!';
        else if ( unit.Id == yw->_lastMsgSender.Id && ttt )
            return '"';
        else
            return VehicleTypeIcon(yw, unit.Vehicle);
    };

    std::string units(1, icon(commander));
    int hoverIconX = hoverUnit && commander.Id == hoverUnit->Id ? squadron_manager.cmdrIconX : 0;
    int iconX = squadron_manager.kidIconsX;

    scene.ForEachKid(commander, [&](const MapScene::Unit &kid)
    {
        if ( units.size() >= SquadRowMaxUnits )
            return;

        if ( kid.Status != BACT_STATUS_DEAD && kid.Status != BACT_STATUS_CREATE && kid.Status != BACT_STATUS_BEAM )
        {
            units += icon(kid);

            if ( hoverUnit && kid.Id == hoverUnit->Id )
                hoverIconX = iconX;

            iconX += squadron_manager.iconWidth;
        }
    });

    bool chosen = yw->_activeCmdrRemapIndex != -1 && commander.Id == yw->_cmdrsRemap[ yw->_activeCmdrRemapIndex ].Id &&
                  !(bzda.activeMode & 0x20);

    LayoutSquadRow(yw, cur, squadron_manager.entryWidth, GetSquadStatusIcon(commander), units, chosen, hoverIconX,
                   hoverUnit ? EnergyGlyph(hoverUnit->Energy, hoverUnit->EnergyMax) : 0);
}

void SquadManagerLayoutFooter(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int fillWidth = squadron_manager.entryWidth - yw->_fontBorderW;

    if ( yw->_activeCmdrRemapIndex == -1 || bzda.activeMode & 0x20 )
    {
        FontUA::select_tileset(cur, 0);
        FontUA::store_u8(cur, '{');
    }
    else
    {
        int aggression = yw->_cmdrsRemap[ yw->_activeCmdrRemapIndex ].Aggression;
        FontUA::select_tileset(cur, 0);
        FontUA::store_u8(cur, '{');
        FontUA::store_u8(cur, ' ');

        FontUA::select_tileset(cur, 25);
        FontUA::store_u8(cur, 49);

        if (aggression < 25)
            FontUA::select_tileset(cur, 24);

        FontUA::store_u8(cur, 50);

        if (aggression >= 25 && aggression < 50)
            FontUA::select_tileset(cur, 24);

        FontUA::store_u8(cur, 51);

        if (aggression >= 50 && aggression < 75)
            FontUA::select_tileset(cur, 24);

        FontUA::store_u8(cur, 52);

        if (aggression >= 75 && aggression < 100)
            FontUA::select_tileset(cur, 24);

        FontUA::store_u8(cur, 53);

        FontUA::select_tileset(cur, 0);

        FontUA::set_txtColor(cur, yw->_iniColors[60].r, yw->_iniColors[60].g, yw->_iniColors[60].b);

        FontUA::FormateClippedText(yw->_guiTiles[0], cur, fmt::sprintf(" %d", yw->_activeCmdrKidsCount + 1) , 4 * yw->_guiTiles[0]->map[65].w, 32);

        FontUA::store_u8(cur, ' ');

        FontUA::select_tileset(cur, 0);
    }

    FontUA::op17(cur, fillWidth);
    FontUA::store_u8(cur, ' ');
    FontUA::store_u8(cur, '}');

    FontUA::next_line(cur);

    FontUA::reset_tileset(cur, 28);

    FontUA::set_yoff(cur, yw->_guiTiles[28]->h - yw->_fontBorderH);

    FontUA::store_u8(cur, 38);

    FontUA::op17(cur, squadron_manager.entryWidth - yw->_fontBorderW);

    FontUA::store_u8(cur, 47);
    FontUA::store_u8(cur, 61);
}



void SquadManagerLayoutItems(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;
    
    squadron_manager.itemBlock.clear();

    squadron_manager.ItemsPreLayout(yw, &squadron_manager.itemBlock, 0, "{ }");

    FontUA::select_tileset(&squadron_manager.itemBlock, 28);

    for (int i = 0; i < squadron_manager.shownEntries; i++)
    {
        const MapScene::Unit &squad = squadron_manager.squads[i];

        if ( squad.Id )
        {
            if ( squad.Type == BACT_TYPES_ROBO )
            {
                SquadManagerLayoutHostStations(yw, &squadron_manager.itemBlock);
            }
            else
            {
                if ( squad.Commander )
                    SquadManagerLayoutSquad(yw, squad, &squadron_manager.itemBlock);
                else
                    SquadManagerLayoutEmptyLine(yw, &squadron_manager.itemBlock);
            }
        }
        else
        {
            SquadManagerLayoutEmptyLine(yw, &squadron_manager.itemBlock);
        }
    }

    SquadManagerLayoutFooter(yw, &squadron_manager.itemBlock);

    if ( squadron_manager.dragFlags & 1 )
    {
        if ( squadron_manager.dragStartX != winpt->move.ScreenPos.x || squadron_manager.dragStartY != winpt->move.ScreenPos.y )
        {
            SquadManagerDrawDragIcon(yw, &squadron_manager.itemBlock, squadron_manager.dragIconX, squadron_manager.dragIconY);

            if ( squadron_manager.dragFlags & 2 )
                SquadManagerDrawDragIcon(yw, &squadron_manager.itemBlock, squadron_manager.dragIconX + squadron_manager.iconWidth / 4, squadron_manager.dragIconY + (yw->_fontH / 4) );
        }
    }

    FontUA::set_end(&squadron_manager.itemBlock);
}

void SquadManagerUpdateRows(NC_STACK_ypaworld *yw)
{
    const MapScene &scene = yw->UiScene();

    squadron_manager.squads.fill(MapScene::Unit());

    // The player's faction's host stations still going, in the order they
    // came
    squadron_manager.hostStations.clear();

    scene.ForEachHostStation(scene.PlayerOwner(), [](const MapScene::Unit &host)
    {
        squadron_manager.hostStations.push_back(host);
    });

    squadron_manager.hostRows = squadron_manager.hostStations.empty() ? 0 : 1;

    int rows = yw->_cmdrsRemap.size() + squadron_manager.hostRows;

    if ( squadron_manager.firstShownEntries + squadron_manager.shownEntries >= rows )
    {
        squadron_manager.firstShownEntries = rows - squadron_manager.shownEntries;

        if ( squadron_manager.firstShownEntries < 0 )
            squadron_manager.firstShownEntries = 0;
    }

    int firstShown = squadron_manager.firstShownEntries;
    for (int i = 0; i < rows; i++ )
    {
        const MapScene::Unit &squad = i >= squadron_manager.hostRows ? yw->_cmdrsRemap.at(i - squadron_manager.hostRows)
                                                                     : squadron_manager.hostStations.front();

        if ( i >= firstShown && i - firstShown < squadron_manager.shownEntries )
            squadron_manager.squads[i - firstShown] = squad;
    }

    squadron_manager.numEntries = rows;
}


bool NC_STACK_ypaworld::SquadManagerIsDragValid()
{
    if ( squadron_manager.dragCmdrIdx < 0 || (size_t)squadron_manager.dragCmdrIdx >= _cmdrsRemap.size() )
        return false;

    const MapScene::Unit &commander = _cmdrsRemap[squadron_manager.dragCmdrIdx];
    uint32_t dragged = squadron_manager.dragUnit.Id;

    if ( commander.Id == dragged )
    {
        if (commander.Status != BACT_STATUS_DEAD && commander.Status != BACT_STATUS_CREATE && commander.Status != BACT_STATUS_BEAM )
            return true;
    }

    bool valid = false;

    UiScene().ForEachKid(commander, [&](const MapScene::Unit &kid)
    {
        if ( kid.Id == dragged && kid.Status != BACT_STATUS_DEAD && kid.Status != BACT_STATUS_CREATE && kid.Status != BACT_STATUS_BEAM )
            valid = true;
    });

    return valid;
}

// The units of a squad from one of them on, in the squad's order
static std::vector<uint32_t> SquadUnitsFrom(const MapScene &scene, const MapScene::Unit &unit)
{
    std::vector<uint32_t> units;
    MapScene::Unit commander;

    if ( !unit.HasParent || !scene.Find(unit.Parent, commander) )
        return units;

    scene.ForEachKid(commander, [&](const MapScene::Unit &kid)
    {
        if ( kid.Id == unit.Id || !units.empty() )
            units.push_back(kid.Id);
    });

    return units;
}

static void IssueReorganize(NC_STACK_ypaworld *yw, uint32_t unit, int mode, uint32_t other)
{
    World::PlayerOrder order;
    order.Type = World::PlayerOrder::REORGANIZE;
    order.Unit = unit;
    order.Value = mode;
    order.Target = other;
    yw->IssueOrder(order);
}

void SquadManagerDropUnit(NC_STACK_ypaworld *yw, TClickBoxInf *winpt)
{
    const MapScene &scene = yw->UiScene();
    const MapScene::Unit dragged = scene.Fresh(squadron_manager.dragUnit);

    if ( winpt->selected_btn != &squadron_manager
            || winpt->selected_btnID < 8
            || squadron_manager.numEntries <= (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries)
            || squadron_manager.squads[ winpt->selected_btnID - 8 ].Id == 0
            || squadron_manager.squads[ winpt->selected_btnID - 8 ].Type == BACT_TYPES_ROBO )
    {
        // A squad of their own: with the right button, the unit and those
        // after it in its squad, the first of them leading
        if ( squadron_manager.dragFlags & 2 )
        {
            if ( !dragged.Commander )
            {
                std::vector<uint32_t> units = SquadUnitsFrom(scene, dragged);

                for ( size_t i = 0; i < units.size(); i++ )
                {
                    if ( i == 0 )
                        IssueReorganize(yw, units[0], 3, 0);
                    else
                        IssueReorganize(yw, units[0], 4, units[i]);
                }
            }
        }
        else
        {
            IssueReorganize(yw, dragged.Id, 3, 0);
        }

        // Its squad now
        yw->_activeCmdrID = scene.Fresh(dragged).CommandId;
        SquadManagerUpdateRows(yw);
        yw->UpdateCommanderList();
    }
    else
    {
        MapScene::Unit targetSquad = squadron_manager.squads[ winpt->selected_btnID - 8 ];
        bool sameSquad;

        if ( dragged.Commander )
            sameSquad = targetSquad.Id == dragged.Id;
        else
            sameSquad = dragged.HasParent && targetSquad.Id == dragged.Parent;

        if ( !sameSquad )
        {
            if ( squadron_manager.dragFlags & 2 )
            {
                if ( !dragged.Commander )
                {
                    for ( uint32_t unit : SquadUnitsFrom(scene, dragged) )
                        IssueReorganize(yw, unit, 1, targetSquad.Id);
                }
                else
                {
                    IssueReorganize(yw, dragged.Id, 1, targetSquad.Id);
                }
            }
            else
            {
                IssueReorganize(yw, targetSquad.Id, 4, dragged.Id);
            }

            yw->_activeCmdrID = scene.Fresh(targetSquad).CommandId;
            SquadManagerUpdateRows(yw);
            yw->UpdateCommanderList();
        }
    }
}

void NC_STACK_ypaworld::SquadManagerShowStatusTooltip(const MapScene::Unit &unit)
{
    if ( unit.Status == BACT_STATUS_NORMAL )
    {
        if ( unit.StatusFlags & BACT_STFLAG_ESCAPE )
            SetShowingTooltip(Locale::TIP_FINDER_ESCAPE);
        else if ( unit.SecondaryType )
            SetShowingTooltip(Locale::TIP_FINDER_FIGHTING);
        else if ( unit.PrimaryType )
            SetShowingTooltip(Locale::TIP_FINDER_MOVE);
    }
    else if (unit.Status == BACT_STATUS_IDLE)
        SetShowingTooltip(Locale::TIP_FINDER_WAITORDER);
}

bool NC_STACK_ypaworld::SquadManagerPickUnit(int sqid, int btnX, MapScene::Unit &out)
{
    const MapScene::Unit &cmdr = squadron_manager.squads[sqid];

    if ( !cmdr.Id )
    {
        squadron_manager.pickKidIdx = -1;
        squadron_manager.pickCmdrIdx = -1;
        return false;
    }

    // The top row: the host station under the mouse
    if ( cmdr.Type == BACT_TYPES_ROBO )
    {
        squadron_manager.pickKidIdx = -1;
        squadron_manager.pickCmdrIdx = -1;

        std::string icons = SquadManagerHostStationIcons(this);
        int x = _fontBorderW;

        for ( size_t i = 0; i < icons.size(); i++ )
        {
            x += _guiTiles[28]->map[(uint8_t)icons[i]].w;

            if ( btnX < x )
            {
                out = squadron_manager.hostStations[i];
                return true;
            }
        }

        return false;
    }

    squadron_manager.pickKidIdx = -1;
    squadron_manager.pickCmdrIdx = squadron_manager.firstShownEntries + sqid - squadron_manager.hostRows;

    if ( btnX > squadron_manager.cmdrIconX && btnX < squadron_manager.cmdrIconX + squadron_manager.iconWidth )
    {
        out = cmdr;
        return true;
    }

    int kidOffsetX = btnX - squadron_manager.kidIconsX;
    if ( kidOffsetX < 0 )
    {
        squadron_manager.pickKidIdx = -1;
        squadron_manager.pickCmdrIdx = -1;
        return false;
    }

    squadron_manager.pickKidIdx = kidOffsetX / squadron_manager.iconWidth;

    int kidsLeft = squadron_manager.pickKidIdx;
    bool found = false;

    UiScene().ForEachKid(cmdr, [&](const MapScene::Unit &kid)
    {
        if ( !found && kid.Status != BACT_STATUS_DEAD && kid.Status != BACT_STATUS_CREATE && kid.Status != BACT_STATUS_BEAM )
        {
            kidsLeft--;
            if (kidsLeft == -1)
            {
                out = kid;
                found = true;
            }
        }
    });

    if ( found )
        return true;

    squadron_manager.pickKidIdx = -1;
    squadron_manager.pickCmdrIdx = -1;
    return false;
}

int NC_STACK_ypaworld::SquadManagerBeginDrag(TClickBoxInf *winpt)
{
    if ( winpt->selected_btnID < 8 )
        return 0;
    if ( bzda.activeMode & 8 )
        return 0;
    if ( (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries) >= squadron_manager.numEntries )
        return 0;

    MapScene::Unit pickedUnit;

    if ( !SquadManagerPickUnit(winpt->selected_btnID - 8, winpt->move.BtnPos.x, pickedUnit) )
        return 0;

    if ( robo_map.IsOpen() )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
        {
            robo_map.viewCenterX = pickedUnit.Position.x;
            robo_map.lockViewer = 0;
            robo_map.viewCenterZ = pickedUnit.Position.z;

            RoboMapClampView(this, robo_map.hScrollLength - robo_map.borderLeft, robo_map.vScrollLength);
        }
    }

    // Host stations stay where they are
    if ( pickedUnit.Type == BACT_TYPES_ROBO )
        return 0;

    squadron_manager.dragCmdrIdx = squadron_manager.pickCmdrIdx;
    squadron_manager.dragKidIdx = squadron_manager.pickKidIdx;
    squadron_manager.dragStartX = winpt->move.ScreenPos.x;
    squadron_manager.dragUnit = pickedUnit;

    if ( pickedUnit.Commander )
    {
        squadron_manager.dragOffsetX = -(winpt->move.BtnPos.x - squadron_manager.cmdrIconX);
    }
    else
    {
        int kidOffsetX = winpt->move.BtnPos.x - squadron_manager.kidIconsX;
        squadron_manager.dragOffsetX = -(kidOffsetX % squadron_manager.iconWidth);
    }

    squadron_manager.dragOffsetY = -winpt->move.BtnPos.y;
    squadron_manager.dragStartY = winpt->move.ScreenPos.y;
    return 1;
}

void NC_STACK_ypaworld::SquadManager_InputHandle(TInputState *inpt)
{
    if ( squadron_manager.flags & (GuiBase::FLAG_CLOSED | GuiBase::FLAG_ICONIFED) )
    {
        squadron_manager.dragFlags &= 0xFFFFFFFE;
        squadron_manager.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;
        SquadManagerUpdateRows(this);

        if ( squadron_manager.dragFlags & 1 )
        {
            if ( SquadManagerIsDragValid() && (winpt->flag & (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD)) != (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD) )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD) )
                {
                    squadron_manager.dragIconX = winpt->move.ScreenPos.x + squadron_manager.dragOffsetX;
                    squadron_manager.dragIconY = winpt->move.ScreenPos.y + squadron_manager.dragOffsetY;
                }
                else
                {
                    SquadManagerDropUnit(this, winpt);
                    squadron_manager.dragFlags &= 0xFFFFFFFC;
                }
            }
            else
            {
                squadron_manager.dragFlags &= 0xFFFFFFFC;
            }
        }
        else if ( inpt->ClickInf.selected_btn == &squadron_manager )
        {
            if ( inpt->ClickInf.selected_btnID == 7 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    ConfirmOpenHelp(Locale::Text::Help(Locale::HELP_FINDERWND));
                }
                SetShowingTooltip(Locale::TIP_ONLINEHELP);
            }
            if ( winpt->selected_btnID == 6 && this->_activeCmdrRemapIndex != -1 && !(bzda.activeMode & 0x20) )
            {
                int aggrLevel = (winpt->move.BtnPos.x - squadron_manager.aggrIconsX) / _guiTiles[24]->map[49].w;

                if ( aggrLevel >= 0 && aggrLevel <= 4 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                    {
                        World::PlayerOrder order;
                        order.Type = World::PlayerOrder::AGGRESSION;
                        order.Unit = _cmdrsRemap[_activeCmdrRemapIndex].Id;
                        order.Value = 25 * aggrLevel;
                        IssueOrder(order);

                        if ( _GameShell )
                            SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                    }
                }

                switch ( aggrLevel )
                {
                case 0:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_0, 38);
                    break;

                case 1:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_1, 39);
                    break;

                case 2:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_2, 40);
                    break;

                case 3:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_3, 41);
                    break;

                case 4:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_4, 42);
                    break;

                default:
                    break;
                }

                if ( winpt->move.BtnPos.x >= squadron_manager.unitCountX )
                    SetShowingTooltip(Locale::TIP_COUNTINSQUAD);
            }

            if ( winpt->flag & (TClickBoxInf::FLAG_RM_DOWN | TClickBoxInf::FLAG_LM_DOWN) )
            {
                if ( SquadManagerBeginDrag(winpt) )
                {
                    squadron_manager.dragFlags &= 0xFFFFFFFC;
                    squadron_manager.dragFlags |= 1;

                    if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
                        squadron_manager.dragFlags |= 3;

                    squadron_manager.dragIconX = winpt->move.ScreenPos.x + squadron_manager.dragOffsetX;
                    squadron_manager.dragIconY = winpt->move.ScreenPos.y + squadron_manager.dragOffsetY;
                }
            }
            if ( squadron_manager.mouseItem != -1 )
            {
                int row = squadron_manager.mouseItem - squadron_manager.firstShownEntries;

                if ( row >= 0 && row < squadron_manager.maxShownEntries )
                {
                    const MapScene::Unit &squad = squadron_manager.squads[row];
                    if ( squad.Id )
                    {
                        if ( winpt->move.BtnPos.x < squadron_manager.cmdrIconX )
                            SquadManagerShowStatusTooltip(squad);
                    }
                }
            }
        }

        squadron_manager.InputHandle(this, inpt);
        squadron_manager.Formate(this);
        SquadManagerLayoutItems(this, inpt);
    }
}


void RoboMapUpdateSelectionRect()
{
    ButtonBox &pMapInter = robo_map.buttons[17];

    int offsetX = -(pMapInter.x + robo_map.x + (pMapInter.w / 2));
    int offsetY = -(pMapInter.y + robo_map.y + (pMapInter.h / 2));
    mapSelWorldMinX = (float)(offsetX + mapSelStartX) * robo_map.unitsPerPixelX + robo_map.viewCenterX;
    mapSelWorldMaxX = (float)(offsetX + mapSelEndX) * robo_map.unitsPerPixelX + robo_map.viewCenterX;
    mapSelWorldMinZ = robo_map.viewCenterZ - (float)(offsetY + mapSelStartY) * robo_map.unitsPerPixelY;
    mapSelWorldMaxZ = robo_map.viewCenterZ - (float)(offsetY + mapSelEndY) * robo_map.unitsPerPixelY;

    if ( mapSelWorldMinX > mapSelWorldMaxX )
    {
        float tmp = mapSelWorldMinX;
        mapSelWorldMinX = mapSelWorldMaxX;
        mapSelWorldMaxX = tmp;
    }

    if ( mapSelWorldMinZ > mapSelWorldMaxZ )
    {
        float tmp = mapSelWorldMinZ;
        mapSelWorldMinZ = mapSelWorldMaxZ;
        mapSelWorldMaxZ = tmp;
    }
}

void RoboMapMakeSquadFromBox(NC_STACK_ypaworld *yw)
{
    int hostSelected = 0;

    int minCellX = mapSelWorldMinX / World::CVSectorLength;
    int minCellY = -mapSelWorldMaxZ / World::CVSectorLength;
    int maxCellX = mapSelWorldMaxX / World::CVSectorLength;
    int maxCellY = -mapSelWorldMinZ / World::CVSectorLength;

    if ( minCellX >= yw->_mapSize.x )
        minCellX = yw->_mapSize.x - 1;
    else if ( minCellX < 1 )
        minCellX = 1;

    if ( maxCellX >= yw->_mapSize.x )
        maxCellX = yw->_mapSize.x - 1;
    else if ( maxCellX < 1 )
        maxCellX = 1;

    if ( minCellY >= yw->_mapSize.y )
        minCellY = yw->_mapSize.y - 1;
    else if ( minCellY < 1 )
        minCellY = 1;

    if ( maxCellY >= yw->_mapSize.y )
        maxCellY = yw->_mapSize.y - 1;
    else if ( maxCellY < 1 )
        maxCellY = 1;

    const MapScene &scene = yw->UiScene();
    uint8_t player = scene.PlayerOwner();
    MapScene::Unit newChief;

    // The first unit of the player's in the box leads a new squad of them
    // all; the host station isn't one of them
    for (int i = minCellY; i <= maxCellY; i++)
    {
        for (int j = minCellX; j <= maxCellX; j++)
        {
            if ( robo_map.MapViewMask & scene.ViewMask({j, i}) )
            {
                scene.ForEachUnitIn({j, i}, [&](const MapScene::Unit &unit)
                {
                    if ( unit.Owner == player )
                    {
                        if ( unit.Position.x > mapSelWorldMinX && unit.Position.z > mapSelWorldMinZ && unit.Position.x < mapSelWorldMaxX && unit.Position.z < mapSelWorldMaxZ )
                        {
                            if ( unit.Type != BACT_TYPES_MISSLE && unit.Type != BACT_TYPES_GUN )
                            {
                                if ( unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_BEAM && unit.Status != BACT_STATUS_DEAD )
                                {

                                    if ( newChief.Id )
                                    {
                                        if ( unit.Type != BACT_TYPES_ROBO )
                                        {
                                            IssueReorganize(yw, newChief.Id, 4, unit.Id);

                                            hostSelected = 0;
                                        }
                                    }
                                    else
                                    {
                                        if ( unit.Type == BACT_TYPES_ROBO )
                                        {
                                            if ( unit.Id == scene.UserRoboId() )
                                                hostSelected = 1;
                                        }
                                        else
                                        {
                                            newChief = unit;

                                            IssueReorganize(yw, newChief.Id, 3, 0);

                                            hostSelected = 0;
                                        }
                                    }


                                }
                            }
                        }
                    }
                });
            }

        }
    }

    if ( hostSelected )
        bzda.activeMode = bzda.enabledModes & 0x20;

    if ( newChief.Id )
        yw->_activeCmdrID = scene.Fresh(newChief).CommandId;

    yw->UpdateCommanderList();
}

void  RoboMap_InputHandle(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( robo_map.IsClosed() )
    {
        robo_map.stateFlags &= 0xFFFFFDE8;
    }
    else
    {
        robo_map.MapViewMask = cellArea::ViewMask( robo_map.Scene->PlayerOwner() );

        TClickBoxInf *winpt = &inpt->ClickInf;

        // Watching a replay, the map only shows: it doesn't select or help
        bool interactive = robo_map.Scene->Interactive();

        if ( yw->_showDebugMode )
            robo_map.MapViewMask = -1;

        switch ( inpt->HotKeyID )
        {
        case 10:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 3;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 11:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 4;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 12:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 5;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 14:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 6;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 16:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 7;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            break;

        case 17:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 8;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            break;

        case 18:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 9;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            break;

        default:
            break;
        }

        if ( robo_map.stateFlags & 1 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                int newW = winpt->move.ScreenPos.x + robo_map.resizeOffsetX;
                int newH = winpt->move.ScreenPos.y + robo_map.resizeOffsetY;

                if ( newW < robo_map.minWidth )
                    newW = robo_map.minWidth;
                else if ( newW > robo_map.maxWidth )
                    newW = robo_map.maxWidth;

                if ( newH < robo_map.minHeight )
                    newH = robo_map.minHeight;
                else if ( newH > robo_map.maxHeight )
                    newH = robo_map.maxHeight;

                if ( newW + robo_map.x >= yw->_screenSize.x )
                    newW = yw->_screenSize.x - robo_map.x;

                if ( robo_map.y + newH >= yw->_screenSize.y - yw->_downScreenBorder )
                    newH = yw->_screenSize.y - yw->_downScreenBorder - robo_map.y;

                robo_map.h = newH;
                robo_map.w = newW;
            }
            else
            {
                robo_map.stateFlags &= 0xFFFFFFFE;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.stateFlags & 2 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                robo_map.lockViewer = 0;
                robo_map.viewCenterX = (yw->_mapSize.x * World::CVSectorLength) / (float)robo_map.hScrollLength * (float)(winpt->move.ScreenPos.x - robo_map.dragStartX ) + robo_map.dragStartCenterX;
            }
            else
            {
                robo_map.stateFlags &= 0xFFFFFFFD;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.stateFlags & 4 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                robo_map.lockViewer = 0;
                robo_map.viewCenterZ = robo_map.dragStartCenterZ - (yw->_mapSize.y * World::CVSectorLength) / (float)robo_map.vScrollLength * (float)(winpt->move.ScreenPos.y - robo_map.dragStartY);
            }
            else
            {
                robo_map.stateFlags &= 0xFFFFFFFB;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.stateFlags & 0x10 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_RM_HOLD )
            {
                robo_map.lockViewer = 0;
                robo_map.viewCenterX = robo_map.dragStartCenterX + (float)(robo_map.dragStartX - winpt->move.ScreenPos.x) * robo_map.unitsPerPixelX;
                robo_map.viewCenterZ = robo_map.dragStartCenterZ - (float)(robo_map.dragStartY - winpt->move.ScreenPos.y) * robo_map.unitsPerPixelY;
            }
            else
            {
                int dx = abs(robo_map.dragStartX - winpt->move.ScreenPos.x);
                int dy = abs(robo_map.dragStartY - winpt->move.ScreenPos.y);

                if ( dx <= 1 && dy <= 1 && interactive )
                {
                    yw->_activeCmdrRemapIndex = -1;
                    yw->_activeCmdrID = 0;

                    if ( bzda.activeMode & 0x28 )
                        bzda.activeMode = bzda.enabledModes & 1;
                }
                robo_map.stateFlags &= 0xFFFFFFEF;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.stateFlags & 0x200 )
        {
            mapSelEndX = winpt->move.ScreenPos.x;
            mapSelEndY = winpt->move.ScreenPos.y;

            RoboMapUpdateSelectionRect();

            if ( !(winpt->flag & TClickBoxInf::FLAG_LM_HOLD) )
            {
                if ( abs(mapSelEndX - mapSelStartX) <= 5 || abs(mapSelEndY - mapSelStartY) <= 5 )
                {
                    robo_map.stateFlags &= 0xFFFFFDFF;
                    winpt->flag |= TClickBoxInf::FLAG_LM_DOWN;
                }
                else
                {
                    bzda.activeMode = 1;
                    robo_map.stateFlags &= 0xFFFFFDFF;
                    RoboMapMakeSquadFromBox(yw);
                }
            }
        }
        else if ( winpt->selected_btn == &robo_map )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
            {
                if ( winpt->selected_btnID == 17 )
                {
                    robo_map.dragStartCenterX = robo_map.viewCenterX;
                    robo_map.dragStartX = winpt->move.ScreenPos.x;
                    robo_map.dragStartY = winpt->move.ScreenPos.y;
                    robo_map.dragStartCenterZ = robo_map.viewCenterZ;
                    robo_map.stateFlags |= 0x10;
                    yw->_guiDragDefaultMouse = true;
                }
            }

            if ( (winpt->flag & TClickBoxInf::FLAG_LM_DOWN) && interactive )
            {
                if ( winpt->selected_btnID == 17 )
                {
                    robo_map.stateFlags |= 0x200;
                    mapSelStartX = winpt->move.ScreenPos.x;
                    mapSelStartY = winpt->move.ScreenPos.y;
                    mapSelEndX = mapSelStartX;
                    mapSelEndY = mapSelStartY;

                    RoboMapUpdateSelectionRect();

                    winpt->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
                }
            }

            if ( winpt->selected_btnID > 1 && winpt->selected_btnID <= 9 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( yw->_GameShell )
                        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
                }
            }

            if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
            {
                switch ( winpt->selected_btnID )
                {
                case 16:
                    robo_map.resizeOffsetX = robo_map.w - winpt->ldw_pos.ScreenPos.x;
                    robo_map.resizeOffsetY = robo_map.h - winpt->ldw_pos.ScreenPos.y;

                    robo_map.stateFlags |= 1;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 14:
                    robo_map.dragStartX = winpt->ldw_pos.ScreenPos.x;
                    robo_map.dragStartCenterX = robo_map.viewCenterX;
                    robo_map.stateFlags |= 2;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 11:
                    robo_map.dragStartY = winpt->ldw_pos.ScreenPos.y;
                    robo_map.dragStartCenterZ = robo_map.viewCenterZ;
                    robo_map.stateFlags |= 4;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 3:
                    if ( robo_map.layerFlags & 1 )
                        robo_map.layerFlags &= 0xFE;
                    else
                        robo_map.layerFlags |= 1;

                    RoboMapZoom(yw, 0);
                    break;

                case 4:
                    if ( robo_map.layerFlags & 2 )
                        robo_map.layerFlags &= 0xFD;
                    else
                        robo_map.layerFlags |= 2;

                    RoboMapZoom(yw, 0);
                    break;

                case 5:
                    if ( robo_map.layerFlags & 4 )
                        robo_map.layerFlags &= 0xFB;
                    else
                        robo_map.layerFlags |= 4;

                    RoboMapZoom(yw, 0);
                    break;

                case 6:
                    robo_map.lockViewer = robo_map.lockViewer != 1;
                    break;

                default:
                    break;
                }
            }

            switch ( winpt->selected_btnID )
            {
            case 13:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.dragStartCenterX = robo_map.viewCenterX;
                    robo_map.lockViewer = 0;
                    robo_map.viewCenterX = yw->_mapSize.x * World::CVSectorLength / (-1.0 * (float)robo_map.hScrollLength) + robo_map.viewCenterX;
                }
                break;

            case 15:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.dragStartCenterX = robo_map.viewCenterX;
                    robo_map.lockViewer = 0;
                    robo_map.viewCenterX = yw->_mapSize.x * World::CVSectorLength / (float)robo_map.hScrollLength + robo_map.viewCenterX;
                }
                break;

            case 10:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.dragStartCenterZ = robo_map.viewCenterZ;
                    robo_map.lockViewer = 0;
                    robo_map.viewCenterZ = robo_map.viewCenterZ - yw->_mapSize.y * World::CVSectorLength / (-1.0 * (float)robo_map.vScrollLength);
                }
                break;

            case 12:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.dragStartCenterZ = robo_map.viewCenterZ;
                    robo_map.lockViewer = 0;
                    robo_map.viewCenterZ = robo_map.viewCenterZ - yw->_mapSize.y * World::CVSectorLength / (float)robo_map.vScrollLength;
                }
                break;

            case 9:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    robo_map.stateFlags |= 0x20;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    int tmp1, tmp2, tmp3, tmp4;
                    tmp1 = robo_map.altX;
                    tmp2 = robo_map.altY;
                    tmp3 = robo_map.altW;
                    tmp4 = robo_map.altH;

                    robo_map.altX = robo_map.x;
                    robo_map.altY = robo_map.y;
                    robo_map.altW = robo_map.w;
                    robo_map.altH = robo_map.h;

                    robo_map.x = tmp1;
                    robo_map.y = tmp2;
                    robo_map.w = tmp3;
                    robo_map.h = tmp4;
                }
                break;

            case 2:
                if ( !interactive )
                    break;

                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    robo_map.flags |= GuiBase::FLAG_HELP_DOWN;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    robo_map.flags &= ~GuiBase::FLAG_HELP_DOWN;
                    yw->ConfirmOpenHelp(Locale::Text::Help(Locale::HELP_MAPWND)); //MAKE ME
                }

                yw->SetShowingTooltip(Locale::TIP_ONLINEHELP);
                break;

            case 7:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    robo_map.stateFlags |= 0x40;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    RoboMapZoom(yw, 1);

                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    robo_map.stateFlags |= 0x80;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    RoboMapZoom(yw, 2);

                break;

            default:
                break;
            }

            switch ( winpt->selected_btnID )
            {
            case 3:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_DETAIL, 10);
                break;

            case 4:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_OWNERS, 11);
                break;

            case 5:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_EINFO, 12);
                break;

            case 6:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_LOCKVIEWER, 14);
                break;

            case 7:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_ZOOMIN, 16);
                break;

            case 8:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_ZOOMOUT, 17);
                break;

            case 9:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_SIZE, 18);
                break;

            default:
                break;
            }
        }
    }
}

void InfoLogLayoutMessage(NC_STACK_ypaworld *yw, CmdStream *cur, int id)
{
    int msgIdx = id + info_log.oldest_msg_idx + info_log.firstShownEntries;

    if ( msgIdx >= 64 )
        msgIdx -= 64;
    else if ( msgIdx < 0 )
        msgIdx += 64;

    FontUA::store_u8(cur, 123);

    FontUA::FormateClippedText(yw->_guiTiles[0], cur, info_log.msgs[msgIdx].txt, info_log.entryWidth - 2 * yw->_fontBorderW, 32);

    FontUA::store_u8(cur, 125);
    FontUA::next_line(cur);
}

void InfoLogLayoutItems(NC_STACK_ypaworld *yw)
{
    info_log.itemBlock.clear();
    
    info_log.ItemsPreLayout(yw, &info_log.itemBlock, 0, "{ }");

    for (int i = 0; i < info_log.shownEntries; i++ )
        InfoLogLayoutMessage(yw, &info_log.itemBlock, i);

    info_log.ItemsPostLayout(yw, &info_log.itemBlock, 0, "xyz");

    FontUA::set_end(&info_log.itemBlock);
}

void InfoLogHandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( info_log.flags & (GuiBase::FLAG_CLOSED | GuiBase::FLAG_ICONIFED) )
    {
        info_log.InputHandle(yw, inpt);
    }
    else
    {
        info_log.InputHandle(yw, inpt);
        info_log.Formate(yw);
        InfoLogLayoutItems(yw);
    }
}


GuiBase * GetConfirmDialogOwner()
{
    return confirmDialogOwner;
}

void NC_STACK_ypaworld::NetSendExitMsg(uint8_t normExit)
{
    uamessage_exit exMsg;
    exMsg.msgID = UAMSG_EXIT;
    exMsg.owner = _GameShell->netPlayerOwner;
    exMsg.norm = normExit;

    NetBroadcastMessage(&exMsg, sizeof(exMsg), true);

    _GameShell->sentAQ = false;
}

void LayoutCenteredTextField(TileMap *, CmdStream *cur, const std::string &txt, int width, uint8_t fillChar)
{
    if ( width > 0 )
    {
        FontUA::copy_position(cur);

        int remaining = width;
        while (remaining > 0)
        {
            if ( remaining <= 255 )
                FontUA::op10(cur, remaining);
            else
                FontUA::op10(cur, 255);

            FontUA::store_u8(cur, fillChar);

            remaining -= 255;
        }

        FontUA::add_txt(cur, width, 4, txt);
    }
}


void ExitMenuLayoutButton(NC_STACK_ypaworld *yw, CmdStream *cur, int disabled, int pressed, const std::string &label)
{
    std::string text = label;

    FontUA::set_yheight(cur, exitMenuButtonPadY);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, exitMenuMarginX + exitMenuButtonWidth);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int tileset;

    if ( disabled )
        tileset = 4;
    else if ( pressed )
        tileset = 8;
    else
        tileset = 3;

    FontUA::select_tileset(cur, tileset);
    FontUA::store_u8(cur, 98);

    if ( disabled )
        text = " ";

    if ( pressed )
    {
        FontUA::set_flag(cur, 0x10);
    }

    LayoutCenteredTextField(yw->_guiTiles[tileset], cur, text, exitMenuButtonWidth - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::set_yheight(cur, exitMenuButtonPadY);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, exitMenuMarginX + exitMenuButtonWidth);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}



void ExitMenuLayoutItems(NC_STACK_ypaworld *yw, int pressedFlags, int disabledFlags)
{
    exit_menu.itemBlock.clear();
    exit_menu.ItemsPreLayout(yw, &exit_menu.itemBlock, 0, "{ }");

    FontUA::set_txtColor(&exit_menu.itemBlock, yw->_iniColors[68].r, yw->_iniColors[68].g, yw->_iniColors[68].b);

    ExitMenuLayoutButton(yw, &exit_menu.itemBlock, disabledFlags & 0x100, pressedFlags & 0x100, Locale::Text::Common(Locale::CMN_EXITMISN));
    ExitMenuLayoutButton(yw, &exit_menu.itemBlock, disabledFlags & 0x200, pressedFlags & 0x200, Locale::Text::Common(Locale::CMN_SAVE));
    ExitMenuLayoutButton(yw, &exit_menu.itemBlock, disabledFlags & 0x400, pressedFlags & 0x400, Locale::Text::Common(Locale::CMN_LOAD));
    ExitMenuLayoutButton(yw, &exit_menu.itemBlock, disabledFlags & 0x800, pressedFlags & 0x800, Locale::Text::Common(Locale::CMN_RESTART));
    ExitMenuLayoutButton(yw, &exit_menu.itemBlock, disabledFlags & 0x1000, pressedFlags & 0x1000, Locale::Text::Common(Locale::CMN_CONTINUE));

    exit_menu.ItemsPostLayout(yw, &exit_menu.itemBlock, 0, "xyz");

    FontUA::set_end(&exit_menu.itemBlock);
}


void NC_STACK_ypaworld::ExitMenuHandleInput(TInputState *inpt)
{
    if ( exit_menu.IsClosed() )
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PLAYING )
        {
            if ( _levelInfo.State == TLevelInfo::STATE_SAVE )
            {
                TInfoMessage msg;
                msg.unit = 0;
                msg.Priority = 10;
                msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_GAMESAVED);
                msg.MsgID = 0;

                ShowMessage(&msg);
            }

            _levelInfo.State = TLevelInfo::STATE_PLAYING;
        }
    }
    else
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PAUSED )
        {
            _levelInfo.State = TLevelInfo::STATE_PAUSED;

            if ( _GameShell )
            {
                _hasSGMSave = UserData::HasSaveGame(_GameShell->UserName);

                // Restart starts the level's recording again
                _canRestart = _recorder.IsRecording();

                // Started from the command line: nothing to load, and
                // Restart starts the level the same way again
                if ( _GameShell->DirectStart )
                {
                    _hasSGMSave = false;
                    _canRestart = true;
                }
            }
        }
    }

    if ( GetConfirmDialogOwner() == &exit_menu )
    {
        int confirmState = ConfirmDialogGetState();

        if ( confirmState == 1 )
        {
            switch ( exitMenuPendingAction )
            {
            case 8:
                _levelInfo.State = TLevelInfo::STATE_ABORTED;

                if ( _isNetGame )
                    NetSendExitMsg(1);

                break;

            case 9:
                _levelInfo.State = TLevelInfo::STATE_SAVE;
                break;

            case 10:
                _levelInfo.State = TLevelInfo::STATE_LOAD;
                break;

            case 11:
                _levelInfo.State = TLevelInfo::STATE_RESTART;
                break;

            case 13:
                _helpURL = _helpSavedURL;
                _helpSavedURL.clear();
                break;

            default:
                break;
            }

            confirmDialogState = 0;
        }
        else if ( confirmState == 2 )
        {
            confirmDialogState = 0;
        }
    }

    if ( exit_menu.IsClosed() )
    {
        exit_menu.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int pressedFlags = 0;
        int disabledFlags = 0;

        if ( !_hasSGMSave )
            disabledFlags = 1024;

        if ( !_canRestart )
            disabledFlags |= 0x800;

        if ( UserUnitOf(this).Status == BACT_STATUS_DEAD || (_GameShell && _GameShell->DirectStart) )
            disabledFlags |= 0x200;

        if ( _isNetGame )
            disabledFlags |= 0xE00;

        if ( inpt->ClickInf.selected_btn == &exit_menu )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            switch ( inpt->ClickInf.selected_btnID )
            {
            case 7:
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    ConfirmOpenHelp(Locale::Text::Help(Locale::HELP_GAMEMENU));

                SetShowingTooltip(Locale::TIP_ONLINEHELP);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    pressedFlags |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    exitMenuPendingAction = 8;

                    if ( ConfirmDialogGetState() != 3 )
                        GuiWinClose( &exit_menu );

                    OpenConfirmDialog( Locale::Text::Advanced(Locale::ADV_RLYEXIT) , &exit_menu);
                }
                break;

            case 9:
                if ( !(disabledFlags & 0x200) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        pressedFlags |= 0x200;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        exitMenuPendingAction = 9;

                        if ( ConfirmDialogGetState() != 3 )
                            GuiWinClose( &exit_menu );

                        OpenConfirmDialog( Locale::Text::Advanced(Locale::ADV_RLYSAVE) , &exit_menu);
                    }
                }
                break;

            case 10:
                if ( !(disabledFlags & 0x400) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        pressedFlags |= 0x400;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        exitMenuPendingAction = 10;

                        if ( ConfirmDialogGetState() != 3 )
                            GuiWinClose( &exit_menu );

                        OpenConfirmDialog( Locale::Text::Advanced(Locale::ADV_RLYLOAD) , &exit_menu);
                    }
                }
                break;

            case 11:
                if ( !(disabledFlags & 0x800) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        pressedFlags |= 0x800;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        exitMenuPendingAction = 11;

                        if ( ConfirmDialogGetState() != 3 )
                            GuiWinClose( &exit_menu );

                        OpenConfirmDialog(Locale::Text::Advanced(Locale::ADV_RLYRESTART) , &exit_menu);
                    }
                }
                break;

            case 12:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    pressedFlags |= 0x1000;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    GuiWinClose( &exit_menu );

                break;

            default:
                break;
            }
        }
        exit_menu.InputHandle(this, inpt);
        exit_menu.Formate(this);

        ExitMenuLayoutItems(this, pressedFlags, disabledFlags);
    }
}




void ConfirmDialogLayoutTextLine(NC_STACK_ypaworld *yw, CmdStream *cur, const std::string &text)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    LayoutCenteredTextField(yw->_guiTiles[0], cur, text, lstvw2.entryWidth - 2 * confirmDialogMarginX, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void ConfirmDialogLayoutButtons(NC_STACK_ypaworld *yw, CmdStream *cur, int pressedFlags)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int tileset;

    if ( pressedFlags & 0x100 )
    {
        FontUA::set_flag(cur, 0x10);
        tileset = 8;
    }
    else
    {
        tileset = 3;
    }

    FontUA::select_tileset(cur, tileset);

    FontUA::store_u8(cur, 98);

    LayoutCenteredTextField(yw->_guiTiles[tileset], cur, Locale::Text::Common(Locale::CMN_OK), confirmButtonWidth - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::op10(cur, confirmButtonGap);

    FontUA::store_u8(cur, 32);

    if ( pressedFlags & 0x200 )
    {
        FontUA::set_flag(cur, 0x10);
        tileset = 8;
    }
    else
    {
        tileset = 3;
    }

    FontUA::select_tileset(cur, tileset);

    FontUA::store_u8(cur, 98);

    LayoutCenteredTextField(yw->_guiTiles[tileset], cur, Locale::Text::Common(Locale::CMN_CANCEL), confirmButtonWidth - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void NC_STACK_ypaworld::ConfirmDialogLayoutItems(int pressedFlags)
{
    lstvw2.itemBlock.clear();
    lstvw2.ItemsPreLayout(this, &lstvw2.itemBlock, 0, "{ }");

    FontUA::set_txtColor(&lstvw2.itemBlock, _iniColors[60].r, _iniColors[60].g, _iniColors[60].b);

    ConfirmDialogLayoutTextLine(this, &lstvw2.itemBlock, confirmDialogText);
    ConfirmDialogLayoutTextLine(this, &lstvw2.itemBlock, " ");

    FontUA::set_txtColor(&lstvw2.itemBlock, _iniColors[68].r, _iniColors[68].g, _iniColors[68].b);

    ConfirmDialogLayoutButtons(this, &lstvw2.itemBlock, pressedFlags);
    lstvw2.ItemsPostLayout(this, &lstvw2.itemBlock, 0, "xyz");

    FontUA::set_end(&lstvw2.itemBlock);
}

void NC_STACK_ypaworld::ConfirmDialogHandleInput(TInputState *inpt)
{
    if ( lstvw2.IsClosed() )
    {
        lstvw2.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int pressedFlags = 0;

        if ( _kbdLastKeyHit == Input::KC_RETURN )
        {
            GuiWinClose( &lstvw2 );

            confirmDialogState = 1;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( _kbdLastKeyHit == Input::KC_ESCAPE )
        {
            GuiWinClose( &lstvw2 );

            confirmDialogState = 2;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( inpt->ClickInf.selected_btn == &lstvw2 )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            if ( winpt->selected_btnID == 8 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    pressedFlags = 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    confirmDialogState = 1;
                }
            }
            else if ( winpt->selected_btnID == 9 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    pressedFlags = 0x200;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    confirmDialogState = 2;
                }
            }
        }

        lstvw2.InputHandle(this, inpt);
        lstvw2.Formate(this);
        ConfirmDialogLayoutItems(pressedFlags);
    }
}


// What the windows do alike: clicked, one comes to the front; its close
// button closes or iconifies it, and its title bar drags it about
void NC_STACK_ypaworld::HandleGuiWindows(TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    for(GuiBaseList::iterator it = _guiActive.begin(); it != _guiActive.end(); it++)
    {
        GuiBase *llst = *it;

        if ( llst->flags & GuiBase::FLAG_WITH_ICON )
        {
            if ( llst->flags & GuiBase::FLAG_ICONIFED )
                llst->iconString[2] = 1;
        }

        if ( llst->flags & GuiBase::FLAG_WITH_CLOSE )
        {
            if ( !( llst->flags & (GuiBase::FLAG_ICONIFED | GuiBase::FLAG_CLOSED) ) )
                llst->flags &= ~GuiBase::FLAG_CLOSE_DOWN;
        }
    }

    if ( inpt->ClickInf.selected_btn )
    {
        GuiBase *clickedGui = (GuiBase *)inpt->ClickInf.selected_btn->pobject;
        if ( clickedGui )
        {
            if ( clickedGui->flags & 1 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    clickedGui->iconString[2] = 2;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    clickedGui->flags &= 0xFFFFFFFE;

                    Input::Engine.RemClickBox(&clickedGui->iconBox);
                    Input::Engine.AddClickBoxFront(clickedGui);

                    clickedGui->Attach(_guiActive);
                }
            }
            else
            {
                if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
                {
                    Input::Engine.RemClickBox(clickedGui);
                    Input::Engine.AddClickBoxFront(clickedGui);

                    clickedGui->Attach(_guiActive);
                }

                if ( clickedGui->flags & 0x10 )
                {
                    if ( winpt->selected_btnID == 0)
                    {
                        if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                            clickedGui->flags |= 0x40;

                        if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        {
                            if ( _GameShell )
                                SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                        }

                        if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        {
                            Input::Engine.RemClickBox(clickedGui);

                            if ( clickedGui->flags & 4 )
                            {
                                Input::Engine.AddClickBoxFront(&clickedGui->iconBox);

                                clickedGui->flags |= 1;
                            }
                            else
                            {
                                clickedGui->flags |= 0x20;
                            }

                            clickedGui->Attach(_guiActive);
                        }
                    }
                }

                if ( clickedGui->flags & 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                    {
                        if ( winpt->selected_btnID == 1 )
                        {
                            _guiDragDefaultMouse = true;
                            _guiDragElement = clickedGui;
                            _guiDragging = true;
                            _guiDragPos.x = winpt->ldw_pos.BoxPos.x;
                            _guiDragPos.y = winpt->ldw_pos.BoxPos.y;
                        }
                    }
                }
            }
        }
    }

    if ( _guiDragging )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
        {
            int newX = winpt->move.ScreenPos.x - _guiDragPos.x;
            int newY = winpt->move.ScreenPos.y - _guiDragPos.y;

            if ( newX >= 0 )
            {
                if ( _guiDragElement->w + newX > _screenSize.x )
                    newX = _screenSize.x - _guiDragElement->w;
            }
            else
            {
                newX = 0;
            }

            if ( newY >= _upScreenBorder )
            {
                if ( _guiDragElement->h + newY > _screenSize.y - _downScreenBorder )
                    newY = _screenSize.y - _downScreenBorder - _guiDragElement->h;
            }
            else
            {
                newY = _upScreenBorder;
            }

            _guiDragElement->y = newY;
            _guiDragElement->x = newX;

            int16_t tmp = newX - (_screenSize.x / 2);
            _guiDragElement->cmdCommands[5] = (tmp >>  8) & 0xFF ; //HACKY UPDATE!
            _guiDragElement->cmdCommands[6] = tmp & 0xFF ;

            tmp = newY - (_screenSize.y / 2); 
            _guiDragElement->cmdCommands[9] = (tmp >>  8) & 0xFF ; //HACKY UPDATE!
            _guiDragElement->cmdCommands[10] = tmp & 0xFF ;
        }
    }

    if ( _guiDragging )
    {
        if ( !(winpt->flag & TClickBoxInf::FLAG_LM_HOLD) )
        {
            _guiDragDefaultMouse = false;
            _guiDragging = false;
        }
    }
}

void NC_STACK_ypaworld::HandleGuiInput(TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( UserUnitOf(this).Type != BACT_TYPES_MISSLE )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_OK )
        {
            HandleGuiWindows(inpt);

            if ( _isNetGame )
                NetGameWindowHandleInput(this, inpt);

            ConfirmDialogHandleInput(inpt);
            ExitMenuHandleInput(inpt);
            RoboMap_InputHandle(this, inpt);
            SquadManager_InputHandle(inpt);
            InfoLogHandleInput(this, inpt);
            EnergyPanelHandleInput(this, inpt);
            _viewDistanceWindow.HandleInput(this, inpt);
        }
        BottomBarHandleInput(this, inpt);
    }
}










bool NC_STACK_ypaworld::GetLastMsgSender(MapScene::Unit &out)
{
    if ( !info_log.last_sender_gid )
        return false;

    const MapScene &scene = UiScene();

    for (const MapScene::Unit &comm : _cmdrsRemap)
    {
        bool found = false;
        
        if ( info_log.last_sender_gid == comm.Id )
        {
            out = comm;
            found = true;
        }
        else
        {
            scene.ForEachKid(comm, [&](const MapScene::Unit &unit)
            {
                if ( !found &&
                     unit.Status != BACT_STATUS_DEAD && 
                     unit.Status != BACT_STATUS_CREATE && 
                     unit.Status != BACT_STATUS_BEAM && 
                     unit.Id == info_log.last_sender_gid )
                {
                    out = unit;
                    found = true;
                }
            });
        }

        if (found)
            return (_timeStamp - info_log.last_msg_time) <= 10000;
    }

    return false;
}

bool CompareCommanderIds(const MapScene::Unit &lhs, const MapScene::Unit &rhs)
{
    return (lhs.CommandId & 0xFFFFFF) < (rhs.CommandId & 0xFFFFFF);
}


void NC_STACK_ypaworld::UpdateCommanderList()
{
    const MapScene &scene = UiScene();

    _cmdrsRemap.clear();
    _cmdrsRemap.reserve(128);
    // The squads of the player's faction, of all its host stations
    scene.ForEachSquad(scene.PlayerOwner(), [this](const MapScene::Unit &commander)
    {
        if (commander.Status != BACT_STATUS_DEAD && commander.Status != BACT_STATUS_BEAM && commander.Type != BACT_TYPES_GUN)
            _cmdrsRemap.push_back(commander);
    });

    std::sort(_cmdrsRemap.begin(), _cmdrsRemap.end(), CompareCommanderIds);

    if ( !_cmdrsRemap.empty() )
    {
        bool selected = false;
        if (_cmdrIdToSelect != -1)
        {
            for (size_t i = 0; i < _cmdrsRemap.size(); i++)
            {
                if (_cmdrIdToSelect == (int32_t)_cmdrsRemap[i].CommandId)
                {
                    _activeCmdrRemapIndex = i;
                    _activeCmdrID = _cmdrIdToSelect;
                    selected = true;
                    break;
                }
            }     
        }
        
        if (!selected)
        {
            if ( _activeCmdrID == 0 )
            {
                if ( bzda.activeMode & 6 )
                {
                    _activeCmdrID = _cmdrsRemap.back().CommandId;
                    _activeCmdrRemapIndex = _cmdrsRemap.size() - 1;
                }
            }
            else
            {
                _activeCmdrRemapIndex = -1;

                for (size_t i = 0; i < _cmdrsRemap.size(); i++)
                {
                    if (_activeCmdrID == _cmdrsRemap[i].CommandId)
                    {
                        _activeCmdrRemapIndex = i;
                        break;
                    }
                }                
            }
        }
    }
    else
    {
        _activeCmdrRemapIndex = -1;
    }
    
    _cmdrIdToSelect = -1;

    if ( _activeCmdrRemapIndex == -1 )
    {
        _activeCmdrKidsCount = 0;
    }
    else
    {
        _activeCmdrKidsCount = 0;
        scene.ForEachKid(_cmdrsRemap[ _activeCmdrRemapIndex ], [this](const MapScene::Unit &kid)
        {
            if (kid.Status != BACT_STATUS_DEAD && kid.Status != BACT_STATUS_BEAM && kid.Type != BACT_TYPES_GUN)
                _activeCmdrKidsCount++;
        });
    }

    MapScene::Unit sender;
    _lastMsgSender = GetLastMsgSender(sender) ? sender : MapScene::Unit();
}

void RoboMapUpdateLayout(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        RoboMapCalcScrollThumbs(yw);
        RoboMapClampView(yw, robo_map.hScrollLength - robo_map.borderLeft, robo_map.vScrollLength);
        RoboMapLayoutFrame(yw);
        RoboMapLayoutHScroll(yw);
        RoboMapLayoutVScroll(yw);
        MapRoboUpdateMapInt(yw);
    }
}

void RoboMapFollowViewer(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        vec3d viewer;

        if ( !(robo_map.stateFlags & 0x16) && robo_map.lockViewer == 1 )
        {
            if ( robo_map.Scene->ViewerPosition(viewer) )
            {
                robo_map.viewCenterX = viewer.x;
                robo_map.viewCenterZ = viewer.z;

                RoboMapClampView(yw, robo_map.hScrollLength - robo_map.borderLeft, robo_map.vScrollLength);
            }
        }
        robo_map.stateFlags &= 0xFFFFFF1F;
    }
}



int CompareVehicleProtos(const void *lhs, const void *rhs)
{
    uint8_t aa1 = *(const uint8_t *)lhs;
    uint8_t aa2 = *(const uint8_t *)rhs;

    const World::TVhclProto &lhsProto = protoSortWorld->_vhclProtos[aa1];
    const World::TVhclProto &rhsProto = protoSortWorld->_vhclProtos[aa2];

    int lhsRank, rhsRank;

    switch ( lhsProto.model_id )
    {
    case BACT_TYPES_BACT:
        lhsRank = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        lhsRank = 0;
        break;

    case BACT_TYPES_ROBO:
        lhsRank = 5;
        break;

    case BACT_TYPES_FLYER:
        lhsRank = 2;
        break;

    case BACT_TYPES_UFO:
        lhsRank = 3;
        break;

    case BACT_TYPES_GUN:
        lhsRank = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        lhsRank = 10;
        break;

    default:
        lhsRank = 0;
        break;
    }

    switch ( rhsProto.model_id )
    {
    case BACT_TYPES_BACT:
        rhsRank = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        rhsRank = 0;
        break;

    case BACT_TYPES_ROBO:
        rhsRank = 5;
        break;

    case BACT_TYPES_FLYER:
        rhsRank = 2;
        break;

    case BACT_TYPES_UFO:
        rhsRank = 3;
        break;

    case BACT_TYPES_GUN:
        rhsRank = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        rhsRank = 10;
        break;

    default:
        rhsRank = 0;
        break;
    }

    if ( lhsRank < rhsRank )
        return -1;
    else if ( lhsRank > rhsRank)
        return 1;
    else if ( lhsRank == rhsRank )
    {
        if ( lhsProto.energy > rhsProto.energy )
            return 1;
        else if (lhsProto.energy < rhsProto.energy)
            return -1;
    }

    return 0;
}

void UpdateVehicleList(NC_STACK_ypaworld *yw)
{
    MapScene::Unit robo;
    int ownerMask = 0;

    if ( yw->UiScene().UserRobo(robo) )
        ownerMask = 1 << robo.Owner;

    int count = 0;
    int i = 0;
    for (World::TVhclProto &proto : yw->_vhclProtos)
    {
        if ( ownerMask & proto.disable_enable_bitmask )
        {
            bzda.vhclProtoIds[count] = i;
            count++;
        }
        i++;
    }

    bzda.vhclCount = count;

    protoSortWorld = yw;

    qsort(bzda.vhclProtoIds, count, 1, CompareVehicleProtos);

    if ( !count )
    {
        bzda.selVhclIdx = -1;
    }
    else
    {
        if ( count == 1 )
        {
            bzda.selVhclIdx = 0;
        }
        else if ( count <= bzda.selVhclIdx )
        {
            bzda.selVhclIdx = count - 1;
        }
    }
}

int CompareBuildingProtos(const void *lhs, const void *rhs)
{

    uint8_t aa1 = *(const uint8_t *)lhs;
    uint8_t aa2 = *(const uint8_t *)rhs;

    World::TBuildingProto *lhsProto = &protoSortWorld->_buildProtos[aa1];
    World::TBuildingProto *rhsProto = &protoSortWorld->_buildProtos[aa2];

    int lhsRank, rhsRank;

    if ( lhsProto->ModelID == 1 )
        lhsRank = 0;
    else if ( lhsProto->ModelID == 2 )
        lhsRank = 2;
    else if ( lhsProto->ModelID == 3 )
        lhsRank = 1;
    else
        lhsRank = 10;

    if ( rhsProto->ModelID == 1 )
        rhsRank = 0;
    else if ( rhsProto->ModelID == 2 )
        rhsRank = 2;
    else if ( rhsProto->ModelID == 3 )
        rhsRank = 1;
    else
        rhsRank = 10;

    if ( lhsRank >= rhsRank )
    {
        if ( lhsRank <= rhsRank )
        {
            if ( lhsProto->Energy >= rhsProto->Energy )
                return lhsProto->Energy > rhsProto->Energy;
            else
                return -1;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}


void UpdateBuildingList(NC_STACK_ypaworld *yw)
{
    MapScene::Unit robo;
    int ownerMask = 0;

    if ( yw->UiScene().UserRobo(robo) )
        ownerMask = 1 << robo.Owner;

    int count = 0;

    int i = 0;
    for (const World::TBuildingProto &proto : yw->_buildProtos)
    {
        if ( ownerMask & proto.EnableMask )
        {
            bzda.buildProtoIds[count] = i;
            count++;
        }
        i++;
    }

    bzda.buildCount = count;

    protoSortWorld = yw;

    qsort(bzda.buildProtoIds, count, 1, CompareBuildingProtos);

    if ( !count )
    {
        bzda.selBuildIdx = -1;
    }
    else
    {
        if ( count == 1 )
        {
            bzda.selBuildIdx = 0;
        }
        else if ( count <= bzda.selBuildIdx )
        {
            bzda.selBuildIdx = count - 1;
        }
    }
}

void UpdateGunList(NC_STACK_ypaworld *yw)
{
    const MapScene &scene = yw->UiScene();
    MapScene::Unit robo;
    int count = 0;

    if ( scene.UserRobo(robo) )
    {
        uint32_t user = scene.UserUnitId();

        scene.ForEachRoboGun(robo, [&count, user](const MapScene::Unit &gun)
        {
            if ( count >= 256 )
                return;

            bzda.gunIds[count] = gun.Id;

            if ( gun.Id == user )
                bzda.selGunIdx = count;

            count++;
        });
    }

    bzda.gunCount = count;

    if ( !count )
    {
        bzda.selGunIdx = -1;
    }
    else
    {
        if ( count == 1 )
        {
            bzda.selGunIdx = 0;
        }
        else if ( count <= bzda.selGunIdx )
        {
            bzda.selGunIdx = count - 1;
        }
    }
}

void NC_STACK_ypaworld::VoiceMessagePlayFile(const std::string &flname, NC_STACK_ypabact *unit, int priority)
{
    _voiceMessage.Reset();

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    std::string filename = "sounds/speech/";
    std::string locName = Locale::Text::GetLocaleName();

    if ( !locName.empty() )
        filename += locName + "/";
    else
        filename += "language/";

    filename += flname;

    if ( !uaFileExist("rsrc:" + filename) )
    {
        filename = "sounds/speech/language/";
        filename += flname;
    }

    NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>( {{NC_STACK_rsrc::RSRC_ATT_NAME, filename}} );

    Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( wav )
    {        
        _voiceMessage.Carrier.Sounds[0].Pitch = 0;
        _voiceMessage.Carrier.Sounds[0].Volume = priority + 500;

        if ( wav )
            _voiceMessage.Carrier.Sounds[0].PSample = wav->GetSampleData();
        else
            _voiceMessage.Carrier.Sounds[0].PSample = NULL;

        _voiceMessage.Priority = priority;
        _voiceMessage.Sample = wav;
        _voiceMessage.Unit = unit;

        VoiceMessageCalcPositionToUnit();

        if ( UserUnit() )
            _voiceMessage.Carrier.Vector = UserUnit()->_fly_dir * UserUnit()->_fly_dir_length;

        SFXEngine::SFXe.startSound(&_voiceMessage.Carrier, 0);
    }
}
void NC_STACK_ypaworld::VoiceMessagePlayMsg(NC_STACK_ypabact *unit, int priority, int msgID)
{
    if ( _voiceMessage.Priority < 0 || _voiceMessage.Priority < priority )
    {

        struct TMsgVals
        {
            int v1 = 0;
            int type = 0;
            int v3 = 0;
            int v4 = 0;
            int num = 0;
            
            TMsgVals() {};
            
            TMsgVals(int p1, int t, int p2, int p3, int n)
            : v1(p1), type(t), v3(p2), v4(p3), num(n) {};
            
        } msgvals;

        switch ( msgID )
        {
        case 1:
            msgvals = TMsgVals(1, 0, 1, 2, 2);
            break;
        case 5:
            msgvals = TMsgVals(1, 0, 3, 2, 2);
            break;
        case 6:
            msgvals = TMsgVals(1, 0, 2, 3, 1);
            break;
        case 7:
            msgvals = TMsgVals(1, 0, 2, 2, 2);
            break;
        case 8:
            msgvals = TMsgVals(1, 0, 2, 6, 2);
            break;
        case 9:
            msgvals = TMsgVals(1, 0, 2, 5, 1);
            break;
        case 10:
            msgvals = TMsgVals(1, 0, 2, 1, 1);
            break;
        case 11:
            msgvals = TMsgVals(1, 0, 3, 3, 1);
            break;
        case 12:
            msgvals = TMsgVals(2, 1, 2, 6, 2);
            break;
        case 13:
            msgvals = TMsgVals(2, 1, 2, 1, 1);
            break;
        case 14:
            msgvals = TMsgVals(1, 0, 1, 1, 2);
            break;
        case 15:
            msgvals = TMsgVals(1, 0, 1, 3, 2);
            break;
        case 16:
            msgvals = TMsgVals(1, 0, 1, 4, 2);
            break;
        case 17:
            msgvals = TMsgVals(1, 0, 1, 5, 1);
            break;
        case 18:
            msgvals = TMsgVals(1, 0, 2, 4, 1);
            break;
        case 19:
            msgvals = TMsgVals(1, 0, 3, 1, 1);
            break;
        case 22:
            msgvals = TMsgVals(1, 0, 1, 6, 1);
            break;
        case 23:
            msgvals = TMsgVals(2, 1, 1, 1, 1);
            break;
        case 24:
            msgvals = TMsgVals(2, 1, 1, 2, 1);
            break;
        case 25:
            msgvals = TMsgVals(2, 1, 1, 3, 1);
            break;
        case 26:
            msgvals = TMsgVals(2, 1, 1, 4, 1);
            break;
        case 27:
            msgvals = TMsgVals(2, 1, 1, 5, 1);
            break;
        case 28:
            msgvals = TMsgVals(2, 1, 1, 6, 1);
            break;
        case 29:
            msgvals = TMsgVals(2, 1, 1, 7, 1);
            break;
        case 30:
            msgvals = TMsgVals(2, 1, 1, 8, 1);
            break;
        case 31:
            msgvals = TMsgVals(2, 2, 1, 8, 1);
            break;
        case 32:
            msgvals = TMsgVals(2, 3, 1, 8, 1);
            break;
        case 33:
            msgvals = TMsgVals(2, 1, 1, 9, 1);
            break;
        case 34:
            msgvals = TMsgVals(2, 2, 1, 9, 1);
            break;
        case 35:
            msgvals = TMsgVals(2, 3, 1, 9, 1);
            break;
        case 36:
            msgvals = TMsgVals(2, 1, 1, 10, 1);
            break;
        case 37:
            msgvals = TMsgVals(2, 2, 1, 10, 1);
            break;
        case 38:
            msgvals = TMsgVals(2, 3, 1, 10, 1);
            break;
        case 39:
            msgvals = TMsgVals(3, 0, 1, 7, 1);
            break;
        case 40:
            msgvals = TMsgVals(2, 1, 2, 3, 1);
            break;
        case 41:
            msgvals = TMsgVals(2, 1, 2, 4, 2);
            break;
        case 42:
            msgvals = TMsgVals(2, 1, 2, 5, 2);
            break;
        case 43:
            msgvals = TMsgVals(2, 1, 2, 7, 3);
            break;
        case 44:
            msgvals = TMsgVals(2, 1, 2, 8, 2);
            break;
        case 45:
            msgvals = TMsgVals(1, 0, 3, 4, 1);
            if ( !unit )
                return;
            break;
        case 46:
            msgvals = TMsgVals(2, 4, 1, 1, 1);
            break;
        case 47:
            msgvals = TMsgVals(3, 0, 0, 1, 1);
            priority++;
            break;
        case 48:
            msgvals = TMsgVals(3, 0, 0, 2, 2);
            priority += 2;
            break;
        case 49:
            msgvals = TMsgVals(3, 0, 0, 3, 3);
            priority += 3;
            break;
        case 50:
            msgvals = TMsgVals(3, 0, 0, 5, 1);
            priority++;
            break;
        case 51:
            msgvals = TMsgVals(3, 0, 0, 7, 1);
            priority += 3;
            break;
        case 52:
            msgvals = TMsgVals(3, 0, 0, 8, 1);
            priority += 4;
            break;
        case 53:
            msgvals = TMsgVals(3, 0, 0, 9, 1);
            priority += 5;
            break;
        case 54:
            msgvals = TMsgVals(3, 0, 1, 1, 1);
            priority++;
            break;
        case 55:
            msgvals = TMsgVals(3, 0, 1, 2, 1);
            priority += 2;
            break;
        case 56:
            msgvals = TMsgVals(3, 0, 1, 3, 1);
            priority += 3;
            break;
        case 59:
            msgvals = TMsgVals(3, 0, 1, 8, 1);
            priority++;
            break;
        case 60:
            msgvals = TMsgVals(3, 0, 0, 4, 1);
            priority += 4;
            break;
        case 61:
            msgvals = TMsgVals(3, 0, 1, 0, 1);
            priority += 6;
            break;
        case 63:
            msgvals = TMsgVals(3, 0, 3, 0, 1);
            priority += 13;
            break;
        case 64:
            msgvals = TMsgVals(3, 0, 0, 6, 1);
            priority += 2;
            break;
        case 65:
            msgvals = TMsgVals(3, 0, 1, 4, 1);
            priority += 4;
            break;
        case 66:
            msgvals = TMsgVals(3, 0, 1, 6, 1);
            priority += 5;
            break;
        case 67:
            msgvals = TMsgVals(2, 1, 1, 11, 1);
            break;
        case 68:
            msgvals = TMsgVals(2, 2, 1, 8, 1);
            break;
        case 69:
            msgvals = TMsgVals(2, 3, 1, 8, 1);
            break;
        case 70:
            msgvals = TMsgVals(2, 4, 1, 5, 1);
            break;
        case 71:
            msgvals = TMsgVals(2, 4, 1, 8, 1);
            break;
        case 72:
            msgvals = TMsgVals(2, 4, 1, 6, 1);
            break;
        case 73:
            msgvals = TMsgVals(2, 4, 1, 7, 1);
            break;
        case 74:
            msgvals = TMsgVals(2, 4, 1, 9, 1);
            break;
        case 75:
            msgvals = TMsgVals(2, 4, 1, 12, 1);
            break;
        case 76:
            msgvals = TMsgVals(2, 4, 1, 10, 1);
            break;
        case 77:
            msgvals = TMsgVals(2, 4, 1, 11, 1);
            break;
        case 78:
            msgvals = TMsgVals(2, 1, 1, 12, 1);
            break;
        case 79:
            msgvals = TMsgVals(2, 4, 1, 2, 1);
            break;
        case 80:
            msgvals = TMsgVals(2, 1, 1, 13, 1);
            break;
        case 81:
            msgvals = TMsgVals(2, 4, 1, 3, 1);
            break;
        case 82:
            msgvals = TMsgVals(2, 4, 1, 4, 1);
            break;
        case 83:
            msgvals = TMsgVals(2, 5, 1, 2, 1);
            break;
        case 84:
            msgvals = TMsgVals(2, 5, 1, 1, 1);
            break;
        case 85:
            msgvals = TMsgVals(2, 5, 1, 3, 1);
            break;
        case 86:
            msgvals = TMsgVals(2, 5, 1, 4, 1);
            break;
        case 87:
            msgvals = TMsgVals(2, 5, 1, 7, 1);
            break;
        case 88:
            msgvals = TMsgVals(2, 5, 1, 6, 1);
            break;
        case 89:
            msgvals = TMsgVals(2, 5, 1, 8, 1);
            break;
        case 90:
            msgvals = TMsgVals(2, 5, 1, 9, 1);
            break;
        case 91:
            msgvals = TMsgVals(2, 5, 1, 5, 1);
            break;
        case 92:
            msgvals = TMsgVals(2, 5, 1, 10, 1);
            break;
        case 93:
            msgvals = TMsgVals(4, 1, 1, 1, 1);
            break;
        case 94:
            msgvals = TMsgVals(3, 0, 1, 9, 1);
            priority += 2;
            break;
        case 95:
            msgvals = TMsgVals(3, 0, 2, 0, 1);
            priority += 3;
            break;
        case 96:
            msgvals = TMsgVals(3, 0, 2, 1, 1);
            priority += 4;
            break;
        case 97:
            msgvals = TMsgVals(3, 0, 2, 2, 1);
            priority += 5;
            break;
        case 98:
            msgvals = TMsgVals(3, 0, 2, 3, 1);
            priority += 6;
            break;
        case 99:
            msgvals = TMsgVals(3, 0, 2, 4, 1);
            priority += 7;
            break;
        case 100:
            msgvals = TMsgVals(3, 0, 2, 5, 1);
            priority += 8;
            break;
        case 101:
            msgvals = TMsgVals(3, 0, 2, 6, 1);
            priority += 9;
            break;
        case 102:
            msgvals = TMsgVals(3, 0, 2, 7, 1);
            priority += 10;
            break;
        case 103:
            msgvals = TMsgVals(3, 0, 2, 8, 1);
            priority += 11;
            break;
        case 104:
            msgvals = TMsgVals(3, 0, 2, 9, 1);
            priority += 12;
            break;
        
        default:
            break;
        }
        
        if ( msgvals.v1 )
        {
            if ( !unit )
                unit = UserRobo() ? UserRobo() : UserUnit();

            if ( !unit )
                return;
            
            int vo_type = msgvals.type;

            if ( msgvals.v1 == 1 )
            {
                vo_type = _vhclProtos[unit->_vehicleID].vo_type;

                if ( !vo_type )
                    vo_type = 11;
            }

            int variant = 1;
            
            if ( msgvals.num > 1 )
                variant = rand() % msgvals.num + 1;

            VoiceMessagePlayFile(fmt::sprintf("%x%x%x%x%x.wav", msgvals.v1, vo_type, msgvals.v3, msgvals.v4, variant), unit, priority);
        }
    }
}

void DrawGameMessages(NC_STACK_ypaworld *yw)
{
    
    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_isNetGame )
    {
        if ( yw->_netEvent.EventType )
        {
            uint32_t timeDelay;
            std::string str;
            
            if ( yw->_netEvent.EventType == 1 )
            {
                str = Locale::Text::Advanced(Locale::ADV_VICTORY);
                timeDelay = 40000;
            }
            else if ( yw->_netEvent.EventType > 1 && yw->_netEvent.EventType <= 4 )
            {
                str = fmt::sprintf( Locale::Text::Advanced(Locale::ADV_DEFEATED) , yw->_netEvent.PlayerName);

                timeDelay = 20000;
            }
            else
            {
                timeDelay = 0;
            }

            if ( !str.empty() && yw->_timeStamp - yw->_netEvent.TimeStamp < timeDelay )
            {
                if ( yw->_timeStamp / 300 & 1 )
                {
                    FontUA::select_tileset(&buf, 15);
                    FontUA::set_xpos(&buf, 0);
                    FontUA::set_ypos(&buf, yw->_screenSize.y / 3);

                    FontUA::set_txtColor(&buf, 255, 255, 255);

                    FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, str, yw->_screenSize.x);
                }
            }
            else
            {
                yw->_netEvent.EventType = 0;
            }
        }
    }

    if ( DrawTechUpgrade(yw) )
        return;

    int shownCount;

    if ( info_log.oldest_msg_idx <= info_log.msg_count )
        shownCount = info_log.msg_count - info_log.oldest_msg_idx;
    else
        shownCount = info_log.msg_count + 64 - info_log.oldest_msg_idx;

    if ( shownCount > 10 )
        shownCount = 10;

    FontUA::select_tileset(&buf, 15);
    FontUA::set_ypos(&buf, up_panel.panelHeight + yw->_fontH / 2);


    int msgIdx = (info_log.msg_count - shownCount) % 64;

    if ( msgIdx < 0 )
        msgIdx += 64;

    int msgTime = 0;
    int decayApplied = 0;

    while ( 1 )
    {
        if ( msgIdx >= 64 )
            msgIdx = 0;

        TInfoLogMsg *msg = &info_log.msgs[msgIdx];

        if ( msg->displayTime > 0 )
        {
            if ( !decayApplied )
            {
                decayApplied = 1;

                int decayRate;

                if ( msgIdx > info_log.msg_count )
                    decayRate = info_log.msg_count + 64 - msgIdx;
                else
                    decayRate = info_log.msg_count - msgIdx;

                decayRate = (decayRate + 1) / 2;

                if ( !decayRate )
                    decayRate = 1;

                msg->displayTime -= yw->_frameTime * decayRate;
            }

            if ( msg->timestamp )
                msgTime = msg->timestamp;
            else
                msgTime += 200;

            int age = yw->_timeStamp - msgTime;

            int revealPercent;

            if ( age < 200 )
            {
                revealPercent = 100 * age / 200;

                if ( revealPercent < 0 )
                    revealPercent = 0;
                else if ( revealPercent > 100 )
                    revealPercent = 100;
            }
            else
            {
                revealPercent = 100;
            }

            if ( revealPercent > 0 )
            {
                FontUA::set_xpos(&buf, 16);
                FontUA::set_txtColor(&buf, yw->_iniColors[64].r, yw->_iniColors[64].g, yw->_iniColors[64].b);
                // Output ingame messages. From analyzer and other.
                FontUA::TextRelWidthItem(yw->_guiTiles[15], &buf, msg->txt, revealPercent, 4);

                FontUA::next_line(&buf);
            }
        }

        if ( msgIdx == info_log.msg_count )
            break;

        msgIdx++;
    }

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);
}



void wis_color(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float pulsePos = yw->_hud.pulse_phase * 2.0 + -1.5;

    float dist1 = fabs(pulsePos - x1);
    float dist2 = fabs(pulsePos - x2);

    if ( dist1 > 0.5 )
        dist1 = 0.5;

    if ( dist2 > 0.5 )
        dist2 = 0.5;

    float blend1 = dist1 * 2.0;
    int g1 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend1 + yw->_hud.cl1_g;
    int r1 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend1 + yw->_hud.cl1_r;
    int b1 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend1 + yw->_hud.cl1_b;

    *out1 = GFX::Engine.Color(r1, g1, b1);

    float blend2 = 2.0 * dist2;
    int g2 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend2 + yw->_hud.cl1_g;
    int r2 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend2 + yw->_hud.cl1_r;
    int b2 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend2 + yw->_hud.cl1_b;
    // TODO(cleanup): Passes (g2, r2, b2), so red and green are swapped compared with out1.
    // Harmless today: yw_RenderVector2D never uses out2.
    *out2 = GFX::Engine.Color(g2, r2, b2);
}




void yw_RenderVector2D(NC_STACK_ypaworld *yw, UAskeleton::Data *wire, float posX, float posY, float m00, float m01, float m10, float m11, float scaleX, float scaleY, SDL_Color coloooor, wis_color_func color_func, wis_color_func color_func2, bool aspectCorrection)
{
    float CW = 1.0, CH = 1.0;

    if (aspectCorrection)
        GFX::Engine.getAspectCorrection(CW, CH, false);

    if ( wire )
    {
        GFX::Engine.SetPen(coloooor);

        for (size_t i = 0; i < wire->POO.size(); i++)
        {
            float tmpx = wire->POO[i].x * 0.001;
            float tmpz = -wire->POO[i].z * 0.001;

            wire->tformedVertex[i].x = (tmpx * m00 + tmpz * m01) * scaleX * CW + posX;
            wire->tformedVertex[i].y = (tmpx * m10 + tmpz * m11) * scaleY * CH + posY;
        }

        for (size_t i = 0; i < wire->polygons.size(); i++)
        {
            UAskeleton::Polygon *poly = &wire->polygons[i];

            if (poly->num_vertices >= 2)
            {
                Common::FLine line;
                line.x1 = wire->tformedVertex[ poly->v[0] ].x;
                line.y1 = wire->tformedVertex[ poly->v[0] ].y;
                line.x2 = wire->tformedVertex[ poly->v[1] ].x;
                line.y2 = wire->tformedVertex[ poly->v[1] ].y;

                if ( color_func )
                {
                    SDL_Color startColor = coloooor;
                    SDL_Color endColor = coloooor;

                    color_func(yw,  line.x1 - posX,   line.y1 - posY,   line.x2 - posX,   line.y2 - posY, &startColor, &endColor);

                    GFX::Engine.SetPen(startColor);
                }
                else if ( color_func2 )
                {
                    SDL_Color startColor = coloooor;
                    SDL_Color endColor = coloooor;

                    color_func2(yw, line.x1, line.y1, line.x2, line.y2, &startColor, &endColor);


                    GFX::Engine.SetPen(startColor);
                }

                GFX::Engine.DrawClippedLineNormalized(line);
            }
        }
    }
}

void yw_RenderInfoVehicleWire(NC_STACK_ypaworld *yw, THudState *wis, World::TVhclProto *vhcl, float xpos, float ypos, float appearProgress)
{
    SDL_Color color_25 = yw->GetColor(25);
    SDL_Color color_34 = yw->GetColor(34);

    UAskeleton::Data *wairufureimu = NULL;

    wis_color_func func = NULL;

    if ( wis->color_fx )
        func = wis_color;

    if ( vhcl->wireframe )
        wairufureimu = vhcl->wireframe->GetSkelet();

    if ( wairufureimu )
    {
        float scaleY = wis->line_height * 6.0 * 0.8;

        float scaleX = 0.1;

        if ( appearProgress < 1.4 )
            scaleX = appearProgress * 0.1;

        yw->_hud.cl1_r = color_34.r;
        yw->_hud.cl1_g = color_34.g;
        yw->_hud.cl1_b = color_34.b;
        yw->_hud.cl2_r = color_25.r;
        yw->_hud.cl2_b = color_25.b;
        yw->_hud.cl2_g = color_25.g;

        yw_RenderVector2D(yw, wairufureimu, xpos, ypos, 1.0, 0.0, 0.0, 1.0, scaleX, scaleY, color_34, NULL, func, true);
    }
}



void RenderInfoBar(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, float x, float y, int value, int maxval, int valCH, int valBG, const std::string &txt1, const std::string &txt2, int flag = 0)
{
    int wnd_xpos = 0;
    int wnd_xpos2 = 0;
    int wnd_ypos = 0;
    int wnd_ypos2 = 0;

    int wnd_vis = 0;

    if ( robo_map.IsOpen() )
    {
        wnd_xpos = robo_map.x - (yw->_screenSize.x / 2);
        wnd_xpos2 = robo_map.w + wnd_xpos;
        wnd_ypos = robo_map.y - (yw->_screenSize.y / 2);
        wnd_ypos2 = robo_map.h + wnd_ypos;

        wnd_vis = 1;
    }

    int penX = (yw->_screenSize.x / 2) * x;
    int penY = (yw->_screenSize.y / 2) * y;

    int barWidth = yw->_guiTiles[51]->map[valCH].w * wis->bar_segments;


    FontUA::set_txtColor(cur, yw->_iniColors[65].r, yw->_iniColors[65].g, yw->_iniColors[65].b);

    penX -= (wis->value_width + wis->label_width + barWidth) / 2;

    if ( !txt1.empty() )
    {
        int labelX = (wis->label_width / 2) + penX;
        int labelY = penY;

        if ( !wnd_vis || labelX <= wnd_xpos || labelX >= wnd_xpos2 || labelY <= wnd_ypos || labelY >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, penX);
            FontUA::set_center_ypos(cur, penY - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt1, 100, 4);
        }
    }

    FontUA::select_tileset(cur, 51);

    penX += wis->label_width;

    FontUA::set_center_xpos(cur, penX);
    FontUA::set_center_ypos(cur, penY - (yw->_guiTiles[51]->h / 2));

    int segX = penX + (yw->_guiTiles[51]->map[1].w / 2);
    int segY = penY + (yw->_guiTiles[51]->h >> 1);

    for (int i = 1; i <= wis->bar_segments; i++)
    {
        int segValue = i * (maxval / wis->bar_segments) - (maxval / wis->bar_segments) / 2;

        if ( !wnd_vis || segX <= wnd_xpos || segX >= wnd_xpos2 || segY <= wnd_ypos || segY >= wnd_ypos2 )
        {
            if ( segValue > value )
            {
                if ( (flag & 2) == 0 )
                    FontUA::store_u8(cur, valBG);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
            else
            {
                if ( (flag & 1) == 0 )
                    FontUA::store_u8(cur, valCH);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
        }

        segX += yw->_guiTiles[51]->map[1].w;
    }

    penX += wis->value_width + barWidth;

    if ( !txt2.empty() )
    {
        int valueX = penX - (wis->value_width / 2);
        int valueY = penY;

        if ( !wnd_vis || valueX <= wnd_xpos || valueX >= wnd_xpos2 || valueY <= wnd_ypos || valueY >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, penX);
            FontUA::set_center_ypos(cur, penY - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt2, 100, 8);
        }
    }
}

void yw_RenderInfoLifebar(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, const THudSubject *bact, World::TVhclProto *vhcl, float xpos, float ypos)
{
    int energy;
    int maxEnergy;

    if ( bact )
    {
        energy = bact->Energy;
        maxEnergy = bact->EnergyMax;
    }
    else
    {
        maxEnergy = vhcl->energy;
        energy = vhcl->energy;
    }

    RenderInfoBar(yw, wis, cur, xpos, ypos, energy, maxEnergy, 2, 6, Locale::Text::HUD(Locale::HUDSTR_EGY), fmt::sprintf("%d", (energy + 99) / 100));
}


void yw_RenderInfoShieldbar(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, const THudSubject *bact, World::TVhclProto *vhcl, float xpos, float ypos)
{
    int shield;

    if ( bact )
        shield = bact->Shield;
    else
        shield = vhcl->shield;

    RenderInfoBar(yw, wis, cur, xpos, ypos, shield, 100, 1, 5, Locale::Text::HUD(Locale::HUDSTR_AMR), fmt::sprintf("%d%%", shield), 2);
}



void yw_RenderInfoVehicleName(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, const std::string &name, float xpos, float ypos)
{
    int centerX = (yw->_screenSize.x / 2) * xpos;
    int centerY = (yw->_screenSize.y / 2) * ypos;

    int fieldWidth = yw->_screenSize.x * wis->bar_width;

    SDL_Color highlightColor = yw->GetColor(25);
    SDL_Color baseColor = yw->GetColor(34);

    wis_color_func colorFunc;

    if ( wis->color_fx )
        colorFunc = wis_color;
    else
        colorFunc = NULL;

    int wnd_xpos = 0;
    int wnd_xpos2 = 0;
    int wnd_ypos = 0;
    int wnd_ypos2 = 0;

    int wnd_vis = 0;

    if ( robo_map.IsOpen() )
    {
        wnd_xpos = robo_map.x - (yw->_screenSize.x / 2);
        wnd_xpos2 = robo_map.w + wnd_xpos;
        wnd_ypos = robo_map.y - (yw->_screenSize.y / 2);
        wnd_ypos2 = robo_map.h + wnd_ypos;

        wnd_vis = 1;
    }

    int textX = centerX - (fieldWidth / 2);
    int textY = centerY - (yw->_fontH / 2);

    if ( !wnd_vis || centerX <= wnd_xpos || centerX >= wnd_xpos2 || centerY <= wnd_ypos || centerY >= wnd_ypos2 )
    {
        FontUA::select_tileset(cur, 15);

        FontUA::set_center_xpos(cur, textX);
        FontUA::set_center_ypos(cur, textY);

        FontUA::set_txtColor(cur,  yw->_iniColors[65].r,  yw->_iniColors[65].g,  yw->_iniColors[65].b);

        LayoutCenteredTextField(yw->_guiTiles[15], cur, name, fieldWidth, 32);
    }

    if ( wis->sklts_intern[2] )
    {
        yw->_hud.cl1_r = baseColor.r;
        yw->_hud.cl1_g = baseColor.g;
        yw->_hud.cl1_b = baseColor.b;
        yw->_hud.cl2_r = highlightColor.r;
        yw->_hud.cl2_b = highlightColor.b;
        yw->_hud.cl2_g = highlightColor.g;

        float wireY = (1.0 / (float)yw->_screenSize.y) * 2.0 + ypos;

        float scaleY = wis->line_height * 1.2;

        yw_RenderVector2D(yw, wis->sklts_intern[2], xpos, wireY, 1.0, 0.0, 0.0, 1.0, wis->bar_width, scaleY, baseColor, NULL, colorFunc, true);
    }
}


void yw_RenderInfoWeaponWire(NC_STACK_ypaworld *yw, THudState *wis, World::TWeapProto *wpn, float xpos, float ypos)
{
    UAskeleton::Data *wairufureimu = NULL;

    wis_color_func func;

    if ( wis->color_fx )
        func = wis_color;
    else
        func = NULL;

    if ( wpn )
    {
        if ( wpn->wireframe )
            wairufureimu = wpn->wireframe->GetSkelet();

        if ( wairufureimu )
        {
            SDL_Color highlightColor = yw->GetColor(20);
            SDL_Color baseColor = yw->GetColor(29);

            yw->_hud.cl1_r = baseColor.r;
            yw->_hud.cl1_g = baseColor.g;
            yw->_hud.cl1_b = baseColor.b;
            yw->_hud.cl2_r = highlightColor.r;
            yw->_hud.cl2_b = highlightColor.b;
            yw->_hud.cl2_g = highlightColor.g;

            yw_RenderVector2D(yw, wairufureimu, xpos, ypos, 1.0, 0.0, 0.0, 1.0, 0.0415, 0.05, baseColor, NULL, func, true);
        }
    }
}

int WeaponReloadPercent(const World::TWeapProto &wpn, int sinceShot, int salveCounter)
{
    int reloadTime = wpn.shot_time_user;

    if ( wpn.salve_shots )
    {
        if ( salveCounter >= wpn.salve_shots )
            reloadTime = wpn.salve_delay;
    }

    if ( reloadTime <= 0 )
        return 100;

    return std::min(100 * sinceShot / reloadTime, 100);
}

void yw_RenderInfoReloadbar(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, const THudSubject *bact, World::TWeapProto *wpn, float xpos, float ypos)
{
    if ( wpn )
    {
        int reloadPercent = bact ? bact->ReloadPercent : 100;

        std::string txt2;

        if ( reloadPercent == 100 )
            txt2 = Locale::Text::HUD(Locale::HUDSTR_OK);
        else
            txt2 = fmt::sprintf("%d%%", reloadPercent);

        RenderInfoBar(yw, wis, cur, xpos, ypos, reloadPercent, 100, 1, 3, Locale::Text::HUD(Locale::HUDSTR_RLD), txt2);
    }
}

void yw_RenderInfoWeaponInf(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, const THudSubject *bact, World::TVhclProto *vhcl, World::TWeapProto *weap, float xpos, float ypos)
{
    if ( weap )
    {
        std::string txt2;

        if ( vhcl->num_weapons <= 1 )
            txt2 = fmt::sprintf("%d", weap->energy / 100);
        else
            txt2 = fmt::sprintf("%d x%d", weap->energy / 100, vhcl->num_weapons);

        RenderInfoBar(yw, wis, cur, xpos, ypos, weap->energy, 100, 7, 7, Locale::Text::HUD(Locale::HUDSTR_DMG), txt2, 1 | 2);
    }
}


void yw_RenderHUDInfo(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, float xpos, float ypos, const THudSubject *bact, int vhclid, int flag)
{
//  yw->GetColor(25);
//  yw->GetColor(34);
    int showWeapon = 1;
    int showEnergy = 1;
    int showShield = 1;
    int showVehicle = 1;

    if ( flag & 1 )
        showWeapon = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 2 )
        showEnergy = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 4 )
        showShield = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 8 )
        showVehicle = ((yw->_timeStamp / 200) & 1) != 0;

    float appearProgress;

    if ( flag & 0x10 )
    {
        appearProgress = (yw->_timeStamp - wis->switch_time - 200) / 180.0;

        if ( appearProgress <= 0.0 )
            return;
    }
    else
    {
        appearProgress = 1.0;
    }

    if ( vhclid == -1 )
        vhclid = bact->Vehicle;

    World::TVhclProto *vhcl = &yw->_vhclProtos[vhclid];


    World::TWeapProto *weap;

    if ( vhcl->weapon == -1 )
        weap = NULL;
    else
        weap = &yw->_weaponProtos.at(vhcl->weapon);

    if ( showVehicle )
        yw_RenderInfoVehicleWire(yw, wis, vhcl, xpos, ypos, appearProgress);

    if ( showEnergy )
        yw_RenderInfoLifebar(yw, wis, cur, bact, vhcl, xpos,   wis->line_height * 7.0 + ypos  );

    if ( showShield )
        yw_RenderInfoShieldbar(yw, wis, cur, bact, vhcl, xpos,   wis->line_height * 9.0 + ypos  );

    if ( showVehicle )
    {
        if ( vhcl->model_id != BACT_TYPES_GUN )
        {
            float nameY = wis->line_height * 12.0 + ypos;

            yw_RenderInfoVehicleName(yw, wis, cur, yw->GetVehicleName(*vhcl), xpos, nameY);
        }
    }

    if ( weap )
    {
        if ( showWeapon )
        {
            yw_RenderInfoWeaponWire(yw, wis, weap, xpos,   ypos - wis->line_height * 9.0);

            yw_RenderInfoReloadbar(yw, wis, cur, bact, weap, xpos,  ypos - wis->line_height * 7.0);

            yw_RenderInfoWeaponInf(yw, wis, cur, bact, vhcl, weap, xpos,  ypos - wis->line_height * 9.0);
        }
    }
}

void DrawTechUpgradeBuildingName(NC_STACK_ypaworld *yw, THudState *wis, CmdStream *cur, float xpos, float ypos, int bldId, int flags)
{
    //yw->GetColor(25);
    //yw->GetColor(34);

    if ( !(flags & 0x10) || (yw->_timeStamp - wis->switch_time - 200) / 180.0 > 0.0 )
    {
        if ( yw->_timeStamp / 200 & 1 )
        {
            std::string bldName;
            if ( yw->_isNetGame )
                bldName = yw->GetBuildingName(bldId, true);
            else
                bldName = yw->GetBuildingName(bldId, false);

            yw_RenderInfoVehicleName(yw, wis, cur, bldName, xpos, ypos);
        }
    }
}

int DrawTechUpgrade(NC_STACK_ypaworld *yw)
{
    //Tech update draw
    GFX::Engine.SetClipNormalized( Common::FRect(-1.0, -1.0, 1.0, 1.0) );

    THudState *wis = &yw->_hud;

    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_upgradeId == -1 || yw->_timeStamp - yw->_upgradeTimeStamp >= 10000 || (!yw->_upgradeVehicleId && !yw->_upgradeWeaponId && !yw->_upgradeBuildId) )
        return 0;

    int blinkFlags = 0;
    int vhclId = yw->_upgradeVehicleId;
    int bldId = yw->_upgradeBuildId;

    if ( !vhclId && yw->_upgradeWeaponId)
    {
        bool notOk = true;
        int i = 0;
        for (const World::TVhclProto &proto : yw->_vhclProtos)
        {
            if ( proto.weapon == yw->_upgradeWeaponId)
            {
                vhclId = i;
                notOk = false;
                break;
            }
            i++;
        }
        
        if (notOk)
            return 0;
    }

    wis->pulse_phase = 1.0;

    int upgradeType = yw->_techUpgrades[ yw->_upgradeId ].Type;

    if ( upgradeType == World::UPGRADE_WEAPON )
        blinkFlags |= 1;
    else if ( upgradeType == World::UPGRADE_ARMOR )
        blinkFlags |= 4;
    else if ( upgradeType == World::UPGRADE_VEHICLE )
        blinkFlags |= 8;

    if ( vhclId )
        yw_RenderHUDInfo(yw, wis, &buf, 0.0, -0.5, NULL, vhclId, blinkFlags);
    else if ( bldId )
        DrawTechUpgradeBuildingName(yw, wis, &buf, 0.0, -0.5, bldId, blinkFlags);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    return 1;
}




void wis_color2(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float phase = yw->_hud.pulse_phase * 6.28;

    float cosPhase = cos(phase);
    float sinPhase = sin(phase);

    float len1 = sqrt(POW2(x1) + POW2(y1));
    if ( len1 > 0.0 )
    {
        x1 /= len1;
        y1 /= len1;
    }

    float len2 = sqrt(POW2(x2) + POW2(y2));
    if ( len2 > 0.0 )
    {
        x2 /= len2;
        y2 /= len2;
    }

    float blend1 = (cosPhase * x1 + sinPhase * y1 + 1.0) * 0.5;
    int r1 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend1 + yw->_hud.cl1_r;
    int b1 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend1 + yw->_hud.cl1_b;
    int g1 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend1 + yw->_hud.cl1_g;
    *out1 = GFX::Engine.Color(r1, g1, b1);

    float blend2 = (cosPhase * x2 + sinPhase * y2 + 1.0) * 0.5;
    int g2 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend2 + yw->_hud.cl1_g;
    int r2 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend2 + yw->_hud.cl1_r;
    int b2 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend2 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(r2, g2, b2);
}



void yw_RenderHUDCompass(NC_STACK_ypaworld *yw, THudState *wis, const THudSubject &unit)
{
    float openProgress = (yw->_timeStamp - wis->switch_time) / 180.0;

    wis_color_func func = NULL;

    if ( wis->color_fx )
        func = wis_color2;

    float xx = unit.Rotation.m02;
    float yy = unit.Rotation.m22;

    float headingLen = sqrt( POW2(xx) + POW2(yy) );

    if ( headingLen > 0.0 )
    {
        xx /= headingLen;
        yy /= headingLen;
    }

    SDL_Color roboColor = yw->GetColor(21);
    SDL_Color roboColor2 = yw->GetColor(30);

    float tx = unit.ParentPos.x - unit.Position.x;
    float ty = unit.ParentPos.z - unit.Position.z;

    float roboDx = tx * yy + ty * xx;
    float roboDy = tx * -xx + ty * yy;

    float roboDist = sqrt( POW2(roboDx) + POW2(roboDy) );

    if ( unit.HasParent && roboDist > 30.0 )
    {
        float roboDirX = roboDx * (1.0 / roboDist);
        float roboDirY = roboDy * (1.0 / roboDist);

        yw->_hud.cl1_r = roboColor.r;
        yw->_hud.cl1_g = roboColor.g;
        yw->_hud.cl1_b = roboColor.b;
        yw->_hud.cl2_r = roboColor2.r;
        yw->_hud.cl2_b = roboColor2.b;
        yw->_hud.cl2_g = roboColor2.g;

        yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, roboDirY, -roboDirX, roboDirX, roboDirY, 0.25, 0.3, roboColor, func, 0, true);
    }

    {
        if ( unit.HasTarget )
        {
            tx = unit.TargetPos.x - unit.Position.x;
            ty = unit.TargetPos.z - unit.Position.z;

            SDL_Color targetColor = yw->GetColor(22);
            SDL_Color targetColor2 = yw->GetColor(31);

            float targetDx = tx * yy + ty * xx;
            float targetDy = tx * -xx + ty * yy;

            float targetDist = sqrt( POW2(targetDx) + POW2(targetDy) );

            if ( targetDist > 30.0 )
            {
                float targetDirX = targetDx * (1.0 / targetDist);
                float targetDirY = targetDy * (1.0 / targetDist);

                yw->_hud.cl1_r = targetColor.r;
                yw->_hud.cl1_g = targetColor.g;
                yw->_hud.cl1_b = targetColor.b;
                yw->_hud.cl2_r = targetColor2.r;
                yw->_hud.cl2_b = targetColor2.b;
                yw->_hud.cl2_g = targetColor2.g;

                yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, targetDirY, -targetDirX, targetDirX, targetDirY, 0.25, 0.3, targetColor, func, 0, true);
            }
        }
    }

    if ( unit.VisorTarget )
    {
        SDL_Color visorColor = yw->GetColor(23);
        SDL_Color visorColor2 = yw->GetColor(32);

        float visorTx = unit.VisorTargetPos.x - unit.Position.x;
        float visorTy = unit.VisorTargetPos.z - unit.Position.z;

        float visorDx = visorTx * yy + visorTy * xx;
        float visorDy = visorTx * -xx + visorTy * yy;


        float visorDist = sqrt( POW2(visorDx) + POW2(visorDy) );

        if ( visorDist > 30.0 )
        {
            float visorDirX = visorDx * (1.0 / visorDist);
            float visorDirY = visorDy * (1.0 / visorDist);

            yw->_hud.cl1_r = visorColor.r;
            yw->_hud.cl1_g = visorColor.g;
            yw->_hud.cl1_b = visorColor.b;
            yw->_hud.cl2_r = visorColor2.r;
            yw->_hud.cl2_b = visorColor2.b;
            yw->_hud.cl2_g = visorColor2.g;

            yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, visorDirY, -visorDirX, visorDirX, visorDirY, 0.25, 0.3, visorColor, func, 0, true);
        }
    }

    SDL_Color compassColor = yw->GetColor(24);
    SDL_Color compassColor2 = yw->GetColor(33);

    float compassScaleX = 0.25;
    float compassScaleY = 0.3;

    if ( openProgress < 1.4 )
    {
        compassScaleY = openProgress * 0.3;
        compassScaleX = openProgress * 0.25;
    }

    yw->_hud.cl1_r = compassColor.r;
    yw->_hud.cl1_g = compassColor.g;
    yw->_hud.cl1_b = compassColor.b;
    yw->_hud.cl2_r = compassColor2.r;
    yw->_hud.cl2_b = compassColor2.b;
    yw->_hud.cl2_g = compassColor2.g;

    yw_RenderVector2D(yw, wis->sklts_intern[1], 0.7, 0.3, yy, -xx, xx, yy, compassScaleX, compassScaleY, compassColor, func, 0, true);

    int sectorOwner;

    switch ( unit.SectorOwner )
    {
    case 0:
        sectorOwner = 0;
        break;

    case 1:
        sectorOwner = 1;
        break;

    case 2:
        sectorOwner = 2;
        break;

    case 3:
        sectorOwner = 3;
        break;

    case 4:
        sectorOwner = 4;
        break;

    case 5:
        sectorOwner = 5;
        break;

    case 6:
        sectorOwner = 6;
        break;

    case 7:
        sectorOwner = 7;
        break;

    default:
        sectorOwner = 0;
        break;
    }

    SDL_Color ownerColor = yw->GetColor(sectorOwner);

    yw->_hud.cl1_r = ownerColor.r;
    yw->_hud.cl1_g = ownerColor.g;
    yw->_hud.cl1_b = ownerColor.b;
    yw->_hud.cl2_r = ownerColor.r;
    yw->_hud.cl2_b = ownerColor.b;
    yw->_hud.cl2_g = ownerColor.g;

    yw_RenderVector2D(yw, wis->sklts_intern[2], 0.7, 0.3, yy, -xx, xx, yy, 0.07, 0.08, ownerColor, func, 0, true);
}


void yw_RenderHUDVectorGFX(NC_STACK_ypaworld *yw, CmdStream *cur, const THudSubject &unit)
{
    GFX::Engine.SetClipNormalized( Common::FRect(-1.0, -1.0, 1.0, 1.0) );

    Common::Rect mapRect;

    if ( !robo_map.IsClosed() )
    {
        mapRect.left = robo_map.x - (yw->_screenSize.x / 2);
        mapRect.top = robo_map.y - (yw->_screenSize.y / 2);
        mapRect.right = robo_map.w + mapRect.left;
        mapRect.bottom = robo_map.h + mapRect.top;
    }

    GFX::Engine.SetInverseClipCentered(mapRect);

    THudState *wis = &yw->_hud;

    yw_RenderHUDCompass(yw, wis, unit);

    GFX::Engine.SetInverseClipCentered(Common::Rect());

    yw_RenderHUDTarget(yw, wis, unit);

    GFX::Engine.SetInverseClipCentered(mapRect);

    yw_RenderHUDInfo(yw, wis, cur, -0.7, 0.3, &unit, -1, 0x10);

    if ( robo_map.IsOpen() )
        GFX::Engine.SetInverseClipCentered(Common::Rect());
}

void DrawNetPlayerName(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact)
{
    if ( yw->_GameShell )
    {
        if ( yw->_isNetGame )
        {
            if ( !yw->_GameShell->netPlayers[bact->_owner].Name.empty() )
            {
                float dx = bact->_position.x - yw->_viewerPosition.x;
                float dy = bact->_position.y - yw->_viewerPosition.y;
                float dz = bact->_position.z - yw->_viewerPosition.z;

                mat3x3 corrected = yw->_viewerRotation;
                GFX::Engine.matrixAspectCorrection(corrected, false);

                float viewX = corrected.m00 * dx + corrected.m01 * dy + corrected.m02 * dz;
                float viewY = corrected.m10 * dx + corrected.m11 * dy + corrected.m12 * dz;
                float viewZ = corrected.m20 * dx + corrected.m21 * dy + corrected.m22 * dz;

                if ( viewZ > 30.0 && viewX < viewZ )
                {
                    if ( viewX > -viewZ && viewY < viewZ && viewY > -viewZ )
                    {
                        int tileH = yw->_guiTiles[15]->h;

                        int screenX = ((yw->_screenSize.x / 2) * (viewX / viewZ + 1.0));
                        int screenY = (yw->_screenSize.y / 2) * (viewY / viewZ + 1.0);

                        int textWidth = 96;

                        if ( screenX + 96 >= yw->_screenSize.x )
                            textWidth = yw->_screenSize.x - screenX - 1;

                        if ( textWidth > 0 && screenX >= 0 )
                        {
                            if ( textWidth + screenX <  yw->_screenSize.x && screenY >= 0 )
                            {
                                if ( tileH + screenY < yw->_screenSize.y )
                                {
                                    FontUA::select_tileset(cur, 15);
                                    FontUA::set_center_xpos(cur, screenX - (yw->_screenSize.x / 2) );
                                    FontUA::set_center_ypos(cur, screenY - (yw->_screenSize.y / 2) );

                                    FontUA::set_txtColor(cur, yw->_iniColors[ bact->_owner ].r, yw->_iniColors[ bact->_owner ].g, yw->_iniColors[ bact->_owner ].b);

                                    FontUA::FormateClippedText(yw->_guiTiles[15], cur,  yw->_GameShell->netPlayers[bact->_owner].Name, textWidth, 32);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void yw_RenderUnitLifeBar(NC_STACK_ypaworld *yw, CmdStream *cur, const MapScene::Unit &bact)
{
    // Render fraction triangles above units

    float dx = bact.Position.x - yw->_viewerPosition.x;
    float dy = bact.Position.y - yw->_viewerPosition.y;
    float dz = bact.Position.z - yw->_viewerPosition.z;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    float viewX = corrected.m00 * dx + corrected.m01 * dy + corrected.m02 * dz;
    float viewY = corrected.m10 * dx + corrected.m11 * dy + corrected.m12 * dz;
    float viewZ = corrected.m20 * dx + corrected.m21 * dy + corrected.m22 * dz;

    if ( viewZ > 30.0 && viewX < viewZ )
    {
        if ( viewX > -viewZ && viewY < viewZ && viewY > -viewZ )
        {
            int mapLeft = 0;
            int mapRight = 0;
            int mapBottom = 0;
            int mapTop = 0;
            int mapOpen = 0;

            if ( robo_map.IsOpen() )
            {
                mapOpen = 1;
                mapLeft = robo_map.x;
                mapRight = mapLeft + robo_map.w;
                mapTop = robo_map.y;
                mapBottom = mapTop + robo_map.h;
            }

            float projX = viewX * (1.0 / viewZ);
            float projY = viewY * (1.0 / viewZ);

            int segCount;

            if ( yw->_screenSize.x >= 512 )
                segCount = 8;
            else
                segCount = 4;

            int barWidth = segCount * yw->_guiTiles[50]->map[0].w;

            int screenX = (yw->_screenSize.x / 2) * (projX + 1.0);
            int screenY = (yw->_screenSize.y / 2) * (projY + 1.0);

            if ( !mapOpen || screenX <= mapLeft || screenX >= mapRight || screenY <= mapTop || screenY >= mapBottom )
            {
                screenX -= barWidth / 2;
                screenY -= (yw->_guiTiles[50]->h / 2) + (yw->_screenSize.y / 16);

                if ( screenX >= 0 )
                {
                    if ( screenX + barWidth < yw->_screenSize.x && screenY >= 0 )
                    {
                        if ( yw->_guiTiles[50]->h + screenY < yw->_screenSize.y )
                        {
                            FontUA::select_tileset(cur, 50);

                            FontUA::set_center_xpos(cur, screenX - (yw->_screenSize.x / 2) );
                            FontUA::set_center_ypos(cur, screenY - (yw->_screenSize.y / 2) );

                            int energyPerSeg = bact.EnergyMax / segCount;

                            int threshold = energyPerSeg / 2; //v38 - (v38 / 2);

                            for (int i = 1; i <= segCount; i++)
                            {
                                if ( threshold > bact.Energy )
                                    FontUA::store_u8(cur, 6);
                                else
                                    FontUA::store_u8(cur, 2);

                                threshold += energyPerSeg;
                            }
                        }
                    }
                }
            }
        }
    }
}


void yw_RenderOverlayCursors(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    const MapScene &scene = *robo_map.Scene;
    MapScene::Unit user;

    if ( (yw->_preferences & World::PREF_ENEMYINDICATOR) && scene.UserUnit(user) )
    {
        Common::Point userSector = user.Sector;

        int minX = userSector.x - 1;
        int maxX = userSector.x + 1;
        int minY = userSector.y - 1;
        int maxY = userSector.y + 1;

        if ( minX < 1 )
            minX = 1;

        if ( minY < 1 )
            minY = 1;

        if ( maxX >= yw->_mapSize.x )
            maxX = yw->_mapSize.x - 1;

        if ( maxY >= yw->_mapSize.y )
            maxY = yw->_mapSize.y - 1;

        for (int i = minY; i <= maxY; i++ )
        {
            for (int  j = minX; j <= maxX; j++ )
            {
                if ( robo_map.MapViewMask & scene.ViewMask({j, i}) )
                {
                    scene.ForEachUnitIn({j, i}, [&](const MapScene::Unit &bct)
                    {
                        if ( bct.Type != BACT_TYPES_MISSLE && bct.Type != BACT_TYPES_ROBO )
                        {
                            if ( bct.Status != BACT_STATUS_CREATE && bct.Status != BACT_STATUS_BEAM && bct.Status != BACT_STATUS_DEAD )
                            {
                                if ( bct.Type != BACT_TYPES_GUN || !bct.RoboGun )
                                    yw_RenderCursorOverUnit(yw, bct);

                                if ( yw->_isNetGame && bct.Bact )
                                {
                                    if ( bct.StatusFlags & BACT_STFLAG_ISVIEW )
                                    {
                                        if ( yw->_GameShell )
                                            DrawNetPlayerName(yw, cur, bct.Bact);
                                    }
                                }
                            }
                        }
                    });
                }
            }
        }
    }
}

THudSubject HudSubjectOf(NC_STACK_ypaworld *yw)
{
    NC_STACK_ypabact *unit = yw->UserUnit();

    THudSubject subject;
    subject.Vehicle = unit->_vehicleID;
    subject.Position = unit->_position;
    subject.Rotation = yw->_viewerRotation;
    subject.Energy = unit->_energy;
    subject.EnergyMax = unit->_energy_max;
    subject.Shield = unit->_shield;
    subject.SectorOwner = unit->_pSector->owner;

    if ( unit->_weapon != -1 )
        subject.ReloadPercent = WeaponReloadPercent(yw->_weaponProtos.at(unit->_weapon), unit->_clock - unit->_weapon_time, unit->_salve_counter);

    if ( unit->_parent )
    {
        subject.HasParent = true;
        subject.ParentPos = unit->_parent->_position;
    }

    if ( unit->IsCommander() )
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            subject.HasTarget = true;
            subject.TargetPos = unit->_primTpos;
        }
        else if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            subject.HasTarget = true;
            subject.TargetPos = unit->_primT.pbact->_position;
        }
    }

    subject.Visor = yw->_guiVisor;

    if ( subject.Visor.target )
    {
        subject.VisorTarget = subject.Visor.target->_gid;
        subject.VisorTargetPos = subject.Visor.target->_position;
    }

    return subject;
}

void DrawHudDestroyed(NC_STACK_ypaworld *yw, CmdStream *buf, int textId)
{
    std::string msg = Locale::Text::Common(textId);

    if ( yw->_timeStamp / 500 & 1 )
    {
        FontUA::select_tileset(buf, 15);
        FontUA::set_xpos(buf, 0);
        FontUA::set_center_ypos(buf, -(yw->_fontH / 2));

        FontUA::set_txtColor(buf, 255, 255, 255);

        FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], buf, msg, yw->_screenSize.x);
    }
}

void DrawHudOf(NC_STACK_ypaworld *yw, CmdStream *buf, const THudSubject &unit)
{
    if ( unit.Energy >= unit.EnergyMax )
    {
        yw->_hud.pulse_period = 3000;
    }
    else
    {
        yw->_hud.pulse_period = 3000 * unit.Energy / std::max(unit.EnergyMax, 1);

        if ( yw->_hud.pulse_period < 200 )
            yw->_hud.pulse_period = 200;
    }

    yw->_hud.pulse_phase += (float)yw->_frameTime / (float)yw->_hud.pulse_period;

    if ( yw->_hud.pulse_phase >= 1.0 )
        yw->_hud.pulse_phase = 0;

    yw_RenderHUDVectorGFX(yw, buf, unit);
}

void DrawHud(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    if ( yw->UserUnit()->_status == BACT_STATUS_DEAD )
    {
        // Not while the level's units are cleaned away
        NC_STACK_ypabact *home = yw->UserRobo() ? yw->UserRobo() : yw->UserUnit();

        // The player's unit, or with it their faction
        int textId = Locale::CMN_UNITDESTR;

        if ( yw->LocalPlayerDefeated() )
            textId = yw->UserRobo() ? Locale::CMN_HSDESTR : Locale::CMN_ALLDESTR;

        if ( !(home->_status_flg & BACT_STFLAG_CLEAN) )
            DrawHudDestroyed(yw, &buf, textId);
    }
    else if ( yw->UserUnit() != yw->UserRobo() && yw->_hud.visible )
    {
        yw_RenderHUDRadare(yw);
        DrawHudOf(yw, &buf, HudSubjectOf(yw));
    }

    if ( yw->_guiActFlags & 0x20 )
    {
        MapScene::Unit hovered = yw->UiScene().Fresh(yw->_bactOnMouse);
        yw_RenderCursorOverUnit(yw, hovered);
        yw_RenderUnitLifeBar(yw, &buf, hovered);
    }

    if ( yw->_guiVisor.target )
        yw_RenderUnitLifeBar(yw, &buf, LiveMapUnit(yw, yw->_guiVisor.target));

    yw_RenderOverlayCursors(yw, &buf);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);
}



void HudColorRadialPulse(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float ringPos = yw->_hud.pulse_phase * 2.0 - 1.0;

    float blend1 = fabs( ringPos - sqrt(POW2(x1) + POW2(y1)) );
    float blend2 = fabs( ringPos - sqrt(POW2(x2) + POW2(y2)) );

    if ( blend1 > 1.0)
        blend1 = 1.0;
    else if ( blend1 < 0.0)
        blend1 = 0.0;

    if ( blend2 > 1.0)
        blend2 = 1.0;
    else if ( blend2 < 0.0)
        blend2 = 0.0;

    int g1 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend1 + yw->_hud.cl1_g;
    int r1 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend1 + yw->_hud.cl1_r;
    int b1 =  (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend1 + yw->_hud.cl1_b;
    *out1 = GFX::Engine.Color(r1, g1, b1);

    int g2 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * blend2 + yw->_hud.cl1_g;
    int r2 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * blend2 + yw->_hud.cl1_r;
    int b2 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * blend2 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(r2, g2, b2);
}

void HudColorTimeBlend(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    int cycleMs = yw->_timeStamp % 300;
    // TODO(bug): This blend factor is at most 0.01, so the colour hardly changes.
    // Maybe meant / 300.0; check against the original.
    float blend = cycleMs / 30000.0;

    THudState *hud = &yw->_hud;

    int g1 = (hud->cl2_g - hud->cl1_g) * blend + hud->cl1_g;
    int r1 = (hud->cl2_r - hud->cl1_r) * blend + hud->cl1_r;
    int b1 = (hud->cl2_b - hud->cl1_b) * blend + hud->cl1_b;
    *out1 = GFX::Engine.Color(r1, g1, b1);

    int r2 = (hud->cl2_r - hud->cl1_r) * blend + hud->cl1_r;
    int b2 = (hud->cl2_b - hud->cl1_b) * blend + hud->cl1_b;
    int g2 = (hud->cl2_g - hud->cl1_g) * blend + hud->cl1_g;
    *out2 = GFX::Engine.Color(r2, g2, b2);
}

void yw_RenderHUDTarget(NC_STACK_ypaworld *yw, THudState *wis, const THudSubject &unit)
{
    UAskeleton::Data *mg_wure = NULL;
    UAskeleton::Data *hud_wure = NULL;
    UAskeleton::Data *wpn_wure = NULL;
    UAskeleton::Data *wpn_wure2 = NULL;

    const THudVisor &visor = unit.Visor;

    float openProgress = (yw->_timeStamp - wis->switch_time - 350) / 200.0;

    if ( openProgress > 0.0 )
    {
        World::TVhclProto *vhcl = &yw->_vhclProtos[ unit.Vehicle ];

        if ( vhcl->hud_wireframe )
            hud_wure = vhcl->hud_wireframe->GetSkelet();
        else
            hud_wure = NULL;

        if ( vhcl->mg_wireframe )
            mg_wure = vhcl->mg_wireframe->GetSkelet();
        else
            mg_wure = wis->sklts_intern[4];

        if ( vhcl->wpn_wireframe_1 )
            wpn_wure = vhcl->wpn_wireframe_1->GetSkelet();
        else if ( visor.weaponSight == 2)
            wpn_wure = wis->sklts_intern[5];
        else if ( visor.weaponSight == 3 )
            wpn_wure = wis->sklts_intern[6];
        else if ( visor.weaponSight == 4 )
            wpn_wure = wis->sklts_intern[7];

        if ( vhcl->wpn_wireframe_2 )
            wpn_wure2 = vhcl->wpn_wireframe_2->GetSkelet();
        else if ( visor.weaponSight == 2)
            wpn_wure2 = wis->sklts_intern[9];
        else if ( visor.weaponSight == 3 )
            wpn_wure2 = wis->sklts_intern[10];
        else if ( visor.weaponSight == 4 )
            wpn_wure2 = wis->sklts_intern[11];

        if ( unit.VisorTarget )
        {
            if ( unit.VisorTarget != wis->target )
            {
                wis->target = unit.VisorTarget;
                wis->target_time = yw->_timeStamp;
            }
        }
        else
        {
            wis->target = 0;
        }

        if ( hud_wure )
        {
            SDL_Color hudColor = yw->GetColor(34);
            yw_RenderVector2D(yw, hud_wure, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.99, 0.99, hudColor, NULL, NULL, true);
        }

        if ( visor.mgunSight )
        {
            if ( mg_wure )
            {
                SDL_Color mgColor2 = yw->GetColor(26);
                SDL_Color mgColor = yw->GetColor(35);

                float mgScaleX = 0.3;

                if ( openProgress < 1.4 )
                    mgScaleX = openProgress * 0.3;

                wis_color_func func;

                if ( wis->color_fx )
                    func = HudColorRadialPulse;
                else
                    func = NULL;

                THudState *hud = &yw->_hud;

                hud->cl1_r = mgColor.r;
                hud->cl1_g = mgColor.g;
                hud->cl1_b = mgColor.b;
                hud->cl2_r = mgColor2.r;
                hud->cl2_g = mgColor2.g;
                hud->cl2_b = mgColor2.b;

                yw_RenderVector2D(yw, mg_wure, visor.mgunX, visor.mgunY, 1.0, 0.0, 0.0, 1.0, mgScaleX, 0.4, mgColor, func, NULL, true);
            }
        }

        if ( visor.weaponSight )
        {
            float lockProgress[2];
            wis_color_func func;

            SDL_Color sightColor;
            SDL_Color sightColor2;

            if ( unit.VisorTarget )
            {
                for (int i = 0; i < 2; i++)
                {
                    lockProgress[i] = (yw->_timeStamp - wis->target_time) / 200.0;

                    if ( lockProgress[i] > 1.0 )
                        lockProgress[i] = 1.0;
                }

                sightColor = yw->GetColor(27);
                sightColor2 = yw->GetColor(36);

                if ( wis->color_fx )
                    func = HudColorTimeBlend;
                else
                    func = NULL;
            }
            else
            {
                lockProgress[0] = 0;
                lockProgress[1] = 0;

                sightColor2 = yw->GetColor(28);
                sightColor = yw->GetColor(37);

                if ( wis->color_fx )
                    func = HudColorRadialPulse;
                else
                    func = NULL;
            }

            int r1 = sightColor.r;
            int g1 = sightColor.g;
            int b1 = sightColor.b;
            int r2 = sightColor2.r;
            int b2 = sightColor2.b;
            int g2 = sightColor2.g;

            for (int i = 0; i < 2; i++)
            {
                float sightX, sightY;

                if ( unit.VisorTarget )
                {
                    sightX = (visor.weaponX - visor.mgunX) * lockProgress[i] + visor.mgunX;
                    sightY = (visor.weaponY - visor.mgunY) * lockProgress[i] + visor.mgunY;
                }
                else
                {
                    sightX = visor.weaponX;
                    sightY = visor.weaponY;
                }

                float scaleY = lockProgress[i] * -0.23 + 0.4;
                float scaleX = lockProgress[i] * -0.17 + 0.3;

                if ( openProgress < 1.4 )
                    scaleY = scaleY * openProgress;

                float angle;

                if ( i & 1 )
                {
                    angle = lockProgress[i] * 1.571;
                }
                else if ( lockProgress[i] >= 1.0 )
                {
                    angle = (yw->_timeStamp - wis->target_time) * 0.001 * 6.282;
                }
                else
                {
                    angle = lockProgress[i] * -3.141;
                }

                float sinA = sin(angle);
                float cosA = cos(angle);

                UAskeleton::Data *sightWire;

                if ( i & 1 )
                    sightWire = wpn_wure;
                else
                    sightWire = wpn_wure2;

                if ( sightWire )
                {
                    yw->_hud.cl1_r = r1;
                    yw->_hud.cl1_g = g1;
                    yw->_hud.cl1_b = b1;
                    yw->_hud.cl2_r = r2;
                    yw->_hud.cl2_g = g2;
                    yw->_hud.cl2_b = b2;

                    yw_RenderVector2D(yw, sightWire, sightX, sightY, cosA, -sinA, sinA, cosA, scaleX, scaleY, sightColor, func, 0, true);
                }
            }
        }
    }
}

void yw_RenderCursorOverUnit(NC_STACK_ypaworld *yw, const MapScene::Unit &bact)
{
    float dx = bact.Position.x - yw->_viewerPosition.x;
    float dy = bact.Position.y - yw->_viewerPosition.y;
    float dz = bact.Position.z - yw->_viewerPosition.z;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    float viewX  = corrected.m00 * dx + corrected.m01 * dy + corrected.m02 * dz;
    float viewY = corrected.m10 * dx + corrected.m11 * dy + corrected.m12 * dz;
    float viewZ = corrected.m20 * dx + corrected.m21 * dy + corrected.m22 * dz;

    if ( viewZ > 30.0 && viewX < viewZ )
    {
        if ( viewX > -viewZ && viewY < viewZ && viewY > -viewZ )
        {
            SDL_Color ownerColor = yw->GetColor(bact.Owner);
            UAskeleton::Data *cursorWire = yw->_hud.sklts_intern[13];

            float screenX = viewX / viewZ;
            float screenY = viewY / viewZ;

            if ( cursorWire )
            {
                float cursorY = screenY - 0.08;

                if ( bact.Commander && bact.Owner == robo_map.Scene->PlayerOwner() )
                {
                    yw_RenderVector2D(yw, cursorWire, screenX, cursorY, 1.0, 0.0, 0.0, 1.0, 0.015, 0.02, ownerColor, NULL, NULL, true);
                    yw_RenderVector2D(yw, cursorWire, screenX, cursorY, 1.0, 0.0, 0.0, 1.0, 0.005, 0.00666, ownerColor, NULL, NULL, true);
                }
                else
                {
                    yw_RenderVector2D(yw, cursorWire, screenX, cursorY, 1.0, 0.0, 0.0, 1.0, 0.0075, 0.01, ownerColor, NULL, NULL, true);
                }
            }
        }
    }
}









void DrawRadarUnits(NC_STACK_ypaworld *yw)
{
    // Render units on radare

    CmdStream buf;
    buf.reserve(8192);
    
    FontUA::select_tileset(&buf, 61);

    float halfW = yw->_screenSize.x / 2;
    float halfH = yw->_screenSize.y / 2;

    robo_map.viewWidth = dround((robo_map.radarRight - robo_map.radarLeft) * halfW);
    robo_map.viewHeight = dround((robo_map.radarBottom - robo_map.radarTop) * halfH);

    robo_map.scrollPixelX = dround(robo_map.viewCenterX / robo_map.unitsPerPixelX) - (robo_map.viewWidth / 2);
    robo_map.scrollPixelY = -dround(robo_map.viewCenterZ / robo_map.unitsPerPixelY) - (robo_map.viewHeight / 2);

    robo_map.viewScreenX = dround(halfW * robo_map.radarLeft);
    robo_map.viewScreenY = dround(halfH * robo_map.radarTop);

    Common::Rect drect;
    drect.left = robo_map.viewScreenX;
    drect.top = robo_map.viewScreenY;
    drect.right = robo_map.viewWidth + robo_map.viewScreenX - 1;
    drect.bottom = robo_map.viewHeight + robo_map.viewScreenY - 1;

    GFX::Engine.SetClipCentered(drect);

    int minX = dround(robo_map.scrollPixelX * robo_map.unitsPerPixelX) / World::CVSectorLength;
    int minY = dround(robo_map.scrollPixelY * robo_map.unitsPerPixelY) / World::CVSectorLength;
    int maxX = dround( (robo_map.viewWidth + robo_map.scrollPixelX) * robo_map.unitsPerPixelX ) / World::CVSectorLength;
    int maxY = dround( (robo_map.viewHeight + robo_map.scrollPixelY) * robo_map.unitsPerPixelY ) / World::CVSectorLength;

    if ( minX < 1 )
        minX = 1;

    if ( minY < 1 )
        minY = 1;

    if ( maxX >= yw->_mapSize.x )
        maxX = yw->_mapSize.x - 1;

    if ( maxY >= yw->_mapSize.y )
        maxY = yw->_mapSize.y - 1;

    int drawnCount = 0;
    const MapScene &scene = *robo_map.Scene;

    for (int i = minY; i <= maxY; i++ )
    {
        for (int j = minX; j <= maxX; j++ )
        {
            if ( drawnCount < 512 ) // Units maximum?
            {
                if ( robo_map.MapViewMask & scene.ViewMask({j, i}) )
                {
                    scene.ForEachUnitIn({j, i}, [&](const MapScene::Unit &bact)
                    {
                        int underMap = 0;
                        if ( robo_map.IsOpen() )
                        {
                            Common::PointRect r { robo_map.x - (yw->_screenSize.x / 2)
                                                , robo_map.y - (yw->_screenSize.y / 2)
                                                , robo_map.w
                                                , robo_map.h };

                            if ( r.IsIn( RoboMapWorldToScreen( bact.Position.XZ() ) ) )
                                underMap = 1;
                        }

                        if ( bact.Type == BACT_TYPES_ROBO )
                        {
                            if ( !underMap )
                            {
                                FontUA::select_tileset(&buf, 1);

                                RoboMapDrawUnit(yw, &buf, yw->_guiTiles[1]->h, yw->_guiTiles[1]->map[24].w, bact, 0);

                                FontUA::select_tileset(&buf, 61);

                                drawnCount++;
                            }
                        }
                        else if ( bact.Type == BACT_TYPES_GUN )
                        {
                            if ( (!bact.RoboGun || bact.Id == scene.UserUnitId()) && !underMap )
                            {
                                RoboMapDrawUnit(yw, &buf, 7, 7, bact, 0);

                                drawnCount++;
                            }
                        }
                        else if ( !underMap )
                        {
                            RoboMapDrawUnit(yw, &buf, 7, 7, bact, 0);

                            drawnCount++;
                        }
                    });
                }
            }
        }
    }

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);
}

void yw_RenderHUDRadare(NC_STACK_ypaworld *yw)
{
    float savedCenterX = robo_map.viewCenterX;
    float savedCenterZ = robo_map.viewCenterZ;
    float savedUnitsPerPixelX = robo_map.unitsPerPixelX;
    float savedUnitsPerPixelY = robo_map.unitsPerPixelY;
    int savedLayerFlags = robo_map.layerFlags;
    int32_t savedViewMask = robo_map.MapViewMask;
    int savedZoom = robo_map.zoomLevel;

    // And where the map's view is on the screen, which finding a unit
    // under the pointer on the map goes by too
    int savedViewWidth = robo_map.viewWidth;
    int savedViewHeight = robo_map.viewHeight;
    int savedScrollPixelX = robo_map.scrollPixelX;
    int savedScrollPixelY = robo_map.scrollPixelY;
    int savedViewScreenX = robo_map.viewScreenX;
    int savedViewScreenY = robo_map.viewScreenY;

    robo_map.layerFlags = 1;
    robo_map.MapViewMask = -1;
    robo_map.stateFlags |= 0x100;
    robo_map.zoomLevel = 3;

    vec3d viewer;
    robo_map.Scene->ViewerPosition(viewer);
    robo_map.viewCenterX = viewer.x;
    robo_map.viewCenterZ = viewer.z;
    robo_map.unitsPerPixelX = 4800.0 / ((float)(yw->_screenSize.x / 2) * (robo_map.radarRight - robo_map.radarLeft));
    robo_map.unitsPerPixelY = 4800.0 / ((float)(yw->_screenSize.y / 2) * (robo_map.radarBottom - robo_map.radarTop));

    mat3x3 view = robo_map.Scene->ViewerRotation();
    float fwdX = view.m20;
    float fwdZ = view.m22;

    float rotCos, rotSin;

    float fwdLen = sqrt( POW2(fwdZ) + POW2(fwdX));
    if ( fwdLen <= 0.0 )
    {
        rotCos = 1.0;
        rotSin = 0.0;
    }
    else
    {
        rotSin = fwdX / fwdLen;
        rotCos = fwdZ / fwdLen;
    }
    robo_map.radarRotM00 = rotCos;

    radarRotM01 = -rotSin;
    radarRotM10 = rotSin;
    radarRotM11 = rotCos;

    DrawRadarUnits(yw);

    robo_map.layerFlags = savedLayerFlags;
    robo_map.zoomLevel = savedZoom;
    robo_map.viewCenterX = savedCenterX;
    robo_map.MapViewMask = savedViewMask;
    robo_map.viewCenterZ = savedCenterZ;
    robo_map.unitsPerPixelX = savedUnitsPerPixelX;
    robo_map.stateFlags &= 0xFFFFFEFF;
    robo_map.unitsPerPixelY = savedUnitsPerPixelY;

    robo_map.viewWidth = savedViewWidth;
    robo_map.viewHeight = savedViewHeight;
    robo_map.scrollPixelX = savedScrollPixelX;
    robo_map.scrollPixelY = savedScrollPixelY;
    robo_map.viewScreenX = savedViewScreenX;
    robo_map.viewScreenY = savedViewScreenY;
}


struct aab
{
    int a;
    int b;
};

int CompareScoresDesc(const void *lhs, const void *rhs)
{
    return (((const aab *)rhs)->b) - (((const aab *)lhs)->b);
}

void DrawNetScores(NC_STACK_ypaworld *yw, CmdStream *cur, const std::array<World::TPlayerStatus, World::CVFractionsCount> &statuses, int width)
{
    if ( yw->_isNetGame )
    {
        aab scores[8];

        int nums = 0;

        for (int i = 0; i < 8; i++)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
            {
                scores[nums].a = i;
                scores[nums].b = statuses[i].Score;
                nums++;
            }
        }

        qsort(scores, nums, sizeof(aab), CompareScoresDesc);

        for (int i = 0; i < nums; i++)
        {
            int clrid;
            
            FontUA::ColumnItem columns[2];

            switch ( scores[i].a )
            {
            case 1:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_RESIST);
                clrid = 1;
                break;

            case 2:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_SULG);
                clrid = 2;
                break;

            case 3:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_MYKO);
                clrid = 3;
                break;

            case 4:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_TAER);
                clrid = 4;
                break;

            case 5:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_BLKSCT);
                clrid = 5;
                break;

            case 6:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_GHOR);
                clrid = 6;
                break;

            default:
                columns[0].txt = Locale::Text::Advanced(Locale::ADV_NEUTRAL);
                clrid = 7;
                break;
            }

            FontUA::set_txtColor(cur, yw->_iniColors[clrid].r, yw->_iniColors[clrid].g, yw->_iniColors[clrid].b);

            columns[0].spaceChar = 32;
            columns[0].prefixChar = 0;
            columns[0].postfixChar = 0;
            columns[0].fontID = 15;
            columns[0].flags = 36;
            columns[0].width = width * 0.5;

            columns[1].txt = fmt::sprintf("%d", statuses[ scores[i].a ].Score);
            columns[1].width = width * 0.5;
            columns[1].fontID = 15;
            columns[1].spaceChar = 32;
            columns[1].flags = 36;
            columns[1].prefixChar = 0;
            columns[1].postfixChar = 0;

            FormateColumnItem(yw, cur, 2, columns);

            FontUA::next_line(cur);
        }

        FontUA::next_line(cur);
    }
}


void DrawUnitLimit(NC_STACK_ypaworld *yw, CmdStream *cur, int width)
{
    if ( yw->_isNetGame )
    {
        if ( yw->_countUnitsPerOwner[ yw->GetPlayerOwner() ] <= yw->_levelUnitLimit )
            FontUA::set_txtColor(cur, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);
        else
            FontUA::set_txtColor(cur, yw->_iniColors[6].r, yw->_iniColors[6].g, yw->_iniColors[6].b);

        FontUA::ColumnItem columns[2];

        columns[0].txt = Locale::Text::Advanced(Locale::ADV_UNITS);
        columns[0].width = width * 0.5;
        columns[0].spaceChar = 32;
        columns[0].fontID = 15;
        columns[0].prefixChar = 0;
        columns[0].postfixChar = 0;
        columns[0].flags = 36;

        columns[1].txt = fmt::sprintf("%d / %d", yw->_countUnitsPerOwner[ yw->GetPlayerOwner() ], yw->_levelUnitLimit);
        columns[1].width = width * 0.5;
        columns[1].fontID = 15;
        columns[1].postfixChar = 0;
        columns[1].spaceChar = 32;
        columns[1].flags = 36;
        columns[1].prefixChar = 0;

        FormateColumnItem(yw, cur, 2, columns);

        FontUA::next_line(cur);
    }
}

void DrawGameStatus(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    int textX = 2 * yw->_screenSize.x / 3;
    int columnWidth = yw->_screenSize.x - textX;

    FontUA::select_tileset(&buf, 15);
    FontUA::set_xpos(&buf, textX);
    FontUA::set_ypos(&buf, up_panel.panelHeight + (yw->_fontH / 2));

    DrawUnitLimit(yw, &buf, columnWidth);
    DrawNetScores(yw, &buf, yw->_gameplayStats, columnWidth);

    for (const TMapSuperItem &sitem : yw->_levelInfo.SuperItems)
    {
        if ( sitem.Type != 0 )
        {
            std::string timeStr;
            std::string typeStr = "SUPER ITEM";

            if (sitem.Type == TMapSuperItem::TYPE_BOMB)
                typeStr = Locale::Text::Common(Locale::CMN_BOMBNAME);
            else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
                typeStr = Locale::Text::Common(Locale::CMN_WAVENAME);

            int hasText = 0;

            if ( sitem.State == TMapSuperItem::STATE_ACTIVE || sitem.State == TMapSuperItem::STATE_STOPPED )
            {
                int secondsLeft = (sitem.CountDown + 1023) / 1024;

                if ( secondsLeft < 0 )
                    secondsLeft = 0;

                timeStr = fmt::sprintf("%s: %02d:%02d", typeStr, secondsLeft / 60 % 60, secondsLeft % 60);

                hasText = 1;
            }
            else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
            {
                timeStr = fmt::sprintf("%s: %s", typeStr,  Locale::Text::Advanced(Locale::ADV_TRIGGERED) );

                hasText = 1;
            }

            if ( hasText )
            {
                FontUA::set_xpos(&buf, textX);

                FontUA::set_txtColor(&buf, yw->_iniColors[sitem.ActivateOwner].r, yw->_iniColors[sitem.ActivateOwner].g, yw->_iniColors[sitem.ActivateOwner].b);

                FontUA::FormateClippedText(yw->_guiTiles[15], &buf, timeStr.c_str(), yw->_screenSize.x - textX, 32);

                FontUA::next_line(&buf);
            }
        }
    }

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);
}




void RestoreWinStatus(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2)
{
    if ( bact2->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_roboMapStatus.Valid )
        {
            if ( yw->_roboMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_roboMapStatus.Rect.x;
            robo_map.y = yw->_roboMapStatus.Rect.y;
            robo_map.w = yw->_roboMapStatus.Rect.w;
            robo_map.h = yw->_roboMapStatus.Rect.h;
            robo_map.layerFlags = yw->_roboMapStatus.Data[0];
            robo_map.lockViewer = yw->_roboMapStatus.Data[1];
            robo_map.zoomLevel = yw->_roboMapStatus.Data[2];
            robo_map.altX = yw->_roboMapStatus.Data[3];
            robo_map.altY = yw->_roboMapStatus.Data[4];
            robo_map.altW = yw->_roboMapStatus.Data[5];
            robo_map.altH = yw->_roboMapStatus.Data[6];

            RoboMapZoom(yw, 0);
        }

        if ( yw->_roboFinderStatus.Valid )
        {
            if ( yw->_roboFinderStatus.IsOpen )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            squadron_manager.x = yw->_roboFinderStatus.Rect.x;
            squadron_manager.y = yw->_roboFinderStatus.Rect.y;
            squadron_manager.w = yw->_roboFinderStatus.Rect.w;
            squadron_manager.h = yw->_roboFinderStatus.Rect.h;

            squadron_manager.SetRect(yw, -2, -2);
        }
    }
    else if ( bact1->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_vhclMapStatus.Rect.x;
            robo_map.y = yw->_vhclMapStatus.Rect.y;
            robo_map.w = yw->_vhclMapStatus.Rect.w;
            robo_map.h = yw->_vhclMapStatus.Rect.h;
            robo_map.layerFlags = yw->_vhclMapStatus.Data[0];
            robo_map.lockViewer = yw->_vhclMapStatus.Data[1];
            robo_map.zoomLevel = yw->_vhclMapStatus.Data[2];
            robo_map.altX = yw->_vhclMapStatus.Data[3];
            robo_map.altY = yw->_vhclMapStatus.Data[4];
            robo_map.altW = yw->_vhclMapStatus.Data[5];
            robo_map.altH = yw->_vhclMapStatus.Data[6];

            RoboMapZoom(yw, 0);
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            squadron_manager.x = yw->_vhclFinderStatus.Rect.x;
            squadron_manager.y = yw->_vhclFinderStatus.Rect.y;
            squadron_manager.w = yw->_vhclFinderStatus.Rect.w;
            squadron_manager.h = yw->_vhclFinderStatus.Rect.h;

            squadron_manager.SetRect(yw, -2, -2);
        }
    }
    else if ( bact1->_status == BACT_STATUS_DEAD )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
        }

        if ( bact2->_bact_type != BACT_TYPES_ROBO )
            yw->_invulnerable = 1;
    }

    if ( bact2->_bact_type != BACT_TYPES_ROBO )
        robo_map.lockViewer = 1;
}










int NC_STACK_ypaworld::yw_MouseFindCreationPoint(TClickBoxInf *winp)
{
    const World::TVhclProto &vhcl = _vhclProtos[bzda.vhclProtoIds[bzda.selVhclIdx]];

    vec3d mouseDir( (float)(winp->move.ScreenPos.x - (_screenSize.x / 2)) / (float)(_screenSize.x / 2),
               (float)(winp->move.ScreenPos.y - (_screenSize.y / 2)) / (float)(_screenSize.y / 2),
               1.0 );

    mouseDir.normalise();

    float searchDist = vhcl.radius * 4.0 + 200.0;

    float ringRadius = 0.0;

    if ( searchDist > 0.0 )
    {
        vec3d centerVec = mouseDir * searchDist;

        while ( ringRadius < searchDist )
        {
            float angle = 0.0;

            while ( angle < C_2PI )
            {

                vec3d probeView = centerVec + vec3d(ringRadius * cos(angle),  ringRadius * sin(angle),  0.0);

                mat3x3 corrected = _viewerRotation;
                GFX::Engine.matrixAspectCorrection(corrected, true);

                vec3d probeWorld = corrected.Transpose().Transform(probeView);

                TRayIntersection ray;
                ray.stPos = _viewerPosition;
                ray.vect = probeWorld;
                ray.flags = 0;

                IntersectLongRay(&ray);

                if ( !ray.isect )
                {
                    float probeLen = probeWorld.length();

                    if ( probeLen > 0.0 )
                        probeWorld /= probeLen;

                    _mouseVector = probeWorld;
                    _mouseSelDistance = probeLen;

                    return 1;
                }

                angle += 0.349;
            }

            ringRadius += searchDist * 0.1;
        }
    }

    return 0;
}

int NC_STACK_ypaworld::UserActCreateCheck(TClickBoxInf *winp)
{
    const MapScene &scene = UiScene();
    MapScene::Unit robo;
    MapScene::Batteries batteries;

    if ( _guiActFlags & 8 || bzda.selVhclIdx == -1 || !scene.UserRobo(robo) || !scene.HostStationBatteries(robo.Id, batteries) )
        return 4;

    if ( _actionEnergyCost > batteries.Create )
        return 2;


    if ( !yw_MouseFindCreationPoint(winp) )
        return 1;

    if ( _isNetGame && !_levelUnitLimitType && _levelUnitLimit <= _countUnitsPerOwner[robo.Owner] )
        return 3;

    return 0;
}

int NC_STACK_ypaworld::UserActBuildCheck()
{
    const MapScene &scene = UiScene();
    MapScene::Unit robo;

    if ( !(_guiActFlags & 0x10) || bzda.selBuildIdx == -1 || !scene.UserRobo(robo) || robo.Type != BACT_TYPES_ROBO )
        return 1; // NO with no tip

    return CheckBuild(scene, robo, _cellOnMouse, _actionEnergyCost);
}

int NC_STACK_ypaworld::CheckBuild(const MapScene &scene, const MapScene::Unit &robo, const Common::Point &sector, int cost)
{
    MapScene::Batteries batteries;

    if ( !IsSector(sector) || !scene.HostStationBatteries(robo.Id, batteries) )
        return 1;

    if ( cost > batteries.Create )
        return 6; // TIP_NOENERGY

    uint8_t owner = scene.Owner(sector);

    if ( owner && owner != robo.Owner )
        return 2; // TIP_NOTOWN

    Common::Point cellDist = sector.AbsDistance( robo.Sector );

    if ( cellDist.IsNull() )
        return 3; // TIP_BUILD_TOOCLOSE

    if ( cellDist.x > 1 || cellDist.y > 1 )
        return 4; // TIP_BUILD_TOOFAR

    uint8_t purpose = scene.Purpose(sector);

    if ( purpose == cellArea::PT_CONSTRUCTING ||
         (!_allowMultiBuildLevel && scene.Constructing(robo.Owner)) )
        return 5; // TIP_BUILD_INPROC

    if ( purpose == cellArea::PT_TECHUPGRADE || 
         purpose == cellArea::PT_GATECLOSED || 
         purpose == cellArea::PT_GATEOPENED || 
         purpose == cellArea::PT_STOUDSON || 
         (purpose == cellArea::PT_TECHDEACTIVE && _isNetGame) )
        return 1; // NO with no tip

    return 0; // Can build
}


int NC_STACK_ypaworld::UserActBeamCheck()
{
    const MapScene &scene = UiScene();
    MapScene::Unit robo;

    if ( !(_guiActFlags & 0x10) || !scene.UserRobo(robo) )
        return 6;

    // Where on the ground it was asked for, which the order goes by
    _beamGroundPos = _cellMouseIsectPos;

    int cost = -1;
    int result = CheckBeam(scene, robo, _cellOnMouse, &_cellMouseIsectPos, &cost);

    if ( cost >= 0 )
        _actionEnergyCost = cost;

    return result;
}

int NC_STACK_ypaworld::CheckBeam(const MapScene &scene, const MapScene::Unit &robo, const Common::Point &sector, vec3d *point, int *cost)
{
    MapScene::Batteries batteries;

    if ( !IsSector(sector) || !scene.HostStationBatteries(robo.Id, batteries) )
        return 6;

    if ( !scene.CanSee(sector, robo.Owner) )
        return 3;

    uint8_t purpose = scene.Purpose(sector);

    if ( purpose == cellArea::PT_CONSTRUCTING )
        return 6;

    float dx = point->x - robo.Position.x;
    float dy = point->y - robo.Position.y;
    float dz = point->z - robo.Position.z;

    float dist = sqrt(POW2(dx) + POW2(dy) + POW2(dz));

    if ( dist < 100.0 )
        return 4;

    int beamCost = 0;

    if ( purpose != cellArea::PT_GATEOPENED && (purpose != cellArea::PT_POWERSTATION || scene.Owner(sector) != robo.Owner) )
    {
        beamCost = (POW2(dist) / 230.4);
    }

    int availEnergy = batteries.Beam;

    *cost = beamCost;

    if ( beamCost > availEnergy )
        return 1;

    bool enemyHost = false;

    scene.ForEachUnitIn(sector, [&](const MapScene::Unit &unit)
    {
        if ( unit.Type == BACT_TYPES_ROBO && unit.Status != BACT_STATUS_DEAD && unit.Owner != robo.Owner )
            enemyHost = true;
    });

    if ( enemyHost )
        return 5;

    float minY = 100000.0;
    float maxY = -100000.0;

    TRayIntersection ray;
    ray.vect.x = 0;
    ray.vect.y = 50000.0;
    ray.vect.z = 0;
    ray.flags = 0;

    for (int j = -3; j <= 3; j++)
    {
        for (int i = -3; i <= 3; i++)
        {
            ray.stPos.y = -25000.0;
            ray.stPos.x = point->x - j * 57.14285714285715;
            ray.stPos.z = point->z - i * 57.14285714285715;

            IntersectRay(&ray);

            if ( ray.isect )
            {
                if ( ray.isectPos.y < minY )
                    minY = ray.isectPos.y;

                if ( ray.isectPos.y > maxY )
                    maxY = ray.isectPos.y;
            }
        }
    }

    if ( abs(maxY - minY) > 800 )
        return 2;

    if ( point->x < 1500.0 || point->z > -1500.0 || point->x > _mapLength.x - 1500.0 || point->z < 1500.0 - _mapLength.y )
        return 2;

    point->y = minY - robo.Height;
    return 0;
}

int NC_STACK_ypaworld::HandleWaypointMode(TInputState *arg, int action)
{
    if ( !_makingWaypointsMode && arg->Buttons.Is(4) )
    {
        _makingWaypointsMode = true;
        _waypointCount = 0;
        return action;
    }

    if ( _makingWaypointsMode && !(arg->Buttons.Is(4)) )
    {
        _makingWaypointsMode = false;
        return action;
    }

    if ( _makingWaypointsMode   &&   action == World::DOACTION_2 &&   arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        if ( _waypointCount )
        {
            if ( (_firstWaypointPosition.XZ() - _cellMouseIsectPos.XZ()).length() < 100.0 )
            {
                _waypointCount++;
                return World::DOACTION_WP_CYCLE;
            }

            _waypointCount++;

            return World::DOACTION_WP_ADD;
        }
        else
        {
            _firstWaypointPosition = _cellMouseIsectPos;

            _waypointCount++;

            return World::DOACTION_WP_BEGIN;
        }
    }

    return action;
}

bool NC_STACK_ypaworld::CheckUnitDoubleClick(TClickBoxInf *winp)
{
    if ( winp->flag & TClickBoxInf::FLAG_DBL_CLICK
            && _bactPrevClicked.Id
            && Common::ABS(winp->move.ScreenPos.x - _prevMousePos.x) < 4
            && Common::ABS(winp->move.ScreenPos.y - _prevMousePos.y) < 4 )
    {
        _prevMousePos = Common::Point();

        _bactOnMouse = UiScene().Fresh(_bactPrevClicked);
        _bactPrevClicked = MapScene::Unit();

        _guiActFlags |= 0x20;

        return true;
    }
    else
    {
        if ( (winp->flag & TClickBoxInf::FLAG_LM_DOWN)   &&   _guiActFlags & 0x20   &&   UiScene().PlayerOwner() == _bactOnMouse.Owner )
        {
            _bactPrevClicked = _bactOnMouse;
            _prevMousePos = winp->move.ScreenPos;
        }
        else if ( winp->flag & TClickBoxInf::FLAG_LM_DOWN )
        {
            if ( !(_guiActFlags & 0x20) )
            {
                _bactPrevClicked = MapScene::Unit();
                _prevMousePos = Common::Point();
            }
        }
    }
    return false;
}


void NC_STACK_ypaworld::ComputeMouseSelectFlags(TInputState *arg)
{
    _guiActFlags = 0;

    TClickBoxInf *winp = &arg->ClickInf;

    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_OK )
    {
        if ( winp->selected_btn )
        {
            _guiActFlags |= 4;

            if ( winp->selected_btn == &robo_map )
            {
                if ( winp->selected_btnID == 17 )
                    _guiActFlags |= 8;
            }
            else if ( winp->selected_btn == &squadron_manager )
            {
                if ( winp->selected_btnID >= 8 && (int32_t)(squadron_manager.firstShownEntries + winp->selected_btnID - 8) < squadron_manager.numEntries )
                    _guiActFlags |= 0x40;
            }
        }

        if ( bzda.activeMode == 8 )
            _guiActFlags |= 2;
        else if ( bzda.activeMode == 1 || bzda.activeMode == 2 || bzda.activeMode == 4 || bzda.activeMode == 16 || bzda.activeMode == 32 )
            _guiActFlags |= 0x83;
    }
}

// Where the pointer is on the map's view, in pixels of the whole map at
// its zoom
static Common::Point RoboMapPixelAt(const TClickBoxInf *winp)
{
    return Common::Point( winp->move.BtnPos.x + robo_map.viewCenterX / robo_map.unitsPerPixelX - (robo_map.viewWidth / 2),
                          winp->move.BtnPos.y - (robo_map.viewCenterZ / robo_map.unitsPerPixelY) - (robo_map.viewHeight / 2) );
}

vec2d RoboMapWorldAt(const TClickBoxInf *winp)
{
    Common::Point pixel = RoboMapPixelAt(winp);
    return vec2d( (float)pixel.x * robo_map.unitsPerPixelX + 0.5, -((float)pixel.y * robo_map.unitsPerPixelY + 0.75) );
}

bool RoboMapUnitAt(NC_STACK_ypaworld *yw, const TClickBoxInf *winp, MapScene::Unit &out)
{
    const MapScene &scene = *robo_map.Scene;
    Common::Point pixel = RoboMapPixelAt(winp);

    int sectorShift = robo_map.zoomLevel + 2;

    Common::Point pt( pixel.x >> sectorShift, pixel.y >> sectorShift);

    if ( !yw->IsGamePlaySector( pt ) || !scene.CanSee(pt, scene.PlayerOwner()) )
        return false;

    // The player's own first
    bool found = false;

    for ( int pass = 0; pass < 2 && !found; pass++ )
    {
        scene.ForEachUnitIn(pt, [&](const MapScene::Unit &unit)
        {
            if ( found || (unit.Owner == scene.PlayerOwner()) != (pass == 0) )
                return;

            if ( unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_DEAD && unit.Status != BACT_STATUS_BEAM && unit.Type != BACT_TYPES_MISSLE )
            {
                int unitPixelX = unit.Position.x / robo_map.unitsPerPixelX;
                int unitPixelY = unit.Position.z / robo_map.unitsPerPixelY;

                if ( abs(unitPixelX - pixel.x) <= 8 && abs(-unitPixelY - pixel.y) <= 8 )
                {
                    out = unit;
                    found = true;
                }
            }
        });
    }

    return found;
}

void NC_STACK_ypaworld::yw_MAP_MouseSelect(TClickBoxInf *winp)
{
    Common::Point pixel = RoboMapPixelAt(winp);

    int sectorShift = robo_map.zoomLevel + 2;

    Common::Point pt( pixel.x >> sectorShift, pixel.y >> sectorShift);

    if ( IsGamePlaySector( pt ) )
    {
        if ( _guiActFlags & 1 )
        {
            vec2d at = RoboMapWorldAt(winp);

            _cellOnMouse = pt;
            _guiActFlags |= 0x10;
            _cellMouseIsectPos.x = at.x;
            _cellMouseIsectPos.y = _cells( pt ).height;
            _cellMouseIsectPos.z = at.y;
        }

        MapScene::Unit unit;

        if ( _guiActFlags & 2 && !(bzda.activeMode & 0x30) && RoboMapUnitAt(this, winp, unit) )
        {
            _guiActFlags |= 0x20;

            _bactOnMouse = unit;
            _guiActFlags &= ~0x10;
        }
    }
}

void NC_STACK_ypaworld::yw_SMAN_MouseSelect(TClickBoxInf *winp)
{
    if ( _guiActFlags & 2 )
    {
        int squadRow = winp->selected_btnID - 8;

        MapScene::Unit unit;

        if ( SquadManagerPickUnit(squadRow, winp->move.BtnPos.x, unit) )
        {
            if ( unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_DEAD && unit.Status != BACT_STATUS_BEAM )
            {
                _bactOnMouse = unit;
                _guiActFlags |= 0x20;
            }
        }
        else
        {
            unit = squadron_manager.squads[squadRow];

            if ( unit.Id && unit.Status != BACT_STATUS_CREATE && unit.Status != BACT_STATUS_BEAM && unit.Status != BACT_STATUS_DEAD )
            {
                _bactOnMouse = unit;
                _guiActFlags |= 0x20;
            }
        }
    }
}

void NC_STACK_ypaworld::UpdateMouseVector(TClickBoxInf *winp)
{
    float screenX = (float)(winp->move.ScreenPos.x - (_screenSize.x / 2)) / (float)(_screenSize.x / 2);
    float screenY = (float)(winp->move.ScreenPos.y - (_screenSize.y / 2)) / (float)(_screenSize.y / 2);

    mat3x3 corrected = _viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, true);

    float dirX = corrected.m00 * screenX + corrected.m10 * screenY + corrected.m20;
    float dirY = corrected.m01 * screenX + corrected.m11 * screenY + corrected.m21;
    float dirZ = corrected.m02 * screenX + corrected.m12 * screenY + corrected.m22;

    float len = sqrt(POW2(dirX) + POW2(dirY) + POW2(dirZ));

    _mouseVector.x = dirX / len;
    _mouseVector.y = dirY / len;
    _mouseVector.z = dirZ / len;
}

void NC_STACK_ypaworld::yw_3D_MouseSelect(TClickBoxInf *winp)
{
    UpdateMouseVector(winp);

    const MapScene &scene = UiScene();

    // Not the unit looked from, nor from one of its guns the host station
    uint32_t ignored = _playerInHSGun ? scene.UserRoboId() : 0;
    MapScene::Unit hitUnit;
    float hitDistance = 0.0;
    bool hit = UnitOnRay(scene, _viewerPosition, _mouseVector * _normalVizLimit, scene.UserUnitId(), ignored, hitUnit, hitDistance);

    TRayIntersection ray;
    ray.stPos = _viewerPosition;
    ray.vect = _mouseVector * _normalVizLimit;
    ray.flags = 0;

    IntersectLongRay(&ray);

    float groundDist, unitDist;

    if ( ray.isect )
    {
        _guiActFlags |= 0x10;

        _mouseSelDistance = (ray.isectPos - _viewerPosition).length();

        groundDist = _mouseSelDistance;
    }

    if ( hit )
    {
        _guiActFlags |= 0x20;

        _mouseSelDistance = hitDistance;

        unitDist = _mouseSelDistance;
    }

    if ( (_guiActFlags & 0x30) == 0x30 )
    {
        if ( groundDist >= unitDist )
        {
            _mouseSelDistance = unitDist;
            _guiActFlags &= ~0x10;
        }
        else
        {
            _mouseSelDistance = groundDist;
            _guiActFlags &= ~0x20;
        }
    }

    if ( _guiActFlags & 0x10 )
    {
        _cellOnMouse = World::PositionToSectorID(ray.isectPos);
        _cellMouseIsectPos = ray.isectPos;
    }

    if ( _guiActFlags & 0x20 )
    {
        int ignoreUnit = 0;

        if ( hitUnit.Status == BACT_STATUS_CREATE || hitUnit.Status == BACT_STATUS_DEAD || hitUnit.Status == BACT_STATUS_BEAM || hitUnit.Type == BACT_TYPES_MISSLE )
            ignoreUnit = 1;

        if ( ignoreUnit )
            _guiActFlags &= ~0x20;
        else
            _bactOnMouse = hitUnit;
    }
}


void NC_STACK_ypaworld::yw_MouseSelect(TInputState *arg)
{
    ComputeMouseSelectFlags(arg);

    if ( _guiActFlags & 4 )
    {
        if ( _guiActFlags & 8 )
        {
            yw_MAP_MouseSelect(&arg->ClickInf);
        }
        else if ( _guiActFlags & 0x40 )
        {
            yw_SMAN_MouseSelect(&arg->ClickInf);
        }
    }
    else
    {
        yw_3D_MouseSelect(&arg->ClickInf);
    }
}

void NC_STACK_ypaworld::UpdateActionEnergyCost()
{
    _actionEnergyCost = 0;

    if ( bzda.activeMode == 2 || bzda.activeMode == 4 )
    {
        if ( bzda.selVhclIdx != -1 )
        {
            _actionEnergyCost = dround(GetUnitLimitCostFactor() * (2 * _vhclProtos[bzda.vhclProtoIds[bzda.selVhclIdx]].energy));
        }
    }
    else if ( bzda.activeMode == 16 && bzda.selBuildIdx != -1 )
    {
        _actionEnergyCost = dround(GetUnitLimitCostFactor() * _buildProtos[bzda.buildProtoIds[bzda.selBuildIdx]].Energy);
    }
}

int SelectMouseUnitSquadLeader(NC_STACK_ypaworld *yw)
{
    const MapScene &scene = yw->UiScene();

    if ( !yw->_bactOnMouse.Id )
        return 0;

    if ( yw->_bactOnMouse.Id == scene.UserRoboId() )
        return 0;

    MapScene::Unit leader = yw->_bactOnMouse;

    if ( !leader.Commander && !(leader.HasParent && scene.Find(leader.Parent, leader)) )
        return 0;
    
    yw->_bactOnMouse = leader;
    return 1;
}

void NC_STACK_ypaworld::ExecuteMouseAction(int arg)
{
    const MapScene &scene = UiScene();

    World::PlayerOrder order;
    order.Type = World::PlayerOrder::COMMAND;
    order.Action = arg;

    switch ( arg )
    {
    case World::DOACTION_8:
        if ( _bactOnMouse.Type != BACT_TYPES_GUN && _bactOnMouse.Id != scene.UserRoboId() )
        {
            if ( SelectMouseUnitSquadLeader(this) )
            {
                _activeCmdrID = _bactOnMouse.CommandId;
                UpdateCommanderList();
            }
        }

        if ( scene.UserRoboId() != _bactOnMouse.Id )
            bzda.activeMode = bzda.enabledModes & 1;
        else
            bzda.activeMode = bzda.enabledModes & 0x20;
        break;

    case World::DOACTION_2:
        if (_activeCmdrRemapIndex >= 0)
        {
            order.Unit = _cmdrsRemap[_activeCmdrRemapIndex].Id;

            if ( _guiActFlags & 0x10 )
            {
                order.Sector = SectorIndex(_cellOnMouse);
                order.Point = _cellMouseIsectPos;
            }
            else
            {
                order.Target = _bactOnMouse.Id;
            }

            IssueOrder(order);

            bzda.activeMode = bzda.enabledModes & 1;
        }
        break;

    case World::DOACTION_WP_BEGIN:
    case World::DOACTION_WP_ADD:
    case World::DOACTION_WP_CYCLE:
        if (_activeCmdrRemapIndex >= 0)
        {
            order.Unit = _cmdrsRemap[_activeCmdrRemapIndex].Id;
            order.Sector = SectorIndex(_cellOnMouse);
            order.Point = _cellMouseIsectPos;

            IssueOrder(order);
        }
        break;

    case World::DOACTION_ADD_UNIT1:
        order.Proto = bzda.vhclProtoIds[bzda.selVhclIdx];
        order.Point = _mouseVector * _mouseSelDistance + _viewerPosition;
        IssueOrder(order);

        bzda.activeMode = 4;

        _activeCmdrRemapIndex = -1;
        _activeCmdrID = 0;
        break;

    case World::DOACTION_ADD_UNIT2:
        if (_activeCmdrRemapIndex >= 0)
        {
            order.Unit = _cmdrsRemap[_activeCmdrRemapIndex].Id;
            order.Proto = bzda.vhclProtoIds[bzda.selVhclIdx];
            order.Point = _mouseVector * _mouseSelDistance + _viewerPosition;
            IssueOrder(order);
        }
        break;

    case World::DOACTION_5:
        SwitchControlToUnit(this, _bactOnMouse);

        if ( _bactOnMouse.Type != BACT_TYPES_GUN && _bactOnMouse.Id != scene.UserRoboId() )
        {
            if ( SelectMouseUnitSquadLeader(this) )
            {
                _activeCmdrID = _bactOnMouse.CommandId;
                UpdateCommanderList();
            }
        }
        break;

    case World::DOACTION_6:
        order.Proto = bzda.buildProtoIds[bzda.selBuildIdx];
        order.Sector = SectorIndex(_cellOnMouse);
        order.Point = _cellMouseIsectPos;
        IssueOrder(order);

        bzda.activeMode = bzda.enabledModes & 1;
        break;

    case World::DOACTION_10:
        // Where on the ground it was asked for: the order is checked again
        order.Sector = SectorIndex(_cellOnMouse);
        order.Point = _beamGroundPos;
        IssueOrder(order);

        bzda.activeMode = 1;
        break;

    case World::DOACTION_19:
        // Looking from any unit, the game's own only
        if ( _bactOnMouse.Bact && UserUnit() )
        {
            UserUnit()->setBACT_viewer(false, _localPlayer);
            UserUnit()->setBACT_inputting(false, _localPlayer);
            _bactOnMouse.Bact->setBACT_viewer(true, _localPlayer);
        }
        break;

    default:
        return;
    }
}


void NC_STACK_ypaworld::HandleWorldMouse(TInputState *arg)
{
    const MapScene &scene = UiScene();
    uint8_t player = scene.PlayerOwner();

    if ( player > 0 && player < 8 && (arg->ClickInf.flag & TClickBoxInf::FLAG_OK) && scene.UserUnitId() )
    {
        yw_MouseSelect(arg);
        UpdateActionEnergyCost();

        int tooltip = 0;
        int mousePointer = 0;
        int doAction = World::DOACTION_0;

        if ( _mouseGrabbed )
        {
            mousePointer = -1;
        }
        else if ( _guiDragDefaultMouse || ((_guiActFlags & 4) && !(_guiActFlags & 0x48)) )
        {
            mousePointer = 0;
        }
        else if ( robo_map.stateFlags & 0x200 )
        {
            mousePointer = 2;
        }
        else
        {

            mousePointer = 1;
            doAction = World::DOACTION_0;

            if ( bzda.activeMode == 1 )
            {
                if ( !(_guiActFlags & 0x10) )
                {
                    if ( (_guiActFlags & 0x20) )
                    {
                        if ( player == _bactOnMouse.Owner )
                        {
                            doAction = World::DOACTION_8;
                            mousePointer = 2;
                            tooltip = Locale::TIP_DO_SELECT;
                        }
                        else if ( _activeCmdrRemapIndex != -1 )
                        {
                            doAction = World::DOACTION_2;
                            mousePointer = 3;
                            tooltip = Locale::TIP_DO_ATKVHCL;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( player == scene.Owner(_cellOnMouse) && scene.CanSee(_cellOnMouse, player) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                        mousePointer = 3;
                    }
                }
            }
            else if ( bzda.activeMode == 2 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && player == _bactOnMouse.Owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        switch ( UserActCreateCheck(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT1;
                            mousePointer = 6;
                            tooltip = Locale::TIP_DO_NEWSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( player == _bactOnMouse.Owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( player == scene.Owner(_cellOnMouse) && scene.CanSee(_cellOnMouse, player) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if ( bzda.activeMode == 4 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && player == _bactOnMouse.Owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else if ( _activeCmdrRemapIndex != -1 )
                    {
                        switch ( UserActCreateCheck(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT2;
                            mousePointer = 7;
                            tooltip = Locale::TIP_DO_ADDSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {

                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( player == _bactOnMouse.Owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( player == scene.Owner(_cellOnMouse) && scene.CanSee(_cellOnMouse, player) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if (bzda.activeMode == 8 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( arg->KbdLastDown == Input::KC_F7 && (arg->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD) )
                    {
                        doAction = World::DOACTION_19;
                        mousePointer = 8;
                    }
                    else if ( player == _bactOnMouse.Owner )
                    {
                        doAction = World::DOACTION_5;
                        mousePointer = 8;
                        tooltip = Locale::TIP_DO_CONTROL;
                    }
                }
            }
            else if ( bzda.activeMode == 16 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( player == _bactOnMouse.Owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( UserActBuildCheck() )
                    {
                    case 0:
                        doAction = World::DOACTION_6;
                        mousePointer = 10;
                        tooltip = Locale::TIP_DO_BUILD;
                        break;

                    case 6:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_NOTOWNED;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_BUILD_TOOCLOSE;
                        break;

                    case 4:
                        tooltip = Locale::TIP_ERR_BUILD_TOOFAR;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_BUILD_INPROC;
                        break;

                    default:
                        break;
                    }
                }
            }
            else if ( bzda.activeMode == 32 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( player == _bactOnMouse.Owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( UserActBeamCheck() )
                    {
                    case 0:
                        doAction = World::DOACTION_10;
                        mousePointer = 9;
                        tooltip = Locale::TIP_DO_TELEPORT;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_UNITSINSECTOR;
                        break;

                    case 1:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_SECTORJAGGY;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_LOCUNKNOWN;
                        break;

                    default:
                        break;
                    }
                }
            }

            _doAction = HandleWaypointMode(arg, doAction);
            int action = _doAction;

            if ( CheckUnitDoubleClick(&arg->ClickInf) )
            {
                action = World::DOACTION_5;
                mousePointer = 8;
                arg->ClickInf.flag |= TClickBoxInf::FLAG_LM_DOWN;
            }

            if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
                ExecuteMouseAction(action);

            if ( (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD) && action != World::DOACTION_0 )
                mousePointer = 0;
        }

        if ( _mouseCursorHidden )
            mousePointer = -1;

        // Cursor ids are the mouse pointer numbers plus one; 0 hides it
        int cursorId = 0;

        if ( mousePointer >= 0 && mousePointer <= 10 )
            cursorId = mousePointer + 1;

        if ( squadron_manager.dragFlags & 1 )
            cursorId = 1;

        GFX::Engine.SetCursor(cursorId, 0);

        if ( tooltip )
            SetShowingTooltip(tooltip);
    }
}



// Whether the fire button went down in this step, as a dead unit's
// player's goes on to the next squad
void NC_STACK_ypaworld::TrackFireButton(TInputState *inpt)
{
    _fireBtnDownHappen = false;

    if ( inpt->Buttons.Is(0) )
    {
        if ( !_fireBtnIsDown )
            _fireBtnDownHappen = true;

        _fireBtnIsDown = true;
    }
    else
    {
        _fireBtnIsDown = false;
    }
}

void NC_STACK_ypaworld::PrepareUserInput(TInputState *inpt)
{
    //Precompute input
    TClickBoxInf *winp = &inpt->ClickInf;

    // The unit the player looks from, whose controls these are
    MapScene::Unit user;
    UiScene().UserUnit(user);

    inpt->Buttons.UnSet(31);

    int joyDisabled = 0;

    if ( _timeStamp - _vehicleTakenControlTimestamp < 500 )
        inpt->Buttons.UnSet(0);

    if ( _timeStamp - _vehicleTakenControlTimestamp > 5000 )
        _invulnerable = 0;

    if ( _shellConfIsParsed )
    {
        if ( _preferences & World::PREF_JOYDISABLE )
            joyDisabled = 1;
    }

    TrackFireButton(inpt);

    if ( user.Status == BACT_STATUS_DEAD )
    {
        _mouseGrabbed = false;
    }
    else if ( winp->flag & TClickBoxInf::FLAG_RM_DOWN )
    {
        if ( _mouseGrabbed
                || winp->selected_btn == &robo_map
                || winp->selected_btn == &bzda
                || winp->selected_btn == &squadron_manager
                || (!(bzda.activeMode & 1) && _activeCmdrRemapIndex != -1) )
        {
            if ( _mouseGrabbed )
            {
                _mouseGrabbed = false;
            }
            else
            {
                if ( winp->selected_btn != &robo_map && winp->selected_btn != &bzda && !(bzda.activeMode & 1) )
                    bzda.activeMode = bzda.enabledModes & 1;
            }
        }
        else
        {
            _mouseGrabbed = true;

            Input::Engine.ResetSlider(10);
            Input::Engine.ResetSlider(11);
        }
    }

    if ( gui_lstvw.IsOpen() )
        inpt->Sliders[1] = 0;

    if ( _mouseGrabbed ) // If grabbed mouse
    {
        // Piu-piu mazafaka
        winp->selected_btnID = -1;
        winp->flag &= ~(TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD | TClickBoxInf::FLAG_BTN_UP);

        inpt->Sliders[0] += inpt->Sliders[10];
        inpt->Sliders[1] += inpt->Sliders[11];
        inpt->Sliders[3] += inpt->Sliders[10];
        inpt->Sliders[5] -= inpt->Sliders[11] * 1.5;

        if ( winp->flag & TClickBoxInf::FLAG_LM_HOLD )
            inpt->Buttons.Set(0);

        if ( winp->flag & TClickBoxInf::FLAG_MM_HOLD )
            inpt->Buttons.Set(3);

        SetShowingTooltip(Locale::TIP_MOUSECAPTURED);
    }

    if ( !joyDisabled )
    {
        int isGroundVehicle = user.Type == BACT_TYPES_TANK || user.Type == BACT_TYPES_CAR;
        int isRobo = user.Type == BACT_TYPES_ROBO;

        if ( !_joyKnown )
        {
            _joyKnown = true;
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        int joyXMoved = fabs(_oldJoyX - inpt->Sliders[12]) >= 0.05;
        int joyYMoved = fabs(_oldJoyY - inpt->Sliders[13]) >= 0.05;
        int joyZMoved = fabs(_oldJoyZ - inpt->Sliders[14]) >= 0.05;
        int hatXMoved = fabs(_oldJoyHatX - inpt->Sliders[15]) >= 0.05;
        int hatYMoved = fabs(_oldJoyHatY - inpt->Sliders[16]) >= 0.05;

        if ( joyXMoved || joyYMoved || joyZMoved || hatXMoved || hatYMoved)
        {
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        if ( _joyIgnoreX )
        {
            if ( joyXMoved )
                _joyIgnoreX = 0;
        }
        else
        {
            if ( ( isGroundVehicle && inpt->Sliders[3] != 0.0 ) || inpt->Sliders[0] != 0.0 )
                _joyIgnoreX = 1;
        }

        if ( _joyIgnoreY )
        {
            if ( joyYMoved || hatXMoved || hatYMoved )
                _joyIgnoreY = 0;
        }
        else
        {
            if ( isGroundVehicle )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK ) && inpt->Sliders[5] != 0.0 )
                    _joyIgnoreY = 1;
                else if ( inpt->Sliders[4] != 0.0 )
                    _joyIgnoreY = 1;
            }
            else if ( inpt->Sliders[1] != 0.0 )
                _joyIgnoreY = 1;
        }

        if ( _joyIgnoreZ )
        {
            if ( joyZMoved || hatXMoved || hatYMoved )
                _joyIgnoreZ = 0;
        }
        else
        {
            if ( isGroundVehicle )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK) && inpt->Sliders[4] != 0.0 )
                    _joyIgnoreZ = 1;
            }
            else if ( !isRobo && inpt->Sliders[2] != 0.0 )
                _joyIgnoreZ = 1;
        }

        if ( inpt->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
        {
            if ( !(hatXMoved || joyYMoved || joyXMoved || joyZMoved || hatYMoved) )
            {
                _joyIgnoreY = 1;
                _joyIgnoreX = 1;
            }
        }

        if ( _joyIgnoreX )
            inpt->Sliders[12] = 0;

        if ( _joyIgnoreY )
            inpt->Sliders[13] = 0;

        if ( _joyIgnoreZ )
            inpt->Sliders[14] = 0;

        if ( !_joyIgnoreX )
        {
            if ( isGroundVehicle )
                inpt->Sliders[3] += inpt->Sliders[12];
            else
                inpt->Sliders[0] += inpt->Sliders[12];
        }

        if ( !_joyIgnoreY )
        {
            if ( isGroundVehicle )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[16] != 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                    else
                        inpt->Sliders[5] += inpt->Sliders[13] * 2.0;
                }
                else
                {

                    if ( fabs(inpt->Sliders[13]) < 0.3 )
                        inpt->Buttons.Set(3);
                    else
                        inpt->Sliders[4] += -inpt->Sliders[13];

                    inpt->Buttons.Set(31);
                }
            }
            else
            {
                inpt->Sliders[1] += inpt->Sliders[13];
            }
        }

        if ( !_joyIgnoreZ )
        {
            if ( isGroundVehicle )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[4] != 0.0 )
                    {
                    }
                    else if ( fabs(inpt->Sliders[14]) < 0.3 )
                    {
                        inpt->Buttons.Set(3);
                    }
                    else
                        inpt->Sliders[4] += inpt->Sliders[14];

                    inpt->Buttons.Set(31);
                }
                else
                {
                    if ( inpt->Sliders[5] == 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                }
            }
            else
            {
                if ( inpt->Sliders[2] == 0.0 )
                    inpt->Sliders[2] += inpt->Sliders[14];
            }
        }
        if ( inpt->Buttons.Is(16) )
            inpt->Buttons.Set(0);

        if ( inpt->Buttons.Is(17) )
            inpt->Buttons.Set(2);

        if ( inpt->Buttons.Is(18) )
            inpt->Buttons.Set(1);

        if ( inpt->Buttons.Is(19) )
            inpt->Buttons.Set(3);
    }
}


void NC_STACK_ypaworld::GUI_Close()
{
    if ( _guiLoaded )
    {
        GuiWinDetach(&lstvw2);
        GuiWinDetach(&exit_menu);
        GuiWinDetach(&info_log);
        GuiWinDetach(&squadron_manager);
        GuiWinDetach(&robo_map);
        GuiWinDetach(&netgame_wnd);
        GuiWinDetach(&_viewDistanceWindow);

        _viewDistanceWindow.Free();
        FreeExitMenu(this);
        FreeInfoLog(this);
        FreeSquadManager(this);
        FreeBottomBar(this);
        FreeEnergyPanel();
        FreeHud(this);
        FreeConfirmWindow(this);

        FFeedback_StopAll();

        _mouseCursorHidden = false;
        _guiLoaded = false;
    }
}

// Keeps a window rectangle on screen, between the energy panel and the bottom bar.
static void ClampToGuiArea(NC_STACK_ypaworld *yw, int16_t &x, int16_t &y, int16_t &w, int16_t &h, int minW, int minH)
{
    int top = yw->_upScreenBorder;
    int bottom = yw->_screenSize.y - yw->_downScreenBorder;

    int ww = std::max(minW, std::min<int>(w, yw->_screenSize.x));
    int hh = std::max(minH, std::min<int>(h, bottom - top));

    x = std::max(0, std::min<int>(x, yw->_screenSize.x - ww));
    y = std::max(top, std::min<int>(y, bottom - hh));
    w = ww;
    h = hh;
}

// The map back on screen, as it is laid out anew. It keeps a second
// rectangle for its minimise/maximise toggle.
void RoboMapKeepOnScreen(NC_STACK_ypaworld *yw)
{
    ClampToGuiArea(yw, robo_map.x, robo_map.y, robo_map.w, robo_map.h, robo_map.minWidth, robo_map.minHeight);
    ClampToGuiArea(yw, robo_map.altX, robo_map.altY, robo_map.altW, robo_map.altH,
                   robo_map.minWidth, robo_map.minHeight);
}

// Lays the in-game GUI out for a new screen size, keeping the windows' state.
void NC_STACK_ypaworld::RelayoutGameGui()
{
    FreeEnergyPanel();
    CreateEnergyPanel(this);
    SetBottomBarLayout(this);
    SetHudLayout(this);
    SetNetGameWindowLayout(this);
    RoboMapKeepOnScreen(this);

    GuiList *windows[] = {&squadron_manager, &info_log, &exit_menu, &lstvw2, &gui_lstvw, &_viewDistanceWindow};
    for (GuiList *win : windows)
        win->SetRect(this, -2, -2);
}

void NC_STACK_ypaworld::GuiWinToFront(GuiBase *win)
{
    if ( !(win->flags & GuiBase::FLAG_CLOSED) )
    {
        win->Detach();
        win->Attach(_guiActive);
        Input::Engine.RemClickBox(win);
        Input::Engine.AddClickBoxFront(win);
    }
}

void NC_STACK_ypaworld::GuiWinOpen(GuiBase *win)
{
    if ( win->flags & GuiBase::FLAG_CLOSED )
    {
        win->flags &= ~GuiBase::FLAG_CLOSED;
        Input::Engine.AddClickBoxFront(win);
        _guiDragDefaultMouse = false;
    }
}

void NC_STACK_ypaworld::GuiWinClose(GuiBase *win)
{
    if ( !(win->flags & GuiBase::FLAG_CLOSED) )
    {
        win->flags |= GuiBase::FLAG_CLOSED;
        Input::Engine.RemClickBox(win);
        _guiDragDefaultMouse = false;
    }
}
