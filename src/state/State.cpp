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
#include "ReplaySelectionState.h"
#include "NetworkState.h"
#include "NetworkSearchState.h"
#include "OptionsState.h"

#include "BlobbyApp.h"
#include "SoundManager.h"
#include "IMGUI.h"
#include "TextManager.h"
#include "SpeedController.h"
#include "InputManager.h"
#include "IScriptableComponent.h"
#include "WarmUI.h"

/* implementation */

State::State() = default;

void State::playSound(const std::string& sound, float volume) {
	assert(m_App);
	m_App->getSoundManager().playSound(sound, volume);
}

bool State::is_exiting() const {
	return getInputMgr().exit();
}

bool State::clicked() const {
	return getInputMgr().click();
}

void State::setApp(BlobbyApp* app) {
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

IMGUI& State::getIMGUI() const {
	return getApp().getIMGUI();
}

InputManager& State::getInputMgr() const {
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

struct MainMenuState::Menu {
 WarmUI::Navigation navigation,confirm;
 bool quitting=false;
 UserConfig config;
 Menu(){config.loadFile("config.xml");}
};
MainMenuState::~MainMenuState() = default;
void MainMenuState::init() { mMenu.reset(new Menu);draw(false); }
void MainMenuState::step_impl() { if(!mMenu)mMenu.reset(new Menu);draw(true); }
void MainMenuState::draw(bool interactive)
{
 using namespace WarmUI;
 auto& g=getIMGUI();auto& in=getInputMgr();auto& m=*mMenu;
 const auto* config=&m.config;
 language()=config->getString("language");
 getApp().getRenderManager().setCourtWidth(config->getBool("court_widescreen",false)?3200.f/3.f:800.f);
 g.doImage(GEN_ID,Vector2(400,300),"menu-backgrounds/"+config->getString("background"),Vector2(800,600));
 g.doOverlay(GEN_ID,Vector2(0,0),Vector2(800,600));
 g.doImage(GEN_ID,Vector2(211,111),"gfx/titel.bmp",Vector2(350,202));
 m.navigation.begin(in,7,interactive&&!m.quitting);
 const char* labelsDE[]={"Spiel starten","Online spielen","Im LAN spielen","Optionen","Replays","Mitwirkende","Beenden"};
 const char* labelsEN[]={"Start match","Play online","Play on LAN","Options","Replays","Credits","Quit"};
 int action=-1;
 for(int i=0;i<7;++i)if(m.navigation.button(g,in,i,40,212.f+i*40.f,350,34,tr(labelsDE[i],labelsEN[i]),false,true))action=i;
 const char* de[]={"Ab auf den Sand!","Gemeinsam online spielen","Ein Match im lokalen Netzwerk","Dein Spiel, deine Einstellungen","Deine besten Ballwechsel","Das Team hinter Blobby Volley","Bis zum n\xC3\xA4" "chsten Match"};
 const char* en[]={"Back to the beach!","Play together online","A match on your local network","Your game, your settings","Your best rallies","The team behind Blobby Volley","See you next match"};
 text(g,420,406,tr(de[m.navigation.focus],en[m.navigation.focus]),31,2);
 const char* detailsDE[]={"Starte ein Match gegen Freunde","Finde Mitspieler im Internet.","Spiele mit Freunden im selben Netz.","Passe Spiel, Spieler und Anzeige an.","Sieh dir gespeicherte Spiele an.","Ideen, Entwicklung und Gestaltung.","Schlie\xC3\x9F" "e Blobby Volley 2 HD."};
 const char* detailsEN[]={"Start a match against friends","Find other players online.","Play with friends on your network.","Adjust gameplay, players and display.","Watch your saved matches.","Ideas, development and artwork.","Close Blobby Volley 2 HD."};
 text(g,420,443,tr(detailsDE[m.navigation.focus],detailsEN[m.navigation.focus]),22);
 if(m.navigation.focus==0)text(g,420,464,tr("oder den Computer.","or the computer."),22);
 text(g,40,571,tr("A / Enter  Ausw\xC3\xA4hlen     B / Esc  Beenden","A / Enter  Select     B / Esc  Quit"),18);
 text(g,760,571,"Blobby Volley 2 HD",18,2,2);
 bool opening=interactive&&!m.quitting&&(action==6||in.exit());
 if(opening){m.quitting=true;m.confirm.focus=0;}
 if(m.quitting){
  shade(g,"quit-app");panel(g,144,191,512,215,Color(65,45,29),.98f);
  text(g,400,214,tr("Blobby Volley 2 HD beenden?","Quit Blobby Volley 2 HD?"),35,2,1);
  text(g,400,259,tr("Das Spiel wird geschlossen.","The game will close."),24,0,1);
  m.confirm.begin(in,2,interactive&&!opening);
  if(m.confirm.button(g,in,0,171,310,219,38,tr("Im Men\xC3\xBC bleiben","Stay in menu"))||(!opening&&interactive&&in.exit()))m.quitting=false;
  if(m.confirm.button(g,in,1,406,310,223,38,tr("Beenden","Quit")))in.setEndBlobby();
  text(g,400,378,tr("A  Ausw\xC3\xA4hlen     B  Abbrechen","A  Select     B  Cancel"),18,0,1);
 }else if(interactive){
  switch(action){
   case 0:try{switchState(new LocalGameState());}catch(const ScriptException& e){std::fprintf(stderr,"Lua Error: %s\n",e.luaerror.c_str());}break;
   case 1:switchState(new OnlineSearchState());break;
   case 2:switchState(new LANSearchState());break;
   case 3:switchState(new OptionState());break;
   case 4:switchState(new ReplaySelectionState());break;
   case 5:switchState(new CreditsState());break;
   default:break;
  }
 }
 g.doCursor();
}

const char* MainMenuState::getStateName() const
{
	return "MainMenuState";
}

CreditsState::CreditsState()
{
	mYPosition = 600;
}

void CreditsState::step_impl()
{
	IMGUI& imgui = getIMGUI();
	imgui.doCursor();
	imgui.doImage(GEN_ID, Vector2(400.0, 300.0), "background");
	imgui.doOverlay(GEN_ID, Vector2(0.0, 0.0), Vector2(800.0, 600.0));

	const float xPosition = 50;

	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition), TextManager::CRD_PROGRAMMERS);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+30), "Daniel Knobe");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+60), "  (daniel-knobe(at)web.de)", TF_SMALL_FONT);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+85), "Jonathan Sieber");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+115), "  (jonathan_sieber(at)yahoo.de)", TF_SMALL_FONT);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+140), "Erik Schultheis");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+170), "  (erik-schultheis(at)freenet.de)", TF_SMALL_FONT);

	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+210), TextManager::CRD_GRAPHICS);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+240), "Silvio Mummert");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+270), "  (mummertathome(at)t-online.de)", TF_SMALL_FONT);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+295), "Richard Bertrand");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+325), "  (ricbertrand(at)hotmail.com)", TF_SMALL_FONT);

	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+365), TextManager::CRD_THX);
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+395), "Daniel Skoraszewsky");
	imgui.doText(GEN_ID, Vector2(xPosition, mYPosition+425), "  (skoraszewsky(at)t-online.de)", TF_SMALL_FONT);

	if (mYPosition > 20)
		mYPosition -= 2.5;

	if (imgui.doButton(GEN_ID, Vector2(74.0, 530.0), TextManager::LBL_OK))
	{
		switchState(new MainMenuState());
	}

#if SDL_VERSION_ATLEAST(2, 0, 14)
	{
		SDL_version linked;
		SDL_GetVersion(&linked);

		if (SDL_VERSIONNUM(linked.major, linked.minor, linked.patch) >=
			SDL_VERSIONNUM(2, 0, 14))
		{
			if (imgui.doButton(GEN_ID, Vector2(294.0, 530.0), "www.blobbyvolley.de"))
			{
				SDL_OpenURL("https://www.blobbyvolley.de");
			}
		}
	}
#endif
}

const char* CreditsState::getStateName() const
{
	return "CreditsState";
}

