#pragma once
#include <cstdint>

// Immediate first step, then a deliberate delay and a time-based repeat rate.
class MenuRepeat
{
  public:
    int update(bool left, bool right, uint32_t now)
    {
        const int direction = int(right) - int(left);
        if (!direction)
        {
            mDirection = 0;
            return 0;
        }
        if (direction != mDirection)
        {
            mDirection = direction;
            mNext = now + 300;
            return direction;
        }
        if (int32_t(now - mNext) >= 0)
        {
            mNext = now + 45;
            return direction;
        }
        return 0;
    }

  private:
    int mDirection = 0;
    uint32_t mNext = 0;
};
