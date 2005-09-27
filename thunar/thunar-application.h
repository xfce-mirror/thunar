/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005 Jeff Franks <jcfranks@xfce.org>
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

GType              thunar_application_get_type      (void) G_GNUC_CONST;

ThunarApplication *thunar_application_get           (void);

GList             *thunar_application_get_windows   (ThunarApplication *application);

void               thunar_application_open_window   (ThunarApplication *application,
                                                     ThunarFile        *directory,
                                                     GdkScreen         *screen);

void               thunar_application_copy_uris     (ThunarApplication *application,
                                                     GtkWindow         *window,
                                                     GList             *source_uri_list,
                                                     ThunarVfsURI      *target_uri);

void               thunar_application_move_uris     (ThunarApplication *application,
                                                     GtkWindow         *window,
                                                     GList             *source_uri_list,
                                                     ThunarVfsURI      *target_uri);

void               thunar_application_delete_uris   (ThunarApplication *application,
                                                     GtkWindow         *window,
                                                     GList             *uri_list);

G_END_DECLS;

#endif /* !__THUNAR_APPLICATION_H__ */
