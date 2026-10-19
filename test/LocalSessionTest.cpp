#include "LocalSession.h"
#include "LevelCatalog.h"
#include "SpectatorOrder.h"
#include "BlobbyApp.h"
#include "FileSystem.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "SpeedController.h"
#include "server/DedicatedServer.h"
#include "state/LocalSetupState.h"
#include "raknet/RakClient.h"
#include <SDL_image.h>
#include <iostream>
#include <stdexcept>
namespace
{
void need(bool value, const char* error)
{
    if (!value)
    {
        throw std::runtime_error(error);
    }
}
void tick(BlobbyApp& app)
{
    app.getInputManager().updateInput();
    app.getIMGUI().begin();
    app.step();
    app.getInputManager().clearTransientInput();
}
void focusedTick(BlobbyApp& app)
{
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.windowID = SDL_GetWindowID(app.getWindow());
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    SDL_PushEvent(&focus);
    tick(app);
}
void click(BlobbyApp& app, int x, int y)
{
    SDL_WarpMouseInWindow(app.getWindow(), x, y);
    SDL_Event e{};
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    SDL_PushEvent(&e);
    tick(app);
    e.type = SDL_MOUSEBUTTONUP;
    SDL_PushEvent(&e);
    tick(app);
}
void capture(BlobbyApp& app, const std::string& path)
{
    app.render(0);
    auto* renderer = SDL_GetRenderer(app.getWindow());
    if (!renderer)
    {
        return;
    }
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, image->pixels, image->pitch);
    need(IMG_SavePNG(image, path.c_str()) == 0, "Cannot capture setup");
    SDL_FreeSurface(image);
}
} // namespace
void testLocalSession(const std::string& directory)
{
    auto& fs = FileSystem::getSingleton();
    fs.setWriteDir(directory);
    // Pure competition progression with shared local identities and no sockets.
    for (auto format :
         {tournament::Format::Knockout, tournament::Format::RoundRobin, tournament::Format::Groups})
    {
        for (int count = 2; count <= 32; ++count)
        {
            if (format == tournament::Format::Groups && count < 6)
            {
                continue;
            }
            LocalSession s;
            s.tournament = true;
            s.cup.settings.format = format;
            s.players.resize(count);
            for (int i = 0; i < count; ++i)
            {
                s.players[i].name = "Entrant " + std::to_string(i);
                s.players[i].human = i < 4;
            }
            s.prepare(1234);
            // Progression is tested without disk writes; persistence is checked below.
            s.started = false;
            int played = 0;
            while (s.pending >= 0)
            {
                auto ids = s.nextPlayers();
                need(ids.first != ids.second, "Same local entrant on both seats");
                s.beginMatch();
                s.finish(LEFT_PLAYER, 15, 2);
                need(++played < 600, "Local tournament did not progress");
            }
            need(s.cup.phase == tournament::Phase::Finished && s.cup.champion() >= 0,
                 "Local cup did not finish");
        }
    }
    LocalSession cup;
    cup.tournament = true;
    cup.players.resize(6);
    for (int i = 0; i < 6; ++i)
    {
        cup.players[i].name = i ? "Entrant " + std::to_string(i) : "Bob Müller";
        cup.players[i].human = i < 4;
    }
    cup.players[0].cycling = true;
    cup.prepare(12);
    cup.started = true;
    cup.save();
    auto saved = LocalSession::resume();
    need(saved && saved->players.size() == 6 && saved->players[0].cycling,
         "Local save lost roster or cycling");
    need(saved->players[3].human && !saved->players[4].human, "Local save lost bot assignment");
    need(spectatorLeft("Bob Müller", 1, "Manfred Müller", 0) &&
             !spectatorLeft("Manfred Müller", 0, "Bob Müller", 1),
         "Spectator orientation incorrect");
    need(spectatorLeft("bob", 1, "BOB", 2) && spectatorLeft("Änne", 1, "Bob", 0), "Name ordering not stable");
    PlayerProfiles profiles;
    profiles.players[0].name = "Bob Müller";
    profiles.players[0].side = 1;
    profiles.players[0].input = 0;
    profiles.players[0].cycling = true;
    profiles.selected = 0;
    profiles.save();
    PlayerProfiles loaded;
    need(loaded.players[0].side == 1 && loaded.players[0].input == 0 && loaded.players[0].cycling,
         "Profile lost side, controls or cycling");
    loaded.publishNetwork();
    UserConfig network;
    network.loadFile("config.xml");
    need(network.getInteger("network_side") == 1 && network.getInteger("network_input") == 0 &&
             network.getString("left_player_name") == "Bob Müller" &&
             network.getString("right_player_name") == "Bob Müller",
         "Network profile was tied to a physical side");
    const auto levels = builtInLevels();
    need(!levels.empty(), "Missing built-in archive");
    for (const auto& l : levels)
    {
        need(!l.custom, "Custom level leaked into network list");
    }
    // Reject modified-catalog clients before their identity reaches matchmaking.
    ServerInfo info("Catalog test");
    info.port = 23473;
    DedicatedServer server(info, {"default.lua"}, {75}, 2, false, "test", levels.back().id);
    RakClient client;
    client.Connect("127.0.0.1", info.port, 0, 0, 1);
    bool rejected = false;
    auto start = SDL_GetTicks();
    while (SDL_GetTicks() - start < 4000 && !rejected)
    {
        server.processPackets();
        server.updateGames();
        packet_ptr p;
        while ((p = client.Receive()))
        {
            if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
            {
                RakNet::BitStream out;
                out.Write((unsigned char)ID_ENTER_SERVER);
                out.Write(int(LEFT_PLAYER));
                char name[16] = "Invalid catalog";
                out.Write(name, 16);
                out.Write(int(0x123456));
                out.Write(uint32_t(0x48444c31));
                out.Write(builtInLevelChecksum() ^ 1u);
                out.Write(false);
                out.Write((unsigned char)3);
                out.Write("Bob", 3);
                client.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
            }
            if (p->data[0] == ID_LEVEL_CATALOG)
            {
                RakNet::BitStream in(p->data, p->length, false);
                in.IgnoreBytes(1);
                bool valid = true;
                need(in.Read(valid) && !valid, "Modified catalog accepted");
                rejected = true;
            }
        }
        SDL_Delay(2);
    }
    need(rejected && server.getWaitingPlayers() == 0, "Unverified client entered matchmaking");
    client.Disconnect(20);
    std::cout << "Local tournaments 2..32: all formats, mixed entrants, saves, profiles, alphabetical "
                 "viewers and catalog rejection passed\n";
}
void testLocalSetupUI(const std::string& directory)
{
    auto& fs = FileSystem::getSingleton();
    fs.setWriteDir(directory);
    SpeedController speed(75);
    SpeedController::setMainInstance(&speed);
    for (const char* language : {"en", "de", "cs", "es", "fr", "it"})
    {
        UserConfig c;
        c.loadFile("config.xml");
        c.setString("device", "SDL");
        c.setString("language", language);
        c.setBool("fullscreen", false);
        c.setInteger("window_width", 800);
        c.setInteger("window_height", 600);
        c.saveFile("config.xml");
        auto s = std::make_shared<LocalSession>();
        s->tournament = true;
        s->players.resize(6);
        for (int i = 0; i < 6; ++i)
        {
            s->players[i].name = "Player " + std::to_string(i + 1);
            s->players[i].human = i < 4;
        }
        BlobbyApp app(std::unique_ptr<State>(new LocalSetupState(s)), c);
        SDL_HideWindow(app.getWindow());
        app.getCurrentState().init();
        tick(app);
        capture(app, directory + "/setup-" + language + "-game.png");
        click(app, 270, 102);
        capture(app, directory + "/setup-" + language + "-participants.png");
        click(app, 180, 190);
        capture(app, directory + "/setup-" + language + "-editor.png");
        click(app, 160, 190);
        SDL_Delay(200);
        SDL_Event key{};
        key.type = SDL_KEYDOWN;
        key.key.keysym.sym = SDLK_RETURN;
        SDL_PushEvent(&key);
        tick(app);
        click(app, 700, 512);
        need(s->players.size() == 6, "Editor changed participant count");
        click(app, 655, 492);
        capture(app, directory + "/setup-" + language + "-overview.png");
        need(!s->cup.matches.empty(), "Overview did not prepare bracket");
        app.switchToState(std::unique_ptr<State>(new MainMenuState));
        tick(app);
        capture(app, directory + "/setup-" + language + "-main.png");
        app.switchToState(std::unique_ptr<State>(new LocalSetupState(0)));
        tick(app);
        capture(app, directory + "/setup-" + language + "-network-profile.png");
    }
    // Actual ready-screen -> bot simulation transition, without a network server.
    UserConfig c;
    c.loadFile("config.xml");
    c.setString("language", "en");
    c.saveFile("config.xml");
    auto s = std::make_shared<LocalSession>();
    s->players[0].human = false;
    s->players[1].human = false;
    s->settings.setInteger("scoretowin", 1);
    s->prepare(5);
    s->started = true;
    {
        BlobbyApp app(std::unique_ptr<State>(new LocalSetupState(s, true)), c);
        SDL_HideWindow(app.getWindow());
        app.getCurrentState().init();
        SDL_Delay(300);
        tick(app);
        tick(app);
        need(std::string(app.getCurrenStateName()) == "LocalGameState",
             "Ready screen did not launch local match");
        for (int i = 0; i < 100; ++i)
        {
            tick(app);
        }
    }
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_NumJoysticks() == 0)
    {
        const int first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
        const int second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
        need(first >= 0 && second >= 0, "Cannot attach ready-test controllers");
        auto* left = SDL_JoystickOpen(first);
        auto* right = SDL_JoystickOpen(second);
        UserConfig bindings;
        bindings.loadFile("inputconfig.xml");
        bindings.setString("left_blobby_device", "gamepad");
        bindings.setString("right_blobby_device", "gamepad");
        bindings.setInteger("left_blobby_controller_slot", 0);
        bindings.setInteger("right_blobby_controller_slot", 1);
        bindings.setInteger("left_blobby_controller_jump", 0);
        bindings.setInteger("right_blobby_controller_jump", 0);
        bindings.saveFile("inputconfig.xml");
        auto humans = std::make_shared<LocalSession>();
        humans->players[0].human = true;
        humans->players[1].human = true;
        humans->prepare(7);
        humans->started = true;
        {
            BlobbyApp app(std::unique_ptr<State>(new LocalSetupState(humans, true)), c);
            SDL_HideWindow(app.getWindow());
            app.getCurrentState().init();
            SDL_WarpMouseInWindow(app.getWindow(), 799, 599);
            SDL_Delay(300);
            focusedTick(app);
            focusedTick(app);
            SDL_JoystickSetVirtualButton(left, 0, 1);
            SDL_JoystickSetVirtualButton(right, 0, 1);
            SDL_JoystickUpdate();
            focusedTick(app);
            focusedTick(app);
            need(std::string(app.getCurrenStateName()) == "LocalSetupState",
                 "Held ready buttons started the match early");
            capture(app, directory + "/ready-two-controllers.png");
            SDL_JoystickSetVirtualButton(left, 0, 0);
            SDL_JoystickSetVirtualButton(right, 0, 0);
            SDL_JoystickUpdate();
            focusedTick(app);
            focusedTick(app);
            need(std::string(app.getCurrenStateName()) == "LocalGameState",
                 "Ready A changed the input device instead of confirming");
        }
        bindings.setInteger("right_blobby_controller_slot", 0);
        bindings.saveFile("inputconfig.xml");
        {
            BlobbyApp app(std::unique_ptr<State>(new LocalSetupState(humans, true)), c);
            SDL_HideWindow(app.getWindow());
            app.getCurrentState().init();
            SDL_Delay(300);
            focusedTick(app);
            SDL_JoystickSetVirtualButton(left, 0, 1);
            SDL_JoystickUpdate();
            focusedTick(app);
            SDL_JoystickSetVirtualButton(left, 0, 0);
            SDL_JoystickUpdate();
            focusedTick(app);
            need(std::string(app.getCurrenStateName()) == "LocalSetupState",
                 "Duplicate controller assignment launched a match");
        }
        SDL_JoystickClose(left);
        SDL_JoystickClose(right);
        SDL_JoystickDetachVirtual(second);
        SDL_JoystickDetachVirtual(first);
        std::cout << "Ready handoff: two virtual controllers, A release and conflicting slots passed\n";
    }
    std::cout << "Local setup: all six languages, roster/editor/preview, ready screen and real bot "
                 "simulation passed\n";
    SpeedController::setMainInstance(nullptr);
}
