#include <Eina.h>
#include "empd.h"

void
empd_playlist_append(empd_connection_t* conn, const struct mpd_song* song)
{
    conn->playlist = eina_list_append(conn->playlist,
        mpd_song_dup(song));
}

void
empd_playlist_clear(empd_connection_t* conn)
{
    struct mpd_song* song;
    EINA_LIST_FREE(conn->playlist, song)
        mpd_song_free(song);
    conn->playlist = NULL;
}
