/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-image.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-thumbnailer.h>



#define THUNAR_IMAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_IMAGE, ThunarImagePrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void thunar_image_finalize             (GObject           *object);
static void thunar_image_get_property         (GObject           *object,
                                               guint              prop_id,
                                               GValue            *value,
                                               GParamSpec        *pspec);
static void thunar_image_set_property         (GObject           *object,
                                               guint              prop_id,
                                               const GValue      *value,
                                               GParamSpec        *pspec);
static void thunar_image_file_changed         (ThunarImage       *image);
static void thunar_image_thumbnailer_error    (ThunarThumbnailer *thumbnailer,
                                               guint              handle,
                                               const gchar      **uris,
                                               gint               code,
                                               const gchar       *message,
                                               ThunarImage       *image);
static void thunar_image_thumbnailer_finished (ThunarThumbnailer *thumbnailer,
                                               guint              handle,
                                               ThunarImage       *image);
static void thunar_image_thumbnailer_started  (ThunarThumbnailer *thumbnailer,
                                               guint              handle,
                                               ThunarImage       *image);



struct _ThunarImageClass
{
  GtkImageClass __parent__;
};

struct _ThunarImage
{
  GtkImage __parent__;

  ThunarImagePrivate *priv;
};

struct _ThunarImagePrivate
{
  ThunarThumbnailer *thumbnailer;
  ThunarFile        *file;
  guint              thumbnailer_handle;
};



G_DEFINE_TYPE (ThunarImage, thunar_image, GTK_TYPE_IMAGE);



static void
thunar_image_class_init (ThunarImageClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (ThunarImagePrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_image_finalize; 
  gobject_class->get_property = thunar_image_get_property;
  gobject_class->set_property = thunar_image_set_property;

  g_object_class_install_property (gobject_class, PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        THUNAR_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}



static void
thunar_image_init (ThunarImage *image)
{
  image->priv = THUNAR_IMAGE_GET_PRIVATE (image);
  image->priv->file = NULL;

  g_signal_connect (image, "notify::file", G_CALLBACK (thunar_image_file_changed), NULL);

  image->priv->thumbnailer = thunar_thumbnailer_new ();
}



static void
thunar_image_finalize (GObject *object)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  thunar_image_set_file (image, NULL);

  g_object_unref (image->priv->thumbnailer);

  (*G_OBJECT_CLASS (thunar_image_parent_class)->finalize) (object);
}



static void
thunar_image_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, image->priv->file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_image_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ThunarImage *image = THUNAR_IMAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_image_set_file (image, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_image_file_changed (ThunarImage *image)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GdkPixbuf         *icon;
  GdkScreen         *screen;

  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->thumbnailer_handle != 0)
    {
      thunar_thumbnailer_unqueue (image->priv->thumbnailer, 
                                  image->priv->thumbnailer_handle);

      image->priv->thumbnailer_handle = 0;

      g_signal_handlers_disconnect_matched (image->priv->thumbnailer, 
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                            image);
    }

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);

  if (image->priv->file != NULL)
    {
      g_signal_connect (image->priv->thumbnailer, "error", 
                        G_CALLBACK (thunar_image_thumbnailer_error), image);
      g_signal_connect (image->priv->thumbnailer, "finished", 
                        G_CALLBACK (thunar_image_thumbnailer_finished), image);
      g_signal_connect (image->priv->thumbnailer, "started", 
                        G_CALLBACK (thunar_image_thumbnailer_started), image);

      if (!thunar_thumbnailer_queue_file (image->priv->thumbnailer, image->priv->file, 
                                          &image->priv->thumbnailer_handle))
        {
          g_signal_handlers_disconnect_matched (image->priv->thumbnailer, 
                                                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                                image);
        }
          
      screen = gtk_widget_get_screen (GTK_WIDGET (image));
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      icon = thunar_icon_factory_load_file_icon (icon_factory, image->priv->file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT, 48);

      gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);

      g_object_unref (icon_factory);
    }
}



static void
thunar_image_thumbnailer_error (ThunarThumbnailer *thumbnailer,
                                guint              handle,
                                const gchar      **uris,
                                gint               code,
                                const gchar       *message,
                                ThunarImage       *image)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GdkPixbuf         *icon;
  GdkScreen         *screen;

  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->thumbnailer_handle != handle)
    return;
  
  image->priv->thumbnailer_handle = 0;

  if (image->priv->file == NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
    }
  else
    {
      screen = gtk_widget_get_screen (GTK_WIDGET (image));
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      icon = thunar_icon_factory_load_file_icon (icon_factory, image->priv->file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT, 48);

      gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);

      g_object_unref (icon_factory);
    }
}



static void
thunar_image_thumbnailer_finished (ThunarThumbnailer *thumbnailer,
                                   guint              handle,
                                   ThunarImage       *image)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GdkPixbuf         *icon;
  GdkScreen         *screen;

  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->thumbnailer_handle != handle)
    return;

  image->priv->thumbnailer_handle = 0;

  g_signal_handlers_disconnect_matched (image->priv->thumbnailer, 
                                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                        image);

  if (image->priv->file == NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
    }
  else
    {
      /* TODO we only need to reload if the thumbnail was regenerated */
      thunar_file_changed (image->priv->file);

      screen = gtk_widget_get_screen (GTK_WIDGET (image));
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      icon = thunar_icon_factory_load_file_icon (icon_factory, image->priv->file,
                                                 THUNAR_FILE_ICON_STATE_DEFAULT, 48);

      gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);
      
      g_object_unref (icon_factory);
    }
}



static void
thunar_image_thumbnailer_started (ThunarThumbnailer *thumbnailer,
                                  guint              handle,
                                  ThunarImage       *image)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GdkPixbuf         *icon;
  GdkScreen         *screen;

  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->thumbnailer_handle != handle)
    return;

  if (image->priv->file == NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
    }
  else
    {
      screen = gtk_widget_get_screen (GTK_WIDGET (image));
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      icon = thunar_icon_factory_load_icon (icon_factory, "gnome-fs-loading-icon", 48,
                                            NULL, FALSE);

      gtk_image_set_from_pixbuf (GTK_IMAGE (image), icon);
      
      g_object_unref (icon_factory);
    }
}



GtkWidget *
thunar_image_new (void)
{
  return g_object_new (THUNAR_TYPE_IMAGE, NULL);
}



void
thunar_image_set_file (ThunarImage *image,
                       ThunarFile  *file)
{
  _thunar_return_if_fail (THUNAR_IS_IMAGE (image));

  if (image->priv->file != NULL)
    {
      if (image->priv->file == file)
        return;

      g_object_unref (image->priv->file);
    }

  if (file != NULL)
    image->priv->file = g_object_ref (file);
  else
    image->priv->file = NULL;

  g_object_notify (G_OBJECT (image), "file");
}
