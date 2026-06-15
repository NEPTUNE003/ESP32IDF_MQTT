#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

#include <stdbool.h>

extern volatile bool g_music_next;
extern volatile bool g_music_prev;
extern volatile bool g_music_playpause;
extern volatile bool g_music_exit_req;

#endif /* AUDIO_COMMON_H */