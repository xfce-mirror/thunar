/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (THUNAR_VFS_INSIDE_THUNAR_VFS_H) && !defined (THUNAR_VFS_COMPILATION)
#error "Only <thunar-vfs/thunar-vfs.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __THUNAR_VFS_MIME_HANDLER_H__
#define __THUNAR_VFS_MIME_HANDLER_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeHandlerClass ThunarVfsMimeHandlerClass;
typedef struct _ThunarVfsMimeHandler      ThunarVfsMimeHandler;

#define THUNAR_VFS_TYPE_MIME_HANDLER            (thunar_vfs_mime_handler_get_type ())
#define THUNAR_VFS_MIME_HANDLER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_HANDLER, ThunarVfsMimeHandler))
#define THUNAR_VFS_MIME_HANDLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_HANDLER, ThunarVfsMimeHandlerClass))
#define THUNAR_VFS_IS_MIME_HANDLER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_HANDLER))
#define THUNAR_VFS_IS_MIME_HANDLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_HANDLER))
#define THUNAR_VFS_MIME_HANDLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_HANDLER, ThunarVfsMimeHandlerClass))

/**
 * ThunarVfsMimeHandlerFlags:
 * @THUNAR_VFS_MIME_HANDLER_HIDDEN                  : the handler should not be displayed in the menu system.
 * @THUNAR_VFS_MIME_HANDLER_REQUIRES_TERMINAL       : the handler must be run in a terminal.
 * @THUNAR_VFS_MIME_HANDLER_SUPPORTS_STARTUP_NOTIFY : the handler supports startup notification.
 * @THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI          : the handler supports opening multiple documents at once (%F or %U).
 * @THUNAR_VFS_MIME_HANDLER_SUPPORTS_URIS           : the handler supports opening URIs (%u or %U).
 *
 * Various flags associated with a #ThunarVfsMimeHandler.
 **/
typedef enum /*< flags >*/
{
  THUNAR_VFS_MIME_HANDLER_HIDDEN                  = (1 << 0L),
  THUNAR_VFS_MIME_HANDLER_REQUIRES_TERMINAL       = (1 << 1L),
  THUNAR_VFS_MIME_HANDLER_SUPPORTS_STARTUP_NOTIFY = (1 << 2L),
  THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI          = (1 << 3L),
  THUNAR_VFS_MIME_HANDLER_SUPPORTS_URIS           = (1 << 4L),
} ThunarVfsMimeHandlerFlags;

GType                     thunar_vfs_mime_handler_get_type          (void) G_GNUC_CONST;

const gchar              *thunar_vfs_mime_handler_get_command       (const ThunarVfsMimeHandler *mime_handler);
ThunarVfsMimeHandlerFlags thunar_vfs_mime_handler_get_flags         (const ThunarVfsMimeHandler *mime_handler);
const gchar              *thunar_vfs_mime_handler_get_name          (const ThunarVfsMimeHandler *mime_handler);

gboolean                  thunar_vfs_mime_handler_exec              (const ThunarVfsMimeHandler *mime_handler,
                                                                     GdkScreen                  *screen,
                                                                     GList                      *path_list,
                                                                     GError                    **error);
gboolean                  thunar_vfs_mime_handler_exec_with_env     (const ThunarVfsMimeHandler *mime_handler,
                                                                     GdkScreen                  *screen,
                                                                     GList                      *path_list,
                                                                     gchar                     **envp,
                                                                     GError                    **error);

const gchar              *thunar_vfs_mime_handler_lookup_icon_name  (const ThunarVfsMimeHandler *mime_handler,
                                                                     GtkIconTheme               *icon_theme);

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_HANDLER_H__ */
