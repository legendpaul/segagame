#include "input_mgr.h"

static u16 curState;
static u16 prevState;

void input_mgr_update(void)
{
    prevState = curState;
    curState = JOY_readJoypad(JOY_1);
}

bool input_held(u16 button)
{
    return (curState & button) ? TRUE : FALSE;
}

bool input_pressed(u16 button)
{
    return ((curState & button) && !(prevState & button)) ? TRUE : FALSE;
}
