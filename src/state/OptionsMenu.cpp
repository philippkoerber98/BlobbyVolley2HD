#include "BackgroundSettings.h"
#include "BlobbyApp.h"
#include "Blood.h"
#include "FileSystem.h"
#include "IMGUI.h"
#include "InputManager.h"
#include "OptionsMenu.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include "SpeedController.h"
#include "UITranslations.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

using namespace OptionsUI;

IMGUI& OptionState::Menu::gui()
{
    return owner.getIMGUI();
}

InputManager& OptionState::Menu::in()
{
    return owner.getInputMgr();
}

std::string OptionState::Menu::tr(const char* english) const
{
    return translateUI(config.getString("language"), english);
}

OptionState::Menu::Menu(OptionState& state) : owner(state)
{
    config.loadFile("config.xml");
    original.loadFile("config.xml");
    input.loadFile("inputconfig.xml");
    auto& fs = FileSystem::getSingleton();
    backgrounds = availableBackgroundNames();
    std::sort(backgrounds.begin(), backgrounds.end());
    rules = fs.enumerateFiles("rules", ".lua", true);
    std::sort(rules.begin(), rules.end());
    scripts = fs.enumerateFiles("scripts", ".lua", false);
    std::sort(scripts.begin(), scripts.end());
    scripts.insert(scripts.begin(), "Human");
}

OptionState::Menu::~Menu()
{
    if (capture == NAME)
    {
        SDL_StopTextInput();
    }
}

void OptionState::Menu::captureInput(Capture kind, const std::string& key)
{
    capture = kind;
    captureKey = key;
    captureAfter = SDL_GetTicks() + 180;
    dragging = -1;
    if (kind == NAME)
    {
        nameBefore = config.getString(key);
        SDL_StartTextInput();
    }
}

void OptionState::Menu::nameRow(float x, float y, int player)
{
    const auto key = sidePrefix(player) + "player_name";
    if (row(y, tr("Name"), config.getString(key), tr("Enter a name; Enter confirms, Esc cancels."), x, 350))
    {
        captureInput(NAME, key);
    }
}

void OptionState::Menu::apply()
{
    try
    {
        if (!config.saveFile("config.xml") || !input.saveFile("inputconfig.xml"))
        {
            notice = tr("Could not save settings.");
            return;
        }
        auto& app = owner.getApp();
        bool rebuild = config.getBool("fullscreen") != original.getBool("fullscreen") ||
                       config.getString("device") != original.getString("device") ||
                       config.getInteger("window_width", 800) != original.getInteger("window_width", 800) ||
                       config.getInteger("window_height", 600) != original.getInteger("window_height", 600);
        try
        {
            if (rebuild)
            {
                app.setupRenderManager(config);
            }
        }
        catch (const std::exception& e)
        {
            notice = e.what();
            return;
        }
        if (rebuild || config.getInteger("render_fps", 0) != original.getInteger("render_fps", 0))
        {
            app.setFrameLimit(config.getInteger("render_fps", 0));
        }
        app.getRenderManager().setCourtWidth(config.getBool("court_widescreen", false) ? 3200.f / 3.f
                                                                                       : 800.f);
        app.getRenderManager().showShadow(config.getBool("show_shadow"));
        app.getRenderManager().getBlood().enable(config.getBool("blood"));
        if (!rebuild && config.getString("background") != original.getString("background"))
        {
            if (!app.getRenderManager().setBackground("backgrounds/" +
                                                      config.getString("background", "strand1.bmp")))
            {
                throw std::runtime_error("Could not load selected background");
            }
        }
        app.getSoundManager().setVolume(config.getFloat("global_volume", 1));
        app.getSoundManager().setMute(config.getBool("mute"));
        if (auto* speed = SpeedController::getMainInstance())
        {
            speed->setDrawFPS(config.getBool("showfps"));
        }
        if (config.getString("language") != original.getString("language"))
        {
            gui().setTextMgr(config.getString("language", "en"));
        }
        original = config;
        dirty = false;
    }
    catch (const std::exception& e)
    {
        notice = tr("Could not update: ") + e.what();
    }
}

void OptionState::Menu::back()
{
    if (dirty && capture == NONE)
    {
        apply();
    }
    owner.switchState(new MainMenuState());
}

void OptionState::Menu::resetDefaults()
{
    try
    {
        UserConfig defaults, defaultInput;
        if (!defaults.loadFile("default-config.xml") || !defaultInput.loadFile("default-inputconfig.xml"))
        {
            throw std::runtime_error("Default settings are unavailable");
        }
        config = defaults;
        input = defaultInput;
        dirty = true;
        confirmReset = false;
        dragging = -1;
        notice = tr("Default settings restored.");
    }
    catch (const std::exception& e)
    {
        notice = e.what();
        confirmReset = false;
    }
}

void OptionState::Menu::captureDialog()
{
    gui().doOverlay(gui().getNextId(), Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .6);
    panel(145, 205, 510, 175);
    text(165, 220, capture == NAME ? tr("Edit name") : tr("New binding"), 28, 2);
    if (capture == NAME)
    {
        text(165, 270, config.getString(captureKey) + "_", 26);
        text(165, 326, tr("Enter: confirm     Esc: cancel"), 20);
    }
    else
    {
        text(165, 272, tr("Press a key, mouse button or joystick control."));
        text(165, 326, tr("Esc / B: cancel"), 20);
    }
}

void OptionState::Menu::processCapture()
{
    if (capture == NONE)
    {
        return;
    }
    if (SDL_GetTicks() < captureAfter)
    {
        return;
    }
    if (in().exit())
    {
        if (capture == NAME)
        {
            config.setString(captureKey, nameBefore);
            SDL_StopTextInput();
        }
        capture = NONE;
        return;
    }
    if (capture == NAME)
    {
        std::string value = config.getString(captureKey), key = in().getLastTextKey();
        if (key == "return")
        {
            capture = NONE;
            dirty |= value != nameBefore;
            SDL_StopTextInput();
        }
        else if (key == "backspace")
        {
            if (!value.empty())
            {
                size_t i = value.size() - 1;
                while (i > 0 && (static_cast<unsigned char>(value[i]) & 0xc0) == 0x80)
                {
                    --i;
                }
                value.erase(i);
            }
        }
        else if (key == "paste")
        {
            char* clip = SDL_GetClipboardText();
            if (clip)
            {
                value += clip;
                SDL_free(clip);
            }
        }
        else if (key != "del" && !key.empty() && key.size() < 16)
        {
            value += key;
        }
        size_t boundary = 0;
        int count = 0;
        while (boundary < value.size() && count < 15)
        {
            ++boundary;
            while (boundary < value.size() && (static_cast<unsigned char>(value[boundary]) & 0xc0) == 0x80)
            {
                ++boundary;
            }
            ++count;
        }
        value.resize(boundary);
        config.setString(captureKey, value);
    }
    else if (capture == KEYBOARD && !in().getLastActionKey().empty())
    {
        input.setString(captureKey, in().getLastActionKey());
        capture = NONE;
        dirty = true;
    }
    else if (capture == JOYSTICK && !in().getLastJoyAction().empty())
    {
        input.setString(captureKey, in().getLastJoyAction());
        capture = NONE;
        dirty = true;
    }
    else if (capture == MOUSE && in().getLastMouseButton() > 0)
    {
        input.setInteger(captureKey, in().getLastMouseButton());
        capture = NONE;
        dirty = true;
    }
}

void OptionState::Menu::step(bool interactive)
{
    mouse = in().position();
    mouseMoved = mouse != lastMouse;
    lastMouse = mouse;
    if (in().unclick())
    {
        dragging = -1;
    }
    bool hadCapture = capture != NONE;
    bool hadReset = confirmReset;
    suppressActions = !interactive || hadCapture || hadReset;
    if (interactive && hadCapture)
    {
        processCapture();
    }
    else if (interactive && !confirmReset)
    {
        if (in().previousTab())
        {
            category(tab - 1);
        }
        else if (in().nextTab())
        {
            category(tab + 1);
        }
        if (in().up())
        {
            focus = wrap(focus - 1, previousCount);
        }
        if (in().down())
        {
            focus = wrap(focus + 1, previousCount);
        }

        if (in().exit())
        {
            back();
        }
    }
    rowCount = 0;
    helpTitle = "";
    helpText = "";
    gui().doCursor();
    gui().doImage(gui().getNextId(), Vector2(400, 300),
                  "menu-backgrounds/" + config.getString("background", "strand1.bmp"), Vector2(800, 600));
    // Use exactly the main menu's default black overlay and opacity.
    gui().doOverlay(gui().getNextId(), Vector2(0, 0), Vector2(800, 600));
    text(40, 28, tr("Options"), 36);
    gui().doImage(gui().getNextId(), Vector2(700, 43), "gfx/titel.bmp", Vector2(120, 69));

    const char* english[] = {"Game", "Players", "Controls", "Graphics", "Audio"};
    float x = 40;
    for (int i = 0; i < 5; ++i)
    {
        std::string label = tr(english[i]);
        float w = RenderManager::menuTextWidth(label, 22) + 24;
        if (!suppressActions && capture == NONE && !confirmReset && hit(x, 87, w, 34) && in().click())
        {
            category(i);
        }
        panel(x, 87, w, 34, i == tab ? accent : surface);
        text(x + 12, 93, label, 22, i == tab ? 1 : 0);
        x += w + 6;
    }
    if (tab == 0)
    {
        game();
    }
    else if (tab == 1)
    {
        players();
    }
    else if (tab == 2)
    {
        controls();
    }
    else if (tab == 3)
    {
        graphics();
    }
    else
    {
        audio();
    }
    if (tab != 1 && !helpText.empty())
    {
        panel(410, 140, 350, 210);
        text(425, 152, helpTitle, 27, 2);
        std::istringstream words(helpText);
        std::string word, line;
        float y = 188;
        while (words >> word)
        {
            if (RenderManager::menuTextWidth(line + " " + word, 22) > 320 && !line.empty())
            {
                text(425, y, line);
                line = word;
                y += 20;
            }
            else
            {
                if (!line.empty())
                {
                    line += " ";
                }
                line += word;
            }
        }
        if (!line.empty())
        {
            text(425, y, line);
        }
    }
    text(40, 546, tr("D-pad: select / change    A: edit"), 17);
    if (button(655, 530, 105, "B  " + tr("Back")))
    {
        back();
    }
    if (button(463, 530, 182, tr("Restore defaults")))
    {
        confirmReset = true;
    }
    text(40, 578, tr("LB / RB: category    Page Up / Down: category    Changes apply immediately"), 16);
    if (!notice.empty())
    {
        text(40, 516, notice, 17, 2);
    }
    previousCount = rowCount;
    if (capture != NONE)
    {
        captureDialog();
    }
    if (confirmReset)
    {
        gui().doOverlay(gui().getNextId(), Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .65);
        panel(145, 215, 510, 140);
        text(165, 231, tr("Restore default settings?"), 28, 2);
        text(165, 271, tr("A / Enter: reset     B / Esc: cancel"), 21);
        if (hadReset && (in().select() || (in().click() && hit(165, 304, 210, 34))))
        {
            resetDefaults();
        }
        if (hadReset && (in().exit() || (in().click() && hit(395, 304, 230, 34))))
        {
            confirmReset = false;
        }
        panel(165, 304, 210, 34);
        text(180, 310, tr("Reset"));
        panel(395, 304, 230, 34);
        text(410, 310, tr("Cancel"));
    }
    if (dirty && capture == NONE && !confirmReset)
    {
        apply();
    }
}
