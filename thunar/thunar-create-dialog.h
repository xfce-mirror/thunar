/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_CREATE_DIALOG_H__
#define __THUNAR_CREATE_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarCreateDialogClass ThunarCreateDialogClass;
typedef struct _ThunarCreateDialog      ThunarCreateDialog;

#define THUNAR_TYPE_CREATE_DIALOG             (thunar_create_dialog_get_type ())
#define THUNAR_CREATE_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_CREATE_DIALOG, ThunarCreateDialog))
#define THUNAR_CREATE_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_CREATE_DIALOG, ThunarCreateDialogClass))
#define THUNAR_IS_CREATE_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_CREATE_DIALOG))
#define THUNAR_IS_CREATE_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_CREATE_DIALOG))
#define THUNAR_CREATE_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_CREATE_DIALOG, ThunarCreateDialogClass))

GType  thunar_create_dialog_get_type (void) G_GNUC_CONST;

gchar *thunar_show_create_dialog     (gpointer     parent,
                                      const gchar *content_type,
                                      const gchar *filename,
                                      const gchar *title) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_CREATE_DIALOG_H__ */
