#include "server/DedicatedServer.h"
#include "server/Tournament.h"
#include "TournamentProtocol.h"
#include "DuelMatchState.h"
#include "FileSystem.h"
#include "FileRead.h"
#include "FileWrite.h"
#include "GenericIO.h"
#include "raknet/RakClient.h"
#include <SDL.h>
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace tournament;
namespace
{
void verify(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
std::string networkScenario(bool hosted)
{
    const unsigned short port = hosted ? 23461 : 23460;
    auto& fs = FileSystem::getSingleton();
    const auto save = "Tournaments/server-" + std::to_string(port);
    fs.deleteFile(save + "-a.xml");
    fs.deleteFile(save + "-b.xml");
    ServerInfo info("Tournament test");
    info.port = port;
    DedicatedServer server(info, {"default.lua"}, {75}, 3, hosted, "test-organizer");
    std::array<std::unique_ptr<RakClient>, 3> clients;
    std::array<int, 3> own{{-1, -1, -1}}, errors{{0, 0, 0}};
    std::array<bool, 3> connected{{false, false, false}}, playing{{false, false, false}};
    std::array<std::string, 3> tokens;
    std::array<Tournament, 3> cups;
    int live = 0;
    bool livePaused = true;
    for (auto& client : clients)
    {
        client.reset(new RakClient);
    }
    auto send = [&](int i, Command c, const std::string& payload = "")
    {
        RakNet::BitStream out;
        writePacket(out, c, payload);
        clients[i]->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
    };
    auto pump = [&]()
    {
        server.processPackets();
        server.updateGames();
        for (int i = 0; i < 3; ++i)
        {
            packet_ptr p;
            while ((p = clients[i]->Receive()))
            {
                if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                {
                    connected[i] = true;
                    RakNet::BitStream out;
                    makeEnterServerPacket(out,
                                          PlayerIdentity("Player " + std::to_string(i),
                                                         Color(40 + i * 50, 120, 200), false, LEFT_PLAYER));
                    clients[i]->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                    send(i, Command::Hello, tokens[i]);
                }
                else if (p->data[0] == ID_TOURNAMENT)
                {
                    RakNet::BitStream in(p->data, p->length, false);
                    if (p->length >= 2 && p->data[1] == unsigned(Command::Live))
                    {
                        in.IgnoreBytes(2);
                        int match;
                        DuelMatchState state;
                        int timeMs;
                        verify(in.Read(match) && in.Read(livePaused) && in.Read(timeMs), "Bad live header");
                        createGenericReader(&in)->generic<DuelMatchState>(state);
                        ++live;
                    }
                    else
                    {
                        Command c;
                        auto body = readPacket(in, c);
                        if (c == Command::Welcome)
                        {
                            auto split = body.find(':');
                            own[i] = std::stoi(body.substr(0, split));
                            tokens[i] = body.substr(split + 1);
                        }
                        else if (c == Command::State)
                        {
                            cups[i] = Tournament::parse(body);
                        }
                        else if (c == Command::Error)
                        {
                            ++errors[i];
                        }
                    }
                }
                else if (p->data[0] == ID_RULES_CHECKSUM)
                {
                    RakNet::BitStream out;
                    out.Write((unsigned char)ID_RULES);
                    out.Write(true);
                    clients[i]->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                }
                else if (p->data[0] == ID_GAME_READY)
                {
                    playing[i] = true;
                }
            }
        }
        SDL_Delay(2);
    };
    auto until = [&](const std::function<bool()>& condition, const char* error)
    {
        const auto deadline = SDL_GetTicks() + 7000;
        while (!condition() && !SDL_TICKS_PASSED(SDL_GetTicks(), deadline))
        {
            pump();
        }
        verify(condition(), error);
    };
    for (int i = 0; i < 3; ++i)
    {
        verify(clients[i]->Connect("127.0.0.1", port, 0, 0, 1), "Cannot connect entrant");
        until([&]() { return own[i] >= 0; }, "Tournament hello timed out");
    }
    until([&]() { return cups[0].people.size() == 3; }, "Roster not broadcast");
    verify(cups[0].organizer == own[0], "Loopback organizer not assigned");
    const int before = errors[2];
    send(2, Command::Start);
    until([&]() { return errors[2] > before; }, "Non-organizer could start tournament");
    auto settings = cups[0];
    settings.settings.capacity = 2;
    settings.settings.points = 2;
    settings.settings.parallel = 1;
    send(0, Command::Configure, settings.xml());
    until([&]() { return cups[0].settings.capacity == 2; }, "Configuration not applied");
    send(0, Command::Enroll, "1");
    send(1, Command::Enroll, "1");
    until([&]() { return cups[0].person(own[0]).entered && cups[0].person(own[1]).entered; },
          "Enrollment failed");
    send(0, Command::Ready, "1");
    send(1, Command::Ready, "1");
    until([&]() { return cups[0].person(own[0]).ready && cups[0].person(own[1]).ready; }, "Readiness failed");
    send(0, Command::Start);
    until([&]() { return playing[0] && playing[1]; }, "Tournament did not launch real network match");
    verify(!playing[2], "Spectator started as player");
    until([&]() { return !cups[2].matches.empty() && cups[2].matches[0].status == MatchStatus::Playing; },
          "Live match absent from spectator overview");
    send(2, Command::Watch, "0");
    until([&]() { return live >= 3; }, "Spectator did not receive live state");
    verify(!livePaused, "Live match unexpectedly paused");
    RakNet::BitStream pause;
    pause.Write((unsigned char)ID_PAUSE);
    clients[2]->Send(&pause, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
    int frames = live;
    until([&]() { return live >= frames + 3; }, "Spectator stream stopped");
    verify(!livePaused, "Spectator paused a match");
    RakClient extra;
    verify(extra.Connect("127.0.0.1", port, 0, 0, 1), "Cannot probe capacity");
    bool refused = false;
    until(
        [&]()
        {
            packet_ptr p;
            while ((p = extra.Receive()))
            {
                refused |= p->data[0] == ID_NO_FREE_INCOMING_CONNECTIONS;
            }
            return refused;
        },
        "Fourth client exceeded three-person limit");
    extra.Disconnect(0);
    const int entrant = own[1];
    clients[1]->Disconnect(50);
    until(
        [&]()
        {
            return cups[0].matches[0].status == MatchStatus::Interrupted &&
                   !cups[0].person(entrant).connected;
        },
        "Active disconnect did not reserve/interupt");
    RakClient newcomer;
    newcomer.Connect("127.0.0.1", port, 0, 0, 1);
    bool reserved = false;
    until(
        [&]()
        {
            packet_ptr p;
            while ((p = newcomer.Receive()))
            {
                if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                {
                    RakNet::BitStream out;
                    makeEnterServerPacket(
                        out, PlayerIdentity("Late spectator", Color(80, 90, 100), false, LEFT_PLAYER));
                    newcomer.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                    RakNet::BitStream hello;
                    writePacket(hello, Command::Hello, "");
                    newcomer.Send(&hello, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                }
                if (p->data[0] == ID_TOURNAMENT || p->data[0] == ID_DISCONNECTION_NOTIFICATION)
                {
                    reserved = true;
                }
            }
            return reserved;
        },
        "Disconnected entrant slot was not reserved");
    newcomer.Disconnect(25);
    until([&]() { return server.getConnectedClients() == 2; },
          "Rejected spectator retained reserved connection");
    verify(cups[0].people.size() == 3, "Late spectator stole a reserved participant slot");
    clients[1].reset(new RakClient);
    own[1] = -1;
    playing[0] = playing[1] = false;
    clients[1]->Connect("127.0.0.1", port, 0, 0, 1);
    until([&]() { return own[1] == entrant; }, "Active entrant identity did not reconnect");
    send(0, Command::Return);
    send(1, Command::Return);
    for (int i = 0; i < 10; ++i)
    {
        pump();
    }
    send(0, Command::Ready, "1");
    send(1, Command::Ready, "1");
    until([&]() { return playing[0] && playing[1] && cups[0].matches[0].status == MatchStatus::Playing; },
          "Checkpoint match did not resume after both players were ready");
    send(0, Command::Return);
    send(1, Command::Return);
    until([&]() { return cups[0].matches[0].status == MatchStatus::Interrupted; },
          "Leaving unfinished match was not an interruption");
    send(0, Command::Award, "0 " + std::to_string(own[0]));
    until([&]() { return cups[0].phase == Phase::Finished && cups[2].phase == Phase::Finished; },
          "Forfeit did not finish/broadcast tournament");
    verify(cups[0].matches[0].forfeit && cups[0].matches[0].winner == own[0], "Forfeit result incorrect");
    const int stable = own[1];
    clients[1]->Disconnect(50);
    connected[1] = false;
    until([&]() { return !cups[0].person(stable).connected; },
          "Disconnected participant retained live status");
    clients[1].reset(new RakClient);
    own[1] = -1;
    verify(clients[1]->Connect("127.0.0.1", port, 0, 0, 1), "Reconnect failed");
    until([&]() { return own[1] >= 0; }, "Reconnect token not accepted");
    verify(own[1] == stable, "Reconnect created duplicate participant");
    for (auto& client : clients)
    {
        client->Disconnect(50);
    }
    for (int i = 0; i < 20; ++i)
    {
        pump();
    }
    std::cout << (hosted ? "Hosted" : "Dedicated")
              << " tournament: settings, authorization, real match, spectator isolation, capacity, results "
                 "and reconnect passed\n";
    return tokens[0];
}

void recoveryScenario(const std::string& token)
{
    const std::string base = "Tournaments/server-23460";
    unsigned newest = 0;
    std::string corrupt;
    for (const char* suffix : {"-a.xml", "-b.xml"})
    {
        FileRead file(base + suffix);
        const auto bytes = file.readRawBytes(file.length());
        auto saved = Tournament::parse(std::string(bytes.begin(), bytes.end()), true);
        if (saved.revision >= newest)
        {
            newest = saved.revision;
            corrupt = base + suffix;
        }
    }
    {
        FileWrite interruptedWrite(corrupt);
        interruptedWrite.write("<truncated");
    }
    ServerInfo info("Recovery test");
    info.port = 23460;
    DedicatedServer server(info, {"default.lua"}, {75}, 3, false, "test-organizer");
    RakClient client;
    client.Connect("127.0.0.1", 23460, 0, 0, 1);
    Tournament cup;
    int own = -1;
    bool gotState = false;
    auto send = [&](Command c, const std::string& body)
    {
        RakNet::BitStream out;
        writePacket(out, c, body);
        client.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
    };
    auto pump = [&]()
    {
        server.processPackets();
        server.updateGames();
        packet_ptr p;
        while ((p = client.Receive()))
        {
            if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
            {
                RakNet::BitStream out;
                makeEnterServerPacket(out,
                                      PlayerIdentity("Restored", Color(40, 120, 200), false, LEFT_PLAYER));
                client.Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                send(Command::Hello, token);
            }
            if (p->data[0] == ID_TOURNAMENT)
            {
                RakNet::BitStream in(p->data, p->length, false);
                Command c;
                const auto body = readPacket(in, c);
                if (c == Command::Welcome)
                {
                    own = std::stoi(body.substr(0, body.find(':')));
                }
                else if (c == Command::State)
                {
                    cup = Tournament::parse(body);
                    gotState = true;
                }
                else if (c == Command::Error)
                {
                    throw std::runtime_error(body);
                }
            }
        }
        SDL_Delay(2);
    };
    auto until = [&](const std::function<bool()>& done)
    {
        auto end = SDL_GetTicks() + 6000;
        while (!done() && !SDL_TICKS_PASSED(SDL_GetTicks(), end))
        {
            pump();
        }
        verify(done(), "Server recovery timed out");
    };
    until([&]() { return own >= 0 && gotState; });
    verify(cup.phase == Phase::Finished && cup.matches.size() == 1 && cup.matches[0].forfeit,
           "Completed tournament was not recovered from backup");
    verify(cup.organizer == own, "Organizer identity did not survive restart");
    send(Command::NewCup, "");
    until([&]() { return cup.phase == Phase::Registration; });
    verify(cup.matches.empty(), "New tournament retained old matches");
    client.Disconnect(50);
    for (int i = 0; i < 10; ++i)
    {
        pump();
    }
    std::cout << "Tournament restart: truncated newest save recovered from backup, identity/results "
                 "retained, new cup passed\n";
}
} // namespace
void testTournamentLAN(const std::string& directory)
{
    auto& fs = FileSystem::getSingleton();
    fs.setWriteDir(directory);
    const auto organizer = networkScenario(false);
    recoveryScenario(organizer);
    networkScenario(true);
}
