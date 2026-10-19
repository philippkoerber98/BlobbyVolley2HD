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

#include "ApplicationLoop.h"
#include "BackgroundSettings.h"
#include "FileSystem.h"
#include "Global.h"
#include "SpeedController.h"
#include "UserConfig.h"
#include <SDL.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// this global allows the host game thread to be killed
extern std::atomic<bool> gKillHostThread;
extern std::shared_ptr<std::thread> gHostedServerThread;

/* implementation */

void deinit()
{
    SDL_Quit();
}

void setupPHYSFS()
{
    FileSystem& fs = FileSystem::getSingleton();

    // Game should be playable out of the source package on all
    // relevant platforms.

    // List all the subdirectories / zip archives that we want to read from
    std::vector<std::string> subdirs = {"gfx", "sounds", "scripts", "rules", "levels"};

    // set write dir
    std::string writeDir = fs.getPrefDir();
    fs.setWriteDir(writeDir);
    for (const auto& sub_dir : subdirs)
    {
        fs.probeDir(sub_dir);
    }
    fs.probeDir("replays");

    // set read dir (files in datafolder next to working dir)
    {
        std::string dataDir = "data";
        fs.addToSearchPath(dataDir);
        for (const auto& archive : subdirs)
        {
            fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
        }
    }

    // set read dir (files next to application)
    {
        const auto installedData = fs.join(fs.getBaseDir(), "data");
        fs.addToSearchPath(installedData);
        for (const auto& archive : subdirs)
        {
            fs.addToSearchPath(fs.join(installedData, archive + ".zip"));
        }
    }
    {
        std::string dataDir = fs.getBaseDir();
        fs.addToSearchPath(dataDir);
        for (const auto& archive : subdirs)
        {
            fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
        }
    }

// set read dir (files inside MacOS bundle)
#if (defined BUILD_MACOS_BUNDLE)
    {
        std::string dataDir = fs.join(fs.getBaseDir(), std::string("Contents") + fs.getDirSeparator() +
                                                           std::string("Resources"));
        fs.addToSearchPath(dataDir);
        for (const auto& archive : subdirs)
        {
            fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
        }
    }
#endif

// set read dir (unix-like when installed)
#ifdef BLOBBY_DATA_DIR
    fs.addToSearchPath(BLOBBY_DATA_DIR);
    for (const auto& archive : subdirs)
    {
        fs.addToSearchPath(fs.join(BLOBBY_DATA_DIR, archive + ".zip"));
    }
#endif
}

int main(int argc, char* argv[])
{

    DEBUG_STATUS("started main");

    FileSystem filesys(argv[0]);
    setupPHYSFS();

    DEBUG_STATUS("physfs initialised");

#if defined(_WIN32)
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

    DEBUG_STATUS("SDL initialised");

    atexit(SDL_Quit);
    atexit(
        []()
        {
            gKillHostThread = true;
            if (gHostedServerThread)
            {
                gHostedServerThread->join();
            }
        });
    srand(SDL_GetTicks());

    // Test Version Startup Warning

    try
    {
        UserConfig gameConfig;
        gameConfig.loadFile("config.xml");
        const auto background = resolveBackgroundName(gameConfig.getString("background"));
        if (background != gameConfig.getString("background"))
        {
            gameConfig.setString("background", background);
            gameConfig.saveFile("config.xml");
        }

        SpeedController scontroller(gameConfig.getFloat("gamefps"));
        SpeedController::setMainInstance(&scontroller);
        scontroller.setDrawFPS(gameConfig.getBool("showfps"));

        runApplicationLoop(gameConfig, scontroller);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                 (std::string("An error occurred, blobby will close: ") + e.what()).c_str(),
                                 nullptr);
        SDL_Quit();
        exit(EXIT_FAILURE);
    }
    deinit();
    exit(EXIT_SUCCESS);
}
