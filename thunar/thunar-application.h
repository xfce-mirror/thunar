/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
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

#ifndef __THUNAR_APPLICATION_H__
#define __THUNAR_APPLICATION_H__

#include <thunar/thunar-window.h>

G_BEGIN_DECLS;

typedef struct _ThunarApplicationClass ThunarApplicationClass;
typedef struct _ThunarApplication      ThunarApplication;

#define THUNAR_TYPE_APPLICATION             (thunar_application_get_type ())
#define THUNAR_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_APPLICATION, ThunarApplication))
#define THUNAR_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_APPLICATION, ThunarApplicationClass))
#define THUNAR_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_APPLICATION))
#define THUNAR_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_APPLICATION))
#define THUNAR_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_APPLICATION, ThunarApplicationClass))

GType              thunar_application_get_type          (void) G_GNUC_CONST;

ThunarApplication *thunar_application_get               (void);

gboolean           thunar_application_get_daemon        (ThunarApplication *application);
void               thunar_application_set_daemon        (ThunarApplication *application,
                                                         gboolean           daemon);

GList             *thunar_application_get_windows       (ThunarApplication *application);

gboolean           thunar_application_has_windows       (ThunarApplication *application);

void               thunar_application_take_window       (ThunarApplication *application,
                                                         GtkWindow         *window);

void               thunar_application_open_window       (ThunarApplication *application,
                                                         ThunarFile        *directory,
                                                         GdkScreen         *screen);

gboolean           thunar_application_process_filenames (ThunarApplication *application,
                                                         const gchar       *working_directory,
                                                         gchar            **filenames,
                                                         GdkScreen         *screen,
                                                         GError           **error);

gboolean           thunar_application_restore_session   (ThunarApplication *application,
                                                         const gchar       *session_data,
                                                         GError           **error);

void               thunar_application_copy_to           (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *source_path_list,
                                                         GList             *target_path_list,
                                                         GClosure          *new_files_closure);

void               thunar_application_copy_into         (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *source_path_list,
                                                         ThunarVfsPath     *target_path,
                                                         GClosure          *new_files_closure);

void               thunar_application_link_into         (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *source_path_list,
                                                         ThunarVfsPath     *target_path,
                                                         GClosure          *new_files_closure);

void               thunar_application_move_into         (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *source_path_list,
                                                         ThunarVfsPath     *target_path,
                                                         GClosure          *new_files_closure);

void               thunar_application_unlink            (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *path_list);

void               thunar_application_creat             (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *path_list,
                                                         GClosure          *new_files_closure);

void               thunar_application_mkdir             (ThunarApplication *application,
                                                         GtkWidget         *widget,
                                                         GList             *path_list,
                                                         GClosure          *new_files_closure);

G_END_DECLS;

#endif /* !__THUNAR_APPLICATION_H__ */
