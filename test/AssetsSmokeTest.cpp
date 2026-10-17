#include "AssetTransforms.h"
#include "DisplaySettings.h"
#include "DuelMatchState.h"
#include "FileSystem.h"
#include "ImagePaths.h"
#include "RenderManager.h"
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <stdexcept>

void testFrameTiming();
void testPresentation(const std::string& outputDirectory);
void testReplayTiming(const std::string& outputDirectory);
void testWindowDecorations(const std::string& outputDirectory);
void testTabbedOptions(const std::string& outputDirectory);
void testGameMenus(const std::string& outputDirectory);
void testRoundedPanels(const std::string& outputDirectory);
void testSceneBlurAspect(const std::string& outputDirectory);
void testWidescreen(const std::string& outputDirectory);
void testBotModes(const std::string& outputDirectory);
void testHeldSliderInput();
void testLevelAudio(const std::string& outputDirectory);

class AssetProbe : public RenderManager
{
  public:
    using RenderManager::assetScale;
    using RenderManager::loadSurface;
    void drawGame(const DuelMatchState&) override {}
};

static void save(RenderManager& render, const std::string& path)
{
    auto* renderer = SDL_GetRenderer(render.getWindow());
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    // Read the entire physical framebuffer, including both letterbox margins.
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1, 1);
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!image ||
        SDL_RenderReadPixels(renderer, nullptr, image->format->format, image->pixels, image->pitch) != 0)
    {
        throw std::runtime_error(SDL_GetError());
    }
    if (IMG_SavePNG(image, path.c_str()) != 0)
    {
        throw std::runtime_error(IMG_GetError());
    }
    SDL_FreeSurface(image);
    SDL_RenderSetLogicalSize(renderer, 800, 600);
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--display-info")
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << SDL_GetError();
            return 1;
        }
        for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
        {
            auto* window = SDL_CreateWindow("Display query", SDL_WINDOWPOS_CENTERED_DISPLAY(i),
                                            SDL_WINDOWPOS_CENTERED_DISPLAY(i), 640, 480, SDL_WINDOW_HIDDEN);
            const auto display = queryDisplaySettings(window);
            std::cout << "Display " << i << ": " << display.desktopWidth << "x" << display.desktopHeight
                      << " @ " << display.refreshRate << " Hz; FPS caps:";
            for (int limit : display.frameLimits)
            {
                std::cout << " " << limit;
            }
            std::cout << "; window sizes:";
            for (auto size : display.windowSizes)
            {
                std::cout << " " << size.width << "x" << size.height;
            }
            std::cout << '\n';
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        return 0;
    }
    if (argc != 3 && argc != 4)
    {
        return 1;
    }
    try
    {
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        const bool hardware = argc == 4 && std::string(argv[3]) == "--hardware";
        if (!hardware)
        {
            SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        }
        else
        {
            SDL_setenv("BLOBBY_TEST_OPENGL", "1", 1);
        }
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            throw std::runtime_error(SDL_GetError());
        }
        FileSystem fs(argv[0]);
        fs.addToSearchPath(argv[1]);
        fs.addToSearchPath(fs.join(argv[1], ".."));
        for (const char* archive : {"gfx", "sounds", "scripts", "rules"})
        {
            fs.addToSearchPath(std::string(argv[1]) + "/" + archive + ".zip");
        }
        if (argc == 4 && std::string(argv[3]) == "--audio-only")
        {
            testLevelAudio(argv[2]);
            SDL_Quit();
            return 0;
        }
        testFrameTiming();
        testHeldSliderInput();
        testLevelAudio(argv[2]);
        testSceneBlurAspect(argv[2]);
        testTabbedOptions(argv[2]);
        testGameMenus(argv[2]);
        testRoundedPanels(argv[2]);
        testPresentation(argv[2]);
        testWindowDecorations(argv[2]);
        testReplayTiming(argv[2]);
        testWidescreen(argv[2]);
        testBotModes(argv[2]);
        AssetProbe probe;
        // Recolor both gel materials, preserve metal/highlights and alpha.
        auto* material = SDL_CreateRGBSurfaceWithFormat(0, 5, 1, 32, SDL_PIXELFORMAT_RGBA32);
        auto* materialPixels = static_cast<Uint8*>(material->pixels);
        const Uint8 samples[] = {200, 40,  200, 255, 120, 20,  120, 128, 230, 180,
                                 20,  255, 180, 180, 180, 255, 255, 0,   255, 0};
        std::copy_n(samples, sizeof(samples), materialPixels);
        auto* tinted = colorizeChampion(material, {0, 255, 255, 255});
        if (!tinted)
        {
            throw std::runtime_error("Cannot recolor champion materials");
        }
        const Uint8 expected[] = {40, 200, 200, 255, 20,  120, 120, 128, 230, 180,
                                  20, 255, 180, 180, 180, 255, 255, 0,   255, 0};
        if (!std::equal(expected, expected + sizeof(expected), static_cast<Uint8*>(tinted->pixels)))
        {
            throw std::runtime_error("Champion body/hand tint changed trophy colors or alpha");
        }
        SDL_FreeSurface(material);
        SDL_FreeSurface(tinted);
        auto* champion = probe.loadSurface("gfx/champion.webp");
        if (champion->w < 1024 || champion->h < 1024 || !champion->format->Amask)
        {
            throw std::runtime_error("Champion must retain native HD resolution and transparency");
        }
        SDL_FreeSurface(champion);
        // Regression: remove detached debris while preserving a soft alpha edge.
        auto* sample = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_FillRect(sample, nullptr, 0);
        SDL_Rect core = {8, 8, 12, 12}, fringe = {7, 10, 1, 1}, debris = {28, 28, 1, 1};
        SDL_FillRect(sample, &core, SDL_MapRGBA(sample->format, 100, 120, 140, 255));
        SDL_FillRect(sample, &fringe, SDL_MapRGBA(sample->format, 255, 255, 255, 64));
        SDL_FillRect(sample, &debris, SDL_MapRGBA(sample->format, 255, 255, 255, 255));
        cleanSpriteEdges(sample);
        Uint8 sr, sg, sb, sa;
        SDL_GetRGBA(reinterpret_cast<Uint32*>(static_cast<Uint8*>(sample->pixels) + 10 * sample->pitch)[7],
                    sample->format, &sr, &sg, &sb, &sa);
        if (sr != 100 || sg != 120 || sb != 140 || sa != 64)
        {
            throw std::runtime_error("Sprite soft edge was damaged");
        }
        SDL_GetRGBA(reinterpret_cast<Uint32*>(static_cast<Uint8*>(sample->pixels) + 28 * sample->pitch)[28],
                    sample->format, &sr, &sg, &sb, &sa);
        if (sa)
        {
            throw std::runtime_error("Detached sprite debris survived cleanup");
        }
        SDL_FreeSurface(sample);
        auto* icon = probe.loadSurface("Icon.bmp");
        if (!icon || probe.assetScale("Icon.bmp") != 4)
        {
            throw std::runtime_error("New window icon failed");
        }
        SDL_FreeSurface(icon);
        const char* names[] = {"pokal",      "titel",       "cursor",    "flag",        "btn_play",
                               "btn_pause",  "btn_fast",    "btn_slow",  "pfeil_links", "pfeil_rechts",
                               "pfeil_oben", "pfeil_unten", "scrollbar", "blood",       "schball"};
        for (auto name : names)
        {
            std::string path = std::string("gfx/") + name + ".bmp";
            auto* asset = probe.loadSurface(path);
            if (!asset || probe.assetScale(path) != 4)
            {
                throw std::runtime_error("Missing new asset: " + path);
            }
            SDL_FreeSurface(asset);
        }
        for (int i = 1; i <= 16; ++i)
        {
            std::string path = "gfx/ball" + std::string(i < 10 ? "0" : "") + std::to_string(i) + ".bmp";
            auto* ball = probe.loadSurface(path);
            if (!ball || ball->w != 256 || ball->h != 256 || SDL_HasColorKey(ball))
            {
                throw std::runtime_error("Ball rotation/alpha failed");
            }
            double top = 0, bottom = 0;
            for (int y = 35; y < 100; ++y)
            {
                for (int x = 80; x < 176; ++x)
                {
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(
                        reinterpret_cast<Uint32*>(static_cast<Uint8*>(ball->pixels) + y * ball->pitch)[x],
                        ball->format, &r, &g, &b, &a);
                    top += r;
                    SDL_GetRGBA(reinterpret_cast<Uint32*>(static_cast<Uint8*>(ball->pixels) +
                                                          (255 - y) * ball->pitch)[x],
                                ball->format, &r, &g, &b, &a);
                    bottom += r;
                }
            }
            if ((top - bottom) / (65 * 96) < 30)
            {
                throw std::runtime_error("Ball lighting rotated away from top");
            }
            SDL_FreeSurface(ball);
        }
        // Rotations inside one former 22.5-degree frame must now differ,
        // while lighting remains brighter above the ball at every angle.
        {
            auto* source = probe.loadSurface("gfx/volleyball.png");
            auto* panels = rotateVolleyball(source, 1, VolleyballLayer::Panels);
            auto* lighting = rotateVolleyball(source, 1, VolleyballLayer::Lighting);
            auto* canvas = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
            auto* renderer = SDL_CreateSoftwareRenderer(canvas);
            auto* panelTexture = SDL_CreateTextureFromSurface(renderer, panels);
            auto* lightTexture = SDL_CreateTextureFromSurface(renderer, lighting);
            SDL_SetTextureBlendMode(panelTexture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureBlendMode(lightTexture, SDL_BLENDMODE_BLEND);
            std::vector<Uint8> firstPixels;
            int index = 0;
            for (double angle : {0.0, 5.0, 90.0, 180.0, 270.0, 359.0})
            {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderCopyEx(renderer, panelTexture, nullptr, nullptr, angle, nullptr, SDL_FLIP_NONE);
                SDL_RenderCopy(renderer, lightTexture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
                auto* pixels = static_cast<Uint8*>(canvas->pixels);
                if (index == 0)
                {
                    firstPixels.assign(pixels, pixels + canvas->pitch * canvas->h);
                }
                if (index == 1 && std::equal(firstPixels.begin(), firstPixels.end(), pixels))
                {
                    throw std::runtime_error("Ball rotation still snaps to animation frames");
                }
                double top = 0, bottom = 0;
                for (int y = 35; y < 100; ++y)
                {
                    for (int x = 80; x < 176; ++x)
                    {
                        top += pixels[y * canvas->pitch + x * 4];
                        bottom += pixels[(255 - y) * canvas->pitch + x * 4];
                    }
                }
                if ((top - bottom) / (65 * 96) < 30)
                {
                    throw std::runtime_error("Continuous ball lighting rotates with panels");
                }
                ++index;
            }
            SDL_DestroyTexture(panelTexture);
            SDL_DestroyTexture(lightTexture);
            SDL_DestroyRenderer(renderer);
            SDL_FreeSurface(canvas);
            SDL_FreeSurface(source);
            SDL_FreeSurface(panels);
            SDL_FreeSurface(lighting);
        }
        for (int i = 1; i <= 5; ++i)
        {
            auto* blob = probe.loadSurface("gfx/blobbym" + std::to_string(i) + ".bmp");
            if (!blob || blob->w != 300 || blob->h != 356 || SDL_HasColorKey(blob))
            {
                throw std::runtime_error("Individual PNG blob failed");
            }
            SDL_FreeSurface(blob);
            auto* shadow = probe.loadSurface("gfx/sch1" + std::to_string(i) + ".bmp");
            if (!shadow || shadow->w != 512 || shadow->h != 128 || SDL_HasColorKey(shadow))
            {
                throw std::runtime_error("PNG shadow failed");
            }
            SDL_FreeSurface(shadow);
        }
        // Rasterize the actual bundled TTF at different monitor densities.
        for (int pixels : {24, 64, 144})
        {
            RuntimeFontAtlas atlas;
            auto* image = createNunitoAtlas(pixels, atlas);
            if (!image || atlas.pixels != pixels)
            {
                throw std::runtime_error("Runtime TTF atlas failed");
            }
            for (unsigned c : {unsigned('A'), unsigned('g'), unsigned('*'), 0xe4u, 0xdfu, 0x10du})
            {
                const auto& r = atlas.glyphs[c].source;
                if (r.w <= 0 || r.h <= 0 || r.x < 3 || r.y < 3 || r.x + r.w >= image->w ||
                    r.y + r.h >= image->h)
                {
                    throw std::runtime_error("Runtime glyph clipped or missing");
                }
                int opaque = 0, soft = 0;
                for (int y = r.y; y < r.y + r.h; ++y)
                {
                    for (int x = r.x; x < r.x + r.w; ++x)
                    {
                        unsigned alpha = static_cast<Uint8*>(image->pixels)[y * image->pitch + x * 4 + 3];
                        opaque += alpha == 255;
                        soft += alpha > 0 && alpha < 255;
                    }
                }
                if (!opaque || !soft)
                {
                    throw std::runtime_error("Runtime glyph has no antialiased coverage");
                }
            }
            SDL_FreeSurface(image);
        }

        {
            auto render = RenderManager::createRenderManagerSDL();
            render->init(1920, 1080, false);
            render->showShadow(true);
            render->setBlobColor(LEFT_PLAYER, Color(255, 0, 0));
            render->setBlobColor(RIGHT_PLAYER, Color(0, 200, 0));
            DuelMatchState match{};
            match.worldState.blobPosition[LEFT_PLAYER] = Vector2(200, 480);
            match.worldState.blobPosition[RIGHT_PLAYER] = Vector2(600, 480);
            match.worldState.ballPosition = Vector2(350, 280);
            render->drawGame(match);
            save(*render, std::string(argv[2]) + "/assets-beach.png");
            render->refresh();
            if (!render->setBackground("backgrounds/strand1.bmp"))
            {
                throw std::runtime_error("Stadium failed");
            }
            render->drawGame(match);
            save(*render, std::string(argv[2]) + "/assets-stadium.png");
            render->refresh();
            if (fs.exists("backgrounds/strand3.bmp") ||
                fs.exists(preferredImage("Levels/strand3/background.png")))
            {
                if (!render->setBackground("backgrounds/strand3.bmp"))
                {
                    throw std::runtime_error("Third beach failed");
                }
                render->drawGame(match);
                save(*render, std::string(argv[2]) + "/assets-beach3.png");
                render->refresh();
            }
            if (!render->setBackground("backgrounds/strand1.bmp"))
            {
                throw std::runtime_error("Stadium restore failed");
            }
            render->drawImage("background", Vector2(400, 300));
            render->drawImage("gfx/titel.bmp", Vector2(400, 150));
            render->drawImage("gfx/pokal.bmp", Vector2(160, 360));
            render->drawImage("gfx/flag.bmp", Vector2(350, 400));
            render->drawImage("gfx/cursor.bmp", Vector2(450, 400));
            render->drawImage("gfx/btn_play.bmp", Vector2(350, 500));
            render->drawImage("gfx/btn_pause.bmp", Vector2(400, 500));
            render->drawImage("gfx/btn_fast.bmp", Vector2(450, 500));
            render->drawImage("gfx/btn_slow.bmp", Vector2(500, 500));
            save(*render, std::string(argv[2]) + "/assets-ui.png");
            render->refresh();
            render->drawImage("background", Vector2(400, 300));
            render->drawOverlay(.85, Vector2(50, 70), Vector2(750, 530), Color(15, 25, 35));
            render->drawText("BLOBBY VOLLEY 2", Vector2(90, 100));
            render->drawText("GRAFIK AUDIO STEUERUNG SPIEL", Vector2(90, 160));
            render->drawText("0123456789 ABCDEFGHIJKLM", Vector2(90, 230));
            render->drawText("ABCDEFGHIJKLMNOPQRSTUVWXYZ", Vector2(90, 290), TF_HIGHLIGHT);
            render->drawText("ÄÖÜ ß !? +-%", Vector2(90, 350));
            render->drawText("KLEINE SCHRIFT", Vector2(90, 410), TF_SMALL_FONT);
            save(*render, std::string(argv[2]) + "/font-game.png");
            // A jumping left blob projects its shadow across the central pole.
            // Pole pixels must stay identical to the shadow-free frame while the
            // shadow remains visible on neighboring sand, for every background.
            match.worldState.blobPosition[LEFT_PLAYER] = Vector2(330, 300);
            for (int background = 1; background <= 3; ++background)
            {
                if (!fs.exists("backgrounds/strand" + std::to_string(background) + ".bmp") &&
                    !fs.exists(
                        preferredImage("Levels/strand" + std::to_string(background) + "/background.png")))
                {
                    continue;
                }
                if (!render->setBackground("backgrounds/strand" + std::to_string(background) + ".bmp"))
                {
                    throw std::runtime_error("Occlusion background failed");
                }
                const std::string base = std::string(argv[2]) + "/pole-" + std::to_string(background);
                render->refresh();
                render->showShadow(false);
                render->drawGame(match);
                save(*render, base + "-baseline.png");
                render->refresh();
                render->showShadow(true);
                render->drawGame(match);
                save(*render, base + "-shadow.png");
                auto* baseline = IMG_Load((base + "-baseline.png").c_str());
                auto* shadow = IMG_Load((base + "-shadow.png").c_str());
                if (!baseline || !shadow)
                {
                    throw std::runtime_error("Occlusion capture failed");
                }
                auto* before = SDL_ConvertSurfaceFormat(baseline, SDL_PIXELFORMAT_RGBA32, 0);
                auto* after = SDL_ConvertSurfaceFormat(shadow, SDL_PIXELFORMAT_RGBA32, 0);
                int changedSand = 0;
                for (int y = 475; y < 486; ++y)
                {
                    for (int x = 370; x < 406; ++x)
                    {
                        // 1920x1080 output: centered 1440x1080 game viewport.
                        int px = 240 + int((x + .5) * 1.8), py = int((y + .5) * 1.8);
                        auto a = reinterpret_cast<Uint32*>(static_cast<Uint8*>(before->pixels) +
                                                           py * before->pitch)[px];
                        auto b = reinterpret_cast<Uint32*>(static_cast<Uint8*>(after->pixels) +
                                                           py * after->pitch)[px];
                        if (x >= 395 && a != b)
                        {
                            throw std::runtime_error("Shadow covers net pole");
                        }
                        if (x < 385 && a != b)
                        {
                            ++changedSand;
                        }
                    }
                }
                SDL_FreeSurface(before);
                SDL_FreeSurface(after);
                SDL_FreeSurface(baseline);
                SDL_FreeSurface(shadow);
                if (!changedSand)
                {
                    throw std::runtime_error("Sand shadow disappeared");
                }
            }
        } // A failed image load leaves an empty cache slot. Renderer destruction
        // must remain safe while unwinding the original file error.
        for (const char* backend : {"SDL", "OpenGL"})
        {
            if (std::string(backend) == "OpenGL" && !SDL_getenv("BLOBBY_TEST_OPENGL"))
            {
                continue;
            }
            auto renderer = std::string(backend) == "SDL" ? RenderManager::createRenderManagerSDL()
                                                          : RenderManager::createRenderManagerGL2D();
            renderer->init(800, 600, false);
            SDL_HideWindow(renderer->getWindow());
            bool failed = false;
            try
            {
                renderer->drawImage("gfx/missing-regression-test.png", Vector2(400, 300), Vector2(100, 100));
            }
            catch (const std::exception&)
            {
                failed = true;
            }
            if (!failed)
            {
                throw std::runtime_error("Missing image did not report an error");
            }
            renderer.reset();
        }

        SDL_Quit();
        std::cout << "Assets, fixed ball lighting, PNG shadows, runtime Nunito TTF atlases and SDL rendering "
                     "passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
