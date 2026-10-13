-- Widescreen-only lifecycle safety; the selected bot retains its own OnGame.
local originalStep = __OnStep
local serveTicks = 0
function __OnStep()
 if not is_ball_valid() then
  serveTicks = 0
  __WANT_LEFT, __WANT_RIGHT, __WANT_JUMP = false, false, false
  moveto(CONST_FIELD_WIDTH/4)
  return
 end
 if not is_game_running() then
  local server = get_serving_player()
  local mine = server == LEFT_PLAYER or (server == NO_PLAYER and ballx() < CONST_FIELD_MIDDLE)
  if mine then serveTicks = serveTicks + 1 else serveTicks = 0 end
  originalStep()
  -- Rescue only a stalled serve, not the bot's normal tactical decisions.
  if mine and serveTicks > 240 then
   __WANT_LEFT, __WANT_RIGHT, __WANT_JUMP = false, false, false
   if moveto(ballx()-22) then jump() end
  end
 else
  serveTicks = 0
  originalStep()
 end
end
