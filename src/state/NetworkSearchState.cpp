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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

/* header include */
#include "NetworkSearchState.h"

/* includes */
#include <future>
#include <iostream> // debugging
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "raknet/PacketEnumerations.h"
#include "raknet/RakClient.h"
#include "raknet/RakServer.h"

#include "blobnet/exception/HttpException.hpp"
#include "blobnet/layer/Http.hpp"

#include "tinyxml2.h"

#include "FileRead.h"
#include "FileWrite.h"
#include "IMGUI.h"
#include "WarmUI.h"
#include "IUserConfigReader.h"
#include "LobbyStates.h"
#include "NetworkState.h"
#include "SpeedController.h"
#include "TextManager.h"
#include "UserConfig.h"
#include "server/DedicatedServer.h"

// control variables for a dedicated server thread in case a game is hosted
std::shared_ptr<std::thread> gHostedServerThread;
std::atomic<bool> gKillHostThread{false};
namespace
{
std::atomic<bool> hostFailed{false};
}

/* implementation */
NetworkSearchState::NetworkSearchState() : mPingClient(new RakClient)
{
    mSelectedServer = 0;
    mServerBoxPosition = 0;
    mDisplayInfo = false;
    mEnteringServer = false;
    mDisplayUpdateNotification = false;
}

NetworkSearchState::~NetworkSearchState()
{
    stopSearch();
    // Disconnect from servers
    for (auto& client : mQueryClients)
    {
        if (client)
        {
            client->Disconnect(50);
            delete client;
        }
    }
}

void NetworkSearchState::stopSearch()
{
    mCancelPing = true;
    if (mPingJob.valid())
    {
        mPingJob.wait();
    }
}

void NetworkSearchState::searchServers()
{
    if (mPingJob.valid() && mPingJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }
    if (mPingJob.valid())
    {
        mPingJob.get();
    }
    mScannedServers.clear();
    mSelectedServer = 0;
    mCancelPing = false;
    if (dynamic_cast<LANSearchState*>(this))
    {
        doSearchServers(); // One nonblocking UDP broadcast; no worker needs the state.
        return;
    }
    // we need the explicit async launch policy here, because otherwise gcc will always launch deferred and we
    // have no sync point where we would wait for that.
    mPingJob = std::async(std::launch::async, [this]() { doSearchServers(); });
}

void NetworkSearchState::step_impl()
{
    packet_ptr packet;

    // set to true to initiate server connection
    bool doEnterServer = false;

    for (auto iter = mQueryClients.begin(); iter != mQueryClients.end(); ++iter)
    {
        bool skip = false;
        bool skip_iter = false;

        while ((packet = (*iter)->Receive()) && !skip)
        {
            switch (packet->data[0])
            {
            case ID_CONNECTION_REQUEST_ACCEPTED:
            {
                printf("connection accepted from %s:%d\n", mPingClient->PlayerIDToDottedIP(packet->playerId),
                       packet->playerId.port);

                RakNet::BitStream stream;
                stream.Write((unsigned char)ID_BLOBBY_SERVER_PRESENT);
                stream.Write(BLOBBY_VERSION_MAJOR);
                stream.Write(BLOBBY_VERSION_MINOR);
                (*iter)->Send(&stream, LOW_PRIORITY, RELIABLE_ORDERED, 0);
                break;
            }
            case ID_BLOBBY_SERVER_PRESENT:
            {
                if (packet->length != ServerInfo::BLOBBY_SERVER_PRESENT_PACKET_SIZE)
                {
                    break;
                }
                // FIXME: We must copy the needed information, so that we can call DeallocatePacket(packet)
                // FIXME: The client finds a server at this point, which is not valid
                RakNet::BitStream stream(packet->data, packet->length, false);
                stream.IgnoreBytes(1); // ID_BLOBBY_SERVER_PRESENT
                ServerInfo info(stream, (*iter)->PlayerIDToDottedIP(packet->playerId), packet->playerId.port);

                printf("server %s at %s is a blobby server\n", info.name, info.hostname);

                // check that the packet sizes match
                if (packet->length == ServerInfo::BLOBBY_SERVER_PRESENT_PACKET_SIZE)
                {
                    if (std::find(mScannedServers.begin(), mScannedServers.end(), info) ==
                        mScannedServers.end())
                    {
                        mScannedServers.push_back(info);

                        // check whether this was a direct connect server
                        if (*iter == mDirectConnectClient)
                        {
                            mSelectedServer = mScannedServers.size() - 1;
                            doEnterServer = true;
                        }
                    }
                    else
                    {
                        std::cout << "duplicate server entry\n";
                        std::cout << info << "\n";
                    }
                }
                else
                {
                    std::cout << " server invalid " << packet->length << " "
                              << ServerInfo::BLOBBY_SERVER_PRESENT_PACKET_SIZE << "\n";
                }
                if (*iter == mDirectConnectClient)
                {
                    const auto selected = std::find(mScannedServers.begin(), mScannedServers.end(), info);
                    mSelectedServer = unsigned(selected - mScannedServers.begin());
                    doEnterServer = true;
                }
                // the RakClient will be deleted, so
                // we must free the packet here
                packet.reset();
                (*iter)->Disconnect(50);
                if (*iter == mDirectConnectClient)
                {
                    mDirectConnectClient = nullptr;
                }
                delete *iter;
                iter = mQueryClients.erase(iter);
                if (iter == mQueryClients.end())
                {
                    skip_iter = true;
                }
                skip = true;

                // if one server is the one this PC is hosting, auto connect
                if (mHostedServer && std::string(info.hostname) == mHostedServer->hostname &&
                    info.port == mHostedServer->port)
                {
                    switchState(new LobbyState(info, dynamic_cast<OnlineSearchState*>(this) == nullptr
                                                         ? PreviousState::LAN
                                                         : PreviousState::ONLINE));
                    return;
                }
                break;
            }
            case ID_CONNECTION_ATTEMPT_FAILED:
            case ID_NO_FREE_INCOMING_CONNECTIONS:
            {
                mNotice = getIMGUI().getText(TextManager::NET_CON_FAILED);
                if (*iter == mDirectConnectClient)
                {
                    mDirectConnectClient = nullptr;
                }
                packet.reset();
                (*iter)->Disconnect(0);
                delete *iter;
                iter = mQueryClients.erase(iter);
                skip_iter = iter == mQueryClients.end();
                skip = true;
                break;
            }
            case ID_VERSION_MISMATCH:
            {
                // this packet is sent when the client is older than the server!
                // so
                RakNet::BitStream stream(packet->data, packet->length, false);
                stream.IgnoreBytes(1); // ID_VERSION_MISMATCH

                // default values if server does not send versions.
                // that's the 0.9 behaviour
                int smajor = 0, sminor = 9;
                stream.Read(smajor); // load server version information
                stream.Read(sminor);
                printf("found blobby server with version %d.%d\n", smajor, sminor);

                mDisplayUpdateNotification = true;

                // the RakClient will be deleted, so
                // we must free the packet here
                packet.reset();
                (*iter)->Disconnect(50);
                if (*iter == mDirectConnectClient)
                {
                    mDirectConnectClient = nullptr;
                }
                delete *iter;
                iter = mQueryClients.erase(iter);
                if (iter == mQueryClients.end())
                {
                    skip_iter = true;
                }
                skip = true;
                break;
            }
            default:
                break;
            }

            if (skip)
            {
                break;
            }
        }
        if (skip_iter)
        {
            break;
        }
    }
    while (nullptr != (packet = mPingClient->Receive()))
    {
        switch (packet->data[0])
        {
        case ID_PONG:
        {
            std::string hostname = mPingClient->PlayerIDToDottedIP(packet->playerId);
            printf("got ping response by \"%s:%d\", trying to connect\n", hostname.c_str(),
                   packet->playerId.port);
            RakClient* newClient = new RakClient;
            newClient->Connect(hostname.c_str(), packet->playerId.port, 0, 0, RAKNET_THREAD_SLEEP_TIME);
            mQueryClients.push_back(newClient);
        }
        default:
            break;
        }
    }

    IMGUI& imgui = getIMGUI();

    WarmUI::page(imgui, imgui.getText(TextManager::MNU_LABEL_LAN));
    WarmUI::text(
        imgui, 40, 578,
        translateUI(WarmUI::language(), "Up/down: list    Left/right: actions    A: select    B: back"), 15);
    bool startHosting = false;
    if (hostFailed.exchange(false))
    {
        mHostedServer.reset();
        mNotice = translateUI(WarmUI::language(),
                              "Could not start server. Check whether the port is already in use.");
    }
    if (!mNotice.empty())
    {
        WarmUI::text(imgui, 40, 507, mNotice, 16, 2);
    }

    if (mDisplayInfo || mEnteringServer || mNamingServer)
    {
        imgui.doInactiveMode(true);
    }

    if (imgui.doButton(GEN_ID, Vector2(40, 95), TextManager::NET_SERVER_SCAN, TF_NORMAL, 180))
    {
        searchServers();
    }

    if (imgui.doButton(GEN_ID, Vector2(232, 95), TextManager::NET_DIRECT_CONNECT, TF_NORMAL, 220) &&
        !mEnteringServer)
    {
        mEnteringServer = true;
        imgui.resetSelection();
        mEnteredServer = "";
        mServerBoxPosition = 0;
    }

    const bool showInfo =
        imgui.doButton(GEN_ID, Vector2(600, 95), TextManager::NET_SERVER_INFO, TF_NORMAL, 160);
    std::vector<std::string> servernames;
    for (auto& server : mScannedServers)
    {
        servernames.push_back(std::string(server.name) + " (" + std::to_string(server.waitingplayers) + ")");
    }

    if (imgui.doSelectbox(GEN_ID, Vector2(40, 145), Vector2(760, 505), servernames, mSelectedServer) ==
        SBA_DBL_CLICK)
    {
        doEnterServer = true;
    }

    if (showInfo && !mDisplayInfo && !mScannedServers.empty())
    {
        mDisplayInfo = true;
        imgui.resetSelection();
    }

    if (mEnteringServer)
    {
        imgui.doInactiveMode(false);
        WarmUI::panel(imgui, 120, 190, 560, 210);
        WarmUI::text(imgui, 144, 210, imgui.getText(TextManager::NET_DIRECT_CONNECT), 26, 2);
        // Game crashes if the mEnteredServer is not a possible input
        imgui.doEditbox(GEN_ID, Vector2(144, 258), 31, mEnteredServer, mServerBoxPosition);
        if (imgui.doButton(GEN_ID, Vector2(144, 342), TextManager::LBL_CONNECT, TF_NORMAL, 220))
        {
            /// \todo adapt direct connect
            std::string server = mEnteredServer;
            int port = BLOBBY_PORT;
            std::size_t found = mEnteredServer.find(':');
            if (found != std::string::npos)
            {
                server = mEnteredServer.substr(0, found);

                try
                {
                    port = std::stoi(mEnteredServer.substr(found + 1));
                }
                catch (... /* std::invalid_argument, std::out_of_range */)
                {
                    /// \todo inform the user that default port was selected
                }
                if ((port <= 0) || (port > 65535))
                {
                    port = BLOBBY_PORT;
                }
            }

            // add this address / port info as client
            mDirectConnectClient = new RakClient;
            if (server.empty() ||
                !mDirectConnectClient->Connect(server.c_str(), port, 0, 0, RAKNET_THREAD_SLEEP_TIME))
            {
                delete mDirectConnectClient;
                mDirectConnectClient = nullptr;
                mNotice = imgui.getText(TextManager::NET_CON_FAILED);
            }
            else
            {
                mQueryClients.push_back(mDirectConnectClient);
            }
            mEnteringServer = false;
            imgui.resetSelection();
        }
        if (imgui.doButton(GEN_ID, Vector2(490, 342), TextManager::LBL_CANCEL, TF_NORMAL, 166))
        {
            mEnteringServer = false;
            imgui.resetSelection();
        }
        imgui.doInactiveMode(true);
    }

    if (mDisplayInfo)
    {
        imgui.doInactiveMode(false);
        imgui.doOverlay(GEN_ID, Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .55f);
        WarmUI::panel(imgui, 120, 140, 560, 340);
        const auto& server = mScannedServers[mSelectedServer];
        WarmUI::text(imgui, 144, 158, server.name, 26, 2);
        WarmUI::text(imgui, 144, 198, std::string(server.hostname) + ":" + std::to_string(server.port), 20);
        WarmUI::text(imgui, 144, 232,
                     imgui.getText(TextManager::NET_ACTIVE_GAMES) + std::to_string(server.activegames), 20);
        WarmUI::text(imgui, 144, 262,
                     imgui.getText(TextManager::NET_WAITING_PLAYER) + std::to_string(server.waitingplayers),
                     20);
        std::istringstream words(server.description);
        std::string word, line;
        float y = 306;
        while (words >> word && y < 398)
        {
            if (!line.empty() && RenderManager::menuTextWidth(line + " " + word, 18) > 508)
            {
                WarmUI::text(imgui, 144, y, line, 18);
                line = word;
                y += 23;
            }
            else
            {
                line += (line.empty() ? "" : " ") + word;
            }
        }
        if (!line.empty() && y < 398)
        {
            WarmUI::text(imgui, 144, y, line, 18);
        }
        if (imgui.doButton(GEN_ID, Vector2(516, 424), TextManager::LBL_CLOSE, TF_NORMAL, 140) ||
            getInputMgr().exit())
        {
            mDisplayInfo = false;
            imgui.resetSelection();
        }
        imgui.doInactiveMode(true);
    }

    if (imgui.doButton(GEN_ID, Vector2(40, 530), TextManager::NET_HOST_GAME, TF_NORMAL, 220))
    {
        UserConfig config;
        config.loadFile("config.xml");
        mServerName = config.getString("lan_server_name", "Blobby Volley 2 HD");
        mServerNamePosition = unsigned(mServerName.size());
        mNamingServer = true;
        imgui.resetSelection();
    }
    if (mNamingServer)
    {
        imgui.doInactiveMode(false);
        imgui.doOverlay(GEN_ID, Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .55f);
        WarmUI::panel(imgui, 120, 190, 560, 210);
        WarmUI::text(imgui, 144, 210, translateUI(WarmUI::language(), "Server name"), 26, 2);
        imgui.doEditbox(GEN_ID, Vector2(144, 258), 31, mServerName, mServerNamePosition);
        if (imgui.doButton(GEN_ID, Vector2(144, 342), TextManager::NET_HOST_GAME, TF_NORMAL, 220))
        {
            if (mServerName.find_first_not_of(" \t\r\n") == std::string::npos)
            {
                mServerName = "Blobby Volley 2 HD";
            }
            // The existing network packet has 32 name bytes, including its terminator.
            if (mServerName.size() > 31)
            {
                size_t end = 31;
                while ((static_cast<unsigned char>(mServerName[end]) & 0xc0) == 0x80)
                {
                    --end;
                }
                mServerName.resize(end);
            }
            UserConfig config;
            config.loadFile("config.xml");
            config.setString("lan_server_name", mServerName);
            config.saveFile("config.xml");
            startHosting = true;
            mNamingServer = false;
            imgui.resetSelection();
        }
        if (imgui.doButton(GEN_ID, Vector2(490, 342), TextManager::LBL_CANCEL, TF_NORMAL, 166))
        {
            mNamingServer = false;
            imgui.resetSelection();
        }
        imgui.doInactiveMode(true);
    }
    if (startHosting)
    {
        const auto hostName = mServerName;
        UserConfig hostConfig;
        hostConfig.loadFile("config.xml");
        auto identity =
            hostConfig.loadPlayerIdentity(PlayerSide(hostConfig.getInteger("network_side")), true);
        ServerInfo hostInfo(identity.getName());
        std::strncpy(hostInfo.name, hostName.c_str(), sizeof(hostInfo.name) - 1);
        const auto rule = hostConfig.getString("rules");
        const auto speed = SpeedController::getMainInstance()->getGameSpeed();
        auto server_func = [hostInfo, rule, speed]()
        {
            try
            {
                DedicatedServer server(hostInfo, {rule}, {speed}, 4, true);
                SpeedController clock(10);
                while (!gKillHostThread)
                {
                    server.allowNewPlayers(!server.hasActiveGame());
                    server.processPackets();
                    server.updateGames();
                    clock.update();
                }
            }
            catch (const std::exception& error)
            {
                std::cerr << "LAN host failed: " << error.what() << '\n';
                hostFailed = true;
            }
        };

        // shutdown previous hosted server
        gKillHostThread = true;
        if ((gHostedServerThread != nullptr) && (gHostedServerThread->joinable()))
        {
            gHostedServerThread->join();
        }

        // start server
        hostFailed = false;
        gKillHostThread = false;
        gHostedServerThread = std::make_shared<std::thread>(server_func);

        SDL_Delay(100); // give the server some time to start up.
        // might cause a slight visible delay, but I think we can
        // live with that right now.
        searchServers();

        mHostedServer.reset(new ServerInfo(hostInfo));
        std::strncpy(mHostedServer->hostname, "127.0.0.1", sizeof(mHostedServer->hostname) - 1);
        mDirectConnectClient = new RakClient;
        mDirectConnectClient->Connect("127.0.0.1", hostInfo.port, 0, 0, RAKNET_THREAD_SLEEP_TIME);
        mQueryClients.push_back(mDirectConnectClient);
    }

    const bool serverSelected = mSelectedServer < mScannedServers.size();
    if ((imgui.doButton(GEN_ID, Vector2(460, 530), TextManager::LBL_CONNECT, TF_NORMAL, 140,
                        serverSelected)) ||
        doEnterServer)
    {
        if (mSelectedServer >= mScannedServers.size())
        {
            return;
        }
        ServerInfo server = mScannedServers[mSelectedServer];
        switchState(new LobbyState(server, dynamic_cast<OnlineSearchState*>(this) == nullptr
                                               ? PreviousState::LAN
                                               : PreviousState::ONLINE));
    }
    if (imgui.doButton(GEN_ID, Vector2(620, 530), TextManager::LBL_BACK, TF_NORMAL, 140))
    {
        switchState(new MainMenuState);
    }

    if (mDisplayUpdateNotification)
    {
        imgui.doOverlay(GEN_ID, Vector2(71, 572), Vector2(729, 590), Color(128, 0, 0));
        imgui.doText(GEN_ID, Vector2(85, 577), TextManager::UPDATE_NOTIFICATION, TF_SMALL_FONT);
    }
}

const char* NetworkSearchState::getStateName() const
{
    return "NetworkSearchState";
}

// the different networkmodi classes (online/LAN)
OnlineSearchState::OnlineSearchState()
{
    searchServers();
}

void OnlineSearchState::doSearchServers()
{
    // Get the serverlist
    try
    {
        BlobNet::Layer::Http http("blobbyvolley.de", 80);

        std::stringstream serverListXml;
        http.request("server.php", serverListXml);

        // this trows an exception if the file could not be opened for writing
        FileWrite file("onlineserver.xml");

        file.write(serverListXml.str());

        file.close();
    }
    catch (std::exception& e)
    {
        std::cout << "Can't get onlineserver.xml: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Can't get onlineserver.xml. Unknown error." << std::endl;
    }

    std::vector<std::pair<std::string, int>> serverList;

    // Get the serverlist
    try
    {
        auto serverListXml = FileRead::readXMLDocument("onlineserver.xml");

        if (serverListXml->Error())
        {
            std::cerr << "Warning: Parse error in " << "onlineserver.xml";
            std::cerr << "!" << std::endl;
        }

        const auto* onlineserverElem = serverListXml->FirstChildElement("onlineserver");

        if (!onlineserverElem)
        {
            std::cout << "Can't read onlineserver.xml" << std::endl;
            return;
        }

        for (const auto* serverElem = onlineserverElem->FirstChildElement("server"); serverElem;
             serverElem = serverElem->NextSiblingElement("server"))
        {
            std::string host;
            int port;
            for (const auto* varElem = serverElem->FirstChildElement("var"); varElem;
                 varElem = varElem->NextSiblingElement("var"))
            {
                const char* tmp;
                tmp = varElem->Attribute("host");
                if (tmp)
                {
                    host = tmp;
                    continue;
                }

                port = varElem->IntAttribute("port", BLOBBY_PORT);
                if ((port <= 0) || (port > 65535))
                {
                    port = BLOBBY_PORT;
                }
            }
            std::pair<std::string, int> pairs(host, port);
            serverList.push_back(pairs);
        }
    }
    catch (...)
    {
        std::cout << "Can't read onlineserver.xml" << std::endl;
    }

    /// \todo check if we already try to connect to this one!
    std::string address =
        IUserConfigReader::createUserConfigReader("config.xml")->getString("additional_network_server");
    std::string server = address;
    int port = BLOBBY_PORT;
    std::size_t found = address.find(':');
    if (found != std::string::npos)
    {
        server = address.substr(0, found);

        try
        {
            port = std::stoi(address.substr(found + 1));
        }
        catch (... /* std::invalid_argument, std::out_of_range */)
        {
            /// \todo inform the user that default port was selected
        }
        if ((port <= 0) || (port > 65535))
        {
            port = BLOBBY_PORT;
        }
    }

    std::pair<std::string, int> pairs(server.c_str(), port);
    serverList.push_back(pairs);

    mPingClient->Initialize(serverList.size() + 2, 0, 10);

    for (auto& server : serverList)
    {
        // if we receive the cancel signal from the main thread, we leave as soon as possible
        if (mCancelPing)
        {
            break;
        }

        std::cout << "ping" << server.first << "\n";
        mPingClient->Ping(server.first.c_str(), server.second, true);
    }
}

const char* OnlineSearchState::getStateName() const
{
    return "OnlineSearchState";
}

LANSearchState::LANSearchState()
{
    searchServers();
}

void LANSearchState::doSearchServers()
{
    mPingClient->PingServer("255.255.255.255", BLOBBY_PORT, 0, true);
}

const char* LANSearchState::getStateName() const
{
    return "LANSearchState";
}
