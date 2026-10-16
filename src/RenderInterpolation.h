#pragma once
#include "DuelMatchState.h"
#include <algorithm>
#include <cmath>

// Interpolate a COPY for display; never feed these positions back into physics,
// recordings, bots or network messages. Reset transitions must not leave trails.
inline DuelMatchState interpolateMatch(const DuelMatchState& previous, const DuelMatchState& current,
                                       float alpha)
{
    auto result = current;
    if (previous.logicState.leftScore != current.logicState.leftScore ||
        previous.logicState.rightScore != current.logicState.rightScore ||
        previous.logicState.servingPlayer != current.logicState.servingPlayer ||
        previous.logicState.isBallValid != current.logicState.isBallValid ||
        (current.worldState.ballPosition - previous.worldState.ballPosition).lengthSQ() > 160 * 160)
    {
        return result;
    }
    alpha = (std::max)(0.f, (std::min)(1.f, alpha));
    result.worldState.ballPosition =
        previous.worldState.ballPosition +
        (current.worldState.ballPosition - previous.worldState.ballPosition) * alpha;
    const float tau = 6.283185307179586f;
    float angle = std::remainder(current.worldState.ballRotation - previous.worldState.ballRotation, tau);
    result.worldState.ballRotation = std::fmod(previous.worldState.ballRotation + angle * alpha + tau, tau);
    for (int side = 0; side < MAX_PLAYERS; ++side)
    {
        if ((current.worldState.blobPosition[side] - previous.worldState.blobPosition[side]).lengthSQ() <
            160 * 160)
        {
            result.worldState.blobPosition[side] =
                previous.worldState.blobPosition[side] +
                (current.worldState.blobPosition[side] - previous.worldState.blobPosition[side]) * alpha;
        }
    }
    return result;
}
