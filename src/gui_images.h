/* Xsynth DSSI software synthesizer GUI
 *
 * Copyright (C) 2004, 2009 Sean Bolton.
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

#ifndef _GUI_IMAGES_H
#define _GUI_IMAGES_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

/* gui_images.c */
GtkWidget *create_about_image(void);
GtkWidget *create_logo_image(void);

void       create_waveform_gdk_pixbufs(void);
void       free_waveform_gdk_pixbufs(void);
GtkWidget* create_waveform_gtk_image(void);
void       set_waveform_image(GtkWidget *widget, int waveform);

#endif /* _GUI_IMAGES_H */
