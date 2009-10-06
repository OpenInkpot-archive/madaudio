#include <stdio.h>
#include <string.h>
#include "madaudio.h"

bool
madaudio_command(madaudio_player_t* player, const char* cmd)
{
    printf("madaudio: remote command: %s\n", cmd);
    if(!strcmp(cmd, "raise"))
        return true;
    if(!strcmp(cmd, "play"))
        madaudio_play(player, -1);
    else if (!strcmp(cmd, "pause"))
        madaudio_pause(player);
    else if (!strcmp(cmd, "playpause"))
        madaudio_play_pause(player);
    else
        return true;
    return false;
}

