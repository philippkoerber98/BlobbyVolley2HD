#include "BackgroundSettings.h"
#include "OptionsState.h"
#include "UITranslations.h"
#include "BlobbyApp.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include "SpeedController.h"
#include "Blood.h"
#include "FileSystem.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

namespace {
// Sand, sunshine and cream echo the beach without competing with the logo.
const Color surface(65,45,29), accent(255,207,91), focusSurface(104,75,43);
int wrap(int value,int count) { return count ? (value%count+count)%count : 0; }
std::string sidePrefix(int player) { return player ? "right_" : "left_"; }
std::string inputPrefix(int player) { return sidePrefix(player)+"blobby_"; }
}

// One settings session owns every tab. Completed edits are applied immediately.
struct OptionState::Menu
{
	OptionState& owner;
	UserConfig config, input, original;
	int tab=0, focus=0, previousCount=0, rowCount=0, dragging=-1, inputPlayer=0;
	bool dirty=false, confirmReset=false, suppressActions=false;
	std::string notice, helpTitle, helpText, captureKey;
	enum Capture { NONE, NAME, KEYBOARD, JOYSTICK, MOUSE } capture=NONE;
	Uint32 captureAfter=0;
	Vector2 mouse, lastMouse=Vector2(-1,-1);
	bool mouseMoved=false;
	std::vector<std::string> backgrounds, rules, scripts;
	IMGUI& gui() { return owner.getIMGUI(); }
	InputManager& in() { return owner.getInputMgr(); }
	bool de() const { return config.getString("language")=="de"; }
	std::string tr(const char* german,const char* english) const { return translateUI(config.getString("language"),english); }

	explicit Menu(OptionState& state) : owner(state)
	{
		config.loadFile("config.xml"); original.loadFile("config.xml"); input.loadFile("inputconfig.xml");
		auto& fs=FileSystem::getSingleton();
		backgrounds=availableBackgroundNames();
		std::sort(backgrounds.begin(),backgrounds.end());
		rules=fs.enumerateFiles("rules",".lua",true); std::sort(rules.begin(),rules.end());
		scripts=fs.enumerateFiles("scripts",".lua",false); std::sort(scripts.begin(),scripts.end());
		scripts.insert(scripts.begin(),"Human");
	}
	~Menu() { if(capture==NAME) SDL_StopTextInput(); }

	void text(float x,float y,const std::string& s,float height=22,int style=0)
	{ if(y>=546 && x==40)height=(std::min)(height,height*720/(std::max)(1.f,RenderManager::menuTextWidth(s,height)));gui().doMenuText(gui().getNextId(),Vector2(x,y),s,height,style); }
	void rightText(float x,float y,const std::string& s,float height=22,int style=0)
	{ text(x-RenderManager::menuTextWidth(s,height),y,s,height,style); }
	void panel(float x,float y,float w,float h,Color col=surface,float opacity=.88f)
 {
  if(w<=0||h<=0)return;
  if(w<8||h<8)gui().doOverlay(gui().getNextId(),Vector2(x,y),Vector2(x+w,y+h),col,opacity);
  else gui().doRoundedPanel(gui().getNextId(),Vector2(x,y),Vector2(x+w,y+h),col,opacity);
 }

	bool hit(float x,float y,float w,float h) const
	{ return mouse.x>=x&&mouse.x<x+w&&mouse.y>=y&&mouse.y<y+h; }
	void category(int next)
	{ tab=wrap(next,5); focus=0; previousCount=0; dragging=-1; }
	int widget(float x,float y,float w,float h,const std::string& title,const std::string& explanation)
	{
		const int id=rowCount++;
		const bool hover=hit(x,y,w,h);
		if(!suppressActions&&capture==NONE&&!confirmReset && hover&&(mouseMoved||in().click())) focus=id;
		if(id==focus) { helpTitle=title; helpText=explanation; }
		return id;
	}
	int action(int id,float x,float y,float w,float h)
	{
		if(capture!=NONE||confirmReset||suppressActions) return 0;
		if(hit(x,y,w,h)&&in().click()) return -1*(SDL_GetModState()&KMOD_SHIFT?1:-1);
		if(id!=focus) return 0;
		if(in().left()) return -1;
		if(in().right()||in().select()) return 1;
		return 0;
	}
	int row(float y,const std::string& label,const std::string& value,const std::string& explanation,
		float x=40,float w=350)
	{
		int id=widget(x,y,w,34,label,explanation);
		if(focus==id) panel(x-1,y-1,w+2,36,accent);
		panel(x,y,w,34,id==focus?focusSurface:surface);
		text(x+12,y+6,label);
		rightText(x+w-12,y+6,value,21,id==focus?2:0);
		return action(id,x,y,w,34);
	}
	void integer(float y,const std::string& label,UserConfig& c,const std::string& key,int lo,int hi,
		int fallback,const std::string& explanation,float x=40,float w=350)
	{
		int value=c.getInteger(key,fallback),change=row(y,label,"<  "+std::to_string(value)+"  >",explanation,x,w);
		if(change) { c.setInteger(key,std::max(lo,std::min(hi,value+change))); dirty=true; }
	}
	void toggle(float y,const std::string& label,UserConfig& c,const std::string& key,const std::string& explanation,
		float x=40,float w=350)
	{
		if(row(y,label,c.getBool(key)?tr("Ein","On"):tr("Aus","Off"),explanation,x,w))
		{ c.setBool(key,!c.getBool(key)); dirty=true; }
	}
	void choice(float y,const std::string& label,UserConfig& c,const std::string& key,
		const std::vector<std::string>& values,const std::vector<std::string>& names,const std::string& explanation,
		float x=40,float w=350)
	{
		if(values.empty()) return;
		auto found=std::find(values.begin(),values.end(),c.getString(key));
		int index=found==values.end()?0:int(found-values.begin());
		int delta=row(y,label,"<  "+names[index]+"  >",explanation,x,w);
		if(delta) { c.setString(key,values[wrap(index+delta,int(values.size()))]); dirty=true; }
	}
	bool slider(float x,float y,float w,const std::string& label,float& value,
		const std::string& display,const std::string& explanation,Color colour=accent,float step=.01f,float height=42)
	{
		const int id=widget(x,y,w,height,label,explanation);
		if(id==focus) panel(x-1,y-1,w+2,height+2,accent);
		panel(x,y,w,height,id==focus?focusSurface:surface);
		text(x+12,y+2,label); rightText(x+w-12,y+2,display,22,2);
		float next=value;
		if(capture==NONE&&!confirmReset&&!suppressActions)
		{
			if(hit(x+8,y+24,w-16,22)&&in().click()) dragging=id;
			if(dragging==id) next=(mouse.x-x-12)/(w-24);
			else if(focus==id) { if(in().left()) next-=step; if(in().right()) next+=step; }
		}
		next=std::max(0.f,std::min(1.f,next));
		bool changed=std::abs(next-value)>.00001f;
		if(changed){value=next;dirty=true;}
		panel(x+12,y+32,w-24,4,Color(132,108,73),1);
		if(value>0)panel(x+12,y+32,(w-24)*value,4,colour,1);
		panel(x+8+(w-24)*value,y+28,8,12,Color(255,245,220),1);
		return changed;
	}
	bool button(float x,float y,float w,const std::string& label,bool primary=false)
	{
		int id=widget(x,y,w,34,label,"");
		if(focus==id) panel(x-1,y-1,w+2,36,accent);
		panel(x,y,w,34,primary?accent:surface);
		const float size=(std::min)(21.f,21.f*(w-12)/(std::max)(1.f,RenderManager::menuTextWidth(label,21)));
		text(x+(w-RenderManager::menuTextWidth(label,size))/2,y+(34-size)/2,label,size,primary?1:0);
		return capture==NONE&&!confirmReset&&!suppressActions&&
			((hit(x,y,w,34)&&in().click())||(focus==id&&in().select()));
	}
	void section(float y,const std::string& label,float x=40) { text(x,y,label,16); }
	void captureInput(Capture kind,const std::string& key)
	{ capture=kind;captureKey=key;captureAfter=SDL_GetTicks()+180;dragging=-1;if(kind==NAME){nameBefore=config.getString(key);SDL_StartTextInput();} }
	void nameRow(float x,float y,int player)
	{
		const auto key=sidePrefix(player)+"player_name";
		if(row(y,tr("Name","Name"),config.getString(key),tr("Namen eingeben; Enter bestätigt, Esc bricht ab.","Enter a name; Enter confirms, Esc cancels."),x,350)) captureInput(NAME,key);
	}

	void game()
	{
		section(128,tr("SPIELREGELN","GAME RULES"));
		choice(148,tr("Regelwerk","Rules"),config,"rules",rules,rules,tr("Regelwerk für das nächste Spiel.","Rules for the next match."));
		integer(188,tr("Punkte bis zum Sieg","Points to win"),config,"scoretowin",1,99,15,tr("Benötigte Punkte für einen Sieg.","Points needed to win."));
		int speed=std::max(30,std::min(120,config.getInteger("gamefps",75)));
		float value=(speed-30)/90.f;
		if(slider(40,228,350,tr("Spieltempo","Game speed"),value,std::to_string(int(std::lround(speed*100.f/75)))+" %",
			tr("100 % ist das klassische Spieltempo. Die Bildrate verändert es nicht.","100% is the original game speed. Rendering FPS does not affect it."),accent,1.f/90)) config.setInteger("gamefps",int(std::lround(value*90+30)));
		const int speeds[]={30,60,75,90,120}; const char* labels[]={"Very slow","Slow","Normal","Fast","Very fast"};
		for(int i=0;i<5;++i) if(button(40+i*71,276,66,tr(labels[i],labels[i]),config.getInteger("gamefps",75)==speeds[i])) {config.setInteger("gamefps",speeds[i]);dirty=true;}
		section(322,tr("ALLGEMEIN","GENERAL"));
		std::vector<std::string> languages,names;for(auto& lang:TextManager::languageNames()){languages.push_back(lang.first);names.push_back(lang.second);}
		choice(342,tr("Sprache","Language"),config,"language",languages,names,tr("Die Menüsprache wird sofort umgestellt.","The menu language changes immediately."));
		choice(382,tr("Netzwerkseite","Network side"),config,"network_side",{"0","1"},{tr("Links","Left"),tr("Rechts","Right")},tr("Bevorzugte Seite im Netzwerkspiel.","Preferred side in network matches."));
		toggle(422,tr("FPS anzeigen","Show FPS"),config,"showfps",tr("Aktuelle Darstellungsrate anzeigen.","Display the current rendering FPS."));
		toggle(462,tr("Blut","Blood"),config,"blood",tr("Blutpartikel ein- oder ausschalten.","Enable or disable blood particles."));
	}
	void players()
	{
		for(int player=0;player<2;++player)
		{
			float x=40+player*370;std::string pre=sidePrefix(player);
			section(128,tr("SPIELER ","PLAYER ")+std::to_string(player+1),x);
			nameRow(x,148,player);
            std::string script=config.getBool(pre+"player_human",true)?"Human":config.getString(pre+"script_name");
            auto it=std::find(scripts.begin(),scripts.end(),script);int index=it==scripts.end()?0:int(it-scripts.begin());
            if(int delta=row(188,tr("Spielerart","Player type"),index?scripts[index]:tr("Mensch","Human"),tr("Jeder Bot nutzt in 16:9 seine Breitbild-Variante. Die Originalskripte für 4:3 bleiben unverändert.","Each bot uses its widescreen variant in 16:9. Original 4:3 scripts stay unchanged."),x,350))
            {index=wrap(index+delta,int(scripts.size()));config.setBool(pre+"player_human",index==0);if(index)config.setString(pre+"script_name",scripts[index]);dirty=true;}

			int delay=config.getInteger(pre+"script_strength");float strength=1-std::max(0,std::min(25,delay))/25.f;
			if(slider(x,228,350,tr("Bot-Stärke","Bot strength"),strength,std::to_string(int(std::lround(strength*100)))+" %",tr("Reaktionsstärke des ausgewählten Bots.","Reaction strength of the selected bot."),accent,.01f,42)) config.setInteger(pre+"script_strength",int(std::lround((1-strength)*25)));
			const char* channels[]={"r","g","b"};const char* german[]={"Rot","Grün","Blau"};const char* english[]={"Red","Green","Blue"};Color colors[]={Color(238,124,121),Color(173,228,106),Color(115,197,242)};
			for(int j=0;j<3;++j){auto key=pre+"blobby_color_"+channels[j];int c=config.getInteger(key,player?255:128);float v=c/255.f;
				if(slider(x,278+j*48,350,tr(german[j],english[j]),v,std::to_string(c),tr("RGB-Farbe des Blobs. Die Vorschau aktualisiert sich sofort.","Blob RGB colour. The preview updates immediately."),colors[j],1.f/255,42))config.setInteger(key,int(std::lround(v*255)));}
			toggle(424,tr("Farbwechsel","Colour cycling"),config,pre+"blobby_oscillate",tr("Blob-Farbe automatisch wechseln lassen.","Cycle the blob colour automatically."),x,350);
			Color col(config.getInteger(pre+"blobby_color_r"),config.getInteger(pre+"blobby_color_g"),config.getInteger(pre+"blobby_color_b"));
			if(config.getBool(pre+"blobby_oscillate"))col=owner.getApp().getRenderManager().getOscillationColor();
			gui().doBlob(gui().getNextId(),Vector2(x+175,496),col,.5f);
		}
	}
	void controls()
	{
		section(128,tr("GERÄTE","DEVICES"));
		const std::vector<std::string> devices={"gamepad","keyboard","mouse","joystick"};
		const std::vector<std::string> names={tr("Controller","Controller"),tr("Tastatur","Keyboard"),tr("Maus","Mouse"),tr("Joystick","Joystick")};
		for(int i=0;i<2;++i) {
			const std::string key=inputPrefix(i)+"device",old=input.getString(key);
			choice(148+i*40,tr("Gerät Spieler ","Player device ")+std::to_string(i+1),input,key,devices,names,tr("Eingabegerät getrennt für jeden Spieler auswählen.","Select each player's input device independently."));
			if(input.getString(key)=="mouse"&&input.getString(inputPrefix(1-i)+"device")=="mouse") {input.setString(key,old);notice=tr("Die Maus kann nur einem Spieler zugewiesen werden.","Only one player can use the mouse.");}
		}
		if(row(228,tr("Belegung für","Bindings for"),tr("Spieler ","Player ")+std::to_string(inputPlayer+1),tr("Tastenbelegung des jeweiligen Spielers bearbeiten.","Edit this player's bindings.")))inputPlayer=1-inputPlayer;
		const std::string prefix=inputPrefix(inputPlayer),device=input.getString(prefix+"device");
		if(device=="gamepad")
		{
			integer(268,tr("Controller","Controller"),input,prefix+"controller_slot",0,7,inputPlayer,tr("Controller-Slot: 0 ist der erste Controller.","Controller slot: 0 is the first controller."));
			const int jumps[]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y};const char* names[]={"A","X","Y"};
			int current=input.getInteger(prefix+"controller_jump",0),j=0;for(int i=0;i<3;++i)if(jumps[i]==current)j=i;
			if(int delta=row(308,tr("Springen","Jump"),names[j],tr("B bleibt zum Zurückgehen reserviert.","B remains reserved for Back."))) {input.setInteger(prefix+"controller_jump",jumps[wrap(j+delta,3)]);dirty=true;}
			float dz=input.getInteger(prefix+"controller_deadzone",10000)/32767.f;
			if(slider(40,348,350,tr("Stick-Totzone","Stick deadzone"),dz,std::to_string(int(std::lround(dz*100)))+" %",tr("Bei Stick-Drift erhöhen. Kleinere Werte reagieren früher.","Increase for stick drift. Lower values respond sooner."),accent,.01f))input.setInteger(prefix+"controller_deadzone",std::min(30000,int(dz*32767)));
			row(396,tr("Bewegen","Move"),"Stick / D-Pad",tr("Linker Stick und Steuerkreuz bewegen den Blob.","Move using the left stick or D-pad."));
			row(436,tr("Pause","Pause"),"Start",tr("Start öffnet das Pausenmenü.","Start opens the pause menu."));
		}
		else if(device=="keyboard"||device=="joystick")
		{
			const char* actions[]={"left","right","jump"};const char* deLabels[]={"Nach links","Nach rechts","Springen"};const char* enLabels[]={"Move left","Move right","Jump"};
			for(int j=0;j<3;++j){auto key=prefix+device+"_"+actions[j];std::string value=input.getString(key);if(value.size()>26)value=value.substr(0,25)+"...";
				if(row(268+j*40,tr(deLabels[j],enLabels[j]),value,tr("Auswählen, dann die gewünschte Taste oder Achse betätigen.","Select, then press the desired key, button or axis."))) captureInput(device=="keyboard"?KEYBOARD:JOYSTICK,key);}
		}
		else if(device=="mouse")
		{
			if(row(268,tr("Springen","Jump"),tr("Taste ","Button ")+std::to_string(input.getInteger(prefix+"mouse_jumpbutton",1)),tr("Auswählen, dann die Maustaste drücken.","Select, then press a mouse button.")))captureInput(MOUSE,prefix+"mouse_jumpbutton");
			float sensitivity=input.getFloat(prefix+"mouse_sensitivity",1);
			if(slider(40,308,350,tr("Empfindlichkeit","Sensitivity"),sensitivity,std::to_string(int(std::lround(sensitivity*100)))+" %",tr("Empfindlichkeit der Maussteuerung.","Mouse movement sensitivity.")))input.setFloat(prefix+"mouse_sensitivity",sensitivity);
		}
		else row(268,tr("Touch-Steuerung","Touch controls"),tr("Unverändert","Unchanged"),tr("Bestehende Touch-Konfiguration bleibt erhalten.","Existing touch configuration is preserved."));
	}
	void graphics()
	{
		section(128,tr("ANZEIGE","DISPLAY"));
		choice(148,tr("Anzeigemodus","Display mode"),config,"fullscreen",{"false","true"},{tr("Fenster","Window"),tr("Vollbild","Fullscreen")},tr("Fullscreen nutzt den Desktopmodus. Fenster behalten ihre Titelleiste.","Fullscreen uses desktop mode. Windowed mode keeps its title bar."));
		const auto& display=owner.getApp().displaySettings();
		std::vector<int> caps{0};caps.insert(caps.end(),display.frameLimits.begin(),display.frameLimits.end());
		int fps=config.getInteger("render_fps",0);auto cap=std::find(caps.begin(),caps.end(),fps);int idx=cap==caps.end()?0:int(cap-caps.begin());
		if(int delta=row(188,tr("Bildrate","Frame rate"),fps==0?tr("Automatisch","Automatic"):std::to_string(fps)+" FPS",tr("Automatisch folgt dem Bildschirm. Das Spieltempo bleibt unabhängig.","Automatic follows the monitor. Game speed stays independent."))) {config.setInteger("render_fps",caps[wrap(idx+delta,int(caps.size()))]);dirty=true;}
		if(!config.getBool("fullscreen"))
		{
			const auto& sizes=display.windowSizes;int w=config.getInteger("window_width",800),h=config.getInteger("window_height",600);int index=0;
			for(size_t i=0;i<sizes.size();++i)if(sizes[i].width==w&&sizes[i].height==h)index=int(i);
			if(int delta=row(228,tr("Fenstergröße","Window size"),std::to_string(w)+" x "+std::to_string(h),tr("Nur Größen, die auf den aktuellen Bildschirm passen.","Only sizes that fit the current monitor.")))if(!sizes.empty()){auto size=sizes[wrap(index+delta,int(sizes.size()))];config.setInteger("window_width",size.width);config.setInteger("window_height",size.height);dirty=true;}
		}
		else {row(228,tr("Auflösung","Resolution"),std::to_string(display.desktopWidth)+" x "+std::to_string(display.desktopHeight),tr("Wird im Vollbild vom Desktop übernommen.","Fullscreen uses the desktop resolution."));}
		section(272,tr("DARSTELLUNG","RENDERING"));
		toggle(292,tr("Schatten","Shadows"),config,"show_shadow",tr("Schatten von Ball und Blobs anzeigen.","Display ball and blob shadows."));
        const std::map<std::string,std::string> backgroundTitles={
            {"aurora.bmp","Aurora"},{"beach.bmp","Rocky beach"},{"beacharena.bmp","Beach arena"},
            {"berge.bmp","Mountain coast"},{"berge2.bmp","Alpine lake"},{"burg.bmp","Castle"},
            {"landhaus.bmp","Country house"},{"mars.bmp","Mars"},{"mars2.bmp","Mars colony"},
            {"mond.bmp","Moon"},{"oase.bmp","Oasis"},{"saturn.bmp","Saturn"},{"schnee.bmp","Snow"},
            {"sportanlage.bmp","Sports hall"},{"steppe.bmp","Meadow"},{"strand1-classic.bmp","Classic beach"},
            {"strand1.bmp","Beach 1"},{"strand2.bmp","Beach 2"},{"strand3.bmp","Beach 3"},{"street.bmp","Road"},
            {"tempel.bmp","Temple"},{"tropen.bmp","Tropical resort"},{"volleyfeld.bmp","Volleyball court"},
            {"woizn.bmp","Wheat field"},{"wolken.bmp","Clouds"}};
        std::vector<std::string> backgroundLabels;
        for(const auto& name:backgrounds){auto title=backgroundTitles.find(name);
            backgroundLabels.push_back(title==backgroundTitles.end()?name.substr(0,name.size()-4):translateUI(config.getString("language"),title->second));}
        choice(332,tr("Hintergrund","Background"),config,"background",backgrounds,backgroundLabels,tr("HD-Hintergrund für das Spielfeld auswählen.","Select an HD court background."));
#if HAVE_LIBGL
		choice(372,tr("Renderer","Renderer"),config,"device",{"OpenGL","SDL"},{"OpenGL","SDL"},tr("Der Renderer wird sofort umgestellt.","The renderer switches immediately."));
#endif
        if(row(412,tr("Spielfeld","Court"),config.getBool("court_widescreen",false)?tr("16:9 Breitbild","16:9 Widescreen"):tr("4:3 Klassik","4:3 Classic"),tr("4:3: klassischer Bildausschnitt und Spielfeld. 16:9: mehr Spielfeld und längere Laufwege, 20 % mehr Lauftempo. Netzwerk bleibt 4:3. Ab nächstem Match.","4:3: classic view and court. 16:9: wider court and longer runs, 20% faster running. Network stays 4:3. From next match."))){config.setBool("court_widescreen",!config.getBool("court_widescreen",false));if(!config.getBool("fullscreen"))config.setInteger("window_width",int(std::lround(config.getInteger("window_height",600)*(config.getBool("court_widescreen")?16.f/9.f:4.f/3.f))));dirty=true;}
	}
	void audio()
	{
		section(128,tr("LAUTSTÄRKE","VOLUME"));
		float volume=config.getFloat("global_volume",1);
		if(slider(40,148,350,tr("Gesamtlautstärke","Master volume"),volume,std::to_string(int(std::lround(volume*100)))+" %",tr("Lautstärke aller Spielgeräusche.","Volume of all game sounds.")))config.setFloat("global_volume",volume);
		toggle(196,tr("Stummschalten","Mute"),config,"mute",tr("Alle Spielgeräusche stummschalten.","Mute all game sounds."));
		if(button(40,236,180,tr("Sound testen","Test sound")))owner.playSound(SoundManager::IMPACT,volume);
	}

	void apply()
	{
		try {
		if(!config.saveFile("config.xml")||!input.saveFile("inputconfig.xml")) {notice=tr("Speichern fehlgeschlagen.","Could not save settings.");return;}
		auto& app=owner.getApp();
		bool rebuild=config.getBool("fullscreen")!=original.getBool("fullscreen")||config.getString("device")!=original.getString("device")||config.getInteger("window_width",800)!=original.getInteger("window_width",800)||config.getInteger("window_height",600)!=original.getInteger("window_height",600);
		try {if(rebuild)app.setupRenderManager(config);}catch(const std::exception& e){notice=e.what();return;}
		if(rebuild||config.getInteger("render_fps",0)!=original.getInteger("render_fps",0))
			app.setFrameLimit(config.getInteger("render_fps",0));
		app.getRenderManager().setCourtWidth(config.getBool("court_widescreen",false)?3200.f/3.f:800.f);
		app.getRenderManager().showShadow(config.getBool("show_shadow"));
		app.getRenderManager().getBlood().enable(config.getBool("blood"));
		if(!rebuild&&config.getString("background")!=original.getString("background"))
			if(!app.getRenderManager().setBackground("backgrounds/"+config.getString("background","strand1.bmp")))
				throw std::runtime_error("Could not load selected background");
		app.getSoundManager().setVolume(config.getFloat("global_volume",1));app.getSoundManager().setMute(config.getBool("mute"));
		if(auto* speed=SpeedController::getMainInstance())speed->setDrawFPS(config.getBool("showfps"));
		if(config.getString("language")!=original.getString("language"))gui().setTextMgr(config.getString("language","en"));
		original=config;dirty=false;
		} catch(const std::exception& e) { notice=tr("Änderung fehlgeschlagen: ","Could not update: ")+e.what(); }
	}
	void back() { if(dirty&&capture==NONE)apply();owner.switchState(new MainMenuState()); }
	void resetDefaults()
	{
		try {
			UserConfig defaults, defaultInput;
			if(!defaults.loadFile("default-config.xml")||!defaultInput.loadFile("default-inputconfig.xml"))
				throw std::runtime_error("Default settings are unavailable");
			config=defaults; input=defaultInput; dirty=true;
			confirmReset=false; dragging=-1;
			notice=tr("Standardwerte wiederhergestellt.","Default settings restored.");
		} catch(const std::exception& e){notice=e.what();confirmReset=false;}
	}
	void captureDialog()
	{
		gui().doOverlay(gui().getNextId(),Vector2(0,0),Vector2(800,600),Color(0,0,0),.6);
		panel(145,205,510,175);
		text(165,220,capture==NAME?tr("Namen bearbeiten","Edit name"):tr("Neue Belegung","New binding"),28,2);
		if(capture==NAME)
		{
			text(165,270,config.getString(captureKey)+"_",26);
			text(165,326,tr("Enter: bestätigen     Esc: abbrechen","Enter: confirm     Esc: cancel"),20);
		}
		else {text(165,272,tr("Taste, Maustaste oder Joystick betätigen.","Press a key, mouse button or joystick control."));text(165,326,tr("Esc / B: abbrechen","Esc / B: cancel"),20);}
	}
	std::string nameBefore;
	void processCapture()
	{
		if(capture==NONE)return;
		if(SDL_GetTicks()<captureAfter)return;
		if(in().exit()) {if(capture==NAME){config.setString(captureKey,nameBefore);SDL_StopTextInput();}capture=NONE;return;}
		if(capture==NAME)
		{
			std::string value=config.getString(captureKey),key=in().getLastTextKey();
			if(key=="return"){capture=NONE;dirty|=value!=nameBefore;SDL_StopTextInput();}
			else if(key=="backspace") {if(!value.empty()){size_t i=value.size()-1;while(i>0&&(static_cast<unsigned char>(value[i])&0xc0)==0x80)--i;value.erase(i);}}
			else if(key=="paste") {char* clip=SDL_GetClipboardText();if(clip){value+=clip;SDL_free(clip);}}
			else if(key!="del"&&!key.empty()&&key.size()<16)value+=key;
			size_t boundary=0;int count=0;while(boundary<value.size()&&count<15){++boundary;while(boundary<value.size()&&(static_cast<unsigned char>(value[boundary])&0xc0)==0x80)++boundary;++count;}value.resize(boundary);
			config.setString(captureKey,value);
		}
		else if(capture==KEYBOARD&&!in().getLastActionKey().empty()) {input.setString(captureKey,in().getLastActionKey());capture=NONE;dirty=true;}
		else if(capture==JOYSTICK&&!in().getLastJoyAction().empty()) {input.setString(captureKey,in().getLastJoyAction());capture=NONE;dirty=true;}
		else if(capture==MOUSE&&in().getLastMouseButton()>0) {input.setInteger(captureKey,in().getLastMouseButton());capture=NONE;dirty=true;}
	}
	void step(bool interactive=true)
	{
		mouse=in().position();mouseMoved=mouse!=lastMouse;lastMouse=mouse;
		if(in().unclick())dragging=-1;
		bool hadCapture=capture!=NONE;
		bool hadReset=confirmReset;
		suppressActions=!interactive||hadCapture||hadReset;
		if(interactive&&hadCapture)processCapture();
		else if(interactive&&!confirmReset)
		{
			if(in().previousTab())category(tab-1);else if(in().nextTab())category(tab+1);
			if(in().up())focus=wrap(focus-1,previousCount);if(in().down())focus=wrap(focus+1,previousCount);
			
			if(in().exit())back();
		}
		rowCount=0;helpTitle="";helpText="";
		gui().doCursor();gui().doImage(gui().getNextId(),Vector2(400,300),"menu-backgrounds/"+config.getString("background","strand1.bmp"),Vector2(800,600));
		// Use exactly the main menu's default black overlay and opacity.
		gui().doOverlay(gui().getNextId(),Vector2(0,0),Vector2(800,600));
		text(40,28,tr("Optionen","Options"),36);
		gui().doImage(gui().getNextId(),Vector2(700,43),"gfx/titel.bmp",Vector2(120,69));
		const char* german[]={"Spiel","Spieler","Steuerung","Grafik","Audio"};const char* english[]={"Game","Players","Controls","Graphics","Audio"};
		float x=40;
		for(int i=0;i<5;++i){std::string label=tr(german[i],english[i]);float w=RenderManager::menuTextWidth(label,22)+24;
			if(!suppressActions&&capture==NONE&&!confirmReset&&hit(x,87,w,34)&&in().click())category(i);
			panel(x,87,w,34,i==tab?accent:surface);text(x+12,93,label,22,i==tab?1:0);x+=w+6;}
		if(tab==0)game();else if(tab==1)players();else if(tab==2)controls();else if(tab==3)graphics();else audio();
		if(tab!=1&&!helpText.empty())
		{
			panel(410,140,350,210);text(425,152,helpTitle,27,2);
			std::istringstream words(helpText);std::string word,line;float y=188;
			while(words>>word){if(RenderManager::menuTextWidth(line+" "+word,22)>320&&!line.empty()){text(425,y,line);line=word;y+=20;}else {if(!line.empty())line+=" ";line+=word;}}if(!line.empty())text(425,y,line);
		}
		text(40,546,tr("Steuerkreuz: Auswahl / Wert    A: Ändern","D-pad: select / change    A: edit"),17);
		if(button(655,530,105,"B  "+tr("Zurück","Back")))back();
		if(button(463,530,182,tr("Standardwerte","Restore defaults")))confirmReset=true;
		text(40,578,tr("LB / RB: Kategorie    Bild auf / ab: Kategorie    Änderungen gelten sofort","LB / RB: category    Page Up / Down: category    Changes apply immediately"),16);
		if(!notice.empty())text(40,516,notice,17,2);
		previousCount=rowCount;
		if(capture!=NONE)captureDialog();
		if(confirmReset)
		{
			gui().doOverlay(gui().getNextId(),Vector2(0,0),Vector2(800,600),Color(0,0,0),.65);
			panel(145,215,510,140);text(165,231,tr("Standardwerte wiederherstellen?","Restore default settings?"),28,2);
			text(165,271,tr("A / Enter: zurücksetzen     B / Esc: abbrechen","A / Enter: reset     B / Esc: cancel"),21);
			if(hadReset&&(in().select()||(in().click()&&hit(165,304,210,34))))resetDefaults();
			if(hadReset&&(in().exit()||(in().click()&&hit(395,304,230,34))))confirmReset=false;
			panel(165,304,210,34);text(180,310,tr("Zurücksetzen","Reset"));panel(395,304,230,34);text(410,310,tr("Abbrechen","Cancel"));
		}
		if(dirty&&capture==NONE&&!confirmReset)apply();
	}
};

OptionState::OptionState() = default;
OptionState::~OptionState() = default;
void OptionState::init()
{
	mMenu.reset(new Menu(*this));
	// State switches clear the old UI queue. Populate the new one immediately,
	// without handling the click/A/Enter that opened the options a second time.
	mMenu->step(false);
}
void OptionState::step_impl() { mMenu->step(); }
const char* OptionState::getStateName() const { return "OptionState"; }
