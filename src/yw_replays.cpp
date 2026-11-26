#include <algorithm>
#include <ctime>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "font.h"
#include "button.h"
#include "log.h"
#include "utils.h"

// The shell's layout (yw.cpp)
extern int vertMenuSpace;
extern int buttonsSpace;
extern int scaledFontHeight;
extern int bottomButtonsY;
extern int button1LineWidth;

// Replays: a level's recording kept from its debriefing in the profile's
// replays folder, listed on the replays page with a map of how the level
// went, and watched in the replay viewer (yw_replay.cpp)

// The replays page's map of the replay chosen: its level's map as its
// debriefing shows it, with the level's history played over it as there
struct TReplayPreview
{
    TBriefengScreen Brf;
    Common::PlaneBytes StartOwners; // As the level starts
    World::History::Replay History;
    int32_t Time = 0;   // Of the history played, in game time
    int32_t Ended = 0;  // How long it has waited at the end, in real time
};

static const int ReplaysListRows = 12;

// A preview plays 60 times as fast as the level went, as the debriefing
// does, and starts again 3 s after it ends
static const int32_t PreviewSpeed = 60;
static const int32_t PreviewEndWait = 3 * 1024;

// The page's buttons
enum
{
    REPLAYS_TITLE = 1400,
    REPLAYS_VIEW = 1401,
    REPLAYS_DELETE = 1402,
    REPLAYS_BACK = 1403,

    REPLAYS_VIEW_UP = 1410,
    REPLAYS_DELETE_UP = 1411,
    REPLAYS_BACK_UP = 1412,
};

std::string UserData::ReplaysPath(const std::string &username)
{
    return fmt::sprintf("save:%s/replays", username);
}

std::string FileSizeText(uint64_t bytes)
{
    const double mb = 1024.0 * 1024.0;

    if ( bytes >= 1024 * mb )
        return fmt::sprintf("%.1f GB", bytes / (1024 * mb));

    if ( bytes >= mb )
        return fmt::sprintf("%.1f MB", bytes / mb);

    return fmt::sprintf("%d KB", (int)std::max<uint64_t>(1, bytes / 1024));
}

// The time a replay was saved, from its name: "YYYY-MM-DD HH:MM"
static std::string ReplayDate(const std::string &name)
{
    int y, mo, d, h, mi, sec;

    if ( sscanf(name.c_str(), "%4d-%2d-%2d_%2d-%2d-%2d", &y, &mo, &d, &h, &mi, &sec) != 6 )
        return std::string();

    return fmt::sprintf("%04d-%02d-%02d %02d:%02d", y, mo, d, h, mi);
}


// Debriefing

// The level's recording kept as a replay, named by when it's saved and the
// level
void UserData::SaveDebriefReplay()
{
    std::time_t now = std::time(NULL);
    std::tm local = *std::localtime(&now);

    std::string dir = ReplaysPath(UserName);
    std::string path = fmt::sprintf("%s/%04d-%02d-%02d_%02d-%02d-%02d_level%d.rec", dir,
                                    local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                    local.tm_hour, local.tm_min, local.tm_sec, p_YW->_levelInfo.LevelID);

    if ( !uaFileExist(dir) )
        uaCreateDir(dir);

    if ( World::CopyRecording(p_YW->_debriefRecording, path) )
        p_YW->_debriefReplaySaved = true;
    else
        ShowConfirmDialog(4, Locale::Text::Advanced(Locale::ADV_CANTSAVEREPLAY), "", 1);
}

// Watching the replay from the debriefing: what the debriefing shows is
// kept meanwhile, as setting the replay's level up reads over the world's
void NC_STACK_ypaworld::KeepDebriefing()
{
    _keptDebriefing.reset(new TKeptDebriefing);
    TKeptDebriefing &kept = *_keptDebriefing;

    kept.History = std::move(_history);
    _history.Close();
    kept.PrimevalTypeMap = _lvlPrimevalTypeMap;
    kept.PrimevalOwnMap = _lvlPrimevalOwnMap;
    kept.LevelId = _levelInfo.LevelID;
    kept.LevelDescription = _levelDescription;
    kept.PlayerOwner = _playerOwner;
    kept.OwnerMask = _levelInfo.OwnerMask;
    kept.UserMask = _levelInfo.UserMask;
}

// And the debriefing shown again, as it ends. False when it can't be.
bool NC_STACK_ypaworld::ResumeDebriefing()
{
    if ( !_keptDebriefing )
        return false;

    TKeptDebriefing &kept = *_keptDebriefing;

    _history = std::move(kept.History);
    _lvlPrimevalTypeMap = kept.PrimevalTypeMap;
    _lvlPrimevalOwnMap = kept.PrimevalOwnMap;
    _levelInfo.LevelID = kept.LevelId;
    _levelDescription = kept.LevelDescription;
    _playerOwner = kept.PlayerOwner;
    _levelInfo.OwnerMask = kept.OwnerMask;
    _levelInfo.UserMask = kept.UserMask;

    _keptDebriefing.reset();

    // Not COMPLETED: it doesn't zoom out from the gate again
    _levelInfo.State = TLevelInfo::STATE_MENU;

    if ( !InitDebrief() )
        return false;

    _briefScreen.Instant = true;
    return true;
}


// The replays page

bool NC_STACK_ypaworld::CreateReplaysControls()
{
    UserData *usr = _GameShell;

    int padX = _screenSize.x * 0.05;
    int listW = _screenSize.x * 0.52;
    int top = scaledFontHeight;

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = usr->replays.size();
    args.shownEntries = ReplaysListRows;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = ReplaysListRows;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = listW;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    if ( !usr->replays_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create the replays list\n");
        return false;
    }

    usr->replays_listvw.x = padX;
    usr->replays_listvw.y = top + 2 * (_fontH + vertMenuSpace);

    usr->replays_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y}} );

    if ( !usr->replays_button )
    {
        ypa_log_out("Unable to create the replays page's buttons\n");
        return false;
    }

    NC_STACK_button::WidgetDesc btn;

    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.tileset_disabled = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = padX;
    btn.ypos = top;
    btn.width = listW;
    btn.caption = Locale::Text::Title(Locale::TITLE_REPLAYS);
    btn.button_id = REPLAYS_TITLE;
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;

    bool added = usr->replays_button->Add(&btn);

    // View and Delete under the list, Back where the other pages have theirs
    int buttonsY = usr->replays_listvw.y + (ReplaysListRows + 1) * _fontH + 2 * _fontBorderH + 2 * vertMenuSpace;
    int buttonW = (listW - buttonsSpace) / 2;

    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.tileset_disabled = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.xpos = padX;
    btn.ypos = buttonsY;
    btn.width = buttonW;
    btn.caption = Locale::Text::Title(Locale::TITLE_VIEW);
    btn.downCode = 1251;
    btn.pressedCode = 0;
    btn.upCode = REPLAYS_VIEW_UP;
    btn.button_id = REPLAYS_VIEW;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

    added = added && usr->replays_button->Add(&btn);

    btn.xpos = padX + buttonW + buttonsSpace;
    btn.caption = Locale::Text::Dialogs(Locale::DLG_P_DELETE);
    btn.upCode = REPLAYS_DELETE_UP;
    btn.button_id = REPLAYS_DELETE;

    added = added && usr->replays_button->Add(&btn);

    btn.xpos = padX;
    btn.ypos = top + bottomButtonsY;
    btn.width = button1LineWidth;
    btn.caption = Locale::Text::Advanced(Locale::ADV_BACK);
    btn.upCode = REPLAYS_BACK_UP;
    btn.button_id = REPLAYS_BACK;

    added = added && usr->replays_button->Add(&btn);

    if ( !added )
    {
        ypa_log_out("Unable to add the replays page's buttons\n");
        return false;
    }

    // The map, right of the list, down to Back
    usr->replaysMapArea = Common::Rect(padX + listW + padX / 2, usr->replays_listvw.y,
                                       _screenSize.x - padX, top + bottomButtonsY - vertMenuSpace);

    usr->replays_button->HideScreen();
    return true;
}

void NC_STACK_ypaworld::DeleteReplaysControls()
{
    UserData *usr = _GameShell;

    usr->FreeReplayPreview();

    if ( usr->replays_button )
    {
        if ( usr->replays_listvw.IsOpen() )
            GuiWinClose( &usr->replays_listvw );
        usr->replays_listvw.Free();

        usr->replays_button->HideScreen();
        usr->replays_button->Delete();
        usr->replays_button = NULL;
    }
}

// The profile's replays, newest first
void UserData::FindReplays()
{
    replays.clear();

    std::string dir = ReplaysPath(UserName);
    FSMgr::DirIter it = uaOpenDir(dir);
    FSMgr::iNode *node;

    while ( it && it.getNext(&node) )
    {
        std::string name = node->getName();

        if ( node->getType() != FSMgr::iNode::NTYPE_FILE || name.size() < 5 ||
             StriCmp(name.substr(name.size() - 4), ".rec") )
            continue;

        TReplayFile replay;
        replay.Path = dir + "/" + name;
        replay.Name = name;

        World::Recording recording;

        if ( !recording.Open(replay.Path) )
            continue;

        replay.Level = recording.Level();
        replay.LevelDescription = p_YW->RecordedLevel(recording);
        replay.Duration = (recording.LastStep() - recording.FirstStep()) * World::Recording::StepTicks;
        replay.Size = World::RecordingFileSize(replay.Path);
        replay.Date = ReplayDate(name);
        replays.push_back(replay);
    }

    std::sort(replays.begin(), replays.end(), [](const TReplayFile &a, const TReplayFile &b) { return a.Name > b.Name; });

    replaysSelected = std::min(replaysSelected, (int)replays.size() - 1);
    replaysSelected = std::max(replaysSelected, 0);

    replays_listvw.numEntries = replays.size();
    replays_listvw.selectedEntry = replaysSelected;
    replays_listvw.PosOnSelected(replaysSelected);
}

void UserData::ShowReplays()
{
    titel_button->HideScreen();
    replays_button->ShowScreen();

    EnvMode = ENVMODE_REPLAYS;

    FindReplays();

    p_YW->GuiWinClose( &replays_listvw );
    p_YW->GuiWinOpen( &replays_listvw );
}

void UserData::CloseReplays()
{
    FreeReplayPreview();

    replays_button->HideScreen();
    p_YW->GuiWinClose( &replays_listvw );

    titel_button->ShowScreen();
    EnvMode = ENVMODE_TITLE;
}

void UserData::ViewSelectedReplay()
{
    if ( replaysSelected < 0 || replaysSelected >= (int)replays.size() )
        return;

    replayToView = replays[replaysSelected].Path;
    replayFromDebriefing = false;
    envAction.action = EnvAction::ACTION_VIEW_REPLAY;
}

void UserData::DeleteSelectedReplay()
{
    if ( replaysSelected < 0 || replaysSelected >= (int)replays.size() )
        return;

    FreeReplayPreview();

    if ( !uaDeleteFile(replays[replaysSelected].Path) )
        ypa_log_out("Could not delete the replay %s\n", replays[replaysSelected].Path.c_str());

    FindReplays();
}

void UserData::FreeReplayPreview()
{
    delete replayPreview;
    replayPreview = NULL;
    replayPreviewOf.clear();
}

// The map of the replay chosen, made when it's chosen: once, made or not
void UserData::LoadReplayPreview()
{
    if ( replaysSelected < 0 || replaysSelected >= (int)replays.size() )
    {
        FreeReplayPreview();
        return;
    }

    const TReplayFile &replay = replays[replaysSelected];

    if ( replayPreviewOf == replay.Path )
        return;

    FreeReplayPreview();

    replayPreviewOf = replay.Path;
    replayPreview = new TReplayPreview;
    TBriefengScreen &brf = replayPreview->Brf;

    // Reading the level's description puts its maps in the world's, which
    // aren't the shell's
    bool loaded = p_YW->LoadDebriefMap(&brf, replay.Level, replay.LevelDescription) && !p_YW->_lvlOwnMap.IsNull() &&
                  p_YW->_lvlOwnMap.Size() == brf.MapSize;

    World::History::LevelLayout level;

    if ( loaded )
    {
        level.MapSize = brf.MapSize;
        level.StartOwners.assign(brf.MapSize.x * brf.MapSize.y, 0);

        for (int y = 1; y < brf.MapSize.y - 1; y++)
        {
            for (int x = 1; x < brf.MapSize.x - 1; x++)
                level.StartOwners[y * brf.MapSize.x + x] = p_YW->_lvlOwnMap(x, y);
        }

        brf.OwnMap = p_YW->_lvlOwnMap;
        replayPreview->StartOwners = p_YW->_lvlOwnMap;
    }

    p_YW->_lvlTypeMap.Clear();
    p_YW->_lvlOwnMap.Clear();
    p_YW->_lvlHeightMap.Clear();
    p_YW->_lvlBuildingsMap.Clear();

    if ( !loaded || !replayPreview->History.Open(replay.Path, level) )
    {
        ypa_log_out("No map of the replay %s\n", replay.Path.c_str());
        delete replayPreview;
        replayPreview = NULL;
        return;
    }

    // The map at its shape in the area for it
    const Common::Rect &area = replaysMapArea;
    float aspect = (float)brf.MapSize.x / std::max(1, brf.MapSize.y);
    int w = area.Width(), h = area.Height();

    if ( w > h * aspect )
        w = h * aspect;
    else
        h = w / aspect;

    int x = area.left + (area.Width() - w) / 2;
    int y = area.top;

    auto nx = [this](int px) { return 2.0f * px / p_YW->_screenSize.x - 1.0f; };
    auto ny = [this](int py) { return 2.0f * py / p_YW->_screenSize.y - 1.0f; };

    brf.MapBlitEnd = Common::FRect(nx(x), ny(y), nx(x + w), ny(y + h));
    brf.MapBlitParams.pbitm = brf.MbmapImg->GetBitmap();
    brf.MapBlitParams.srcRect = Common::FRect(-1.0, -1.0, 1.0, 1.0);
    brf.MapBlitParams.dstRect = brf.MapBlitEnd;
}

void UserData::ReplaysUiHandleInput()
{
    if ( Input->KbdLastHit == Input::KC_ESCAPE || Input->HotKeyID == 24 )
    {
        CloseReplays();
        return;
    }

    if ( Input->KbdLastHit == Input::KC_RETURN )
        ViewSelectedReplay();

    NC_STACK_button::ResCode r = replays_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        switch ( r.code )
        {
        case REPLAYS_VIEW_UP:
            ViewSelectedReplay();
            break;

        case REPLAYS_DELETE_UP:
            if ( !replays.empty() )
                ShowConfirmDialog(7, Locale::Text::Advanced(Locale::ADV_RLYDELETEREPLAY), "", 0);
            break;

        case REPLAYS_BACK_UP:
            CloseReplays();
            return;

        default:
            break;
        }
    }

    replays_listvw.InputHandle(p_YW, Input);

    if ( (replays_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT) ||
         Input->KbdLastHit == Input::KC_UP || Input->KbdLastHit == Input::KC_DOWN )
        replaysSelected = std::max(0, std::min((int)replays_listvw.selectedEntry, (int)replays.size() - 1));

    replays_listvw.Formate(p_YW);

    NC_STACK_button::WidgetState state;
    state.state = 0;

    state.butID = REPLAYS_VIEW;
    if ( replays.empty() )
        replays_button->Disable(&state);
    else
        replays_button->Enable(&state);

    state.butID = REPLAYS_DELETE;
    if ( replays.empty() )
        replays_button->Disable(&state);
    else
        replays_button->Enable(&state);

    LoadReplayPreview();
}

static void DrawReplaysList(NC_STACK_ypaworld *yw, UserData *usr)
{
    GuiList &list = usr->replays_listvw;

    list.itemBlock.clear();
    list.SetRect(yw, -2, -2);
    list.ItemsPreLayout(yw, &list.itemBlock, 0, "uvw");

    int rowWidth = list.entryWidth - 2 * yw->_fontBorderW;
    int index = 0;

    for ( const TReplayFile &replay : usr->replays )
    {
        if ( index >= list.firstShownEntries && index < list.firstShownEntries + list.shownEntries )
        {
            bool selected = index == usr->replaysSelected;
            int fontId = selected ? 9 : 0;
            int fill = selected ? 99 : 102;

            FontUA::ColumnItem columns[4];
            const char *texts[4];
            std::string name = (replay.Level > 0 && replay.Level < (int)yw->_globalMapRegions.MapRegions.size()) ?
                               yw->GetLevelName(replay.Level) : fmt::sprintf("%d", replay.Level);
            std::string length = fmt::sprintf("%d:%02d", replay.Duration / 1024 / 60, replay.Duration / 1024 % 60);
            std::string size = FileSizeText(replay.Size);
            texts[0] = name.c_str();
            texts[1] = replay.Date.c_str();
            texts[2] = length.c_str();
            texts[3] = size.c_str();

            // The name and the date on the left, the length and the size
            // on the right; the row's ends before the first and after the
            // last
            const float widths[4] = {0.44, 0.28, 0.12, 0.16};
            const int16_t flags[4] = {0x20 | 4 | 1, 0x20 | 4, 0x20 | 8, 0x20 | 8 | 2};

            for (int i = 0; i < 4; i++)
            {
                columns[i].txt = texts[i];
                columns[i].fontID = fontId;
                columns[i].spaceChar = fill;
                columns[i].width = rowWidth * widths[i];
                columns[i].prefixChar = selected ? 98 : 102;
                columns[i].postfixChar = selected ? 100 : 102;
                columns[i].flags = flags[i];
            }

            columns[3].width = rowWidth - columns[0].width - columns[1].width - columns[2].width;

            const SDL_Color &color = yw->_iniColors[selected ? 62 : 61];

            FontUA::select_tileset(&list.itemBlock, 0);
            FontUA::store_u8(&list.itemBlock, 123);
            FontUA::set_txtColor(&list.itemBlock, color.r, color.g, color.b);
            FormateColumnItem(yw, &list.itemBlock, 4, columns);
            FontUA::select_tileset(&list.itemBlock, 0);
            FontUA::store_u8(&list.itemBlock, 125);
            FontUA::next_line(&list.itemBlock);
        }

        index++;
    }

    if ( index < ReplaysListRows )
    {
        list.shownEntries = index;

        for (int i = index; i < ReplaysListRows; i++)
            FormatListItem(yw, &list, &list.itemBlock, (i == 0) ? Locale::Text::Title(Locale::TITLE_NOREPLAYS) : " ");
    }

    list.numEntries = index;
    list.ItemsPostLayout(yw, &list.itemBlock, 0, "xyz");

    FontUA::set_end(&list.itemBlock);

    GFX::Engine.ProcessDrawSeq(list.cmdCommands, &list.cmdInclude);
}

// The map of the replay chosen, its history played over it as in its
// debriefing, again and again
static void DrawReplayPreview(NC_STACK_ypaworld *yw, UserData *usr)
{
    TReplayPreview *preview = usr->replayPreview;

    if ( !preview )
        return;

    TBriefengScreen &brf = preview->Brf;

    if ( preview->History.AtEnd() )
    {
        preview->Ended += usr->DTime;

        if ( preview->Ended > PreviewEndWait )
        {
            preview->History.Rewind();
            preview->Time = 0;
            preview->Ended = 0;
            brf.EventsReplayed = 0;
            brf.StatsIngame.fill(World::TPlayerStatus());
            brf.OwnMap = preview->StartOwners;
        }
    }
    else
    {
        preview->Time += PreviewSpeed * usr->DTime;
    }

    GFX::Engine.DrawClippedBitmapNormalized(&brf.MapBlitParams);
    yw_DebriefRenderSectorsOwners(yw, &brf);
    DebriefPlayHistory(yw, &brf, preview->History, preview->Time, false);
}

void DrawReplaysPage(NC_STACK_ypaworld *yw, UserData *usr)
{
    usr->replays_button->Draw();
    DrawReplaysList(yw, usr);
    DrawReplayPreview(yw, usr);
}
