#pragma once
#include <algorithm>
#include <cmath>

// Wall-clock accumulation only. Physics still advances by its original unit step.
class SimulationClock
{
  public:
    void advance(double seconds)
    {
        mPending += (std::max)(0.0, (std::min)(seconds, 0.25));
    }
    bool consume(double rate)
    {
        const double step = 1.0 / (std::max)(5.0, rate);
        if (mPending + 1e-10 < step)
        {
            return false;
        }
        mPending = (std::max)(0.0, mPending - step);
        return true;
    }
    float alpha(double rate) const
    {
        return float((std::max)(0.0, (std::min)(1.0, mPending * rate)));
    }
    double untilNextStep(double rate) const
    {
        return (std::max)(0.0, 1.0 / (std::max)(5.0, rate) - mPending);
    }

  private:
    double mPending = 0;
};
