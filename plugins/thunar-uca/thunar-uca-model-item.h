/*
 * Copyright (c) 2021 Chigozirim Chukwu <noblechuk5[at]web[dot]de>
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

#ifndef __THUNAR_UCA_MODEL_ITEM_H__
#define __THUNAR_UCA_MODEL_ITEM_H__

#include <gio/gio.h>
#include <glib/gstdio.h>

G_BEGIN_DECLS

typedef struct _ThunarUcaModelItemPrivate ThunarUcaModelItemPrivate;
typedef struct _ThunarUcaModelItem ThunarUcaModelItem;



/**
 * ThunarUcaTypes:
 * @THUNAR_UCA_TYPE_DIRECTORIES : directories.
 * @THUNAR_UCA_TYPE_AUDIO_FILES : audio files.
 * @THUNAR_UCA_TYPE_IMAGE_FILES : image files.
 * @THUNAR_UCA_TYPE_OTHER_FILES : other files.
 * @THUNAR_UCA_TYPE_TEXT_FILES  : text files.
 * @THUNAR_UCA_TYPE_VIDEO_FILES : video files.
 **/
typedef enum /*< flags >*/
{
  THUNAR_UCA_TYPE_DIRECTORIES = 1 << 0,
  THUNAR_UCA_TYPE_AUDIO_FILES = 1 << 1,
  THUNAR_UCA_TYPE_IMAGE_FILES = 1 << 2,
  THUNAR_UCA_TYPE_OTHER_FILES = 1 << 3,
  THUNAR_UCA_TYPE_TEXT_FILES  = 1 << 4,
  THUNAR_UCA_TYPE_VIDEO_FILES = 1 << 5,
} ThunarUcaTypes;



struct _ThunarUcaModelItem
{
  gchar						 *name;
  gchar						 *submenu;
  gchar						 *description;
  gchar						 *unique_id;
  gchar						 *icon_name;
  GIcon						 *gicon;
  gchar						 *command;
  guint          			  startup_notify : 1;
  gchar        				**patterns;
  ThunarUcaTypes 			  types;

  /* derived attributes */
  guint          			  multiple_selection : 1;
  ThunarUcaModelItemPrivate	 *priv;
};


ThunarUcaModelItem  *thunar_uca_model_item_new 				(void) G_GNUC_MALLOC;

gchar				*thunar_uca_model_item_get_unique_id	(void);
void				 thunar_uca_model_item_free 			(ThunarUcaModelItem *data);
void				 thunar_uca_model_item_reset 			(ThunarUcaModelItem *data);
void				 thunar_uca_model_item_update 			(ThunarUcaModelItem *data,
										  					 const gchar		*name,
															 const gchar		*submenu,
															 const gchar		*unique_id,
															 const gchar		*description,
															 const gchar		*icon,
															 const gchar		*command,
															 gboolean			 startup_notify,
															 const gchar		*patterns,
															 const gchar 		*filename,
															 ThunarUcaTypes		 types);
const gchar 		*thunar_uca_model_item_get_filename		(ThunarUcaModelItem *data);
void	 			 thunar_uca_model_item_write_file		(ThunarUcaModelItem *item,
															 FILE 				*fp);

G_END_DECLS

#endif // __THUNAR_UCA_MODEL_ITEM_H__
