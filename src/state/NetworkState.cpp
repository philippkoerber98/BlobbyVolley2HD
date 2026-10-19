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

/* includes */
#include <algorithm>
#include <iostream>

#include <utility>

#include "raknet/PacketEnumerations.h"
#include "raknet/RakClient.h"

#include "DuelMatch.h"
#include "FileExceptions.h"
#include "FileRead.h"
#include "FileWrite.h"
#include "GenericIO.h"
#include "IMGUI.h"
#include "InputDevice.h"
#include "LobbyStates.h"
#include "LocalInputSource.h"
#include "NetworkState.h"
#include "TournamentState.h"
#include "SoundManager.h"
#include "SpeedController.h"
#include "UserConfig.h"
#include "replays/ReplayRecorder.h"

// global variable to save the lag
int CURRENT_NETWORK_LAG = -1;

/* implementation */
NetworkGameState::NetworkGameState(std::shared_ptr<RakClient> client, int rule_checksum, int score_to_win,
                                   std::shared_ptr<TournamentConnection> tournament)
    : GameState(new DuelMatch(true, DEFAULT_RULES_FILE, score_to_win)), mNetworkState(WAITING_FOR_OPPONENT),
      mWaitingForReplay(false), mClient(std::move(client)), mTournament(std::move(tournament)),
      mWinningPlayer(NO_PLAYER), mSelectedChatmessage(0), mChatCursorPosition(0),
      mRulesChecksum(rule_checksum)
{
    if (mTournament)
    {
        for (const auto& m : mTournament->cup.matches)
        {
            if (m.status == tournament::MatchStatus::Playing &&
                (m.a == mTournament->own || m.b == mTournament->own))
            {
                mTournament->activeMatch = m.id;
            }
        }
    }
}

void NetworkGameState::init()
{
    std::shared_ptr<IUserConfigReader> config = IUserConfigReader::createUserConfigReader("config.xml");
    mOwnSide = (PlayerSide)config->getInteger("network_side");
    mUseRemoteColor = true;
    UserConfig bindings;
    bindings.loadFile("inputconfig.xml");
    const std::string source = config->getInteger("network_input", 0) ? "right_blobby_" : "left_blobby_";
    const std::string target = mOwnSide == LEFT_PLAYER ? "left_blobby_" : "right_blobby_";
    const UserConfig original = bindings;
    for (const char* key : {"device", "keyboard_left", "keyboard_right", "keyboard_jump", "controller_slot",
                            "controller_deadzone", "controller_jump", "joystick_left", "joystick_right",
                            "joystick_jump", "mouse_jumpbutton", "mouse_sensitivity"})
    {
        const auto value = original.getString(source + key);
        if (!value.empty())
        {
            bindings.setString(target + key, value);
        }
    }
    const int preset = config->getInteger("network_input", 0);
    bindings.setInteger(target + "controller_slot", original.getInteger(source + "controller_slot", preset));
    bindings.setInteger(target + "controller_deadzone",
                        original.getInteger(source + "controller_deadzone", 10000));
    bindings.setInteger(target + "controller_jump",
                        original.getInteger(source + "controller_jump", SDL_CONTROLLER_BUTTON_A));
    mLocalInput.reset(new LocalInputSource(getInputMgr().beginGame(mOwnSide, &bindings)));

    // game is not started until two players are connected
    mMatch->pause();

    // load/init players

    if (mOwnSide == LEFT_PLAYER)
    {
        PlayerIdentity localplayer = config->loadPlayerIdentity(LEFT_PLAYER, true);
        PlayerIdentity remoteplayer = config->loadPlayerIdentity(RIGHT_PLAYER, true);
        mLocalPlayer = &mMatch->getPlayer(LEFT_PLAYER);
        mRemotePlayer = &mMatch->getPlayer(RIGHT_PLAYER);
        mMatch->setPlayers(localplayer, remoteplayer);
    }
    else
    {
        PlayerIdentity localplayer = config->loadPlayerIdentity(RIGHT_PLAYER, true);
        PlayerIdentity remoteplayer = config->loadPlayerIdentity(LEFT_PLAYER, true);
        mLocalPlayer = &mMatch->getPlayer(RIGHT_PLAYER);
        mRemotePlayer = &mMatch->getPlayer(LEFT_PLAYER);
        mMatch->setPlayers(remoteplayer, localplayer);
    }

    mRemotePlayer->setName("");

    // check the rules
    int ourChecksum = 0;
    if (mRulesChecksum != 0)
    {
        try
        {
            FileRead rulesFile(TEMP_RULES_NAME);
            ourChecksum = rulesFile.calcChecksum(0);
            rulesFile.close();
        }
        catch (FileLoadException& ex)
        {
            // file doesn't exist - nothing to do here
        }
    }

    RakNet::BitStream stream2;
    stream2.Write((unsigned char)ID_RULES);
    stream2.Write(bool(mRulesChecksum != 0 && mRulesChecksum != ourChecksum));
    mClient->Send(&stream2, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
}

NetworkGameState::~NetworkGameState()
{
    CURRENT_NETWORK_LAG = -1;
    if (!mTournament)
    {
        mClient->Disconnect(50);
    }
}

void NetworkGameState::step_impl()
{
    IMGUI& imgui = getIMGUI();

    packet_ptr packet;
    while (nullptr != (packet = mClient->Receive()))
    {
        if (mTournament && packet->data[0] == ID_TOURNAMENT)
        {
            mTournament->receive(packet);
            if (mTournament->activeMatch >= 0)
            {
                const auto& match = mTournament->cup.match(mTournament->activeMatch);
                if (match.status == tournament::MatchStatus::Interrupted && mNetworkState != PLAYER_WON)
                {
                    mNetworkState = OPPONENT_DISCONNECTED;
                    mMatch->pause();
                }
                if (match.status == tournament::MatchStatus::Finished && match.forfeit)
                {
                    mNetworkState = PLAYER_WON;
                    mWinningPlayer = match.winner == mTournament->own
                                         ? mOwnSide
                                         : (mOwnSide == LEFT_PLAYER ? RIGHT_PLAYER : LEFT_PLAYER);
                    mMatch->pause();
                }
            }
            continue;
        }
        switch (packet->data[0])
        {
        case ID_GAME_UPDATE:
        {
            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_GAME_UPDATE
            unsigned timeBack;
            stream.Read(timeBack);
            CURRENT_NETWORK_LAG = SDL_GetTicks() - timeBack;
            DuelMatchState ms;
            /// \todo this is a performance nightmare: we create a new reader for every packet!
            ///			there should be a better way to do that
            std::shared_ptr<GenericIn> in = createGenericReader(&stream);
            in->generic<DuelMatchState>(ms);
            // inject network data into game
            mMatch->setState(ms);
            break;
        }

        case ID_GAME_EVENTS:
        {
            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_GAME_EVENTS
            // printf("Physic packet received. Time: %d\n", ival);
            //  read events
            char event = 0;
            for (stream.Read(event); event != 0; stream.Read(event))
            {
                char side;
                float intensity = -1;
                stream.Read(side);
                if (event == MatchEvent::BALL_HIT_BLOB)
                {
                    stream.Read(intensity);
                }
                MatchEvent me{MatchEvent::EventType(event), (PlayerSide)side, intensity};
                mMatch->trigger(me);
            }
            break;
        }
        case ID_WIN_NOTIFICATION:
        {
            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_WIN_NOTIFICATION
            stream.Read((int&)mWinningPlayer);

            // last point must not be added anymore, because
            // the score is also simulated local, so it is already
            // right. under strange circumstances this need not
            // be true, but then the score is set to the correy value
            // by ID_BALL_RESET

            mNetworkState = PLAYER_WON;
            break;
        }
        case ID_OPPONENT_DISCONNECTED:
        {
            // In this state, a leaving opponent would not be very surprising
            if (mNetworkState != PLAYER_WON)
            {
                mNetworkState = OPPONENT_DISCONNECTED;
            }
            break;
        }
        case ID_PAUSE:
            if (mNetworkState == PLAYING)
            {
                mNetworkState = PAUSING;
                mMatch->pause();
            }
            break;
        case ID_UNPAUSE:
            if (mNetworkState == PAUSING)
            {
                SDL_StopTextInput();
                mNetworkState = PLAYING;
                mMatch->unpause();
            }
            break;
        case ID_GAME_READY:
        {
            char charName[16];
            RakNet::BitStream stream(packet->data, packet->length, false);

            stream.IgnoreBytes(1); // ignore ID_GAME_READY

            // read gamespeed
            int speed;
            stream.Read(speed);
            SpeedController::getMainInstance()->setGameSpeed(speed);

            // read playername
            stream.Read(charName, sizeof(charName));

            // ensures that charName is null terminated
            charName[sizeof(charName) - 1] = '\0';

            // read colors
            int temp;
            stream.Read(temp);
            auto ncolor = Color(temp);
            int matchTime = 0;
            if (stream.GetNumberOfUnreadBits() >= 32 && stream.Read(matchTime))
            {
                mMatch->setMatchTimeMs((std::max)(0, matchTime));
            }

            bool cycling = false;
            if (stream.Read(cycling))
            {
                mRemotePlayer->setOscillating(cycling);
            }
            unsigned char length = 0;
            char fullName[64] = {};
            if (stream.Read(length) && length > 0 && length < sizeof(fullName) &&
                stream.Read(fullName, length))
            {
                mRemotePlayer->setName(std::string(fullName, length));
            }
            else
            {
                mRemotePlayer->setName(charName);
            }

            setDefaultReplayName(mLocalPlayer->getName(), mRemotePlayer->getName());

            // check whether to use remote player color
            if (mUseRemoteColor)
            {
                mRemotePlayer->setStaticColor(ncolor);
            }

            mNetworkState = PLAYING;
            // start game
            mMatch->unpause();

            // game ready whistle
            playSound(SoundManager::WHISTLE, ROUND_START_SOUND_VOLUME);
            break;
        }
        case ID_RULES_CHECKSUM:
        {
            assert(0);
            break;
        }
        case ID_RULES:
        {
            RakNet::BitStream stream(packet->data, packet->length, false);

            stream.IgnoreBytes(1); // ignore ID_RULES

            int rulesLength;
            stream.Read(rulesLength);
            if (rulesLength)
            {
                std::vector<char> rulesString(rulesLength + 1, '\0');
                stream.Read(rulesString.data(), rulesLength);
                FileWrite rulesFile("rules/" + TEMP_RULES_NAME);
                rulesFile.write(rulesString.data(), rulesLength);
                rulesFile.close();
                mMatch->setRules(TEMP_RULES_NAME);
            }
            else
            {
                // either old server, or we have to use fallback ruleset
                mMatch->setRules(FALLBACK_RULES_NAME);
            }

            break;
        }
        // status messages we don't care about
        case ID_REMOTE_DISCONNECTION_NOTIFICATION:
        case ID_REMOTE_CONNECTION_LOST:
        case ID_SERVER_STATUS:
        case ID_REMOTE_NEW_INCOMING_CONNECTION:
        case ID_REMOTE_EXISTING_CONNECTION:
            break;
        case ID_DISCONNECTION_NOTIFICATION:
        case ID_CONNECTION_LOST:
            if (mNetworkState != PLAYER_WON)
            {
                mNetworkState = DISCONNECTED;
            }
            break;
        case ID_NO_FREE_INCOMING_CONNECTIONS:
            mNetworkState = SERVER_FULL;
            break;
        case ID_CHAT_MESSAGE:
        {
            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_CHAT_MESSAGE
            // Insert Message in the log and focus the last element
            char message[31];
            stream.Read(message, sizeof(message));
            message[30] = '\0';

            // Insert Message in the log and focus the last element
            mChatlog.push_back((std::string)message);
            mChatOrigin.push_back(false);
            mSelectedChatmessage = mChatlog.size() - 1;
            playSound(SoundManager::CHAT, ROUND_START_SOUND_VOLUME);
            break;
        }
        case ID_REPLAY:
        {
            /// \todo we should take more action if server sends replay
            ///		even if not requested!
            if (!mWaitingForReplay)
            {
                break;
            }

            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_REPLAY

            // read stream into a dummy replay recorder
            std::shared_ptr<GenericIn> reader = createGenericReader(&stream);
            ReplayRecorder dummyRec;
            dummyRec.receive(*reader);
            // and save that
            saveReplay(dummyRec);

            // mWaitingForReplay will be set to false even if replay could not be saved because
            // the server won't send it again.
            mWaitingForReplay = false;

            break;
        }

        // we never do anything that should cause such a packet to be received!
        case ID_CONNECTION_REQUEST_ACCEPTED:
        case ID_CONNECTION_ATTEMPT_FAILED:
            assert(0);
            break;

        case ID_BLOBBY_SERVER_PRESENT:
        {
            // this should only be called if we use the stay on server option
            RakNet::BitStream stream(packet->data, packet->length, false);
            stream.IgnoreBytes(1); // ID_BLOBBY_SERVER_PRESENT
            ServerInfo info(stream, mClient->PlayerIDToDottedIP(packet->playerId), packet->playerId.port);

            if (packet->length == ServerInfo::BLOBBY_SERVER_PRESENT_PACKET_SIZE)
            {
                switchState(new LobbyState(info, PreviousState::MAIN));
            }
            break;
        }
        default:
            printf("Received unknown Packet %d\n", packet->data[0]);
            std::cout << packet->data << "\n";
            break;
        }
    }

    // does this generate any problems if we pause at the exact moment an event is set ( i.e. the ball hit
    // sound could be played in a loop)?
    presentGame();
    presentGameUI();

    if (is_exiting() && mNetworkState != PLAYING)
    {
        if (mNetworkState == PAUSING)
        {
            // end pause
            RakNet::BitStream stream;
            stream.Write((unsigned char)ID_UNPAUSE);
            mClient->Send(&stream, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
        }
        else
        {
            returnToLobby();
        }
    }
    else if (is_exiting() && mSaveReplay)
    {
        mSaveReplay = false;
        getIMGUI().resetSelection();
    }
    else if (!mErrorMessage.empty())
    {
        displayErrorMessageBox();
    }
    else if (mSaveReplay)
    {
        if (displaySaveReplayPrompt())
        {

            // request replay from server
            RakNet::BitStream stream;
            stream.Write((unsigned char)ID_REPLAY);
            mClient->Send(&stream, LOW_PRIORITY, RELIABLE_ORDERED, 0);

            mSaveReplay = false;
            mWaitingForReplay = true;
        }
    }
    else if (mWaitingForReplay)
    {
        imgui.doOverlay(GEN_ID, Vector2(150, 200), Vector2(650, 400));
        imgui.doText(GEN_ID, Vector2(190, 220), TextManager::RP_WAIT_REPLAY);
        if (imgui.doButton(GEN_ID, Vector2(440, 330), TextManager::LBL_CANCEL))
        {
            mSaveReplay = false;
            mWaitingForReplay = false;
            imgui.resetSelection();
        }
        imgui.doCursor();
    }
    else
    {
        switch (mNetworkState)
        {
        case WAITING_FOR_OPPONENT:
        {
            imgui.doOverlay(GEN_ID, Vector2(100.0, 210.0), Vector2(700.0, 310.0));
            imgui.doText(GEN_ID, Vector2(150.0, 250.0), TextManager::GAME_WAITING);
            break;
        }
        case OPPONENT_DISCONNECTED:
        {
            imgui.doCursor();
            if (mTournament)
            {
                imgui.useMenuStyle();
                WarmUI::panel(imgui, 150, 220, 500, 150);
                WarmUI::text(imgui, 170, 242,
                             translateUI(WarmUI::language(), "Match interrupted. Return to the tournament."),
                             20);
                if (imgui.doButton(GEN_ID, Vector2(230, 310),
                                   translateUI(WarmUI::language(), "Tournament overview"), TF_NORMAL, 340))
                {
                    returnToLobby();
                }
                break;
            }

            displayQueryPrompt(200, TextManager::GAME_OPP_LEFT,
                               std::make_tuple(TextManager::LBL_MAIN_MENU, [&]() { returnToLobby(); }),
                               std::make_tuple(TextManager::RP_SAVE,
                                               [&]()
                                               {
                                                   mSaveReplay = true;
                                                   imgui.resetSelection();
                                               }),
                               std::make_tuple(TextManager::NET_STAY_ON_SERVER,
                                               [&]()
                                               {
                                                   // Send a blobby server connection request
                                                   RakNet::BitStream stream;
                                                   stream.Write((unsigned char)ID_BLOBBY_SERVER_PRESENT);
                                                   stream.Write(BLOBBY_VERSION_MAJOR);
                                                   stream.Write(BLOBBY_VERSION_MINOR);
                                                   mClient->Send(&stream, LOW_PRIORITY, RELIABLE_ORDERED, 0);
                                               }));
            break;
        }
        case DISCONNECTED:
        {
            imgui.useMenuStyle();
            imgui.doCursor();
            imgui.doOverlay(GEN_ID, Vector2(100.0, 210.0), Vector2(700.0, 370.0));
            imgui.doText(GEN_ID, Vector2(120.0, 250.0), TextManager::NET_DISCONNECT);
            if (imgui.doButton(GEN_ID, Vector2(120, 320), TextManager::LBL_MAIN_MENU, TF_NORMAL, 180))
            {
                returnToLobby();
            }
            if (imgui.doButton(GEN_ID, Vector2(320, 320), TextManager::RP_SAVE, TF_NORMAL, 220))
            {
                mSaveReplay = true;
                imgui.resetSelection();
            }
            break;
        }
        case SERVER_FULL:
        {
            imgui.useMenuStyle();
            imgui.doCursor();
            imgui.doOverlay(GEN_ID, Vector2(100.0, 210.0), Vector2(700.0, 370.0));
            imgui.doText(GEN_ID, Vector2(200.0, 250.0), TextManager::NET_SERVER_FULL);
            if (imgui.doButton(GEN_ID, Vector2(300, 310), TextManager::LBL_MAIN_MENU, TF_NORMAL, 200))
            {
                returnToLobby();
            }
            break;
        }
        case PLAYING:
        {
            mMatch->step();

            mLocalInput->updateInput();
            PlayerInputAbs input = mLocalInput->getRealInput();

            if (is_exiting())
            {
                RakNet::BitStream stream;
                stream.Write((unsigned char)ID_PAUSE);
                mClient->Send(&stream, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
            }
            RakNet::BitStream stream;
            stream.Write((unsigned char)ID_INPUT_UPDATE);
            stream.Write(SDL_GetTicks());
            input.writeTo(stream);
            mClient->Send(&stream, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);
            break;
        }
        case PLAYER_WON:
        {
            mMatch->updateEvents(); // so the last whistle will be sounded
            displayWinningPlayerScreen(mWinningPlayer);
            const int action = displayVictoryActions(
                false, false, mTournament ? translateUI(WarmUI::language(), "Tournament overview") : "");
            if (action == 2)
            {
                returnToLobby();
            }
            if (action == 3)
            {
                mSaveReplay = true;
                imgui.resetSelection();
            }

            break;
        }
        case PAUSING:
        {
            // Query
            displayQueryPrompt(20, TextManager::GAME_PAUSED,
                               std::make_tuple(TextManager::LBL_CONTINUE,
                                               [&]()
                                               {
                                                   RakNet::BitStream stream;
                                                   stream.Write((unsigned char)ID_UNPAUSE);
                                                   mClient->Send(&stream, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
                                               }),
                               std::make_tuple(TextManager::GAME_QUIT, [&]() { returnToLobby(); }),
                               std::make_tuple(TextManager::RP_SAVE,
                                               [&]()
                                               {
                                                   mSaveReplay = true;
                                                   imgui.resetSelection();
                                               }));

            // Chat
            imgui.doChatbox(GEN_ID, Vector2(10, 240), Vector2(790, 500), mChatlog, mSelectedChatmessage,
                            mChatOrigin);
            if (imgui.doEditbox(GEN_ID, Vector2(30, 510), 30, mChattext, mChatCursorPosition, 0, true))
            {

                // GUI-Hack, so that we can send messages
                if ((getInputMgr().getLastActionKey() == "Return") && (!mChattext.empty()))
                {
                    RakNet::BitStream stream;
                    char message[31];

                    strncpy(message, mChattext.c_str(), sizeof(message));
                    stream.Write((unsigned char)ID_CHAT_MESSAGE);
                    stream.Write(message, sizeof(message));
                    mClient->Send(&stream, LOW_PRIORITY, RELIABLE_ORDERED, 0);
                    mChatlog.push_back(mChattext);
                    mChatOrigin.push_back(true);
                    mSelectedChatmessage = mChatlog.size() - 1;
                    mChattext = "";
                    mChatCursorPosition = 0;
                    playSound(SoundManager::CHAT, ROUND_START_SOUND_VOLUME);
                }
            }
            imgui.doCursor();
        }
        }
    }
}

const char* NetworkGameState::getStateName() const
{
    return "NetworkGameState";
}

// definition of syslog for client hosted games
void syslog(int pri, const char* format, ...)
{
    // do nothing?
}

// debug counters
int SWLS_PacketCount;
int SWLS_Connections;
int SWLS_Games;
int SWLS_GameSteps;
int SWLS_ServerEntered;

void NetworkGameState::returnToLobby()
{
    if (mTournament)
    {
        switchState(new TournamentState(mTournament));
    }
    else
    {
        switchState(new MainMenuState);
    }
}
