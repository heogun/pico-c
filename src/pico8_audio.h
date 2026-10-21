#ifndef PICO8_AUDIO_H
#define PICO8_AUDIO_H

/*
 * pico8_audio.h — the sound of the PICO-8.
 *
 * The synthesis itself lives in src/audio.cpp.  The functions below start
 * a sound effect or a song, stop them again, and answer the questions the
 * stat() function asks about the audio state.
 *
 * 실제 합성은 src/audio.cpp에 있다. 아래 함수는 효과음이나 song을 시작하고
 * 멈추며, stat()이 오디오 상태에 대해 묻는 질문에 답한다.
 */

#include "pico8_core.h"

/* ------------------------------------------------------------------ *
 *  Playback
 * ------------------------------------------------------------------ */

/* Open the audio device.  Returns 0 on success. */
int  pico8_audio_init(void);

/* Close the audio device. */
void pico8_audio_destroy(void);

/*
 * Start a sound effect.
 *
 * sfx_id   the sound effect, 0..63
 * channel  the channel to play it on, 0..3
 * offset   where in the effect to start, in samples
 * length   how long to play it, in samples
 */
void pico8_audio_sfx(int sfx_id, int channel, int offset, int length);

/*
 * Start a song.
 *
 * song_id       the song, 0..63
 * fade_len      the length of the fade in, in ticks (0 for none)
 * channel_mask  which channels the song may use
 */
void pico8_audio_music(int song_id, int fade_len, int channel_mask);

/* Stop everything that is playing. */
void pico8_audio_stop(void);

/* ------------------------------------------------------------------ *
 *  State, as stat() wants to know it
 * ------------------------------------------------------------------ */

/* The sound effect playing on a channel, or -1 when the channel is quiet. */
int pico8_audio_sfx_id(int channel);

/* The note of the sound effect playing on a channel, or -1. */
int pico8_audio_note_num(int channel);

/* The pattern of the playing song, or -1 when nothing plays. */
int pico8_audio_music_pattern(void);        /* stat(54), or -1 */
int pico8_audio_music_count(void);          /* stat(55) */
int pico8_audio_music_ticks(void);          /* stat(56), or -1 */
int pico8_audio_music_playing(void);        /* stat(57) */

#endif /* PICO8_AUDIO_H */
