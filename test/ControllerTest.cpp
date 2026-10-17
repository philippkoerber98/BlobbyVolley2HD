#include "input_device/JoystickPool.h"
#include "input_device/ReleaseGate.h"
#include "input_device/MenuRepeat.h"
#include <SDL.h>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

int main(int, char**)
{
    try
    {
        MenuRepeat repeat;
        check(repeat.update(false, true, 0) == 1, "Slider initial press failed");
        check(repeat.update(false, true, 299) == 0, "Slider repeated too soon");
        check(repeat.update(false, true, 300) == 1 && repeat.update(false, true, 345) == 1,
              "Held slider did not repeat");
        check(repeat.update(true, false, 346) == -1, "Slider reversal did not react immediately");
        check(repeat.update(true, true, 347) == 0 && repeat.update(false, false, 348) == 0,
              "Opposing or released directions kept moving");
        check(repeat.update(false, true, 349) == 1, "Slider did not rearm after release");
        ReleaseGate gate;
        check(!gate.update(true) && !gate.update(true), "Held menu A caused jump");
        check(!gate.update(false) && gate.update(true) && gate.update(true), "Jump after release failed");
        check(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0, "SDL init failed");
        {
            int first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
            int second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
            check(first >= 0 && second >= 0, "Virtual devices failed");
            JoystickPool pool;
            pool.probeJoysticks();
            check(pool.getController(0) && pool.getController(1), "Missing controller slots");
            auto* left = SDL_GameControllerGetJoystick(pool.getController(0));
            int leftId = SDL_JoystickInstanceID(left);
            int rightId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pool.getController(1)));
            check(leftId != rightId, "Shared player slot");
            pool.openJoystick(first);
            check(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pool.getController(0))) == leftId,
                  "Duplicate add changed slot");
            SDL_JoystickSetVirtualAxis(left, 0, -25000);
            SDL_JoystickSetVirtualButton(left, 0, 1);
            SDL_JoystickUpdate();
            check(SDL_GameControllerGetAxis(pool.getController(0), SDL_CONTROLLER_AXIS_LEFTX) < -15000,
                  "Stick mapping failed");
            check(SDL_GameControllerGetButton(pool.getController(0), SDL_CONTROLLER_BUTTON_A),
                  "A mapping failed");
            check(!SDL_GameControllerGetButton(pool.getController(1), SDL_CONTROLLER_BUTTON_A),
                  "Input leaked to player 2");
            SDL_JoystickDetachVirtual(first);
            pool.closeJoystick(leftId);
            check(!pool.getController(0) && pool.getController(1), "Disconnect moved other player");
            int replacement = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
            pool.openJoystick(replacement);
            check(pool.getController(0), "Reconnect failed");
            check(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pool.getController(1))) == rightId,
                  "Reconnect moved other player");
        }
        SDL_Quit();
        std::cout << "Controller mapping, reconnect and release gate passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << ": " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
}
