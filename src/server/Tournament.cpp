#include "Tournament.h"
#include "tinyxml2.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>

namespace tournament
{
namespace
{
void check(bool valid, const char* error)
{
    if (!valid)
    {
        throw std::runtime_error(error);
    }
}
const char* text(const tinyxml2::XMLElement* e, const char* key)
{
    const auto* value = e->Attribute(key);
    return value ? value : "";
}
bool done(const Match& m)
{
    return m.status == MatchStatus::Finished || m.status == MatchStatus::Bye;
}
} // namespace
Person& Tournament::person(int id)
{
    check(id >= 0 && id < int(people.size()), "Invalid participant");
    return people[id];
}
const Person& Tournament::person(int id) const
{
    return const_cast<Tournament*>(this)->person(id);
}
Match& Tournament::match(int id)
{
    check(id >= 0 && id < int(matches.size()), "Invalid match");
    return matches[id];
}
const Match& Tournament::match(int id) const
{
    return const_cast<Tournament*>(this)->match(id);
}
int Tournament::registerPerson(const std::string& name, int color, const std::string& token)
{
    check(people.size() < 128, "Tournament visitor history is full");
    Person p;
    p.id = int(people.size());
    p.name = name.substr(0, 63);
    p.color = color;
    p.token = token;
    p.connected = true;
    people.push_back(p);
    ++revision;
    return p.id;
}
void Tournament::enroll(int id, bool enter)
{
    check(phase == Phase::Registration, "Registration is closed");
    auto& p = person(id);
    if (enter && !p.entered)
    {
        int count = 0;
        for (const auto& other : people)
        {
            count += other.entered;
        }
        check(count < settings.capacity, "Tournament is full");
    }
    matches.clear();
    p.entered = enter;
    p.ready = false;
    ++revision;
}
int Tournament::groupCount() const
{
    int count = 0;
    for (const auto& p : people)
    {
        count += p.entered;
    }
    return settings.format == Format::Groups ? (count > 16 ? 4 : 2) : 0;
}
std::vector<int> Tournament::groupMembers(int group) const
{
    std::vector<int> result;
    for (const auto& p : people)
    {
        if (p.entered && p.group == group)
        {
            result.push_back(p.id);
        }
    }
    return result;
}
void Tournament::roundRobin(const std::vector<int>& players, int group)
{
    auto ring = players;
    if (ring.size() % 2)
    {
        ring.push_back(-1);
    }
    for (int round = 0; round < int(ring.size()) - 1; ++round)
    {
        for (int i = 0; i < int(ring.size()) / 2; ++i)
        {
            int a = ring[i], b = ring[ring.size() - 1 - i];
            if (a < 0 || b < 0)
            {
                continue;
            }
            Match m;
            m.id = int(matches.size());
            m.a = a;
            m.b = b;
            m.round = round;
            m.group = group;
            matches.push_back(m);
        }
        std::rotate(ring.begin() + 1, ring.end() - 1, ring.end());
    }
}
void Tournament::bracket(const std::vector<int>& players)
{
    int size = 2;
    while (size < int(players.size()))
    {
        size *= 2;
    }
    std::vector<int> order{0, 1};
    for (int n = 4; n <= size; n *= 2)
    {
        std::vector<int> next;
        for (int x : order)
        {
            next.push_back(x);
            next.push_back(n - 1 - x);
        }
        order = std::move(next);
    }
    std::vector<int> previous;
    for (int i = 0; i < size; i += 2)
    {
        Match m;
        m.id = int(matches.size());
        m.knockout = true;
        m.a = order[i] < int(players.size()) ? players[order[i]] : -1;
        m.b = order[i + 1] < int(players.size()) ? players[order[i + 1]] : -1;
        previous.push_back(m.id);
        matches.push_back(m);
    }
    for (int round = 1; previous.size() > 1; ++round)
    {
        std::vector<int> next;
        for (size_t i = 0; i < previous.size(); i += 2)
        {
            Match m;
            m.id = int(matches.size());
            m.knockout = true;
            m.round = round;
            m.fromA = previous[i];
            m.fromB = previous[i + 1];
            next.push_back(m.id);
            matches.push_back(m);
        }
        if (previous.size() == 2 && settings.thirdPlace)
        {
            bronze(previous[0], previous[1], round);
        }
        previous = std::move(next);
    }
}
void Tournament::start(unsigned randomSeed)
{
    check(phase == Phase::Registration, "Tournament already started");
    check(settings.capacity >= 2 && settings.capacity <= MaxPeople && settings.parallel >= 1 &&
              settings.parallel <= 16,
          "Invalid tournament limits");
    check(settings.points >= 1 && settings.points <= 50 && settings.speed >= 30 && settings.speed <= 120,
          "Invalid match settings");
    std::vector<int> ids;
    for (const auto& p : people)
    {
        if (p.entered)
        {
            check(p.connected && p.ready, "All entrants must be connected and ready");
            ids.push_back(p.id);
        }
    }
    check(ids.size() >= 2 && ids.size() <= size_t(settings.capacity), "Choose between 2 and 32 entrants");
    check(settings.format != Format::Groups || ids.size() >= 6, "Groups need at least six entrants");
    seed = int(randomSeed & 0x7fffffff);
    std::mt19937 rng{unsigned(seed)};
    std::shuffle(ids.begin(), ids.end(), rng);
    for (int i = 0; i < int(ids.size()); ++i)
    {
        person(ids[i]).draw = i;
        person(ids[i]).group = settings.format == Format::Groups ? i % groupCount() : -1;
    }
    matches.clear();
    if (settings.format == Format::Knockout)
    {
        bracket(ids);
    }
    else if (settings.format == Format::RoundRobin)
    {
        roundRobin(ids, -1);
    }
    else
    {
        for (int g = 0; g < groupCount(); ++g)
        {
            roundRobin(groupMembers(g), g);
        }
    }
    phase = Phase::Running;
    ++revision;
    advance();
}
std::vector<Standing> Tournament::standings(int group) const
{
    std::vector<int> members;
    if (group >= 0)
    {
        members = groupMembers(group);
    }
    else
    {
        for (const auto& p : people)
        {
            if (p.entered)
            {
                members.push_back(p.id);
            }
        }
    }
    std::vector<Standing> rows;
    for (int id : members)
    {
        Standing r;
        r.person = id;
        for (const auto& m : matches)
        {
            if (!m.knockout && m.group == group && m.status == MatchStatus::Finished &&
                (m.a == id || m.b == id))
            {
                ++r.played;
                r.wins += m.winner == id;
                if (!m.forfeit)
                {
                    r.scored += m.a == id ? m.scoreA : m.scoreB;
                    r.conceded += m.a == id ? m.scoreB : m.scoreA;
                }
            }
        }
        rows.push_back(r);
    }
    // A mini-table of all players tied on match wins avoids a non-transitive pairwise comparator.
    auto mini = [&](const Standing& row)
    {
        int wins = 0, diff = 0, points = 0;
        std::set<int> tied;
        for (const auto& r : rows)
        {
            if (r.wins == row.wins)
            {
                tied.insert(r.person);
            }
        }
        for (const auto& m : matches)
        {
            if (!m.knockout && m.group == group && m.status == MatchStatus::Finished && tied.count(m.a) &&
                tied.count(m.b) && (m.a == row.person || m.b == row.person))
            {
                wins += m.winner == row.person;
                if (!m.forfeit)
                {
                    int a = m.a == row.person ? m.scoreA : m.scoreB,
                        b = m.a == row.person ? m.scoreB : m.scoreA;
                    diff += a - b;
                    points += a;
                }
            }
        }
        return std::vector<int>{wins, diff, points};
    };
    // Compute keys before sorting; the sort must never inspect its mutating range.
    std::vector<std::vector<int>> keys(people.size());
    for (const auto& row : rows)
    {
        auto k = mini(row);
        k.insert(k.begin(), row.wins);
        k.push_back(row.scored - row.conceded);
        k.push_back(row.scored);
        // Persisted draw order is the final, announced tie-break for a completely equal table.
        k.push_back(-person(row.person).draw);
        keys[row.person] = k;
    }
    std::sort(rows.begin(), rows.end(),
              [&](const Standing& a, const Standing& b) { return keys[a.person] > keys[b.person]; });
    return rows;
}
void Tournament::advance()
{
    for (auto& m : matches)
    {
        if (m.knockout && m.status == MatchStatus::Waiting)
        {
            if (m.fromA >= 0)
            {
                if (!done(match(m.fromA)))
                {
                    continue;
                }
                const auto& parent = match(m.fromA);
                m.a = m.placement ? (parent.winner == parent.a ? parent.b : parent.a) : parent.winner;
            }
            if (m.fromB >= 0)
            {
                if (!done(match(m.fromB)))
                {
                    continue;
                }
                const auto& parent = match(m.fromB);
                m.b = m.placement ? (parent.winner == parent.a ? parent.b : parent.a) : parent.winner;
            }
            if (m.a < 0 || m.b < 0)
            {
                m.winner = m.a >= 0 ? m.a : m.b;
                m.status = MatchStatus::Bye;
            }
        }
    }
    bool allDone = !matches.empty() && std::all_of(matches.begin(), matches.end(), done);
    const bool hasBracket =
        std::any_of(matches.begin(), matches.end(), [](const Match& m) { return m.knockout; });
    if (allDone && settings.format == Format::Groups && !hasBracket)
    {
        std::vector<int> qualified;
        // Cross groups: A1-B2 and B1-A2 (likewise C/D with four groups).
        for (int g = 0; g < groupCount(); g += 2)
        {
            auto a = standings(g), b = standings(g + 1);
            qualified.push_back(a[0].person);
            qualified.push_back(b[1].person);
            qualified.push_back(b[0].person);
            qualified.push_back(a[1].person);
        }
        // Pair in this explicit cross-group order rather than re-seeding qualified players.
        int base = int(matches.size());
        for (size_t i = 0; i < qualified.size(); i += 2)
        {
            Match m;
            m.id = int(matches.size());
            m.a = qualified[i];
            m.b = qualified[i + 1];
            m.knockout = true;
            matches.push_back(m);
        }
        std::vector<int> prev;
        for (int i = base; i < int(matches.size()); ++i)
        {
            prev.push_back(i);
        }
        for (int round = 1; prev.size() > 1; ++round)
        {
            std::vector<int> next;
            for (size_t i = 0; i < prev.size(); i += 2)
            {
                Match m;
                m.id = int(matches.size());
                m.knockout = true;
                m.round = round;
                m.fromA = prev[i];
                m.fromB = prev[i + 1];
                next.push_back(m.id);
                matches.push_back(m);
            }
            if (prev.size() == 2 && settings.thirdPlace)
            {
                bronze(prev[0], prev[1], round);
            }
            prev = next;
        }
        allDone = false;
    }
    if (allDone && settings.format == Format::RoundRobin && settings.finalMatch && !hasBracket)
    {
        auto rows = standings();
        bracket({rows[0].person, rows[1].person});
        allDone = false;
    }
    if (allDone)
    {
        phase = Phase::Finished;
    }
}
void Tournament::result(int id, int winner, int scoreA, int scoreB, bool forfeit)
{
    auto& m = match(id);
    check(phase == Phase::Running &&
              (m.status == MatchStatus::Playing || m.status == MatchStatus::Interrupted),
          "Match has no pending result");
    check((winner == m.a || winner == m.b) && scoreA >= 0 && scoreB >= 0, "Invalid result");
    check(forfeit || (scoreA != scoreB && (scoreA > scoreB ? m.a : m.b) == winner),
          "Score contradicts winner");
    m.winner = winner;
    m.scoreA = scoreA;
    m.scoreB = scoreB;
    m.forfeit = forfeit;
    m.status = MatchStatus::Finished;
    person(m.a).ready = false;
    person(m.b).ready = false;
    ++revision;
    advance();
}
void Tournament::interrupt(int id)
{
    auto& m = match(id);
    if (m.status == MatchStatus::Playing)
    {
        m.status = MatchStatus::Interrupted;
        person(m.a).ready = false;
        person(m.b).ready = false;
        ++revision;
    }
}
void Tournament::restart(int id)
{
    auto& m = match(id);
    check(m.status == MatchStatus::Interrupted, "Only interrupted matches can restart");
    m.status = MatchStatus::Waiting;
    ++revision;
}
std::vector<int> Tournament::available() const
{
    std::vector<int> result;
    std::set<int> busy;
    int active = 0;
    for (const auto& m : matches)
    {
        if (m.status == MatchStatus::Playing || m.status == MatchStatus::Interrupted)
        {
            busy.insert(m.a);
            busy.insert(m.b);
            ++active;
        }
    }
    for (const auto& m : matches)
    {
        if (m.status == MatchStatus::Waiting && m.a >= 0 && m.b >= 0 && !busy.count(m.a) &&
            !busy.count(m.b) && person(m.a).connected && person(m.b).connected && person(m.a).ready &&
            person(m.b).ready)
        {
            if (active >= settings.parallel)
            {
                break;
            }
            result.push_back(m.id);
            busy.insert(m.a);
            busy.insert(m.b);
            ++active;
        }
    }
    return result;
}
std::string Tournament::xml(bool privateData) const
{
    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("tournament");
    doc.InsertEndChild(root);
    root->SetAttribute("slots", slots);
    root->SetAttribute("version", 1);
    root->SetAttribute("revision", revision);
    root->SetAttribute("phase", int(phase));
    root->SetAttribute("organizer", organizer);
    root->SetAttribute("seed", seed);
    root->SetAttribute("name", settings.name.c_str());
    root->SetAttribute("rules", settings.rules.c_str());
    root->SetAttribute("format", int(settings.format));
    root->SetAttribute("capacity", settings.capacity);
    root->SetAttribute("thirdPlace", settings.thirdPlace);
    root->SetAttribute("finalMatch", settings.finalMatch);
    root->SetAttribute("parallel", settings.parallel);
    root->SetAttribute("points", settings.points);
    root->SetAttribute("speed", settings.speed);
    for (const auto& p : people)
    {
        auto* e = doc.NewElement("person");
        root->InsertEndChild(e);
        e->SetAttribute("id", p.id);
        e->SetAttribute("name", p.name.c_str());
        e->SetAttribute("color", p.color);
        e->SetAttribute("group", p.group);
        e->SetAttribute("draw", p.draw);
        e->SetAttribute("entered", p.entered);
        e->SetAttribute("connected", p.connected);
        e->SetAttribute("ready", p.ready);
        e->SetAttribute("cycling", p.cycling);
        if (privateData)
        {
            e->SetAttribute("token", p.token.c_str());
        }
    }
    for (const auto& m : matches)
    {
        auto* e = doc.NewElement("match");
        root->InsertEndChild(e);
        e->SetAttribute("id", m.id);
        e->SetAttribute("a", m.a);
        e->SetAttribute("b", m.b);
        e->SetAttribute("fromA", m.fromA);
        e->SetAttribute("fromB", m.fromB);
        e->SetAttribute("round", m.round);
        e->SetAttribute("group", m.group);
        e->SetAttribute("winner", m.winner);
        e->SetAttribute("scoreA", m.scoreA);
        e->SetAttribute("scoreB", m.scoreB);
        e->SetAttribute("placement", m.placement);
        e->SetAttribute("knockout", m.knockout);
        e->SetAttribute("forfeit", m.forfeit);
        e->SetAttribute("status", int(m.status));
    }
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}
Tournament Tournament::parse(const std::string& xml, bool privateData)
{
    check(xml.size() < 262144, "Tournament document too large");
    tinyxml2::XMLDocument doc;
    check(doc.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS, "Invalid tournament document");
    auto* root = doc.FirstChildElement("tournament");
    check(root && root->IntAttribute("version") == 1, "Unsupported tournament version");
    Tournament t;
    t.slots = root->IntAttribute("slots", 16);
    check(t.slots >= 2 && t.slots <= 32, "Invalid server slots");
    t.revision = root->UnsignedAttribute("revision");
    int phase = root->IntAttribute("phase"), format = root->IntAttribute("format");
    check(phase >= 0 && phase <= 2 && format >= 0 && format <= 2, "Invalid tournament enum");
    t.phase = Phase(phase);
    t.settings.format = Format(format);
    t.organizer = root->IntAttribute("organizer", -1);
    t.seed = root->IntAttribute("seed");
    t.settings.name = text(root, "name");
    t.settings.rules = text(root, "rules");
    check(!t.settings.name.empty() && t.settings.name.size() <= 63 && t.settings.rules.size() <= 255,
          "Invalid saved settings");
    t.settings.capacity = root->IntAttribute("capacity");
    t.settings.thirdPlace = root->BoolAttribute("thirdPlace");
    t.settings.finalMatch = root->BoolAttribute("finalMatch");
    t.settings.parallel = root->IntAttribute("parallel");
    t.settings.points = root->IntAttribute("points");
    t.settings.speed = root->IntAttribute("speed");
    check(t.settings.capacity >= 2 && t.settings.capacity <= 32 && t.settings.parallel >= 1 &&
              t.settings.parallel <= 16 && t.settings.points >= 1 && t.settings.points <= 99 &&
              t.settings.speed >= 30 && t.settings.speed <= 120,
          "Invalid saved settings");
    for (auto* e = root->FirstChildElement("person"); e; e = e->NextSiblingElement("person"))
    {
        check(t.people.size() < 128, "Too many visitors");
        Person p;
        p.id = e->IntAttribute("id", -1);
        check(p.id == int(t.people.size()), "Invalid participant ID");
        p.name = text(e, "name");
        p.color = e->IntAttribute("color");
        p.group = e->IntAttribute("group", -1);
        p.draw = e->IntAttribute("draw", p.id);
        p.entered = e->BoolAttribute("entered");
        p.connected = e->BoolAttribute("connected");
        p.ready = e->BoolAttribute("ready");
        p.cycling = e->BoolAttribute("cycling");
        if (privateData)
        {
            p.token = text(e, "token");
        }
        check(p.name.size() <= 63 && p.token.size() <= 128, "Invalid participant data");
        t.people.push_back(p);
    }
    for (auto* e = root->FirstChildElement("match"); e; e = e->NextSiblingElement("match"))
    {
        check(t.matches.size() < 600, "Too many matches");
        Match m;
        m.id = e->IntAttribute("id", -1);
        m.a = e->IntAttribute("a", -1);
        m.b = e->IntAttribute("b", -1);
        m.fromA = e->IntAttribute("fromA", -1);
        m.fromB = e->IntAttribute("fromB", -1);
        m.round = e->IntAttribute("round");
        m.group = e->IntAttribute("group", -1);
        m.winner = e->IntAttribute("winner", -1);
        m.scoreA = e->IntAttribute("scoreA");
        m.scoreB = e->IntAttribute("scoreB");
        m.placement = e->BoolAttribute("placement");
        m.knockout = e->BoolAttribute("knockout");
        m.forfeit = e->BoolAttribute("forfeit");
        int status = e->IntAttribute("status");
        check(m.id == int(t.matches.size()) && m.a >= -1 && m.b >= -1 && m.a < int(t.people.size()) &&
                  m.b < int(t.people.size()) && m.fromA >= -1 && m.fromB >= -1 && m.fromA < m.id &&
                  m.fromB < m.id && status >= 0 && status <= 4,
              "Invalid match record");
        check(m.winner >= -1 && m.winner < int(t.people.size()) && m.round >= 0 && m.round <= (m.knockout ? 6 : 31) &&
                  m.group >= -1 && m.group <= 3 && m.scoreA >= 0 && m.scoreB >= 0,
              "Invalid match record");
        check(status != int(MatchStatus::Finished) || (m.winner >= 0 && (m.winner == m.a || m.winner == m.b)),
              "Invalid result");
        m.status = MatchStatus(status);
        t.matches.push_back(m);
    }
    check(t.organizer >= -1 && t.organizer < int(t.people.size()), "Invalid organizer");
    return t;
}
void Tournament::recover()
{
    for (auto& p : people)
    {
        p.connected = false;
        p.ready = false;
    }
    for (auto& m : matches)
    {
        if (m.status == MatchStatus::Playing)
        {
            m.status = MatchStatus::Interrupted;
        }
    }
    ++revision;
}

void Tournament::bronze(int first, int second, int round)
{
    Match m;
    m.id = int(matches.size());
    m.knockout = true;
    m.placement = true;
    m.fromA = first;
    m.fromB = second;
    m.round = round;
    matches.push_back(m);
}
void Tournament::preview(unsigned randomSeed)
{
    check(phase == Phase::Registration, "Registration is closed");
    auto copy = *this;
    for (auto& p : copy.people)
    {
        p.connected = true;
        p.ready = true;
    }
    copy.start(randomSeed);
    matches = copy.matches;
    seed = copy.seed;
    for (size_t i = 0; i < people.size(); ++i)
    {
        {
            people[i].group = copy.people[i].group;
            people[i].draw = copy.people[i].draw;
        }
    }
    ++revision;
}
int Tournament::champion() const
{
    if (phase != Phase::Finished)
    {
        return -1;
    }
    const Match* final = nullptr;
    for (const auto& m : matches)
    {
        if (m.knockout && !m.placement && (!final || m.round > final->round))
        {
            final = &m;
        }
    }
    if (final)
    {
        return final->winner;
    }
    auto rows = standings();
    return rows.empty() ? -1 : rows[0].person;
}
} // namespace tournament
