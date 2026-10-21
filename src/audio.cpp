/* audio.cpp — PICO-8 SFX / music mixer.
 *
 * The note encoding, timing and effect behaviour follow PICO-8's own
 * data layout (and the formulas measured by the zepto8 project):
 *
 *   - each SFX is 68 bytes in RAM: 32 notes * 2 bytes (LSB first) then
 *     editor mode, speed, loop start, loop end;
 *   - a note is   w2-w1-pppppp / c-eee-vvv-w3  (pitch 0..63, waveform
 *     0..15 where 8..15 are custom instruments, volume 0..7, effect 0..7);
 *   - a note lasts `speed` * (183 / 22050) seconds;
 *   - pitch 0 is C-0 and pitch 33 is A (440 Hz).
 *
 * 음표의 인코딩, 박자, 효과 동작은 PICO-8 자체의 데이터 배치(그리고 zepto8
 * 프로젝트가 측정한 공식)를 따른다:
 *
 *   - RAM에 효과음 하나는 68바이트다: 32개 음표 * 2바이트(LSB 우선) 다음에
 *     편집기 모드, 속도, 루프 시작, 루프 끝이 온다.
 *   - 음표 하나는 w2-w1-pppppp / c-eee-vvv-w3 꼴이다(피치 0..63, 파형 0..15이며
 *     8..15는 사용자 정의 악기, 음량 0..7, 효과 0..7).
 *   - 음표 하나는 `speed` * (183 / 22050) 초 동안 이어진다.
 *   - 피치 0은 C-0이고 피치 33은 A(440Hz)다.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "pico8.h"

#define AUDIO_SFX_SIZE   68
#define AUDIO_SFX_NOTES  32
#define AUDIO_NUM_CHANS  4
#define AUDIO_NUM_SONGS  64
#define AUDIO_SONG_SIZE  4

#define PICO8_SAMPLE_RATE 44100
#define PICO8_BUFFER_SIZE 1024

/* PICO-8 exports each SFX at 22050 Hz with 183 samples per speed unit,
   so at our 44100 Hz output a note lasts speed * 2 * 183 samples. */
#define SAMPLES_PER_SPEED_UNIT (2.0 * 183.0)
#define PICO8_TICK_RATE        (22050.0 / 183.0)

enum {
    FX_NONE = 0, FX_SLIDE, FX_VIBRATO, FX_DROP,
    FX_FADE_IN, FX_FADE_OUT, FX_ARP_FAST, FX_ARP_SLOW
};

struct SfxNote {
    int key;
    int instrument;   /* 0..7 */
    int custom;       /* waveform >= 8 */
    int volume;       /* 0..7 */
    int effect;       /* 0..7 */
};

struct SynthParam {
    int    instrument;
    int    key;
    float  freq;
    float  volume;
    int    is_music;
    double phi;           /* unwrapped phase in cycles */
    float  last_sample;   /* noise filter state */
};

struct Channel {
    /* main sfx */
    int    sfx;           /* -1 = idle */
    double offset;        /* note offset, 0..32 */
    double time;          /* monotonic "note" time */
    int    is_music;
    int    can_loop;
    int    length;        /* explicit note count, or 0 */
    int    sfx_music;     /* music sfx waiting for a busy channel */
    int    prev_key;
    float  prev_vol;

    /* synth state carried between samples */
    double phi;
    float  last_sample;

    /* cached current note (for stat + effects) */
    int    cur_note;
};

static struct Channel channels[AUDIO_NUM_CHANS] = {
    { -1, 0.0, 0.0, 0, 1, 0, -1, 24, 0.0f, 0.0, 0.0f, -1 },
    { -1, 0.0, 0.0, 0, 1, 0, -1, 24, 0.0f, 0.0, 0.0f, -1 },
    { -1, 0.0, 0.0, 0, 1, 0, -1, 24, 0.0f, 0.0, 0.0f, -1 },
    { -1, 0.0, 0.0, 0, 1, 0, -1, 24, 0.0f, 0.0, 0.0f, -1 },
};

static SDL_AudioDeviceID audio_dev = 0;
static int audio_ready = 0;

/* Music state. */
static int    music_pattern = -1;
static int    music_count   = -1;
static double music_offset  = -1;
static int    music_mask    = 0;
static double music_length  = 0;
static float  music_fade_volume = 1.0f;
static float  music_fade_step   = 0.0f;

/* ------------------------------------------------------------------ */
/*  SFX decoding                                                       */
/* ------------------------------------------------------------------ */

/*
 * ram_byte(addr) — one byte of the RAM.
 *
 * Reads a byte of the machine, and gives 0 for an address that is outside
 * of it, so that a broken sound effect cannot read past the end.
 *
 * 기계의 RAM 바이트 하나를 읽는다. 범위 밖의 주소는 0을 주므로, 망가진
 * 효과음이 메모리 끝을 넘어 읽지 못한다.
 */
static int ram_byte(int addr)
{
    if (addr < 0 || addr >= PICO8_RAM_SIZE) return 0;
    return pico8_ram[addr];
}

/*
 * sfx_speed(sfx_id) — how fast a sound effect plays.
 *
 * The speed is the byte 65 of the sound effect in the RAM.  It says how
 * long a note lasts; the speed has to be between 1 and 255, and a value of
 * zero is read as 1.
 *
 * 속도는 RAM에 있는 효과음의 65번째 바이트다. 음표가 얼마나 오래 가는지를
 * 말하며, 1에서 255 사이여야 하고 0은 1로 읽는다.
 */
static int sfx_speed(int sfx_id)
{
    int s = ram_byte(PICO8_SFX_ADDR + sfx_id * AUDIO_SFX_SIZE + 65);
    return s < 1 ? 1 : s;              /* speed must be 1..255 */
}

/*
 * sfx_get_note(sfx_id, note, out) — one note of a sound effect.
 *
 * A note takes two bytes in the RAM.  The first one holds the pitch (the
 * lower six bits) and the lower two bits of the instrument, the second one
 * the rest of the instrument, the volume, the effect and the flag that
 * says whether the waveform is a custom one.
 *
 * 음표는 RAM에서 두 바이트를 차지한다. 첫 번째 바이트는 피치(하위 6비트)와
 * 악기의 하위 두 비트, 두 번째 바이트는 나머지 악기 비트와 음량, 효과, 그리고
 * 파형이 사용자 정의인지를 말하는 플래그를 든다.
 */
static void sfx_get_note(int sfx_id, int note, struct SfxNote *out)
{
    int base = PICO8_SFX_ADDR + sfx_id * AUDIO_SFX_SIZE + note * 2;
    int lsb = ram_byte(base);
    int msb = ram_byte(base + 1);
    out->key        = lsb & 0x3f;
    out->instrument = ((msb & 0x01) << 2) | ((lsb & 0xc0) >> 6);
    out->custom     = (msb & 0x80) ? 1 : 0;   /* waveform 8..15 */
    out->volume     = (msb & 0x0e) >> 1;
    out->effect     = (msb & 0x70) >> 4;
}

/*
 * key_to_freq(key) — the frequency of a note.
 *
 * PICO-8 numbers its notes from 0 to 63.  The note 33 is the A of 440Hz
 * and every step is a half tone, so the frequency of a note is 440Hz times
 * 2 to the power of (key - 33) / 12.
 *
 * PICO-8은 음을 0부터 63까지 센다. 음 33이 440Hz의 A이고 한 단계가 반음
 * 이므로, 음의 주파수는 440Hz에 2의 (key - 33) / 12 제곱을 곱한 값이다.
 */
static float key_to_freq(int key)
{
    return 440.0f * exp2f((key - 33.0f) / 12.0f);
}

/* ------------------------------------------------------------------ */
/*  Waveform generation (formulas measured from PICO-8 WAV exports)     */
/* ------------------------------------------------------------------ */

/*
 * pico8_wave(instrument, advance, freq, noise_state, key) — one sample.
 *
 * Makes the waveform of the given instrument.  The `advance` is the phase
 * of the note in cycles, and the fractional part of it is what the
 * waveform is built from.  The eight instruments of PICO-8 are a
 * triangle, a tilted sawtooth, a sawtooth, a long square, a short square,
 * an organ, noise and a phaser.
 *
 * The noise instrument keeps its state in `noise_state`, because the noise
 * is a filter that remembers the last sample.  A higher key makes the
 * noise brighter.
 *
 * 주어진 악기의 파형을 만든다. `advance`는 음의 위상(사이클 수)이고, 그
 * 소수 부분으로 파형이 만들어진다. PICO-8의 여덟 악기는 삼각파, 기울인 톱니파,
 * 톱니파, 긴 제곱파, 짧은 제곱파, 오르간, 노이즈, 페이저다.
 *
 * 노이즈 악기는 `noise_state`에 상태를 남긴다. 노이즈가 마지막 샘플을 기억하는
 * 필터이기 때문이다. 높은 음은 노이즈를 더 밝게 만든다.
 */
static float pico8_wave(int instrument, double advance, float freq,
                        float *noise_state, int key)
{
    double t = advance - floor(advance);
    float ret = 0.0f;

    switch (instrument) {
    case 0:   /* triangle */
        ret = 1.0f - (float)fabs(4.0 * t - 2.0);
        return ret * 0.5f;

    case 1: { /* "tilted" sawtooth */
        double a = 0.875;
        ret = (float)(t < a ? 2.0 * t / a - 1.0
                            : 2.0 * (1.0 - t) / (1.0 - a) - 1.0);
        return ret * 0.5f;
    }

    case 2:   /* sawtooth */
        ret = (float)(t < 0.5 ? t : t - 1.0);
        return 0.653f * ret;

    case 3:   /* long square (50% duty) */
        return t < 0.5 ? 0.25f : -0.25f;

    case 4:   /* short square (25% duty) */
        return t < 0.316f ? 0.25f : -0.25f;

    case 5:   /* organ (super triangle) */
        ret = (float)(t < 0.5 ? 3.0 - fabs(24.0 * t - 6.0)
                              : 1.0 - fabs(16.0 * t - 12.0));
        return ret / 9.0f;

    case 6: { /* noise */
        const float tscale = 8.858923f;
        float scale = freq / (float)PICO8_SAMPLE_RATE * tscale;
        float new_sample = (*noise_state + scale * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f))
                           / (1.0f + scale);
        float factor = 1.0f - key / 63.0f;
        *noise_state = new_sample;
        return new_sample * 1.5f * (1.0f + factor * factor);
    }

    case 7:   /* phaser (two detuned triangles) */
        ret = 2.0f - (float)fabs(8.0 * t - 4.0);
        ret += 1.0f - (float)fabs(4.0 * (advance * 109.0 / 110.0
                                         - floor(advance * 109.0 / 110.0)) - 2.0);
        return ret / 6.0f;
    }

    return 0.0f;
}

/* ------------------------------------------------------------------ */
/*  Per-channel sfx update                                             */
/* ------------------------------------------------------------------ */

/*
 * update_sfx_state(ch, param, inv_sr) — advance a channel by one sample.
 *
 * Moves the channel one output sample forward and fills in the parameters
 * of the synthesizer for that sample: the instrument, the frequency, the
 * volume and the phase.
 *
 * The effects of a note are applied here as well: the slide, the
 * vibrato, the drop, the two fades and the two arpeggios.  The phase is
 * kept in the channel, so it runs on over the notes.
 *
 * The channel also decides when the sound is over: at the end of the
 * notes, at the loop end, or at the length that the caller gave.
 *
 * 채널을 출력 샘플 하나만큼 앞으로 보내고, 그 샘플의 신디사이저 파라미터(악기,
 * 주파수, 음량, 위상)를 채운다.
 *
 * 음의 효과도 여기서 적용된다. slide, vibrato, drop, 두 개의 fade, 두 개의
 * arp. 위상은 채널이 들고 있으므로 음이 바뀌어도 이어진다.
 *
 * 채널은 소리가 언제 끝나는지도 정한다. 음표의 끝, 루프 끝, 아니면 호출한 쪽이
 * 준 길이에서 끝난다.
 */
static void update_sfx_state(struct Channel *ch, struct SynthParam *param,
                             float inv_sr)
{
    if (ch->sfx < 0) { param->volume = 0.0f; return; }

    int sfx_id = ch->sfx;
    int speed  = sfx_speed(sfx_id);

    int    loop_start = ram_byte(PICO8_SFX_ADDR + sfx_id * AUDIO_SFX_SIZE + 66);
    int    loop_end   = ram_byte(PICO8_SFX_ADDR + sfx_id * AUDIO_SFX_SIZE + 67);
    double offset_per_second = (double)PICO8_TICK_RATE / speed;
    double offset_per_sample = offset_per_second * inv_sr;

    double offset = ch->offset;
    double time   = ch->time;
    double next_offset = offset + offset_per_sample;
    double next_time   = time + offset_per_sample;

    int loop_range = loop_end - loop_start;
    if (loop_range > 0 && next_offset >= loop_end && ch->can_loop)
        next_offset = fmod(next_offset - loop_start, (double)loop_range) + loop_start;

    int has_end = 0;
    double end_time = 32.0;
    if (ch->length > 0) { has_end = 1; end_time = ch->length; }
    if (!ch->is_music && loop_end == 0 && loop_start > 0) {
        has_end = 1;
        if (loop_start < end_time) end_time = loop_start;
    }
    if (loop_range <= 0) {
        has_end = 1;
        if (!ch->is_music) {
            int last_note = 0;
            for (int n = 0; n < AUDIO_SFX_NOTES; n++) {
                struct SfxNote note;
                sfx_get_note(sfx_id, n, &note);
                if (note.volume > 0) last_note = (n + 1 < 32) ? n + 1 : 32;
            }
            if (last_note < end_time) end_time = last_note;
        }
    }

    param->instrument = 0;
    param->volume = 0.0f;

    if (offset < 32.0) {
        int note_id = (int)floor(offset);
        struct SfxNote note;
        sfx_get_note(sfx_id, note_id, &note);
        ch->cur_note = note_id;

        float volume = note.volume / 7.0f;
        float freq = key_to_freq(note.key);

        if (volume > 0.0f) {
            switch (note.effect) {
            case FX_NONE:
                break;
            case FX_SLIDE: {
                double t = offset - floor(offset);
                freq = (float)(key_to_freq(ch->prev_key) * (1.0 - t) + freq * t);
                if (ch->prev_vol > 0.0f)
                    volume = (float)(ch->prev_vol * (1.0 - t) + volume * t);
                break;
            }
            case FX_VIBRATO: {
                /* half a semitone, 7.5 cycles per note (measured) */
                double t = fabs(fmod(7.5 * offset / offset_per_second, 1.0) - 0.5) - 0.25;
                freq = (float)(freq * (1.0 - t) + freq * 1.059463094359 * t);
                break;
            }
            case FX_DROP:
                freq *= (float)(1.0 - (offset - floor(offset)));
                break;
            case FX_FADE_IN:
                volume *= (float)(offset - floor(offset));
                break;
            case FX_FADE_OUT:
                volume *= (float)(1.0 - (offset - floor(offset)));
                break;
            case FX_ARP_FAST:
            case FX_ARP_SLOW: {
                int m = (speed <= 8 ? 32 : 16) / (note.effect == FX_ARP_FAST ? 4 : 8);
                int n = (int)(m * 7.5 * offset / offset_per_second);
                int arp_note = (note_id & ~3) | (n & 3);
                struct SfxNote an;
                sfx_get_note(sfx_id, arp_note, &an);
                freq = key_to_freq(an.key);
                break;
            }
            }

            param->instrument = note.instrument;
            param->key        = note.key;
            param->freq       = freq;
            param->volume     = volume;
            param->is_music   = ch->is_music;

            ch->phi += freq * inv_sr;
            if (ch->phi > 1048576.0) ch->phi -= 1048576.0;
            param->phi = ch->phi;
        }

        if ((int)floor(next_offset) != note_id) {
            ch->prev_key = note.key;
            ch->prev_vol = note.volume / 7.0f;
        }
    }

    ch->offset = next_offset;
    ch->time   = next_time;

    if (has_end && next_time >= end_time)
        ch->sfx = -1;
}

/* ------------------------------------------------------------------ */
/*  Music sequencing                                                   */
/* ------------------------------------------------------------------ */

/*
 * song_sfx(song, channel) — the sound effect of a channel of a song.
 *
 * The four bytes of a song hold one sound effect per channel.  The upper
 * bit of the byte is a flag and the rest is the number of the effect.
 *
 * song의 네 바이트는 채널마다 효과음 하나를 가리킨다. 바이트의 최상위 비트는
 * 플래그이고 나머지는 효과음 번호다.
 */
static int song_sfx(int song, int channel)
{
    return ram_byte(PICO8_SONG_ADDR + song * AUDIO_SONG_SIZE + channel) & 0x7f;
}

/*
 * song_flag(song, which) — one of the flags of a song.
 *
 * The flags of a song live in the upper bits of its four channel bytes:
 * the first one is "next" (the pattern that follows this one), the second
 * "loop" and the third "stop".
 *
 * song의 플래그는 채널 바이트들의 최상위 비트에 있다. 첫 번째는 "다음"(이
 * 패턴 다음에 오는 패턴), 두 번째는 "반복", 세 번째는 "정지"다.
 */
static int song_flag(int song, int which)
{
    /* which: 0 = fnext (chan0), 1 = loop/end (chan1), 2 = stop (chan2) */
    return (ram_byte(PICO8_SONG_ADDR + song * AUDIO_SONG_SIZE + which) >> 7) & 1;
}

/*
 * set_music_pattern(pattern) — start a song pattern.
 *
 * Starts the given song pattern.  Every channel that plays music is
 * stopped and the channels of the new pattern are started; a channel that
 * is busy with something else remembers the effect and starts it as soon
 * as it is free.
 *
 * A pattern of -1 stops the music.
 *
 * The length of the pattern is worked out from its channels: the first
 * channel that does not loop decides it, and if every channel loops the
 * slowest one does.
 *
 * 주어진 song 패턴을 시작한다. 음악을 담당하는 채널은 모두 멈추고 새 패턴의
 * 채널들을 시작한다. 다른 것으로 바쁜 채널은 효과음을 기억해 두었다가 채널이
 * 비면 시작한다.
 *
 * 패턴이 -1이면 음악을 멈춘다.
 *
 * 패턴의 길이는 그 채널들로부터 계산한다. 반복하지 않는 첫 채널이 정하고,
 * 모든 채널이 반복하면 가장 느린 채널이 정한다.
 */
static void set_music_pattern(int pattern)
{
    for (int n = 0; n < AUDIO_NUM_CHANS; n++) {
        if (channels[n].is_music) {
            channels[n].sfx = -1;
            channels[n].sfx_music = -1;
        }
    }

    if (pattern < 0 || pattern > 63) {
        music_pattern = -1;
        music_count   = -1;
        music_offset  = -1;
        music_mask    = 0;
        music_length  = 0.0;
        return;
    }

    /* Duration: the first non-looping channel, or the slowest looping one. */
    int duration_looping = -1;
    int duration_no_loop = -1;
    for (int i = 0; i < AUDIO_NUM_CHANS; i++) {
        int n = song_sfx(pattern, i);
        if (n & 0x40) continue;
        int speed = sfx_speed(n & 0x3f);
        int loop_start = ram_byte(PICO8_SFX_ADDR + (n & 0x3f) * AUDIO_SFX_SIZE + 66);
        int loop_end   = ram_byte(PICO8_SFX_ADDR + (n & 0x3f) * AUDIO_SFX_SIZE + 67);
        int has_loop = (loop_end > 0 && loop_end > loop_start);
        if (has_loop) {
            int d = 32 * speed;
            if (d > duration_looping) duration_looping = d;
        } else {
            int end_time = 32;
            if (loop_end == 0 && loop_start > 0 && loop_start < end_time)
                end_time = loop_start;
            duration_no_loop = end_time * speed;
            break;
        }
    }

    int duration = duration_no_loop > 0 ? duration_no_loop : duration_looping;
    if (duration <= 0) {
        music_pattern = -1;
        music_offset  = -1;
        music_count   = -1;
        music_mask    = 0;
        music_length  = 0.0;
        return;
    }

    music_pattern = pattern;
    music_offset  = 0;
    music_length  = duration;

    for (int i = 0; i < AUDIO_NUM_CHANS; i++) {
        int n = song_sfx(pattern, i);
        if (n & 0x40) continue;
        if (channels[i].sfx == -1) {
            channels[i].sfx        = n & 0x3f;
            channels[i].offset     = 0;
            channels[i].time       = 0;
            channels[i].length     = 0;
            channels[i].can_loop   = 1;
            channels[i].is_music   = 1;
            channels[i].prev_key   = 24;
            channels[i].prev_vol   = 0.0f;
            channels[i].phi        = 0;
            channels[i].last_sample = 0.0f;
        } else {
            channels[i].sfx_music = n & 0x3f;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Mixing                                                             */
/* ------------------------------------------------------------------ */

/*
 * fill_buffer(buf, samples) — make the next samples of the sound.
 *
 * Fills the buffer of the host with the mixed output of the four
 * channels.  The music timeline moves forward here as well, and when a
 * pattern is over the next one is started (or the music loops or stops,
 * as the flags of the song say).
 *
 * The mix is cut off at -1 and 1, so that a loud cart cannot produce
 * anything outside of the range of the host.
 *
 * 호스트의 버퍼를 네 채널의 합친 출력으로 채운다. 음악의 시간표도 여기서
 * 앞으로 가고, 패턴이 끝나면 다음 패턴을 시작한다(song의 플래그가 말하는 대로
 * 반복하거나 멈춘다).
 *
 * 합은 -1과 1 사이로 잘라내므로, 시끄러운 카트가 호스트의 범위를 벗어난 소리를
 * 만들 수 없다.
 */
static void fill_buffer(float *buf, int samples)
{
    memset(buf, 0, (size_t)samples * sizeof(float));
    if (!audio_ready) return;

    float inv_sr = 1.0f / PICO8_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        /* Advance the music timeline on the first channel. */
        if (music_pattern != -1) {
            music_offset += PICO8_TICK_RATE * inv_sr;
            music_fade_volume += music_fade_step * inv_sr;
            if (music_fade_volume < 0.0f) music_fade_volume = 0.0f;
            if (music_fade_volume > 1.0f) music_fade_volume = 1.0f;

            if (music_fade_step < 0.0f && music_fade_volume <= 0.0f) {
                set_music_pattern(-1);
            } else if (music_offset >= music_length) {
                int next_pattern = music_pattern + 1;
                int next_count   = music_count + 1;
                if (song_flag(music_pattern, 2)) {          /* stop */
                    next_pattern = -1;
                    next_count   = -1;
                } else if (song_flag(music_pattern, 1)) {   /* loop */
                    while (--next_pattern > 0 && !song_flag(next_pattern, 0))
                        ;
                }
                music_count = next_count;
                set_music_pattern(next_pattern);
            }
        }

        float mix = 0.0f;
        for (int c = 0; c < AUDIO_NUM_CHANS; c++) {
            struct Channel *ch = &channels[c];

            /* Start a queued music sfx once the channel frees up. */
            if (ch->sfx == -1 && ch->sfx_music != -1) {
                int index = ch->sfx_music;
                int speed = sfx_speed(index);
                int loop_start = ram_byte(PICO8_SFX_ADDR + index * AUDIO_SFX_SIZE + 66);
                int loop_end   = ram_byte(PICO8_SFX_ADDR + index * AUDIO_SFX_SIZE + 67);
                int loop_range = loop_end - loop_start;
                double new_offset = music_offset / speed;
                int want_play = 1;
                if (loop_range > 0 && ch->can_loop) {
                    if (new_offset > loop_start)
                        new_offset = fmod(new_offset - loop_start, (double)loop_range) + loop_start;
                } else if (new_offset > 32.0) {
                    want_play = 0;
                }
                if (want_play) {
                    ch->sfx       = index;
                    ch->offset    = new_offset;
                    ch->time      = new_offset;
                    ch->length    = 0;
                    ch->can_loop  = 1;
                    ch->is_music  = 1;
                    ch->prev_key  = 24;
                    ch->prev_vol  = 0.0f;
                    ch->phi       = 0;
                    ch->last_sample = 0.0f;
                }
                ch->sfx_music = -1;
            }

            struct SynthParam param;
            param.phi         = ch->phi;
            param.last_sample = ch->last_sample;
            param.freq        = 0.0f;
            param.volume      = 0.0f;
            param.is_music    = ch->is_music;

            update_sfx_state(ch, &param, inv_sr);
            ch->last_sample = param.last_sample;

            if (param.volume > 0.0f) {
                float w = pico8_wave(param.instrument, param.phi, param.freq,
                                     &ch->last_sample, param.key);
                float vol = param.volume;
                if (param.is_music) vol *= music_fade_volume;
                mix += w * vol * 0.5f;
            }
        }

        if (mix > 1.0f) mix = 1.0f;
        else if (mix < -1.0f) mix = -1.0f;
        buf[i] = mix;
    }
}

/*
 * audio_callback(userdata, stream, len) — the callback of SDL.
 *
 * The callback that SDL calls when it wants sound.  It only fills the
 * buffer that SDL gave.
 *
 * SDL이 소리를 원할 때 부르는 콜백이다. SDL이 준 버퍼를 채우는 것뿐이다.
 */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    fill_buffer((float *)stream, len / (int)sizeof(float));
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/*
 * pico8_audio_init() — open the sound card.
 *
 * Opens the device of SDL at 44100Hz, mono, with a buffer of 1024 samples,
 * and starts it right away.  A device that cannot be opened gives -1.
 *
 * SDL의 장치를 44100Hz, 모노, 1024샘플 버퍼로 열고 곧바로 재생하기 시작한다.
 * 열 수 없는 장치는 -1을 준다.
 */
int pico8_audio_init(void)
{
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = PICO8_SAMPLE_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = PICO8_BUFFER_SIZE;
    want.callback = audio_callback;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio_dev) {
        fprintf(stderr, "Audio init failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(audio_dev, 0);
    audio_ready = 1;
    return 0;
}

/*
 * pico8_audio_destroy() — close the sound card.
 *
 * Closes the device of SDL, if there is one.
 *
 * SDL의 장치를 닫는다. 열려 있다면.
 */
void pico8_audio_destroy(void)
{
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
        audio_ready = 0;
    }
}

/*
 * lock_audio() — keep the mixer out.
 *
 * Locks the device of SDL, so that the audio thread does not change the
 * channels while they are being set up.
 *
 * SDL의 장치를 잠근다. 설정하는 동안 오디오 스레드가 채널을 바꾸지 못하게.
 */
static void lock_audio(void)   { if (audio_dev) SDL_LockAudioDevice(audio_dev); }

/*
 * unlock_audio() — let the mixer in again.
 *
 * Unlocks the device of SDL.
 *
 * SDL의 장치를 다시 연다.
 */
static void unlock_audio(void) { if (audio_dev) SDL_UnlockAudioDevice(audio_dev); }

/*
 * pico8_audio_sfx(sfx_id, channel, offset, length) — play a sound effect.
 *
 * Starts a sound effect on a channel.  The special numbers of `sfx_id`
 * mean something else: -1 stops the sound effects, -2 stops their
 * looping.  A channel of -2 says that the sound must not play on any
 * channel.
 *
 * Without a channel of its own the effect looks for a free one (or one
 * that already plays the same effect), and only takes a channel that is
 * busy with music when there is nothing else left.  The channel that
 * plays music is never taken away from the music.
 *
 * `sfx_id`의 특별한 값은 다른 뜻을 갖는다. -1은 효과음을 멈추고, -2는 반복을
 * 멈춘다. 채널이 -2이면 어느 채널에서도 나오지 않게 한다.
 *
 * 채널을 따로 주지 않으면 빈 채널(또는 같은 효과음을 이미 연주하고 있는 채널)을
 * 찾고, 그것도 없으면 음악에 쓰이는 채널만 가져온다. 음악 채널은 음악에서
 * 뺏지 않는다.
 */
void pico8_audio_sfx(int sfx_id, int channel, int offset, int length)
{
    lock_audio();

    if (sfx_id == -1) {                 /* stop (non-music channel(s)) */
        if (channel >= 0 && channel < AUDIO_NUM_CHANS) {
            if (!channels[channel].is_music) channels[channel].sfx = -1;
        } else {
            for (int i = 0; i < AUDIO_NUM_CHANS; i++)
                if (!channels[i].is_music) channels[i].sfx = -1;
        }
        unlock_audio();
        return;
    }
    if (sfx_id == -2) {                 /* stop looping */
        if (channel >= 0 && channel < AUDIO_NUM_CHANS) {
            if (!channels[channel].is_music) channels[channel].can_loop = 0;
        } else {
            for (int i = 0; i < AUDIO_NUM_CHANS; i++)
                if (!channels[i].is_music) channels[i].can_loop = 0;
        }
        unlock_audio();
        return;
    }
    if (sfx_id < 0 || sfx_id >= 64 || offset > 31) { unlock_audio(); return; }

    /* CHANNEL -2: stop the given sound from playing on any channel. */
    if (channel == -2) {
        for (int i = 0; i < AUDIO_NUM_CHANS; i++)
            if (channels[i].sfx == sfx_id) channels[i].sfx = -1;
        unlock_audio();
        return;
    }

    /* Pick a channel: prefer a free one (or one already playing this sfx),
       skipping music-reserved channels. */
    if (channel < 0 || channel >= AUDIO_NUM_CHANS) {
        channel = -1;
        for (int i = 0; i < AUDIO_NUM_CHANS; i++) {
            if (music_mask & (1 << i)) continue;
            if (channels[i].sfx == -1 || channels[i].sfx == sfx_id) { channel = i; break; }
        }
        if (channel == -1)
            for (int i = 0; i < AUDIO_NUM_CHANS; i++)
                if (!(music_mask & (1 << i)) && channels[i].is_music) { channel = i; break; }
        if (channel == -1) {
            int fastest = 256;
            for (int i = 0; i < AUDIO_NUM_CHANS; i++) {
                if (music_mask & (1 << i)) continue;
                int idx = channels[i].sfx;
                if (idx < 0 || idx >= 64) continue;
                int sp = sfx_speed(idx);
                if (sp <= fastest) { channel = i; fastest = sp; }
            }
        }
        if (channel == -1) { unlock_audio(); return; }
    }

    for (int i = 0; i < AUDIO_NUM_CHANS; i++)
        if (channels[i].sfx == sfx_id) channels[i].sfx = -1;

    if (channels[channel].sfx != -1 && channels[channel].is_music)
        channels[channel].sfx_music = channels[channel].sfx;

    struct Channel *ch = &channels[channel];
    ch->sfx        = sfx_id;
    ch->offset     = offset < 0 ? 0 : offset;
    ch->time       = 0;
    ch->length     = length < 0 ? 0 : length;
    ch->can_loop   = 1;
    ch->is_music   = 0;
    ch->sfx_music  = -1;
    ch->prev_key   = 24;
    ch->prev_vol   = 0.0f;
    ch->phi        = 0;
    ch->last_sample = 0.0f;

    unlock_audio();
}

/*
 * pico8_audio_music(song_id, fade_len, channel_mask) — play a song.
 *
 * Starts a song.  The mask says which of the four channels the song may
 * use; a channel that is not in the mask is left alone.
 *
 * A fade length of zero starts the song at full volume, a larger one
 * fades it in over that many milliseconds.  A song of -1 stops the music,
 * and a fade length with it fades the music out first.
 *
 * song을 시작한다. 마스크는 song이 어느 채널을 쓸 수 있는지 말하고, 마스크에
 * 없는 채널은 건드리지 않는다.
 *
 * fade 길이가 0이면 최대 음량으로 시작하고, 그보다 크면 그 밀리초에 걸쳐
 * 소리를 올린다. song이 -1이면 음악을 멈추고, fade 길이를 주면 먼저 지운다.
 */
void pico8_audio_music(int song_id, int fade_len, int channel_mask)
{
    lock_audio();

    if (song_id == -1 || song_id < -1 || song_id > 63) {
        /* Stop: fade the music out, then clear it. */
        music_fade_step = (fade_len <= 0) ? -1e9f
                                          : -(float)(1000.0 / fade_len) * music_fade_volume;
        if (fade_len <= 0) set_music_pattern(-1);
        unlock_audio();
        return;
    }

    music_count = 0;
    music_mask  = channel_mask & 0xf;
    music_fade_volume = 1.0f;
    music_fade_step   = 0.0f;
    if (fade_len > 0) {
        music_fade_volume = 0.0f;
        music_fade_step   = (float)(1000.0 / fade_len);
    }

    set_music_pattern(song_id);
    unlock_audio();
}

/*
 * pico8_audio_stop() — silence everything.
 *
 * Stops every channel and the music.
 *
 * 모든 채널과 음악을 멈춘다.
 */
void pico8_audio_stop(void)
{
    lock_audio();
    for (int c = 0; c < AUDIO_NUM_CHANS; c++) {
        channels[c].sfx = -1;
        channels[c].sfx_music = -1;
    }
    music_pattern = -1;
    music_count   = -1;
    music_offset  = -1;
    music_length  = 0.0;
    unlock_audio();
}

/*
 * pico8_audio_sfx_id(channel) — what a channel is playing.
 *
 * The number of the sound effect on the channel, or -1 when the channel is
 * free.
 *
 * 그 채널에서 나오는 효과음 번호. 채널이 비어 있으면 -1.
 */
int pico8_audio_sfx_id(int channel)
{
    if (channel < 0 || channel >= AUDIO_NUM_CHANS) return -1;
    return channels[channel].sfx;
}

/*
 * pico8_audio_note_num(channel) — which note a channel is on.
 *
 * The number of the note the channel is playing, counted from 0.  A
 * channel that plays nothing gives -1.
 *
 * 그 채널이 연주 중인 음 번호(0부터). 아무것도 연주하지 않으면 -1.
 */
int pico8_audio_note_num(int channel)
{
    if (channel < 0 || channel >= AUDIO_NUM_CHANS) return -1;
    if (channels[channel].sfx < 0) return -1;
    int n = (int)channels[channel].offset;
    if (n < 0) n = 0;
    if (n > 31) n = 31;
    return n;
}

/*
 * pico8_audio_music_pattern() — the song that is playing.
 *
 * The number of the pattern the music is on, or -1 when there is no
 * music.
 *
 * 음악이 있는 패턴 번호. 음악이 없으면 -1.
 */
int pico8_audio_music_pattern(void) { return music_pattern; }

/*
 * pico8_audio_music_count() — how many patterns the song has played.
 *
 * The number of patterns the music has gone through since it started.
 *
 * 음악이 시작된 뒤 지나온 패턴의 수.
 */
int pico8_audio_music_count(void)   { return music_count; }

/*
 * pico8_audio_music_ticks() — where the music is.
 *
 * The number of ticks the current pattern has been playing for, or -1
 * when there is no music.
 *
 * 현재 패턴이 얼마나 재생되었는지(틱 수). 음악이 없으면 -1.
 */
int pico8_audio_music_ticks(void)
{
    if (music_pattern < 0) return -1;
    return (int)music_offset;
}

/*
 * pico8_audio_music_playing() — is the music running?
 *
 * 1 when a song is playing, 0 when there is none.
 *
 * song이 재생 중이면 1, 없으면 0.
 */
int pico8_audio_music_playing(void) { return music_pattern != -1; }
