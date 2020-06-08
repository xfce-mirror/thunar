/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SHORTCUTS_PANE_H__
#define __THUNAR_SHORTCUTS_PANE_H__

#include <thunar/thunar-side-pane.h>

G_BEGIN_DECLS;

typedef struct _ThunarShortcutsPaneClass ThunarShortcutsPaneClass;
typedef struct _ThunarShortcutsPane      ThunarShortcutsPane;

#define THUNAR_TYPE_SHORTCUTS_PANE             (thunar_shortcuts_pane_get_type ())
#define THUNAR_SHORTCUTS_PANE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SHORTCUTS_PANE, ThunarShortcutsPane))
#define THUNAR_SHORTCUTS_PANE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SHORTCUTS_PANE, ThunarShortcutsPaneClass))
#define THUNAR_IS_SHORTCUTS_PANE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SHORTCUTS_PANE))
#define THUNAR_IS_SHORTCUTS_PANE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SHORTCUTS_PANE))
#define THUNAR_SHORTCUTS_PANE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_SHORTCUTS_PANE, ThunarShortcutsPaneClass))

GType      thunar_shortcuts_pane_get_type     (void) G_GNUC_CONST;
void       thunar_shortcuts_pane_add_shortcut (ThunarShortcutsPane *shortcuts_pane,
                                               ThunarFile          *file);

G_END_DECLS;

#endif /* !__THUNAR_SHORTCUTS_PANE_H__ */
