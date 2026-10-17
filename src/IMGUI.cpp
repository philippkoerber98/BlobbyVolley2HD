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

/* header include */
#include "IMGUI.h"

/* includes */
#include <cassert>
#include <queue>

#include <SDL.h>

/* implementation */

constexpr unsigned MenuStyle = 1u << 30;

enum ObjectType
{
    IMAGE,
    OVERLAY,
    TEXT,
    MENU_TEXT,
    SCENE_BLUR,
    CONFETTI,
    ROUNDED_PANEL,
    BUTTON, // Unused!
    SCROLLBAR,
    ACTIVESCROLLBAR,
    EDITBOX,
    ACTIVEEDITBOX,
    SELECTBOX,
    ACTIVESELECTBOX,
    BLOB,
    CHAMPION,
    CHAT,
    ACTIVECHAT
};

struct QueueObject
{
    ObjectType type;
    int id;
    Vector2 pos1;
    Vector2 pos2;
    Color col;
    float alpha;
    std::string text;
    std::vector<std::string> entries;
    int selected;
    int length;
    unsigned int flags;
};

IMGUI::IMGUI(InputManager* inputMgr) : mInputManager(inputMgr)
{
    assert(mInputManager);
    mQueue.reset(new RenderQueue);
    mActiveButton = -1;
    mHeldWidget = 0;
    mLastKeyAction = KeyAction::NONE;
    mLastWidget = 0;
    mButtonReset = false;
    mInactive = false;
    mIdCounter = 0;
    mDrawCursor = false;
}

IMGUI::~IMGUI() = default;

void IMGUI::begin()
{
    mMenuStyle = false;
    mHasSceneBlur = false;
    mDrawCursor = false;
    mUsingCursor = false;
    mButtonReset = false;

    while (!mQueue->empty())
    {
        mQueue->pop();
    }

    mLastKeyAction = KeyAction::NONE;

    if (mInputManager->up())
    {
        mLastKeyAction = KeyAction::UP;
    }

    if (mInputManager->down())
    {
        mLastKeyAction = KeyAction::DOWN;
    }

    if (mInputManager->left())
    {
        mLastKeyAction = KeyAction::LEFT;
    }

    if (mInputManager->right())
    {
        mLastKeyAction = KeyAction::RIGHT;
    }

    if (mInputManager->select())
    {
        mLastKeyAction = KeyAction::SELECT;
    }

    if (mInputManager->exit())
    {
        mLastKeyAction = KeyAction::BACK;
    }

    mIdCounter = 0;
}

void IMGUI::end(RenderManager& renderer)
{
    int FontSize;
    // Keep the last simulation tick's UI for additional render frames.
    if (!mHasSceneBlur)
    {
        renderer.resetSceneBlur();
    }
    auto renderQueue = *mQueue;
    while (!renderQueue.empty())
    {
        QueueObject& obj = renderQueue.front();
        const float offset = (renderer.courtWidth() - 800.f) / 2;
        if (obj.type != SCENE_BLUR && obj.type != CONFETTI)
        {
            const bool fullLayer =
                (obj.type == OVERLAY && obj.pos1 == Vector2(0, 0) && obj.pos2 == Vector2(800, 600));
            obj.pos1.x += offset;
            if (obj.type == OVERLAY || obj.type == ROUNDED_PANEL || obj.type == CHAT ||
                obj.type == ACTIVECHAT || obj.type == SELECTBOX || obj.type == ACTIVESELECTBOX)
            {
                obj.pos2.x += offset;
            }
            if (fullLayer)
            {
                obj.pos1.x = 0;
                obj.pos2.x = renderer.courtWidth();
            }
            if (obj.type == IMAGE &&
                (obj.text == "background" || obj.text.compare(0, 17, "menu-backgrounds/") == 0))
            {
                obj.pos1.x = renderer.courtWidth() / 2;
                obj.pos2 = Vector2(renderer.courtWidth(), 600);
            }
        }
        switch (obj.type)
        {
        case CONFETTI:
            renderer.drawConfetti(obj.flags);
            break;
        case CHAMPION:
            renderer.drawChampion(obj.pos1, obj.col, obj.alpha);
            break;
        case SCENE_BLUR:
            renderer.drawSceneBlur(obj.text);
            break;
        case IMAGE:
            renderer.drawImage(obj.text, obj.pos1, obj.pos2);
            break;

        case ROUNDED_PANEL:
            renderer.drawRoundedPanel(obj.alpha, obj.pos1, obj.pos2, obj.col);
            break;
        case OVERLAY:
            renderer.drawOverlay(obj.alpha, obj.pos1, obj.pos2, obj.col);
            break;

        case TEXT:
            renderer.drawText(obj.text, obj.pos1, obj.flags);
            break;
        case MENU_TEXT:
            renderer.drawMenuText(obj.text, obj.pos1, obj.pos2.x, obj.flags);
            break;

        case SCROLLBAR:
            renderer.drawOverlay(0.5, obj.pos1, obj.pos1 + Vector2(210.0, 26.0));
            renderer.drawImage("gfx/scrollbar.bmp", obj.pos1 + Vector2(obj.pos2.x * 200.0 + 5, 13));
            break;

        case ACTIVESCROLLBAR:
            renderer.drawOverlay(0.4, obj.pos1, obj.pos1 + Vector2(210.0, 26.0));
            renderer.drawImage("gfx/scrollbar.bmp", obj.pos1 + Vector2(obj.pos2.x * 200.0 + 5, 13));
            break;

        case EDITBOX:
        case ACTIVEEDITBOX:
            FontSize = (obj.flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL);
            if (obj.flags & MenuStyle)
            {
                const auto end = obj.pos1 + Vector2(obj.pos2.y, 34);
                renderer.drawRoundedPanel(.9f, obj.pos1, end,
                                          obj.type == ACTIVEEDITBOX ? Color(104, 75, 43) : Color(65, 45, 29));
                const auto visible = obj.flags & TF_OBFUSCATE ? std::string(obj.text.size(), '*') : obj.text;
                renderer.drawMenuText(visible, obj.pos1 + Vector2(5, 5), 22, 0);
                if (obj.type == ACTIVEEDITBOX && obj.pos2.x >= 0)
                {
                    const float caret =
                        RenderManager::menuTextWidth(visible.substr(0, size_t(obj.pos2.x)), 22);
                    renderer.drawOverlay(1.f, obj.pos1 + Vector2(caret + 5, 5),
                                         obj.pos1 + Vector2(caret + 7, 29), Color(255, 207, 91));
                }
            }
            else
            {
                renderer.drawOverlay(obj.type == ACTIVEEDITBOX ? .3f : .5f, obj.pos1,
                                     obj.pos1 + Vector2(10 + obj.length * FontSize, 10 + FontSize));
                renderer.drawText(obj.text, obj.pos1 + Vector2(5, 5), obj.flags);
                if (obj.type == ACTIVEEDITBOX && obj.pos2.x >= 0)
                {
                    renderer.drawOverlay(1.f, obj.pos1 + Vector2(obj.pos2.x * FontSize + 5, 5),
                                         obj.pos1 + Vector2(obj.pos2.x * FontSize + 8, 5 + FontSize),
                                         Color(255, 255, 255));
                }
            }
            break;

        case SELECTBOX:
        case ACTIVESELECTBOX:
            if (obj.flags & MenuStyle)
            {
                renderer.drawRoundedPanel(.88f, obj.pos1, obj.pos2, Color(65, 45, 29));
                for (size_t i = 0; i < obj.entries.size(); ++i)
                {
                    const auto start = obj.pos1 + Vector2(8, 5 + i * 34.f);
                    if (i == size_t(obj.selected))
                    {
                        renderer.drawRoundedPanel(1.f, start, Vector2(obj.pos2.x - 34, start.y + 32),
                                                  Color(104, 75, 43));
                    }
                    const float width = obj.pos2.x - obj.pos1.x - 54;
                    const float height =
                        (std::min)(21.f,
                                   21.f * width /
                                       (std::max)(1.f, RenderManager::menuTextWidth(obj.entries[i], 21)));
                    renderer.drawMenuText(obj.entries[i], start + Vector2(6, 5), height,
                                          i == size_t(obj.selected) ? 2 : 0);
                }
                break;
            }
            FontSize = (obj.flags & TF_SMALL_FONT ? (FONT_WIDTH_SMALL + LINE_SPACER_SMALL)
                                                  : (FONT_WIDTH_NORMAL + LINE_SPACER_NORMAL));
            renderer.drawOverlay((obj.type == SELECTBOX ? 0.5 : 0.3), obj.pos1, obj.pos2);
            for (unsigned int c = 0; c < obj.entries.size(); c++)
            {
                if (c == static_cast<unsigned int>(obj.selected))
                {
                    renderer.drawText(obj.entries[c],
                                      Vector2(obj.pos1.x + 5, obj.pos1.y + (c * FontSize) + 5),
                                      obj.flags | TF_HIGHLIGHT);
                }
                else
                {
                    renderer.drawText(obj.entries[c],
                                      Vector2(obj.pos1.x + 5, obj.pos1.y + (c * FontSize) + 5), obj.flags);
                }
            }
            break;

        case CHAT:
        case ACTIVECHAT:
            FontSize = (obj.flags & TF_SMALL_FONT ? (FONT_WIDTH_SMALL + LINE_SPACER_SMALL)
                                                  : (FONT_WIDTH_NORMAL + LINE_SPACER_NORMAL));
            renderer.drawOverlay((obj.type == CHAT ? 0.5 : 0.3), obj.pos1, obj.pos2);
            for (unsigned int c = 0; c < obj.entries.size(); c++)
            {
                if (obj.text[c] == 'R')
                {
                    renderer.drawText(obj.entries[c],
                                      Vector2(obj.pos1.x + 5, obj.pos1.y + (c * FontSize) + 5),
                                      obj.flags | TF_HIGHLIGHT);
                }
                else
                {
                    renderer.drawText(obj.entries[c],
                                      Vector2(obj.pos1.x + 5, obj.pos1.y + (c * FontSize) + 5), obj.flags);
                }
            }
            break;

        case BLOB:
            renderer.drawBlob(obj.pos1, obj.col, obj.alpha);
            break;

        default:
            break;
        }
        renderQueue.pop();
    }
#if BLOBBY_ON_DESKTOP
    if (mDrawCursor)
    {
        renderer.drawImage("gfx/cursor.bmp", mInputManager->position() +
                                                 Vector2(24.f + (renderer.courtWidth() - 800.f) / 2, 24.f));
    }
#endif
    static bool lastCursor = false;
    bool relativeCursor = !mUsingCursor && mInputManager->isMouseCaptured();
    if (relativeCursor != lastCursor)
    {
        lastCursor = relativeCursor;
        SDL_SetRelativeMouseMode(relativeCursor ? SDL_TRUE : SDL_FALSE);
    }
}

void IMGUI::prepend(const std::function<void()>& draw)
{
    RenderQueue previous;
    previous.swap(*mQueue);
    draw();
    while (!previous.empty())
    {
        mQueue->push(previous.front());
        previous.pop();
    }
}
void IMGUI::doConfetti(int id, Uint32 started)
{
    QueueObject obj{};
    obj.type = CONFETTI;
    obj.id = id;
    obj.flags = started;
    mQueue->push(obj);
}
void IMGUI::doSceneBlur(int id, const std::string& key)
{
    mHasSceneBlur = true;
    QueueObject obj{};
    obj.type = SCENE_BLUR;
    obj.id = id;
    obj.text = key;
    mQueue->push(obj);
}

void IMGUI::doImage(int id, const Vector2& position, const std::string& name, const Vector2& size)
{
    QueueObject obj;
    obj.type = IMAGE;
    obj.id = id;
    obj.pos1 = position;
    obj.pos2 = size;
    obj.text = name;
    mQueue->push(obj);
}

void IMGUI::doText(int id, const Vector2& position, const std::string& text, unsigned int flags)
{
    if (mMenuStyle)
    {
        const float height = flags & TF_SMALL_FONT ? 16.f : 22.f;
        Vector2 start = position;
        const float width = RenderManager::menuTextWidth(text, height);
        if (flags & TF_ALIGN_CENTER)
        {
            start.x -= width / 2;
        }
        if (flags & TF_ALIGN_RIGHT)
        {
            start.x -= width;
        }
        doMenuText(id, start, text, height, flags & TF_HIGHLIGHT ? 2 : 0);
        return;
    }
    QueueObject obj;
    obj.type = TEXT;
    obj.id = id;
    obj.pos1 = position;

    int const fontSize = flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    // update position depending on alignment
    if (flags & TF_ALIGN_CENTER)
    {
        obj.pos1.x -= text.size() * fontSize / 2;
    }

    if (flags & TF_ALIGN_RIGHT)
    {
        obj.pos1.x -= text.size() * fontSize;
    }

    obj.text = text;
    obj.flags = flags;
    mQueue->push(obj);
}

void IMGUI::doText(int id, const Vector2& position, TextManager::STRING text, unsigned int flags)
{
    doText(id, position, getText(text), flags);
}

void IMGUI::doMenuText(int id, const Vector2& position, const std::string& text, float height, int style)
{
    QueueObject obj{};
    obj.type = MENU_TEXT;
    obj.id = id;
    obj.pos1 = position;
    obj.pos2 = Vector2(height, 0);
    obj.text = text;
    obj.flags = style;
    mQueue->push(obj);
}

void IMGUI::doRoundedPanel(int id, const Vector2& pos1, const Vector2& pos2, const Color& col, float alpha)
{
    QueueObject obj{};
    obj.type = ROUNDED_PANEL;
    obj.id = id;
    obj.pos1 = pos1;
    obj.pos2 = pos2;
    obj.col = col;
    obj.alpha = alpha;
    mQueue->push(obj);
}

void IMGUI::doOverlay(int id, const Vector2& pos1, const Vector2& pos2, const Color& col, float alpha)
{
    if (mMenuStyle && pos2.x - pos1.x < 800 && pos2.y - pos1.y < 600)
    {
        doRoundedPanel(id, pos1, pos2, col.r == 0 && col.g == 0 && col.b == 0 ? Color(65, 45, 29) : col, .9f);
        return;
    }
    QueueObject obj;
    obj.type = OVERLAY;
    obj.id = id;
    obj.pos1 = pos1;
    obj.pos2 = pos2;
    obj.col = col;
    obj.alpha = alpha;
    mQueue->push(obj);
}

bool IMGUI::doButton(int id, const Vector2& position, TextManager::STRING text, unsigned int flags,
                     float width, bool enabled)
{
    return doButton(id, position, getText(text), flags, width, enabled);
}

bool IMGUI::doButton(int id, const Vector2& position, const std::string& text, unsigned int flags,
                     float width, bool enabled)
{
    bool clicked = false;
    QueueObject obj;
    obj.id = id;
    obj.pos1 = position;
    obj.text = text;
    obj.type = TEXT;
    obj.flags = flags;

    int const textLength = TextManager::getUTF8Length(text);
    int const fontSize = flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;

    const float buttonWidth =
        mMenuStyle
            ? (std::min)(760.f - position.x, width > 0 ? width : RenderManager::menuTextWidth(text, 21) + 24)
            : float(textLength * fontSize);
    const float buttonHeight = mMenuStyle ? 34.f : float(fontSize);
    // update position depending on alignment
    if (flags & TF_ALIGN_CENTER)
    {
        obj.pos1.x -= buttonWidth / 2;
    }

    if (flags & TF_ALIGN_RIGHT)
    {
        obj.pos1.x -= buttonWidth;
    }

    if (!enabled && mActiveButton == id)
    {
        mActiveButton =
            (mLastKeyAction == KeyAction::UP || mLastKeyAction == KeyAction::LEFT) ? mLastWidget : 0;
        mLastKeyAction = KeyAction::NONE;
    }
    if (!mInactive && enabled)
    {
        // M.W. : Activate cursorless object-highlighting once the up or down key is pressed.
        if (mActiveButton == -1)
        {
            switch (mLastKeyAction)
            {
            case KeyAction::RIGHT:
                if (!mMenuStyle)
                {
                    break;
                }
                [[fallthrough]];
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::LEFT:
                if (!mMenuStyle)
                {
                    break;
                }
                [[fallthrough]];
            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // Highlight first menu object for arrow key navigation.
        if (mActiveButton == 0 && !mButtonReset)
        {
            mActiveButton = id;
        }

        // React to keyboard input.
        if (id == mActiveButton)
        {
            obj.flags = obj.flags | TF_HIGHLIGHT;
            switch (mLastKeyAction)
            {
            case KeyAction::RIGHT:
                if (!mMenuStyle)
                {
                    break;
                }
                [[fallthrough]];
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::LEFT:
                if (!mMenuStyle)
                {
                    break;
                }
                [[fallthrough]];
            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::SELECT:
                clicked = true;
                mLastKeyAction = KeyAction::NONE;
                break;
            default:
                break;
            }
        }

        // React to back button
        if (mLastKeyAction == KeyAction::BACK)
        {
            if ((text == getText(TextManager::LBL_CANCEL)) || (text == getText(TextManager::LBL_NO)) ||
                (text == getText(TextManager::MNU_LABEL_EXIT)) || (text == getText(TextManager::LBL_BACK)) ||
                (text == getText(TextManager::LBL_CLOSE)) || (text == getText(TextManager::LBL_MAIN_MENU)))
            {
                // todo: Workaround to catch backkey
                clicked = true;
                mActiveButton = id;
            }
        }

#if BLOBBY_ON_MOBILE
        const int tolerance = 3;
#else
        const int tolerance = 0;
#endif
        // React to mouse input.
        Vector2 mousepos = mInputManager->position();
        if (mousepos.x + tolerance >= obj.pos1.x && mousepos.y + tolerance * 2 >= obj.pos1.y &&
            mousepos.x - tolerance <= obj.pos1.x + buttonWidth &&
            mousepos.y - tolerance * 2 <= obj.pos1.y + buttonHeight)
        {
            obj.flags = obj.flags
#if BLOBBY_ON_DESKTOP
                        | TF_HIGHLIGHT
#endif
                ;
            if (mInputManager->click())
            {
                clicked = true;
                mActiveButton = id;
            }
        }
    }

    if (enabled)
    {
        mLastWidget = id;
    }
    if (mMenuStyle)
    {
        const bool selected = obj.flags & TF_HIGHLIGHT;
        doRoundedPanel(
            getNextId(), obj.pos1 - Vector2(1, 1), obj.pos1 + Vector2(buttonWidth + 1, buttonHeight + 1),
            selected ? Color(255, 207, 91) : Color(65, 45, 29), mInactive || !enabled ? .55f : .9f);
        doRoundedPanel(getNextId(), obj.pos1, obj.pos1 + Vector2(buttonWidth, buttonHeight),
                       Color(65, 45, 29), mInactive || !enabled ? .55f : .9f);
        const float height = (std::min)(21.f, 21.f * (buttonWidth - 16) /
                                                  (std::max)(1.f, RenderManager::menuTextWidth(text, 21)));
        doMenuText(getNextId(),
                   obj.pos1 + Vector2((buttonWidth - RenderManager::menuTextWidth(text, height)) / 2,
                                      (34 - height) / 2),
                   text, height, enabled ? 0 : 3);
    }
    else
    {
        mQueue->push(obj);
    }
    return clicked;
}

bool IMGUI::doImageButton(int id, const Vector2& position, const Vector2& size, const std::string& image)
{
    doImage(id, position, image);

    // React to mouse input.
    if (mInputManager->click())
    {
        Vector2 mousepos = mInputManager->position();
        Vector2 btnpos = position - size * 0.5;
        if (mousepos.x > btnpos.x && mousepos.y > btnpos.y && mousepos.x < btnpos.x + size.x &&
            mousepos.y < btnpos.y + size.y)
        {
            return true;
        }
    }

    return false;
}

bool IMGUI::doScrollbar(int id, const Vector2& position, float& value)
{
    QueueObject obj;
    obj.id = id;
    obj.pos1 = position;
    obj.type = SCROLLBAR;

    bool deselected = false;

    if (mInputManager->unclick())
    {
        if (id == mHeldWidget)
        {
            deselected = true;
        }

        mHeldWidget = 0;
    }

    if (!mInactive)
    {
        // M.W. : Activate cursorless object-highlighting once the up or down key is pressed.
        if (mActiveButton == -1)
        {
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // Highlight first menu object for arrow key navigation.
        if (mActiveButton == 0 && !mButtonReset)
        {
            mActiveButton = id;
        }

        // React to keyboard input.
        if (id == mActiveButton)
        {
            obj.type = ACTIVESCROLLBAR;
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::LEFT:
                value -= 0.1;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::RIGHT:
                value += 0.1;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

#if BLOBBY_ON_MOBILE
        const int tolerance = 3;
#else
        const int tolerance = 0;
#endif

        // React to mouse input.
        Vector2 mousepos = mInputManager->position();
        if (mousepos.x + 5 > position.x && mousepos.y + tolerance * 2 > position.y &&
            mousepos.x < position.x + 205 && mousepos.y - tolerance < position.y + 24.0)
        {
            obj.type = ACTIVESCROLLBAR;

            if (mInputManager->click())
            {
                mHeldWidget = id;
            }

            if (mHeldWidget == id)
            {
                value = (mousepos.x - position.x) / 200.0;
                mActiveButton = id;
            }

            if (mInputManager->mouseWheelUp())
            {
                value += 0.1;
            }

            if (mInputManager->mouseWheelDown())
            {
                value -= 0.1;
            }
        }
    }

    value = value > 0.f ? (value < 1.f ? value : 1.f) : 0.f;
    obj.pos2.x = value;

    mLastWidget = id;
    mQueue->push(obj);

    return deselected;
}

void IMGUI::resetSelection()
{
    mInactive = false;
    mActiveButton = -1;
    mButtonReset = true;
}

bool IMGUI::doEditbox(int id, const Vector2& position, unsigned int length, std::string& text, unsigned& cpos,
                      unsigned int flags, bool force_active)
{
    int FontSize = mMenuStyle ? 16 : (flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL);
    bool changed = false;
    QueueObject obj;
    obj.id = id;
    obj.pos1 = position;
    obj.type = EDITBOX;
    obj.length = length; // length does not actually work!
    obj.flags = flags | (mMenuStyle ? MenuStyle : 0);

    // Width and height including border
    int width = length * FontSize + 10;
    int height = mMenuStyle ? 34 : FontSize + 10;
    obj.pos2.y = float(width);

    // update position depending on alignment
    if (flags & TF_ALIGN_CENTER)
    {
        obj.pos1.x -= width / 2;
    }

    // React to mouse input.
    Vector2 mousepos = mInputManager->position();
    if (!mInactive && mousepos.x > obj.pos1.x && mousepos.y > obj.pos1.y && mousepos.x < obj.pos1.x + width &&
        mousepos.y < obj.pos1.y + height)
    {
        obj.flags = obj.flags | TF_HIGHLIGHT;
        if (mInputManager->click())
        {
            // Handle click on the text.
            if (mousepos.x < obj.pos1.x + text.length() * FontSize)
            {
                cpos = (int)((mousepos.x - obj.pos1.x - 5 + (FontSize / 2)) / FontSize);
            }
            // Handle click behind the text.
            else if (mousepos.x < obj.pos1.x + width)
            {
                cpos = (int)text.length();
            }

            mActiveButton = id;

            // Show keyboard
            SDL_StartTextInput();
        }
    }

    if (!mInactive)
    {
        // M.W. : Activate cursorless object-highlighting once the up or down key is pressed.
        if (mActiveButton == -1)
        {
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }

            // M.W. : Initialize the cursor position at the end of the string.
            // IMPORTANT: If you make changes to EditBox text that alter the length
            //				of the text, either call resetSelection() to come back
            //				to this area of code or update cpos manually to prevent
            //				crashes due to a misplaced cursor.
            cpos = text.length();
        }

        // Highlight first menu object for arrow key navigation.
        if (mActiveButton == 0 && !mButtonReset)
        {
            mActiveButton = id;
        }

        // React to keyboard input.
        if (id == mActiveButton || force_active)
        {
            obj.type = ACTIVEEDITBOX;
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::LEFT:
                if (cpos > 0)
                {
                    cpos--;
                    while (mMenuStyle && cpos > 0 && (static_cast<unsigned char>(text[cpos]) & 0xc0) == 0x80)
                    {
                        --cpos;
                    }
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::RIGHT:
                if (cpos < text.length())
                {
                    cpos++;
                    while (mMenuStyle && cpos < text.size() &&
                           (static_cast<unsigned char>(text[cpos]) & 0xc0) == 0x80)
                    {
                        ++cpos;
                    }
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
            std::string input = mInputManager->getLastTextKey();

            if (input == "backspace" && text.length() > 0 && cpos > 0)
            {
                auto previous = cpos - 1;
                while (mMenuStyle && previous > 0 &&
                       (static_cast<unsigned char>(text[previous]) & 0xc0) == 0x80)
                {
                    --previous;
                }
                text.erase(previous, cpos - previous);
                cpos = previous;
            }
            else if (input == "del" && text.length() > cpos)
            {
                auto next = cpos + 1;
                while (mMenuStyle && next < text.size() &&
                       (static_cast<unsigned char>(text[next]) & 0xc0) == 0x80)
                {
                    ++next;
                }
                text.erase(cpos, next - cpos);
            }
            else if (input == "return")
            {
                // Workarround for chatwindow! Delete this after GUI-Rework
                changed = true;
            }
            // This is a temporary solution until the new
            // UTF-8 class can tell the real length!!!
            else if (text.length() < length)
            {
                if (input == "paste")
                {
                    char* clipboardText = SDL_GetClipboardText();

                    if (clipboardText != nullptr)
                    {
                        for (int i = 0; text.length() < length; i++)
                        {
                            const char c = clipboardText[i];

                            // Only ASCII is currently supported
                            if ((c == '\0') || (c < 0x20) || (c > 0xf7))
                            {
                                break;
                            }

                            text.insert(cpos, &c, 1);
                            cpos++;
                            changed = true;
                        }

                        SDL_free(clipboardText);
                    }
                }
                if (input == "space")
                {
                    text.insert(cpos, " ");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad0")
                {
                    text.insert(cpos, "0");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad1")
                {
                    text.insert(cpos, "1");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad2")
                {
                    text.insert(cpos, "2");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad3")
                {
                    text.insert(cpos, "3");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad4")
                {
                    text.insert(cpos, "4");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad5")
                {
                    text.insert(cpos, "5");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad6")
                {
                    text.insert(cpos, "6");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad7")
                {
                    text.insert(cpos, "7");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad8")
                {
                    text.insert(cpos, "8");
                    cpos++;
                    changed = true;
                }
                else if (input == "keypad9")
                {
                    text.insert(cpos, "9");
                    cpos++;
                    changed = true;
                }
                else if (input.length() == 1 || (mMenuStyle && !input.empty() && input != "paste" &&
                                                 input != "backspace" && input != "del"))
                {
                    if (text.size() + input.size() <= length)
                    {
                        text.insert(cpos, input);
                        cpos += unsigned(input.size());
                        changed = true;
                    }
                }
            }
        }
    }

    obj.pos2.x = SDL_GetTicks() % 1000 >= 500 ? cpos : -1.0;
    obj.text = text;

    mLastWidget = id;
    mQueue->push(obj);

    // when content changed, it is active
    // part of chat window hack
    if (changed && force_active)
    {
        mActiveButton = id;
    }

    return changed;
}

SelectBoxAction IMGUI::doSelectbox(int id, const Vector2& pos1, const Vector2& pos2,
                                   const std::vector<std::string>& entries, unsigned int& selected,
                                   unsigned int flags)
{
    int FontSize = (flags & TF_SMALL_FONT ? (FONT_WIDTH_SMALL + LINE_SPACER_SMALL)
                                          : (FONT_WIDTH_NORMAL + LINE_SPACER_NORMAL));
    if (mMenuStyle)
    {
        FontSize = 34;
    }
    if (!entries.empty() && selected >= entries.size())
    {
        selected = 0;
    }
    SelectBoxAction changed = SBA_NONE;
    QueueObject obj;
    obj.id = id;
    obj.pos1 = pos1;
    obj.pos2 = pos2;
    obj.type = SELECTBOX;
    obj.flags = flags | (mMenuStyle ? MenuStyle : 0);

    const int itemsPerPage = int(pos2.y - pos1.y - 10) / FontSize;
    int first = (int)(selected / itemsPerPage) * itemsPerPage; // the first visible element in the list

    if (!mInactive)
    {
        // M.W. : Activate cursorless object-highlighting once the up or down key is pressed.
        if (mActiveButton == -1)
        {
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // Highlight first menu object for arrow key navigation.
        if (mActiveButton == 0 && !mButtonReset)
        {
            mActiveButton = id;
        }

        // React to keyboard input.
        if (id == mActiveButton)
        {
            obj.type = ACTIVESELECTBOX;
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                if (mMenuStyle && selected + 1 < entries.size())
                {
                    ++selected;
                    changed = SBA_SELECT;
                    mLastKeyAction = KeyAction::NONE;
                    break;
                }
                if (mMenuStyle)
                {
                    mLastKeyAction = KeyAction::NONE;
                    break;
                }
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                if (mMenuStyle)
                {
                    if (selected > 0)
                    {
                        --selected;
                        changed = SBA_SELECT;
                    }
                    mLastKeyAction = KeyAction::NONE;
                    break;
                }
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::SELECT:
                if (mMenuStyle && !entries.empty())
                {
                    changed = SBA_DBL_CLICK;
                    mLastKeyAction = KeyAction::NONE;
                }
                break;
            case KeyAction::LEFT:
                if (mMenuStyle)
                {
                    mActiveButton = mLastWidget;
                    mLastKeyAction = KeyAction::NONE;
                    break;
                }
                if (selected > 0)
                {
                    selected--;
                    changed = SBA_SELECT;
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::RIGHT:
                if (mMenuStyle)
                {
                    mActiveButton = 0;
                    mLastKeyAction = KeyAction::NONE;
                    break;
                }
                if (selected + 1 < entries.size())
                {
                    selected++;
                    changed = SBA_SELECT;
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // React to mouse input.
        Vector2 mousepos = mInputManager->position();
        if (mousepos.x > pos1.x && mousepos.y > pos1.y && mousepos.x < pos2.x && mousepos.y < pos2.y)
        {
            obj.type = ACTIVESELECTBOX;
            if (mInputManager->click())
            {
                mActiveButton = id;
            }
        }
        // entries mouseclick:
        if (mousepos.x > pos1.x && mousepos.y > pos1.y + 5 && mousepos.x < pos2.x - 35 &&
            mousepos.y < pos1.y + 5 + FontSize * itemsPerPage)
        {
            if (mInputManager->click())
            {
                int tmp = (int)((mousepos.y - pos1.y - 5) / FontSize) + first;
                /// \todo well, it's not really a doulbe click...
                /// we need to do this in inputmanager
                if (selected == static_cast<unsigned int>(tmp) && mInputManager->doubleClick())
                {
                    changed = SBA_DBL_CLICK;
                }

                if (tmp >= 0 && static_cast<unsigned int>(tmp) < entries.size())
                {
                    selected = tmp;
                }

                mActiveButton = id;
            }
            if ((mInputManager->mouseWheelUp()) && (selected > 0))
            {
                selected--;
                changed = SBA_SELECT;
            }
            if ((mInputManager->mouseWheelDown()) && (selected + 1 < entries.size()))
            {
                selected++;
                changed = SBA_SELECT;
            }
        }
        // arrows mouseclick:
        if (mousepos.x > pos2.x - 30 && mousepos.x < pos2.x - 30 + 24 && mInputManager->click())
        {
            if (mousepos.y > pos1.y + 3 && mousepos.y < pos1.y + 3 + 24 && selected > 0)
            {
                selected--;
                changed = SBA_SELECT;
            }

            if (mousepos.y > pos2.y - 27 && mousepos.y < pos2.y - 27 + 24 && selected + 1 < entries.size())
            {
                selected++;
                changed = SBA_SELECT;
            }
        }
    }
    if (!mMenuStyle)
    {
        doImage(getNextId(), Vector2(pos2.x - 15, pos1.y + 15), "gfx/pfeil_oben.bmp");
        doImage(getNextId(), Vector2(pos2.x - 15, pos2.y - 15), "gfx/pfeil_unten.bmp");
    }

    first = (selected / itemsPerPage) * itemsPerPage; // recalc first
    if (!entries.empty())
    {
        unsigned int last = first + itemsPerPage;
        if (last > entries.size())
        {
            last = entries.size();
        }

        obj.entries = std::vector<std::string>(entries.begin() + first, entries.begin() + last);
    }
    else
    {
        obj.entries = std::vector<std::string>();
    }

    obj.selected = selected - first;

    mLastWidget = id;
    mQueue->push(obj);
    if (mMenuStyle)
    {
        doMenuText(getNextId(), Vector2(pos2.x - 24, pos1.y + 6), "^", 22, 2);
        doMenuText(getNextId(), Vector2(pos2.x - 24, pos2.y - 28), "v", 22, 2);
    }
    return changed;
}

void IMGUI::doChatbox(int id, const Vector2& pos1, const Vector2& pos2,
                      const std::vector<std::string>& entries, unsigned int& selected,
                      const std::vector<bool>& local, unsigned int flags)
{
    assert(entries.size() == local.size());
    int FontSize = (flags & TF_SMALL_FONT ? (FONT_WIDTH_SMALL + LINE_SPACER_SMALL)
                                          : (FONT_WIDTH_NORMAL + LINE_SPACER_NORMAL));
    QueueObject obj;
    obj.id = id;
    obj.pos1 = pos1;
    obj.pos2 = pos2;
    obj.type = CHAT;
    obj.flags = flags;

    const unsigned int itemsPerPage = int(pos2.y - pos1.y - 10) / FontSize;

    if (!mInactive)
    {
        // M.W. : Activate cursorless object-highlighting once the up or down key is pressed.
        if (mActiveButton == -1)
        {
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // Highlight first menu object for arrow key navigation.
        if (mActiveButton == 0 && !mButtonReset)
        {
            mActiveButton = id;
        }

        // React to keyboard input.
        if (id == mActiveButton)
        {
            obj.type = ACTIVECHAT;
            switch (mLastKeyAction)
            {
            case KeyAction::DOWN:
                mActiveButton = 0;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::UP:
                mActiveButton = mLastWidget;
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::LEFT:
                if (selected > 0)
                {
                    selected--;
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            case KeyAction::RIGHT:
                if (selected + 1 < entries.size())
                {
                    selected++;
                }
                mLastKeyAction = KeyAction::NONE;
                break;

            default:
                break;
            }
        }

        // React to mouse input.
        Vector2 mousepos = mInputManager->position();
        if (mousepos.x > pos1.x && mousepos.y > pos1.y && mousepos.x < pos2.x && mousepos.y < pos2.y)
        {
            if (mInputManager->click())
            {
                mActiveButton = id;
            }
        }
        // entries mouseclick:
        if (mousepos.x > pos1.x && mousepos.y > pos1.y + 5 && mousepos.x < pos2.x - 35 &&
            mousepos.y < pos1.y + 5 + FontSize * itemsPerPage)
        {
            if ((mInputManager->mouseWheelUp()) && (selected > 0))
            {
                selected--;
            }

            if ((mInputManager->mouseWheelDown()) && (selected + 1 < entries.size()))
            {
                selected++;
            }
        }
        // arrows mouseclick:
        if (mousepos.x > pos2.x - 30 && mousepos.x < pos2.x - 30 + 24 && mInputManager->click())
        {
            if (mousepos.y > pos1.y + 3 && mousepos.y < pos1.y + 3 + 24 && selected > 0)
            {
                selected--;
            }

            if (mousepos.y > pos2.y - 27 && mousepos.y < pos2.y - 27 + 24 && selected + 1 < entries.size())
            {
                selected++;
            }
        }
    }
    doImage(getNextId(), Vector2(pos2.x - 15, pos1.y + 15), "gfx/pfeil_oben.bmp");
    doImage(getNextId(), Vector2(pos2.x - 15, pos2.y - 15), "gfx/pfeil_unten.bmp");

    unsigned int first = (selected / itemsPerPage) * itemsPerPage; // recalc first
    if (!entries.empty())
    {
        unsigned int last = selected + 1;
        /// \todo maybe we should adapt selected so we even can't scroll up further!
        // we don't want negative chatlog, so we just scroll upward without coming to negative
        // elements.
        if (last >= itemsPerPage)
        {
            first = last - itemsPerPage;
        }
        else
        {
            first = 0;
        }

        // check that we don't create out of bounds problems
        if (last > entries.size())
        {
            last = entries.size();
        }

        obj.entries = std::vector<std::string>(entries.begin() + first, entries.begin() + last);
        // HACK: we use taxt to store information which text is from local player and which from
        //			remote player.
        obj.text = "";
        for (unsigned int i = first; i < last; ++i)
        {
            obj.text += local[i] ? 'L' : 'R';
        }
    }
    else
    {
        obj.entries = std::vector<std::string>();
    }

    obj.selected = selected - first;

    mLastWidget = id;
    mQueue->push(obj);
}

void IMGUI::doChampion(int id, const Vector2& center, const Color& color, float height)
{
    QueueObject obj{};
    obj.type = CHAMPION;
    obj.id = id;
    obj.pos1 = center;
    obj.col = color;
    obj.alpha = height;
    mQueue->push(obj);
}

bool IMGUI::doBlob(int id, const Vector2& position, const Color& col, float scale)
{
    QueueObject obj;
    obj.id = id;
    obj.pos1 = position;
    obj.type = BLOB;
    obj.col = col;
    obj.alpha = scale;
    mQueue->push(obj);
    return false;
}

void IMGUI::doCursor(bool draw)
{
    mDrawCursor = draw;
    mUsingCursor = true;
}

bool IMGUI::usingCursor() const
{
    return mUsingCursor;
}

const TextManager& IMGUI::textMgr() const
{
    return *mTextManager;
}

const std::string& IMGUI::getText(TextManager::STRING id) const
{
    return textMgr().getString(id);
}

void IMGUI::setTextMgr(std::string lang)
{
    mTextManager.reset(new TextManager(std::move(lang)));
}
