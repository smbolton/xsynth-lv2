/* Xsynth DSSI software synthesizer plugin
 *
 * Copyright (C) 2004, 2009, 2010 Sean Bolton and others.
 *
 * Portions of this file may have come from Peter Hanappe's
 * Fluidsynth, copyright (C) 2003 Peter Hanappe and others.
 * Portions of this file may have come from alsa-lib, copyright
 * and licensed under the LGPL v2.1.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifndef _XSYNTH_SYNTH_H
#define _XSYNTH_SYNTH_H

#include <pthread.h>

#include <ladspa.h>
#include "dssi.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"

#include "xsynth.h"
#include "xsynth_types.h"

#define XSYNTH_MONO_MODE_OFF  0
#define XSYNTH_MONO_MODE_ON   1
#define XSYNTH_MONO_MODE_ONCE 2
#define XSYNTH_MONO_MODE_BOTH 3

#define XSYNTH_GLIDE_MODE_LEGATO   0
#define XSYNTH_GLIDE_MODE_INITIAL  1
#define XSYNTH_GLIDE_MODE_ALWAYS   2
#define XSYNTH_GLIDE_MODE_LEFTOVER 3
#define XSYNTH_GLIDE_MODE_OFF      4

/*
 * xsynth_synth_t
 */
struct _xsynth_synth_t {
    /* output */
    LADSPA_Data    *output;
    unsigned long   sample_rate;
    float           deltat;            /* 1 / sample_rate */
    unsigned long   nugget_remains;

    /* voice tracking and data */
    unsigned int    note_id;           /* incremented for every new note, used for voice-stealing prioritization */
    int             polyphony;         /* requested polyphony, must be <= XSYNTH_MAX_POLYPHONY */
    int             voices;            /* current polyphony, either requested polyphony above or 1 while in monophonic mode */
    int             monophonic;        /* true if operating in monophonic mode */
    int             glide;             /* current glide mode */
    float           last_noteon_pitch; /* glide start pitch for non-legato modes */
    signed char     held_keys[8];      /* for monophonic key tracking, an array of note-ons, most recently received first */

    pthread_mutex_t voicelist_mutex;
    int             voicelist_mutex_grab_failed;

    xsynth_voice_t *voice[XSYNTH_MAX_POLYPHONY];

    pthread_mutex_t patches_mutex;
    xsynth_patch_t *patches;
    int             pending_program_change;
    int             current_program;

    /* current non-LADSPA-port-mapped controller values */
    unsigned char   key_pressure[128];
    unsigned char   cc[128];                  /* controller values */
    unsigned char   channel_pressure;
    unsigned char   pitch_wheel_sensitivity;  /* in semitones */
    int             pitch_wheel;              /* range is -8192 - 8191 */

    /* translated controller values */
    float           mod_wheel;                /* filter cutoff multiplier, off = 1.0, full on = 0.0 */
    float           pitch_bend;               /* frequency multiplier, product of wheel setting and sensitivity, center = 1.0 */
    float           cc_volume;                /* volume multiplier, 0.0 to 1.0 */

    /* LADSPA ports / Xsynth patch parameters */
    LADSPA_Data    *osc1_pitch;
    LADSPA_Data    *osc1_waveform;
    LADSPA_Data    *osc1_pulsewidth;
    LADSPA_Data    *osc2_pitch;
    LADSPA_Data    *osc2_waveform;
    LADSPA_Data    *osc2_pulsewidth;
    LADSPA_Data    *osc_sync;
    LADSPA_Data    *osc_balance;
    LADSPA_Data    *lfo_frequency;
    LADSPA_Data    *lfo_waveform;
    LADSPA_Data    *lfo_amount_o;
    LADSPA_Data    *lfo_amount_f;
    LADSPA_Data    *eg1_attack_time;
    LADSPA_Data    *eg1_decay_time;
    LADSPA_Data    *eg1_sustain_level;
    LADSPA_Data    *eg1_release_time;
    LADSPA_Data    *eg1_vel_sens;
    LADSPA_Data    *eg1_amount_o;
    LADSPA_Data    *eg1_amount_f;
    LADSPA_Data    *eg2_attack_time;
    LADSPA_Data    *eg2_decay_time;
    LADSPA_Data    *eg2_sustain_level;
    LADSPA_Data    *eg2_release_time;
    LADSPA_Data    *eg2_vel_sens;
    LADSPA_Data    *eg2_amount_o;
    LADSPA_Data    *eg2_amount_f;
    LADSPA_Data    *vcf_cutoff;
    LADSPA_Data    *vcf_qres;
    LADSPA_Data    *vcf_mode;
    LADSPA_Data    *glide_time;
    LADSPA_Data    *volume;
    LADSPA_Data    *tuning;
    const LV2_Atom_Sequence
                   *events;
};

void  xsynth_synth_all_voices_off(xsynth_synth_t *synth);
void  xsynth_synth_note_off(xsynth_synth_t *synth, unsigned char key,
                            unsigned char rvelocity);
void  xsynth_synth_all_notes_off(xsynth_synth_t *synth);
void  xsynth_synth_note_on(xsynth_synth_t *synth, unsigned char key,
                           unsigned char velocity);
void  xsynth_synth_key_pressure(xsynth_synth_t *synth, unsigned char key,
                                unsigned char pressure);
void  xsynth_synth_damp_voices(xsynth_synth_t *synth);
void  xsynth_synth_update_wheel_mod(xsynth_synth_t *synth);
void  xsynth_synth_control_change(xsynth_synth_t *synth, unsigned int param,
                                  signed int value);
void  xsynth_synth_channel_pressure(xsynth_synth_t *synth, signed int pressure);
void  xsynth_synth_pitch_bend(xsynth_synth_t *synth, signed int value);
void  xsynth_synth_init_controls(xsynth_synth_t *synth);
void  xsynth_synth_select_program(xsynth_synth_t *synth, unsigned long bank,
                                  unsigned long program);
void  xsynth_data_friendly_patches(xsynth_synth_t *synth);
int   xsynth_synth_set_program_descriptor(xsynth_synth_t *synth,
                                          DSSI_Program_Descriptor *pd,
                                          unsigned long bank,
                                          unsigned long program);
char *xsynth_synth_handle_patches(xsynth_synth_t *synth, const char *key,
                                  const char *value);
char *xsynth_synth_handle_polyphony(xsynth_synth_t *synth, const char *value);
char *xsynth_synth_handle_monophonic(xsynth_synth_t *synth, const char *value);
char *xsynth_synth_handle_glide(xsynth_synth_t *synth, const char *value);
char *xsynth_synth_handle_bendrange(xsynth_synth_t *synth, const char *value);
void  xsynth_synth_render_voices(xsynth_synth_t *synth, LADSPA_Data *out,
                                 unsigned long sample_count,
                                 int do_control_update);

/* in xsynth-dssi.c: */
int   xsynth_voicelist_mutex_lock(xsynth_synth_t *synth);
int   xsynth_voicelist_mutex_unlock(xsynth_synth_t *synth);
char *xsynth_dssi_configure_message(const char *fmt, ...);

/* these come right out of alsa/asoundef.h */
#define MIDI_CTL_MSB_MODWHEEL           0x01    /**< Modulation */
#define MIDI_CTL_MSB_PORTAMENTO_TIME    0x05    /**< Portamento time */
#define MIDI_CTL_MSB_MAIN_VOLUME        0x07    /**< Main volume */
#define MIDI_CTL_MSB_BALANCE            0x08    /**< Balance */
#define MIDI_CTL_LSB_MODWHEEL           0x21    /**< Modulation */
#define MIDI_CTL_LSB_PORTAMENTO_TIME    0x25    /**< Portamento time */
#define MIDI_CTL_LSB_MAIN_VOLUME        0x27    /**< Main volume */
#define MIDI_CTL_LSB_BALANCE            0x28    /**< Balance */
#define MIDI_CTL_SUSTAIN                0x40    /**< Sustain pedal */
#define MIDI_CTL_ALL_SOUNDS_OFF         0x78    /**< All sounds off */
#define MIDI_CTL_RESET_CONTROLLERS      0x79    /**< Reset Controllers */
#define MIDI_CTL_ALL_NOTES_OFF          0x7b    /**< All notes off */

#define XSYNTH_SYNTH_SUSTAINED(_s)  ((_s)->cc[MIDI_CTL_SUSTAIN] >= 64)

#endif /* _XSYNTH_SYNTH_H */
