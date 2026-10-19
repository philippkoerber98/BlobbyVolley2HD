#pragma once
#include "NetworkMessage.h"
#include <stdexcept>
#include <string>
namespace tournament
{
enum class Command : unsigned char
{
    Hello,
    Welcome,
    State,
    Error,
    Configure,
    Enroll,
    Ready,
    Start,
    Claim,
    Return,
    Watch,
    StopWatching,
    Live,
    Restart,
    Award,
    Resume,
    NewCup,
    Draw,
    RemoveEntrant
};
inline void writePacket(RakNet::BitStream& stream, Command command, const std::string& payload)
{
    if (payload.size() > 262144)
    {
        throw std::runtime_error("Tournament packet too large");
    }
    stream.Write((unsigned char)ID_TOURNAMENT);
    stream.Write((unsigned char)command);
    stream.Write((unsigned)payload.size());
    if (!payload.empty())
    {
        stream.Write(payload.data(), int(payload.size()));
    }
}
inline std::string readPacket(RakNet::BitStream& stream, Command& command)
{
    unsigned char id, kind;
    unsigned length;
    if (!stream.Read(id) || id != ID_TOURNAMENT || !stream.Read(kind) ||
        kind > unsigned(Command::RemoveEntrant) || !stream.Read(length) || length > 262144 ||
        unsigned(stream.GetNumberOfUnreadBits()) < length * 8)
    {
        throw std::runtime_error("Invalid tournament packet");
    }
    command = Command(kind);
    std::string payload(length, '\0');
    if (length && !stream.Read(&payload[0], int(length)))
    {
        throw std::runtime_error("Truncated tournament packet");
    }
    return payload;
}
} // namespace tournament
