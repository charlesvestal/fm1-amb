/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   the bells' octave
 *   ENV            the pad's colour: Rhodes / Voices (lit: Voices), kept with the piece
 *   SELECT tempo   ALGORITHM mode (loads a pool)   PRESETS the piece (Root is on the Setup page)
 *   HOME SEQ FX SEL GLO   the pages: LAYERS, MOTION, SPACE, TONE, SETUP (EDIT steps through them)
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe
 * The SMK-37 build (BOARD_SMK37): the same, and the SMK's own controls (below, "SMK-37"). */
#include "../dsp/fastmath.h"

/* the pages, one a layer (a button each); SELECT steps through them in this order */
enum { V_GARDEN, V_PAD, V_BELLS, V_BASS, V_PULSE, V_SPACE, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Garden", "Pad", "Bells", "Bass", "Pulse", "Space", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO};
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PAD, P_SWELL, P_BRIGHT, P_PADTONE},
    {P_BELLS, P_DENSITY, P_BTONE, P_BOCT},
    {P_BASS, P_BASSOCT, P_GLIDE, P_FIFTH},
    {P_PULSE, P_HITS, P_SWING, P_HATS},
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Swell", "Bright", "Colour"},
    {"Level", "Density", "Tone", "Octave"},
    {"Level", "Octave", "Glide", "Fifth"},
    {"Level", "Hits", "Swing", "Hats"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Root", "Tune", "MIDI out", "Lights"},
};

/* the screen's colours: the cover of Ambient 1: Music for Airports (1978), as the web app and the site.
 * The map's paper, its ink; the land's greens for the pad (the chord) and the scale, its rivers for the
 * bells; the bass and the pulse in darker inks. Each layer has a pale tint for the shapes at rest. */
#define K_BG RGB(246, 238, 181)
#define K_PANEL RGB(255, 251, 224)
#define K_LINE RGB(208, 214, 150)
#define K_DIM RGB(112, 106, 58)
#define K_TEXT RGB(29, 27, 2)
#define K_ACC RGB(64, 128, 124)          /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(84, 153, 149)
#define K_BELL_T RGB(176, 214, 206)
#define K_PAD RGB(112, 142, 62)
#define K_PAD_T RGB(208, 220, 146)
#define K_PULSE RGB(70, 66, 28)
#define K_PULSE_T RGB(214, 214, 160)
#define K_BASS RGB(138, 128, 52)
#define K_BASS_T RGB(222, 218, 160)
#define K_FROST RGB(60, 132, 160)
#define K_FROST_T RGB(170, 210, 216)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        say("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        say("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say("PIECE", piece_name(proj.piece));
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_BOCT, proj.par[P_BOCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_BOCT, proj.par[P_BOCT], t);
            say("BELL OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}

#if BOARD_SMK37
/* ------------------------------------------------------------- SMK-37 --- */
/* The SMK-37's controls beyond the FM-1's (plat.h SMK section; the FM-1 ones keep their AMB-1 meaning):
 *   keys           all 37 are the scale, as the FM-1's 27: each adds or removes its note (C3..E3, G#5..C6 too)
 *   pads           the bells: sixteen steps of the scale up from the chord's root, low notes on the bottom row
 *                  (as the loops hold them: step 0 is the chord's root). A pad rings a bell there, with velocity;
 *                  the loops' bells light the pads they ring, in their shades
 *   KNOB BANK      the picker on the pads and the screen: the top row the eight pieces, the bottom row the eight
 *                  modes (a pad picks). It also shows for a moment after a PRESETS or ALGORITHM turn
 *   FADER BANK     what the four faders set: A the levels (Pad, Bells, Bass, Pulse), B the space (Echo, Echo
 *                  time, Reverb, Size), C the garden (Evolve, Motion, Drift, Density). A fader takes over once it
 *                  moves. The fader card shows them; the picker and the fader card close each other
 *   < >            the previous / next page (as SELECT)
 *   pitch wheel    pitch bend, +-2 semitones, on everything that sounds (the ringing bells too)
 *   mod wheel      vibrato on everything that sounds, 5.5 Hz, up to +-1/2 semitone
 * The wheels never change what is saved. */
#define SMK_NBANK 3
#define SMK_CARD_MS 1800u
static const uint8_t SMK_FADER_PAR[SMK_NBANK][4] = {{P_PAD, P_BELLS, P_BASS, P_PULSE},
                                                    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
                                                    {P_EVOLVE, P_MOTION, P_DRIFT, P_DENSITY}};
static const char *const SMK_FADER_LABEL[SMK_NBANK][4] = {{"Pad", "Bells", "Bass", "Pulse"},
                                                          {"Echo", "Echo time", "Reverb", "Size"},
                                                          {"Evolve", "Motion", "Drift", "Density"}};
static const char *const SMK_BANK_NAME[SMK_NBANK] = {"A LEVELS", "B SPACE", "C GARDEN"};
static struct {
    uint32_t sbtn;                     /* the four extra buttons held (SB_*) */
    uint8_t fbank;                     /* the fader bank, 0..2 */
    uint8_t flast[4];                  /* each fader's last value, 0xFF: not read yet (takes over once it moves) */
    int8_t ftouch;                     /* the fader moved last (the card shows it), -1 none */
    uint8_t pick;                      /* KNOB BANK: the picker pinned */
    uint32_t pick_until, card_until;   /* the picker after a turn; the fader card */
    int32_t pitch, modw;               /* the wheels: pitch -127..127 (0 at rest), mod 0..127 */
    float lglow[EN_NLOOP];             /* each loop's light, last frame (a rise: it rang) */
    int8_t lstep[EN_NLOOP];            /* the step each loop rang last, -1 none */
    float tap[SMK_NPADS];              /* the pads' own bells, by step, fading */
} smk;
static void smk_reset(void)
{
    int i;
    memset(&smk, 0, sizeof smk);
    memset(smk.flast, 0xFF, sizeof smk.flast);
    smk.ftouch = -1;
    for (i = 0; i < EN_NLOOP; i++)
        smk.lstep[i] = -1;
}
static int smk_picker(void) { return smk.pick || plat_ms() < smk.pick_until; }
static int smk_fcard(void) { return plat_ms() < smk.card_until; }

/* hardware pad p (0..7 the top row, 8..15 the bottom row) <-> the bells' step (0..7 the bottom row, 8..15 the top) */
static int smk_step_of_pad(int p) { return p < 8 ? p + 8 : p - 8; }

static void smk_input(void)
{
    uint32_t sb = plat_smk_buttons(), sp = sb & ~smk.sbtn, ph = plat_pad_hits(), now = plat_ms(), i;
    uint64_t kh = plat_keybed_hits();
    int32_t pitch, modw;
    smk.sbtn = sb;
    if (sp >> SB_KNOB_BANK & 1u) {               /* KNOB BANK: the picker, pinned or put away */
        smk.pick = (uint8_t)!smk_picker();
        smk.pick_until = 0;
        smk.card_until = 0;                      /* (one card at a time) */
    }
    if (sp >> SB_FADER_BANK & 1u) {              /* FADER BANK: the next bank, on the fader card */
        smk.fbank = (uint8_t)((smk.fbank + 1u) % SMK_NBANK);
        memset(smk.flast, 0xFF, sizeof smk.flast);
        smk.ftouch = -1;
        smk.card_until = now + SMK_CARD_MS;
        smk.pick = 0;
        smk.pick_until = 0;
    }
    if ((sp >> SB_LEFT & 1u) && ui.view > 0)     /* < >: the pages, as SELECT */
        set_view(ui.view - 1);
    if ((sp >> SB_RIGHT & 1u) && ui.view + 1 < NVIEWS)
        set_view(ui.view + 1);
    for (i = 0; i < SMK_NKEYBED; i++)            /* the keys: all 37 are the scale (key 0 is C3) */
        if (kh >> i & 1u)
            toggle_note((int)(i % 12u));
    for (i = 0; i < SMK_NPADS; i++) {
        int st = smk_step_of_pad((int)i);
        if (!(ph >> i & 1u))
            continue;
        if (smk_picker()) {                      /* the picker: the top row the pieces, the bottom row the modes */
            if (i < 8u) {
                if ((int)i != proj.piece)
                    set_piece((int)i);
            } else {
                set_mode((int)i - 8);
            }
            if (!smk.pick)
                smk.pick_until = now + 2u * SMK_CARD_MS;
        } else {                                 /* a bell */
            en_bell(st, plat_pad_vel((int)i));
            smk.tap[st] = 1.0f;
        }
    }
    if (sb || kh || ph)
        ui.act_t = now;
    for (i = 0; i < 4u; i++) {                   /* the faders: a parameter each, once a fader moves */
        int v = plat_fader((int)i), k = SMK_FADER_PAR[smk.fbank][i], lo, hi;
        if (v < 0)
            continue;
        if (smk.flast[i] == 0xFFu) {
            smk.flast[i] = (uint8_t)v;
            continue;
        }
        if (v == smk.flast[i])
            continue;
        smk.flast[i] = (uint8_t)v;
        knob_range(k, &lo, &hi);
        knob_set(k, lo + (v * (hi - lo) + 63) / 127);
        smk.ftouch = (int8_t)i;
        if (smk.pick) {                          /* the picker is pinned: a word at the top instead */
            char t[16];
            knob_text(k, t);
            say(SMK_FADER_LABEL[smk.fbank][i], t);
        } else {
            smk.card_until = now + SMK_CARD_MS;
            smk.pick_until = 0;
        }
        ui.act_t = now;
    }
    if (plat_wheels(&pitch, &modw)) {
        char t[8];
        int32_t st10 = (pitch < 0 ? -pitch : pitch) * 20 / 127;   /* the bend in tenths of a semitone */
        en_perf(pitch * 8192 / 127, modw);           /* the engine: +-2 semitones, the vibrato's depth */
        if (pitch != smk.pitch && st10) {
            t[0] = pitch > 0 ? '+' : '-';
            t[1] = (char)('0' + st10 / 10);
            t[2] = '.';
            t[3] = (char)('0' + st10 % 10);
            t[4] = 0;
            say("BEND", t);
        }
        if (modw != smk.modw && modw) {
            itoa_u((uint32_t)(modw * 100 / 127), t);
            say("VIBRATO", t);
        }
        smk.pitch = pitch;
        smk.modw = modw;
        ui.act_t = now;
    }
}
#endif

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press, and SEL has no job here: neither has a card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_SEL && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
#if BOARD_SMK37
    smk_input();                                 /* the keys too (all 37, from latched presses) */
#endif
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
#if !BOARD_SMK37
    for (i = 0; i < NKEYS; i++)
        if ((ch & keys) >> i & 1u)
            toggle_note((int)((5u + i) % 12u));  /* key 0 is F3 */
#endif
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GARDEN) {           /* Tempo is the Garden's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
#if BOARD_SMK37
        smk.pick_until = ui.turn_t + 2u * SMK_CARD_MS;   /* the picker shows where it is */
        smk.card_until = 0;
#endif
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
#if BOARD_SMK37
        smk.pick_until = ui.turn_t + 2u * SMK_CARD_MS;
        smk.card_until = 0;
#endif
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the garden (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a slow beat light
 * (a play triangle while resting) and a dot while there is something unsaved. Messages take its place
 * for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (ui.view == V_SETUP ? plat_cpu_pct() << 8 : 0u));
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too */
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a coral tag */
        cv_round(4, 4, w, 20, 6, K_TEXT);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
        cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
    }
    cv_blit(0, 0);
}

/* the garden: the scale as a ring of twelve, the chord lit in it; the four bell loops around it, each
 * a ring of its steps with its notes on it and its playhead going round; the three pulses on the
 * right, the four layers' levels on the left */
#define CX 120
#define CY 84
static const int32_t LOOP_R[EN_NLOOP] = {36, 43, 50, 57};

static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static void draw_ring(void)
{
    uint32_t abs_pool = pool_abs(), chord = 0;
    int i, l;
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    for (l = 0; l < EN_NLOOP; l++) {
        int len = EN_LOOP_LEN[l], s, pos = en_loop_pos[l], g = q16(en_loop_glow[l]);
        for (s = 0; s < len; s++) {
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], s, len, &x, &y);
            if (en_loop_note[l][s] >= 0)
                cv_disc(x, y, 2, (s == pos && en_playing) ? mix(K_BELL, K_FROST, g / 2) : mix(K_BELL_T, K_BELL, 12));
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (en_playing) {                        /* the playhead */
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], pos, len, &x, &y);
            cv_disc(x, y, 1 + g / 6, mix(K_DIM, K_BELL, g));
        }
    }
    for (i = 0; i < 12; i++) {                  /* the scale: C at the top */
        int32_t x, y;
        int in = (int)(abs_pool >> i & 1u), ch = (int)(chord >> i & 1u);
        polar(CX, CY, 24.0f, i, 12, &x, &y);
        if (i == proj.par[P_ROOT])
            cv_disc(x, y, 5, K_TEXT);           /* the root: ringed in ink */
        if (ch)
            cv_disc(x, y, 4, mix(K_PAD_T, K_PAD, 6 + q16(en_pad_level) * 10 / 16));
        else if (in)
            cv_disc(x, y, 3, mix(K_PAD_T, K_PAD, 7));
        else
            cv_disc(x, y, 1, K_LINE);
    }
    {   /* the chord's name, in the middle */
        char nm[16];
        int pcs[EN_NPAD + 1], k;
        pcs[0] = en_chord_root;
        for (k = 0; k < EN_NPAD; k++)
            pcs[k + 1] = en_chord[k] % 12;
        en_chord_name(pcs, EN_NPAD + 1, nm);
        if (text_w(&FONT_B, nm) <= 38)
            text_c(CX, CY - 8, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
        else
            text_c(CX, CY - 6, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_meters(void)                    /* left: the four layers */
{
    static const uint16_t C[4] = {K_PAD, K_BELL, K_BASS, K_PULSE}, T[4] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T};
    float lv[4], bm = 0.0f, dm = 0.0f;
    int i;
    for (i = 0; i < EN_NLOOP; i++)
        bm = fm_maxf(bm, en_loop_glow[i]);
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = bm * (float)proj.par[P_BELLS] * 0.01f;
    lv[2] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[3] = dm * (float)proj.par[P_PULSE] * 0.01f;
    for (i = 0; i < 4; i++) {
        int32_t x = 10 + i * 11, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 80.0f + 0.5f);
        cv_round(x, 46, 7, 86, 3, T[i]);
        if (h > 0)
            cv_round(x, 46 + 86 - h - 2, 7, h + 2, 3, C[i]);
    }
}

static void draw_pulse(void)                     /* right: the three Euclidean rings */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        int32_t cx = 212, cy = 52 + d * 34, s, n = EN_DRUM_N[d];
        int g = q16(en_drum_glow[d]);
        uint32_t p = en_drum_pat[d];
        for (s = 0; s < n; s++) {
            int32_t x, y;
            int cur = en_playing && s == en_drum_pos[d];
            polar(cx, cy, 13.0f, s, n, &x, &y);
            if ((p >> s & 1u) && en_pulse_rest)   /* sitting a few bars out: faded */
                cv_disc(x, y, 2, K_LINE);
            else if (p >> s & 1u)
                cv_disc(x, y, cur ? 3 : 2, cur ? K_PULSE : mix(K_PULSE_T, K_PULSE, 10));
            else if (cur)
                cv_disc(x, y, 1, K_DIM);
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (g)
            cv_disc(cx, cy, 1 + g / 4, mix(K_BG, K_PULSE, g));
    }
}

/* a button's card (help_tick): its name, then what it does, in the garden's place */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE GARDEN"); L("TEMPO, EVOLVE, MOTION, DRIFT"); L("+ SELECT: TEMPO"); break;
    case B_ENV: L("THE PAD"); L("LEVEL, SWELL, BRIGHT,"); L("COLOUR"); break;
    case B_ARP: L("THE BELLS"); L("LEVEL, DENSITY, TONE,"); L("OCTAVE"); break;
    case B_LFO: L("THE BASS"); L("LEVEL, OCTAVE, GLIDE, FIFTH"); break;
    case B_SEQ: L("THE PULSE"); L("LEVEL, HITS, SWING, HATS"); break;
    case B_FX: L("THE SPACE"); L("ECHO, ECHO TIME, REVERB,"); L("SIZE"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); break;
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L("AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("THE BELLS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE BELLS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L("BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k], K_TEXT);
}

#if BOARD_SMK37
/* SMK-37: the cards in the garden's place. The fader card: the bank's four values as four faders, in the
 * knob strip's columns (the faders sit in a row too); the bank's tabs above. The picker: the pads' two rows,
 * the eight pieces over the eight modes. */
static void smk_draw_fcard(void)
{
    static const uint16_t LAYER[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};
    int i, now_touch = smk.ftouch >= 0;
    int32_t x = 10;
    for (i = 0; i < SMK_NBANK; i++)              /* the banks: the one the faders set, dark */
        tag(&x, SMK_BANK_NAME[i], i == smk.fbank ? K_TEXT : K_PANEL, i == smk.fbank ? K_BG : K_DIM);
    for (i = 0; i < 4; i++) {
        int k = SMK_FADER_PAR[smk.fbank][i], lo, hi, v, hot = now_touch && smk.ftouch == i;
        int32_t cx = i * 60 + 30, h;
        uint16_t fill = smk.fbank == 0 ? LAYER[i] : hot ? K_ACC : K_PAD;
        char t[16];
        if (i)
            cv_rect(i * 60, 30, 1, 108, K_LINE);
        if (hot)
            cv_round(i * 60 + 3, 27, 54, 114, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, SMK_FADER_LABEL[smk.fbank][i], sizeof t);
        text_c(cx, 30, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        h = hi > lo ? (int32_t)((v - lo) * 70 / (hi - lo)) : 0;
        cv_round(cx - 4, 48, 8, 74, 3, K_LINE);
        if (h > 0)
            cv_round(cx - 4, 48 + 70 - h, 8, h + 4, 3, fill);
        cv_round(cx - 9, 48 + 70 - h - 1, 18, 6, 2, hot ? K_ACC : K_TEXT);   /* the fader's cap */
        knob_text(k, t);
        text_c(cx, 124, &FONT_XS, t, hot ? K_ACC : K_TEXT);
    }
}

static void smk_draw_picker(void)
{
    char t[4];
    int i, mode = proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? proj.par[P_MODE] : -1;
    int32_t x;
    x = cv_text(10, 2, &FONT_XS, "PIECE", K_DIM);
    cv_text(x + 8, 0, &FONT_B, piece_name(proj.piece), K_TEXT);
    x = cv_text(10, 70, &FONT_XS, "MODE", K_DIM);
    cv_text(x + 8, 68, &FONT_B, mode_name(), K_TEXT);
    for (i = 0; i < 16; i++) {                   /* the pads: pieces on the top row, modes on the bottom */
        int row = i / 8, on = row ? i - 8 == mode : i == proj.piece;
        int32_t cx = 4 + (i % 8) * 29, cy = row ? 92 : 24;
        const char *n = row ? EN_MODE_NAME[i - 8] : piece_name(i);
        cv_round(cx, cy, 27, 30, 5, on ? (row ? K_PAD : K_TEXT) : K_PANEL);
        t[0] = n[0];
        t[1] = n[1];
        t[2] = n[2] == ' ' ? 0 : n[2];          /* (Night Bus: "Nig"; the pentatonics: "Pen") */
        t[3] = 0;
        if (row && i - 8 >= MODE_PENT_MAJ) {     /* the two pentatonics, told apart */
            t[0] = 'P';
            t[1] = i - 8 == MODE_PENT_MAJ ? '+' : '-';
            t[2] = 0;
        }
        text_c(cx + 13, cy + 8, &FONT_XS, t, on ? K_BG : K_DIM);
    }
    text_c(120, 128, &FONT_XS, smk.pick ? "PADS PICK    KNOB BANK CLOSES" : "PADS PICK", K_DIM);
}

/* 1 = a card has the garden's place this frame (drawn when it changed) */
static int smk_card(void)
{
    uint32_t h;
    int i;
    if (smk_fcard()) {
        h = hash(0x46414452u, smk.fbank | (uint32_t)(smk.ftouch + 1) << 4);
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)knob_get(SMK_FADER_PAR[smk.fbank][i]));
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            smk_draw_fcard();
            cv_blit(0, MAIN_Y);
        }
        return 1;
    }
    if (smk_picker()) {
        h = hash(hash(0x5049434Bu, proj.piece | (uint32_t)proj.par[P_MODE] << 8 | (uint32_t)smk.pick << 16),
                 proj.pool);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            smk_draw_picker();
            cv_blit(0, MAIN_Y);
        }
        return 1;
    }
    return 0;
}

/* the pads' lights (leds()). The bells: each pad a step of the scale up from the chord's root; at rest the
 * chord's tones glow in the pad's green (its root brightest), the other steps faintly; a bell from a loop
 * lights its step in the loop's shade, a pad's own bell in the accent, both fading. Frozen: the chord in frost.
 * Resting: dimmer. The picker: the pieces and the modes, the ones playing lit. Lights Off: dark. */
static const uint8_t SMK_LOOP_RGB[EN_NLOOP][3] = {{40, 150, 140}, {30, 100, 200}, {140, 200, 230}, {20, 130, 80}};
static void smk_pad(int p, uint32_t r, uint32_t g, uint32_t b)
{
    plat_pad_rgb(p, (uint8_t)(r > 255u ? 255u : r), (uint8_t)(g > 255u ? 255u : g), (uint8_t)(b > 255u ? 255u : b));
}
static void smk_pads(void)
{
    float gl[SMK_NPADS] = {0};
    uint8_t shade[SMK_NPADS];
    int p, l, n = pool_count(proj.pool), nt = en_chord_n;
    if (smk_picker()) {
        int mode = proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? proj.par[P_MODE] : -1;
        for (p = 0; p < SMK_NPADS; p++) {
            int on = p < 8 ? p == proj.piece : p - 8 == mode;
            if (!proj.leds)
                smk_pad(p, 0, 0, 0);
            else if (on)
                smk_pad(p, p < 8 ? 120u : 60u, p < 8 ? 110u : 110u, p < 8 ? 30u : 20u);
            else
                smk_pad(p, 8, 7, 2);
        }
        return;
    }
    for (l = 0; l < EN_NLOOP; l++) {             /* a loop that just rang: the step it rang */
        float g = en_loop_glow[l];
        if (g > smk.lglow[l] + 0.2f) {
            int s = en_loop_note[l][en_loop_pos[l] % EN_LOOP_MAX];
            smk.lstep[l] = (int8_t)(s >= 0 && s < SMK_NPADS ? s : -1);
        }
        smk.lglow[l] = g;
    }
    for (p = 0; p < SMK_NPADS; p++)
        shade[p] = 0xFF;
    for (l = 0; l < EN_NLOOP; l++)
        if (smk.lstep[l] >= 0 && en_loop_glow[l] > gl[smk.lstep[l]]) {
            gl[smk.lstep[l]] = en_loop_glow[l];
            shade[smk.lstep[l]] = (uint8_t)l;
        }
    for (p = 0; p < SMK_NPADS; p++) {
        int st = smk_step_of_pad(p), deg = n > 0 ? st % n : 0, tone = !(deg & 1) && deg / 2 < nt;
        uint32_t r, g, b, q;
        float tp = smk.tap[st];
        if (!proj.leds) {
            smk_pad(p, 0, 0, 0);
            continue;
        }
        if (tone && en_frozen) {
            r = 6;
            g = 18;
            b = 26;
        } else if (tone) {
            r = deg ? 10u : 22u;
            g = deg ? 20u : 34u;
            b = deg ? 3u : 5u;
        } else {
            r = 6;
            g = 6;
            b = 2;
        }
        if (!en_playing) {
            r = (r + 1u) / 2u;
            g = (g + 1u) / 2u;
            b = (b + 1u) / 2u;
        }
        q = (uint32_t)(fm_clampf(gl[st], 0.0f, 1.0f) * 200.0f);
        if (shade[st] != 0xFF && q > 6u) {
            r += SMK_LOOP_RGB[shade[st]][0] * q >> 8;
            g += SMK_LOOP_RGB[shade[st]][1] * q >> 8;
            b += SMK_LOOP_RGB[shade[st]][2] * q >> 8;
        }
        if (tp > 0.02f) {                        /* a pad's own bell: the accent */
            q = (uint32_t)(tp * 220.0f);
            r += 64u * q >> 8;
            g += 128u * q >> 8;
            b += 124u * q >> 8;
            smk.tap[st] = tp * 0.94f;
        } else {
            smk.tap[st] = 0.0f;
        }
        smk_pad(p, r, g, b);
    }
}
#endif

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)proj.par[P_ROOT] << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 3u)                           /* the garden moves slowly: 20 frames a second is plenty */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
#if BOARD_SMK37
    if (smk_card())                              /* the fader card or the picker */
        return;
#endif
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_NLOOP; i++) {
        uint32_t s;
        h = hash(h, en_loop_pos[i] | (uint32_t)q16(en_loop_glow[i]) << 8);
        for (s = 0; s < EN_LOOP_LEN[i]; s++)
            h = hash(h, (uint32_t)(en_loop_note[i][s] >= 0));
    }
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, en_drum_pos[i] | (uint32_t)q16(en_drum_glow[i]) << 8 | en_drum_pat[i] << 13 | (uint32_t)en_pulse_rest << 31);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (en_frozen) {                             /* the way to the next chord, top right (held: frozen) */
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    draw_ring();
    draw_meters();
    draw_pulse();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == ui.view ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    plat_glow(proj.leds == 1);
#if BOARD_SMK37
    plat_smk_leds((uint32_t)smk.pick << SB_KNOB_BANK | (uint32_t)(smk.fbank != 0) << SB_FADER_BANK);
    smk_pads();
#endif
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
#if BOARD_SMK37
    smk_reset();
#endif
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
