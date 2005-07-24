/* $Id$ */
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

#ifndef __THUNAR_DESKTOP_MODEL_H__
#define __THUNAR_DESKTOP_MODEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarDesktopPosition ThunarDesktopPosition;

#define THUNAR_TYPE_DESKTOP_POSITION (thunar_desktop_position_get_type ())

struct _ThunarDesktopPosition
{
  gint row;
  gint column;
  gint screen_number;
};

GType                  thunar_desktop_position_get_type (void) G_GNUC_CONST;

ThunarDesktopPosition *thunar_desktop_position_dup      (const ThunarDesktopPosition *position) G_GNUC_MALLOC;
void                   thunar_desktop_position_free     (ThunarDesktopPosition       *position);


typedef struct _ThunarDesktopModelClass ThunarDesktopModelClass;
typedef struct _ThunarDesktopModel      ThunarDesktopModel;

#define THUNAR_TYPE_DESKTOP_MODEL             (thunar_desktop_model_get_type ())
#define THUNAR_DESKTOP_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DESKTOP_MODEL, ThunarDesktopModel))
#define THUNAR_DESKTOP_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DESKTOP_MODEL, ThunarDesktopModelClass))
#define THUNAR_IS_DESKTOP_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DESKTOP_MODEL))
#define THUNAR_IS_DESKTOP_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_DESKTOP_MODEL))
#define THUNAR_DESKTOP_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DESKTOP_MODEL, ThunarDesktopModelClass))

enum
{
  THUNAR_DESKTOP_MODEL_COLUMN_FILE,
  THUNAR_DESKTOP_MODEL_COLUMN_POSITION,
  THUNAR_DESKTOP_MODEL_N_COLUMNS,
};

GType               thunar_desktop_model_get_type     (void) G_GNUC_CONST;

ThunarDesktopModel *thunar_desktop_model_get_default  (void);

G_END_DECLS;

#endif /* !__THUNAR_DESKTOP_MODEL_H__ */
