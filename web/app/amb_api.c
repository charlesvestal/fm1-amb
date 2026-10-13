/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine for the web app: dsp/amb.c alone, the same code the FM-1 runs, behind a few exports.
 * One instance runs in the AudioWorklet (en_render); another on the page answers the pure questions
 * (parameter names and texts, chord names, the pieces' recipes). No device, no panel, no screen.
 *
 *   web/app/build.sh  ->  build/app/amb.wasm */
#include <stdint.h>
#include "amb.h"

#define API __attribute__((used, visibility("default")))

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
}

/* ---- control */
API void app_init(void) { en_init(); }
API void app_set(int p, int v) { en_set(p, v); }
API void app_pool(int pool) { en_pool((uint32_t)pool); }
API void app_play(int on) { en_play(on); }
API void app_next(void) { en_next(); }
API void app_freeze(int on) { en_freeze(on); }
API void app_seed(int s) { en_seed((uint32_t)s); }
API void app_regrow(void) { en_regrow(); }
API void app_panic(void) { en_panic(); }

/* ---- audio: n (<= 1024) frames into out_l / out_r, float */
static int32_t buf[2 * 1024];
static float out_l[1024], out_r[1024];
API float *app_out_l(void) { return out_l; }
API float *app_out_r(void) { return out_r; }
API void app_render(int n, int gain_q12)
{
    int i;
    if (n > 1024)
        n = 1024;
    en_render(buf, (uint32_t)n, (uint32_t)gain_q12);
    for (i = 0; i < n; i++) {
        out_l[i] = (float)buf[2 * i] * (1.0f / 8388608.0f);
        out_r[i] = (float)buf[2 * i + 1] * (1.0f / 8388608.0f);
    }
}

/* ---- what the screen draws: one flat array (layout in app.js, STATE) */
#define ST_N (32 + EN_NLOOP * EN_LOOP_MAX)
static float st[ST_N];
API float *app_state(void)
{
    int i, l;
    st[0] = en_playing;
    st[1] = en_frozen;
    st[2] = en_chord_root;
    st[3] = en_chord_n;
    for (i = 0; i < EN_NPAD; i++) {
        st[4 + i] = en_chord[i];
        st[8 + i] = en_pad_amp[i];
    }
    st[12] = en_drift;
    st[13] = en_pad_level;
    st[14] = en_bass_level;
    st[15] = en_arc;
    st[16] = (float)en_bars;
    for (l = 0; l < EN_NLOOP; l++) {
        st[17 + l] = en_loop_pos[l];
        st[21 + l] = en_loop_glow[l];
    }
    for (i = 0; i < EN_NDRUM; i++) {
        st[25 + i] = en_drum_pos[i];
        st[28 + i] = en_drum_glow[i];
    }
    st[31] = en_pulse_rest;
    for (l = 0; l < EN_NLOOP; l++)
        for (i = 0; i < EN_LOOP_MAX; i++)
            st[32 + l * EN_LOOP_MAX + i] = en_loop_note[l][i];
    return st;
}
API int app_state_len(void) { return ST_N; }
API int app_drum_pat(int d) { return (int)en_drum_pat[d % EN_NDRUM]; }

/* ---- the pure parts, for the page */
static char text[32];
API int app_nparams(void) { return P_NPARAMS; }
API const char *app_param_name(int p) { return en_param_info(p)->name; }
API int app_param_lo(int p) { return en_param_info(p)->lo; }
API int app_param_hi(int p) { return en_param_info(p)->hi; }
API int app_param_def(int p) { return en_param_info(p)->def; }
API const char *app_param_text(int p, int v)
{
    en_param_text(p, v, text);
    return text;
}
API const char *app_chord_name(int root, int a, int b, int c, int d)
{
    int pcs[5] = {root % 12, a % 12, b % 12, c % 12, d % 12};
    return en_chord_name(pcs, 5, text);
}
API int app_npieces(void) { return EN_NPIECES; }
API const char *app_piece_name(int i) { return en_piece_name(i); }
static int16_t rpar[P_NPARAMS];
static uint16_t rpool;
API int app_piece_par(int i, int p)
{
    en_piece(i, rpar, &rpool);
    return rpar[p % P_NPARAMS];
}
API int app_piece_pool(int i)
{
    en_piece(i, rpar, &rpool);
    return rpool;
}
API int app_mode_pool(int m) { return EN_MODE_POOL[(unsigned)m < EN_NMODES ? m : 0]; }
API int app_nmodes(void) { return EN_NMODES; }
API const char *app_note_name(int n) { return EN_NOTE_NAME[((n % 12) + 12) % 12]; }
API int app_loop_len(int l) { return EN_LOOP_LEN[l % EN_NLOOP]; }
API int app_drum_n(int d) { return EN_DRUM_N[d % EN_NDRUM]; }
