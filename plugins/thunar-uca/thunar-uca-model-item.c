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

#include <thunar-uca/thunar-uca-model-item.h>
#include <libxfce4util/libxfce4util.h>


struct _ThunarUcaModelItemPrivate
{
  gchar *filename;
};



ThunarUcaModelItem*
thunar_uca_model_item_new (void)
{
  return g_new0 (ThunarUcaModelItem, 1);
}



gchar*
thunar_uca_model_item_get_unique_id (void)
{
  static guint counter = 0;

  /* produce a "<timestamp>-<counter>" string */
  return g_strdup_printf ("%" G_GINT64_FORMAT "-%u", g_get_real_time (), ++counter);
}



void
thunar_uca_model_item_free (ThunarUcaModelItem *item)
{
  thunar_uca_model_item_reset (item);
  if (item->priv != NULL)
    g_free (item->priv->filename);
  g_free (item->priv);
  item->priv = NULL;
  g_free (item);
}



void
thunar_uca_model_item_reset (ThunarUcaModelItem *item)
{
  ThunarUcaModelItemPrivate* private;

  /* release the previous values... */
  g_strfreev (item->patterns);
  g_free (item->description);
  g_free (item->command);
  g_free (item->name);
  g_free (item->submenu);
  g_free (item->unique_id);
  g_free (item->icon_name);

  if (item->gicon != NULL)
    g_object_unref (item->gicon);

  private = item->priv;

  /* ...and reset the item memory */
  memset (item, 0, sizeof (*item));
  item->priv = private;
}



void
thunar_uca_model_item_update (ThunarUcaModelItem *data,
                              const gchar        *name,
                              const gchar        *submenu,
                              const gchar        *unique_id,
                              const gchar        *description,
                              const gchar        *icon,
                              const gchar        *command,
                              gboolean            startup_notify,
                              const gchar        *patterns,
                              const gchar        *filename,
                              ThunarUcaTypes      types)
{
  guint m, n;

  if (G_LIKELY (name != NULL && *name != '\0'))
    data->name = g_strdup (name);
  if (G_LIKELY (submenu != NULL && *submenu != '\0'))
    data->submenu = g_strdup (submenu);
  if (G_LIKELY (icon != NULL && *icon != '\0'))
    data->icon_name = g_strdup (icon);
  if (G_LIKELY (command != NULL && *command != '\0'))
    data->command = g_strdup (command);
  if (G_LIKELY (description != NULL && *description != '\0'))
    data->description = g_strdup (description);

  data->types = types;
  data->startup_notify = startup_notify;

  /* set the unique id once */
  if (data->unique_id == NULL)
    {
      if (G_LIKELY (!xfce_str_is_empty (unique_id)))
        data->unique_id = g_strdup (unique_id);
      else
        data->unique_id = thunar_uca_model_item_get_unique_id ();
    }

  /* setup the patterns */
  data->patterns = g_strsplit (!xfce_str_is_empty (patterns) ? patterns : "*", ";", -1);
  for (m = n = 0; data->patterns[m] != NULL; ++m)
    {
      if (G_UNLIKELY (xfce_str_is_empty (data->patterns[m])))
        g_free (data->patterns[m]);
      else
        data->patterns[n++] = g_strstrip (data->patterns[m]);
    }
  data->patterns[n] = NULL;

  /* check if this data will work for multiple files */
  /* TODO #179: Some of these command codes are deprecated
   * See: https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s07.html*/
  data->multiple_selection = (command != NULL && (strstr (command, "%F") != NULL
                                                  || strstr (command, "%D") != NULL
                                                  || strstr (command, "%N") != NULL
                                                  || strstr (command, "%U") != NULL));

  /* We only update the filename once */
  if (!xfce_str_is_empty (filename))
    {
      if (data->priv == NULL)
        data->priv = g_new0 (ThunarUcaModelItemPrivate, 1);
      data->priv->filename = g_strdup (filename);
    }
}



const gchar*
thunar_uca_model_item_get_filename (ThunarUcaModelItem *data)
{
  if (data->priv != NULL)
    return data->priv->filename;
  return NULL;
}



void
thunar_uca_model_item_write_file (ThunarUcaModelItem *item,
                                  FILE               *fp)
{
  gchar *patterns;
  gchar *escaped;

  fputs ("<action>\n", fp);
  patterns = g_strjoinv (";", item->patterns);
  escaped = g_markup_printf_escaped ("\t<icon>%s</icon>\n"
                                     "\t<name>%s</name>\n"
                                     "\t<submenu>%s</submenu>\n"
                                     "\t<unique-id>%s</unique-id>\n"
                                     "\t<command>%s</command>\n"
                                     "\t<description>%s</description>\n"
                                     "\t<patterns>%s</patterns>\n",
                                     (item->icon_name != NULL) ? item->icon_name : "",
                                     (item->name != NULL) ? item->name : "",
                                     (item->submenu != NULL) ? item->submenu : "",
                                     (item->unique_id != NULL) ? item->unique_id : "",
                                     (item->command != NULL) ? item->command : "",
                                     (item->description != NULL) ? item->description : "",
                                     patterns);
  fputs (escaped, fp);
  g_free (patterns);
  g_free (escaped);
  if (item->startup_notify)
    fputs ("\t<startup-notify/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_DIRECTORIES) != 0)
    fputs ("\t<directories/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_AUDIO_FILES) != 0)
    fputs ("\t<audio-files/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_IMAGE_FILES) != 0)
    fputs ("\t<image-files/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_OTHER_FILES) != 0)
    fputs ("\t<other-files/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_TEXT_FILES) != 0)
    fputs ("\t<text-files/>\n", fp);
  if ((item->types & THUNAR_UCA_TYPE_VIDEO_FILES) != 0)
    fputs ("\t<video-files/>\n", fp);
  fputs ("</action>\n", fp);
}
