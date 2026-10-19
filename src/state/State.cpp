/*=============================================================================
Blobby Volley 2
Copyright (C) 2006 Jonathan Sieber (jonathan_sieber@yahoo.de)
Copyright (C) 2006 Daniel Knobe (daniel-knobe@web.de)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

/* header include */
#include "State.h"

/* includes */
#include <algorithm>
#include <cstdio>

#include "LocalGameState.h"
#include "LocalSetupState.h"
#include "NetworkSearchState.h"
#include "NetworkState.h"
#include "OptionsState.h"
#include "ReplaySelectionState.h"

#include "BlobbyApp.h"
#include "IMGUI.h"
#include "IScriptableComponent.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "SpeedController.h"
#include "TextManager.h"
#include "WarmUI.h"

/* implementation */

State::State() = default;

void State::playSound(const std::string& sound, float volume)
{
    assert(m_App);
    m_App->getSoundManager().playSound(sound, volume);
}

bool State::is_exiting() const
{
    return getInputMgr().exit();
}

bool State::clicked() const
{
    return getInputMgr().click();
}

void State::setApp(BlobbyApp* app)
{
    assert(app);
    m_App = app;
}

void State::switchState(State* newState)
{
    assert(m_App);
    m_App->switchToState(std::unique_ptr<State>(newState));
}

BlobbyApp& State::getApp() const
{
    assert(m_App);
    return *m_App;
}

IMGUI& State::getIMGUI() const
{
    return getApp().getIMGUI();
}

InputManager& State::getInputMgr() const
{
    return getApp().getInputManager();
}

// --------------------------------------------------------------------------------------------------------------------------------
//  concrete implementation of MainMenuState and CreditsState
// -----------------------------------------------------------

MainMenuState::MainMenuState()
{
    // set main menu fps
    SpeedController::getMainInstance()->setGameSpeed(75);
}

struct MainMenuState::Menu
{
    WarmUI::Navigation navigation, confirm;
    bool quitting = false;
    UserConfig config;
    PlayerProfiles profiles;
    Menu()
    {
        config.loadFile("config.xml");
    }
};
MainMenuState::~MainMenuState() = default;
void MainMenuState::init()
{
    mMenu.reset(new Menu);
    draw(false);
}
void MainMenuState::step_impl()
{
    if (!mMenu)
    {
        mMenu.reset(new Menu);
    }
    draw(true);
}
void MainMenuState::draw(bool interactive)
{
    using namespace WarmUI;
    auto& g = getIMGUI();
    auto& in = getInputMgr();
    auto& m = *mMenu;
    const auto* config = &m.config;
    language() = config->getString("language");
    getApp().getRenderManager().setCourtWidth(config->getBool("court_widescreen", false) ? 3200.f / 3.f
                                                                                         : 800.f);
    g.doImage(GEN_ID, Vector2(400, 300), "menu-backgrounds/" + config->getString("background"),
              Vector2(800, 600));
    g.doOverlay(GEN_ID, Vector2(0, 0), Vector2(800, 600));
    g.doImage(GEN_ID, Vector2(700, 43), "gfx/titel.bmp", Vector2(120, 69));
    text(g, 40, 35, tr("Dein Spiel. Dein Strand.", "Your game. Your beach."), 34, 2);
    text(g, 40, 84, tr("Einzelmatch oder Turnier mit Freunden.", "A match or a tournament with friends."),
         20);
    m.navigation.begin(in, 8, interactive && !m.quitting);
    const char* labelsDE[] = {"Lokales Spiel", "Online spielen", "Im LAN spielen", "Optionen",
                              "Replays",       "Mitwirkende",    "Profil",         "Beenden"};
    const char* labelsEN[] = {"Local game", "Play online", "Play on LAN", "Options",
                              "Replays",    "Credits",     "Profile",     "Quit"};
    panel(g, 390, 160, 370, 240, Color(37, 29, 23), .94f);
    panel(g, 390, 424, 370, 85);
    const auto& profile = m.profiles.players.at(m.profiles.selected);
    text(g, 450, 435, profile.name, 22);
    g.doBlob(GEN_ID, Vector2(420, 465),
             profile.cycling ? getApp().getRenderManager().getOscillationColor() : Color(profile.color), .4f);
    int action = -1;
    for (int i = 0; i < 8; ++i)
    {
        const float xs[] = {40, 195, 40, 40, 40, 195, 450, 40};
        const float ys[] = {160, 214, 214, 326, 270, 326, 466, 388};
        const float widths[] = {300, 145, 145, 145, 300, 145, 290, 300};
        if (m.navigation.button(g, in, i, xs[i], ys[i], widths[i], 38, tr(labelsDE[i], labelsEN[i]), false,
                                true))
        {
            action = i;
        }
    }
    const char* de[] = {"Ab auf den Sand!",
                        "Gemeinsam online spielen",
                        "Ein Match im lokalen Netzwerk",
                        "Dein Spiel, deine Einstellungen",
                        "Deine besten Ballwechsel",
                        "Das Team hinter Blobby Volley",
                        "Dein Profil",
                        "Bis zum n\xC3\xA4"
                        "chsten Match"};
    const char* en[] = {"Back to the beach!",
                        "Play together online",
                        "A match on your local network",
                        "Your game, your settings",
                        "Your best rallies",
                        "The team behind Blobby Volley",
                        "Your profile",
                        "See you next match"};
    text(g, 410, 188, tr(de[m.navigation.focus], en[m.navigation.focus]), 31, 2);
    const char* detailsDE[] = {"Starte ein Match gegen Freunde",
                               "Finde Mitspieler im Internet.",
                               "Spiele mit Freunden im selben Netz.",
                               "Passe Spiel, Spieler und Anzeige an.",
                               "Sieh dir gespeicherte Spiele an.",
                               "Ideen, Entwicklung und Gestaltung.",
                               "Name und Blob bearbeiten.",
                               "Schlie\xC3\x9F"
                               "e Blobby Volley 2 HD."};
    const char* detailsEN[] = {"Start a match against friends",      "Find other players online.",
                               "Play with friends on your network.", "Adjust gameplay, players and display.",
                               "Watch your saved matches.",          "Ideas, development and artwork.",
                               "Edit your name and blob.",           "Close Blobby Volley 2 HD."};
    text(g, 410, 248, tr(detailsDE[m.navigation.focus], detailsEN[m.navigation.focus]), 22);
    if (m.navigation.focus == 0)
    {
        text(g, 410, 279, tr("oder den Computer.", "or the computer."), 22);
    }
    text(g, 410, 343,
         tr("Gemeinsam an einem PC oder im Netzwerk.", "Together on one PC or over the network."), 17);
    text(g, 40, 571,
         tr("A / Enter  Ausw\xC3\xA4hlen     B / Esc  Beenden", "A / Enter  Select     B / Esc  Quit"), 18);
    text(g, 760, 571, "Blobby Volley 2 HD", 18, 2, 2);
    bool opening = interactive && !m.quitting && (action == 7 || in.exit());
    if (opening)
    {
        m.quitting = true;
        m.confirm.focus = 0;
    }
    if (m.quitting)
    {
        shade(g, "quit-app");
        panel(g, 144, 191, 512, 215, Color(65, 45, 29), .98f);
        text(g, 400, 214, tr("Blobby Volley 2 HD beenden?", "Quit Blobby Volley 2 HD?"), 35, 2, 1);
        text(g, 400, 259, tr("Das Spiel wird geschlossen.", "The game will close."), 24, 0, 1);
        m.confirm.begin(in, 2, interactive && !opening);
        if (m.confirm.button(g, in, 0, 171, 310, 219, 38, tr("Im Men\xC3\xBC bleiben", "Stay in menu")) ||
            (!opening && interactive && in.exit()))
        {
            m.quitting = false;
        }
        if (m.confirm.button(g, in, 1, 406, 310, 223, 38, tr("Beenden", "Quit")))
        {
            in.setEndBlobby();
        }
        text(g, 400, 378, tr("A  Ausw\xC3\xA4hlen     B  Abbrechen", "A  Select     B  Cancel"), 18, 0, 1);
    }
    else if (interactive)
    {
        switch (action)
        {
        case 0:
            try
            {
                switchState(new LocalSetupState());
            }
            catch (const ScriptException& e)
            {
                std::fprintf(stderr, "Lua Error: %s\n", e.luaerror.c_str());
            }
            break;
        case 1:
            switchState(new LocalSetupState(2));
            break;
        case 2:
            switchState(new LocalSetupState(1));
            break;
        case 3:
            switchState(new OptionState());
            break;
        case 4:
            switchState(new ReplaySelectionState());
            break;
        case 5:
            switchState(new CreditsState());
            break;
        case 6:
            switchState(new LocalSetupState(0));
            break;
        default:
            break;
        }
    }
    g.doCursor();
}

const char* MainMenuState::getStateName() const
{
    return "MainMenuState";
}

CreditsState::CreditsState() = default;

void CreditsState::step_impl()
{
    IMGUI& imgui = getIMGUI();
    using namespace WarmUI;
    page(imgui, translateUI(language(), "Credits"));
    text(imgui, 40, 578, translateUI(language(), "D-pad: navigate    A: select    B: back"), 16);
    panel(imgui, 40, 100, 720, 390);
    text(imgui, 62, 116, imgui.getText(TextManager::CRD_PROGRAMMERS), 24, 2);
    text(imgui, 62, 156, "Daniel Knobe", 22);
    text(imgui, 62, 184, "daniel-knobe(at)web.de", 17);
    text(imgui, 62, 224, "Jonathan Sieber", 22);
    text(imgui, 62, 252, "jonathan_sieber(at)yahoo.de", 17);
    text(imgui, 62, 292, "Erik Schultheis", 22);
    text(imgui, 62, 320, "erik-schultheis(at)freenet.de", 17);
    text(imgui, 420, 116, imgui.getText(TextManager::CRD_GRAPHICS), 24, 2);
    text(imgui, 420, 156, "Silvio Mummert", 22);
    text(imgui, 420, 184, "mummertathome(at)t-online.de", 16);
    text(imgui, 420, 224, "Richard Bertrand", 22);
    text(imgui, 420, 252, "ricbertrand(at)hotmail.com", 17);
    text(imgui, 62, 376, imgui.getText(TextManager::CRD_THX), 24, 2);
    text(imgui, 62, 414, "Daniel Skoraszewsky", 22);
    text(imgui, 62, 442, "skoraszewsky(at)t-online.de", 17);
    if (imgui.doButton(GEN_ID, Vector2(40, 530), "www.blobbyvolley.de", TF_NORMAL, 220))
    {
        SDL_OpenURL("https://www.blobbyvolley.de");
    }
    if (imgui.doButton(GEN_ID, Vector2(620, 530), TextManager::LBL_BACK, TF_NORMAL, 140))
    {
        switchState(new MainMenuState());
        return;
    }
}

const char* CreditsState::getStateName() const
{
    return "CreditsState";
}
