#include "server/DedicatedServer.h"
#include "server/Tournament.h"
#include "TournamentProtocol.h"
#include "FileSystem.h"
#include "raknet/RakClient.h"
#include <SDL.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace tournament;
void testTournamentCapacity()
{
    const int port = 23462, count = 32;
    auto& fs = FileSystem::getSingleton();
    fs.deleteFile("Tournaments/server-23462-a.xml");
    fs.deleteFile("Tournaments/server-23462-b.xml");
    ServerInfo info("Capacity test");
    info.port = port;
    DedicatedServer server(info, {"default.lua"}, {75}, 32, false, "test");
    std::vector<std::unique_ptr<RakClient>> clients;
    std::vector<bool> entered(count, false), playing(count, false);
    std::vector<int> own(count, -1);
    std::vector<int> updates(count, 0);
    Tournament cup;
    for (int i = 0; i < count; ++i)
    {
        clients.emplace_back(new RakClient);
        if (!clients.back()->Connect("127.0.0.1", port, 0, 0, 1))
        {
            throw std::runtime_error("32-client connect failed");
        }
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
        for (int i = 0; i < count; ++i)
        {
            packet_ptr p;
            while ((p = clients[i]->Receive()))
            {
                if (p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
                {
                    RakNet::BitStream out;
                    makeEnterServerPacket(out, PlayerIdentity("P" + std::to_string(i), Color(70, 130, 210),
                                                              false, LEFT_PLAYER));
                    clients[i]->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                    send(i, Command::Hello);
                }
                else if (p->data[0] == ID_TOURNAMENT)
                {
                    RakNet::BitStream in(p->data, p->length, false);
                    Command c;
                    auto body = readPacket(in, c);
                    if (c == Command::Welcome)
                    {
                        entered[i] = true;
                        own[i] = std::stoi(body.substr(0, body.find(':')));
                    }
                    else if (c == Command::State && i == 0)
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
                    clients[i]->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                }
                else if (p->data[0] == ID_GAME_READY)
                {
                    playing[i] = true;
                }
                else if (p->data[0] == ID_GAME_UPDATE)
                {
                    ++updates[i];
                }
            }
        }
        SDL_Delay(2);
    };
    auto until = [&](const std::function<bool()>& condition, const char* error)
    {
        const auto end = SDL_GetTicks() + 12000;
        while (!condition() && !SDL_TICKS_PASSED(SDL_GetTicks(), end))
        {
            pump();
        }
        if (!condition())
        {
            throw std::runtime_error(error);
        }
    };
    until(
        [&]()
        {
            return std::all_of(entered.begin(), entered.end(), [](bool x) { return x; }) &&
                   cup.people.size() == 32;
        },
        "32 people did not register");
    if (server.getConnectedClients() != 32)
    {
        throw std::runtime_error("Wrong connected person count");
    }
    RakClient excess;
    excess.Connect("127.0.0.1", port, 0, 0, 1);
    bool refused = false;
    until(
        [&]()
        {
            packet_ptr p;
            while ((p = excess.Receive()))
            {
                refused |= p->data[0] == ID_NO_FREE_INCOMING_CONNECTIONS;
            }
            return refused;
        },
        "33rd person was not refused");
    excess.Disconnect(0);
    send(0, Command::Claim, "test");
    until([&]() { return cup.organizer == own[0]; }, "Organizer claim failed");
    auto settings = cup;
    settings.settings.capacity = 32;
    settings.settings.parallel = 16;
    settings.settings.points = 2;
    send(0, Command::Configure, settings.xml());
    until([&]() { return cup.settings.capacity == 32 && cup.settings.parallel == 16; },
          "32-person configuration failed");
    for (int i = 0; i < count; ++i)
    {
        send(i, Command::Enroll, "1");
        send(i, Command::Ready, "1");
    }
    until(
        [&]()
        {
            return std::all_of(cup.people.begin(), cup.people.end(),
                               [](const Person& p) { return p.entered && p.ready; });
        },
        "32 entrants were not ready");
    // The first connecting client is not guaranteed to own ID zero; use the organizer connection.
    // All loopback clients are equivalent for the test; claim explicitly to avoid connection-order
    // assumptions.
    send(0, Command::Start);
    until([&]() { return std::all_of(playing.begin(), playing.end(), [](bool x) { return x; }); },
          "16 parallel matches did not start");
    if (server.getActiveGamesCount() != 16)
    {
        throw std::runtime_error("Parallel game count was not 16");
    }
    std::fill(updates.begin(), updates.end(), 0);
    const auto end = SDL_GetTicks() + 2000;
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), end))
    {
        pump();
    }
    const int minimum = *std::min_element(updates.begin(), updates.end());
    if (minimum < 25)
    {
        throw std::runtime_error("Parallel simulation/transport stalled under load");
    }
    for (auto& client : clients)
    {
        client->Disconnect(25);
    }
    for (int i = 0; i < 20; ++i)
    {
        pump();
    }
    std::cout << "Capacity: 32 real clients, 33rd refused, 16 parallel matches, minimum " << minimum
              << " updates/client in two seconds passed\n";
}
