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

#pragma once

#include "NetworkMessage.h"
#include "State.h"

#include <atomic>
#include <future>
#include <list>
#include <vector>

class RakClient;
class RakServer;
class DuelMatch;
class NetworkGame;

/*! \class NetworkSearchState
    \brief Base class for search states
    \details This class provides a search interface.
            The actual search process has to be implemented
            by derived class by overriding the searchServers()
            method.
*/
class NetworkSearchState : public State
{
  public:
    NetworkSearchState();
    ~NetworkSearchState() override;

    void step_impl() override;
    // onlinegames connect to the masterserver
    // LAN games send a broadcast to local network
    void searchServers();

    const char* getStateName() const override;

  protected:
    void stopSearch();
    std::vector<ServerInfo> mScannedServers;
    std::unique_ptr<RakClient> mPingClient;
    // set this to true to step pinging before we are finished
    std::atomic<bool> mCancelPing{false};

  private:
    virtual void doSearchServers() = 0;

    typedef std::list<RakClient*> ClientList;

    ClientList mQueryClients;
    RakClient* mDirectConnectClient = nullptr;

    std::future<void> mPingJob;

    unsigned mSelectedServer;
    bool mDisplayInfo;
    bool mNamingServer = false;
    std::string mServerName, mNotice;
    unsigned mServerNamePosition = 0;
    bool mEnteringServer;
    bool mDisplayUpdateNotification;

    std::unique_ptr<ServerInfo> mHostedServer;

    std::string mEnteredServer;
    unsigned mServerBoxPosition;
};

/*! \class OnlineSearchState
    \brief State for online server search screen.
*/
class OnlineSearchState : public NetworkSearchState
{
  public:
    OnlineSearchState();
    ~OnlineSearchState() override
    {
        stopSearch();
    }
    void doSearchServers() override;
    const char* getStateName() const override;
};

/*! \class LANSearchState
    \brief State for LAN game search screen.
*/
class LANSearchState : public NetworkSearchState
{
  public:
    LANSearchState();
    ~LANSearchState() override
    {
        stopSearch();
    }
    const char* getStateName() const override;

  private:
    void doSearchServers() override;
};
