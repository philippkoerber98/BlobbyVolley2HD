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
#include "ReplaySelectionState.h"

/* includes */
#include <algorithm>
#include <ctime>
#include <utility>
#include <iostream> // for cerr

#include "FileSystem.h"
#include "IMGUI.h"
#include "WarmUI.h"
#include "ReplayState.h"
#include "SpeedController.h"
#include "TextManager.h"
#include "replays/IReplayLoader.h"

/* implementation */
ReplaySelectionState::ReplaySelectionState()
    : mSelectedReplay(0), mShowReplayInfo(false), mChecksumError(false), mVersionError(false)
{
}

void ReplaySelectionState::init()
{
    mReplayFiles = FileSystem::getSingleton().enumerateFiles("replays", ".bvr");
    if (mReplayFiles.empty())
    {
        mSelectedReplay = -1;
    }
    std::sort(mReplayFiles.rbegin(), mReplayFiles.rend());

    SpeedController::getMainInstance()->setGameSpeed(75);
}

void ReplaySelectionState::step_impl()
{
    IMGUI& imgui = getIMGUI();

    WarmUI::page(imgui, translateUI(WarmUI::language(), "Replays"));
    WarmUI::text(
        imgui, 40, 578,
        translateUI(WarmUI::language(), "Up/down: list    Left/right: actions    A: select    B: back"), 15);
    imgui.doInactiveMode(mShowReplayInfo || mChecksumError || mVersionError);

    const bool play = imgui.doButton(GEN_ID, Vector2(40, 95), TextManager::RP_PLAY, TF_NORMAL, 160);
    const bool info = imgui.doButton(GEN_ID, Vector2(212, 95), TextManager::RP_INFO, TF_NORMAL, 160);
    const bool remove = imgui.doButton(GEN_ID, Vector2(600, 95), TextManager::RP_DELETE, TF_NORMAL, 160);
    mPlayRequested = mPlayRequested || imgui.doSelectbox(GEN_ID, Vector2(40, 145), Vector2(760, 505),
                                                         mReplayFiles, mSelectedReplay) == SBA_DBL_CLICK;
    if (mReplayFiles.empty())
    {
        WarmUI::text(imgui, 56, 165, translateUI(WarmUI::language(), "No replays found."));
    }
    const bool back = imgui.doButton(GEN_ID, Vector2(620, 530), TextManager::LBL_BACK, TF_NORMAL, 140);

    if ((play || std::exchange(mPlayRequested, false)) && mSelectedReplay < mReplayFiles.size())
    {
        std::string loadrep = mReplayFiles[mSelectedReplay];

        /// \todo we have to do something against this construction!
        ///		this is dangerous. we delete this state before it has done
        ///		all of its work.

        ReplayState* rs = nullptr;
        try
        {
            rs = new ReplayState();
            rs->loadReplay(loadrep);

            imgui.resetSelection();
            // at least make sure we end here!

            switchState(rs);
            return;
        }
        catch (std::exception& exp)
        {
            delete rs;
            std::cerr << exp.what() << "\n";
        }
        return;
    }
    else if (back)
    {
        switchState(new MainMenuState());
        return;
    }

    if (info)
    {
        if (!mReplayFiles.empty())
        {
            try
            {
                mReplayLoader.reset(IReplayLoader::createReplayLoader(
                    std::string("replays/" + mReplayFiles[mSelectedReplay] + ".bvr")));
                mShowReplayInfo = true;
                imgui.resetSelection();
            }
            catch (std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
        }
    }
    if (remove)
    {
        if (!mReplayFiles.empty())
        {
            if (FileSystem::getSingleton().deleteFile("replays/" + mReplayFiles[mSelectedReplay] + ".bvr"))
            {
                mReplayFiles.erase(mReplayFiles.begin() + mSelectedReplay);
                if (mSelectedReplay >= mReplayFiles.size())
                {
                    mSelectedReplay = mReplayFiles.size() - 1;
                }
            }
        }
    }

    if (mShowReplayInfo)
    {
        imgui.doInactiveMode(false);
        imgui.doOverlay(GEN_ID, Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .55f);
        WarmUI::panel(imgui, 120, 160, 560, 310);
        WarmUI::text(imgui, 144, 180, mReplayFiles[mSelectedReplay], 26, 2);
        time_t date = mReplayLoader->getDate();
        char dateText[64]{};
        if (auto* utc = std::gmtime(&date))
        {
            std::strftime(dateText, sizeof(dateText), "%Y-%m-%d  %H:%M UTC", utc);
        }
        WarmUI::text(imgui, 144, 211, dateText, 16);
        WarmUI::text(imgui, 144, 240,
                     mReplayLoader->getPlayerName(LEFT_PLAYER) + "  vs  " +
                         mReplayLoader->getPlayerName(RIGHT_PLAYER),
                     22);
        WarmUI::text(imgui, 144, 272,
                     imgui.getText(TextManager::OP_SPEED) + "  " +
                         std::to_string(mReplayLoader->getSpeed() * 100 / 75) + " %",
                     22);
        WarmUI::text(imgui, 144, 310,
                     imgui.getText(TextManager::RP_DURATION) + "  " +
                         std::to_string(mReplayLoader->getDuration()) + " s",
                     22);
        WarmUI::text(imgui, 144, 348,
                     imgui.getText(TextManager::RP_RESULT) + "  " +
                         std::to_string(mReplayLoader->getFinalScore(LEFT_PLAYER)) + " : " +
                         std::to_string(mReplayLoader->getFinalScore(RIGHT_PLAYER)),
                     22);
        if (imgui.doButton(GEN_ID, Vector2(516, 410), TextManager::LBL_CLOSE, TF_NORMAL, 140) ||
            getInputMgr().exit())
        {
            mShowReplayInfo = false;
            imgui.resetSelection();
        }
        imgui.doInactiveMode(false);
    }

    if (mChecksumError)
    {
        imgui.doInactiveMode(false);
        imgui.doOverlay(GEN_ID, Vector2(210, 180), Vector2(650, 370));
        imgui.doText(GEN_ID, Vector2(250, 200), TextManager::RP_CHECKSUM);
        imgui.doText(GEN_ID, Vector2(250, 250), TextManager::RP_FILE_CORRUPT);

        if (imgui.doButton(GEN_ID, Vector2(486, 330), TextManager::LBL_CLOSE, TF_NORMAL, 140))
        {
            mChecksumError = false;
        }
        else
        {
            imgui.doInactiveMode(true);
        }
    }

    if (mVersionError)
    {
        imgui.doInactiveMode(false);
        imgui.doOverlay(GEN_ID, Vector2(210, 180), Vector2(650, 370));
        imgui.doText(GEN_ID, Vector2(250, 200), TextManager::RP_VERSION);
        imgui.doText(GEN_ID, Vector2(250, 250), TextManager::RP_FILE_OUTDATED);

        if (imgui.doButton(GEN_ID, Vector2(486, 330), TextManager::LBL_CLOSE, TF_NORMAL, 140))
        {
            mVersionError = false;
        }
        else
        {
            imgui.doInactiveMode(true);
        }
    }
}

const char* ReplaySelectionState::getStateName() const
{
    return "ReplaySelectionState";
}
