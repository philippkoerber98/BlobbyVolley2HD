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
#include "BlobbyApp.h"
#include "Blood.h"
#include "FrameTiming.h"
#include "Global.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "SpeedController.h"
#include "UserConfig.h"
#include "state/State.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <sstream>

void runApplicationLoop(UserConfig& gameConfig, SpeedController& scontroller)
{
    int running = 1;

    DEBUG_STATUS("starting mainloop");

    BlobbyApp app(std::unique_ptr<State>(new MainMenuState()), gameConfig);
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
        if (!running)
        {
            break;
        }
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
        // draw FPS:
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
                if (CURRENT_NETWORK_LAG != -1)
                {
                    tmp << "  LAG: " << CURRENT_NETWORK_LAG;
                }
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
            {
                nextRenderTime = frameStart + interval;
            }
        }
        const double remaining = (std::min)(nextRenderTime - secondsNow(),
                                            simulationClock.untilNextStep(scontroller.getGameSpeed()) -
                                                (secondsNow() - previousTime));
        SDL_Delay(remaining > 0.001 ? Uint32(remaining * 1000) : 0);
    }
}
