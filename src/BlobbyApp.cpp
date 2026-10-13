#include "HDImagePaths.h"
/*=============================================================================
Blobby Volley 2
Copyright (C) 2022 Daniel Knobe (daniel-knobe@web.de)
Copyright (C) 2022 Erik Schultheis (erik-schultheis@freenet.de)

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
#include "BlobbyApp.h"

/* includes */
#include <string>
#include <cassert>
#include <algorithm>

#include "state/State.h"
#include "SoundManager.h"
#include "BlobbyDebug.h"
#include "IUserConfigReader.h"
#include "IMGUI.h"
#include "FileSystem.h"

/* implementation */
BlobbyApp::~BlobbyApp() = default;

void BlobbyApp::switchToState(std::unique_ptr<State> newState)
{
	assert(newState != nullptr);
	mStateToSwitchTo = std::move(newState);
	mStateToSwitchTo->setApp(this);
}

State& BlobbyApp::getCurrentState() const
{
	return *mCurrentState;
}

const char* BlobbyApp::getCurrenStateName() const
{
	return mCurrentState->getStateName();
}

void BlobbyApp::step()
{
	// perform a state step
	mCurrentState->step_impl();

	// check if we should switch to a new state
	if( mStateToSwitchTo != nullptr )
	{
		// maybe this debug statuses will help pinpointing crashes faster
		DEBUG_STATUS( std::string("switching to state ") + mStateToSwitchTo->getStateName());

		// if yes, set that new state
		// use swap so the states do not get destroyed here
		mCurrentState = std::move( mStateToSwitchTo );
		mIMGUI->resetSelection();
		mIMGUI->begin(); // Do not present commands belonging to the old state.
		mCurrentState->init();
	}
}

void BlobbyApp::render(float alpha)
{
	mCurrentState->render(alpha);
	mIMGUI->end(*mRenderMgr);
}

void BlobbyApp::updateDisplaySettings(bool force)
{
	const Uint32 now = SDL_GetTicks();
	const int display = (std::max)(0, SDL_GetWindowDisplayIndex(getWindow()));
	if (!force && display == mDisplaySettings.display && now - mLastDisplayQuery < 500) return;
	mLastDisplayQuery = now;
	const auto previous = mDisplaySettings;
	mDisplaySettings = queryDisplaySettings(getWindow());
	if (force || previous.display != mDisplaySettings.display ||
	    previous.refreshRate != mDisplaySettings.refreshRate)
		mRenderMgr->setVSync(mFrameLimit == 0);
}

void BlobbyApp::setFrameLimit(int fps)
{
	mFrameLimit = (std::max)(0, fps);
	updateDisplaySettings(true);
}

int BlobbyApp::renderFPS() const
{
	return mFrameLimit ? (std::min)(mFrameLimit, mDisplaySettings.refreshRate) : mDisplaySettings.refreshRate;
}

BlobbyApp::BlobbyApp(std::unique_ptr<State> initState, const IUserConfigReader& config) :
	mCurrentState(std::move(initState)),
	mSoundManager( new SoundManager ),
	mInputMgr( new InputManager(this) )
{
	mCurrentState->setApp(this);

	mSoundManager->setVolume(config.getFloat("global_volume"));
	mSoundManager->setMute(config.getBool("mute"));
	/// \todo play sound is misleading. what we actually want to do is load the sound
	mSoundManager->playSound(SoundManager::IMPACT, 0.0);
	mSoundManager->playSound(SoundManager::WHISTLE, 0.0);

	mIMGUI.reset(new IMGUI(mInputMgr.get()));
	mIMGUI->setTextMgr(config.getString("language"));

	setupRenderManager(config);
}

SoundManager& BlobbyApp::getSoundManager() const
{
	return *mSoundManager;
}

InputManager& BlobbyApp::getInputManager() const {
	return *mInputMgr;
}

IMGUI& BlobbyApp::getIMGUI() const
{
	return *mIMGUI;
}

SDL_Window* BlobbyApp::getWindow() const
{
	return mRenderMgr->getWindow();
}

RenderManager& BlobbyApp::getRenderManager() const
{
	return *mRenderMgr;
}

void BlobbyApp::setupRenderManager(const IUserConfigReader& config) {
	const bool hadWindow = mRenderMgr && mRenderMgr->getWindow();
	const bool wasFullscreen = hadWindow &&
		(SDL_GetWindowFlags(mRenderMgr->getWindow()) & SDL_WINDOW_FULLSCREEN);
	int displayIndex = hadWindow ? SDL_GetWindowDisplayIndex(mRenderMgr->getWindow()) : 0;
	if (displayIndex < 0) displayIndex = 0;
	int oldX = SDL_WINDOWPOS_UNDEFINED, oldY = SDL_WINDOWPOS_UNDEFINED;
	if (hadWindow) SDL_GetWindowPosition(mRenderMgr->getWindow(), &oldX, &oldY);
	std::string device_name = config.getString("device");
	if(device_name == "SDL")
		mRenderMgr = RenderManager::createRenderManagerSDL();
	else if (device_name == "OpenGL")
		mRenderMgr = RenderManager::createRenderManagerGL2D();
	else if (device_name == "Null")
		mRenderMgr = RenderManager::createRenderManagerNull();
	else
	{
		std::cerr << "Warning: Unknown renderer selected!";
		std::cerr << "Falling back to SDL" << std::endl;
		mRenderMgr = RenderManager::createRenderManagerSDL();
	}

	bool fullscreen = config.getBool("fullscreen");
	bool shadows = config.getBool("show_shadow");
	int width = config.getInteger("window_width", BASE_RESOLUTION_X);
	int height = config.getInteger("window_height", BASE_RESOLUTION_Y);
	if (width < 640 || width > 7680 || height < 480 || height > 4320)
	{ width = BASE_RESOLUTION_X; height = BASE_RESOLUTION_Y; }
#if BLOBBY_ON_DESKTOP
	// Keep the window accessible when a larger preset is chosen on a small monitor.
	SDL_Rect usable;
	if (!fullscreen && SDL_GetDisplayUsableBounds(displayIndex, &usable) == 0 && usable.w > 100 && usable.h > 100)
	{
		float scale = (std::min)(1.0f, (std::min)((usable.w - 32.0f) / width, (usable.h - 64.0f) / height));
		width = int(width * scale); height = int(height * scale);
	}
#endif
	mRenderMgr->init(width, height, fullscreen);
	if (fullscreen && hadWindow)
		SDL_SetWindowPosition(mRenderMgr->getWindow(),
			SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex), SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex));
#if BLOBBY_ON_DESKTOP
	if (!fullscreen)
	{
		auto* window = mRenderMgr->getWindow();
		SDL_SetWindowBordered(window, SDL_TRUE);
		SDL_SetWindowResizable(window, SDL_TRUE);
		// SDL stores client-area coordinates. A fullscreen origin such as (0,0)
		// would put the restored window's caption above the desktop.
		SDL_Rect workArea;
		if (SDL_GetDisplayUsableBounds(displayIndex, &workArea) == 0)
		{
			int top=0, left=0, bottom=0, right=0;
			SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
			int w=0, h=0;
			SDL_GetWindowSize(window, &w, &h);
			int x=oldX, y=oldY;
			if (!hadWindow || wasFullscreen)
			{
				x = workArea.x + (workArea.w - w - left - right) / 2 + left;
				y = workArea.y + (workArea.h - h - top - bottom) / 2 + top;
			}
			const int minX = workArea.x + left, minY = workArea.y + top;
			const int maxX = (std::max)(minX, workArea.x + workArea.w - w - right);
			const int maxY = (std::max)(minY, workArea.y + workArea.h - h - bottom);
			SDL_SetWindowPosition(window, (std::max)(minX, (std::min)(maxX, x)),
				(std::max)(minY, (std::min)(maxY, y)));
		}
	}
#else
	if (!fullscreen && hadWindow) SDL_SetWindowPosition(mRenderMgr->getWindow(), oldX, oldY);
#endif
	mRenderMgr->setCourtWidth(config.getBool("court_widescreen",false)?3200.f/3.f:800.f);
	mRenderMgr->showShadow(shadows);
	mFrameLimit = (std::max)(0, config.getInteger("render_fps", 0));
	updateDisplaySettings(true);

	std::string bg = std::string("backgrounds/") + config.getString("background");
	const auto backgroundName=config.getString("background");
	const auto hdBackground="backgrounds/hd/"+backgroundName.substr(0,backgroundName.find_last_of('.'))+".png";
	if ( FileSystem::getSingleton().exists(bg) || FileSystem::getSingleton().exists(preferredHDImage(hdBackground)) )
		mRenderMgr->setBackground(bg);
}
