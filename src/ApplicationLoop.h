#pragma once

class UserConfig;
class SpeedController;

// Fixed simulation steps and independently paced rendering.
void runApplicationLoop(UserConfig& gameConfig, SpeedController& scontroller);
