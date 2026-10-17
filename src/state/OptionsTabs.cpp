#include "OptionsMenu.h"
#include "BlobbyApp.h"
#include "Blood.h"
#include "BackgroundSettings.h"
#include "FileSystem.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include "SpeedController.h"
#include "UITranslations.h"
#include <algorithm>
#include <cmath>
#include <sstream>

using namespace OptionsUI;

namespace OptionsUI
{
// Sand, sunshine and cream echo the beach without competing with the logo.
extern const Color surface(65, 45, 29), accent(255, 207, 91), focusSurface(104, 75, 43);
int wrap(int value, int count)
{
    return count ? (value % count + count) % count : 0;
}
std::string sidePrefix(int player)
{
    return player ? "right_" : "left_";
}
std::string inputPrefix(int player)
{
    return sidePrefix(player) + "blobby_";
}
} // namespace OptionsUI

// One settings session owns every tab. Completed edits are applied immediately.
void OptionState::Menu::game()
{
    section(128, tr("GAME RULES"));
    choice(148, tr("Rules"), config, "rules", rules, rules, tr("Rules for the next match."));
    integer(188, tr("Points to win"), config, "scoretowin", 1, 99, 15, tr("Points needed to win."));
    int speed = std::max(30, std::min(120, config.getInteger("gamefps", 75)));
    float value = (speed - 30) / 90.f;
    if (slider(40, 228, 350, tr("Game speed"), value,
               std::to_string(int(std::lround(speed * 100.f / 75))) + " %",
               tr("100% is the original game speed. Rendering FPS does not affect it."), accent, 1.f / 90))
    {
        config.setInteger("gamefps", int(std::lround(value * 90 + 30)));
    }
    const int speeds[] = {30, 60, 75, 90, 120};
    const char* labels[] = {"Very slow", "Slow", "Normal", "Fast", "Very fast"};
    for (int i = 0; i < 5; ++i)
    {
        if (button(40 + i * 71, 276, 66, tr(labels[i]), config.getInteger("gamefps", 75) == speeds[i]))
        {
            config.setInteger("gamefps", speeds[i]);
            dirty = true;
        }
    }
    section(322, tr("GENERAL"));
    std::vector<std::string> languages, names;
    for (auto& lang : TextManager::languageNames())
    {
        languages.push_back(lang.first);
        names.push_back(lang.second);
    }
    choice(342, tr("Language"), config, "language", languages, names,
           tr("The menu language changes immediately."));
    choice(382, tr("Network side"), config, "network_side", {"0", "1"}, {tr("Left"), tr("Right")},
           tr("Preferred side in network matches."));
    toggle(422, tr("Show FPS"), config, "showfps", tr("Display the current rendering FPS."));
    toggle(462, tr("Blood"), config, "blood", tr("Enable or disable blood particles."));
}
void OptionState::Menu::players()
{
    for (int player = 0; player < 2; ++player)
    {
        float x = 40 + player * 370;
        std::string pre = sidePrefix(player);
        section(128, tr("PLAYER ") + std::to_string(player + 1), x);
        nameRow(x, 148, player);
        std::string script =
            config.getBool(pre + "player_human", true) ? "Human" : config.getString(pre + "script_name");
        auto it = std::find(scripts.begin(), scripts.end(), script);
        int index = it == scripts.end() ? 0 : int(it - scripts.begin());
        if (int delta =
                row(188, tr("Player type"), index ? scripts[index] : tr("Human"),
                    tr("Each bot uses its widescreen variant in 16:9. Original 4:3 scripts stay unchanged."),
                    x, 350))
        {
            index = wrap(index + delta, int(scripts.size()));
            config.setBool(pre + "player_human", index == 0);
            if (index)
            {
                config.setString(pre + "script_name", scripts[index]);
            }
            dirty = true;
        }

        int delay = config.getInteger(pre + "script_strength");
        float strength = 1 - std::max(0, std::min(25, delay)) / 25.f;
        if (slider(x, 228, 350, tr("Bot strength"), strength,
                   std::to_string(int(std::lround(strength * 100))) + " %",
                   tr("Reaction strength of the selected bot."), accent, .01f, 42))
        {
            config.setInteger(pre + "script_strength", int(std::lround((1 - strength) * 25)));
        }
        const char* channels[] = {"r", "g", "b"};

        const char* english[] = {"Red", "Green", "Blue"};
        Color colors[] = {Color(238, 124, 121), Color(173, 228, 106), Color(115, 197, 242)};
        for (int j = 0; j < 3; ++j)
        {
            auto key = pre + "blobby_color_" + channels[j];
            int c = config.getInteger(key, player ? 255 : 128);
            float v = c / 255.f;
            if (slider(x, 278 + j * 48, 350, tr(english[j]), v, std::to_string(c),
                       tr("Blob RGB colour. The preview updates immediately."), colors[j], 1.f / 255, 42))
            {
                config.setInteger(key, int(std::lround(v * 255)));
            }
        }
        toggle(424, tr("Colour cycling"), config, pre + "blobby_oscillate",
               tr("Cycle the blob colour automatically."), x, 350);
        Color col(config.getInteger(pre + "blobby_color_r"), config.getInteger(pre + "blobby_color_g"),
                  config.getInteger(pre + "blobby_color_b"));
        if (config.getBool(pre + "blobby_oscillate"))
        {
            col = owner.getApp().getRenderManager().getOscillationColor();
        }
        gui().doBlob(gui().getNextId(), Vector2(x + 175, 496), col, .5f);
    }
}
void OptionState::Menu::controls()
{
    section(128, tr("DEVICES"));
    const std::vector<std::string> devices = {"gamepad", "keyboard", "mouse", "joystick"};
    const std::vector<std::string> names = {tr("Controller"), tr("Keyboard"), tr("Mouse"), tr("Joystick")};
    for (int i = 0; i < 2; ++i)
    {
        const std::string key = inputPrefix(i) + "device", old = input.getString(key);
        choice(148 + i * 40, tr("Player device ") + std::to_string(i + 1), input, key, devices, names,
               tr("Select each player's input device independently."));
        if (input.getString(key) == "mouse" && input.getString(inputPrefix(1 - i) + "device") == "mouse")
        {
            input.setString(key, old);
            notice = tr("Only one player can use the mouse.");
        }
    }
    if (row(228, tr("Bindings for"), tr("Player ") + std::to_string(inputPlayer + 1),
            tr("Edit this player's bindings.")))
    {
        inputPlayer = 1 - inputPlayer;
    }
    const std::string prefix = inputPrefix(inputPlayer), device = input.getString(prefix + "device");
    if (device == "gamepad")
    {
        integer(268, tr("Controller"), input, prefix + "controller_slot", 0, 7, inputPlayer,
                tr("Controller slot: 0 is the first controller."));
        const int jumps[] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y};
        const char* names[] = {"A", "X", "Y"};
        int current = input.getInteger(prefix + "controller_jump", 0), j = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (jumps[i] == current)
            {
                j = i;
            }
        }
        if (int delta = row(308, tr("Jump"), names[j], tr("B remains reserved for Back.")))
        {
            input.setInteger(prefix + "controller_jump", jumps[wrap(j + delta, 3)]);
            dirty = true;
        }
        float dz = input.getInteger(prefix + "controller_deadzone", 10000) / 32767.f;
        if (slider(40, 348, 350, tr("Stick deadzone"), dz, std::to_string(int(std::lround(dz * 100))) + " %",
                   tr("Increase for stick drift. Lower values respond sooner."), accent, .01f))
        {
            input.setInteger(prefix + "controller_deadzone", std::min(30000, int(dz * 32767)));
        }
        row(396, tr("Move"), "Stick / D-Pad", tr("Move using the left stick or D-pad."));
        row(436, tr("Pause"), "Start", tr("Start opens the pause menu."));
    }
    else if (device == "keyboard" || device == "joystick")
    {
        const char* actions[] = {"left", "right", "jump"};

        const char* enLabels[] = {"Move left", "Move right", "Jump"};
        for (int j = 0; j < 3; ++j)
        {
            auto key = prefix + device + "_" + actions[j];
            std::string value = input.getString(key);
            if (value.size() > 26)
            {
                value = value.substr(0, 25) + "...";
            }
            if (row(268 + j * 40, tr(enLabels[j]), value,
                    tr("Select, then press the desired key, button or axis.")))
            {
                captureInput(device == "keyboard" ? KEYBOARD : JOYSTICK, key);
            }
        }
    }
    else if (device == "mouse")
    {
        if (row(268, tr("Jump"),
                tr("Button ") + std::to_string(input.getInteger(prefix + "mouse_jumpbutton", 1)),
                tr("Select, then press a mouse button.")))
        {
            captureInput(MOUSE, prefix + "mouse_jumpbutton");
        }
        float sensitivity = input.getFloat(prefix + "mouse_sensitivity", 1);
        if (slider(40, 308, 350, tr("Sensitivity"), sensitivity,
                   std::to_string(int(std::lround(sensitivity * 100))) + " %",
                   tr("Mouse movement sensitivity.")))
        {
            input.setFloat(prefix + "mouse_sensitivity", sensitivity);
        }
    }
    else
    {
        row(268, tr("Touch controls"), tr("Unchanged"), tr("Existing touch configuration is preserved."));
    }
}
void OptionState::Menu::graphics()
{
    section(128, tr("DISPLAY"));
    choice(148, tr("Display mode"), config, "fullscreen", {"false", "true"}, {tr("Window"), tr("Fullscreen")},
           tr("Fullscreen uses desktop mode. Windowed mode keeps its title bar."));
    const auto& display = owner.getApp().displaySettings();
    std::vector<int> caps{0};
    caps.insert(caps.end(), display.frameLimits.begin(), display.frameLimits.end());
    int fps = config.getInteger("render_fps", 0);
    auto cap = std::find(caps.begin(), caps.end(), fps);
    int idx = cap == caps.end() ? 0 : int(cap - caps.begin());
    if (int delta = row(188, tr("Frame rate"), fps == 0 ? tr("Automatic") : std::to_string(fps) + " FPS",
                        tr("Automatic follows the monitor. Game speed stays independent.")))
    {
        config.setInteger("render_fps", caps[wrap(idx + delta, int(caps.size()))]);
        dirty = true;
    }
    if (!config.getBool("fullscreen"))
    {
        const auto& sizes = display.windowSizes;
        int w = config.getInteger("window_width", 800), h = config.getInteger("window_height", 600);
        int index = 0;
        for (size_t i = 0; i < sizes.size(); ++i)
        {
            if (sizes[i].width == w && sizes[i].height == h)
            {
                index = int(i);
            }
        }
        if (int delta = row(228, tr("Window size"), std::to_string(w) + " x " + std::to_string(h),
                            tr("Only sizes that fit the current monitor.")))
        {
            if (!sizes.empty())
            {
                auto size = sizes[wrap(index + delta, int(sizes.size()))];
                config.setInteger("window_width", size.width);
                config.setInteger("window_height", size.height);
                dirty = true;
            }
        }
    }
    else
    {
        row(228, tr("Resolution"),
            std::to_string(display.desktopWidth) + " x " + std::to_string(display.desktopHeight),
            tr("Fullscreen uses the desktop resolution."));
    }
    section(272, tr("RENDERING"));
    toggle(292, tr("Shadows"), config, "show_shadow", tr("Display ball and blob shadows."));
    const std::map<std::string, std::string> backgroundTitles = {{"aurora.bmp", "Aurora"},
                                                                 {"beach.bmp", "Rocky beach"},
                                                                 {"beacharena.bmp", "Beach arena"},
                                                                 {"berge.bmp", "Mountain coast"},
                                                                 {"berge2.bmp", "Alpine lake"},
                                                                 {"burg.bmp", "Castle"},
                                                                 {"landhaus.bmp", "Country house"},
                                                                 {"mars.bmp", "Mars"},
                                                                 {"mars2.bmp", "Mars colony"},
                                                                 {"mond.bmp", "Moon"},
                                                                 {"oase.bmp", "Oasis"},
                                                                 {"saturn.bmp", "Saturn"},
                                                                 {"schnee.bmp", "Snow"},
                                                                 {"sportanlage.bmp", "Sports hall"},
                                                                 {"steppe.bmp", "Meadow"},
                                                                 {"strand1-classic.bmp", "Classic beach"},
                                                                 {"strand1.bmp", "Beach 1"},
                                                                 {"strand2.bmp", "Beach 2"},
                                                                 {"strand3.bmp", "Beach 3"},
                                                                 {"street.bmp", "Road"},
                                                                 {"tempel.bmp", "Temple"},
                                                                 {"tropen.bmp", "Tropical resort"},
                                                                 {"volleyfeld.bmp", "Volleyball court"},
                                                                 {"woizn.bmp", "Wheat field"},
                                                                 {"wolken.bmp", "Clouds"}};
    std::vector<std::string> backgroundLabels;
    for (const auto& name : backgrounds)
    {
        auto title = backgroundTitles.find(name);
        backgroundLabels.push_back(title == backgroundTitles.end()
                                       ? (name.compare(0, 7, "custom:") == 0
                                              ? name.substr(7) + " (" + tr("Custom") + ")"
                                              : name.substr(0, name.size() - 4))
                                       : translateUI(config.getString("language"), title->second));
    }
    choice(332, tr("Level"), config, "background", backgrounds, backgroundLabels,
           tr("Select a level with its background and sounds."));
#if HAVE_LIBGL
    choice(372, tr("Renderer"), config, "device", {"OpenGL", "SDL"}, {"OpenGL", "SDL"},
           tr("The renderer switches immediately."));
#endif
    if (row(412, tr("Court"),
            config.getBool("court_widescreen", false) ? tr("16:9 Widescreen") : tr("4:3 Classic"),
            tr("4:3: classic view and court. 16:9: wider court and longer runs, 20% faster running. "
               "Network stays 4:3. From next match.")))
    {
        config.setBool("court_widescreen", !config.getBool("court_widescreen", false));
        if (!config.getBool("fullscreen"))
        {
            config.setInteger(
                "window_width",
                int(std::lround(config.getInteger("window_height", 600) *
                                (config.getBool("court_widescreen") ? 16.f / 9.f : 4.f / 3.f))));
        }
        dirty = true;
    }
}
void OptionState::Menu::audio()
{
    section(128, tr("VOLUME"));
    float volume = config.getFloat("global_volume", 1);
    if (slider(40, 148, 350, tr("Master volume"), volume,
               std::to_string(int(std::lround(volume * 100))) + " %", tr("Volume of all game sounds.")))
    {
        config.setFloat("global_volume", volume);
    }
    float effects = config.getFloat("effects_volume", 1);
    if (slider(40, 198, 350, tr("Effects volume"), effects,
               std::to_string(int(std::lround(effects * 100))) + " %",
               tr("Volume of impacts, whistles, chat and victory sounds.")))
    {
        config.setFloat("effects_volume", effects);
    }
    float music = config.getFloat("music_volume", 1);
    if (slider(40, 248, 350, tr("Music / background"), music,
               std::to_string(int(std::lround(music * 100))) + " %",
               tr("Volume of menu and level music or background noise.")))
    {
        config.setFloat("music_volume", music);
    }
    toggle(302, tr("Mute"), config, "mute", tr("Mute all game sounds."));
    if (button(40, 346, 180, tr("Test sound")))
    {
        owner.playSound(SoundManager::IMPACT, 1);
    }
}
