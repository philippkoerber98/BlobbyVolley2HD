#pragma once
#include "IMGUI.h"
#include "UITranslations.h"
#include "UserConfig.h"
#include <algorithm>

// Shared presentation and navigation for the main menu, HUD and match dialogs.
namespace WarmUI
{
inline std::string& language()
{
    static std::string value = "en";
    return value;
}
inline std::string tr(const char* de, const char* en)
{
    return translateUI(language(), en);
}
inline void text(IMGUI& g, float x, float y, const std::string& s, float h = 22, int style = 0, int align = 0)
{
    if (align)
    {
        x -= RenderManager::menuTextWidth(s, h) * (align == 1 ? .5f : 1.f);
    }
    g.doMenuText(g.getNextId(), Vector2(x, y), s, h, style);
}
inline void panel(IMGUI& g, float x, float y, float w, float h, Color c = Color(65, 45, 29),
                  float alpha = .9f)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }
    if (w < 8 || h < 8)
    {
        g.doOverlay(g.getNextId(), Vector2(x, y), Vector2(x + w, y + h), c, alpha);
    }
    else
    {
        g.doRoundedPanel(g.getNextId(), Vector2(x, y), Vector2(x + w, y + h), c, alpha);
    }
}

inline bool hit(Vector2 p, float x, float y, float w, float h)
{
    return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
}
struct Navigation
{
    int focus = 0;
    Vector2 previous = Vector2(-1, -1), mouse;
    bool moved = false, active = true;
    void begin(InputManager& in, int count, bool enabled = true)
    {
        active = enabled;
        mouse = in.position();
        moved = mouse != previous;
        previous = mouse;
        if (!active)
        {
            return;
        }
        if (in.up() || in.left())
        {
            focus = (focus + count - 1) % count;
        }
        if (in.down() || in.right())
        {
            focus = (focus + 1) % count;
        }
    }
    bool button(IMGUI& g, InputManager& in, int index, float x, float y, float w, float h,
                const std::string& label, bool flat = false, bool left = false)
    {
        bool hover = hit(mouse, x, y, w, h);
        if (active && hover && (moved || in.click()))
        {
            focus = index;
        }
        bool selected = focus == index;
        if (!flat || selected)
        {
            panel(g, x, y, w, h,
                  selected ? Color(255, 207, 91) : (left ? Color(65, 45, 29) : Color(89, 63, 39)),
                  selected ? 1.f : .9f);
        }
        const float textHeight = left ? 22.f : 25.f;
        text(g, left ? x + 12 : x + w / 2, y + (h - textHeight) / 2, label, textHeight, selected ? 1 : 0,
             left ? 0 : 1);
        return active && ((hover && in.click()) || (selected && in.select()));
    }
};
inline void shade(IMGUI& g, const std::string& key)
{
    g.doSceneBlur(g.getNextId(), key);
    g.doOverlay(g.getNextId(), Vector2(0, 0), Vector2(800, 600));
}
inline void page(IMGUI& g, const std::string& title, const std::string& background = "")
{
    g.useMenuStyle();
    g.doInactiveMode(false);
    g.doCursor();
    UserConfig config;
    config.loadFile("config.xml");
    language() = config.getString("language", "en");
    g.doImage(g.getNextId(), Vector2(400, 300),
              "menu-backgrounds/" + (background.empty() ? config.getString("background", "strand1.bmp") : background), Vector2(800, 600));
    g.doOverlay(g.getNextId(), Vector2(0, 0), Vector2(800, 600));
    const auto titleHeight =
        (std::min)(36.f, 36.f * 550 / (std::max)(1.f, RenderManager::menuTextWidth(title, 36)));
    text(g, 40, 28, title, titleHeight);
    g.doImage(g.getNextId(), Vector2(700, 43), "gfx/titel.bmp", Vector2(120, 69));
}
} // namespace WarmUI
