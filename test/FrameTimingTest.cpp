#include "BackgroundSettings.h"
#include "FrameTiming.h"
#include "UITranslations.h"
#include "RenderInterpolation.h"
#include "DisplaySettings.h"
#include "DisplayViewport.h"
#include "AssetTransforms.h"
#include "PhysicWorld.h"
#include "InputManager.h"
#include "IMGUI.h"
#include "BlobbyApp.h"
#include "UserConfig.h"
#include "FileSystem.h"
#include "SpeedController.h"
#include "state/OptionsState.h"
#include "state/GameState.h"
#include "RenderManagerGL2D.h"
#include "DuelMatch.h"
#include "InputSource.h"
#include "ScriptedInputSource.h"
#include "GenericIO.h"
#include "FileWrite.h"
#include "raknet/BitStream.h"
#include "replays/ReplayRecorder.h"
#include "replays/ReplayPlayer.h"
#include <SDL_image.h>
#if defined(_WIN32)
#include <SDL_syswm.h>
#endif
#include <stdexcept>
#include <vector>
#include <iostream>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

std::vector<PhysicState> simulate(int renderFPS, int simulationRate)
{
    SimulationClock clock;
    PhysicWorld world;
    world.setBallPosition(Vector2(240,170));
    world.setBallVelocity(Vector2(3,-2));
    std::vector<PhysicState> recording;
    for (int frame=0; frame<renderFPS*8; ++frame)
    {
        clock.advance(1.0/renderFPS);
        while (clock.consume(simulationRate))
        {
            int tick = int(recording.size());
            world.step(PlayerInput(tick%150>=75,tick%150<75,tick%90<2),
                       PlayerInput(tick%140<70,tick%140>=70,tick%85<2),true,true);
            recording.push_back(world.getState());
        }
        // Rendering gets a copy. Interpolation must not affect the next physics step.
        DuelMatchState previous{}, current{};
        previous.worldState = world.getState(); current.worldState = previous.worldState;
        interpolateMatch(previous,current,clock.alpha(simulationRate));
    }
    require(recording.size()==size_t(simulationRate*8),"Render FPS changed the simulation tick count");
    return recording;
}
}

void testFrameTiming()
{
    require(resolveBackgroundName("removed-background.bmp")=="strand1.bmp","Removed background must recover to strand1");
    const auto classicCrop=backgroundSourceRect(3840,2160,800);
    const auto wideCrop=backgroundSourceRect(3840,2160,600.f*16/9);
    require(classicCrop.x==480 && classicCrop.y==0 && classicCrop.w==2880 && classicCrop.h==2160,
        "Classic background must trim the widescreen sides and preserve height");
    require(wideCrop.x==0 && wideCrop.y==0 && wideCrop.w==3840 && wideCrop.h==2160,
        "Wide background must show the complete widescreen image");
    auto* backdropSource=SDL_CreateRGBSurfaceWithFormat(0,1600,900,32,SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(backdropSource,nullptr,SDL_MapRGBA(backdropSource->format,120,80,40,255));
    auto* backdrop=createMenuBackdrop(backdropSource,true);
    require(backdrop && backdrop->w==533 && backdrop->h==300,"Menu blur must retain widescreen artwork");
    const auto menuClassic=backgroundSourceRect(backdrop->w,backdrop->h,800);
    require(menuClassic.w==400 && menuClassic.h==300 && menuClassic.x==66,"Menu must crop sides, not squeeze the entire blurred image");
    SDL_FreeSurface(backdrop);SDL_FreeSurface(backdropSource);
    for (int rate : {30,75,120})
    {
        const auto baseline = simulate(75,rate);
        for (int fps : {30,60,144,240})
        {
            const auto actual = simulate(fps,rate);
            for (size_t tick=0;tick<actual.size();++tick)
            {
                require(actual[tick].ballPosition == baseline[tick].ballPosition &&
                        actual[tick].ballVelocity == baseline[tick].ballVelocity &&
                        actual[tick].ballRotation == baseline[tick].ballRotation,
                        "Ball trajectory/recording changed with render FPS");
                for (int side=0;side<MAX_PLAYERS;++side)
                    require(actual[tick].blobPosition[side] == baseline[tick].blobPosition[side] &&
                            actual[tick].blobVelocity[side] == baseline[tick].blobVelocity[side],
                            "Blob movement/jump changed with render FPS");
            }
        }
    }
    SimulationClock stall;
    stall.advance(30);
    int catchup=0;
    while (stall.consume(75)) ++catchup;
    require(catchup<=19,"Window stalls cause unbounded simulation catch-up");

    DuelMatchState old{}, current{};
    old.worldState.ballPosition=Vector2(100,200);
    current.worldState.ballPosition=Vector2(120,220);
    old.worldState.ballRotation=6.2f;
    current.worldState.ballRotation=0.1f;
    old.worldState.blobPosition[0]=Vector2(100,400);
    current.worldState.blobPosition[0]=Vector2(110,390);
    const auto middle=interpolateMatch(old,current,.5f);
    require(middle.worldState.ballPosition==Vector2(110,210),"Missing ball interpolation");
    require(middle.worldState.blobPosition[0]==Vector2(105,395),"Missing blob interpolation");
    require(middle.worldState.ballRotation<.2f || middle.worldState.ballRotation>6.1f,
            "Ball rotation takes a long turn at angle wrap");
    require(current.worldState.ballPosition==Vector2(120,220),"Interpolation mutated the simulation state");
    current.logicState.leftScore=1;
    require(interpolateMatch(old,current,.5f).worldState.ballPosition==current.worldState.ballPosition,
            "A serve reset interpolates across the court");

    for (int hz : {24,59,60,75,120,144,165,240})
    {
        auto limits=supportedFrameLimits(hz);
        require(!limits.empty() && limits.back()==hz,"Missing current monitor frame limit");
        for (int limit:limits) require(limit>0 && limit<=hz,"FPS option exceeds current monitor refresh");
    }

    InputManager input(nullptr);
    SDL_Event key{}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_RETURN;
    SDL_PushEvent(&key);
    input.updateInput(false);
    input.updateInput(false);
    require(input.select(),"A render-only iteration lost a menu input edge");
    input.clearTransientInput();
    require(!input.select(),"A consumed menu input repeats in catch-up ticks");

    struct Counter : RenderManager {
        int texts=0;
        void drawText(const std::string&,Vector2,unsigned) override { ++texts; }
        void drawGame(const DuelMatchState&) override {}
    } renderer;
    IMGUI gui(&input);
    gui.begin();
    gui.doText(0,Vector2(0,0),"cached UI");
    gui.end(renderer); gui.end(renderer);
    require(renderer.texts==2,"UI disappeared between simulation ticks");
    gui.begin(); gui.end(renderer);
    require(renderer.texts==2,"UI commands leaked into the next simulation tick");
    std::cout << "Frame timing: identical trajectories and per-tick recordings at 30/60/75/144/240 render FPS; "
                 "interpolation, limits, buffered input and cached UI passed\n";
}

namespace {
void stepApp(BlobbyApp& app)
{
    app.getIMGUI().begin(); app.step(); app.getInputManager().clearTransientInput();
}

std::vector<Uint8> captureApp(BlobbyApp& app, float alpha, const std::string& filename)
{
    app.render(alpha);
    auto* renderer=SDL_GetRenderer(app.getWindow());
    int w,h;
    if (renderer) SDL_GetRendererOutputSize(renderer,&w,&h);
    else SDL_GL_GetDrawableSize(app.getWindow(),&w,&h);
    auto* image=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
    if (renderer)
        require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,image->pixels,image->pitch)==0,
                "Cannot capture deferred presentation");
#if HAVE_LIBGL
    else
    {
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,image->pixels);
        require(glGetError()==GL_NO_ERROR,"OpenGL rendering/capture reported an error");
        auto* pixels=static_cast<Uint8*>(image->pixels);
        for (int y=0;y<h/2;++y) for (int x=0;x<image->pitch;++x)
            std::swap(pixels[y*image->pitch+x],pixels[(h-1-y)*image->pitch+x]);
    }
#endif
    require(IMG_SavePNG(image,filename.c_str())==0,"Cannot save presentation capture");
    auto* bytes=static_cast<Uint8*>(image->pixels);
    std::vector<Uint8> pixels(bytes,bytes+image->pitch*image->h);
    SDL_FreeSurface(image); app.getRenderManager().refresh();
    return pixels;
}

class PresentationGame : public GameState
{
public:
    explicit PresentationGame(const std::string& rules) : GameState(new DuelMatch(false,rules,15)) {}
    void init() override
    {
		mMatch->setPlayers(PlayerIdentity("Left",Color(255,0,0),false,LEFT_PLAYER),
		                   PlayerIdentity("Right",Color(0,255,0),false,RIGHT_PLAYER));
        auto state=mMatch->getState();
        state.worldState.ballPosition=Vector2(250,200);
        state.worldState.ballVelocity=Vector2(5,-3);
        state.logicState.isGameRunning=true; state.logicState.isBallValid=true;
        mMatch->setState(state);
    }
    void step_impl() override { mMatch->step(); presentGame(); presentGameUI(); }
    const char* getStateName() const override { return "PresentationTest"; }
    DuelMatchState state() const { return mMatch->getState(); }
};
}

void testPresentation(const std::string& outputDirectory)
{
    UserConfig config; config.loadFile("config.xml");
    config.setString("device","SDL"); config.setBool("fullscreen",false);
    config.setBool("mute",true); config.setString("language","de");
    config.setInteger("window_width",800); config.setInteger("window_height",600);
    config.setInteger("render_fps",0);
    auto& fs=FileSystem::getSingleton();
    fs.setWriteDir(outputDirectory); config.saveFile("config.xml");
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SpeedController speed(75); SpeedController::setMainInstance(&speed);
    {
        BlobbyApp app(std::unique_ptr<State>(new OptionState),config);
        SDL_HideWindow(app.getWindow());
        app.getCurrentState().init();
        app.getInputManager().updateInput(); stepApp(app);
        require(app.renderFPS()==app.displaySettings().refreshRate,"Auto FPS does not follow monitor");
        captureApp(app,.5f,outputDirectory+"/graphics-window-auto.png");
        app.setFrameLimit(30);
        require(app.renderFPS()==30,"Manual FPS limit was not applied");
        captureApp(app,.5f,outputDirectory+"/graphics-window-manual.png");
        config.setBool("fullscreen",true);
        app.setupRenderManager(config);
        SDL_HideWindow(app.getWindow());
        captureApp(app,.5f,outputDirectory+"/graphics-fullscreen-options.png");
        app.setFrameLimit(999);
        require(app.renderFPS()<=app.displaySettings().refreshRate,"Effective manual FPS exceeds monitor");
		if (SDL_getenv("BLOBBY_TEST_OPENGL") && SDL_GetNumVideoDisplays()>1)
		{
			config.setBool("fullscreen",false);
			app.setupRenderManager(config);
			SDL_HideWindow(app.getWindow());
			SDL_SetWindowPosition(app.getWindow(),SDL_WINDOWPOS_CENTERED_DISPLAY(1),SDL_WINDOWPOS_CENTERED_DISPLAY(1));
			SDL_PumpEvents(); app.updateDisplaySettings(true);
			require(app.displaySettings().display==1,"Display settings stayed on the primary monitor after moving");
		}
    }
    {
        auto* game=new PresentationGame(config.getString("rules"));
        BlobbyApp app(std::unique_ptr<State>(game),config);
        SDL_HideWindow(app.getWindow());
        game->init(); stepApp(app); stepApp(app);
        const auto before=game->state();
        auto first=captureApp(app,.1f,outputDirectory+"/game-interpolation-a.png");
        auto second=captureApp(app,.9f,outputDirectory+"/game-interpolation-b.png");
        require(first!=second,"Real GameState presentation is not interpolated");
        require(game->state().worldState.ballPosition==before.worldState.ballPosition,
                "Rendering a GameState advanced its simulation");
    }
#if HAVE_LIBGL
    if (SDL_getenv("BLOBBY_TEST_OPENGL"))
    {
        config.setString("device","OpenGL");
        auto* game=new PresentationGame(config.getString("rules"));
        BlobbyApp app(std::unique_ptr<State>(game),config);
        SDL_HideWindow(app.getWindow());
        game->init(); stepApp(app); stepApp(app);
        const auto before=game->state();
        auto first=captureApp(app,.1f,outputDirectory+"/opengl-interpolation-a.png");
        auto second=captureApp(app,.9f,outputDirectory+"/opengl-interpolation-b.png");
        require(first!=second,"OpenGL interpolation frames are identical");
        require(game->state().worldState.ballPosition==before.worldState.ballPosition,
                "OpenGL rendering advanced the simulation");
        app.setFrameLimit(30); captureApp(app,.5f,outputDirectory+"/opengl-manual.png");
        app.setFrameLimit(0); captureApp(app,.5f,outputDirectory+"/opengl-auto.png");
        std::cout << "OpenGL: hardware interpolation and Auto/manual VSync paths passed\n";
    }
#endif
    SpeedController::setMainInstance(nullptr);
    fs.removeFromSearchPath(outputDirectory);
    std::cout << "Presentation: real game interpolation and graphics menu captures passed\n";
}

void testWindowDecorations(const std::string& outputDirectory)
{
    if (!SDL_getenv("BLOBBY_TEST_OPENGL")) return;
    UserConfig config; config.loadFile("config.xml");
    config.setBool("fullscreen",false); config.setBool("mute",true);
    config.setInteger("window_width",800); config.setInteger("window_height",600);
    SpeedController speed(75); SpeedController::setMainInstance(&speed);
    for (const char* backend : {"SDL", "OpenGL"})
    {
        config.setString("device",backend);
        BlobbyApp app(std::unique_ptr<State>(new OptionState),config);
        SDL_HideWindow(app.getWindow());
        require(SDL_SetWindowFullscreen(app.getWindow(),SDL_WINDOW_FULLSCREEN_DESKTOP)==0,
                "Cannot prepare fullscreen-to-window regression");
        app.setupRenderManager(config);
        SDL_HideWindow(app.getWindow());
        const Uint32 flags=SDL_GetWindowFlags(app.getWindow());
        require(!(flags & (SDL_WINDOW_BORDERLESS|SDL_WINDOW_FULLSCREEN)) && (flags & SDL_WINDOW_RESIZABLE),
                "Window lost its borders or resize controls");
        const auto display=app.displaySettings();
#if defined(_WIN32)
        SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
        require(SDL_GetWindowWMInfo(app.getWindow(),&info)==SDL_TRUE,"Cannot inspect native window controls");
        const auto style=GetWindowLongPtr(info.info.win.window,GWL_STYLE);
        require((style & WS_CAPTION)==WS_CAPTION && (style & WS_SYSMENU) &&
                (style & WS_MINIMIZEBOX) && (style & WS_MAXIMIZEBOX),
                "Native title bar/minimize/maximize/close controls are missing");
        RECT frame{}; GetWindowRect(info.info.win.window,&frame);
        require(frame.top>=display.usable.y && frame.left>=display.usable.x &&
                frame.right<=display.usable.x+display.usable.w &&
                frame.bottom<=display.usable.y+display.usable.h,
                "Restored window caption or border is outside the usable monitor area");
#endif
        std::cout << backend << ": restored window has visible caption, minimize/maximize/close controls\n";
    }
    SpeedController::setMainInstance(nullptr);
}

void testTabbedOptions(const std::string& outputDirectory)
{
    auto& fs=FileSystem::getSingleton();
    UserConfig config; config.loadFile("config.xml");
    UserConfig bindings; bindings.loadFile("inputconfig.xml");
    config.setBool("fullscreen",false); config.setString("language","de");
    config.setInteger("window_width",800); config.setInteger("window_height",600);
    config.setInteger("gamefps",75); config.setBool("blood",false); config.setBool("mute",true);
    fs.setWriteDir(outputDirectory);
    SDL_setenv("SDL_AUDIODRIVER","dummy",1); SDL_InitSubSystem(SDL_INIT_AUDIO);
    SpeedController speed(75); SpeedController::setMainInstance(&speed);
    std::vector<std::string> backends{"SDL"};
    if(SDL_getenv("BLOBBY_TEST_OPENGL")) backends.push_back("OpenGL");
    for(const auto& backend:backends)
    {
        config.setString("device",backend); config.saveFile("config.xml"); bindings.saveFile("inputconfig.xml");
        BlobbyApp app(std::unique_ptr<State>(new MainMenuState),config);
        SDL_HideWindow(app.getWindow()); app.getCurrentState().init();
        app.getInputManager().updateInput(); stepApp(app);
        SDL_WarpMouseInWindow(app.getWindow(),60,360);
        SDL_Event openOptions{};openOptions.type=SDL_MOUSEBUTTONDOWN;openOptions.button.button=SDL_BUTTON_LEFT;
        SDL_PushEvent(&openOptions);app.getInputManager().updateInput();stepApp(app);
        require(std::string(app.getCurrenStateName())=="OptionState","Main menu did not open options");
        // No extra simulation tick: the very first presentation must already have a backdrop.
        auto initial=captureApp(app,0,outputDirectory+"/menu-"+backend+"-first-transition.png");
        bool hasImage=false;for(size_t i=0;i+3<initial.size();i+=4)
            if(initial[i]||initial[i+1]||initial[i+2]){hasImage=true;break;}
        require(hasImage,"Opening options presented an empty black frame");
        SDL_WarpMouseInWindow(app.getWindow(),799,599);
        app.getInputManager().updateInput(); stepApp(app);
        auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);};
        auto shoulder=[&](Uint8 button){SDL_Event e{};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=button;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);};
        auto screenshot=[&](const std::string& tab){return captureApp(app,0,outputDirectory+"/menu-"+backend+"-"+tab+".png");};
        screenshot("spiel");
        key(SDLK_DOWN); key(SDLK_DOWN); key(SDLK_RIGHT); // 75 -> 76 fixed game steps.
        UserConfig unchanged; unchanged.loadFile("config.xml");
        require(unchanged.getInteger("gamefps")==76,"Game-speed change was not saved immediately");
        key(SDLK_PAGEDOWN); screenshot("spieler");
        auto mouseEvent=[&](int x,int y,Uint32 type){SDL_WarpMouseInWindow(app.getWindow(),x,y);SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);};
        mouseEvent(215,320,SDL_MOUSEBUTTONDOWN); mouseEvent(215,320,SDL_MOUSEBUTTONUP);
        mouseEvent(90,165,SDL_MOUSEBUTTONDOWN); mouseEvent(90,165,SDL_MOUSEBUTTONUP);
        SDL_Delay(200);
        SDL_Event typed{};typed.type=SDL_TEXTINPUT;typed.text.text[0]='Z';typed.text.text[1]=0;
        SDL_PushEvent(&typed);app.getInputManager().updateInput();stepApp(app);key(SDLK_RETURN);
        key(SDLK_PAGEDOWN); screenshot("steuerung");
        shoulder(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); screenshot("grafik");
        auto courtBefore=screenshot("background-before");
        // Background is row 4 in windowed graphics: down to it, then cycle.
        key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_RIGHT);
        stepApp(app);
        UserConfig backgroundSaved;backgroundSaved.loadFile("config.xml");
        require(backgroundSaved.getString("background")!=config.getString("background"),"Background selection was not saved immediately");
        auto courtAfter=screenshot("background-after");
        // Inspect an unobstructed region: text/control changes cannot satisfy this check.
        bool backdropChanged=false;for(int y=405;y<495;++y)for(int x=510;x<755;++x)
            if(courtBefore[(y*800+x)*4]!=courtAfter[(y*800+x)*4])backdropChanged=true;
        require(backdropChanged,"Menu backdrop did not follow the selected court");
        key(SDLK_PAGEDOWN); screenshot("audio");
        shoulder(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        key(SDLK_PAGEUP); key(SDLK_PAGEUP); key(SDLK_PAGEUP);
        UserConfig saved; saved.loadFile("config.xml");
        require(saved.getInteger("gamefps")==76,"Tab changes lost the game-speed value");
        require(saved.getInteger("left_blobby_color_r")==128,"Mouse RGB slider was not preserved across tabs");
        require(saved.getString("left_player_name").find('Z')!=std::string::npos,"Name editing did not receive text input");
        require(!saved.getBool("blood"),"An unrelated menu edit changed blood effects");
        // Mouse tab selection must produce the same screen as keyboard/shoulder navigation.
        auto before=screenshot("applied");
        SDL_WarpMouseInWindow(app.getWindow(),126,101);
        SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;
        SDL_PushEvent(&click);app.getInputManager().updateInput();stepApp(app);
        require(screenshot("mouse-tab")!=before,"Mouse tab navigation did not change the screen");
        // Restore defaults asks first; cancellation must leave all saved settings intact.
        mouseEvent(540,547,SDL_MOUSEBUTTONDOWN);mouseEvent(540,547,SDL_MOUSEBUTTONUP);
        screenshot("reset-confirmation");key(SDLK_ESCAPE);
        UserConfig notReset;notReset.loadFile("config.xml");
        require(notReset.getInteger("gamefps")==76,"Canceling defaults changed the saved settings");
        mouseEvent(540,547,SDL_MOUSEBUTTONDOWN);mouseEvent(540,547,SDL_MOUSEBUTTONUP);key(SDLK_RETURN);
        UserConfig defaults;defaults.loadFile("default-config.xml");
        UserConfig reset;reset.loadFile("config.xml");UserConfig resetInput;resetInput.loadFile("inputconfig.xml");
        require(reset.getInteger("gamefps")==defaults.getInteger("gamefps"),"Reset did not restore game speed");
        require(reset.getInteger("left_blobby_color_r")==defaults.getInteger("left_blobby_color_r"),"Reset did not restore RGB colour");
        require(reset.getString("background")==defaults.getString("background"),"Reset did not restore the court");
        require(resetInput.getInteger("right_blobby_controller_slot")==1,"Reset lost the second player's controller default");
        stepApp(app);screenshot("reset-done");key(SDLK_ESCAPE);
        require(std::string(app.getCurrenStateName())=="MainMenuState","Back still requires applying changes");
        std::cout<<backend<<": instant settings, live blurred backdrop, RGB/text input and confirmed/canceled defaults reset passed\n";
    }
    require(translateUI("en","16:9 Widescreen")=="16:9 Widescreen","German court label leaked into English");
    require(translateUI("cs","Controls")=="Ovládání","Czech UI lookup failed");
    require(translateUI("es","Graphics")=="Gráficos","Spanish UI lookup failed");
    require(translateUI("fr","Players")=="Joueurs","French UI lookup failed");
    require(translateUI("it","Game")=="Gioco","Italian UI lookup failed");
    for(const auto& backend:backends)for(const char* language:{"de","en","cs","es","fr","it"}){
        config.setString("device",backend);config.setString("language",language);config.saveFile("config.xml");
        BlobbyApp app(std::unique_ptr<State>(new OptionState),config);SDL_HideWindow(app.getWindow());app.getCurrentState().init();
        SDL_WarpMouseInWindow(app.getWindow(),799,599);app.getInputManager().updateInput();stepApp(app);
        for(int tab=0;tab<5;++tab){
            if(tab){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_PAGEDOWN;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);}
            captureApp(app,0,outputDirectory+"/language-"+backend+"-"+language+"-"+std::to_string(tab)+".png");
        }
    }
    std::cout<<"UI catalog: six languages, five tabs, both available renderers passed\n";
    SpeedController::setMainInstance(nullptr);fs.removeFromSearchPath(outputDirectory);
}

void testReplayTiming(const std::string& outputDirectory)
{
    auto& fs=FileSystem::getSingleton(); fs.setWriteDir(outputDirectory);
    UserConfig config; config.loadFile("config.xml");
    const auto rules=config.getString("rules");
    ReplayRecorder recorder;
    // Initialize through the existing wire format, including empty recording data.
    RakNet::BitStream metadata;
    auto metadataOut=createGenericWriter(&metadata);
    metadataOut->string("Left"); metadataOut->string("Right");
    metadataOut->generic<Color>(Color(255,0,0)); metadataOut->generic<Color>(Color(0,255,0));
    metadataOut->uint32(75); metadataOut->uint32(0); metadataOut->uint32(0);
    metadataOut->string("");
    metadataOut->generic<std::vector<unsigned char>>({});
    metadataOut->generic<std::vector<ReplaySavePoint>>({});
    recorder.receive(*createGenericReader(&metadata)); recorder.setGameRules(rules);
    DuelMatch recording(false,rules,15);
    for (int tick=0;tick<600;++tick)
    {
        recorder.record(recording.getState());
        recording.getInputSource(LEFT_PLAYER)->setInput(PlayerInput(tick%150>=75,tick%150<75,tick%90<2));
        recording.getInputSource(RIGHT_PLAYER)->setInput(PlayerInput(tick%140<70,tick%140>=70,tick%85<2));
        recording.step();
    }
    recorder.finalize(recording.getScore(LEFT_PLAYER),recording.getScore(RIGHT_PLAYER));
    { FileWrite file("frame-timing-replay.bvr"); recorder.save(file); }
    auto play=[&](int fps) {
        ReplayPlayer replay; replay.load("frame-timing-replay.bvr");
        require(replay.getGameSpeed()==75,"Replay game speed was changed to render FPS");
        DuelMatch match(false,rules,15);
        SimulationClock clock;
        std::vector<std::vector<unsigned char>> states;
        for (int frame=0;frame<fps*8;++frame)
        {
            clock.advance(1.0/fps);
            while (clock.consume(75))
            {
                require(replay.play(&match),"Replay ended before its scheduled simulation tick");
                // This is also the network serialization path, not interpolated graphics data.
                RakNet::BitStream wire;
                createGenericWriter(&wire)->generic<DuelMatchState>(match.getState());
                states.emplace_back(wire.GetData(),wire.GetData()+wire.GetNumberOfBytesUsed());
                DuelMatchState received{};
                createGenericReader(&wire)->generic<DuelMatchState>(received);
                require(received.worldState.ballPosition==match.getState().worldState.ballPosition,
                        "Network snapshot round-trip changed the simulation");
            }
        }
        require(replay.getReplayPosition()==600,"Render FPS changed replay playback speed");
        return states;
    };
    const auto baseline=play(75);
    for (int fps:{30,60,144})
        require(play(fps)==baseline,"Replay or serialized network states depend on render FPS");
    fs.removeFromSearchPath(outputDirectory);
    std::cout << "Replay: saved/reloaded recording advances identically at 30/60/75/144 render FPS; "
                 "serialized network snapshots match byte-for-byte\n";
}


namespace {
class DialogGame : public GameState {
public:
 int mode=0,action=0;
 explicit DialogGame(const std::string& rules):GameState(new DuelMatch(false,rules,15)){}
 void init() override {
  mMatch->setPlayers(PlayerIdentity("Spieler 1",Color(255,80,40),false,LEFT_PLAYER),PlayerIdentity("Spieler 2",Color(80,180,255),false,RIGHT_PLAYER));
  auto state=mMatch->getState();state.worldState.ballPosition=Vector2(250,220);state.logicState.leftScore=15;state.logicState.rightScore=12;state.logicState.winningPlayer=LEFT_PLAYER;mMatch->setState(state);
  mMatch->setMatchTimeMs(272000);setDefaultReplayName("Spieler 1","Spieler 2");presentGame();presentGameUI();
 }
 void step_impl() override {
  if(mode==1){displayWinningPlayerScreen(LEFT_PLAYER);action=displayVictoryActions(true);}
  if(mode==2){mSaveReplay=true;displaySaveReplayPrompt();}
  presentGameUI();
 }
 const char* getStateName() const override{return "DialogTest";}
};
}
void testGameMenus(const std::string& outputDirectory)
{
 auto& fs=FileSystem::getSingleton();fs.setWriteDir(outputDirectory);
 UserConfig config;config.loadFile("config.xml");config.setBool("fullscreen",false);config.setBool("mute",true);
 config.setInteger("window_width",800);config.setInteger("window_height",600);config.setString("language","de");
 config.setBool("left_player_human",true);config.setBool("right_player_human",true);
 SpeedController speed(75);SpeedController::setMainInstance(&speed);
 std::vector<std::string> backends{"SDL"};if(SDL_getenv("BLOBBY_TEST_OPENGL"))backends.push_back("OpenGL");
 for(const auto& backend:backends){
  config.setString("device",backend);config.saveFile("config.xml");
  {
   BlobbyApp app(std::unique_ptr<State>(new MainMenuState),config);SDL_HideWindow(app.getWindow());app.getCurrentState().init();
   SDL_WarpMouseInWindow(app.getWindow(),799,599);app.getInputManager().updateInput();stepApp(app);
   auto shot=[&](const char* name){return captureApp(app,.5f,outputDirectory+"/new-"+backend+"-"+name+".png");};
   auto key=[&](SDL_Keycode k){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);};
   auto button=[&](Uint8 b){SDL_Event e{};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=b;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);};
   shot("main-menu");key(SDLK_ESCAPE);shot("quit-app");
   require(app.getInputManager().running(),"Opening quit confirmation closed the game");
   key(SDLK_RETURN);require(app.getInputManager().running(),"Quit dialog did not default to staying in menu");
   key(SDLK_ESCAPE);button(SDL_CONTROLLER_BUTTON_B);
   button(SDL_CONTROLLER_BUTTON_A);
   require(std::string(app.getCurrenStateName())=="LocalGameState","Default main-menu selection did not start a match");
   shot("game-hud");key(SDLK_ESCAPE);app.getInputManager().updateInput();stepApp(app);
   auto pause=shot("pause");auto cached=shot("pause-cached");
   require(pause==cached,"Cached pause backdrop changes across render frames");
   button(SDL_CONTROLLER_BUTTON_B);app.getInputManager().updateInput();stepApp(app);
   require(shot("resumed")!=pause,"B did not resume the paused match");
   // Exercise mouse pause, then safe controller confirmation and Replay navigation.
   SDL_WarpMouseInWindow(app.getWindow(),748,18);SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;
   SDL_PushEvent(&click);app.getInputManager().updateInput();stepApp(app);app.getInputManager().updateInput();stepApp(app);app.getInputManager().updateInput();stepApp(app);
   shot("mouse-pause");SDL_WarpMouseInWindow(app.getWindow(),799,599);key(SDLK_DOWN);key(SDLK_DOWN);key(SDLK_RETURN);
   app.getInputManager().updateInput();stepApp(app);shot("save-replay");
   for(int i=0;i<32;++i)key(SDLK_BACKSPACE);
   const auto replayName="HD-"+backend+"-"+std::to_string(SDL_GetTicks());
   SDL_Event typed{};typed.type=SDL_TEXTINPUT;SDL_strlcpy(typed.text.text,replayName.c_str(),sizeof(typed.text.text));
   SDL_PushEvent(&typed);app.getInputManager().updateInput();stepApp(app);
   fs.mkdir("replays");key(SDLK_RETURN);
   require(fs.exists("replays/"+replayName+".bvr"),"Replay dialog did not save the edited filename");
   ReplayPlayer savedReplay;savedReplay.load("replays/"+replayName+".bvr");
   app.getInputManager().updateInput();stepApp(app);key(SDLK_ESCAPE);app.getInputManager().updateInput();stepApp(app);
   key(SDLK_ESCAPE);app.getInputManager().updateInput();stepApp(app);key(SDLK_RIGHT);key(SDLK_RETURN);
   require(std::string(app.getCurrenStateName())=="MainMenuState","Leaving match did not return to main menu");
  }
  {
   auto* fixture=new DialogGame(config.getString("rules"));BlobbyApp app(std::unique_ptr<State>(fixture),config);SDL_HideWindow(app.getWindow());fixture->init();fixture->mode=1;
   app.getInputManager().updateInput();stepApp(app);
   auto victoryFirst=captureApp(app,.5f,outputDirectory+"/new-"+backend+"-victory.png");
   SDL_Delay(80);
   auto victoryLater=captureApp(app,.5f,outputDirectory+"/new-"+backend+"-victory-confetti.png");
   require(victoryFirst!=victoryLater,"Confetti does not animate between presentation frames");
   SDL_Event e{};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=SDL_CONTROLLER_BUTTON_A;SDL_PushEvent(&e);app.getInputManager().updateInput();stepApp(app);
   require(fixture->action==1,"Victory default did not select a rematch");
  }
  std::cout<<backend<<": new main menu, safe quit/resume, mouse pause, replay dialog and victory/controller navigation passed\n";
 }
 SpeedController::setMainInstance(nullptr);fs.removeFromSearchPath(outputDirectory);
}


void testRoundedPanels(const std::string& outputDirectory)
{
 auto& fs=FileSystem::getSingleton();fs.setWriteDir(outputDirectory);
 UserConfig config;config.loadFile("config.xml");config.setBool("fullscreen",false);config.setBool("mute",true);
 config.setInteger("window_width",800);config.setInteger("window_height",600);
 SpeedController speed(75);SpeedController::setMainInstance(&speed);
 std::vector<std::string> backends{"SDL"};if(SDL_getenv("BLOBBY_TEST_OPENGL"))backends.push_back("OpenGL");
 for(const auto& backend:backends){
  config.setString("device",backend);config.saveFile("config.xml");
  BlobbyApp app(std::unique_ptr<State>(new MainMenuState),config);SDL_HideWindow(app.getWindow());
  for(int scale:{1,4}){
   SDL_SetWindowSize(app.getWindow(),800*scale,600*scale);
   app.getInputManager().updateInput();app.getRenderManager().refresh();
   app.getRenderManager().drawRoundedPanel(.75f,Vector2(20.25f,20.25f),Vector2(220.75f,90.75f),Color(255,255,255));
   const auto pixels=captureApp(app,0,outputDirectory+"/rounded-"+backend+"-"+std::to_string(scale)+"x.png");
   int w=0,h=0;if(auto* r=SDL_GetRenderer(app.getWindow()))SDL_GetRendererOutputSize(r,&w,&h);else SDL_GL_GetDrawableSize(app.getWindow(),&w,&h);
   require(w==800*scale&&h==600*scale,"Rounded-panel test did not use the requested pixel size");
   bool partial=false;
   for(int y=20*scale;y<25*scale;++y)for(int x=20*scale;x<25*scale;++x){auto value=pixels[(y*w+x)*4];if(value>4&&value<180)partial=true;}
   require(partial,"Rounded corner has no antialiased edge pixels");
   // Nine slices must meet without gaps or compounded alpha in the interior.
   for(int x=25*scale;x<216*scale;++x){auto value=pixels[(50*scale*w+x)*4];require(value>=189&&value<=193,"Rounded panel has a seam or overlapping transparency");}
  }
  std::cout<<backend<<": rounded-panel antialiasing and seamless alpha passed at 800x600 and 3200x2400\n";
 }
 SpeedController::setMainInstance(nullptr);fs.removeFromSearchPath(outputDirectory);
}


namespace {
class WideGameProbe : public GameState {
public:
 explicit WideGameProbe(const std::string& rules):GameState(new DuelMatch(false,rules,15,3200.f/3.f)){}
 void init() override {
  mMatch->setPlayers(PlayerIdentity("Spieler 1",Color(60,150,255),false,LEFT_PLAYER),PlayerIdentity("Spieler 2",Color(255,155,45),false,RIGHT_PLAYER));
  auto state=mMatch->getState();state.worldState.ballPosition=Vector2(900,190);state.worldState.ballVelocity=Vector2(5,-3);
  state.logicState.isGameRunning=true;state.logicState.isBallValid=true;mMatch->setState(state);presentGame();presentGameUI();
 }
 void step_impl() override{mMatch->step();presentGame();presentGameUI();}
 const char* getStateName() const override{return "WideGameProbe";}
};
}
void testWidescreen(const std::string& outputDirectory)
{
 auto& fs=FileSystem::getSingleton();fs.setWriteDir(outputDirectory);
 const float width=3200.f/3.f;
 // Independent courts can coexist: classic callers do not inherit a preference.
 PhysicWorld classic,wide(width,5.4f);
 classic.setBallPosition(Vector2(740,180));wide.setBallPosition(Vector2(900,180));
 classic.setBallVelocity(Vector2(6,-2));wide.setBallVelocity(Vector2(6,-2));
 for(int i=0;i<20;++i){classic.step({}, {},false,true);wide.step({}, {},false,true);}
 require(classic.getBallVelocity().x<0,"Classic wall moved");
 require(wide.getBallPosition().x>1000&&wide.getBallVelocity().x>0,"Wide ball still bounces at the old wall");
 for(int i=0;i<20;++i)wide.step({}, {},false,true);
 require(wide.getBallVelocity().x<0,"Wide outer wall does not reflect the ball");
 PhysicWorld jumpClassic,jumpWide(width,5.4f);
 for(int i=0;i<90;++i){
  PlayerInput input(false,true,i==0);jumpClassic.step(input,{},false,false);jumpWide.step(input,{},false,false);
  require(jumpClassic.getBlobPosition(LEFT_PLAYER).y==jumpWide.getBlobPosition(LEFT_PLAYER).y,"Wide mode changed jump timing or height");
 }
 require(jumpWide.getBlobPosition(LEFT_PLAYER).x>480,"Wide player cannot use the larger half");
 require(jumpWide.getBlobPosition(LEFT_PLAYER).x<width/2-35,"Wide player can cross the net");
 // From the neutral position, both a short drop by the net and an outer drop
 // must remain catchable in both modes without needing a faster ball or jump.
 for(float field:{800.f,width})for(bool outside:{false,true}){
  PhysicWorld world(field,field>801?5.4f:4.5f);bool caught=false;
  world.setEventCallback([&](MatchEvent e){if(e.event==MatchEvent::BALL_HIT_BLOB&&e.side==RIGHT_PLAYER)caught=true;});
  const float target=outside?field-55:field/2+55;
  world.setBallPosition(Vector2(target,100));world.setBallVelocity(Vector2(0,0));
  for(int i=0;i<75&&!caught;++i){const float x=world.getBlobPosition(RIGHT_PLAYER).x;world.step({},PlayerInput(x>target+2,x<target-2,false),true,true);}
  require(caught,"A representative net/outer drop is unreachable from the neutral position");
 }
 UserConfig config;config.loadFile("config.xml");const auto rules=config.getString("rules");
 // Exercise existing, unmodified legacy bots on each side of the wider court.
 DuelMatch botMatch(false,rules,15,width);
 auto left=std::make_shared<ScriptedInputSource>("scripts/"+config.getString("left_script_name"),LEFT_PLAYER,0,&botMatch);
 auto right=std::make_shared<ScriptedInputSource>("scripts/"+config.getString("right_script_name"),RIGHT_PLAYER,0,&botMatch);
 left->setWaitTime(0);right->setWaitTime(0);botMatch.setInputSources(left,right);
 for(int i=0;i<1500;++i){botMatch.step();require(std::isfinite(botMatch.getBallPosition().x),"Bot produced invalid wide state");}
 // Replay metadata selects the correct geometry regardless of the current setting.
 ReplayRecorder recorder;RakNet::BitStream metadata;auto out=createGenericWriter(&metadata);
 out->string("Wide Left");out->string("Wide Right");out->generic<Color>(Color(60,150,255));out->generic<Color>(Color(255,155,45));
 out->uint32(75);out->uint32(0);out->uint32(0);out->string("");out->generic<std::vector<unsigned char>>({});out->generic<std::vector<ReplaySavePoint>>({});
 recorder.receive(*createGenericReader(&metadata));recorder.setGameRules(rules);recorder.setCourtWidth(width);
 DuelMatch recording(false,rules,15,width);
 for(int i=0;i<300;++i){recorder.record(recording.getState());recording.step();}
 recorder.finalize(recording.getScore(LEFT_PLAYER),recording.getScore(RIGHT_PLAYER));
 {FileWrite file("wide-test.bvr");recorder.save(file);}
 ReplayPlayer replay;replay.load("wide-test.bvr");require(replay.getCourtWidth()==width,"Wide replay lost its geometry");
 DuelMatch playback(false,rules,15);replay.play(&playback);require(playback.getCourtWidth()==width,"Replay uses the menu's geometry");
 while(!replay.gotoPlayingPosition(200,&playback)){}
 require(playback.getCourtWidth()==width,"Seeking changed replay geometry");
 SpeedController speed(75);SpeedController::setMainInstance(&speed);
 std::vector<std::string> backends{"SDL"};if(SDL_getenv("BLOBBY_TEST_OPENGL"))backends.push_back("OpenGL");
 for(const auto& backend:backends){
  config.setString("device",backend);config.setBool("fullscreen",false);config.setBool("mute",true);
  config.setBool("court_widescreen",false);config.setInteger("window_width",1280);config.setInteger("window_height",720);config.saveFile("config.xml");
  {
   BlobbyApp menu(std::unique_ptr<State>(new OptionState),config);SDL_HideWindow(menu.getWindow());menu.getCurrentState().init();
   auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;SDL_PushEvent(&e);menu.getInputManager().updateInput();stepApp(menu);};
   key(SDLK_PAGEDOWN);key(SDLK_PAGEDOWN);key(SDLK_PAGEDOWN);
   int ww=0,hh=0;SDL_GetWindowSize(menu.getWindow(),&ww,&hh);auto v=fitGameViewport(ww,hh);
   SDL_WarpMouseInWindow(menu.getWindow(),v.x+int(200.f*v.w/800),v.y+int(440.f*v.h/600));
   SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;SDL_PushEvent(&click);menu.getInputManager().updateInput();stepApp(menu);
   UserConfig saved;saved.loadFile("config.xml");require(saved.getBool("court_widescreen"),"Graphics court selector was not saved immediately");
   require(menu.getRenderManager().courtWidth()==width,"Graphics court selector did not update the viewport");
   captureApp(menu,0,outputDirectory+"/wide-"+backend+"-options.png");
   key(SDLK_ESCAPE);key(SDLK_RETURN);
   require(std::string(menu.getCurrenStateName())=="LocalGameState","Starting after court selection did not enter a local match");
   require(menu.getRenderManager().courtWidth()==width,"Local match ignored the selected wide court");
   captureApp(menu,0,outputDirectory+"/wide-"+backend+"-local.png");
  }
  config.setBool("court_widescreen",true);config.saveFile("config.xml");
  auto* game=new WideGameProbe(rules);BlobbyApp app(std::unique_ptr<State>(game),config);SDL_HideWindow(app.getWindow());game->init();stepApp(app);stepApp(app);
  require(app.getRenderManager().courtWidth()==width,"Renderer did not select the wide court");
  captureApp(app,.5f,outputDirectory+"/wide-"+backend+"-game.png");
  SDL_WarpMouseInWindow(app.getWindow(),640,360);auto mouse=app.getInputManager().position();
  require(std::abs(mouse.x-400)<3&&std::abs(mouse.y-300)<3,"Wide mouse coordinates are not centered on the menu");
  std::cout<<backend<<": wide court, sprite presentation and centered mouse mapping passed\n";
 }
 config.setBool("court_widescreen",false);config.setInteger("window_width",800);config.setInteger("window_height",600);config.saveFile("config.xml");
 SpeedController::setMainInstance(nullptr);fs.removeFromSearchPath(outputDirectory);
 std::cout<<"Widescreen: classic/wide walls, unchanged jumps, larger movement range, bots, replay mode and seeking passed\n";
}


void testBotModes(const std::string& outputDirectory)
{
 auto& fs=FileSystem::getSingleton();fs.setWriteDir(outputDirectory);
 UserConfig config;config.loadFile("config.xml");const auto rules=config.getString("rules");
 const std::vector<std::string> bots={"axji-0-2","com_11","gintonicV9","hyp014","reduced","Union"};
 for(const auto& name:bots){
  // Both server sides, both geometries and non-grid starting positions.
  for(float width:{800.f,3200.f/3.f})for(auto side:{LEFT_PLAYER,RIGHT_PLAYER}){
   DuelMatch match(false,rules,99,width);
   auto bot=std::make_shared<ScriptedInputSource>("scripts/"+name,side,0,&match);bot->setWaitTime(0);
   match.setInputSources(side==LEFT_PLAYER?bot:std::make_shared<InputSource>(),side==RIGHT_PLAYER?bot:std::make_shared<InputSource>());
   auto state=match.getState();state.logicState.servingPlayer=side;
   state.worldState.ballPosition.x=side==LEFT_PLAYER?width/4:width*3/4;
   state.worldState.blobPosition[side].x=state.worldState.ballPosition.x+(side==LEFT_PLAYER?80.37f:-80.37f);
   match.setState(state);
   int tick=0;for(;tick<600&&!match.getBallActive();++tick)match.step();
   if(!match.getBallActive())throw std::runtime_error(name+" stalled serve on "+(width>801?"16:9":"4:3")+" side "+std::to_string(int(side)));
  }
  DuelMatch match(false,rules,99,3200.f/3.f);
  auto left=std::make_shared<ScriptedInputSource>("scripts/"+name,LEFT_PLAYER,0,&match);
  auto right=std::make_shared<ScriptedInputSource>("scripts/"+name,RIGHT_PLAYER,0,&match);left->setWaitTime(0);right->setWaitTime(0);match.setInputSources(left,right);
  int readyTicks=0,hits=0,wallHits=0,crossings=0;
  for(int tick=0;tick<4500;++tick){
   const bool live=match.getBallActive()&&!match.getBallDown();const float before=match.getBallPosition().x;
   match.step();
   if(live&&match.getBallActive()&&!match.getBallDown()&&((before<match.getCourtWidth()/2)!=(match.getBallPosition().x<match.getCourtWidth()/2)))++crossings;
   if(!match.getBallActive()&&!match.getBallDown())++readyTicks;else readyTicks=0;
   if(readyTicks>600)throw std::runtime_error(name+" stalled a later widescreen serve");
   for(const auto& e:match.getEvents()){if(e.event==MatchEvent::BALL_HIT_BLOB)++hits;if(e.event==MatchEvent::BALL_HIT_WALL)++wallHits;}
  }
  int points=match.getScore(LEFT_PLAYER)+match.getScore(RIGHT_PLAYER);
  if(hits<8||crossings<4)throw std::runtime_error(name+" did not produce meaningful widescreen play: hits="+std::to_string(hits)+" points="+std::to_string(points)+" crossings="+std::to_string(crossings));
  std::cout<<"Bot "<<name<<": classic/wide left+right serves passed; wide match hits="<<hits<<", points="<<points<<", walls="<<wallHits<<", net crossings="<<crossings<<"\n";
 }
 fs.removeFromSearchPath(outputDirectory);
}
