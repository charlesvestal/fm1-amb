/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 platform interface (from X0X's): everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c) wraps Felucca's HAL and panel map; the
 * host implementation (host/amb_host.c) is a simulator fed by a script, which is
 * what lets the whole app — UI, sequencer, engines — run and be tested on a Mac.
 * Drawing goes through Felucca's lcd_fill / lcd_blit (lcd.c on the device, a
 * framebuffer on the host) and gfx.c on top. */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go in the .pool RAM region on the device */
#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5 */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
uint32_t plat_master(void);            /* MASTER pot, 0..4096 gain Q12 */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */
void plat_glow(int on);                /* the unlit buttons glow dimly, readable in the dark (call before plat_leds) */
void plat_play_red(int red);           /* PLAY lights green; red (its other LED) while recording (before plat_leds) */

/* USB MIDI, 4-byte USB-MIDI event packets (cable 0) */
int plat_midi_in(uint32_t *pkt);       /* 1 = got one (called from the audio ISR) */
void plat_midi_out(uint32_t pkt);      /* queue one (audio ISR or main loop) */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe) */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
/* flash: OBJ_PROJ 0xFC000, OBJ_PANEL 0xDC000 (2 sectors each, inside Felucca's data area) */

/* audio load in % of the render budget, and xruns since boot (device: from the ISR) */
uint32_t plat_cpu_pct(void);
uint32_t plat_xruns(void);

#if defined(BOARD_SMK37) && BOARD_SMK37
/* The SMK-37 build: the M-Vave SMK-37's controls beyond the FM-1's (plat_fm1.c over hal/smk_input.h). plat_keys()
 * above is the keybed's F3..G5, the FM-1's 27 keys; the pads are not keys here. Pads are numbered as the hardware:
 * 0..7 the top row, 8..15 the bottom row, left to right. */
#define SMK_NKEYBED 37                 /* C3 .. C6 */
#define SMK_NPADS 16
enum { SB_KNOB_BANK, SB_FADER_BANK, SB_LEFT, SB_RIGHT, NSB };   /* the four buttons the FM-1 has not */
uint32_t plat_smk_buttons(void);       /* held: bit SB_* */
void plat_smk_leds(uint32_t lit);      /* their LEDs, bit SB_* (before plat_leds) */
uint64_t plat_keybed(void);            /* keybed keys held: bit k = key k (0 = C3) */
uint64_t plat_keybed_hits(void);       /* keybed presses since the last call */
uint32_t plat_pad_hits(void);          /* pad presses since the last call: bit p = pad p */
int plat_pad_vel(int pad);             /* velocity 1..127 of pad p's last press */
int plat_fader(int i);                 /* fader 0..3: 0..127, -1 while the readings settle */
int plat_wheels(int32_t *pitch, int32_t *modw);   /* 1 = new values: pitch -127..127 (0 at rest), mod 0..127 */
void plat_pad_rgb(int pad, uint8_t r, uint8_t g, uint8_t b);   /* pad p's LED */
#endif
