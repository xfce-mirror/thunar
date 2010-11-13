/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef __THUNAR_LOCATION_DIALOG_H__
#define __THUNAR_LOCATION_DIALOG_H__

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarLocationDialogClass ThunarLocationDialogClass;
typedef struct _ThunarLocationDialog      ThunarLocationDialog;

#define THUNAR_TYPE_LOCATION_DIALOG             (thunar_location_dialog_get_type ())
#define THUNAR_LOCATION_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_LOCATION_DIALOG, ThunarLocationDialog))
#define THUNAR_LOCATION_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_LOCATION_DIALOG, ThunarLocationDialogClass))
#define THUNAR_IS_LOCATION_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_LOCATION_DIALOG))
#define THUNAR_IS_LOCATION_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_LOCATION_DIALOG))
#define THUNAR_LOCATION_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_LOCATION_DIALOG, ThunarLocationDialogClass))

struct _ThunarLocationDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarLocationDialog
{
  ThunarAbstractDialog __parent__;
  GtkWidget           *entry;
};

GType       thunar_location_dialog_get_type              (void) G_GNUC_CONST;

GtkWidget  *thunar_location_dialog_new                   (void) G_GNUC_MALLOC;

ThunarFile *thunar_location_dialog_get_selected_file     (ThunarLocationDialog *location_dialog);
void        thunar_location_dialog_set_selected_file     (ThunarLocationDialog *location_dialog,
                                                          ThunarFile           *selected_file);
void        thunar_location_dialog_set_working_directory (ThunarLocationDialog *location_dialog,
                                                          ThunarFile           *directory);

G_END_DECLS;

#endif /* !__THUNAR_LOCATION_DIALOG_H__ */
