/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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

#ifndef __THUNAR_LAUNCHER_H__
#define __THUNAR_LAUNCHER_H__

#include <thunar/thunar-component.h>

G_BEGIN_DECLS;

typedef struct _ThunarLauncherClass ThunarLauncherClass;
typedef struct _ThunarLauncher      ThunarLauncher;

#define THUNAR_TYPE_LAUNCHER            (thunar_launcher_get_type ())
#define THUNAR_LAUNCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_LAUNCHER, ThunarLauncher))
#define THUNAR_LAUNCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_LAUNCHER, ThunarLauncherClass))
#define THUNAR_IS_LAUNCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_LAUNCHER))
#define THUNAR_IS_LAUNCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_LAUNCHER))
#define THUNAR_LAUNCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_LAUNCHER, ThunarLauncherClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_LAUNCHER_ACTION_OPEN,
  THUNAR_LAUNCHER_ACTION_EXECUTE,
  THUNAR_LAUNCHER_ACTION_OPEN_IN_TAB,
  THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW,
  THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER,
  THUNAR_LAUNCHER_ACTION_SENDTO_MENU,
  THUNAR_LAUNCHER_ACTION_SENDTO_SHORTCUTS,
  THUNAR_LAUNCHER_ACTION_SENDTO_DESKTOP,
  THUNAR_LAUNCHER_ACTION_PROPERTIES,
  THUNAR_LAUNCHER_ACTION_MAKE_LINK,
  THUNAR_LAUNCHER_ACTION_DUPLICATE,
  THUNAR_LAUNCHER_ACTION_RENAME,
  THUNAR_LAUNCHER_ACTION_EMPTY_TRASH,
  THUNAR_LAUNCHER_ACTION_CREATE_FOLDER,
  THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT,
  THUNAR_LAUNCHER_ACTION_RESTORE,
  THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH,
  THUNAR_LAUNCHER_ACTION_DELETE,
  THUNAR_LAUNCHER_ACTION_TRASH_DELETE,
  THUNAR_LAUNCHER_ACTION_PASTE,
  THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER,
  THUNAR_LAUNCHER_ACTION_COPY,
  THUNAR_LAUNCHER_ACTION_CUT,
  THUNAR_LAUNCHER_ACTION_MOUNT,
  THUNAR_LAUNCHER_ACTION_UNMOUNT,
  THUNAR_LAUNCHER_ACTION_EJECT,
} ThunarLauncherAction;

typedef enum
{
  THUNAR_LAUNCHER_CHANGE_DIRECTORY,
  THUNAR_LAUNCHER_OPEN_AS_NEW_TAB,
  THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW,
  THUNAR_LAUNCHER_NO_ACTION,
} ThunarLauncherFolderOpenAction;

GType           thunar_launcher_get_type                             (void) G_GNUC_CONST;
void            thunar_launcher_activate_selected_files              (ThunarLauncher                 *launcher,
                                                                      ThunarLauncherFolderOpenAction  action,
                                                                      GAppInfo                       *app_info);
void            thunar_launcher_open_selected_folders                (ThunarLauncher                 *launcher,
                                                                      gboolean                        open_in_tabs);
void            thunar_launcher_set_widget                           (ThunarLauncher                 *launcher,
                                                                      GtkWidget                      *widget);
GtkWidget      *thunar_launcher_get_widget                           (ThunarLauncher                 *launcher);
void            thunar_launcher_append_accelerators                  (ThunarLauncher                 *launcher,
                                                                      GtkAccelGroup                  *accel_group);
GtkWidget      *thunar_launcher_append_menu_item                     (ThunarLauncher                 *launcher,
                                                                      GtkMenuShell                   *menu,
                                                                      ThunarLauncherAction            action,
                                                                      gboolean                        force);
gboolean        thunar_launcher_append_open_section                  (ThunarLauncher                 *launcher,
                                                                      GtkMenuShell                   *menu,
                                                                      gboolean                        support_tabs,
                                                                      gboolean                        support_change_directory,
                                                                      gboolean                        force);
gboolean        thunar_launcher_append_custom_actions                (ThunarLauncher                 *launcher,
                                                                      GtkMenuShell                   *menu);
gboolean        thunar_launcher_check_uca_key_activation             (ThunarLauncher                 *launcher,
                                                                      GdkEventKey                    *key_event);
void            thunar_launcher_action_mount                         (ThunarLauncher                 *launcher);
void            thunar_launcher_action_unmount                       (ThunarLauncher                 *launcher);
void            thunar_launcher_action_eject                         (ThunarLauncher                 *launcher);


G_END_DECLS;

#endif /* !__THUNAR_LAUNCHER_H__ */
