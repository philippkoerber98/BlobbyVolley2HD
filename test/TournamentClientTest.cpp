#include "BlobbyApp.h"
#include "FileSystem.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "SpeedController.h"
#include "TournamentProtocol.h"
#include "UserConfig.h"
#include "server/DedicatedServer.h"
#include "server/Tournament.h"
#include "state/LobbyStates.h"
#include "raknet/RakClient.h"
#include <SDL.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace tournament;
void testTournamentClient(const std::string& directory)
{
    auto& fs = FileSystem::getSingleton();
    fs.setWriteDir(directory);
    fs.deleteFile("Tournaments/server-23464-a.xml");
    fs.deleteFile("Tournaments/server-23464-b.xml");
    fs.mkdir("rules");
    UserConfig input;
    input.loadFile("inputconfig.xml");
    input.setString("left_blobby_device", "keyboard");
    input.setString("right_blobby_device", "keyboard");
    input.saveFile("inputconfig.xml");
    UserConfig config;
    config.loadFile("config.xml");
    config.setString("device", "SDL");
    config.setString("language", "en");
    config.setInteger("window_width", 800);
    config.setInteger("window_height", 600);
    config.setBool("fullscreen", false);
    config.saveFile("config.xml");
    SpeedController clock(75);
    SpeedController::setMainInstance(&clock);
    ServerInfo info("Client lifecycle");
    info.port = 23464;
    SDL_strlcpy(info.hostname, "127.0.0.1", sizeof(info.hostname));
    DedicatedServer server(info, {"default.lua"}, {75}, 3, false, "test");
    BlobbyApp app(std::unique_ptr<State>(new LobbyState(info, PreviousState::LAN, true)), config);
    SDL_HideWindow(app.getWindow());
    app.getCurrentState().init();
    RakClient guest;
    guest.Connect("127.0.0.1", info.port, 0, 0, 1);
    Tournament cup;
    int own = -1;
    auto send = [&](Command c, const std::string& payload = "")
    {
        RakNet::BitStream out;
        writePacket(out, c, payload);
        guest.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
    };
    auto pump = [&]()
    {
        server.processPackets();
        server.updateGames();
        app.getInputManager().updateInput();
        app.getIMGUI().begin();
        app.step();
        app.getInputManager().clearTransientInput();
        packet_ptr p;
        while ((p = guest.Receive()))
        {
            if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
            {
                RakNet::BitStream out;
                makeEnterServerPacket(out, PlayerIdentity("Guest", Color(80, 160, 210), false, LEFT_PLAYER));
                guest.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                send(Command::Hello);
            }
            else if (p->data[0] == ID_TOURNAMENT)
            {
                RakNet::BitStream in(p->data, p->length, false);
                Command c;
                auto body = readPacket(in, c);
                if (c == Command::Welcome)
                {
                    own = std::stoi(body.substr(0, body.find(':')));
                }
                else if (c == Command::State)
                {
                    cup = Tournament::parse(body);
                }
                else if (c == Command::Error)
                {
                    throw std::runtime_error(body);
                }
            }
            else if (p->data[0] == ID_RULES_CHECKSUM)
            {
                RakNet::BitStream out;
                out.Write((unsigned char)ID_RULES);
                out.Write(true);
                guest.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
            }
        }
        SDL_Delay(4);
    };
    auto until = [&](const std::function<bool()>& ready, const char* error)
    {
        auto deadline = SDL_GetTicks() + 7000;
        while (!ready() && !SDL_TICKS_PASSED(SDL_GetTicks(), deadline))
        {
            pump();
        }
        if (!ready())
        {
            throw std::runtime_error(error);
        }
    };
    auto state = [&](const char* name) { return std::string(app.getCurrenStateName()) == name; };
    auto click = [&](int x, int y)
    {
        SDL_WarpMouseInWindow(app.getWindow(), x, y);
        SDL_Event event{};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        SDL_PushEvent(&event);
        pump();
    };
    auto back = [&]()
    {
        SDL_Event event{};
        event.type = SDL_CONTROLLERBUTTONDOWN;
        event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
        SDL_PushEvent(&event);
        pump();
    };
    until([&]() { return own >= 0 && cup.people.size() == 2 && state("TournamentState"); },
          "App did not enter tournament from lobby");
    send(Command::Claim, "test");
    until([&]() { return cup.organizer == own; }, "Client test organizer failed");
    send(Command::Enroll, "1");
    click(120, 517);
    until(
        [&]()
        {
            return std::all_of(cup.people.begin(), cup.people.end(),
                               [](const Person& p) { return p.entered; });
        },
        "App enrollment button failed");
    click(300, 517);
    send(Command::Ready, "1");
    until(
        [&]()
        {
            return std::all_of(cup.people.begin(), cup.people.end(), [](const Person& p) { return p.ready; });
        },
        "App ready button failed");
    send(Command::Start);
    until([&]() { return state("NetworkGameState"); }, "App did not enter tournament match");
    for (int i = 0; i < 30; ++i)
    {
        pump();
    }
    send(Command::Return);
    until([&]() { return cup.matches[0].status == MatchStatus::Interrupted; },
          "App match interruption failed");
    for (int i = 0; i < 10; ++i)
    {
        pump();
    }
    click(400, 327);
    until([&]() { return state("TournamentState"); }, "Interrupted match did not return to tournament");
    if (server.getConnectedClients() != 2)
    {
        throw std::runtime_error("Returning to tournament disconnected client");
    }
    for (int i = 0; i < 10; ++i)
    {
        pump();
    }
    click(120, 517);
    send(Command::Ready, "1");
    until([&]() { return state("NetworkGameState"); }, "App could not resume checkpoint match");
    for (int i = 0; i < 20; ++i)
    {
        pump();
    }
    send(Command::Return);
    until([&]() { return cup.matches[0].status == MatchStatus::Interrupted; },
          "Second app interruption failed");
    send(Command::Award, "0 " + std::to_string(cup.matches[0].a));
    until([&]() { return cup.phase == Phase::Finished; }, "App forfeit notification failed");
    for (int i = 0; i < 10; ++i)
    {
        pump();
    }
    back();
    until([&]() { return state("TournamentState"); }, "Victory did not return to tournament");
    if (server.getConnectedClients() != 2)
    {
        throw std::runtime_error("Victory disconnected tournament client");
    }
    back();
    pump();
    if (!state("TournamentState"))
    {
        throw std::runtime_error("Back bypassed tournament leave confirmation");
    }
    back();
    pump();
    if (!state("TournamentState"))
    {
        throw std::runtime_error("Canceling leave confirmation disconnected client");
    }
    guest.Disconnect(50);
    SpeedController::setMainInstance(nullptr);
    std::cout << "Tournament client: actual lobby/UI, match entry, interruption, checkpoint resume, victory "
                 "return and controller leave confirmation passed\n";
}
