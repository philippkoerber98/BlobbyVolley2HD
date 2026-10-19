#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tournament
{
constexpr int MaxPeople = 32;
enum class Format
{
    RoundRobin,
    Knockout,
    Groups
};
enum class Phase
{
    Registration,
    Running,
    Finished
};
enum class MatchStatus
{
    Waiting,
    Playing,
    Finished,
    Bye,
    Interrupted
};
struct Person
{
    int id = -1;
    std::string name, token;
    int color = 0, group = -1, draw = 0;
    bool entered = false, connected = false, ready = false, cycling = false;
};
struct Match
{
    int id = -1, a = -1, b = -1, fromA = -1, fromB = -1;
    int round = 0, group = -1, winner = -1, scoreA = 0, scoreB = 0;
    bool knockout = false, forfeit = false, placement = false;
    MatchStatus status = MatchStatus::Waiting;
};
struct Standing
{
    int person = -1, played = 0, wins = 0, scored = 0, conceded = 0;
};
struct Settings
{
    std::string name = "LAN Cup", rules = "default.lua";
    Format format = Format::Knockout;
    bool thirdPlace = false, finalMatch = false;
    int capacity = 16, parallel = 4, points = 15, speed = 75;
};
// Pure competition state. No sockets, renderer, clocks or simulation steps.
class Tournament
{
  public:
    Settings settings;
    Phase phase = Phase::Registration;
    unsigned revision = 0;
    int organizer = -1, seed = 0, slots = 16;
    std::vector<Person> people;
    std::vector<Match> matches;
    Person& person(int id);
    const Person& person(int id) const;
    Match& match(int id);
    const Match& match(int id) const;
    int registerPerson(const std::string& name, int color, const std::string& token);
    void enroll(int id, bool enter);
    void start(unsigned randomSeed);
    void preview(unsigned randomSeed);
    int champion() const;
    void result(int match, int winner, int scoreA, int scoreB, bool forfeit = false);
    void interrupt(int match);
    void restart(int match);
    std::vector<int> available() const;
    std::vector<Standing> standings(int group = -1) const;
    std::vector<int> groupMembers(int group) const;
    int groupCount() const;
    std::string xml(bool privateData = false) const;
    static Tournament parse(const std::string& xml, bool privateData = false);
    void recover();

  private:
    void roundRobin(const std::vector<int>& players, int group);
    void bracket(const std::vector<int>& players);
    void advance();
    void bronze(int first, int second, int round);
};
} // namespace tournament
