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

static int ram_byte(int addr)
{
    if (addr < 0 || addr >= PICO8_RAM_SIZE) return 0;
    return pico8_ram[addr];
}

static int sfx_speed(int sfx_id)
{
    int s = ram_byte(PICO8_SFX_ADDR + sfx_id * AUDIO_SFX_SIZE + 65);
    return s < 1 ? 1 : s;              /* speed must be 1..255 */
}

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

static float key_to_freq(int key)
{
    return 440.0f * exp2f((key - 33.0f) / 12.0f);
}

/* ------------------------------------------------------------------ */
/*  Waveform generation (formulas measured from PICO-8 WAV exports)     */
/* ------------------------------------------------------------------ */

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

/* Advance one channel by a single output sample and return the synth
   parameters for that sample. */
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

static void set_music_pattern(int pattern);

static int song_sfx(int song, int channel)
{
    return ram_byte(PICO8_SONG_ADDR + song * AUDIO_SONG_SIZE + channel) & 0x7f;
}

static int song_flag(int song, int which)
{
    /* which: 0 = fnext (chan0), 1 = loop/end (chan1), 2 = stop (chan2) */
    return (ram_byte(PICO8_SONG_ADDR + song * AUDIO_SONG_SIZE + which) >> 7) & 1;
}

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

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    fill_buffer((float *)stream, len / (int)sizeof(float));
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

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

void pico8_audio_destroy(void)
{
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
        audio_ready = 0;
    }
}

static void lock_audio(void)   { if (audio_dev) SDL_LockAudioDevice(audio_dev); }
static void unlock_audio(void) { if (audio_dev) SDL_UnlockAudioDevice(audio_dev); }

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

int pico8_audio_sfx_id(int channel)
{
    if (channel < 0 || channel >= AUDIO_NUM_CHANS) return -1;
    return channels[channel].sfx;
}

int pico8_audio_note_num(int channel)
{
    if (channel < 0 || channel >= AUDIO_NUM_CHANS) return -1;
    if (channels[channel].sfx < 0) return -1;
    int n = (int)channels[channel].offset;
    if (n < 0) n = 0;
    if (n > 31) n = 31;
    return n;
}

int pico8_audio_music_pattern(void) { return music_pattern; }
int pico8_audio_music_count(void)   { return music_count; }
int pico8_audio_music_ticks(void)
{
    if (music_pattern < 0) return -1;
    return (int)music_offset;
}
int pico8_audio_music_playing(void) { return music_pattern != -1; }
