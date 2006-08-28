/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_PATH_BUTTON_H__
#define __THUNAR_PATH_BUTTON_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarPathButtonClass ThunarPathButtonClass;
typedef struct _ThunarPathButton      ThunarPathButton;

#define THUNAR_TYPE_PATH_BUTTON             (thunar_path_button_get_type ())
#define THUNAR_PATH_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PATH_BUTTON, ThunarPathButton))
#define THUNAR_PATH_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PATH_BUTTON, ThunarPathButtonClass))
#define THUNAR_IS_PATH_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PATH_BUTTON))
#define THUNAR_IS_PATH_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PATH_BUTTON))
#define THUNAR_PATH_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PATH_BUTTON, ThunarPathButtonClass))

GType       thunar_path_button_get_type   (void) G_GNUC_CONST;

GtkWidget  *thunar_path_button_new        (void) G_GNUC_MALLOC;

gboolean    thunar_path_button_get_active (ThunarPathButton *path_button);
void        thunar_path_button_set_active (ThunarPathButton *path_button,
                                           gboolean          active);

ThunarFile *thunar_path_button_get_file   (ThunarPathButton *path_button);
void        thunar_path_button_set_file   (ThunarPathButton *path_button,
                                           ThunarFile       *file);

void        thunar_path_button_clicked    (ThunarPathButton *path_button);

G_END_DECLS;

#endif /* !__THUNAR_PATH_BUTTON_H__ */
