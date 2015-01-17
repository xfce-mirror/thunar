/* vi:set et ai sw=2 sts=2 ts=2: */
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

#ifndef __THUNAR_TEMPLATES_ACTION_H__
#define __THUNAR_TEMPLATES_ACTION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarTemplatesActionClass ThunarTemplatesActionClass;
typedef struct _ThunarTemplatesAction      ThunarTemplatesAction;

#define THUNAR_TYPE_TEMPLATES_ACTION            (thunar_templates_action_get_type ())
#define THUNAR_TEMPLATES_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TEMPLATES_ACTION, ThunarTemplatesAction))
#define THUNAR_TEMPLATES_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TEMPLATES_ACTION, ThunarTemplatesActionClass))
#define THUNAR_IS_TEMPLATES_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TEMPLATES_ACTION))
#define THUNAR_IS_TEMPLATES_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TEMPLATES_ACTION))
#define THUNAR_TEMPLATES_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TEMPLATES_ACTION, ThunarTemplatesActionClass))

GType      thunar_templates_action_get_type (void) G_GNUC_CONST;

GtkAction *thunar_templates_action_new      (const gchar *name,
                                             const gchar *label) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_TEMPLATES_ACTION_H__ */
