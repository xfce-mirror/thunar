/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_DIALOGS_H__
#define __THUNAR_DIALOGS_H__

#include <thunar-vfs/thunar-vfs.h>

G_BEGIN_DECLS;

void                  thunar_dialogs_show_about           (GtkWindow            *parent,
                                                           const gchar          *title,
                                                           const gchar          *format,
                                                           ...) G_GNUC_INTERNAL G_GNUC_PRINTF (3, 4);

void                  thunar_dialogs_show_error           (gpointer              parent,
                                                           const GError         *error,
                                                           const gchar          *format,
                                                           ...) G_GNUC_INTERNAL G_GNUC_PRINTF (3, 4);

void                  thunar_dialogs_show_help            (gpointer              parent,
                                                           const gchar          *page,
                                                           const gchar          *offset) G_GNUC_INTERNAL;

ThunarVfsJobResponse  thunar_dialogs_show_job_ask         (GtkWindow            *parent,
                                                           const gchar          *question,
                                                           ThunarVfsJobResponse  choices) G_GNUC_INTERNAL;

ThunarVfsJobResponse  thunar_dialogs_show_job_ask_replace (GtkWindow            *parent,
                                                           ThunarVfsInfo        *src_info,
                                                           ThunarVfsInfo        *dst_info) G_GNUC_INTERNAL;

void                  thunar_dialogs_show_job_error       (GtkWindow            *parent,
                                                           GError               *error) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_DIALOGS_H__ */
