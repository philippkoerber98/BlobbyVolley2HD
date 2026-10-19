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

void OptionState::Menu::text(float x, float y, const std::string& s, float height, int style)
{
    if (y >= 546 && x == 40)
    {
        height = (std::min)(height, height * 720 / (std::max)(1.f, RenderManager::menuTextWidth(s, height)));
    }
    gui().doMenuText(gui().getNextId(), Vector2(x, y), s, height, style);
}

void OptionState::Menu::rightText(float x, float y, const std::string& s, float height, int style)
{
    text(x - RenderManager::menuTextWidth(s, height), y, s, height, style);
}

void OptionState::Menu::panel(float x, float y, float w, float h, Color col, float opacity)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    if (w < 8 || h < 8)
    {
        gui().doOverlay(gui().getNextId(), Vector2(x, y), Vector2(x + w, y + h), col, opacity);
    }
    else
    {
        gui().doRoundedPanel(gui().getNextId(), Vector2(x, y), Vector2(x + w, y + h), col, opacity);
    }
}

bool OptionState::Menu::hit(float x, float y, float w, float h) const
{
    return mouse.x >= x && mouse.x < x + w && mouse.y >= y && mouse.y < y + h;
}

void OptionState::Menu::category(int next)
{
    tab = wrap(next, 4);
    focus = 0;
    previousCount = 0;
    dragging = -1;
}

int OptionState::Menu::widget(float x, float y, float w, float h, const std::string& title,
                              const std::string& explanation)
{
    const int id = rowCount++;
    const bool hover = hit(x, y, w, h);
    if (!suppressActions && capture == NONE && !confirmReset && hover && (mouseMoved || in().click()))
    {
        focus = id;
    }
    if (id == focus)
    {
        helpTitle = title;
        helpText = explanation;
    }
    return id;
}

int OptionState::Menu::action(int id, float x, float y, float w, float h)
{
    if (capture != NONE || confirmReset || suppressActions)
    {
        return 0;
    }
    if (hit(x, y, w, h) && in().click())
    {
        return -1 * (SDL_GetModState() & KMOD_SHIFT ? 1 : -1);
    }
    if (id != focus)
    {
        return 0;
    }
    if (in().left())
    {
        return -1;
    }
    if (in().right() || in().select())
    {
        return 1;
    }
    return 0;
}

int OptionState::Menu::row(float y, const std::string& label, const std::string& value,
                           const std::string& explanation, float x, float w)
{
    int id = widget(x, y, w, 34, label, explanation);
    if (focus == id)
    {
        panel(x - 1, y - 1, w + 2, 36, accent);
    }
    panel(x, y, w, 34, id == focus ? focusSurface : surface);
    text(x + 12, y + 6, label);
    rightText(x + w - 12, y + 6, value, 21, id == focus ? 2 : 0);
    return action(id, x, y, w, 34);
}

void OptionState::Menu::integer(float y, const std::string& label, UserConfig& c, const std::string& key,
                                int lo, int hi, int fallback, const std::string& explanation, float x,
                                float w)
{
    int value = c.getInteger(key, fallback),
        change = row(y, label, "<  " + std::to_string(value) + "  >", explanation, x, w);
    if (change)
    {
        c.setInteger(key, std::max(lo, std::min(hi, value + change)));
        dirty = true;
    }
}

void OptionState::Menu::toggle(float y, const std::string& label, UserConfig& c, const std::string& key,
                               const std::string& explanation, float x, float w)
{
    if (row(y, label, c.getBool(key) ? tr("On") : tr("Off"), explanation, x, w))
    {
        c.setBool(key, !c.getBool(key));
        dirty = true;
    }
}

void OptionState::Menu::choice(float y, const std::string& label, UserConfig& c, const std::string& key,
                               const std::vector<std::string>& values, const std::vector<std::string>& names,
                               const std::string& explanation, float x, float w)
{
    if (values.empty())
    {
        return;
    }
    auto found = std::find(values.begin(), values.end(), c.getString(key));
    int index = found == values.end() ? 0 : int(found - values.begin());
    int delta = row(y, label, "<  " + names[index] + "  >", explanation, x, w);
    if (delta)
    {
        c.setString(key, values[wrap(index + delta, int(values.size()))]);
        dirty = true;
    }
}

bool OptionState::Menu::slider(float x, float y, float w, const std::string& label, float& value,
                               const std::string& display, const std::string& explanation, Color colour,
                               float step, float height)
{
    const int id = widget(x, y, w, height, label, explanation);
    if (id == focus)
    {
        panel(x - 1, y - 1, w + 2, height + 2, accent);
    }
    panel(x, y, w, height, id == focus ? focusSurface : surface);
    text(x + 12, y + 2, label);
    rightText(x + w - 12, y + 2, display, 22, 2);
    float next = value;
    if (capture == NONE && !confirmReset && !suppressActions)
    {
        if (hit(x + 8, y + 24, w - 16, 22) && in().click())
        {
            dragging = id;
        }
        if (dragging == id)
        {
            next = (mouse.x - x - 12) / (w - 24);
        }
        else if (focus == id)
        {
            next += in().sliderDirection() * step;
        }
    }
    next = std::max(0.f, std::min(1.f, next));
    bool changed = std::abs(next - value) > .00001f;
    if (changed)
    {
        value = next;
        dirty = true;
    }
    panel(x + 12, y + 32, w - 24, 4, Color(132, 108, 73), 1);
    if (value > 0)
    {
        panel(x + 12, y + 32, (w - 24) * value, 4, colour, 1);
    }
    panel(x + 8 + (w - 24) * value, y + 28, 8, 12, Color(255, 245, 220), 1);
    return changed;
}

bool OptionState::Menu::button(float x, float y, float w, const std::string& label, bool primary)
{
    int id = widget(x, y, w, 34, label, "");
    if (focus == id)
    {
        panel(x - 1, y - 1, w + 2, 36, accent);
    }
    panel(x, y, w, 34, primary ? accent : surface);
    const float size =
        (std::min)(21.f, 21.f * (w - 12) / (std::max)(1.f, RenderManager::menuTextWidth(label, 21)));
    text(x + (w - RenderManager::menuTextWidth(label, size)) / 2, y + (34 - size) / 2, label, size,
         primary ? 1 : 0);
    return capture == NONE && !confirmReset && !suppressActions &&
           ((hit(x, y, w, 34) && in().click()) || (focus == id && in().select()));
}

void OptionState::Menu::section(float y, const std::string& label, float x)
{
    text(x, y, label, 16);
}
