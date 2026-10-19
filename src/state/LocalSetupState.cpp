#include "LocalSetupState.h"
#include "LocalGameState.h"
#include "OptionsState.h"
#include "NetworkSearchState.h"
#include "BlobbyApp.h"
#include "DuelMatch.h"
#include "LevelCatalog.h"
#include "FileSystem.h"
#include "InputDevice.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "WarmUI.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
int wrap(int n, int size)
{
    return size ? (n % size + size) % size : 0;
}
std::string tr(const char* key)
{
    return translateUI(WarmUI::language(), key);
}
// Vertical focus, held left/right repetition and mouse dragging share one path.
struct Widgets
{
    IMGUI& g;
    InputManager& in;
    int focus = 0, count = 0, previousCount = 1, drag = -1;
    bool enabled = false, selectActivates = true;
    Vector2 mouse, last = Vector2(-1, -1);
    bool moved = false;
    Widgets(IMGUI& g, InputManager& in) : g(g), in(in) {}
    void begin(bool active)
    {
        enabled = active;
        selectActivates = true;
        mouse = in.position();
        moved = mouse != last;
        last = mouse;
        if (enabled && in.up())
        {
            focus = wrap(focus - 1, previousCount);
        }
        if (enabled && in.down())
        {
            focus = wrap(focus + 1, previousCount);
        }
        if (in.unclick())
        {
            drag = -1;
        }
        count = 0;
    }
    void end()
    {
        previousCount = std::max(1, count);
        focus = wrap(focus, previousCount);
    }
    int item(float x, float y, float w, float h)
    {
        int id = count++;
        if (enabled && WarmUI::hit(mouse, x, y, w, h) && (moved || in.click()))
        {
            focus = id;
        }
        return id;
    }
    void label(float x, float y, const std::string& s, float size = 20, int style = 0)
    {
        WarmUI::text(g, x, y, s, size, style);
    }
    int row(float x, float y, float w, const std::string& label, const std::string& value, bool active = true)
    {
        int id = item(x, y, w, 34);
        bool selected = focus == id && active;
        WarmUI::panel(g, x, y, w, 34, selected ? Color(104, 75, 43) : Color(65, 45, 29));
        const float labelSize =
            std::min(19.f, 19.f * w * .43f / std::max(1.f, RenderManager::menuTextWidth(label, 19)));
        this->label(x + 10, y + 7, label, labelSize);
        const float size =
            std::min(19.f, 19.f * w * .55f / std::max(1.f, RenderManager::menuTextWidth(value, 19)));
        WarmUI::text(g, x + w - 10, y + 7, value, size, selected ? 2 : 0, 2);
        if (!enabled || !active)
        {
            return 0;
        }
        if (WarmUI::hit(mouse, x, y, w, 34) && in.click())
        {
            return (SDL_GetModState() & KMOD_SHIFT) ? -1 : 1;
        }
        if (focus == id)
        {
            return in.sliderDirection() ? in.sliderDirection() : (selectActivates && in.select() ? 1 : 0);
        }
        return 0;
    }
    bool button(float x, float y, float w, const std::string& s, bool active = true)
    {
        int id = item(x, y, w, 34);
        const bool selected = id == focus && active;
        WarmUI::panel(g, x, y, w, 34, selected ? Color(255, 207, 91) : Color(65, 45, 29));
        const float h = std::min(20.f, 20.f * (w - 18) / std::max(1.f, RenderManager::menuTextWidth(s, 20)));
        WarmUI::text(g, x + w / 2, y + (34 - h) / 2, s, h, selected ? 1 : 0, 1);
        return enabled && active &&
               ((WarmUI::hit(mouse, x, y, w, 34) && in.click()) ||
                (selected && selectActivates && in.select()));
    }
    void slider(float x, float y, float w, const std::string& s, int& value, int lo, int hi,
                Color color = Color(255, 207, 91), int step = 1)
    {
        int id = item(x, y, w, 44);
        WarmUI::panel(g, x, y, w, 44, focus == id ? Color(104, 75, 43) : Color(65, 45, 29));
        if (enabled && WarmUI::hit(mouse, x, y, w, 44) && in.click())
        {
            drag = id;
        }
        if (enabled && drag == id)
        {
            value = lo +
                    int(std::lround(std::max(0.f, std::min(1.f, (mouse.x - x - 12) / (w - 24))) * (hi - lo)));
        }
        else if (enabled && focus == id)
        {
            value = std::max(lo, std::min(hi, value + in.sliderDirection() * step));
        }
        label(x + 10, y + 3, s, 18);
        WarmUI::text(g, x + w - 10, y + 3, std::to_string(value), 18, 2, 2);
        WarmUI::panel(g, x + 12, y + 33, w - 24, 3, Color(116, 92, 58));
        float v = float(value - lo) / (hi - lo);
        WarmUI::panel(g, x + 12, y + 33, (w - 24) * v, 3, color);
        WarmUI::panel(g, x + 9 + (w - 24) * v, y + 28, 6, 13, Color(255, 244, 217));
    }
};
LocalParticipant extraParticipant(unsigned index, bool human)
{
    static const int colors[] = {int(Color(50, 189, 234).toInt()), int(Color(233, 80, 125).toInt()),
                                 int(Color(168, 218, 85).toInt()), int(Color(183, 133, 239).toInt()),
                                 int(Color(245, 170, 72).toInt()), int(Color(99, 213, 191).toInt())};
    LocalParticipant p;
    p.human = human;
    p.name = tr(human ? "Player " : "Bot ") + std::to_string(index + 1);
    p.color = colors[index % 6];
    return p;
}
} // namespace
struct LocalSetupState::Menu
{
    LocalSetupState& owner;
    Widgets ui;
    PlayerProfiles profiles;
    UserConfig bindings;
    int tab = 0, editing = -1, page = 0, selectedProfile = 0, format = 1, view = 0, round = 0, group = 0;
    bool naming = false, first = true, ready[2] = {false, false}, armed[2] = {false, false};
    std::string notice, nameBackup;
    unsigned cursor = 0;
    Uint32 readyAfter = 0, nameAfter = 0;
    std::unique_ptr<InputDevice> devices[2];
    std::unique_ptr<DuelMatch> readinessMatch;
    LocalParticipant draft;
    std::shared_ptr<LocalSession> savedCup;
    Menu(LocalSetupState& o) : owner(o), ui(o.getIMGUI(), o.getInputMgr())
    {
        bindings.loadFile("inputconfig.xml");
        selectedProfile = profiles.selected;
        if (owner.mDestination >= 0)
        {
            editing = 0;
            draft = profiles.players[selectedProfile];
            draft.human = true;
        }
        else if (owner.mSession->started)
        {
            tab = 2;
        }
        format = int(owner.mSession->cup.settings.format);
        if (owner.mDestination < 0 && !owner.mSession->started)
        {
            savedCup = LocalSession::resume();
        }
    }
    void edit(int index)
    {
        editing = index;
        draft = owner.mSession->players.at(index);
        selectedProfile = draft.profile;
        ui.focus = 0;
    }
    void selectLevel()
    {
        auto id = owner.mSession->settings.getString("background", "strand1.bmp");
        owner.getApp().getRenderManager().setBackground("backgrounds/" + id);
        owner.getApp().getSoundManager().selectLevel(id);
    }
    void saveProfile()
    {
        if (draft.name.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            throw std::runtime_error("Enter a name.");
        }
        if (draft.profile >= 0 && draft.profile < int(profiles.players.size()))
        {
            profiles.players[draft.profile] = draft;
        }
        else
        {
            if (profiles.players.size() >= 64)
            {
                throw std::runtime_error("Profile limit reached.");
            }
            draft.profile = int(profiles.players.size());
            profiles.players.push_back(draft);
        }
        selectedProfile = draft.profile;
        profiles.selected = draft.profile;
        profiles.save();
        notice = tr("Profile saved.");
    }
    void editor()
    {
        const bool network = owner.mDestination >= 0;
        auto& g = ui.g;
        std::string current = selectedProfile >= 0 && selectedProfile < int(profiles.players.size())
                                  ? profiles.players[selectedProfile].name
                                  : tr("Guest");
        if (int d = ui.row(40, 140, 350, tr("Profile"), current))
        {
            selectedProfile = wrap(selectedProfile + d, int(profiles.players.size()));
            draft = profiles.players[selectedProfile];
        }
        if (ui.row(40, 180, 350, tr("Name"), draft.name))
        {
            naming = true;
            nameBackup = draft.name;
            nameAfter = SDL_GetTicks() + 180;
            SDL_StartTextInput();
            g.resetSelection();
        }
        if (!network && ui.row(40, 220, 350, tr("Player type"), tr(draft.human ? "Human" : "Bot")))
        {
            draft.human = !draft.human;
        }
        float y = network ? 220.f : 260.f;
        int rgb[3] = {Color(draft.color).r, Color(draft.color).g, Color(draft.color).b};
        const char* channels[] = {"Red", "Green", "Blue"};
        Color cols[] = {Color(238, 124, 121), Color(173, 228, 106), Color(115, 197, 242)};
        for (int j = 0; j < 3; ++j)
        {
            ui.slider(40, y + j * 50, 350, tr(channels[j]), rgb[j], 0, 255, cols[j]);
        }
        draft.color = Color(rgb[0], rgb[1], rgb[2]).toInt();
        if (ui.row(40, y + 154, 350, tr("Colour cycling"), tr(draft.cycling ? "On" : "Off")))
        {
            draft.cycling = !draft.cycling;
        }
        if (!draft.human && !network)
        {
            auto scripts = FileSystem::getSingleton().enumerateFiles("scripts", ".lua", false);
            int index = int(std::find(scripts.begin(), scripts.end(), draft.script) - scripts.begin());
            if (int d = ui.row(410, 325, 350, tr("Bot"), draft.script))
            {
                if (!scripts.empty())
                {
                    draft.script = scripts[wrap(index + d, int(scripts.size()))];
                }
            }
            int strength = 100 - draft.delay * 4;
            ui.slider(410, 366, 350, tr("Bot strength"), strength, 0, 100, Color(255, 207, 91), 4);
            draft.delay = int(std::lround((100 - strength) / 4.f));
        }
        Color color =
            draft.cycling ? owner.getApp().getRenderManager().getOscillationColor() : Color(draft.color);
        g.doBlob(g.getNextId(), Vector2(585, 228), color, 1.25f);
        WarmUI::text(g, 585, 299, draft.name, 25, 0, 1);
        if (network)
        {
            if (ui.row(410, 370, 350, tr("Control preset"), std::to_string(draft.input + 1)))
            {
                draft.input = 1 - draft.input;
            }
            if (ui.row(410, 410, 350, tr("My side"), tr(draft.side ? "Right" : "Left")))
            {
                draft.side = 1 - draft.side;
            }
            ui.label(410, 454, tr("Spectators: alphabetical name order."), 17);
        }
        if (ui.button(410, 495, 140, tr("Save profile")))
        {
            saveProfile();
        }
        if (ui.button(560, 495, 95, tr("New profile")))
        {
            draft.profile = -1;
            saveProfile();
        }
        if (ui.button(665, 495, 95, tr(network ? "Continue" : "Use participant")))
        {
            if (draft.name.find_first_not_of(" \t\r\n") == std::string::npos)
            {
                throw std::runtime_error("Enter a name.");
            }
            if (network)
            {
                saveProfile();
                profiles.publishNetwork();
                if (owner.mDestination == 1)
                {
                    owner.switchState(new LANSearchState());
                }
                else if (owner.mDestination == 2)
                {
                    owner.switchState(new OnlineSearchState());
                }
                else
                {
                    owner.switchState(new MainMenuState());
                }
            }
            else
            {
                owner.mSession->players[editing] = draft;
                editing = -1;
                ui.focus = 0;
            }
        }
        if (naming)
        {
            g.doOverlay(g.getNextId(), Vector2(0, 0), Vector2(800, 600), Color(0, 0, 0), .8f);
            WarmUI::panel(g, 125, 200, 550, 160);
            ui.label(145, 216, tr("Edit name"), 28);
            ui.label(145, 265, draft.name + "_", 25);
            ui.label(145, 317, tr("Enter: confirm     Esc: cancel"), 19);
            if (SDL_TICKS_PASSED(SDL_GetTicks(), nameAfter))
            {
                auto key = ui.in.getLastTextKey();
                if (ui.in.exit())
                {
                    draft.name = nameBackup;
                    naming = false;
                }
                else if (key == "return" || ui.in.select())
                {
                    naming = false;
                }
                else if (key == "backspace" && !draft.name.empty())
                {
                    size_t end = draft.name.size() - 1;
                    while (end > 0 && (static_cast<unsigned char>(draft.name[end]) & 0xc0) == 0x80)
                    {
                        --end;
                    }
                    draft.name.resize(end);
                }
                else if (!key.empty() && key != "paste" && key != "del" &&
                         draft.name.size() + key.size() <= 40)
                {
                    draft.name += key;
                }
                if (!naming)
                {
                    SDL_StopTextInput();
                    g.resetSelection();
                }
            }
        }
    }
    void game()
    {
        auto& s = *owner.mSession;
        if (ui.row(40, 140, 350, tr("Game mode"), tr(s.tournament ? "Tournament" : "Single match")))
        {
            s.tournament = !s.tournament;
            if (!s.tournament)
            {
                s.players.resize(2);
            }
        }
        if (s.tournament)
        {
            const char* names[] = {"Round robin", "Knockout", "Groups + knockout"};
            if (int d = ui.row(40, 180, 350, tr("Format"), tr(names[format])))
            {
                format = wrap(format + d, 3);
                s.cup.settings.format = tournament::Format(format);
            }
            bool& extra = format == 0 ? s.cup.settings.finalMatch : s.cup.settings.thirdPlace;
            if (ui.row(410, 180, 350, tr(format == 0 ? "Final match" : "Third place"),
                       tr(extra ? "On" : "Off")))
            {
                extra = !extra;
            }
        }
        auto rules = FileSystem::getSingleton().enumerateFiles("rules", ".lua", true);
        std::string rule = s.settings.getString("rules", "default.lua");
        int index = int(std::find(rules.begin(), rules.end(), rule) - rules.begin());
        if (int d = ui.row(40, 220, 350, tr("Rules"), rule))
        {
            if (!rules.empty())
            {
                s.settings.setString("rules", rules[wrap(index + d, int(rules.size()))]);
            }
        }
        int score = s.settings.getInteger("scoretowin", 15);
        if (int d = ui.row(40, 260, 350, tr("Points to win"), std::to_string(score)))
        {
            s.settings.setInteger("scoretowin", std::max(1, std::min(99, score + d)));
        }
        bool wide = s.settings.getBool("court_widescreen");
        if (ui.row(40, 300, 350, tr("Court"), tr(wide ? "16:9 Widescreen" : "4:3 Classic")))
        {
            s.settings.setBool("court_widescreen", !wide);
            owner.getApp().getRenderManager().setCourtWidth(!wide ? 3200.f / 3.f : 800.f);
        }
        auto levels = availableLevels();
        auto id = s.settings.getString("background", "strand1.bmp");
        auto found =
            std::find_if(levels.begin(), levels.end(), [&](const LevelDefinition& l) { return l.id == id; });
        int n = found == levels.end() ? 0 : int(found - levels.begin());
        if (int d = ui.row(
                40, 340, 350, tr("Level"),
                levels.empty() ? "-" : levels[n].name + (levels[n].custom ? " (" + tr("Custom") + ")" : "")))
        {
            if (!levels.empty())
            {
                s.settings.setString("background", levels[wrap(n + d, int(levels.size()))].id);
                selectLevel();
            }
        }
        int speed = s.settings.getInteger("gamefps", 75);
        int percent = int(std::lround(speed * 100.f / 75));
        ui.slider(40, 380, 350, tr("Game speed") + " %", percent, 40, 160);
        s.settings.setInteger("gamefps", int(std::lround(percent * .75f)));
        const int speeds[] = {30, 60, 75, 90, 120};
        const char* labels[] = {"Very slow", "Slow", "Normal", "Fast", "Very fast"};
        for (int i = 0; i < 5; ++i)
        {
            if (ui.button(40 + i * 71, 432, 66, tr(labels[i])))
            {
                s.settings.setInteger("gamefps", speeds[i]);
            }
        }
        WarmUI::panel(ui.g, 410, 245, 350, 223);
        ui.label(425, 260, tr("Local tournament"), 25);
        ui.label(425, 304, tr("People and bots play in turns."), 18);
        ui.label(425, 335, tr("Share controllers and keyboard."), 18);
        ui.label(425, 382, tr("4:3 Classic"), 18);
        ui.label(425, 412, tr("16:9: wider court, longer runs."), 18);
        if (ui.button(560, 500, 200, tr("Participants")))
        {
            tab = 1;
            ui.focus = 0;
        }
        if (!s.started && ui.button(410, 500, 140, tr("Resume cup"), bool(savedCup)))
        {
            auto saved = savedCup;
            if (saved)
            {
                owner.switchState(new LocalSetupState(saved));
            }
        }
    }
    void participants()
    {
        auto& s = *owner.mSession;
        ui.label(40, 137, std::to_string(s.players.size()) + " " + tr("Participants"), 23);
        const int pages = std::max(1, (int(s.players.size()) + 5) / 6);
        page = wrap(page, pages);
        for (int j = 0; j < 6; ++j)
        {
            int i = page * 6 + j;
            if (i >= int(s.players.size()))
            {
                break;
            }
            auto& p = s.players[i];
            float y = 177 + j * 47.f;
            ui.g.doBlob(ui.g.getNextId(), Vector2(63, y + 17),
                        p.cycling ? owner.getApp().getRenderManager().getOscillationColor() : Color(p.color),
                        .26f);
            if (ui.row(90, y, 540, p.name,
                       p.human ? tr("Human") : p.script + " · " + std::to_string(100 - p.delay * 4) + " %"))
            {
                edit(i);
            }
            if (ui.button(640, y, 120, tr("Remove entrant"), s.tournament && s.players.size() > 2))
            {
                s.players.erase(s.players.begin() + i);
                break;
            }
        }
        if (ui.button(40, 475, 170, tr("Add human"), s.tournament && s.players.size() < 32))
        {
            s.players.push_back(extraParticipant(unsigned(s.players.size()), true));
            page = (int(s.players.size()) - 1) / 6;
        }
        if (ui.button(220, 475, 170, tr("Add bot"), s.tournament && s.players.size() < 32))
        {
            s.players.push_back(extraParticipant(unsigned(s.players.size()), false));
            page = (int(s.players.size()) - 1) / 6;
        }
        if (ui.button(410, 475, 170, tr("Next page"), pages > 1))
        {
            ++page;
        }
        if (ui.button(590, 475, 170, tr("Overview")))
        {
            s.prepare(SDL_GetTicks());
            tab = 2;
            ui.focus = 0;
        }
        ui.label(40, 526, tr("Edit a participant to change name, colour or bot."), 18);
    }
    void overview()
    {
        auto& s = *owner.mSession;
        if (s.tournament && s.cup.matches.empty())
        {
            s.prepare(SDL_GetTicks());
        }
        if (!s.tournament)
        {
            s.pending = 0;
        }
        const char* views[] = {"Schedule", "Bracket", "Standings"};
        if (s.tournament)
        {
            if (int d = ui.row(40, 138, 350, tr("View"), tr(views[view])))
            {
                view = wrap(view + d, 3);
                page = 0;
            }
            if (view == 2)
            {
                int groups = s.cup.groupCount();
                if (groups && ui.row(410, 138, 350, tr("Group"), std::to_string(group + 1)))
                {
                    group = wrap(group + 1, groups);
                    page = 0;
                }
                auto rows = s.cup.standings(groups ? group : -1);
                int pages = std::max(1, (int(rows.size()) + 7) / 8);
                page = wrap(page, pages);
                ui.label(40, 184, tr("Name"), 18, 2);
                ui.label(400, 184, tr("Played"), 18, 2);
                ui.label(515, 184, tr("Wins"), 18, 2);
                ui.label(620, 184, tr("Points"), 18, 2);
                for (int j = 0; j < 8; ++j)
                {
                    int i = page * 8 + j;
                    if (i >= int(rows.size()))
                    {
                        break;
                    }
                    const auto& r = rows[i];
                    float y = 216 + j * 30.f;
                    ui.label(40, y, std::to_string(i + 1) + ". " + s.cup.person(r.person).name, 20);
                    ui.label(400, y, std::to_string(r.played), 20);
                    ui.label(515, y, std::to_string(r.wins), 20);
                    ui.label(620, y, std::to_string(r.scored) + ":" + std::to_string(r.conceded), 20);
                }
                if (ui.button(40, 477, 170, tr("Next page"), pages > 1))
                {
                    ++page;
                }
            }
            else if (view == 1)
            {
                int maxRound = 0;
                for (const auto& m : s.cup.matches)
                {
                    if (m.knockout)
                    {
                        maxRound = std::max(maxRound, m.round);
                    }
                }
                if (int d = ui.row(410, 138, 350, tr("Round"), std::to_string(round + 1)))
                {
                    round = wrap(round + d, maxRound + 1);
                    page = 0;
                }
                int count = 0;
                for (const auto& m : s.cup.matches)
                {
                    count += m.knockout && m.round == round;
                }
                int pages = std::max(1, (count + 3) / 4);
                page = wrap(page, pages);
                for (int col = 0; col < 3; ++col)
                {
                    int index = 0;
                    for (const auto& m : s.cup.matches)
                    {
                        if (!m.knockout || m.round != round + col)
                        {
                            continue;
                        }
                        int start = (page * 4) >> col, number = std::max(1, 4 >> col), n = index++;
                        if (n < start || n >= start + number)
                        {
                            continue;
                        }
                        float x = 40 + 244.f * col,
                              y = 207 + 32.f * ((1 << col) - 1) + (n - start) * 64.f * (1 << col);
                        WarmUI::panel(ui.g, x, y, 232, 49,
                                      m.id == s.pending ? Color(104, 75, 43) : Color(65, 45, 29));
                        auto name = [&](int id) { return id < 0 ? tr("Waiting") : s.players.at(id).name; };
                        std::string names = name(m.a) + " / " + name(m.b);
                        float size = std::min(
                            18.f, 18.f * 216 / std::max(1.f, RenderManager::menuTextWidth(names, 18)));
                        ui.label(x + 8, y + 5, names, size);
                        std::string score = m.status == tournament::MatchStatus::Finished
                                                ? std::to_string(m.scoreA) + ":" + std::to_string(m.scoreB)
                                            : m.status == tournament::MatchStatus::Bye ? tr("Bye")
                                                                                       : tr("Waiting");
                        ui.label(x + 8, y + 27, "#" + std::to_string(m.id + 1) + "  " + score, 15, 2);
                        if (col < 2)
                        {
                            WarmUI::panel(ui.g, x + 232, y + 23, 12, 2, Color(209, 164, 68));
                        }
                    }
                }
                if (!count)
                {
                    ui.label(40, 205, tr("No knockout matches yet."), 20);
                }
                if (ui.button(40, 477, 170, tr("Next page"), pages > 1))
                {
                    ++page;
                }
            }
            else
            {
                int pages = std::max(1, (int(s.cup.matches.size()) + 7) / 8);
                page = wrap(page, pages);
                for (int j = 0; j < 8; ++j)
                {
                    int i = page * 8 + j;
                    if (i >= int(s.cup.matches.size()))
                    {
                        break;
                    }
                    const auto& m = s.cup.matches[i];
                    auto name = [&](int id) { return id < 0 ? tr("Waiting") : s.players.at(id).name; };
                    std::string score = m.status == tournament::MatchStatus::Finished
                                            ? std::to_string(m.scoreA) + ":" + std::to_string(m.scoreB)
                                        : m.status == tournament::MatchStatus::Bye ? tr("Bye")
                                                                                   : "—";
                    ui.label(40, 195 + j * 33.f,
                             "#" + std::to_string(i + 1) + "  " + name(m.a) + " / " + name(m.b) + "    " +
                                 score,
                             18, i == s.pending ? 2 : 0);
                }
                if (ui.button(40, 477, 170, tr("Next page"), pages > 1))
                {
                    ++page;
                }
            }
        }
        if (s.pending >= 0)
        {
            auto ids = s.nextPlayers();
            if (!s.tournament || view == 0)
            {
                WarmUI::panel(ui.g, 460, 190, 300, 245);
                WarmUI::text(ui.g, 610, 212, tr("Next match"), 25, 2, 1);
                WarmUI::text(ui.g, 610, 264, s.players[ids.first].name, 24, 0, 1);
                WarmUI::text(ui.g, 610, 307, "VS", 25, 2, 1);
                WarmUI::text(ui.g, 610, 350, s.players[ids.second].name, 24, 0, 1);
            }
            if (ui.button(460, 477, 300, tr(s.started ? "Next match" : "Start")))
            {
                s.started = true;
                s.save();
                owner.switchState(new LocalSetupState(owner.mSession, true));
            }
        }
        else
        {
            int winner = s.cup.champion();
            std::string title = tr("Champion") + (winner >= 0 ? ": " + s.players[winner].name : "");
            ui.label(260, 487, title, 24, 2);
        }
    }
    std::string bindingLabel(int side)
    {
        const std::string k = side ? "right_blobby_" : "left_blobby_";
        auto device = bindings.getString(k + "device");
        if (device == "gamepad")
        {
            return tr("Controller") + " " +
                   std::to_string(bindings.getInteger(k + "controller_slot", side) + 1);
        }
        return tr(device == "mouse"      ? "Mouse"
                  : device == "joystick" ? "Joystick"
                                         : "Keyboard") +
               " · " + std::to_string(side + 1);
    }
    void resetDevices()
    {
        for (int i = 0; i < 2; ++i)
        {
            devices[i] = owner.getInputMgr().beginGame(PlayerSide(i), &bindings);
            ready[i] = false;
            armed[i] = false;
        }
        readinessMatch.reset(new DuelMatch(false, "default.lua", 15));
        readyAfter = SDL_GetTicks() + 250;
    }
    void handoff()
    {
        // Jump confirms readiness; A/Enter must never also edit a focused widget.
        ui.selectActivates = false;
        auto& s = *owner.mSession;
        auto pair = s.nextPlayers();
        int players[] = {pair.first, pair.second};
        bool changed = false;
        for (int i = 0; i < 2; ++i)
        {
            auto& p = s.players[players[i]];
            float x = 40 + i * 370.f;
            WarmUI::panel(ui.g, x, 150, 350, 315);
            WarmUI::text(ui.g, x + 175, 170, p.name, 26, 0, 1);
            ui.g.doBlob(ui.g.getNextId(), Vector2(x + 175, 285),
                        p.cycling ? owner.getApp().getRenderManager().getOscillationColor() : Color(p.color),
                        1.0f);
            const std::string k = i ? "right_blobby_" : "left_blobby_";
            if (p.human)
            {
                const std::vector<std::string> dev = {"keyboard", "gamepad", "mouse", "joystick"};
                auto device = bindings.getString(k + "device");
                int n = int(std::find(dev.begin(), dev.end(), device) - dev.begin());
                if (int d = ui.row(x + 10, 365, 330, tr(i ? "Right seat" : "Left seat"), bindingLabel(i)))
                {
                    bindings.setString(k + "device", dev[wrap(n + d, 4)]);
                    changed = true;
                }
                if (device == "gamepad")
                {
                    int slot = bindings.getInteger(k + "controller_slot", i);
                    if (int d = ui.row(x + 10, 405, 330, tr("Controller"), std::to_string(slot + 1)))
                    {
                        bindings.setInteger(k + "controller_slot", wrap(slot + d, 2));
                        changed = true;
                    }
                }
                else
                {
                    ui.label(x + 15, 412, tr("Press jump to get ready."), 18);
                }
            }
            else
            {
                ready[i] = true;
                ui.label(x + 15, 390, tr("Bot") + " · " + p.script, 20);
            }
            WarmUI::text(ui.g, x + 175, 478, tr(ready[i] ? "Ready" : "Waiting"), 22, ready[i] ? 2 : 0, 1);
        }
        if (ui.button(40, 560, 180, tr("Controls")) || ui.in.previousTab() || ui.in.nextTab())
        {
            auto session = owner.mSession;
            owner.switchState(
                new OptionState([session]() -> State* { return new LocalSetupState(session, true); }, 1));
            return;
        }
        if (!devices[0] || changed)
        {
            resetDevices();
        }
        bool conflict = false;
        if (s.players[players[0]].human && s.players[players[1]].human)
        {
            auto a = bindings.getString("left_blobby_device"), b = bindings.getString("right_blobby_device");
            conflict =
                a == b && ((a == "gamepad" && bindings.getInteger("left_blobby_controller_slot", 0) ==
                                                  bindings.getInteger("right_blobby_controller_slot", 1)) ||
                           a == "mouse");
            if (a == "keyboard" && b == "keyboard")
            {
                for (const char* l : {"left", "right", "jump"})
                {
                    for (const char* r : {"left", "right", "jump"})
                    {
                        conflict |= bindings.getString(std::string("left_blobby_keyboard_") + l) ==
                                    bindings.getString(std::string("right_blobby_keyboard_") + r);
                    }
                }
            }
        }
        if (conflict)
        {
            ui.label(40, 523, tr("Choose separate controls for both players."), 18, 2);
            return;
        }
        if (SDL_TICKS_PASSED(SDL_GetTicks(), readyAfter))
        {
            bool released = true;
            for (int i = 0; i < 2; ++i)
            {
                if (s.players[players[i]].human)
                {
                    bool jump = devices[i]->transferInput().toPlayerInput(readinessMatch.get()).up;
                    released = released && !jump;
                    if (!jump)
                    {
                        armed[i] = true;
                    }
                    if (armed[i] && jump)
                    {
                        ready[i] = true;
                    }
                }
            }
            if (ready[0] && ready[1] && released)
            {
                owner.switchState(new LocalGameState(owner.mSession, bindings));
                return;
            }
        }
        ui.label(40, 523, tr("Share devices. Both players confirm with jump."), 18);
    }
    void step(bool interactive)
    {
        auto& s = *owner.mSession;
        bool network = owner.mDestination >= 0;
        WarmUI::page(ui.g,
                     tr(network          ? "Your network profile"
                        : owner.mHandoff ? "Next match"
                                         : "Local game"),
                     network ? "" : s.settings.getString("background", "strand1.bmp"));
        const bool hadNaming = naming;
        ui.begin(interactive && !naming);
        if (!network && !owner.mHandoff && editing < 0)
        {
            const char* names[] = {"Game", "Participants", "Overview"};
            if (!s.started)
            {
                if (ui.in.previousTab())
                {
                    tab = wrap(tab - 1, 3);
                    ui.focus = 0;
                }
                if (ui.in.nextTab())
                {
                    tab = wrap(tab + 1, 3);
                    ui.focus = 0;
                }
            }
            for (int i = 0; i < 3; ++i)
            {
                float x = 40 + i * 180.f;
                int id = ui.item(x, 87, 170, 34);
                WarmUI::panel(ui.g, x, 87, 170, 34, i == tab ? Color(255, 207, 91) : Color(65, 45, 29));
                WarmUI::text(ui.g, x + 85, 94, tr(names[i]), 20, i == tab ? 1 : 0, 1);
                if (ui.enabled && (!s.started || i == 2) &&
                    ((WarmUI::hit(ui.mouse, x, 87, 170, 34) && ui.in.click()) ||
                     (ui.focus == id && ui.in.select())))
                {
                    tab = i;
                    ui.focus = 3;
                    ui.drag = -1;
                }
            }
        }
        if (!network && !owner.mHandoff && !s.started && tab != 2)
        {
            s.cup.matches.clear();
            s.pending = -1;
        }
        try
        {
            if (network || editing >= 0)
            {
                editor();
            }
            else if (owner.mHandoff)
            {
                handoff();
            }
            else if (tab == 0)
            {
                game();
            }
            else if (tab == 1 && !s.started)
            {
                participants();
            }
            else
            {
                overview();
            }
        }
        catch (const std::exception& e)
        {
            notice = translateUI(WarmUI::language(), e.what());
        }
        if (!notice.empty())
        {
            ui.label(40, 554, notice, 16, 2);
        }
        if (ui.button(650, 560, 110, tr("Back")) || (interactive && ui.in.exit() && !naming && !hadNaming))
        {
            if (!network && editing >= 0)
            {
                editing = -1;
                ui.focus = 0;
            }
            else if (owner.mHandoff)
            {
                owner.switchState(new LocalSetupState(owner.mSession));
            }
            else
            {
                owner.switchState(new MainMenuState());
            }
        }
        ui.label(40, 584,
                 tr(owner.mHandoff ? "Left/right: device   Jump: ready   LB/RB: controls"
                                   : "D-pad: select / change    A: edit"),
                 15);
        ui.end();
    }
};
LocalSetupState::LocalSetupState(std::shared_ptr<LocalSession> s, bool handoff)
    : mSession(s ? s : std::make_shared<LocalSession>()), mHandoff(handoff)
{
}
LocalSetupState::LocalSetupState(int destination)
    : mSession(std::make_shared<LocalSession>()), mDestination(destination)
{
}
LocalSetupState::~LocalSetupState()
{
    if (mMenu && mMenu->naming)
    {
        SDL_StopTextInput();
    }
}
void LocalSetupState::init()
{
    mMenu.reset(new Menu(*this));
    if (mDestination < 0)
    {
        getApp().getRenderManager().setCourtWidth(
            mSession->settings.getBool("court_widescreen") ? 3200.f / 3.f : 800.f);
    }
    mMenu->step(false);
}
void LocalSetupState::step_impl()
{
    mMenu->step(true);
}
