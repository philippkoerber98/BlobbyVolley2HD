#pragma once
#include "State.h"
#include "LocalSession.h"
#include <memory>

// The same setup/editor is used for guests, saved profiles and network identity.
class LocalSetupState : public State
{
  public:
    explicit LocalSetupState(std::shared_ptr<LocalSession> session = nullptr, bool handoff = false);
    // destination: 0 = profile only, 1 = LAN, 2 = online.
    explicit LocalSetupState(int destination);
    ~LocalSetupState() override;
    void init() override;
    void step_impl() override;
    const char* getStateName() const override
    {
        return "LocalSetupState";
    }

  private:
    struct Menu;
    std::unique_ptr<Menu> mMenu;
    std::shared_ptr<LocalSession> mSession;
    int mDestination = -1;
    bool mHandoff = false;
};
