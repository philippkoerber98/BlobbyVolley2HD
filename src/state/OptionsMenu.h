#pragma once

#include "OptionsState.h"
#include "Color.h"
#include "UserConfig.h"
#include "Vector.h"
#include <SDL.h>
#include <string>
#include <vector>

class IMGUI;
class InputManager;

namespace OptionsUI
{
extern const Color surface, accent, focusSurface;
int wrap(int, int);
std::string sidePrefix(int);
std::string inputPrefix(int);
} // namespace OptionsUI

struct OptionState::Menu
{
    OptionState& owner;
    UserConfig config, input, original;
    int tab = 0, focus = 0, previousCount = 0, rowCount = 0, dragging = -1, inputPlayer = 0;
    bool dirty = false, confirmReset = false, suppressActions = false;
    std::string notice, helpTitle, helpText, captureKey;
    enum Capture
    {
        NONE,
        NAME,
        KEYBOARD,
        JOYSTICK,
        MOUSE
    } capture = NONE;
    Uint32 captureAfter = 0;
    Vector2 mouse, lastMouse = Vector2(-1, -1);
    bool mouseMoved = false;
    std::vector<std::string> backgrounds, rules, scripts;
    IMGUI& gui();
    InputManager& in();
    std::string tr(const char* english) const;

    explicit Menu(OptionState& state);
    ~Menu();

    void text(float x, float y, const std::string& s, float height = 22, int style = 0);
    void rightText(float x, float y, const std::string& s, float height = 22, int style = 0);
    void panel(float x, float y, float w, float h, Color col = OptionsUI::surface, float opacity = .88f);
    bool hit(float x, float y, float w, float h) const;
    void category(int next);
    int widget(float x, float y, float w, float h, const std::string& title, const std::string& explanation);
    int action(int id, float x, float y, float w, float h);
    int row(float y, const std::string& label, const std::string& value, const std::string& explanation,
            float x = 40, float w = 350);
    void integer(float y, const std::string& label, UserConfig& c, const std::string& key, int lo, int hi,
                 int fallback, const std::string& explanation, float x = 40, float w = 350);
    void toggle(float y, const std::string& label, UserConfig& c, const std::string& key,
                const std::string& explanation, float x = 40, float w = 350);
    void choice(float y, const std::string& label, UserConfig& c, const std::string& key,
                const std::vector<std::string>& values, const std::vector<std::string>& names,
                const std::string& explanation, float x = 40, float w = 350);
    bool slider(float x, float y, float w, const std::string& label, float& value, const std::string& display,
                const std::string& explanation, Color colour = OptionsUI::accent, float step = .01f,
                float height = 42);
    bool button(float x, float y, float w, const std::string& label, bool primary = false);
    void section(float y, const std::string& label, float x = 40);

    void captureInput(Capture kind, const std::string& key);

    void game();
    void controls();
    void graphics();
    void audio();
    void apply();
    void back();
    void resetDefaults();
    void captureDialog();
    std::string nameBefore;
    void processCapture();
    void step(bool interactive = true);
};
