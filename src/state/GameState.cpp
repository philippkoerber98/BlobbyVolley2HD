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
#include "GameState.h"

/* includes */
#include "replays/ReplayRecorder.h"
#include "DuelMatch.h"
#include "BlobbyApp.h"
#include "SoundManager.h"
#include "IMGUI.h"
#include "Blood.h"
#include "FileWrite.h"
#include "Global.h"
#include "RenderInterpolation.h"

/* implementation */


GameState::GameState(DuelMatch* match) : mMatch(match), mSaveReplay(false)
{
 WarmUI::language()=IUserConfigReader::createUserConfigReader("config.xml")->getString("language");
}

GameState::~GameState() = default;

void GameState::render(float alpha)
{
	if(mMatch)getApp().getRenderManager().setCourtWidth(mMatch->getCourtWidth());
	if(!getIMGUI().hasSceneBlur())mDialogKey.clear();
	if (mHasRenderState)
		getApp().getRenderManager().drawGame(interpolateMatch(mPreviousRenderState, mCurrentRenderState, alpha));
}

void GameState::presentGame()
{
	RenderManager& rmanager = getApp().getRenderManager();

	if(mMatch->getPlayer(LEFT_PLAYER).getOscillating())
	{
		rmanager.setBlobColor(LEFT_PLAYER, rmanager.getOscillationColor());
	}
	else
	{
		rmanager.setBlobColor(LEFT_PLAYER, mMatch->getPlayer(LEFT_PLAYER).getStaticColor());
	}

	if(mMatch->getPlayer(RIGHT_PLAYER).getOscillating())
	{
		rmanager.setBlobColor(RIGHT_PLAYER, rmanager.getOscillationColor());
	}
	else
	{
		rmanager.setBlobColor(RIGHT_PLAYER, mMatch->getPlayer(RIGHT_PLAYER).getStaticColor());
	}

	auto events = mMatch->getEvents( );
	for(const auto& e : events )
	{
		if( e.event == MatchEvent::BALL_HIT_BLOB )
		{
			playSound(SoundManager::IMPACT, e.intensity + BALL_HIT_PLAYER_SOUND_VOLUME);
			/// \todo save that position inside the event
			Vector2 hitPos = mMatch->getBallPosition() + (mMatch->getBlobPosition(e.side) - mMatch->getBallPosition()).normalise().scale(31.5);
			getApp().getRenderManager().getBlood().spillBlood(hitPos, e.intensity, e.side);
		}

		if( e.event == MatchEvent::PLAYER_ERROR )
			playSound(SoundManager::WHISTLE, ROUND_START_SOUND_VOLUME);
	}
}

void GameState::presentGameUI()
{
	const auto current = mMatch->getState();
	mPreviousRenderState = mHasRenderState ? mCurrentRenderState : current;
	mCurrentRenderState = current;
	mHasRenderState = true;


 auto& g=getIMGUI();auto& in=getInputMgr();
 // HUD is always below dialogs, including callers that build their modal first.
 g.prepend([&](){
  using namespace WarmUI;
  // Compact TV score bar: keep the court free below the first 54 logical pixels.
  panel(g,177.5f,7.5f,445,31,Color(48,32,22),.94f);
  panel(g,343.75f,7.5f,112.5f,31,Color(255,207,91),1);
  for(int i=0;i<2;++i){
   const auto side=i?RIGHT_PLAYER:LEFT_PLAYER;
   const auto& player=mMatch->getPlayer(side);
   const Color color=player.getOscillating()?getApp().getRenderManager().getOscillationColor():player.getStaticColor();
   panel(g,i?619.5f:178,14,3,17,color,1);
   const auto name=player.getName();float h=19;
   while(RenderManager::menuTextWidth(name,h)>129&&h>11)--h;
   text(g,i?606:194,14,name,h,0,i?2:0);
   const auto score=std::to_string(mMatch->getScore(side));h=28;
   while(RenderManager::menuTextWidth(score,h)>42&&h>14)--h;
   text(g,i?431:368,10,score,h,1,1);
   if(mMatch->getServingPlayer()==side)panel(g,i?471:326,20,4,4,Color(255,207,91),1);
  }
  text(g,400,12,":",23,1,1);
  panel(g,349.4f,38.5f,101.2f,15.5f,Color(48,32,22),.88f);
  text(g,400,40,mMatch->getTimeString()+tr("  /  ZIEL ","  /  TARGET ")+std::to_string(mMatch->getScoreToWin()),14,0,1);
  text(g,780,12,"Esc / Start: Pause",15,0,2);
 });
#if BLOBBY_ON_DESKTOP
 if(mDialogKey.empty()&&!mMatch->isPaused()&&in.click()&&WarmUI::hit(in.position(),704,7,84,26)) {
  SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&e);
 }
#endif

}

bool GameState::beginDialog(const std::string& key,int count)
{
 bool changed=mDialogKey!=key;if(changed){mDialogKey=key;mDialogNavigation.focus=0;getIMGUI().resetSelection();}
 mDialogNavigation.begin(getInputMgr(),count,!changed);
 return !changed;
}


void GameState::displayQueryPrompt(const int height, TextManager::STRING title, const QueryOption& opt1, const QueryOption& opt2, const QueryOption& opt3)
{
 using namespace WarmUI;
 auto& g=getIMGUI();auto& in=getInputMgr();
 bool quit=title==TextManager::LBL_CONF_QUIT;
 bool compact=height<100;
 bool active=beginDialog("query-"+std::to_string(int(title)),3);
 shade(g,mDialogKey);
 const float y=compact?15:179;
 panel(g,compact?100:157,y,compact?600:486,compact?207:234,Color(65,45,29),.98f);
 text(g,400,y+20,quit?tr("Spiel verlassen?","Leave match?"):g.getText(title),39,2,1);
 if(quit){text(g,400,y+66,tr("Das laufende Spiel wird beendet.","The current match will end."),24,0,1);text(g,400,y+91,tr("Du kehrst ins Hauptmen\xC3\xBC zur\xC3\xBC" "ck.","You will return to the main menu."),24,0,1);}
 const auto& safe=quit?opt2:opt1;const auto& leave=quit?opt1:opt2;
 const float by=y+(compact?72:136);
 bool stay=mDialogNavigation.button(g,in,0,compact?122:185,by,compact?266:206,38,quit?tr("Weiterspielen","Continue"):g.getText(std::get<0>(safe)));
 bool exit=mDialogNavigation.button(g,in,1,compact?412:409,by,compact?266:206,38,quit?tr("Spiel verlassen","Leave match"):g.getText(std::get<0>(leave)));
 bool replay=mDialogNavigation.button(g,in,2,280,by+45,240,29,g.getText(std::get<0>(opt3)),true);
 text(g,400,y+(compact?177:211),tr("A  Ausw\xC3\xA4hlen     B  Zur\xC3\xBC" "ck zum Spiel","A  Select     B  Back to game"),17,0,1);
 g.doCursor();
 // Network pause keeps Return for its chat editor; mouse/gamepad can resume.
 if(compact&&(in.getLastActionKey()=="Return"||in.getLastActionKey()=="Space"))stay=exit=replay=false;
 if(stay||(active&&in.exit()&&(quit||title==TextManager::GAME_PAUSED))){mDialogKey.clear();std::get<1>(safe)();}
 else if(exit){mDialogKey.clear();std::get<1>(leave)();}
 else if(replay){mDialogKey.clear();std::get<1>(opt3)();}
}


bool GameState::displaySaveReplayPrompt()
{
 using namespace WarmUI;
 auto& g=getIMGUI();auto& in=getInputMgr();
 const bool opening=mDialogKey!="save-replay";
 if(opening){mDialogKey="save-replay";mDialogNavigation.focus=0;mReplayCursor=unsigned(mFilename.size());SDL_StartTextInput();}
 auto& nav=mDialogNavigation;nav.begin(in,3,false);nav.active=!opening;
 if(!opening){
  if(in.up())nav.focus=(nav.focus+2)%3;
  if(in.down())nav.focus=(nav.focus+1)%3;
  if(nav.focus>0&&(in.left()||in.right()))nav.focus=nav.focus==1?2:1;
  if(in.click()&&hit(in.position(),150,265,500,38)){nav.focus=0;mReplayCursor=unsigned(mFilename.size());}
 }
 bool save=false;
 if(!opening&&nav.focus==0){
  mReplayCursor=std::min(mReplayCursor,unsigned(mFilename.size()));
  auto previous=[&](){if(mReplayCursor){--mReplayCursor;while(mReplayCursor&&(static_cast<unsigned char>(mFilename[mReplayCursor])&0xc0)==0x80)--mReplayCursor;}};
  auto next=[&](){if(mReplayCursor<mFilename.size()){++mReplayCursor;while(mReplayCursor<mFilename.size()&&(static_cast<unsigned char>(mFilename[mReplayCursor])&0xc0)==0x80)++mReplayCursor;}};
  if(in.left())previous();if(in.right())next();
  const auto key=in.getLastTextKey();
  if(key=="backspace"&&mReplayCursor){unsigned end=mReplayCursor;previous();mFilename.erase(mReplayCursor,end-mReplayCursor);}
  else if(key=="del"&&mReplayCursor<mFilename.size()){unsigned start=mReplayCursor;next();mFilename.erase(start,mReplayCursor-start);mReplayCursor=start;}
  else if(key=="paste"){
   char* clipboard=SDL_GetClipboardText();if(clipboard){std::string value=clipboard;SDL_free(clipboard);if(value.size()+mFilename.size()<=64&&value.find_first_of("\\/:*?\"<>|\r\n")==std::string::npos){mFilename.insert(mReplayCursor,value);mReplayCursor+=unsigned(value.size());}}
  }
  else if(key=="space"){if(mFilename.size()<64){mFilename.insert(mReplayCursor," ");++mReplayCursor;}}
  else if(!key.empty()&&key!="return"&&key!="backspace"&&key!="del"&&mFilename.size()+key.size()<=64&&key.find_first_of("\\/:*?\"<>|\r\n")==std::string::npos){mFilename.insert(mReplayCursor,key);mReplayCursor+=unsigned(key.size());}
  if(in.select()&&in.getLastActionKey()!="Space")save=true;
 }
 shade(g,"save-replay");panel(g,100,190,600,220,Color(65,45,29),.98f);
 text(g,400,214,tr("Replay speichern","Save replay"),33,2,1);
 panel(g,149,264,502,40,nav.focus==0?Color(255,207,91):Color(104,75,43),1);
 panel(g,150,265,500,38,Color(45,30,20),1);
 float h=25;while(RenderManager::menuTextWidth(mFilename,h)>470&&h>12)--h;
 text(g,162,270,mFilename,h);
 if(nav.focus==0&&SDL_GetTicks()%1000<600){float x=162+RenderManager::menuTextWidth(mFilename.substr(0,mReplayCursor),h);panel(g,x,272,1,25,Color(255,245,220),1);}
 save |=nav.button(g,in,1,160,337,228,38,tr("Speichern","Save"));
 bool cancel=nav.button(g,in,2,412,337,228,38,tr("Abbrechen","Cancel"))||(!opening&&in.exit());
 text(g,400,383,tr("Pfeiltasten  Navigieren     B / Esc  Abbrechen","Arrows  Navigate     B / Esc  Cancel"),17,0,1);
 g.doCursor();
 if(cancel||(save&&!mFilename.empty())){SDL_StopTextInput();mDialogKey.clear();g.resetSelection();if(cancel)mSaveReplay=false;return !cancel;}
 return false;
}


bool GameState::displayErrorMessageBox()
{
 using namespace WarmUI;
 auto& g=getIMGUI();auto& in=getInputMgr();const bool active=beginDialog("replay-error",1);
 shade(g,"replay-error");panel(g,100,200,600,160,Color(65,45,29),.98f);
 const auto split=mErrorMessage.find(':');
 std::string problem=mErrorMessage.substr(0,split),info=split==std::string::npos?"":mErrorMessage.substr(split+1);
 float h=28;while(RenderManager::menuTextWidth(problem,h)>550&&h>14)--h;text(g,400,215,problem,h,2,1);
 h=22;while(RenderManager::menuTextWidth(info,h)>550&&h>12)--h;text(g,400,258,info,h,0,1);
 bool done=mDialogNavigation.button(g,in,0,300,308,200,36,"OK")||(active&&in.exit());g.doCursor();
 if(done){mErrorMessage.clear();mDialogKey.clear();return true;}return false;
}


bool GameState::displayWinningPlayerScreen(PlayerSide winner)
{
 using namespace WarmUI;
 auto& g=getIMGUI();
 if(!mCelebrating){mCelebrating=true;mVictoryStarted=SDL_GetTicks();}
 shade(g,"victory");
 g.doConfetti(GEN_ID,mVictoryStarted);
 text(g,400,76,tr("MATCH GEWONNEN","MATCH WON"),17,2,1);
 const auto& player=mMatch->getPlayer(winner);
 float h=59;while(RenderManager::menuTextWidth(player.getName(),h)>620&&h>24)--h;
 text(g,400,98,player.getName(),h,0,1);
 text(g,400,146,tr("Der Strand geh\xC3\xB6rt dir!","The beach is yours!"),22,2,1);
 const Color color=player.getOscillating()?getApp().getRenderManager().getOscillationColor():player.getStaticColor();
 // The champion stands freely: no podium box or gold line behind the blob.
 g.doBlob(GEN_ID,Vector2(376,330),color,1.5f);
 g.doImage(GEN_ID,Vector2(498,315),"gfx/pokal.bmp",Vector2(43,119));
 panel(g,271,414,258,40,Color(48,32,22),.94f);
 for(int i=0;i<2;++i){
  const auto side=i?RIGHT_PLAYER:LEFT_PLAYER;
  const auto score=std::to_string(mMatch->getScore(side));float size=46;
  while(RenderManager::menuTextWidth(score,size)>90&&size>20)--size;
  text(g,i?462.5f:337.5f,414,score,size,side==winner?2:0,1);
 }
 text(g,400,416,":",37,0,1);
 text(g,400,463,tr("Spielzeit ","Match time ")+mMatch->getTimeString(),18,0,1);
 g.doCursor();return false;
}

int GameState::displayVictoryActions(bool rematch,bool replay)
{
 using namespace WarmUI;
 auto& g=getIMGUI();auto& in=getInputMgr();bool active=beginDialog("victory-actions",rematch&&!replay?3:2);
 int result=0;
 if(rematch){
  if(mDialogNavigation.button(g,in,0,206,501,187,34,replay?g.getText(TextManager::RP_SHOW_AGAIN):tr("Noch eine Runde","Play again")))result=1;
  if(mDialogNavigation.button(g,in,1,406,501,187,34,replay?g.getText(TextManager::MNU_LABEL_REPLAY):tr("Hauptmen\xC3\xBC","Main menu")))result=2;
  if(!replay&&mDialogNavigation.button(g,in,2,280,546,240,29,g.getText(TextManager::RP_SAVE),true))result=3;
 }else{
  if(mDialogNavigation.button(g,in,0,206,501,187,34,tr("Hauptmen\xC3\xBC","Main menu")))result=2;
  if(mDialogNavigation.button(g,in,1,406,501,187,34,g.getText(TextManager::RP_SAVE)))result=3;
 }
 if(active&&in.exit())result=2;
 if(result)mDialogKey.clear();return result;
}

void GameState::setDefaultReplayName(const std::string& left, const std::string& right)
{
	mFilename = left;
	if(mFilename.size() > 7)
		mFilename.resize(7);
	mFilename += " vs ";

	std::string opp = right;
	if(right.size() > 7)
		opp.resize(7);
	mFilename += opp;
}

void GameState::saveReplay(ReplayRecorder& recorder)
{
	try
	{
		if (!mFilename.empty())
		{
			std::string repFileName = std::string("replays/") + mFilename + std::string(".bvr");

			FileWrite savetarget(repFileName);
			/// \todo add a check whether we overwrite a file
			recorder.save(savetarget);
			mSaveReplay = false;
		}
	}
	catch( FileLoadException& ex)
	{
		mErrorMessage = std::string("Unable to create file:" + ex.getFileName());
		mSaveReplay = true;	// try again
	}
	catch( FileAlreadyExistsException& ex)
	{
		mErrorMessage = std::string("File already exists!:"+ ex.getFileName());
		mSaveReplay = true;	// try again
	}
	catch( std::exception& ex)
	{
		mErrorMessage = std::string("Could not save replay: ");
		mSaveReplay = true;	// try again
	}
}
