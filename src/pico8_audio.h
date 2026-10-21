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

/*
 * Open the audio device.  Returns 0 on success.
 *
 * 오디오 장치를 연다. 성공하면 0.
 */
int  pico8_audio_init(void);

/*
 * Close the audio device.
 *
 * 오디오 장치를 닫는다.
 */
void pico8_audio_destroy(void);

/*
 * Start a sound effect.
 *
 * sfx_id   the sound effect, 0..63
 * channel  the channel to play it on, 0..3
 * offset   where in the effect to start, in samples
 * length   how long to play it, in samples
 *
 * sfx_id   효과음 번호. 0~63
 * channel  어느 채널에서 연주할지. 0~3
 * offset   효과음의 몇 샘플부터 시작할지
 * length   몇 샘플 길이 연주할지
 */
void pico8_audio_sfx(int sfx_id, int channel, int offset, int length);

/*
 * Start a song.
 *
 * song_id       the song, 0..63
 * fade_len      the length of the fade in, in ticks (0 for none)
 * channel_mask  which channels the song may use
 *
 * song_id       song 번호. 0~63
 * fade_len      페이드인 길이(틱 단위). 0이면 없음
 * channel_mask  song이 쓸 수 있는 채널
 */
void pico8_audio_music(int song_id, int fade_len, int channel_mask);

/*
 * Stop everything that is playing.
 *
 * 재생 중인 모든 소리를 멈춘다.
 */
void pico8_audio_stop(void);

/* ------------------------------------------------------------------ *
 *  State, as stat() wants to know it
 * ------------------------------------------------------------------ */

/*
 * The sound effect playing on a channel, or -1 when the channel is quiet.
 *
 * 그 채널에서 연주 중인 효과음 번호. 조용하면 -1.
 */
int pico8_audio_sfx_id(int channel);

/*
 * The note of the sound effect playing on a channel, or -1.
 *
 * 그 채널에서 연주 중인 음 번호. 없으면 -1.
 */
int pico8_audio_note_num(int channel);

/*
 * The pattern of the playing song, or -1 when nothing plays.
 *
 * 재생 중인 song의 패턴 번호. 아무것도 연주하지 않으면 -1.
 */
int pico8_audio_music_pattern(void);        /* stat(54) */
int pico8_audio_music_count(void);          /* stat(55) */
int pico8_audio_music_ticks(void);          /* stat(56) */
int pico8_audio_music_playing(void);        /* stat(57) */

#endif /* PICO8_AUDIO_H */
