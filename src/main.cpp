#include "BackgroundSettings.h"
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

/* includes */
#include <ctime>
#include <cstring>
#include <sstream>
#include <atomic>
#include <thread>
#include <iostream>

#include <SDL.h>

#include "Global.h"

#ifdef __SWITCH__
	#include <switch.h>
#endif

#include "RenderManager.h"
#include "SoundManager.h"
#include "InputManager.h"
#include "TextManager.h"
#include "UserConfig.h"
#include "IMGUI.h"
#include "SpeedController.h"
#include "FrameTiming.h"
#include "Blood.h"
#include "FileSystem.h"
#include "state/State.h"
#include "BlobbyApp.h"

// this global allows the host game thread to be killed
extern std::atomic<bool> gKillHostThread;
extern std::shared_ptr<std::thread> gHostedServerThread;

/* implementation */

void deinit()
{
	SDL_Quit();

#if (defined __SWITCH__) && (defined DEBUG)
	socketExit();
#endif
}

void setupPHYSFS()
{
	FileSystem& fs = FileSystem::getSingleton();
	
	// Game should be playable out of the source package on all
	// relevant platforms.

	// List all the subdirectories / zip archives that we want to read from
	std::vector<std::string> subdirs = {"gfx", "sounds", "scripts", "backgrounds", "rules"};

	// set write dir
	std::string writeDir = fs.getPrefDir();
	fs.setWriteDir(writeDir);
	for(const auto& sub_dir : subdirs) {
		fs.probeDir(sub_dir);
	}
	fs.probeDir("replays");

	// set read dir (files in datafolder next to working dir)
	{
		std::string dataDir = "data";
		fs.addToSearchPath(dataDir);
		for(const auto& archive : subdirs) {
			fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
		}
	}

	// set read dir (files next to application)
	{
		std::string dataDir = fs.getBaseDir();
		fs.addToSearchPath(dataDir);
		for(const auto& archive : subdirs) {
			fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
		}
	}

	// set read dir (files inside MacOS bundle)
	#if (defined BUILD_MACOS_BUNDLE)
	{
		std::string dataDir = fs.join(fs.getBaseDir(), std::string("Contents") + 
		                                               fs.getDirSeparator() +
		                                               std::string("Resources"));
		fs.addToSearchPath(dataDir);
		for(const auto& archive : subdirs) {
			fs.addToSearchPath(fs.join(dataDir, archive + ".zip"));
		}
	}
	#endif

	// set read dir (unix-like when installed)
	#ifdef BLOBBY_DATA_DIR
		fs.addToSearchPath(BLOBBY_DATA_DIR);
		for(const auto& archive : subdirs) {
			fs.addToSearchPath(fs.join(BLOBBY_DATA_DIR, archive + ".zip"));
		}
	#endif
}

int main(int argc, char* argv[])
{
#if (defined __SWITCH__) && (defined DEBUG)
	socketInitializeDefault();   
	nxlinkStdio();
#endif

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
	atexit([](){gKillHostThread=true; if(gHostedServerThread) gHostedServerThread->join();});
	srand(SDL_GetTicks());

	// Test Version Startup Warning
	#ifdef TEST_VERSION
	struct tm* ptm;
	time_t time = std::time(0);
	ptm = gmtime ( &time );

	if( ptm->tm_year > (2015-1900) || ptm->tm_mon >= 12 )
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TEST VERISON OUTDATED",
									(std::string("This is a test version of ") + AppTitle + " which expired on "
									"1.12.2015. Please visit https://blobbyvolley.de for a newer version").c_str(), 0);
		return -1;
	}

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "TEST VERISON WARNING",
								(std::string("This is a test version of ") + AppTitle + " for testing only.\n"
								"It might be unstable and/or incompatible to the current release. "
								"Use of this version is limited to 1.12.2015.\n"
								"Visit https://blobbyvolley.de for more information or bug reporting.").c_str(), 0);
	#endif

	try
	{
		UserConfig gameConfig;
		gameConfig.loadFile("config.xml");
        const auto background=resolveBackgroundName(gameConfig.getString("background"));
        if(background!=gameConfig.getString("background")){
            gameConfig.setString("background",background);gameConfig.saveFile("config.xml");
        }

		SpeedController scontroller(gameConfig.getFloat("gamefps"));
		SpeedController::setMainInstance(&scontroller);
		scontroller.setDrawFPS(gameConfig.getBool("showfps"));

		int running = 1;

		DEBUG_STATUS("starting mainloop");

		BlobbyApp app( std::unique_ptr<State>(new MainMenuState()), gameConfig );
		SimulationClock simulationClock;
		const double timerFrequency = double(SDL_GetPerformanceFrequency());
		auto secondsNow = [&]() { return SDL_GetPerformanceCounter() / timerFrequency; };
		double previousTime = secondsNow();
		double nextRenderTime = previousTime;
		simulationClock.advance(1.0 / scontroller.getGameSpeed());

		while (running)
		{
			// Poll frequently, retaining menu edges until one simulation step consumes them.
			app.getInputManager().updateInput(false);
			running = app.getInputManager().running();
			if (!running) break;
			app.updateDisplaySettings();
			const double now = secondsNow();
			simulationClock.advance(now - previousTime);
			previousTime = now;

			while (simulationClock.consume(scontroller.getGameSpeed()))
			{
				app.getIMGUI().begin();
				app.step();
				app.getInputManager().clearTransientInput();
			}
			//draw FPS:
			static int lastfps = 0;
			static int lastlag = -1;
			if (scontroller.getDrawFPS())
			{
				// We need to ensure that the title bar is only set
				// when the framerate changed, because setting the
				// title can be quite resource intensive on some
				// windows manager, like for example metacity.
				// we only update lag information if lag changed at least by
				// 5 ms, for the same reason.
				int newfps = scontroller.getFPS();
				if (newfps != lastfps || std::abs(CURRENT_NETWORK_LAG - lastlag) > 4)
				{
					std::stringstream tmp;
					tmp << AppTitle << "  FPS: " << newfps;
					if( CURRENT_NETWORK_LAG != -1)
						tmp << "  LAG: " << CURRENT_NETWORK_LAG;
					app.getRenderManager().setTitle(tmp.str());
					lastlag = CURRENT_NETWORK_LAG;
				}
				lastfps = newfps;
			}
			// Dirty workarround for hiding fps in title
			if (!scontroller.getDrawFPS() && (lastfps != -1))
			{
				std::stringstream tmp;
				tmp << AppTitle;
				app.getRenderManager().setTitle(tmp.str());

				lastfps = -1;
			}

			if (secondsNow() >= nextRenderTime)
			{
				const double frameStart = secondsNow();
				app.render(simulationClock.alpha(scontroller.getGameSpeed()));
				app.getRenderManager().getBlood().step(app.getRenderManager());
				app.getRenderManager().refresh();
				scontroller.recordRenderedFrame();
				const double interval = 1.0 / app.renderFPS();
				nextRenderTime += interval;
				if (nextRenderTime < frameStart || nextRenderTime > frameStart + interval)
					nextRenderTime = frameStart + interval;
			}
			const double remaining = (std::min)(nextRenderTime - secondsNow(),
				simulationClock.untilNextStep(scontroller.getGameSpeed()) - (secondsNow() - previousTime));
			SDL_Delay(remaining > 0.001 ? Uint32(remaining * 1000) : 0);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", (std::string("An error occurred, blobby will close: ") + e.what()).c_str(), nullptr);
		SDL_Quit();
#if (defined __SWITCH__) && (defined DEBUG)
	socketExit();
#endif
		exit (EXIT_FAILURE);
	}
	deinit();
	exit(EXIT_SUCCESS);
}
