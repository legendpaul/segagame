#include "input_mgr.h"

/* Both pads are polled every frame. The single-pad helpers stay as player 1 so
 * every existing caller is unchanged; the _p variants take a pad index. */
static u16 curState[2];
static u16 prevState[2];

void input_mgr_update(void)
{
    prevState[0] = curState[0];
    prevState[1] = curState[1];
    curState[0] = JOY_readJoypad(JOY_1);
    curState[1] = JOY_readJoypad(JOY_2);
}

bool input_held_p(u8 pad, u16 button)
{
    return (curState[pad & 1] & button) ? TRUE : FALSE;
}

bool input_pressed_p(u8 pad, u16 button)
{
    u8 i = pad & 1;
    return ((curState[i] & button) && !(prevState[i] & button)) ? TRUE : FALSE;
}

bool input_held(u16 button)
{
    return input_held_p(0, button);
}

bool input_pressed(u16 button)
{
    return input_pressed_p(0, button);
}

bool input_pressed_any(u16 buttons)
{
    return ((curState[0] & (u16)~prevState[0] & buttons) != 0) ? TRUE : FALSE;
}
