#include <string.h>
#include "madaudio.h"

bool
madaudio_command(madaudio_player_t* player, const char* cmd)
{
    if(!strcmp(cmd, "raise"))
        return true;
    return false;
}

