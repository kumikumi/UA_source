#include <inttypes.h>
#include <string.h>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "font.h"
#include "button.h"

#include "yw_net.h"
#include "windp.h"

#include <math.h>

#include "env.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "loaders.h"
#include "obj3d.h"

extern int vertMenuSpace;
extern int videoMenuRowWidth;
extern int menuListWidth;
extern int buttonsSpace;

extern int netMenuWidth;
extern int subBarButtonWidth;

void RenderBriefingVehicle(NC_STACK_ypaworld *yw, bool clockwise, int rot, vec3d *pos, baseRender_msg *arg)
{
    //brf_obj *brobj = &yw->brief.brf_objs + obj_id; // Only one object

    NC_STACK_base *model_base = yw->_vhclModels.at( yw->_vhclProtos[ yw->_briefScreen.ViewingObject.ID ].vp_normal );
    
    model_base->SetVizLimit(16000);
    model_base->SetFadeLength(100);

    model_base->SetPosition(*pos);

    if (clockwise)
    {
        yw->_briefScreen.ViewingObjectAngle += (arg->frameTime / 5);
        if ( yw->_briefScreen.ViewingObjectAngle >= 360 )
            yw->_briefScreen.ViewingObjectAngle -= 360;
    }
    else
    {
        yw->_briefScreen.ViewingObjectAngle -= (arg->frameTime / 5);
        if ( yw->_briefScreen.ViewingObjectAngle < 0 )
            yw->_briefScreen.ViewingObjectAngle += 360;
    }

    model_base->SetEulerRotation( rot + 10, yw->_briefScreen.ViewingObjectAngle, 0);
    //printf("Try DRAW %d\n", (int)model_base);
    
    NC_STACK_base::CheckOpts(&yw->_briefScreen.ViewingObject.VP, model_base);
    
    model_base->Render(arg, yw->_briefScreen.ViewingObject.VP); //Draw vehicle
}

void RenderBriefingSector(NC_STACK_ypaworld *yw, bool clockwise, int rot, vec3d *pos, baseRender_msg *arg)
{
    TSectorDesc *scType = &yw->_secTypeArray[yw->_briefScreen.ViewingObject.ID];

    NC_STACK_base *pivot = yw->_vhclModels.at(0);

    if (clockwise)
    {
        yw->_briefScreen.ViewingObjectAngle += (arg->frameTime / 5);
        if ( yw->_briefScreen.ViewingObjectAngle >= 360 )
            yw->_briefScreen.ViewingObjectAngle -= 360;
    }
    else
    {
        yw->_briefScreen.ViewingObjectAngle -= (arg->frameTime / 5);
        if ( yw->_briefScreen.ViewingObjectAngle < 0 )
            yw->_briefScreen.ViewingObjectAngle += 360;
    }

    pivot->SetEulerRotation(rot + 10, yw->_briefScreen.ViewingObjectAngle, 0);

    int first;
    int demens;

    if ( scType->SectorType == 1 )
    {
        first = 0;
        demens = 1;
    }
    else
    {
        first = -1;
        demens = 3;
    }

    int offsetZ = first;
    for (int y = 0; y < demens; y++)
    {
        int offsetX = first;
        for (int x = 0; x < demens; x++)
        {
            vec3d inSectorPos = vec3d(offsetX, 0.0, offsetZ) * 300.0;

            NC_STACK_base *lego = yw->_legoArray[ scType->SubSectors.At(x, y)->HPModels[0] ].Base;
            lego->SetStatic(false);
            lego->SetVizLimit(16000);
            lego->SetFadeLength(100);

            lego->SetEulerRotation(rot + 10, yw->_briefScreen.ViewingObjectAngle, 0);
            lego->SetPosition( *pos + pivot->TForm().SclRot.Transform( inSectorPos ) );
            
            NC_STACK_base::CheckOpts(&yw->_briefScreen.ViewingObject.VP, lego);
            
            lego->Render(arg, yw->_briefScreen.ViewingObject.VP);
            
            offsetX++;
        }
        offsetZ++;
    }
}

void RenderBriefingObject(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc, int elapsed)
{
    brf->ObjRenderParams.frameTime = struc->Period;
    brf->ObjRenderParams.globTime = brf->CurrTime;

    // The camera lives only for this call, so the view point is put back
    // after drawing. Left set, the mission's first simulation step would read
    // it once it is gone.
    TF::TForm3D *prevView = TF::Engine.GetViewPoint();

    TF::TForm3D camera;
    camera.Scale = vec3d(1.0, 1.0, 1.0);
    camera.SclRot = mat3x3::Ident();

    TF::Engine.SetViewPoint(&camera);
    camera.CalcGlobal();

    vec3d pos;

    if ( brf->ViewingObject ) // Not none
    {
        pos.x = (brf->ViewingObjectRect.right + brf->ViewingObjectRect.left) / 2.0;
        pos.y = (brf->ViewingObjectRect.bottom + brf->ViewingObjectRect.top) / 2.0;

        float startDist;
        float endDist;
        float dist;
        int rot;

        if ( brf->ViewingObject.ObjType == TBriefObject::TYPE_SECTOR )
        {
            startDist = 9600.0;
            endDist = 3600.0;
        }
        else if ( brf->ViewingObject.ObjType == TBriefObject::TYPE_VEHICLE )
        {
            float radius = yw->_vhclProtos[brf->ViewingObject.ID].radius;

            endDist = radius * 7.0;
            startDist = radius * 32.0;
        }

        if ( elapsed >= 500 )
        {
            dist = endDist;
            rot = 0;
        }
        else
        {
            dist = startDist + (endDist - startDist) * elapsed / 500.0;
            rot = -90 * elapsed / 500 + 90;
        }

        pos.z = dist;
        pos.y = pos.y * dist;
        pos.x = pos.x * dist;

        if ( brf->ViewingObject.ObjType == TBriefObject::TYPE_SECTOR )
        {
            RenderBriefingSector(yw, true, rot, &pos, &brf->ObjRenderParams);
        }
        else if ( brf->ViewingObject.ObjType == TBriefObject::TYPE_VEHICLE )
        {
            RenderBriefingVehicle(yw, true, rot, &pos, &brf->ObjRenderParams);
        }
    }

    TF::Engine.SetViewPoint(prevView);
}

void ypaworld_func158__DrawVehicle(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc)
{
    GFX::Engine.SetFBOBlending(1);
    GFX::Engine.BeginScene();

    brf->ObjRenderParams.frameTime = 1;
    brf->ObjRenderParams.globTime = 1;
    brf->ObjRenderParams.adeCount = 0;
    brf->ObjRenderParams.minZ = 17.0;
    brf->ObjRenderParams.maxZ = 32000.0;
    brf->ObjRenderParams.flags = 0;

    if ( brf->ViewingObject ) // Not none
    {
        int elapsed = brf->CurrTime - brf->ViewingObjectStartTime;
        if ( elapsed > 50 )
            RenderBriefingObject(yw, brf, struc, elapsed - 50);
    }
    
    GFX::Engine.Rasterize();

    GFX::Engine.EndScene();
    GFX::Engine.SetFBOBlending(0);
}



void yw_draw_input_list(NC_STACK_ypaworld *yw, UserData *usr)
{
    usr->input_listview.SetRect(yw, -2, -2);
    GFX::Engine.GetTileset(0);
    
    usr->input_listview.itemBlock.clear();

    usr->input_listview.ItemsPreLayout(yw, &usr->input_listview.itemBlock, 0, "uvw");

    for (int i = 1; i <= usr->input_listview.shownEntries; i++ )
    {
        int inputId = i + usr->input_listview.firstShownEntries;
        if ( !usr->InputConfigTitle[inputId].empty() )
        {
            FontUA::ColumnItem columns[2];

            int spaceChar;
            int postfixChar;
            int fontId;
            int prefixChar;

            if ( inputId == usr->inpListActiveElement )
            {
                prefixChar = 98;
                postfixChar = 100;
                fontId = 9;
                spaceChar = 99;
            }
            else
            {
                prefixChar = 102;
                postfixChar = 102;
                fontId = 0;
                spaceChar = 102;
            }

            int rowWidth = usr->input_listview.entryWidth - 2 * usr->p_YW->_fontBorderW + 1;

            std::string keyText;

            if ( usr->InputConfig[ inputId ].Type == World::INPUT_BIND_TYPE_SLIDER )
            {
                std::string negKeyName, posKeyName;
                if ( usr->InputConfig[ inputId ].NKeyCode )
                    negKeyName = Input::Engine.KeyTitle.at( usr->InputConfig[ inputId ].NKeyCode );
                else
                    negKeyName = "-";

                if ( usr->InputConfig[ inputId ].PKeyCode )
                    posKeyName = Input::Engine.KeyTitle.at( usr->InputConfig[ inputId ].PKeyCode );
                else
                    posKeyName = "-";

                if ( usr->InputConfig[ inputId ].SetFlags & UserData::TInputConf::IF_FIRST )
                    posKeyName = Locale::Text::Dialogs(Locale::DLG_I_UNK);

                if ( usr->InputConfig[ inputId ].SetFlags & UserData::TInputConf::IF_SECOND )
                    negKeyName = Locale::Text::Dialogs(Locale::DLG_I_UNK);

                keyText = fmt::sprintf("%s/%s", negKeyName, posKeyName);
            }
            else
            {
                if ( usr->InputConfig[ inputId ].PKeyCode )
                    keyText = Input::Engine.KeyTitle.at( usr->InputConfig[ inputId ].PKeyCode );
                else
                    keyText = "-";

                if ( usr->InputConfig[ inputId ].SetFlags & UserData::TInputConf::IF_FIRST )
                    keyText = Locale::Text::Dialogs(Locale::DLG_I_UNK);
            }

            columns[0].txt = usr->InputConfigTitle[inputId];
            columns[0].width = rowWidth * 0.68;
            columns[0].fontID = fontId;
            columns[0].spaceChar = spaceChar;
            columns[0].prefixChar = prefixChar;
            columns[0].postfixChar = 0;
            columns[0].flags = 37;

            columns[1].txt = keyText;
            columns[1].width = rowWidth - rowWidth * 0.68;
            columns[1].fontID = fontId;
            columns[1].spaceChar = spaceChar;
            columns[1].prefixChar = 0;
            columns[1].postfixChar = postfixChar;
            columns[1].flags = 38;

            FontUA::select_tileset(&usr->input_listview.itemBlock, 0);
            FontUA::store_s8(&usr->input_listview.itemBlock, '{'); // Left wnd border

            if ( inputId == usr->inpListActiveElement )
            {
                FontUA::set_txtColor(&usr->input_listview.itemBlock, usr->p_YW->_iniColors[62].r, usr->p_YW->_iniColors[62].g, usr->p_YW->_iniColors[62].b);
            }
            else
            {
                FontUA::set_txtColor(&usr->input_listview.itemBlock, usr->p_YW->_iniColors[61].r, usr->p_YW->_iniColors[61].g, usr->p_YW->_iniColors[61].b);
            }

            FormateColumnItem(yw, &usr->input_listview.itemBlock, 2, columns);

            FontUA::select_tileset(&usr->input_listview.itemBlock, 0);
            FontUA::store_s8(&usr->input_listview.itemBlock, '}'); // Right wnd border
            FontUA::next_line(&usr->input_listview.itemBlock);
        }
    }
    usr->input_listview.ItemsPostLayout(yw, &usr->input_listview.itemBlock, 0, "xyz");
    FontUA::set_end(&usr->input_listview.itemBlock);

    GFX::Engine.ProcessDrawSeq(usr->input_listview.cmdCommands, &usr->input_listview.cmdInclude);
}


void NC_STACK_ypaworld::LoadKeyNames()
{
    for (std::string &a: Input::Engine.KeyTitle)
        a.clear();

    Input::Engine.KeyTitle[Input::KC_NONE]       = "*"; // Locale::Text::GetKeyNameString(Locale::KEYNAME_NOP);
    Input::Engine.KeyTitle[Input::KC_ESCAPE]     = Locale::Text::KeyName(Locale::KEYNAME_ESC);
    Input::Engine.KeyTitle[Input::KC_SPACE]      = Locale::Text::KeyName(Locale::KEYNAME_SPACE);
    Input::Engine.KeyTitle[Input::KC_UP]         = Locale::Text::KeyName(Locale::KEYNAME_UP);
    Input::Engine.KeyTitle[Input::KC_DOWN]       = Locale::Text::KeyName(Locale::KEYNAME_DOWN);
    Input::Engine.KeyTitle[Input::KC_LEFT]       = Locale::Text::KeyName(Locale::KEYNAME_LEFT);
    Input::Engine.KeyTitle[Input::KC_RIGHT]      = Locale::Text::KeyName(Locale::KEYNAME_RIGHT);
    Input::Engine.KeyTitle[Input::KC_F1]         = Locale::Text::KeyName(Locale::KEYNAME_F1);
    Input::Engine.KeyTitle[Input::KC_F2]         = Locale::Text::KeyName(Locale::KEYNAME_F2);
    Input::Engine.KeyTitle[Input::KC_F3]         = Locale::Text::KeyName(Locale::KEYNAME_F3);
    Input::Engine.KeyTitle[Input::KC_F4]         = Locale::Text::KeyName(Locale::KEYNAME_F4);
    Input::Engine.KeyTitle[Input::KC_F5]         = Locale::Text::KeyName(Locale::KEYNAME_F5);
    Input::Engine.KeyTitle[Input::KC_F6]         = Locale::Text::KeyName(Locale::KEYNAME_F6);
    Input::Engine.KeyTitle[Input::KC_F7]         = Locale::Text::KeyName(Locale::KEYNAME_F7);
    Input::Engine.KeyTitle[Input::KC_F8]         = Locale::Text::KeyName(Locale::KEYNAME_F8);
    Input::Engine.KeyTitle[Input::KC_F9]         = Locale::Text::KeyName(Locale::KEYNAME_F9);
    Input::Engine.KeyTitle[Input::KC_F10]        = Locale::Text::KeyName(Locale::KEYNAME_F10);
    Input::Engine.KeyTitle[Input::KC_F11]        = Locale::Text::KeyName(Locale::KEYNAME_F11);
    Input::Engine.KeyTitle[Input::KC_F12]        = Locale::Text::KeyName(Locale::KEYNAME_F12);
    Input::Engine.KeyTitle[Input::KC_BACKSPACE]  = Locale::Text::KeyName(Locale::KEYNAME_BACK);
    Input::Engine.KeyTitle[Input::KC_TAB]        = Locale::Text::KeyName(Locale::KEYNAME_TAB);
    Input::Engine.KeyTitle[Input::KC_CLEAR]      = Locale::Text::KeyName(Locale::KEYNAME_CLEAR);
    Input::Engine.KeyTitle[Input::KC_RETURN]     = Locale::Text::KeyName(Locale::KEYNAME_RETURN);
    Input::Engine.KeyTitle[Input::KC_CTRL]       = Locale::Text::KeyName(Locale::KEYNAME_CTRL);
    Input::Engine.KeyTitle[Input::KC_SHIFT]      = Locale::Text::KeyName(Locale::KEYNAME_SHIFT);
    Input::Engine.KeyTitle[Input::KC_ALT]        = Locale::Text::KeyName(Locale::KEYNAME_ALT);
    Input::Engine.KeyTitle[Input::KC_PAUSE]      = Locale::Text::KeyName(Locale::KEYNAME_PAUSE);
    Input::Engine.KeyTitle[Input::KC_PGUP]       = Locale::Text::KeyName(Locale::KEYNAME_PGUP);
    Input::Engine.KeyTitle[Input::KC_PGDOWN]     = Locale::Text::KeyName(Locale::KEYNAME_PGDOWN);
    Input::Engine.KeyTitle[Input::KC_END]        = Locale::Text::KeyName(Locale::KEYNAME_END);
    Input::Engine.KeyTitle[Input::KC_HOME]       = Locale::Text::KeyName(Locale::KEYNAME_HOME);
    Input::Engine.KeyTitle[Input::KC_SELECT]     = Locale::Text::KeyName(Locale::KEYNAME_SELECT);
    Input::Engine.KeyTitle[Input::KC_EXECUTE]    = Locale::Text::KeyName(Locale::KEYNAME_EXEC);
    Input::Engine.KeyTitle[Input::KC_SNAPSHOT]   = Locale::Text::KeyName(Locale::KEYNAME_PRINT);
    Input::Engine.KeyTitle[Input::KC_INSERT]     = Locale::Text::KeyName(Locale::KEYNAME_INS);
    Input::Engine.KeyTitle[Input::KC_DELETE]     = Locale::Text::KeyName(Locale::KEYNAME_DEL);
    Input::Engine.KeyTitle[Input::KC_HELP]       = Locale::Text::KeyName(Locale::KEYNAME_HELP);
    Input::Engine.KeyTitle[Input::KC_1]          = Locale::Text::KeyName(Locale::KEYNAME_1);
    Input::Engine.KeyTitle[Input::KC_2]          = Locale::Text::KeyName(Locale::KEYNAME_2);
    Input::Engine.KeyTitle[Input::KC_3]          = Locale::Text::KeyName(Locale::KEYNAME_3);
    Input::Engine.KeyTitle[Input::KC_4]          = Locale::Text::KeyName(Locale::KEYNAME_4);
    Input::Engine.KeyTitle[Input::KC_5]          = Locale::Text::KeyName(Locale::KEYNAME_5);
    Input::Engine.KeyTitle[Input::KC_6]          = Locale::Text::KeyName(Locale::KEYNAME_6);
    Input::Engine.KeyTitle[Input::KC_7]          = Locale::Text::KeyName(Locale::KEYNAME_7);
    Input::Engine.KeyTitle[Input::KC_8]          = Locale::Text::KeyName(Locale::KEYNAME_8);
    Input::Engine.KeyTitle[Input::KC_9]          = Locale::Text::KeyName(Locale::KEYNAME_9);
    Input::Engine.KeyTitle[Input::KC_0]          = Locale::Text::KeyName(Locale::KEYNAME_0);
    Input::Engine.KeyTitle[Input::KC_A]          = Locale::Text::KeyName(Locale::KEYNAME_A);
    Input::Engine.KeyTitle[Input::KC_B]          = Locale::Text::KeyName(Locale::KEYNAME_B);
    Input::Engine.KeyTitle[Input::KC_C]          = Locale::Text::KeyName(Locale::KEYNAME_C);
    Input::Engine.KeyTitle[Input::KC_D]          = Locale::Text::KeyName(Locale::KEYNAME_D);
    Input::Engine.KeyTitle[Input::KC_E]          = Locale::Text::KeyName(Locale::KEYNAME_E);
    Input::Engine.KeyTitle[Input::KC_F]          = Locale::Text::KeyName(Locale::KEYNAME_F);
    Input::Engine.KeyTitle[Input::KC_G]          = Locale::Text::KeyName(Locale::KEYNAME_G);
    Input::Engine.KeyTitle[Input::KC_H]          = Locale::Text::KeyName(Locale::KEYNAME_H);
    Input::Engine.KeyTitle[Input::KC_I]          = Locale::Text::KeyName(Locale::KEYNAME_I);
    Input::Engine.KeyTitle[Input::KC_J]          = Locale::Text::KeyName(Locale::KEYNAME_J);
    Input::Engine.KeyTitle[Input::KC_K]          = Locale::Text::KeyName(Locale::KEYNAME_K);
    Input::Engine.KeyTitle[Input::KC_L]          = Locale::Text::KeyName(Locale::KEYNAME_L);
    Input::Engine.KeyTitle[Input::KC_M]          = Locale::Text::KeyName(Locale::KEYNAME_M);
    Input::Engine.KeyTitle[Input::KC_N]          = Locale::Text::KeyName(Locale::KEYNAME_N);
    Input::Engine.KeyTitle[Input::KC_O]          = Locale::Text::KeyName(Locale::KEYNAME_O);
    Input::Engine.KeyTitle[Input::KC_P]          = Locale::Text::KeyName(Locale::KEYNAME_P);
    Input::Engine.KeyTitle[Input::KC_Q]          = Locale::Text::KeyName(Locale::KEYNAME_Q);
    Input::Engine.KeyTitle[Input::KC_R]          = Locale::Text::KeyName(Locale::KEYNAME_R);
    Input::Engine.KeyTitle[Input::KC_S]          = Locale::Text::KeyName(Locale::KEYNAME_S);
    Input::Engine.KeyTitle[Input::KC_T]          = Locale::Text::KeyName(Locale::KEYNAME_T);
    Input::Engine.KeyTitle[Input::KC_U]          = Locale::Text::KeyName(Locale::KEYNAME_U);
    Input::Engine.KeyTitle[Input::KC_V]          = Locale::Text::KeyName(Locale::KEYNAME_V);
    Input::Engine.KeyTitle[Input::KC_W]          = Locale::Text::KeyName(Locale::KEYNAME_W);
    Input::Engine.KeyTitle[Input::KC_X]          = Locale::Text::KeyName(Locale::KEYNAME_X);
    Input::Engine.KeyTitle[Input::KC_Y]          = Locale::Text::KeyName(Locale::KEYNAME_Y);
    Input::Engine.KeyTitle[Input::KC_Z]          = Locale::Text::KeyName(Locale::KEYNAME_Z);
    Input::Engine.KeyTitle[Input::KC_NUM0]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_0);
    Input::Engine.KeyTitle[Input::KC_NUM1]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_1);
    Input::Engine.KeyTitle[Input::KC_NUM2]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_2);
    Input::Engine.KeyTitle[Input::KC_NUM3]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_3);
    Input::Engine.KeyTitle[Input::KC_NUM4]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_4);
    Input::Engine.KeyTitle[Input::KC_NUM5]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_5);
    Input::Engine.KeyTitle[Input::KC_NUM6]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_6);
    Input::Engine.KeyTitle[Input::KC_NUM7]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_7);
    Input::Engine.KeyTitle[Input::KC_NUM8]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_8);
    Input::Engine.KeyTitle[Input::KC_NUM9]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_9);
    Input::Engine.KeyTitle[Input::KC_NUMMUL]     = Locale::Text::KeyName(Locale::KEYNAME_MUL);
    Input::Engine.KeyTitle[Input::KC_NUMPLUS]    = Locale::Text::KeyName(Locale::KEYNAME_ADD);
    Input::Engine.KeyTitle[Input::KC_NUMDOT]     = Locale::Text::KeyName(Locale::KEYNAME_DOT);
    Input::Engine.KeyTitle[Input::KC_NUMMINUS]   = Locale::Text::KeyName(Locale::KEYNAME_SUB);
    Input::Engine.KeyTitle[Input::KC_NUMENTER]   = Locale::Text::KeyName(Locale::KEYNAME_ENTER);
    Input::Engine.KeyTitle[Input::KC_NUMDIV]     = Locale::Text::KeyName(Locale::KEYNAME_DIV);
    Input::Engine.KeyTitle[Input::KC_EXTRA1]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_1);
    Input::Engine.KeyTitle[Input::KC_EXTRA2]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_2);
    Input::Engine.KeyTitle[Input::KC_EXTRA3]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_3);
    Input::Engine.KeyTitle[Input::KC_EXTRA4]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_4);
    Input::Engine.KeyTitle[Input::KC_EXTRA5]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_5);
    Input::Engine.KeyTitle[Input::KC_EXTRA6]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_6);
    Input::Engine.KeyTitle[Input::KC_EXTRA7]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_7);
    Input::Engine.KeyTitle[Input::KC_EXTRA8]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_8);
    Input::Engine.KeyTitle[Input::KC_EXTRA9]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_9);
    Input::Engine.KeyTitle[Input::KC_EXTRA10]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_10);
    Input::Engine.KeyTitle[Input::KC_EXTRA11]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_11);
    Input::Engine.KeyTitle[Input::KC_EXTRA12]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_12);
    Input::Engine.KeyTitle[Input::KC_EXTRA13]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_13);
    Input::Engine.KeyTitle[Input::KC_EXTRA14]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_14);
    Input::Engine.KeyTitle[Input::KC_EXTRA15]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_15);
    Input::Engine.KeyTitle[Input::KC_EXTRA16]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_16);
    Input::Engine.KeyTitle[Input::KC_EXTRA17]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_17);
    Input::Engine.KeyTitle[Input::KC_EXTRA18]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_18);
    
    Input::Engine.KeyTitle[Input::KC_MMB]        = Locale::Text::KeyName(Locale::KEYNAME_MIDDLE_MOUSE);

    Input::Engine.KeyTitle[Input::KC_JOYB0]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB0);
    Input::Engine.KeyTitle[Input::KC_JOYB1]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB1);
    Input::Engine.KeyTitle[Input::KC_JOYB2]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB2);
    Input::Engine.KeyTitle[Input::KC_JOYB3]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB3);
    Input::Engine.KeyTitle[Input::KC_JOYB4]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB4);
    Input::Engine.KeyTitle[Input::KC_JOYB5]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB5);
    Input::Engine.KeyTitle[Input::KC_JOYB6]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB6);
    Input::Engine.KeyTitle[Input::KC_JOYB7]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB7);
}


int yw_loadSky(NC_STACK_ypaworld *yw, const std::string &skyname)
{
    std::string tmprsrc = Common::Env.SetPrefix("rsrc", "data:");
    
    std::string skyfilename = fmt::sprintf("data:%s", skyname);

    NC_STACK_base *sky = Utils::ProxyLoadBase(skyfilename);
    yw->_skyObject = sky;
    if ( !sky )
    {
        ypa_log_out("Couldn't create %s\n", skyfilename.c_str());
        return 0;
    }
    
    Common::Env.SetPrefix("rsrc", tmprsrc);
    
    sky->SetStatic(true); // Don't rotate sky
    sky->SetVizLimit(yw->_skyVizLimit);
    sky->SetFadeLength(yw->_skyFadeLength);
    sky->ComputeStaticFog();
    sky->MakeVBO();
    return 1;
}

void NC_STACK_ypaworld::listSaveDir(const std::string &saveDir)
{
    auto savedStatuses = _playersStats;
    auto savedCallsign = _GameShell->netPlayerName;
    auto savedMaxroboenrgy = _maxRoboEnergy;
    auto savedMaxreloadconst = _maxReloadConst;

    FSMgr::DirIter dir = uaOpenDir(saveDir);
    if ( dir )
    {
        FSMgr::iNode *dirNode;
        while ( dir.getNext(&dirNode) )
        {
            if ( dirNode->getType() == FSMgr::iNode::NTYPE_DIR )
            {
                if ( StriCmp(dirNode->getName(), ".") && StriCmp(dirNode->getName(), "..") )
                {
                    _GameShell->profiles.emplace_back();
                    ProfilesNode &profile = _GameShell->profiles.back();

                    profile.name = dirNode->getName();

                    ScriptParser::HandlersList hndls{
                        new World::Parsers::UserParser(this)
                    };
                    std::string buf = fmt::sprintf("%s/%s/user.txt\n", saveDir, dirNode->getName());

                    if ( !ScriptParser::ParseFile(buf, hndls, ScriptParser::FLAG_SKIP_UNKNOWN) )
                        ypa_log_out("Warning, cannot parse %s for time scanning\n", buf.c_str());
                    profile.fraction = 1;
                    profile.totalElapsedTime = _playersStats[1].ElapsedTime;
                }
            }
        }
    }
    else
    {
        ypa_log_out("Unknown Game-Directory %s\n", saveDir.c_str());
    }

    _playersStats = savedStatuses;
    _maxReloadConst = savedMaxreloadconst;
    _maxRoboEnergy = savedMaxroboenrgy;
    _GameShell->netPlayerName = savedCallsign;
}


void listLocaleDir(UserData *usr, const char *dirname)
{
    std::string *deflng = NULL;
    FSMgr::DirIter dir = uaOpenDir(dirname);
    if ( dir )
    {
        FSMgr::iNode *node;
        while ( dir.getNext(&node) )
        {
            std::string tmp = node->getName();
            size_t extPos = tmp.rfind(".LNG");
            if ( extPos == std::string::npos )
                extPos = tmp.rfind(".lng");;
            /*if ( !v3 )
              v3 = strstr(v18.e_name, ".dll");
            if ( !v3 )
              v3 = strstr(v18.e_name, ".DLL");*/

            if ( node->getType() == FSMgr::iNode::NTYPE_FILE && extPos != std::string::npos )
            {
                tmp.erase(extPos);

                for (size_t i = 0; i < tmp.length(); i++)
                    tmp[i] = std::toupper(tmp[i]);

                bool finded = false;

                for(const auto &x : usr->lang_dlls)
                {
                    if ( !StriCmp( x, tmp ) )
                    {
                        finded = true;
                        break;
                    }
                }

                if ( !finded )
                {
                    usr->lang_dlls.emplace_back();
                    std::string &elm = usr->lang_dlls.back();

                    elm = tmp;

                    if ( !StriCmp(elm, "language") )
                        deflng = &elm;
                }
            }
        }
    }
    else
    {
        ypa_log_out("Unknown Locale-Directory %s\n", dirname);
    }

    if ( deflng )
        usr->default_lang_dll = deflng;
}

void UserData::CloseSaveLoadMenu()
{
    disk_button->HideScreen();

    p_YW->GuiWinClose( &disk_listvw );

    if ( diskEnterFromMapSelect )
    {
        EnvMode = ENVMODE_SINGLEPLAY;
        sub_bar_button->ShowScreen();
    }
    else
    {
        EnvMode = ENVMODE_TITLE;
        titel_button->ShowScreen();
    }

    diskScreenMode = 0;
}



void NC_STACK_ypaworld::PlayIntroMovie()
{
    if ( !_movies[World::MOVIE_INTRO].empty() )
    {
        std::string buf = correctSeparatorAndExt( Common::Env.ApplyPrefix(_movies[World::MOVIE_INTRO]) );
        
        if ( IsMoviesEnabled() )
        {
            GFX::Engine.EndFrame();
            System::Movie.PlayMovie(buf, _GameShell->soundVolume);
            GFX::Engine.BeginFrame();
        }

        Input::Engine.QueryInput(&input_states);
        input_states.KbdLastHit = Input::KC_NONE;
        input_states.KbdLastDown = Input::KC_NONE;
        input_states.HotKeyID = -1;
    }
}



void CollectNetworkLevels(UserData *usr)
{
    usr->mapDescriptions.clear();
    
    for (size_t i = 0; i < usr->p_YW->_globalMapRegions.MapRegions.size(); i++)
    {
        if (usr->p_YW->_globalMapRegions.MapRegions[i].Status == TMapRegionInfo::STATUS_NETWORK)
            usr->mapDescriptions.push_back( UserData::TMapDescription(i, usr->p_YW->GetLevelName(i)) );
    }

    std::stable_sort(usr->mapDescriptions.begin(), usr->mapDescriptions.end(), UserData::TMapDescription::compare);
}


void UserData::GameShellUiOpenNetwork()
{
    titel_button->HideScreen();

    network_button->ShowScreen();

    EnvMode = ENVMODE_NETPLAY;

    p_YW->GuiWinClose( &network_listvw );
    p_YW->GuiWinOpen( &network_listvw );
}



// A copy of a file as it is, byte for byte
void CopyFileBytes(const std::string &srcPath, const std::string &dstPath)
{
    FSMgr::FileHandle *f1 = uaOpenFileAlloc(srcPath, "rb");
    if ( f1 )
    {
        FSMgr::FileHandle *f2 = uaOpenFileAlloc(dstPath, "wb");

        if ( f2 )
        {
            char buf[65536];
            size_t got;

            while ( (got = f1->read(buf, sizeof(buf))) > 0 )
                f2->write(buf, got);

            delete f2;
        }

        delete f1;
    }
}

void CopyTextFile(const std::string &srcPath, const std::string &dstPath)
{
    FSMgr::FileHandle *f1 = uaOpenFileAlloc(srcPath, "r");
    if ( f1 )
    {
        FSMgr::FileHandle *f2 = uaOpenFileAlloc(dstPath, "w");

        if ( f2 )
        {
            char line[300];

            while ( f1->gets(line, 299) )
                f2->puts(line);

            delete f2;
        }

        delete f1;
    }
}

void  UserData::SaveProfile()
{
    std::string oldsave;

    if ( diskListActiveElement )
    {
        if ( StriCmp(userNameDir, UserName) )
            DeleteDirContents(fmt::sprintf("save:%s", userNameDir));
    }
    else
    {
        profiles.emplace_back();
        ProfilesNode &profile = profiles.back();

        profile.name = userNameDir;

        std::string tmp = fmt::sprintf("save:%s", userNameDir);
        if ( !uaCreateDir(tmp) )
        {
            ypa_log_out("Unable to create directory %s\n", tmp.c_str());
            return;
        }

        disk_listvw.numEntries++;
        diskListActiveElement = disk_listvw.numEntries;
    }

    if ( ! p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", userNameDir), World::SDF_ALL) )
        ypa_log_out("Warning! Error while saving user data for %s\n", userNameDir.c_str());

    oldsave = fmt::sprintf("save:%s", UserName);

    if ( StriCmp(userNameDir, UserName) )
    {
        FSMgr::DirIter dir = uaOpenDir(oldsave);
        if ( dir )
        {
            FSMgr::iNode *node;
            while ( dir.getNext(&node) )
            {
                std::string tmp = node->getName();
                std::string srcPath = fmt::sprintf("%s/%s", oldsave, tmp);
                std::string dstPath = fmt::sprintf("save:%s/%s", userNameDir, tmp);

                if ( node->getType() != FSMgr::iNode::NTYPE_FILE )
                    continue;

                // The saved game and the won levels' ends, recordings
                if ( tmp.rfind(".sav") != std::string::npos || tmp.rfind(".SAV") != std::string::npos
                     || tmp.rfind(".won") != std::string::npos || tmp.rfind(".WON") != std::string::npos )
                    CopyFileBytes(srcPath, dstPath);
                else if ( tmp.rfind(".def") != std::string::npos || tmp.rfind(".DEF") != std::string::npos )
                    CopyTextFile(srcPath, dstPath);
            }
        }
    }

    diskScreenMode = 0;
    UserName = userNameDir;

    disk_button->HideScreen();

    p_YW->GuiWinClose( &disk_listvw );

    if ( diskEnterFromMapSelect )
    {
        EnvMode = ENVMODE_SINGLEPLAY;

        sub_bar_button->ShowScreen();
    }
    else
    {
        EnvMode = ENVMODE_TITLE;

        titel_button->ShowScreen();
    }
}

void ClearProtos(NC_STACK_ypaworld *yw)
{
    yw->_roboProtos.clear();
    yw->_buildProtos.clear();
    
    yw->_weaponProtos.clear();

    yw->_vhclProtos.clear();
}

void LoadEnabledLevels(NC_STACK_ypaworld *yw)
{
    int loaded = 0;

    if ( yw->_GameShell )
    {
        FSMgr::FileHandle *fil = uaOpenFileAlloc("env:levels.def", "r");

        if ( fil )
        {
            std::string line;
            if ( fil->ReadLine(&line) )
            {
                Stok parse(line, "\t ,");
                std::string token;
                while( parse.GetNext(&token) )
                {
                    uint32_t tmp = std::stol(token, 0, 10);

                    if (tmp < 256)
                        yw->_globalMapRegions.MapRegions[tmp].Status = TMapRegionInfo::STATUS_ENABLED;
                }
            }

            loaded = 1;
            delete fil;
        }
    }

    if ( !loaded )
    {
        yw->_globalMapRegions.MapRegions[1].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[25].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[26].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[27].Status = TMapRegionInfo::STATUS_ENABLED;
    }
}

void UserData::CreateNewProfile()
{
    if ( ! p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", UserName), World::SDF_ALL) )
        ypa_log_out("Warning! Error while saving user data for %s\n", UserName.c_str());

    if ( diskListActiveElement )
    {
        DeleteDirContents(fmt::sprintf("save:%s", userNameDir));
    }
    else
    {
        profiles.emplace_back();
        ProfilesNode &profile = profiles.back();

        profile.name = userNameDir;

        std::string dirPath = fmt::sprintf("save:%s", userNameDir);

        if ( !uaCreateDir(dirPath) )
        {
            ypa_log_out("Unable to create directory %s\n", dirPath.c_str());
            return;
        }

        UserName = userNameDir;
        disk_listvw.numEntries++;
        diskListActiveElement = disk_listvw.numEntries;
    }

    p_YW->_levelInfo.Buddies.clear();

    ClearProtos(p_YW);
    
    p_YW->_tutorialHints.Stop();

    if ( p_YW->ProtosInit() )
    {
        for (TMapRegionInfo &mp : p_YW->_globalMapRegions.MapRegions)
        {
            if ( mp.Status != TMapRegionInfo::STATUS_NONE && mp.Status != TMapRegionInfo::STATUS_NETWORK )
                mp.Status = TMapRegionInfo::STATUS_DISABLED;
        }

        LoadEnabledLevels(p_YW);

        p_YW->_maxRoboEnergy = 0;
        p_YW->_maxReloadConst = 0;

        p_YW->_playersStats.fill(World::TPlayerStatus());

        diskScreenMode = 0;

        p_YW->_beamEnergyCapacity = p_YW->_beamEnergyStart;

        p_YW->_levelInfo.JodieFoster.fill(0);

        sgmSaveExist = 0;
        continueLevel = -1;

        disk_button->HideScreen();

        p_YW->GuiWinClose( &disk_listvw );

        EnvMode = ENVMODE_SINGLEPLAY;

        sub_bar_button->ShowScreen();
    }
    else
    {
        ypa_log_out("Warning, error while parsing prototypes\n");
    }
}

void UserData::HideConfirmDialog()
{
    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1300;
    confirm_button->Disable(&btnState);

    btnState.butID = 1301;
    confirm_button->Disable(&btnState);

    confirm_button->HideScreen();

    confirmMode = 0;
}

// C++14 needs the definitions, for code that takes them by reference.
constexpr int32_t TViewDistance::ALL_SECTORS;
constexpr float TViewDistance::UNLIMITED;

TViewDistance NC_STACK_ypaworld::GetViewDistance() const
{
    if ( _viewDistanceSet )
        return _viewDistance;

    return GetDefaultViewDistance();
}

TViewDistance NC_STACK_ypaworld::GetDefaultViewDistance() const
{
    // The fog is each object's own then, which the world sets to these
    // distances when it loads them.
    TViewDistance view;
    view.SectorRadius = (_renderSectors - 1) / 2;
    view.FogStart = _normalVizLimit - _normalFadeLength;
    view.FogEnd = _normalVizLimit;
    return view;
}

void NC_STACK_ypaworld::SetViewDistance(const TViewDistance &view)
{
    _viewDistance = view;

    if ( _viewDistance.SectorRadius < 0 )
        _viewDistance.SectorRadius = 0;

    if ( _viewDistance.ClipDistance < 1.0 )
        _viewDistance.ClipDistance = 1.0;

    // GL fog needs an end beyond its start.
    if ( _viewDistance.FogEnd < 1.0 )
        _viewDistance.FogEnd = 1.0;

    if ( _viewDistance.FogStart > _viewDistance.FogEnd - 1.0 )
        _viewDistance.FogStart = _viewDistance.FogEnd - 1.0;

    // And the fading too, not to divide by zero.
    if ( _viewDistance.FadeOutEnd < 1.0 )
        _viewDistance.FadeOutEnd = 1.0;

    if ( _viewDistance.FadeOutStart > _viewDistance.FadeOutEnd - 1.0 )
        _viewDistance.FadeOutStart = _viewDistance.FadeOutEnd - 1.0;

    _viewDistanceSet = true;
}

void NC_STACK_ypaworld::ResetViewDistance()
{
    _viewDistanceSet = false;
}

int32_t NC_STACK_ypaworld::GetMapSectorRadius() const
{
    return std::max(_mapSize.x + _mapSize.y - 2, 0);
}

float NC_STACK_ypaworld::GetMapDiagonal() const
{
    return vec2d(_mapSize.x, _mapSize.y).length() * World::CVSectorLength;
}

//Options OK
void UserData::ApplyOptions()
{
    NC_STACK_ypaworld *yw = p_YW;

    bool forceChange = false;

    if ( _settingsChangeOptions & 0x200 )
    {
        if ( confSoundFlags & World::SF_CDSOUND )
        {
            soundFlags |= World::SF_CDSOUND;
            yw->_preferences |= World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(true);
            if ( shelltrack )
            {
                SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                SFXEngine::SFXe.PlayMusicTrack();
            }
        }
        else
        {
            soundFlags &= ~World::SF_CDSOUND;
            yw->_preferences &= ~World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.StopMusicTrack();
            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(false);
        }

    }

    if ( _settingsChangeOptions & 2 )
    {
        if ( confSoundFlags & World::SF_INVERTLR )
        {
            soundFlags |= World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(1);
        }
        else
        {
            soundFlags &= ~World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(0);
        }
    }

    if ( _settingsChangeOptions & 8 )
    {
        if ( confGFXFlags & World::GFX_FLAG_SKYRENDER )
        {
            GFXFlags |= World::GFX_FLAG_SKYRENDER;
            yw->setYW_skyRender(1);
        }
        else
        {
            GFXFlags &= ~World::GFX_FLAG_SKYRENDER;
            yw->setYW_skyRender(0);
        }

    }

    if ( _settingsChangeOptions & 0x20 )
    {
        enemyIndicator = confEnemyIndicator;

        if ( enemyIndicator )
            p_YW->_preferences |= World::PREF_ENEMYINDICATOR;
        else
            p_YW->_preferences &= ~World::PREF_ENEMYINDICATOR;
    }

    if ( _settingsChangeOptions & 0x40 )
    {
        fxnumber = confFxNumber;
        yw->_fxLimit = fxnumber;
    }

    if ( _settingsChangeOptions & 0x100 )
    {
        musicVolume = confMusicVolume;

        SFXEngine::SFXe.SetMusicVolume(confMusicVolume);
    }

    if ( _settingsChangeOptions & 0x80 )
    {
        soundVolume = confSoundVolume;
        SFXEngine::SFXe.setMasterVolume(soundVolume);
    }

    if ( _settingsChangeOptions & 0x1000 )
    {
        if ( !conf3DGuid.empty() )
        {
            if ( StriCmp(conf3DGuid, win3d_guid) )
            {
                win3d_name = conf3DName;
                win3d_guid = conf3DGuid;

                GFX::Engine.SetDeviceByGUID(win3d_guid); // Saved, for the next start

                forceChange = true;
            }
        }
    }

    if ( _settingsChangeOptions & 0x400 )
    {
        if ( confGFXFlags & World::GFX_FLAG_WINDOWED )
            GFXFlags |= World::GFX_FLAG_WINDOWED;
        else
            GFXFlags &= ~World::GFX_FLAG_WINDOWED;

        forceChange = true;
    }

    if ( _settingsChangeOptions & 0x2000 )
    {
        if ( confGFXFlags & World::GFX_FLAG_VSYNC )
            GFXFlags |= World::GFX_FLAG_VSYNC;
        else
            GFXFlags &= ~World::GFX_FLAG_VSYNC;

        GFX::Engine.SetVsync( IsVsyncFlag() );
    }

    if ( _settingsChangeOptions & OPT_CHG_FILTERING )
        GFX::Engine.SetTextureFiltering(confFiltering);

    if ( _settingsChangeOptions & OPT_CHG_MOVIES )
        yw->SetMoviesEnabled(confMovies);

    if ( _settingsChangeOptions & OPT_CHG_MAXFPS )
        GFX::Engine.fpsLimitter(confMaxFps);

    if ( _settingsChangeOptions & OPT_CHG_CHANNELS )
        SFXEngine::SFXe.SetChannels(confSoundChannels);

    // Host stations take it when they are made, so from the next level
    if ( _settingsChangeOptions & OPT_CHG_AITIMELINE )
        yw->SetAITimeline(confAITimeline);

    if ( forceChange )
    {
        yw->SetWindowed( IsWindowedFlag() );
    }

    _settingsChangeOptions = 0;
    EnvMode = ENVMODE_TITLE;

    video_button->HideScreen();

    if ( d3d_listvw.IsOpen() )
        p_YW->GuiWinClose( &d3d_listvw );

    NC_STACK_button::WidgetState btnState;
    btnState.state = 2;
    btnState.butID = 1172;
    video_button->SetState(&btnState);

    titel_button->ShowScreen();
}


void UserData::StartNetGame()
{
    uamessage_load lvlMsg;
    lvlMsg.msgID = UAMSG_LOAD;
    lvlMsg.owner = 0;
    lvlMsg.level = netLevelID;

    p_YW->NetBroadcastMessage(&lvlMsg, sizeof(lvlMsg), true);

    p_YW->_netDriver->FlushBroadcastBuffer();

    envAction.action = EnvAction::ACTION_NETPLAY;
    envAction.params[0] = netLevelID;
    network_listvw.firstShownEntries = 0;
    envAction.params[1] = netLevelID;

    int lock = 1;
    p_YW->_netDriver->LockSession(&lock);

    yw_NetPrintStartInfo();
}


int UserData::IsSaveGameOutdated()
{
    FSMgr::FileHandle *fl = uaOpenFileAlloc(fmt::sprintf("save:%s/sgisold.txt",UserName), "r");
    if ( !fl )
        return 0;

    delete fl;
    return 1;
}



void UserData::ShowToolTip(int id)
{
    switch (id)
    {
    default:
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_PROFILESCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_INPUTSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_VIDEOSCREEN);
        break;

    case 1005:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SOUNDSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_QUIT);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGSCREEN);
        break;

    case 1011:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_BRIEFRESET);
        break;

    case 1013:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_BRIEFFAST);
        break;

    case 1014:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_PLAYLEVEL);
        break;

    case 1015:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LOADLAST);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP:
    case 1052:
    case 1107:
    case 1167:
    case 1218:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_HELP);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_CONTINUE:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_CONTINUE);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LVLSELECTMAP);
        break;

    case 1019:
        if ( p_YW->_levelInfo.State == TLevelInfo::STATE_MENU )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_TOMAIN);
        else if ( p_YW->_levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_EXITDEBRIEF);
        else
            p_YW->SetShowingTooltip(Locale::TIP_MENU_BACKTOLVLSEL);
        break;

    case 1020:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_GOTOLOADSAVE);
        break;

    case 1050:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_USEJOY);
        break;

    case 1051:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_APPLY);
        break;

    case 1053:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_RESET);
        break;

    case 1054:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_CANCEL);
        break;

    case 1055:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_FFJOY);
        break;

    case 1056:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_REMOVEKEY);
        break;

    case 1061:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_ALTJOY);
        break;

    case 1101:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LOADUSER);
        break;

    case 1102:
        p_YW->SetShowingTooltip(Locale::TIP_PROFILE_DEL);
        break;

    case 1103:
        p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_NEW);
        break;

    case 1104:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SAVEUSER);
        break;

    case 1105:
        switch ( diskScreenMode )
        {
        case 1:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_SAVETHIS);
            break;
        case 2:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_LOADTHIS);
            break;
        case 3:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_NEWTHIS);
            break;
        case 4:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_DELTHIS);
            break;
        default:
            break;
        }
        break;

    case 1106:
        switch ( diskScreenMode )
        {
        case 0:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_CLOSE);
            break;
        case 1:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELSAVE);
            break;
        case 2:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELLOAD);
            break;
        case 3:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELNEW);
            break;
        case 4:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELDEL);
            break;
        default:
            break;
        }
        break;
    case 1151:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_CHANNELS);
        break;

    case 1152:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_FXVOLUME);
        break;

    case 1154:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MUSICVOLUME);
        break;

    case 1159:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_EXPLODES);
        break;

    case 1160:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_SKY);
        break;

    case 1161:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_APPLY);
        break;

    case 1162:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_CLOSE);
        break;

    case 1163:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_ENEMYSIGNS);
        break;

    case 1164:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MUSIC);
        break;

    case 1166:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_WINDOWED); // windowed
        break;

    case 1174:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_TEXFILTER);
        break;

    case 1175:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MOVIES);
        break;

    case 1177:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MAXFPS);
        break;

    case 1179:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_SNDCHANNELS);
        break;

    case 1181:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_AITIMELINE);
        break;

    case 1172:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELRENDERER);
        break;

    case 1201:
        switch ( netSelMode )
        {
        case NETSCREEN_MODE_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELPROV);
            break;
        case NETSCREEN_SESSION_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELSESS);
            break;
        case NETSCREEN_ENTER_NAME:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETPLNAME);
            break;
        case NETSCREEN_CHOOSE_MAP:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELLVL);
            break;
        case NETSCREEN_INSESSION:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSTART);
            break;
        default:
            break;
        }
        break;

    case 1202:
        if ( netSelMode == NETSCREEN_SESSION_SELECT )
        {
            p_YW->SetShowingTooltip(Locale::TIP_MENU_SELNEWLEVEL);
        }
        else if ( netSelMode == NETSCREEN_INSESSION )
        {
            p_YW->SetShowingTooltip(Locale::TIP_MENU_SENDTOALL);
        }
        break;

    case 1203:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_NETCLOSE);
        break;

    case 1205:
        switch ( netSelMode )
        {
        case NETSCREEN_SESSION_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKPLAYER);
            break;
        case NETSCREEN_ENTER_NAME:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKPROV);
            break;
        case NETSCREEN_CHOOSE_MAP:
        case NETSCREEN_INSESSION:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKSESS);
            break;
        default:
            break;
        }
        break;

    case 1206:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELRESIST);
        break;

    case 1207:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELGHOR);
        break;

    case 1208:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELMYKO);
        break;

    case 1209:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELTAER);
        break;

    case 1219:
        if ( rdyStart )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NOTREADY);
        else
            p_YW->SetShowingTooltip(Locale::TIP_MENU_READY);
        break;

    case 1225:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SENDMSG);
        break;

    case 1250:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGCHOOSE);
        break;

    case 1251:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGEXIT);
        break;

    case 1252:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_HELP);
        break;
    }
}

// Go to options menu
void UserData::ShowOptionsMenu()
{
    titel_button->HideScreen();

    video_button->ShowScreen();

    EnvMode = ENVMODE_SETTINGS;
}

void UserData::ShowSaveLoadMenu()
{
    titel_button->HideScreen();

    disk_button->ShowScreen();

    EnvMode = ENVMODE_SELPLAYER;

    for( ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++ )
    {
        if ( !StriCmp(it->name, UserName))
        {
            it->totalElapsedTime = p_YW->_playersStats[1].ElapsedTime;
            break;
        }
    }
    p_YW->GuiWinClose( &disk_listvw );
    p_YW->GuiWinOpen( &disk_listvw );
}

void UserData::ShowInputSettings()
{
    titel_button->HideScreen();

    button_input_button->ShowScreen();

    EnvMode = ENVMODE_INPUT;
    p_YW->GuiWinClose( &input_listview );
    p_YW->GuiWinOpen( &input_listview );
}

void PlayChatVoiceSample(NC_STACK_ypaworld *yw, int arg)
{
    UserData *usr = yw->_GameShell;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    std::string wavName;
    if ( usr->default_lang_dll )
        wavName = fmt::sprintf("sounds/speech/%s/9%d.wav", *usr->default_lang_dll, arg);
    else
        wavName = fmt::sprintf("sounds/speech/language/9%d.wav", arg);

    if ( !uaFileExist(std::string("rsrc:") + wavName) )
        wavName = fmt::sprintf("sounds/speech/language/9%d.wav", arg);
    
    NC_STACK_sample *&pSmpl = usr->samples1.at(World::SOUND_ID_CHAT);

    if ( pSmpl )
    {
        SFXEngine::SFXe.stopSound(&usr->samples1_info, World::SOUND_ID_CHAT);
        SFXEngine::SFXe.ForceStopSource(&usr->samples1_info, World::SOUND_ID_CHAT);
        pSmpl->Delete();
        pSmpl = NULL;
    }

    pSmpl = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, wavName}});
    if ( pSmpl )
    {
        TSoundSource &pSnd = usr->samples1_info.Sounds.at(World::SOUND_ID_CHAT);
        pSnd.Volume = 500;
        pSnd.Pitch = 0;
        pSnd.PSample = pSmpl->GetSampleData();
        
        SFXEngine::SFXe.startSound(&usr->samples1_info, World::SOUND_ID_CHAT);
    }

    Common::Env.SetPrefix("rsrc", oldRsrc);
}

void AddChatMessage(NC_STACK_ypaworld *yw, const std::string &sender, const std::string &text)
{
    UserData *usr = yw->_GameShell;

    if ( StriCmp(sender, usr->lastSender) )
    {
        // TODO(multiplayer): Drops the oldest line whenever the sender changes, even when the
        // buffer is not full. Probably meant size() >= 31, as below.
        if ( usr->msgBuffers.size() )
            usr->msgBuffers.pop_front();

        usr->msgBuffers.push_back( fmt::sprintf("> %s:", sender));
        usr->lastSender = sender;
    }

    if ( usr->msgBuffers.size() >= 31 )
        usr->msgBuffers.pop_front();

    usr->msgBuffers.push_back( text );
   
    if ( usr->netSelMode == UserData::NETSCREEN_INSESSION )
    {
        int firstShown = usr->msgBuffers.size() - 6;

        if ( firstShown < 0 )
            firstShown = 0;

        yw->_GameShell->network_listvw.firstShownEntries = firstShown;


        yw->_GameShell->network_listvw.numEntries = yw->_GameShell->msgBuffers.size();

        int shownCount;

        if ( usr->network_listvw.numEntries >= 6 )
            shownCount = 6;
        else
            shownCount = usr->network_listvw.numEntries;

        yw->_GameShell->network_listvw.shownEntries = shownCount;
    }
}


void UserData::yw_returnToTitle()
{
    yw_calcPlayerScore(p_YW);
    p_YW->FreeDebrief();

    sub_bar_button->HideScreen();

    titel_button->ShowScreen();

    EnvMode = ENVMODE_TITLE;
    returnToTitle = false;
}

void UserData::ShowLanguageMenu()
{
    titel_button->HideScreen();

    locale_button->ShowScreen();

    EnvMode = ENVMODE_SELLOCALE;
    p_YW->GuiWinClose( &local_listvw );
    p_YW->GuiWinOpen( &local_listvw );

    int i = 0;
    for(const auto &x : lang_dlls)
    {
        if ( &x == default_lang_dll )
            break;

        i++;
    }

    local_listvw.selectedEntry = i;
}

void CancelBriefing(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.Stage = TBriefengScreen::STAGE_CANCEL;
}

void UserData::ShowConfirmDialog( int mode, const std::string &txt1, const std::string &txt2, int singleButton)
{
    confirmMode = mode;
    confirmText1 = txt1;
    confirmText2 = txt2;
    confirmSingleButton = singleButton;

    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1300;
    confirm_button->Enable(&btnState);


    NC_STACK_button::WidgetPlacement placement;

    if ( singleButton )
    {
        placement.butID = 1300;
        placement.xpos = p_YW->_screenSize.x * 0.4375;
        placement.ypos = -1;
        placement.width = -1;
        //v11 = -1;
        confirm_button->setXYWidth(&placement);
    }
    else
    {
        btnState.butID = 1301;
        confirm_button->Enable(&btnState);

        placement.butID = 1300;
        placement.xpos = p_YW->_screenSize.x * 0.25;
        placement.ypos = -1;
        placement.width = -1;
        //v11 = -1;
        confirm_button->setXYWidth(&placement);

        placement.butID = 1301;
        placement.xpos = p_YW->_screenSize.x * 0.625;
        confirm_button->setXYWidth(&placement);
    }

    confirm_button->SetText(1302, txt1);

    if ( !txt2.empty() )
        confirm_button->SetText(1303, txt2);
    else
        confirm_button->SetText(1303, " ");

    confirm_button->ShowScreen();
}

void BriefRequestPlayLevel(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.Stage = TBriefengScreen::STAGE_PLAYLEVEL;
}

void BriefRestart(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_RESTART;
}

void BriefFastForward(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_FAST;
}


void UserData::InputConfCancel()
{
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCode = konf.PKeyCodeBkp;
        konf.NKeyCode = konf.NKeyCodeBkp;
    }
}

void UserData::InputPageCancel()
{
    EnvMode = ENVMODE_TITLE;
    InputConfCancel();

    NC_STACK_button::WidgetState btnState;
    btnState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
    btnState.state = 2;
    sub_bar_button->SetState(&btnState);

    inputChangedParts = 0;

    btnState.butID = 1050;
    btnState.state = (joystickEnabled == false) + 1;
    button_input_button->SetState(&btnState);

    btnState.state = (altJoystickEnabled == false) + 1;
    btnState.butID = 1061;
    button_input_button->SetState(&btnState);

    btnState.butID = 1055;
    btnState.state = ((p_YW->_preferences & World::PREF_FFDISABLE) != 0) + 1;
    button_input_button->SetState(&btnState);

    button_input_button->HideScreen();

    p_YW->GuiWinClose( &input_listview );

    titel_button->ShowScreen();
}

void UserData::InputConfCopyToBackup()
{
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCodeBkp = konf.PKeyCode;
        konf.NKeyCodeBkp = konf.NKeyCode;
    }
}

int NC_STACK_ypaworld::InputConfigLoadDefault()
{
    std::string file = fmt::sprintf("data:settings/%s/input.def", Locale::Text::GetLocaleName());

    ScriptParser::HandlersList hndls
    {
        new World::Parsers::InputParser(this)
    };

    return ScriptParser::ParseFile(file, hndls, 0);
}

void UserData::InputConfigRestoreDefault()
{
    if ( p_YW->InputConfigLoadDefault() )
    {
        for(TInputConf &konf : InputConfig)
        {
            konf.PKeyCodeDef = konf.PKeyCode;
            konf.NKeyCodeDef = konf.NKeyCode;
        }
    }
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCode = konf.PKeyCodeDef;
        konf.NKeyCode = konf.NKeyCodeDef;
    }
}

constexpr std::array<int, 7> UserData::FpsLimits;

int UserData::FpsLimitStep(int fps)
{
    if ( fps == 0 )
        return 0;

    if ( fps < 0 || fps > 1000 )
        return FpsLimits.size() - 1;

    int step = 1;
    for (size_t i = 2; i < FpsLimits.size() - 1; i++)
    {
        if ( std::abs(FpsLimits.at(i) - fps) < std::abs(FpsLimits.at(step) - fps) )
            step = i;
    }
    return step;
}

std::string UserData::FpsLimitText(int fps)
{
    if ( fps == 0 )
        return Locale::Text::Dialogs(Locale::DLG_S_FPSAUTO);

    if ( fps < 0 || fps > 1000 )
        return Locale::Text::Dialogs(Locale::DLG_S_FPSOFF);

    return fmt::sprintf("%d", fps);
}

std::string UserData::AITimelineText(int32_t ms)
{
    const std::string &min = Locale::Text::Dialogs(Locale::DLG_S_MINUTES);

    if ( ms % 60000 == 0 )
        return fmt::sprintf("%d %s", ms / 60000, min);

    return fmt::sprintf("%d:%02d %s", ms / 60000, (ms / 1000) % 60, min);
}

void UserData::SetEngineOptionWidgets()
{
    confFiltering = GFX::Engine.GetTextureFiltering();
    confMovies = p_YW->IsMoviesEnabled();
    confMaxFps = GFX::Engine.GetFpsLimitSetting();
    confSoundChannels = SFXEngine::SFXe.GetChannels();
    confAITimeline = p_YW->GetAITimeline();

    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1174;
    btnState.state = (!confFiltering) + 1;
    video_button->SetState(&btnState);

    btnState.butID = 1175;
    btnState.state = (!confMovies) + 1;
    video_button->SetState(&btnState);

    NC_STACK_button::Slider *slider = video_button->GetSliderData(1177);
    slider->value = FpsLimitStep(confMaxFps);
    video_button->Refresh(1177);

    slider = video_button->GetSliderData(1179);
    slider->value = confSoundChannels;
    video_button->Refresh(1179);

    // The slider has whole minutes; the setting may lie between them
    slider = video_button->GetSliderData(1181);
    slider->value = std::max<int>(slider->min, std::min<int>((confAITimeline + 30000) / 60000, slider->max));
    video_button->Refresh(1181);
}

void UserData::CancelOptions()
{
    _settingsChangeOptions = 0;
    EnvMode = ENVMODE_TITLE;

    SetEngineOptionWidgets();

    video_button->SetText(1172, win3d_name);

    conf3DGuid = win3d_guid;
    conf3DName = win3d_name;

    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1151;
    btnState.state = ((soundFlags & World::SF_INVERTLR) == 0) + 1;
    video_button->SetState(&btnState);

    btnState.state = ((soundFlags & World::SF_CDSOUND) == 0) + 1;
    btnState.butID = 1164;
    video_button->SetState(&btnState);

    btnState.state = ((GFXFlags & World::GFX_FLAG_SKYRENDER) == 0) + 1;
    btnState.butID = 1160;
    video_button->SetState(&btnState);

    btnState.butID = 1166;
    btnState.state = (!IsWindowedFlag()) + 1;
    video_button->SetState(&btnState);

    btnState.butID = 1173;
    btnState.state = (!IsVsyncFlag()) + 1;
    video_button->SetState(&btnState);

    btnState.state = (enemyIndicator == 0) + 1;
    btnState.butID = 1163;
    video_button->SetState(&btnState);

    NC_STACK_button::Slider *tmp = video_button->GetSliderData(1159);
    tmp->value = fxnumber;
    video_button->Refresh(1159);

    tmp = video_button->GetSliderData(1152);
    tmp->value = soundVolume;
    video_button->Refresh(1152);

    tmp = video_button->GetSliderData(1154);
    tmp->value = musicVolume;
    video_button->Refresh(1154);

    video_button->HideScreen();

    if ( d3d_listvw.IsOpen() )
        p_YW->GuiWinClose( &d3d_listvw );

    titel_button->ShowScreen();

    btnState.state = 2;
    btnState.butID = 1172;
    video_button->SetState(&btnState);
}

void  UserData::UpdateSelected3DDevFromList()
{
    std::string name;
    std::string guid;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();
    
    if ((size_t)d3d_listvw.selectedEntry < devices.size())
    {
        const GFX::TGFXDeviceInfo &dev = devices.at(d3d_listvw.selectedEntry);
        name = dev.Caption();
        guid = dev.guid;
    }

    conf3DName = name;
    conf3DGuid = guid;

    video_button->SetText(1172, name);
}

void UserData::LoadProfile()
{
    if ( diskListActiveElement )
    {
        ProfileList::iterator it = profiles.begin();

        for (int i = 0; i < diskListActiveElement - 1; i++) // check usr->field_1612 - 1
            it++;

        EnvMode = ENVMODE_SINGLEPLAY;
printf("%s, %d\n",__FILE__, __LINE__);
        p_YW->LoadSettings(fmt::sprintf("%s/user.txt", it->name),
                           it->name,
                           World::SDF_ALL,
                           true);
printf("%s, %d\n",__FILE__, __LINE__);
        UserName = it->name;
        userNameDir = it->name;


        diskScreenMode = 0;
        sgmSaveExist = 0;
        continueLevel = -1;

        disk_button->HideScreen();

        p_YW->GuiWinClose( &disk_listvw );

        sub_bar_button->ShowScreen();
    }
}

void DeleteDirContents(const std::string &path)
{
    FSMgr::DirIter dir = uaOpenDir(path);
    if ( dir )
    {
        FSMgr::iNode *node;

        while ( dir.getNext(&node) )
        {
            // TODO(bug): The || sends every subdirectory except . and .. to uaDeleteFile too,
            // which cannot remove it. Probably meant && (or the file test alone).
            if ( node->getType() == FSMgr::iNode::NTYPE_FILE || (node->getName().compare(".") && node->getName().compare("..")) )
                uaDeleteFile(fmt::sprintf("%s/%s", path, node->getName()));
        }
    }
}

void UserData::DeleteProfile()
{
    if ( diskListActiveElement )
    {
        if ( StriCmp(userNameDir, UserName) )
        {

            ProfileList::iterator it = profiles.begin();

            for (int i = 0; i < diskListActiveElement - 1; i++) // check usr->field_1612 - 1
                it++;

            ProfileList::iterator nextIt = std::next(it);

            bool HasElements;

            if ( nextIt != profiles.end() )
                HasElements = true;
            else
            {
                nextIt = std::prev(it);
                HasElements = false;
            }

            std::string dirPath = fmt::sprintf("save:%s", it->name);

            DeleteDirContents(dirPath);

            uaDeleteDir(dirPath);

            profiles.erase(it);

            disk_listvw.numEntries--;
            if ( profiles.empty() )
            {
                diskListActiveElement = 0;
                userNameDir = "NEWUSER";
            }
            else
            {
                if ( !HasElements )
                    diskListActiveElement--;

                userNameDir = nextIt->name;
            }

            userNameDirCursor = userNameDir.size();

            if ( diskListActiveElement )
                disk_listvw.PosOnSelected(diskListActiveElement - 1);

            diskScreenMode = 0;

            disk_button->HideScreen();

            p_YW->GuiWinClose( &disk_listvw );

            if ( diskEnterFromMapSelect )
            {
                EnvMode = ENVMODE_SINGLEPLAY;
                sub_bar_button->ShowScreen();
            }
            else
            {
                EnvMode = ENVMODE_TITLE;
                titel_button->ShowScreen();
            }
        }
    }
}

void UserData::ApplySelectedLanguage()
{
    Engine::StringList::iterator lang = std::next(lang_dlls.begin(), local_listvw.selectedEntry);

    prev_lang = &(*lang);

    if ( prev_lang != default_lang_dll )
    {
        if ( prev_lang )
        {
            default_lang_dll = prev_lang;
            p_YW->ReloadLanguage();
        }
    }

    EnvMode = ENVMODE_TITLE;

    prev_lang = default_lang_dll;

    locale_button->HideScreen();

    p_YW->GuiWinClose( &local_listvw );

    titel_button->ShowScreen();
}

void UserData::ExitFromLanguageMenu()
{
    EnvMode = ENVMODE_TITLE;

    prev_lang = default_lang_dll;

    locale_button->HideScreen();

    p_YW->GuiWinClose( &local_listvw );

    titel_button->ShowScreen();
}


int NC_STACK_ypaworld::IsCheatKeyHit(TInputState *struc, int kkode)
{
    return struc->KbdLastHit == kkode && ( (struc->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD) || _easyCheatKeys );
}

void UserData::ShowAbout()
{
    titel_button->HideScreen();

    about_button->ShowScreen();

    EnvMode = ENVMODE_ABOUT;
}


int IsValidFileNameChar(char ch)
{
    if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
        return 0;

    return 1;
}




void UserData::ConnectToServer(std::string connStr){
    fmt::printf("Connectiong to: %s\n", connStr);
    if ( p_YW->_netDriver->Connect(connStr) )
    {
        if (p_YW->_netDriver->HasLobby())
        {
            netSelMode = NETSCREEN_SESSION_SELECT;
            netSel = -1;
            network_listvw.firstShownEntries = 0;

            p_YW->GuiWinOpen( &network_listvw );
        }
        else
        {
            JoinLobbyLessGame();
        }
    }
    else
    {
        printf("Can't connect: Time OUT\n");
    }
}


void UserData::GameShellUiHandleInput()
{
    int confirmShown = 0;

    if ( Input->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
        SFXEngine::SFXe.startSound(&samples1_info, 3);

    if ( netSelMode != NETSCREEN_MODE_SELECT )
        yw_HandleNetMsg(p_YW);

    if ( netSelMode == NETSCREEN_SESSION_SELECT )
    {
        if ( p_YW->_netDriver->GetProvType() == 4 )
        {
            if ( modemAskSession )
            {
                if ( Input->KbdLastHit == Input::KC_SPACE )
                {
                    GFX::Engine.BeginSystemDialog(NULL);
                    p_YW->_netDriver->EnumSessions(NULL);
                    GFX::Engine.EndSystemDialog(NULL);
                }
            }
        }
        else if ( p_YW->_netDriver->GetProvType() != 3 || Input->KbdLastHit == Input::KC_SPACE )
        {
            p_YW->_netDriver->EnumSessions(NULL);
        }
    }

    NC_STACK_button::WidgetState btnState;
    NC_STACK_button::WidgetState enableState;

    enableState.state = 0;
    enableState.butID = 1015;
    sub_bar_button->Disable(&enableState);
    enableState.butID = 1011;
    sub_bar_button->Disable(&enableState);
    enableState.butID = 1019;
    sub_bar_button->Disable(&enableState);
    enableState.butID = 1014;
    sub_bar_button->Disable(&enableState);
    enableState.butID = 1013;
    sub_bar_button->Disable(&enableState);
    enableState.butID = 1020;
    sub_bar_button->Disable(&enableState);

    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;
    titel_button->Disable(&enableState);
    enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_CONTINUE;
    titel_button->Disable(&enableState);

    NC_STACK_button::WidgetPlacement placement;
    placement.ypos = -1;
    placement.width = -1;
    //v394 = -1;
    placement.butID = 1014;
    placement.xpos = 0;
    sub_bar_button->setXYWidth(&placement);

    placement.butID = 1019;
    placement.xpos = p_YW->_screenSize.x - subBarButtonWidth;
    sub_bar_button->setXYWidth(&placement);

    placement.butID = 1011;
    placement.xpos = buttonsSpace + subBarButtonWidth;
    sub_bar_button->setXYWidth(&placement);

    sub_bar_button->SetText(1019,  Locale::Text::GlobMap(Locale::GLOBMAP_GOBACK));

    if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
    {
        if ( p_YW->_levelInfo.State != TLevelInfo::STATE_DEBRIEFING && !GameIsOver )
        {
            enableState.butID = 1014;
            sub_bar_button->Enable(&enableState);

            placement.butID = 1014;
            placement.xpos = p_YW->_screenSize.x - subBarButtonWidth;
            sub_bar_button->setXYWidth(&placement);

            placement.butID = 1019;
            placement.xpos = 0;
            sub_bar_button->setXYWidth(&placement);

            sub_bar_button->SetText(1019, Locale::Text::Advanced(Locale::ADV_BACK));
        }

        if ( p_YW->_levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
        {
            enableState.butID = 1011;
            sub_bar_button->Enable(&enableState);

            placement.butID = 1011;
            placement.xpos = 0;
            sub_bar_button->setXYWidth(&placement);

            sub_bar_button->SetText(1019, Locale::Text::Advanced(Locale::ADV_CONTINUE));
        }

        enableState.butID = 1019;
        sub_bar_button->Enable(&enableState);

        button_input_button->HideScreen();
        video_button->HideScreen();
        disk_button->HideScreen();
        locale_button->HideScreen();
        network_button->HideScreen();

        p_YW->GuiWinClose( &input_listview );
        p_YW->GuiWinClose( &disk_listvw );
        p_YW->GuiWinClose( &local_listvw );
        p_YW->GuiWinClose( &network_listvw );
    }
    else if ( EnvMode == ENVMODE_TITLE )
    {
        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
        titel_button->Enable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;
        titel_button->Enable(&enableState);

        enableState.butID = 1001;
        titel_button->Enable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
        if ( lang_dlls.size() > 1 )
            titel_button->Enable(&enableState);
        else
            titel_button->Disable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;
        titel_button->Enable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
        titel_button->Enable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
        titel_button->Enable(&enableState);

        enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;
        titel_button->Enable(&enableState);

        if ( UnfinishedLevel() )
        {
            enableState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_CONTINUE;
            titel_button->Enable(&enableState);
        }
    }
    else if ( EnvMode == ENVMODE_SINGLEPLAY || EnvMode == ENVMODE_TUTORIAL )
    {
        if ( !sgmSaveExist )
        {
            if ( HasSaveGame(UserName) )
                sgmSaveExist = 1;
            else
                sgmSaveExist = 2; // No, and We check it, so do not check it twice
        }

        if ( sgmSaveExist == 1 )
        {
            enableState.butID = 1015;
            sub_bar_button->Enable(&enableState);
        }

        enableState.butID = 1019;
        sub_bar_button->Enable(&enableState);

        enableState.butID = 1020;
        sub_bar_button->Enable(&enableState);
    }

    if ( confirmMode )
        confirmShown = 1;

    NC_STACK_button::ResCode r = confirm_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1350 ) // OK
        {
            switch ( confirmMode )
            {
            case 1:
            {
                sgmSaveExist = 0;
                p_YW->_isNetGame = false;

                sub_bar_button->HideScreen();

                envAction.action = EnvAction::ACTION_LOAD;
                envAction.params[0] = 0;
            }
            break;

            case 2:
                StartNetGame();
                break;
            case 3:
                SaveProfile();
                break;
            case 4:
                HideConfirmDialog();
                break;
            case 6:
                CreateNewProfile();
                break;
            default:
                break;
            }
            HideConfirmDialog();
        }
        else if ( r.code == 1351 ) // Cancel
        {
            if ( confirmMode == 3 || confirmMode == 6 )
                diskScreenMode = 0;

            HideConfirmDialog();
        }
    }

    if ( confirmMode )
    {
        if ( Input->HotKeyID == 24 )
        {
            if ( confirmMode == 3 || confirmMode == 6 )
                diskScreenMode = 0;
            HideConfirmDialog();
        }
        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
            printf("Line = %d\n", __LINE__);
            switch ( confirmMode )
            {
            case 1:
            {
                sgmSaveExist = 0;
                p_YW->_isNetGame = false;
                sub_bar_button->HideScreen();
                envAction.action = EnvAction::ACTION_LOAD;
                envAction.params[0] = 0;
            }
            break;
            case 2:
                StartNetGame();
                break;
            case 3:
                SaveProfile();
                break;
            case 4:
                HideConfirmDialog();
                break;
            case 6:
                CreateNewProfile();
                break;
            default:
                break;
            }
            HideConfirmDialog();
        }
    }

    if ( confirmShown )
    {
        Input->ClickInf.flag = 0;
        Input->KbdLastHit = Input::KC_NONE;
        Input->KbdLastDown = Input::KC_NONE;
        Input->chr = 0;
        Input->HotKeyID = -1;
    }    

    if ( EnvMode == ENVMODE_TITLE && Input->HotKeyID == 43 )
        p_YW->_helpURL = Locale::Text::Help(Locale::HELP_MAIN);

    r = titel_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SAVE_LOAD_UP )
        {
            ShowSaveLoadMenu();
            diskEnterFromMapSelect = false;
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_OPTIONS_UP )
        {
            ShowOptionsMenu();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_INPUT_SETTINGS_UP )
        {
            ShowInputSettings();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_LANGUAGE_UP )
        {
            ShowLanguageMenu();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_QUIT_UP )
        {
            titel_button->HideScreen();

            envAction.action = EnvAction::ACTION_QUIT;
            SFXEngine::SFXe.startSound(&samples1_info, 4);
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_MULTIPLAYER_UP )
        {
            GameShellUiOpenNetwork();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_CONTINUE_UP )
        {
            ContinueGame();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SINGLE_PLAYER_UP )
        {
            sub_bar_button->ShowScreen();

            titel_button->HideScreen();

            EnvMode = ENVMODE_SINGLEPLAY;
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_HELP_UP )
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_MAIN);
        }
    }

    if ( (EnvMode == ENVMODE_SINGLEPLAY || EnvMode == ENVMODE_TUTORIAL) && Input->HotKeyID == 24 )
    {
        if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
        {
            CancelBriefing(p_YW);
            if ( p_YW->_preferences & World::PREF_CDMUSICDISABLE )
            {
                SFXEngine::SFXe.StopMusicTrack();
                if ( shelltrack )
                {
                    SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                    SFXEngine::SFXe.PlayMusicTrack();
                }
            }
            if ( returnToTitle )
                yw_returnToTitle();
        }
        else
        {
            sub_bar_button->HideScreen();

            titel_button->ShowScreen();

            EnvMode = ENVMODE_TITLE;
        }
    }

    r = sub_bar_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);
        switch ( r.code )
        {
        case 1013:
            if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
            {
                CancelBriefing(p_YW);
                if ( p_YW->_preferences & World::PREF_CDMUSICDISABLE )
                {
                    SFXEngine::SFXe.StopMusicTrack();
                    if ( shelltrack )
                    {
                        SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                        SFXEngine::SFXe.PlayMusicTrack();
                    }
                }
                if ( returnToTitle )
                    yw_returnToTitle();
            }
            else
            {
                sub_bar_button->HideScreen();

                titel_button->ShowScreen();

                EnvMode = ENVMODE_TITLE;
            }
            break;

        case 1019:
        {
            sgmSaveExist = 0;
            p_YW->_isNetGame = false;

            sub_bar_button->HideScreen();

            BriefRequestPlayLevel(p_YW);
        }
        break;

        case 1021:
            if ( IsSaveGameOutdated() )
            {
                ShowConfirmDialog(1, Locale::Text::Advanced(Locale::ADV_WANTTOLOAD)
                            , Locale::Text::Advanced(Locale::ADV_DISCARDPROGRESS), 0);
            }
            else
            {
                ShowConfirmDialog(1, Locale::Text::Advanced(Locale::ADV_RLYLOAD)
                            , Locale::Text::Advanced(Locale::ADV_2440), 0);
            }
            break;

        case 1016:
            BriefRestart(p_YW);
            break;

        case 1020:
            BriefFastForward(p_YW);
            break;

        case 1026:
            ShowSaveLoadMenu();
            diskEnterFromMapSelect = true;
            break;

        default:
            break;
        }
    }

    if ( EnvMode == ENVMODE_INPUT )
    {
        if ( !keyCatchMode && Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_INPUTCONF);

        if ( Input->KbdLastHit != Input::KC_NONE )
        {
            if ( keyCatchMode )
            {
                input_listview.listFlags &= ~GuiList::GLIST_FLAG_KEYB_INPUT;

                if ( !Input::Engine.KeyTitle.at( Input->KbdLastHit ).empty() )
                {
                    if ( confFirstKey )
                    {
                        InputConfig[inpListActiveElement].PKeyCode = Input->KbdLastHit;

                        if ( InputConfig[inpListActiveElement].Type == World::INPUT_BIND_TYPE_SLIDER )
                            confFirstKey = false;

                        keyCatchMode = false;
                        InputConfig[inpListActiveElement].SetFlags = 0;
                    }
                    else
                    {
                        InputConfig[inpListActiveElement].NKeyCode = Input->KbdLastHit;
                        InputConfig[inpListActiveElement].SetFlags &= ~TInputConf::IF_SECOND;
                        confFirstKey = true;
                    }
                }
                Input->KbdLastHit = Input::KC_NONE;
            }
            else
            {
                input_listview.listFlags |= GuiList::GLIST_FLAG_KEYB_INPUT;

                if ( Input->KbdLastHit == Input::KC_BACKSPACE  || Input->KbdLastHit == Input::KC_DELETE)
                {
                    if (InputConfig[inpListActiveElement].Type != World::INPUT_BIND_TYPE_SLIDER)
                        InputConfig[inpListActiveElement].PKeyCode = 0;
                }
                else if ( Input->KbdLastHit == Input::KC_RETURN )
                {
                                printf("Line = %d\n", __LINE__);

                    InputConfig[inpListActiveElement].SetFlags = (TInputConf::IF_FIRST | TInputConf::IF_SECOND);
                    keyCatchMode = true;
                    if ( InputConfig[inpListActiveElement].Type == World::INPUT_BIND_TYPE_SLIDER )
                        confFirstKey = false;
                }
                else if ( Input->KbdLastHit == Input::KC_ESCAPE )
                {
                    InputPageCancel();
                }
            }
        }
    }

    if ( InputConfig[inpListActiveElement].Type == World::INPUT_BIND_TYPE_SLIDER )
    {
        enableState.state = 0;
        enableState.butID = 1056;
        button_input_button->Disable(&enableState);
    }
    else
    {
        enableState.state = 0;
        enableState.butID = 1056;
        button_input_button->Enable(&enableState);
    }

    r = button_input_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if (r.code == 1050)
        {
            confJoystickEnabled = true;
            inputChangedParts |= ICHG_JOYSTICK;
        }
        else if (r.code == 1051)
        {
            confJoystickEnabled = false;
            inputChangedParts |= ICHG_JOYSTICK;
        }
        else if (r.code == 1052) // on OK press
        {
            if ( inputChangedParts & ICHG_JOYSTICK )
            {
                joystickEnabled = confJoystickEnabled;
                if ( confJoystickEnabled )
                    p_YW->_preferences &= ~World::PREF_JOYDISABLE;
                else
                    p_YW->_preferences |= World::PREF_JOYDISABLE;
            }

            if ( inputChangedParts & ICHG_ALTJOYSTICK )
            {
                altJoystickEnabled = confAltJoystickEnabled;
                if ( confAltJoystickEnabled )
                    p_YW->_preferences |= World::PREF_ALTJOYSTICK;
                else
                    p_YW->_preferences &= ~World::PREF_ALTJOYSTICK;
            }

            if ( inputChangedParts & ICHG_FORCEFEEDBACK )
            {
                if ( confFFEnabled )
                    p_YW->_preferences &= ~World::PREF_FFDISABLE;
                else
                    p_YW->_preferences |= World::PREF_FFDISABLE;
            }

            inputChangedParts = 0;
            ReloadAllInputs();
            InputConfCopyToBackup();

            button_input_button->HideScreen();

            titel_button->ShowScreen();

            p_YW->GuiWinClose( &input_listview );
            EnvMode = ENVMODE_TITLE;
        }
        else if (r.code == 1053)
        {
            InputConfigRestoreDefault();
        }
        else if (r.code == 1054)
        {
            InputPageCancel();
        }
        else if ( r.code == 1055 )
        {
            confFFEnabled = false;
            inputChangedParts |= ICHG_FORCEFEEDBACK;
        }
        else if ( r.code == 1056 )
        {
            confFFEnabled = true;
            inputChangedParts |= ICHG_FORCEFEEDBACK;
        }
        else if ( r.code == 1057 )
        {
            InputConfig[ inpListActiveElement ].PKeyCode = 0;
        }
        else if ( r.code == 1058 )
        {
            confAltJoystickEnabled = true;
            inputChangedParts |= ICHG_ALTJOYSTICK;
        }
        else if ( r.code == 1059 )
        {
            confAltJoystickEnabled = false;
            inputChangedParts |= ICHG_ALTJOYSTICK;
        }
        else if ( r.code == 1250 )
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_INPUTCONF);
            keyCatchMode = false;
        }
    }

    if ( EnvMode == ENVMODE_INPUT )
    {
        input_listview.InputHandle(p_YW, Input);

        inpListActiveElement = input_listview.selectedEntry + 1;

        if ( input_listview.listFlags & GuiList::GLIST_FLAG_IN_SELECT )
        {
            InputConfig[ inpListActiveElement ].SetFlags = 0;
            confFirstKey = true;
            keyCatchMode = false;
        }
        input_listview.Formate(p_YW);
    }

    if ( EnvMode == ENVMODE_SETTINGS )
    {
        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
            if ( d3d_listvw.IsClosed() )
                ApplyOptions();
        }
        else if ( Input->KbdLastHit == Input::KC_ESCAPE )
        {
            CancelOptions();
            EnvMode = ENVMODE_TITLE;
        }
        if ( Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SETTINGS);
    }


    r = video_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1106 )
        {
            _settingsChangeOptions |= 8;
            confGFXFlags |= World::GFX_FLAG_SKYRENDER;
        }
        else if ( r.code == 1107 )
        {
            confGFXFlags &= ~World::GFX_FLAG_SKYRENDER;
            _settingsChangeOptions |= 8;
        }
        else if ( r.code == 1108 )
        {
            _settingsChangeOptions |= 0x40;
        }
        else if ( r.code == 1111 )
        {
            _settingsChangeOptions |= 2;
            confSoundFlags &= ~World::SF_INVERTLR;
        }
        else if ( r.code == 1112 )
        {
            confSoundFlags |= World::SF_INVERTLR;
            _settingsChangeOptions |= 2;
        }
        else if ( r.code == 1115 )
        {
            SFXEngine::SFXe.startSound(&samples1_info, 0);
            _settingsChangeOptions |= 0x80;
        }
        else if ( r.code == 1117 )
        {
            SFXEngine::SFXe.stopSound(&samples1_info, 0);
        }
        else if ( r.code == 1118 )
        {
            _settingsChangeOptions |= 0x100;
        }
        else if ( r.code == 1124 )
        {
            ApplyOptions();
        }
        else if ( r.code == 1125 ) // Options CANCEL
        {
            CancelOptions();
        }
        else if ( r.code == 1126 )
        {
            confEnemyIndicator = true;
            _settingsChangeOptions |= 0x20;
        }
        else if ( r.code == 1127 )
        {
            confEnemyIndicator = false;
            _settingsChangeOptions |= 0x20;
        }
        else if ( r.code == 1128 )
        {
            _settingsChangeOptions |= 0x200;
            confSoundFlags |= World::SF_CDSOUND;
        }
        else if ( r.code == 1129 )
        {
            confSoundFlags &= ~World::SF_CDSOUND;
            _settingsChangeOptions |= 0x200;
        }
        else if ( r.code == 1130 )
        {
            _settingsChangeOptions |= 0x400;
            confGFXFlags |= World::GFX_FLAG_WINDOWED;
        }
        else if ( r.code == 1131 )
        {
            confGFXFlags &= ~World::GFX_FLAG_WINDOWED;
            _settingsChangeOptions |= 0x400;
        }
        else if ( r.code == 1136 )
        {
            confGFXFlags |= World::GFX_FLAG_VSYNC;
            _settingsChangeOptions |= 0x2000;
        }
        else if ( r.code == 1137 )
        {
            confGFXFlags &= ~World::GFX_FLAG_VSYNC;
            _settingsChangeOptions |= 0x2000;
        }
        else if ( r.code == 1138 || r.code == 1139 )
        {
            confFiltering = (r.code == 1138);
            _settingsChangeOptions |= OPT_CHG_FILTERING;
        }
        else if ( r.code == 1140 || r.code == 1141 )
        {
            confMovies = (r.code == 1140);
            _settingsChangeOptions |= OPT_CHG_MOVIES;
        }
        else if ( r.code == 1144 )
        {
            _settingsChangeOptions |= OPT_CHG_MAXFPS;
        }
        else if ( r.code == 1145 )
        {
            _settingsChangeOptions |= OPT_CHG_CHANNELS;
        }
        else if ( r.code == 1146 )
        {
            _settingsChangeOptions |= OPT_CHG_AITIMELINE;
        }
        else if ( r.code == 1134 )
        {
            p_YW->GuiWinOpen( &d3d_listvw );
            SFXEngine::SFXe.startSound(&samples1_info, 7);

            Input->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
        }
        else if ( r.code == 1135 )
        {
            p_YW->GuiWinClose( &d3d_listvw );
        }
        else if ( r.code == 1250 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SETTINGS);
    }

    if ( EnvMode == ENVMODE_SETTINGS && d3d_listvw.IsOpen() )
    {
        d3d_listvw.InputHandle(p_YW, Input);

        if ( d3d_listvw.listFlags & GuiList::GLIST_FLAG_SEL_DONE )
        {
            _settingsChangeOptions |= 0x1000;
            
            if (!remoteMode)
                UpdateSelected3DDevFromList();
        }

        if ( d3d_listvw.IsClosed() )
        {
            btnState.state = 2;
            btnState.butID = 1172;

            video_button->SetState(&btnState);
        }

        d3d_listvw.Formate(p_YW);
    }

    NC_STACK_button::Slider *slider = video_button->GetSliderData(1159);
    
    video_button->SetText(1158, fmt::sprintf("%d", slider->value));
    confFxNumber = slider->value;

    slider = video_button->GetSliderData(1152);

    video_button->SetText(1153, fmt::sprintf("%d", slider->value));
    confSoundVolume = slider->value;

    SFXEngine::SFXe.setMasterVolume(confSoundVolume);

    slider = video_button->GetSliderData(1154);

    video_button->SetText(1155, fmt::sprintf("%d", slider->value));
    confMusicVolume = slider->value;

    SFXEngine::SFXe.SetMusicVolume(confMusicVolume);

    // The frame rate limit and the AI timeline show their setting, which
    // may lie between the sliders' steps, until their slider is moved.
    slider = video_button->GetSliderData(1177);
    if ( _settingsChangeOptions & OPT_CHG_MAXFPS )
        confMaxFps = FpsLimits.at(slider->value);
    video_button->SetText(1178, FpsLimitText(confMaxFps));

    slider = video_button->GetSliderData(1179);
    confSoundChannels = slider->value;
    video_button->SetText(1180, fmt::sprintf("%d", confSoundChannels));

    slider = video_button->GetSliderData(1181);
    if ( _settingsChangeOptions & OPT_CHG_AITIMELINE )
        confAITimeline = slider->value * 60000;
    video_button->SetText(1182, AITimelineText(confAITimeline));

    if ( EnvMode == ENVMODE_SELPLAYER ) //Load/Save
    {
        if ( Input->KbdLastHit != Input::KC_NONE || Input->chr )
        {
            if ( diskScreenMode )
            {
                if ( Input->KbdLastHit == Input::KC_BACKSPACE )
                {
                    if ( userNameDirCursor > 0 )
                    {
                        userNameDir.erase( userNameDirCursor - 1, 1 );
                        userNameDirCursor--;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_RETURN )
                {
                                printf("Line = %d\n", __LINE__);

                    switch ( diskScreenMode )
                    {
                    case 1:
                        if ( diskListActiveElement )
                        {
                            ShowConfirmDialog(3, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                        , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                        }
                        else if (userNameDir.size() > 0)
                        {
                            SaveProfile();
                        }
                        break;

                    case 2:
                        LoadProfile();
                        break;

                    case 3:
                        if ( diskListActiveElement )
                        {
                            ShowConfirmDialog(6, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                        , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                        }
                        else if (userNameDir.size() > 0)
                        {
                            CreateNewProfile();
                        }
                        break;

                    case 4:
                        DeleteProfile();
                        break;

                    default:
                        break;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_ESCAPE )
                {
                    diskScreenMode = 0;
                }
                else if ( Input->KbdLastHit == Input::KC_LEFT )
                {
                    if ( userNameDirCursor > 0 )
                        userNameDirCursor--;
                }
                else if ( Input->KbdLastHit == Input::KC_RIGHT )
                {
                    if ( userNameDirCursor < (int)userNameDir.size() )
                        userNameDirCursor++;
                }
                else if ( Input->KbdLastHit == Input::KC_DELETE )
                {
                    if ( userNameDirCursor < (int)userNameDir.size() )
                        userNameDir.erase(userNameDirCursor, 1);
                }

                if ( userNameDir.size() < 32 )
                {
                    if ( Input->chr >= ' ' )
                    {
                        if ( IsValidFileNameChar(Input->chr) )
                        {
                            userNameDir.insert(userNameDirCursor, 1, Input->chr);
                            userNameDirCursor++;
                        }
                    }
                }
            }
            else
            {
                if ( Input->KbdLastHit == Input::KC_ESCAPE )
                    CloseSaveLoadMenu();

                if ( Input->HotKeyID == 43 )
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SAVEGAME);

            }

            if ( diskListActiveElement )
                disk_listvw.PosOnSelected(diskListActiveElement - 1);
        }
    }


    diskListActiveElement = 0;
    int profileIdx = 1;

    for ( ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++)
    {
        if ( !StriCmp(it->name, userNameDir) )
        {
            diskListActiveElement = profileIdx;
            break;
        }
        profileIdx++;
    }

    r = disk_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 103 )
        {
            CloseSaveLoadMenu();
        }
        else if ( r.code == 1160 )
        {
            diskScreenMode = 2;

            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }
            
            userNameDirCursor = userNameDir.size();
                        
            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1161 )
        {
            diskScreenMode = 4;
            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }
            
            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1162 )
        {
            diskScreenMode = 3;

            std::string tmp = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);

            int maxN = 0;

            for (ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++)
            {
                if ( !StriCmp(tmp, it->name.substr(0, tmp.size())) )
                {
                    int n = std::stoi( it->name.substr(tmp.size()) );

                    if ( n > maxN )
                        maxN = n;
                }
            }

            userNameDir = fmt::sprintf("%s%d", tmp, maxN + 1);
            
            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1163 )
        {
            diskScreenMode = 1;
            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }

            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1164)
        {
            switch ( diskScreenMode )
            {
            case 1:
                if ( diskListActiveElement )
                {
                    ShowConfirmDialog(3, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                }
                else
                {
                    SaveProfile();
                }
                break;
            case 2:
                LoadProfile();
                break;
            case 3:
                if ( diskListActiveElement )
                {
                    ShowConfirmDialog(6, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                }
                else
                {
                    CreateNewProfile();
                }
                break;
            case 4:
                DeleteProfile();
                break;
            default:
                break;
            }
        }
        else if (r.code == 1165)
        {
            if ( diskScreenMode )
            {
                diskScreenMode = 0;
            }
            else
                CloseSaveLoadMenu();
        }
        else if (r.code == 1250)
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SAVEGAME);
            diskScreenMode = 0;
        }
    }

    if ( EnvMode == ENVMODE_SELPLAYER ) // Multiplayer
    {
        disk_listvw.InputHandle(p_YW, Input);

        if ( disk_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT || Input->KbdLastHit == Input::KC_UP || Input->KbdLastHit == Input::KC_DOWN )
        {
            diskListActiveElement = disk_listvw.selectedEntry + 1;

            if ( diskListActiveElement < 1 )
                diskListActiveElement = 1;

            if ( diskListActiveElement > disk_listvw.numEntries  )
                diskListActiveElement = disk_listvw.numEntries;


            ProfileList::iterator it = profiles.begin();

            for (int i = 0; i < diskListActiveElement - 1; i++) // check field_1612 - 1
            {
                if ( it == profiles.end() )
                {
                    diskListActiveElement = 0;
                    break;
                }

                it++;
            }

            if (it != profiles.end())
            {
                userNameDir = it->name;
                userNameDirCursor = userNameDir.size();
            }
        }
        disk_listvw.Formate(p_YW);
    }

    if ( diskScreenMode )
    {
        enableState.butID = 1105;
        disk_button->Enable(&enableState);

        enableState.state = 0;
        enableState.butID = 1104;
        disk_button->Disable(&enableState);

        enableState.butID = 1101;
        disk_button->Disable(&enableState);

        enableState.butID = 1102;
        disk_button->Disable(&enableState);

        enableState.butID = 1103;
        disk_button->Disable(&enableState);

        enableState.butID = 1100;
        disk_button->Disable(&enableState);

        if ( diskScreenMode == 4 )
        {
            enableState.state = 0;
            enableState.butID = 1105;

            if ( !diskListActiveElement || !StriCmp(userNameDir, UserName) )
                disk_button->Disable(&enableState);
            else
                disk_button->Enable(&enableState);
        }

        if ( diskScreenMode == 2 && !diskListActiveElement )
        {
            enableState.state = 0;
            enableState.butID = 1105;
            disk_button->Disable(&enableState);
        }

        if ( diskScreenMode == 1 || diskScreenMode == 3 )
        {
            enableState.butID = 1100;
            disk_button->Enable(&enableState);
        }

        std::string tmp = userNameDir;
        tmp.insert(userNameDirCursor, 1, '_');
        
        disk_button->SetText(1100, tmp);
    }
    else
    {
        enableState.state = 0;
        enableState.butID = 1105;
        disk_button->Disable(&enableState);

        enableState.butID = 1104;
        disk_button->Enable(&enableState);

        enableState.butID = 1101;
        disk_button->Enable(&enableState);

        enableState.butID = 1103;
        disk_button->Enable(&enableState);

        enableState.butID = 1100;
        disk_button->Disable(&enableState);

        enableState.butID = 1102;
        if ( !StriCmp(userNameDir, UserName) )
            disk_button->Disable(&enableState);
        else
            disk_button->Enable(&enableState);

        disk_button->SetText(1100, userNameDir);
    }

    if ( EnvMode == ENVMODE_SELLOCALE )
    {

        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
                        printf("Line = %d\n", __LINE__);

            ApplySelectedLanguage();
        }
        else if ( Input->KbdLastHit == Input::KC_ESCAPE )
        {
            ExitFromLanguageMenu();
        }

        if ( Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_LOCALE);
    }


    r = locale_button->ProcessWidgetsEvents(Input);

    if (r)
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 103 )
        {
            ExitFromLanguageMenu();
        }
        else if ( r.code == 1250 )
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_LOCALE);
        }
        else if ( r.code == 1300 )
        {
            ApplySelectedLanguage();
        }
        else if ( r.code == 1301 )
        {
            ExitFromLanguageMenu();
        }
    }

    if ( EnvMode == ENVMODE_SELLOCALE ) //Locale
    {
        local_listvw.InputHandle(p_YW, Input);

        if ( local_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT )
        {
            Engine::StringList::iterator it = std::next(lang_dlls.begin(), local_listvw.selectedEntry);

            prev_lang = &(*it);
        }

        local_listvw.Formate(p_YW);
    }

    r = about_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.code == 103 )
        {
            EnvMode = ENVMODE_TITLE;

            about_button->HideScreen();
        }
    }

    if ( EnvMode == ENVMODE_ABOUT )
    {
        if ( Input->KbdLastHit == Input::KC_RETURN || Input->KbdLastHit == Input::KC_ESCAPE )
        {
                        printf("Line = %d\n", __LINE__);

            EnvMode = ENVMODE_TITLE;

            about_button->HideScreen();

            titel_button->ShowScreen();
        }
    }

    if ( EnvMode == ENVMODE_TITLE )
    {
        if ( aboutDlgKeyCount && GlobalTime - aboutDlgLastKeyTime >= 700 )
        {
            aboutDlgKeyCount = 0;
        }
        else
        {
            switch ( aboutDlgKeyCount )
            {
            case 0:
                if ( p_YW->IsCheatKeyHit(Input, Input::KC_A) ) // VK_A
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 1:
                if ( p_YW->IsCheatKeyHit(Input, Input::KC_M) )
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 2:
                if ( p_YW->IsCheatKeyHit(Input, Input::KC_O) )
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 3:
                if ( p_YW->IsCheatKeyHit(Input, Input::KC_K) )
                {
                    ShowAbout();
                    SFXEngine::SFXe.startSound(&samples1_info, World::SOUND_ID_SECRET);
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;
            default:
                break;
            }
        }
    }
    else
    {
        aboutDlgKeyCount = 0;
    }

    switch ( netSelMode )
    {
    case NETSCREEN_MODE_SELECT:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_SESSION_SELECT:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_CHOOSE_MAP:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_ENTER_NAME:
        network_listvw.maxShownEntries = 12;
        nInputMode = 1;
        break;
    case NETSCREEN_ENTER_IP:
        network_listvw.maxShownEntries = 12;
        nInputMode = 1;
        break;
    case NETSCREEN_INSESSION:
        nInputMode = 1;
        network_listvw.maxShownEntries = 6;
        netListY = p_YW->_fontH * 9.5 + 2 * vertMenuSpace;
        break;
    default:
        break;
    }

    uamessage_fraction fracMsg;

    r = network_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1204 || r.code == 1205 || r.code == 1206 || r.code == 1207 )
        {
            fracMsg.msgID = UAMSG_FRACTION;
            fracMsg.owner = 0;
        }

        if ( r.code == 103 || r.code == 1202 )
        {
            ExitFromNetworkToMain();
        }
        else if ( r.code == 1204 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_RESIST_BIT;
            SelectedFraction = World::OWNER_RESIST_BIT;
            FreeFraction &= ~World::OWNER_RESIST_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1205 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_GHOR_BIT;
            FreeFraction &= ~World::OWNER_GHOR_BIT;
            SelectedFraction = World::OWNER_GHOR_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1206 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_MYKO_BIT;
            SelectedFraction = World::OWNER_MYKO_BIT;
            FreeFraction &= ~World::OWNER_MYKO_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1207 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_TAER_BIT;
            SelectedFraction = World::OWNER_TAER_BIT;
            FreeFraction &= ~World::OWNER_TAER_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }

        switch ( netSelMode )
        {
        case NETSCREEN_MODE_SELECT:
            if ( r.code == 1200 )
            {
                yw_NetOKProvider();
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELPROV);
            }
            break;

        case NETSCREEN_SESSION_SELECT:
            if ( r.code == 1200 )
            {
                yw_JoinNetGame();
            }
            else if ( r.code == 1201 )
            {
                isHost = true;
                netSel = -1;
                network_listvw.firstShownEntries = 0;
                netSelMode = NETSCREEN_CHOOSE_MAP;
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELSESS);
            }
            break;

        case NETSCREEN_ENTER_IP:
            if ( r.code == 1200 ){
                if ( !netName.empty() )
                {
                    std::string ip = netName;
                    netName = "";
                    ConnectToServer(ip);
                }
                else
                {
                    netName = "127.0.0.1";
                    netNameCurPos = netName.size();
                }
            }
            break;

        case NETSCREEN_ENTER_NAME:
            if ( r.code == 1200 )
            {
                if ( !netName.empty() )
                {
                    netPlayerName = netName;
                    netName = "";
                }
                p_YW->_netDriver->SetWantedName(netPlayerName);
                switch ( p_YW->_netDriver->GetMode() )
                {
                    case 1:
                        isHost = true;
                        netSel = -1;
                        network_listvw.firstShownEntries = 0;
                        netSelMode = NETSCREEN_CHOOSE_MAP;
                        p_YW->GuiWinOpen( &network_listvw );
                        break;

                    case 2:
                    {
                        netSelMode = NETSCREEN_ENTER_IP;
                        netName = "";
                        netNameCurPos = 0;
                    }
                        break;

                    default:
                        break;
                }
            }
            else if ( r.code == 1201 )
            {
//                if ( str17_NOT_FALSE )
//                {
//                    windd_dlgBox v339;
//                    memset(&v339, 0, sizeof(windd_dlgBox));
//
//                    v339.title = get_lang_string(413, "ENTER CALLSIGN");
//                    v339.ok = get_lang_string(2, "OK");
//                    v339.cancel = get_lang_string(3, "CANCEL");
//                    v339.maxLen = 32;
//                    v339.timer_func = NULL;
//                    v339.startText = netName;
//
//                    windd->ShowTextInputDialog(&v339);
//
//                    if ( v339.result )
//                    {
//                        strncpy(netName, v339.result, 64);
//                        netName[63] = 0;
//                    }
//                }
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETNAME);
            }
            break;

        case NETSCREEN_CHOOSE_MAP:
            if ( r.code == 1200 )
            {
                AfterMapChoose();
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELLVL);
            }
            break;

        case NETSCREEN_INSESSION:
            if ( r.code == 1200 )
            {
                if ( isHost )
                {
                    std::string unused;
                    std::string unusedText;

                    if ( p_YW->_netDriver->GetPlayerCount() <= 1 )
                    {
                        ShowConfirmDialog(2, Locale::Text::Advanced(Locale::ADV_WANTSTART)
                                    , Locale::Text::Advanced(Locale::ADV_NETALONE), 0);
                    }
                    else
                    {
                        StartNetGame();
                    }
                }
            }
            else if ( r.code == 1203 )
            {
                if ( isHost )
                {
                    netSel = -1;
                    network_listvw.firstShownEntries = 0;
                    msgBuffers.clear();
                    lastSender.clear();
                    netName = "";
                    netSelMode = NETSCREEN_CHOOSE_MAP;
                }
            }
            else if ( r.code == 1208 )
            {
                uamessage_ready rdyMsg;

                rdyStart = true;
                
                int myIndex = p_YW->_netDriver->GetMyIndex();
                
                if (myIndex >= 0)
                    lobbyPlayers[myIndex].Ready = true;

                rdyMsg.msgID = UAMSG_READY;
                rdyMsg.owner = 0;
                rdyMsg.rdy = 1;

                p_YW->NetBroadcastMessage(&rdyMsg, sizeof(rdyMsg), true);

                p_YW->_netDriver->FlushBroadcastBuffer();
            }
            else if ( r.code == 1209 )
            {
                uamessage_ready rdyMsg;

                rdyStart = false;
                
                int myIndex = p_YW->_netDriver->GetMyIndex();
                
                if (myIndex >= 0)
                    lobbyPlayers[myIndex].Ready = false;

                rdyMsg.msgID = UAMSG_READY;
                rdyMsg.owner = 0;
                rdyMsg.rdy = 0;

                p_YW->NetBroadcastMessage(&rdyMsg, sizeof(rdyMsg), true);

                p_YW->_netDriver->FlushBroadcastBuffer();
            }
            else if ( r.code == 1210 )
            {
//                if ( str17_NOT_FALSE )
//                {
//                    windd_dlgBox v316;
//                    memset(&v316, 0, sizeof(windd_dlgBox));
//
//                    v316.title = get_lang_string(422, "ENTER MESSAGE");
//                    v316.ok = get_lang_string(2, "OK");
//                    v316.cancel = get_lang_string(3, "CANCEL");
//                    v316.startText = netName;
//                    v316.timer_func = NULL;
//                    v316.maxLen = 64;
//
//                    windd->ShowTextInputDialog(&v316);
//
//                    if ( v316.result )
//                    {
//                        strncpy(netName, v316.result, 64);
//                        netName[63] = 0;
//                    }
//                }

                if ( !netName.empty() )
                {
                    uamessage_message msgMsg;
                    msgMsg.msgID = UAMSG_MESSAGE;
                    msgMsg.owner = 0;

                    strncpy(msgMsg.message, netName.c_str(), 64);

                    p_YW->NetBroadcastMessage(&msgMsg, sizeof(msgMsg), true);

                    AddChatMessage(p_YW, netPlayerName, msgMsg.message);

                    netName = "";
                    netNameCurPos = 0;

                    int voiceId = strtol(msgMsg.message, NULL, 0);
                    if ( voiceId > 0 )
                        PlayChatVoiceSample(p_YW, voiceId);
                }
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSTRTSCR);
            }
            break;
        default:
            break;
        }
    }

    if ( EnvMode == ENVMODE_NETPLAY )
    {
        int netBtnY = network_button->getBTN_y();

        network_listvw.y = netListY + netBtnY;

        network_listvw.InputHandle(p_YW, Input);

        if ( (network_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT) || Input->KbdLastHit == Input::KC_UP || Input->KbdLastHit == Input::KC_DOWN )
        {
            netSel = network_listvw.selectedEntry;

            switch ( netSelMode )
            {
            case NETSCREEN_MODE_SELECT:
            case NETSCREEN_SESSION_SELECT:
                nInputMode = 0;
                break;
            case NETSCREEN_CHOOSE_MAP:
            {
                uint32_t playerCount = p_YW->_netDriver->GetPlayerCount();

                int filteredID = 0;

                for (const UserData::TMapDescription &desc : mapDescriptions)
                {
                    if ( playerCount <= 1 || playerCount <= p_YW->_globalMapRegions.MapRegions.at( desc.id ).RoboCount)
                    {
                        if ( filteredID == netSel )
                        {
                            netLevelName = desc.pstring;
                            netLevelID = desc.id;
                            break;
                        }

                        filteredID++;
                    }
                }
            }
            break;

            default:
                break;
            }

            netNameCurPos = netName.size();
        }

        network_listvw.Formate(p_YW);
    }

    if ( EnvMode == ENVMODE_NETPLAY )
    {
        if ( Input->KbdLastHit != Input::KC_NONE || Input->chr || Input->HotKeyID >= 0 )
        {
            if ( nInputMode )
            {
                
                uint32_t maxNameLen;

                if ( netSelMode == NETSCREEN_ENTER_NAME )
                    maxNameLen = 32;
                else
                    maxNameLen = 38;

                if ( netName.size() < maxNameLen )
                {
                    if ( Input->chr > ' ' && Input->chr != '*' )
                    {
                        if (netNameCurPos <= (int)netName.size())
                        {
                            netName.insert(netNameCurPos, 1, Input->chr);
                            netNameCurPos++;
                        }                           
                    }
                }

                if ( Input->KbdLastHit == Input::KC_BACKSPACE && netNameCurPos > 0 )
                {
                    if (netNameCurPos > 0 && (int)netName.size() >= netNameCurPos)
                    {
                        netName.erase(netNameCurPos - 1, 1);                        
                        netNameCurPos--;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_LEFT )
                {
                    if ( netNameCurPos > 0 )
                        netNameCurPos--;
                }
                else if ( Input->KbdLastHit == Input::KC_RIGHT )
                {
                    if ( netNameCurPos < (int)netName.size() )
                        netNameCurPos++;
                }
                else if ( Input->KbdLastHit == Input::KC_DELETE && netNameCurPos < (int)netName.size() )
                {
                    if ( netNameCurPos < (int)netName.size() )
                        netName.erase(netNameCurPos, 1);
                }
            }

            if ( Input->KbdLastHit == Input::KC_RETURN )
            {
                switch ( netSelMode )
                {
                case NETSCREEN_MODE_SELECT:
                    yw_NetOKProvider();
                    break;

                case NETSCREEN_SESSION_SELECT:
                    if ( network_listvw.numEntries )
                    {
                        yw_JoinNetGame();
                    }
                    else
                    {
                        isHost = true;
                        netSel = -1;
                        netSelMode = NETSCREEN_CHOOSE_MAP;
                        network_listvw.firstShownEntries = 0;
                    }
                    break;

                case NETSCREEN_ENTER_NAME:
                    if ( !netName.empty() )
                    {
                        netPlayerName = netName;
                        netName = "";

                        p_YW->_netDriver->SetWantedName(netPlayerName);
                        switch ( p_YW->_netDriver->GetMode() )
                        {
                            case 1:
                                isHost = true;
                                netSel = -1;
                                network_listvw.firstShownEntries = 0;
                                netSelMode = NETSCREEN_CHOOSE_MAP;
                                p_YW->GuiWinOpen( &network_listvw );
                                break;

                            case 2:
                            {
                                netSelMode = NETSCREEN_ENTER_IP;
                                netName = "";
                                netNameCurPos = 0;
                            }
                                break;

                            default:
                                break;
                        }
                    }
                    break;
                
                case NETSCREEN_ENTER_IP:
                    {
                        if ( !netName.empty() )
                        {
                            std::string ip = netName;
                            netName = "";
                            ConnectToServer(ip);
                        }
                        else
                        {
                            netName = "127.0.0.1";
                            netNameCurPos = netName.size();
                        }
                    }
                    break;

                case NETSCREEN_CHOOSE_MAP:
                    AfterMapChoose();
                    break;
                case NETSCREEN_INSESSION:
                    if ( !netName.empty() )
                    {
                        uamessage_message msgMsg;
                        msgMsg.msgID = UAMSG_MESSAGE;
                        msgMsg.owner = 0;

                        strncpy(msgMsg.message, netName.c_str(), 64);

                        p_YW->NetBroadcastMessage(&msgMsg, sizeof(msgMsg), true);

                        AddChatMessage(p_YW, netPlayerName, msgMsg.message);
                        netName.clear();
                        netNameCurPos = 0;

                        int voiceId = strtol(msgMsg.message, NULL, 0);
                        if ( voiceId > 0 )
                            PlayChatVoiceSample(p_YW, voiceId);
                    }
                    break;
                default:
                    break;
                }
            }
            else if ( Input->KbdLastHit == Input::KC_ESCAPE )
            {
                ExitFromNetworkToMain();
            }

            if ( Input->HotKeyID == 43 && !nInputMode )
            {
                switch ( netSelMode )
                {
                case NETSCREEN_MODE_SELECT:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELPROV);
                    break;
                case NETSCREEN_SESSION_SELECT:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELSESS);
                    break;
                case NETSCREEN_ENTER_NAME:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETNAME);
                    break;
                case NETSCREEN_CHOOSE_MAP:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELLVL);
                    break;
                case NETSCREEN_INSESSION:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSTRTSCR);
                    break;
                default:
                    break;
                }
            }
            
            if ( netSelMode == NETSCREEN_ENTER_IP && Input->KbdLastHit == Input::KC_V && Input::Engine.GetKeyState(Input::KC_CTRL) )
            {
                char * clpbrd = SDL_GetClipboardText();
                if (clpbrd)
                {
                    ZNDNet::IPaddress tmp;
                    if (ZNDNet::ResolveHost(&tmp, clpbrd, 0))
                    {
                        netName = clpbrd;
                        netNameCurPos = netName.size();
                    }
                    SDL_free(clpbrd);
                }
            }

            if ( netSel != -1 ){
                network_listvw.PosOnSelected(netSel);
            }

            Input->KbdLastHit = Input::KC_NONE;
        }
    }

    if ( isHost )
    {
        if ( netSelMode == NETSCREEN_INSESSION && envAction.action != EnvAction::ACTION_NETPLAY )
        {
            if ( p_YW->_netDriver->GetPlayerCount()   <   p_YW->_globalMapRegions.MapRegions[ netLevelID ].RoboCount )
            {
                if ( blocked )
                {
                    int lock = 0;
                    p_YW->_netDriver->LockSession(&lock);

                    blocked = false;
                }
            }
            else if ( !blocked )
            {
                int lock = 1;
                p_YW->_netDriver->LockSession(&lock);

                blocked = true;
            }
        }
    }

    enableState.butID = 1201;
    network_button->Enable(&enableState);

    enableState.butID = 1205;
    network_button->Disable(&enableState);

    enableState.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
    network_button->Disable(&enableState);

    enableState.butID = 1203;
    network_button->Enable(&enableState);

    enableState.butID = 1225;
    network_button->Disable(&enableState);

    enableState.butID = 1226;
    network_button->Disable(&enableState);

    enableState.butID = 1227;
    network_button->Disable(&enableState);

    network_button->SetText(1201, Locale::Text::Common(Locale::CMN_OK));

    enableState.butID = 1220;
    network_button->Disable(&enableState);

    enableState.butID = 1206;
    network_button->Disable(&enableState);

    enableState.butID = 1207;
    network_button->Disable(&enableState);

    enableState.butID = 1208;
    network_button->Disable(&enableState);

    enableState.butID = 1209;
    network_button->Disable(&enableState);

    enableState.butID = 1219;
    network_button->Disable(&enableState);

    enableState.butID = 1221;
    network_button->Disable(&enableState);

    enableState.state = 0;
    enableState.butID = 1210;
    network_button->Disable(&enableState);

    enableState.butID = 1211;
    network_button->Disable(&enableState);

    enableState.butID = 1212;
    network_button->Disable(&enableState);

    enableState.butID = 1213;
    network_button->Disable(&enableState);

    enableState.butID = 1214;
    network_button->Disable(&enableState);

    enableState.butID = 1215;
    network_button->Disable(&enableState);

    enableState.butID = 1216;
    network_button->Disable(&enableState);

    enableState.butID = 1217;
    network_button->Disable(&enableState);

    if ( (netSelMode != NETSCREEN_SESSION_SELECT || p_YW->_netDriver->GetProvType() != 3)
            && (netSelMode != NETSCREEN_SESSION_SELECT || modemAskSession != 1 || p_YW->_netDriver->GetProvType() != 4)
            && netSelMode != NETSCREEN_MODE_SELECT )
    {
        enableState.butID = 1228;
        network_button->Disable(&enableState);
    }
    else
    {
        std::string infoText;

        if ( netSelMode != NETSCREEN_MODE_SELECT )
        {
            infoText = Locale::Text::Advanced(Locale::ADV_REFRESHSESS);
        }
        else
        {
            if ( !p_YW->_netTcpAddress.empty() )
            {
                infoText = fmt::sprintf("%s  %s",  Locale::Text::Advanced(Locale::ADV_YOURIP) , p_YW->_netTcpAddress);
            }
            else
                infoText = " ";
        }

        network_button->SetText(1228, infoText);

        enableState.butID = 1228;
        network_button->Enable(&enableState);
    }

    // TODO(bug): "!= A || != B" is always true, so this is just !nInputMode.
    // Probably meant &&.
    if ( !nInputMode && (netSelMode != NETSCREEN_ENTER_NAME || netSelMode != NETSCREEN_ENTER_IP) )
    {
        enableState.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
        network_button->Disable(&enableState);
    }
    else
    {
        enableState.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
        network_button->Enable(&enableState);

        placement.xpos = -1;
        placement.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;

        if ( netSelMode == NETSCREEN_ENTER_NAME || netSelMode == NETSCREEN_ENTER_IP)
        {
            placement.width = netMenuWidth;
            placement.ypos = 3 * (vertMenuSpace + p_YW->_fontH);
        }
        else
        {
            placement.width = netMenuWidth * 0.8;
            placement.ypos = 14 * (vertMenuSpace + p_YW->_fontH);
        }

        network_button->setXYWidth(&placement);

        std::string tmp = netName;
        if (tmp.size() >= (size_t)netNameCurPos)
            tmp.insert(netNameCurPos, 1, '_');
        
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX, tmp);
    }

    placement.xpos = -1;
    placement.width = -1;
    placement.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;

    if ( netSelMode == 2 )
    {
        placement.ypos = 4 * (vertMenuSpace + p_YW->_fontH);
    }
    else
    {
        placement.ypos = (vertMenuSpace + p_YW->_fontH) * 15.2;
    }

    network_button->setXYWidth(&placement);

    switch ( netSelMode )
    {
    case NETSCREEN_MODE_SELECT:
        enableState.state = 0;
        enableState.butID = 1205;

        network_button->Disable(&enableState);

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELPROV));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT2));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT3));
        break;

    case NETSCREEN_SESSION_SELECT:
    {
        if ( p_YW->_netDriver->GetProvType() != 4 || !modemAskSession )
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION, Locale::Text::Netdlg(Locale::NETDLG_NEW));

            enableState.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
            network_button->Enable(&enableState);
        }

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELSESS));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT5));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT6));

        windp_getNameMsg msg;
        msg.id = 0;

        if ( p_YW->_netDriver->GetSessionName(&msg) )
        {
            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_JOIN));
        }
        else if ( p_YW->_netDriver->GetProvType() != 4 || modemAskSession )
        {
            enableState.butID = 1201;
            network_button->Disable(&enableState);
        }
        else
        {
            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_CONNECT));
        }
    }
    break;

    case NETSCREEN_ENTER_NAME:
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_ENTERPL));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT11));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT12));
        break;

    case NETSCREEN_ENTER_IP:
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, "Server address");
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, "Please enter server IP");
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, " ");
        break;

    case NETSCREEN_CHOOSE_MAP:
        if ( remoteMode )
        {
            enableState.butID = 1205;
            network_button->Disable(&enableState);
        }

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELLVL));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT8));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT9));
        break;

    case NETSCREEN_INSESSION:
        enableState.butID = 1225;
        network_button->Enable(&enableState);

        network_button->SetText(1225, Locale::Text::Netdlg(Locale::NETDLG_SEND));

        enableState.butID = 1226;
        network_button->Enable(&enableState);

        enableState.butID = 1227;
        network_button->Enable(&enableState);

        if ( netLevelID )
            network_button->SetText(1226, netLevelName);
        else
            network_button->SetText(1226, " ");

        if ( isHost )
        {  // Change map 
            enableState.butID = 1205;
            //network_button->show(&v410);
        }

        if ( rdyStart )
        {
            if ( !isHost )
            {
                enableState.butID = 1205;
                network_button->Disable(&enableState);
            }
        }

        enableState.butID = 1220;
        network_button->Enable(&enableState);

        if ( netLevelID > 0 && netLevelID < 256 )
        {

            if ( !isHost && rdyStart )
            {
                enableState.butID = 1220;
                network_button->Disable(&enableState);
            }
            else
            {
                enableState.butID = 1206;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_RESIST) )
                    network_button->Enable(&enableState);
                else
                    network_button->Disable(&enableState);

                enableState.butID = 1207;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_GHOR) )
                    network_button->Enable(&enableState);
                else
                    network_button->Disable(&enableState);

                enableState.butID = 1208;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_MYKO) )
                    network_button->Enable(&enableState);
                else
                    network_button->Disable(&enableState);

                enableState.butID = 1209;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_TAER) )
                    network_button->Enable(&enableState);
                else
                    network_button->Disable(&enableState);
            }

            btnState.butID = 0;

            switch ( SelectedFraction )
            {
            case World::OWNER_RESIST_BIT:
                btnState.butID = 1206;
                break;
            case World::OWNER_GHOR_BIT:
                btnState.butID = 1207;
                break;
                case World::OWNER_MYKO_BIT:
                btnState.butID = 1208;
                break;
            case World::OWNER_TAER_BIT:
                btnState.butID = 1209;
                break;
                
            default:
                break;
            }

            if ( btnState.butID )
            {
                btnState.state = 1;
                network_button->SetState(&btnState);
            }

            int FractionErrorMask = 0;

            netGameCanStart = true;
            isWelcmd = true;

            // First of all if fraction not allowed - fill 
            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_RESIST) )
                FractionErrorMask |= World::OWNER_RESIST_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_GHOR) )
                FractionErrorMask |= World::OWNER_GHOR_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_MYKO) )
                FractionErrorMask |= World::OWNER_MYKO_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_TAER) )
                FractionErrorMask |= World::OWNER_TAER_BIT;

            int FractionPlID[World::CVFractionsCount];

            for ( TDPPlayerData &p : p_YW->_netDriver->GetPlayersData() )
            {
                int fraction;

                if ( p.IsItMe() )
                    fraction = SelectedFraction;
                else
                    fraction = lobbyPlayers[p.Index].NetFraction;

                if ( fraction & FractionErrorMask )
                {
                    lobbyPlayers[p.Index].IsTrouble = true;
                    netGameCanStart = false;

                    switch ( fraction )
                    {
                    case World::OWNER_RESIST_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_RESIST] ].IsTrouble = true;
                        break;
                    case World::OWNER_GHOR_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_GHOR] ].IsTrouble = true;
                        break;
                    case World::OWNER_MYKO_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_MYKO] ].IsTrouble = true;
                        break;
                    case World::OWNER_TAER_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_TAER] ].IsTrouble = true;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    lobbyPlayers[p.Index].IsTrouble = false;

                    switch ( fraction )
                    {
                    case World::OWNER_RESIST_BIT:
                        FractionPlID[World::OWNER_RESIST] = p.Index;
                        break;
                    case World::OWNER_GHOR_BIT:
                        FractionPlID[World::OWNER_GHOR] = p.Index;
                        break;
                    case World::OWNER_MYKO_BIT:
                        FractionPlID[World::OWNER_MYKO] = p.Index;
                        break;
                    case World::OWNER_TAER_BIT:
                        FractionPlID[World::OWNER_TAER] = p.Index;
                        break;
                    default:
                        break;
                    }

                }
                
                FractionErrorMask |= fraction;
            }
        }

        for ( TDPPlayerData &p : p_YW->_netDriver->GetPlayersData() )
        {
            if ( !p.IsItMe() )
            {
                if ( !lobbyPlayers[p.Index].Ready )
                    netGameCanStart = false;

                if ( !lobbyPlayers[p.Index].Welcomed )
                    isWelcmd = false;
            }
        }

        if ( isHost )
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_STARTOR));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT14));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT15));

            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_START));

            if ( !netGameCanStart )
            {
                enableState.state = 0;
                enableState.butID = 1201;
                network_button->Disable(&enableState);
            }
        }
        else
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_WAITOR));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT17));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT18));

            if ( isWelcmd )
            {
                enableState.butID = 1219;
                network_button->Enable(&enableState);

                enableState.butID = 1221;
                network_button->Enable(&enableState);
            }

            enableState.butID = 1201;
            network_button->Disable(&enableState);
        }
        enableState.butID = 1210;
        network_button->Enable(&enableState);

        enableState.butID = 1211;
        network_button->Enable(&enableState);

        enableState.butID = 1212;
        network_button->Enable(&enableState);

        enableState.butID = 1213;
        network_button->Enable(&enableState);

        enableState.butID = 1214;
        network_button->Enable(&enableState);

        enableState.butID = 1215;
        network_button->Enable(&enableState);

        enableState.butID = 1216;
        network_button->Enable(&enableState);

        enableState.butID = 1217;
        network_button->Enable(&enableState);

        for (size_t i = 0; i < World::CVMaxNetPlayers; i++)
        {
            int statusBtnId;
            TDPPlayerData pData;
            bool hasPlayer = p_YW->_netDriver->GetPlayerData(i, &pData);

            std::string name = " ";
            int btID;

            switch ( i )
            {
            case 0:
                statusBtnId = 1214;
                btID = 1210;
                if ( hasPlayer )
                    name = pData.name;
                break;

            case 1:
                statusBtnId = 1215;
                btID = 1211;
                if ( hasPlayer )
                    name = pData.name;
                break;

            case 2:
                statusBtnId = 1216;
                btID = 1212;
                if ( hasPlayer )
                    name = pData.name;
                break;

            case 3:
                statusBtnId = 1217;
                btID = 1213;
                if ( hasPlayer )
                    name = pData.name;
                break;

            default:
                break;
            }

            network_button->SetText(btID, name);

            std::string statusIcons("     "); // 5 spaces
            
            if ( hasPlayer )
            {
                int faction;

                if ( pData.IsItMe() )
                {
                    faction = SelectedFraction;
                }
                else
                {
                    faction = lobbyPlayers[pData.Index].NetFraction;
                }

                switch ( faction )
                {
                case World::OWNER_RESIST_BIT:
                    statusIcons[0] = 'P';
                    break;
                case World::OWNER_GHOR_BIT:
                    statusIcons[0] = 'R';
                    break;
                case World::OWNER_MYKO_BIT:
                    statusIcons[0] = 'T';
                    break;
                case World::OWNER_TAER_BIT:
                    statusIcons[0] = 'V';
                    break;
                default:
                    break;
                }
                if ( lobbyPlayers[pData.Index].IsTrouble && ((GlobalTime / 300) & 1) )
                    statusIcons[1] = 'f';

                if ( lobbyPlayers[pData.Index].Ready )
                    statusIcons[2] = 'h';
            }

            network_button->SetText(statusBtnId, statusIcons);
        }
        break;

    default:
        break;
    }

}

bool UserData::LoadSample(int sampleID, const std::string &file)
{
    std::string rsrc = Common::Env.SetPrefix("rsrc", "data:");

    NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(file)}});
    if ( !wav )
        return false;

    samples1[sampleID] = wav;
    samples1_info.Sounds[sampleID].PSample = wav->GetSampleData();

    if (sampleID == World::SOUND_ID_VOLUME ||
        sampleID == World::SOUND_ID_SLIDER ||
        sampleID == World::SOUND_ID_TEXTAPPEAR ||
        sampleID == World::SOUND_ID_TIMERCOUNT)
        samples1_info.Sounds[sampleID].SetLoop(true);

    Common::Env.SetPrefix("rsrc", rsrc);
    return true;
}

bool UserData::SaveBuildProtoState()
{
    return p_YW->SaveSettings(this, "settings.tmp", (World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER));
}

void UserData::SaveSettings()
{
    p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", UserName), World::SDF_ALL);

    FSMgr::FileHandle fil = uaOpenFile("env:user.def", "w");
    if ( fil.OK() )
        fil.write(UserName.c_str(), UserName.size());
}

bool UserData::ShellSoundsLoad()
{
    ScriptParser::HandlersList hndls {
        new World::Parsers::ShellSoundParser(this),
        new World::Parsers::ShellTracksParser(this),
    };

    return ScriptParser::ParseFile("env:world.ini", hndls, 0) || ScriptParser::ParseFile("data:world.ini", hndls, 0);
}

int UserData::InputIndexFromConfig(uint32_t type, uint32_t index)
{
    static const std::array<int, 5> BUTTON
    {
        World::INPUT_BIND_FIRE,       World::INPUT_BIND_CAMFIRE,
        World::INPUT_BIND_GUN,        World::INPUT_BIND_BRAKE,
        World::INPUT_BIND_WAPOINT
    };

    static const std::array<int, 6> SLIDER
    {
        World::INPUT_BIND_FLY_DIR,    World::INPUT_BIND_FLY_HEIGHT,
        World::INPUT_BIND_FLY_SPEED,  World::INPUT_BIND_DRIVE_DIR,
        World::INPUT_BIND_DRIVE_SPEED,World::INPUT_BIND_GUN_HEIGHT,
    };

    static const std::array<int, 48> HOTKEY
    {
        World::INPUT_BIND_ORDER,      World::INPUT_BIND_ATTACK,
        World::INPUT_BIND_NEW,        World::INPUT_BIND_ADD,
        World::INPUT_BIND_CONTROL,    -1,
        -1,                           World::INPUT_BIND_AUTOPILOT,
        World::INPUT_BIND_MAP,        World::INPUT_BIND_SQ_MANAGE,

        // 10
        World::INPUT_BIND_LANDLAYER,  World::INPUT_BIND_OWNER,
        World::INPUT_BIND_HEIGHT,     -1,
        World::INPUT_BIND_LOCKVIEW,   -1,
        World::INPUT_BIND_ZOOMIN,     World::INPUT_BIND_ZOOMOUT,
        World::INPUT_BIND_MINIMAP,    -1,

        // 20
        World::INPUT_BIND_NEXT_COMM,  World::INPUT_BIND_TO_HOST,
        World::INPUT_BIND_NEXT_UNIT,  World::INPUT_BIND_TO_COMM,
        World::INPUT_BIND_QUIT,       World::INPUT_BIND_HUD,
        -1,                           World::INPUT_BIND_LOG_WND,
        -1,                           -1,

        // 30
        -1,                           World::INPUT_BIND_LAST_MSG,
        World::INPUT_BIND_PAUSE,      -1,
        -1,                           -1,
        -1,                           World::INPUT_BIND_TO_ALL,
        World::INPUT_BIND_AGGR_1,     World::INPUT_BIND_AGGR_2,

        // 40
        World::INPUT_BIND_AGGR_3,     World::INPUT_BIND_AGGR_4,
        World::INPUT_BIND_AGGR_5,     World::INPUT_BIND_HELP,
        World::INPUT_BIND_LAST_SEAT,  World::INPUT_BIND_SET_COMM,
        World::INPUT_BIND_ANALYZER,   World::INPUT_BIND_REWIND
    };

    if ( type == World::INPUT_BIND_TYPE_BUTTON && index < BUTTON.size())
        return BUTTON[index];
    else if ( type == World::INPUT_BIND_TYPE_SLIDER && index < SLIDER.size() )
        return SLIDER[index];
    else if ( type == World::INPUT_BIND_TYPE_HOTKEY && index < HOTKEY.size() )
        return HOTKEY[index];
    return -1;
}

std::string UserData::SaveGamePath(const std::string &username)
{
    return fmt::sprintf("save:%s/0.sav", username);
}

bool UserData::HasSaveGame(const std::string &username)
{
    return uaFileExist( SaveGamePath(username) );
}

std::string UserData::SessionPath(const std::string &username)
{
    return fmt::sprintf("save:%s/play.rec", username);
}

// The campaign's level whose recording ends unfinished, as when the game was
// quit while it was played, and which can still be played: 0 when there's
// none. Looked for once, until the shell opens again or the profile changes.
int UserData::UnfinishedLevel()
{
    if ( continueLevel >= 0 )
        return continueLevel;

    continueLevel = 0;

    std::string path = SessionPath(UserName);
    World::Recording recording;

    if ( !uaFileExist(path) || !recording.Open(path) || recording.End() != World::END_UNFINISHED )
        return 0;

    const auto &regions = p_YW->_globalMapRegions.MapRegions;
    int level = recording.Level();

    if ( level > 0 && level < (int)regions.size() &&
         (regions[level].Status == TMapRegionInfo::STATUS_ENABLED || regions[level].Status == TMapRegionInfo::STATUS_COMPLETED) )
        continueLevel = level;

    return continueLevel;
}

// Goes on with the level left unfinished, from its recording's last
// keyframe
void UserData::ContinueGame()
{
    titel_button->HideScreen();
    p_YW->_isNetGame = false;

    // Back from the level, the level map
    EnvMode = ENVMODE_SINGLEPLAY;
    envAction.action = EnvAction::ACTION_CONTINUE;
}

