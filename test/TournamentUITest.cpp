#include "BlobbyApp.h"
#include "FileSystem.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "SpeedController.h"
#include "UserConfig.h"
#include "state/TournamentState.h"
#include "raknet/RakClient.h"
#include <SDL_image.h>
#ifdef HAVE_LIBGL
#include <SDL_opengl.h>
#endif
#include <iostream>
#include <stdexcept>
#include <vector>
namespace
{
void frame(BlobbyApp& app)
{
    app.getInputManager().updateInput();
    app.getIMGUI().begin();
    app.step();
    app.getInputManager().clearTransientInput();
}
void screenshot(BlobbyApp& app, const std::string& file)
{
    app.render(0);
    auto* renderer = SDL_GetRenderer(app.getWindow());
    int w, h;
    if (renderer)
    {
        SDL_GetRendererOutputSize(renderer, &w, &h);
    }
    else
    {
        SDL_GL_GetDrawableSize(app.getWindow(), &w, &h);
    }
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (renderer)
    {
        SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, image->pixels, image->pitch);
    }
#ifdef HAVE_LIBGL
    else
    {
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
        auto* top = static_cast<unsigned char*>(image->pixels);
        std::vector<unsigned char> row(image->pitch);
        for (int y = 0; y < h / 2; ++y)
        {
            auto* a = top + y * image->pitch;
            auto* b = top + (h - y - 1) * image->pitch;
            std::copy(a, a + image->pitch, row.begin());
            std::copy(b, b + image->pitch, a);
            std::copy(row.begin(), row.end(), b);
        }
    }
#endif
    if (IMG_SavePNG(image, file.c_str()) != 0)
    {
        SDL_FreeSurface(image);
        throw std::runtime_error("Cannot save tournament screenshot");
    }
    SDL_FreeSurface(image);
}
} // namespace
void testTournamentUI(const std::string& directory)
{
    auto& fs = FileSystem::getSingleton();
    fs.setWriteDir(directory);
    SpeedController speed(75);
    SpeedController::setMainInstance(&speed);
    for (const char* backend : {"SDL", "OpenGL"})
    {
        if (std::string(backend) == "OpenGL" && !SDL_getenv("BLOBBY_TEST_OPENGL"))
        {
            continue;
        }
        for (const char* language : {"en", "de", "cs", "es", "fr", "it"})
        {
            UserConfig config;
            config.loadFile("config.xml");
            config.setString("device", backend);
            config.setString("language", language);
            config.setBool("fullscreen", false);
            config.setInteger("window_width", 800);
            config.setInteger("window_height", 600);
            config.saveFile("config.xml");
            ServerInfo info("LAN Cup");
            SDL_strlcpy(info.hostname, "127.0.0.1", sizeof(info.hostname));
            auto session = std::make_shared<TournamentConnection>(std::make_shared<RakClient>(), info);
            session->received = true;
            session->own = 0;
            auto& t = session->cup;
            t.organizer = 0;
            t.settings.capacity = 16;
            t.settings.name = "Blobby LAN Cup";
            t.settings.format = tournament::Format::Groups;
            for (int i = 0; i < 8; ++i)
            {
                auto id = t.registerPerson("Player " + std::to_string(i + 1), 0, "fixture");
                t.enroll(id, true);
                t.person(id).ready = true;
            }
            BlobbyApp app(std::unique_ptr<State>(new TournamentState(session)), config);
            SDL_HideWindow(app.getWindow());
            app.getCurrentState().init();
            frame(app);
            const auto prefix = directory + "/tournament-" + backend + "-" + language;
            screenshot(app, prefix + "-registration.png");
            t.start(42);
            for (unsigned tab = 0; tab < 4; ++tab)
            {
                if (tab)
                {
                    SDL_Event event{};
                    event.type = SDL_KEYDOWN;
                    event.key.keysym.sym = SDLK_PAGEDOWN;
                    SDL_PushEvent(&event);
                }
                frame(app);
                screenshot(app, prefix + "-tab" + std::to_string(tab) + ".png");
            }
            // Exercise the actual K.O. tree after group results have been applied.
            while (t.matches.size() == 12)
            {
                for (auto& p : t.people)
                {
                    p.ready = true;
                }
                auto ids = t.available();
                if (ids.empty())
                {
                    throw std::runtime_error("Fixture tournament deadlocked");
                }
                for (int id : ids)
                {
                    auto& m = t.match(id);
                    m.status = tournament::MatchStatus::Playing;
                    t.result(id, m.a, 15, 5);
                }
            }
            for (int i = 0; i < 2; ++i)
            {
                SDL_Event e{};
                e.type = SDL_KEYDOWN;
                e.key.keysym.sym = SDLK_PAGEDOWN;
                SDL_PushEvent(&e);
                frame(app);
            }
            screenshot(app, prefix + "-bracket.png");
            app.switchToState(std::unique_ptr<State>(new TournamentSpectatorState(session, 0)));
            frame(app);
            t.matches.clear();
            t.phase = tournament::Phase::Registration;
            frame(app);
            frame(app);
            if (std::string(app.getCurrenStateName()) != "TournamentState")
            {
                throw std::runtime_error("Spectator did not leave a replaced tournament safely");
            }
            session->connected = false;
            app.getInputManager().clearTransientInput();
        }
    }
    SpeedController::setMainInstance(nullptr);
    std::cout << "Tournament UI: registration, overview, groups, bracket and participants in six languages / "
                 "both renderers passed\n";
}
