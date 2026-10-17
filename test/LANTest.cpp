#include "GenericIO.h"
#include "NetworkMessage.h"
#include "PlayerIdentity.h"
#include "UserConfig.h"
#include "raknet/RakClient.h"
#include "server/DedicatedServer.h"
#include <SDL.h>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool ok, const char* message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

void exerciseServer(bool hosted, int port)
{
    UserConfig config;
    config.setInteger("port", port);
    config.setString("name", "Blobby HD LAN Test");
    ServerInfo info(config);
    DedicatedServer server(info, {"default.lua"}, {75}, 8, hosted);
    std::array<RakClient, 2> clients;
    std::array<bool, 2> accepted{}, named{}, ready{};
    unsigned game = 0;
    bool hasGame = false, hasGuest = false;
    auto pump = [&]()
    {
        server.processPackets();
        server.updateGames();
        for (int i = 0; i < 2; ++i)
        {
            packet_ptr packet;
            while ((packet = clients[i].Receive()))
            {
                const auto type = packet->data[0];
                RakNet::BitStream stream(packet->data, packet->length, false);
                stream.IgnoreBytes(1);
                if (type == ID_CONNECTION_REQUEST_ACCEPTED)
                {
                    accepted[i] = true;
                    RakNet::BitStream query;
                    query.Write((unsigned char)ID_BLOBBY_SERVER_PRESENT);
                    query.Write(BLOBBY_VERSION_MAJOR);
                    query.Write(BLOBBY_VERSION_MINOR);
                    clients[i].Send(&query, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                    RakNet::BitStream enter;
                    makeEnterServerPacket(enter, PlayerIdentity(i ? "Guest" : "Host", Color(255, 50, 50),
                                                                false, i ? RIGHT_PLAYER : LEFT_PLAYER));
                    clients[i].Send(&enter, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                }
                else if (type == ID_BLOBBY_SERVER_PRESENT)
                {
                    ServerInfo found(stream, "127.0.0.1", port);
                    check(std::string(found.name) == info.name, "Server name was not advertised");
                    named[i] = true;
                }
                else if (type == ID_RULES_CHECKSUM)
                {
                    RakNet::BitStream response;
                    response.Write((unsigned char)ID_RULES);
                    response.Write(false);
                    clients[i].Send(&response, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                }
                else if (type == ID_GAME_READY)
                {
                    ready[i] = true;
                }
                else if (type == ID_LOBBY)
                {
                    auto reader = createGenericReader(&stream);
                    unsigned char subtype;
                    reader->byte(subtype);
                    if (LobbyPacketType(subtype) == LobbyPacketType::SERVER_STATUS)
                    {
                        uint32_t count;
                        std::vector<unsigned> speeds, ids;
                        std::vector<std::string> rules, authors;
                        reader->uint32(count);
                        reader->generic<std::vector<unsigned>>(speeds);
                        reader->generic<std::vector<std::string>>(rules);
                        reader->generic<std::vector<std::string>>(authors);
                        reader->generic<std::vector<unsigned>>(ids);
                        if (!ids.empty())
                        {
                            game = ids.front();
                            hasGame = true;
                        }
                    }
                    else if (LobbyPacketType(subtype) == LobbyPacketType::GAME_STATUS && i == 0)
                    {
                        uint32_t id, speed, rules, score;
                        PlayerID creator;
                        std::string name;
                        std::vector<PlayerID> guests;
                        reader->uint32(id);
                        reader->generic<PlayerID>(creator);
                        reader->string(name);
                        reader->uint32(speed);
                        reader->uint32(rules);
                        reader->uint32(score);
                        reader->generic<std::vector<PlayerID>>(guests);
                        hasGuest = !guests.empty();
                    }
                }
            }
        }
        SDL_Delay(2);
    };
    auto until = [&](const std::function<bool()>& done, const char* error)
    {
        const auto deadline = SDL_GetTicks() + 6000;
        while (!done() && !SDL_TICKS_PASSED(SDL_GetTicks(), deadline))
        {
            pump();
        }
        check(done(), error);
    };
    auto send = [&](int client, LobbyPacketType type, const std::function<void(GenericOut&)>& payload)
    {
        RakNet::BitStream stream;
        stream.Write((unsigned char)ID_LOBBY);
        stream.Write((unsigned char)type);
        auto writer = createGenericWriter(&stream);
        payload(*writer);
        clients[client].Send(&stream, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
    };
    check(clients[0].Connect("127.0.0.1", port, 0, 0, 1), "Host connection failed");
    until([&]() { return named[0] && server.getWaitingPlayers() == 1; }, "Host entry/name query timed out");
    send(0, LobbyPacketType::OPEN_GAME,
         [](GenericOut& out)
         {
             out.uint32(0);
             out.uint32(15);
             out.uint32(0);
             out.string("");
         });
    until([&]() { return hasGame; }, "Open game was not listed");
    check(clients[1].Connect("127.0.0.1", port, 0, 0, 1), "Guest connection failed");
    until([&]() { return named[1] && server.getWaitingPlayers() == 2; }, "Guest entry timed out");
    if (!hosted)
    {
        send(1, LobbyPacketType::JOIN_GAME,
             [&](GenericOut& out)
             {
                 out.uint32(game);
                 out.string("");
             });
    }
    until([&]() { return hasGuest; }, "Guest did not join hosted/dedicated lobby");
    send(0, LobbyPacketType::START_GAME,
         [&](GenericOut& out) { out.generic<PlayerID>(clients[1].GetPlayerID()); });
    until([&]() { return server.getActiveGamesCount() == 1 && ready[0] && ready[1]; },
          "Match did not start for both LAN clients");
    clients[0].Disconnect(20);
    clients[1].Disconnect(20);
    until([&]() { return server.getConnectedClients() == 0 && !server.hasActiveGame(); },
          "Server did not clean up disconnected match");
    std::cout << (hosted ? "Hosted LAN" : "Dedicated LAN")
              << ": named discovery, two clients, lobby, start and disconnect passed\n";
}
} // namespace
void testLAN()
{
    exerciseServer(false, 23456);
    exerciseServer(true, 23457);
}
