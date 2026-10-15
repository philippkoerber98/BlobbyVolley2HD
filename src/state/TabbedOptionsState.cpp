#include "OptionsMenu.h"

OptionState::OptionState() = default;
OptionState::~OptionState() = default;
void OptionState::init()
{
    mMenu.reset(new Menu(*this));
    // State switches clear the old UI queue. Populate the new one immediately,
    // without handling the click/A/Enter that opened the options a second time.
    mMenu->step(false);
}
void OptionState::step_impl()
{
    mMenu->step();
}
const char* OptionState::getStateName() const
{
    return "OptionState";
}
