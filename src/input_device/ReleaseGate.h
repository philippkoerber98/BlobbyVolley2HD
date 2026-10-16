#pragma once

class ReleaseGate
{
  public:
    bool update(bool pressed)
    {
        if (!pressed)
        {
            mReady = true;
        }
        return mReady && pressed;
    }

  private:
    bool mReady = false;
};
