/* $Id$ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-emblem-chooser.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void       thunar_emblem_chooser_dispose         (GObject                  *object);
static void       thunar_emblem_chooser_finalize        (GObject                  *object);
static void       thunar_emblem_chooser_get_property    (GObject                  *object,
                                                         guint                     prop_id,
                                                         GValue                   *value,
                                                         GParamSpec               *pspec);
static void       thunar_emblem_chooser_set_property    (GObject                  *object,
                                                         guint                     prop_id,
                                                         const GValue             *value,
                                                         GParamSpec               *pspec);
static void       thunar_emblem_chooser_realize         (GtkWidget                *widget);
static void       thunar_emblem_chooser_unrealize       (GtkWidget                *widget);
static void       thunar_emblem_chooser_button_toggled  (GtkToggleButton          *button,
                                                         ThunarEmblemChooser      *chooser);
static void       thunar_emblem_chooser_file_changed    (ThunarFile               *file,
                                                         ThunarEmblemChooser      *chooser);
static void       thunar_emblem_chooser_theme_changed   (GtkIconTheme             *icon_theme,
                                                         ThunarEmblemChooser      *chooser);
static void       thunar_emblem_chooser_create_buttons  (ThunarEmblemChooser      *chooser);
static GtkWidget *thunar_emblem_chooser_create_button   (ThunarEmblemChooser      *chooser,
                                                         const gchar              *emblem);



struct _ThunarEmblemChooserClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarEmblemChooser
{
  GtkScrolledWindow __parent__;

  GtkIconTheme *icon_theme;
  GtkSizeGroup *size_group;
  ThunarFile   *file;
  GtkWidget    *table;
};



G_DEFINE_TYPE (ThunarEmblemChooser, thunar_emblem_chooser, GTK_TYPE_SCROLLED_WINDOW)



static void
thunar_emblem_chooser_class_init (ThunarEmblemChooserClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_emblem_chooser_dispose;
  gobject_class->finalize = thunar_emblem_chooser_finalize;
  gobject_class->get_property = thunar_emblem_chooser_get_property;
  gobject_class->set_property = thunar_emblem_chooser_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_emblem_chooser_realize;
  gtkwidget_class->unrealize = thunar_emblem_chooser_unrealize;

  /**
   * ThunarEmblemChooser::file:
   *
   * The file for which emblems should be choosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_emblem_chooser_init (ThunarEmblemChooser *chooser)
{
  GtkWidget *viewport;

  /* setup the scrolled window instance */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (chooser), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (chooser), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (chooser), NULL);

  /* setup the viewport */
  viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (chooser)),
                               gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser)));
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (chooser), viewport);
  gtk_widget_show (viewport);

  /* setup the wrap table */
  chooser->table = g_object_new (EXO_TYPE_WRAP_TABLE, "border-width", 6, "homogeneous", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (viewport), chooser->table);
  gtk_widget_show (chooser->table);

  /* allocate a size-group for the buttons */
  chooser->size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
}



static void
thunar_emblem_chooser_dispose (GObject *object)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (object);

  /* disconnect from the file */
  thunar_emblem_chooser_set_file (chooser, NULL);

  (*G_OBJECT_CLASS (thunar_emblem_chooser_parent_class)->dispose) (object);
}



static void
thunar_emblem_chooser_finalize (GObject *object)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (object);

  /* release the size-group */
  g_object_unref (G_OBJECT (chooser->size_group));

  (*G_OBJECT_CLASS (thunar_emblem_chooser_parent_class)->finalize) (object);
}



static void
thunar_emblem_chooser_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_emblem_chooser_get_file (chooser));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_emblem_chooser_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_emblem_chooser_set_file (chooser, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_emblem_chooser_realize (GtkWidget *widget)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (widget);

  /* let the GtkWidget class perform the realization */
  (*GTK_WIDGET_CLASS (thunar_emblem_chooser_parent_class)->realize) (widget);

  /* determine the icon theme for the new screen */
  chooser->icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  g_signal_connect (G_OBJECT (chooser->icon_theme), "changed",
                    G_CALLBACK (thunar_emblem_chooser_theme_changed),
                    chooser);
  g_object_ref (G_OBJECT (chooser->icon_theme));

  /* create the emblem buttons */
  thunar_emblem_chooser_create_buttons (chooser);
}



static void
thunar_emblem_chooser_unrealize (GtkWidget *widget)
{
  ThunarEmblemChooser *chooser = THUNAR_EMBLEM_CHOOSER (widget);

  /* drop all check buttons */
  gtk_container_foreach (GTK_CONTAINER (chooser->table), (GtkCallback) gtk_widget_destroy, NULL);

  /* release our reference on the icon theme */
  g_signal_handlers_disconnect_by_func (G_OBJECT (chooser->icon_theme), thunar_emblem_chooser_theme_changed, chooser);
  g_object_unref (G_OBJECT (chooser->icon_theme));
  chooser->icon_theme = NULL;

  /* let the GtkWidget class perform the unrealization */
  (*GTK_WIDGET_CLASS (thunar_emblem_chooser_parent_class)->unrealize) (widget);
}



static void
thunar_emblem_chooser_button_toggled (GtkToggleButton     *button,
                                      ThunarEmblemChooser *chooser)
{
  const gchar *emblem_name;
  GList *emblem_names = NULL;
  GList *children;
  GList *lp;

  _thunar_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_EMBLEM_CHOOSER (chooser));

  /* we just ignore toggle events if no file is set */
  if (G_LIKELY (chooser->file == NULL))
    return;

  /* determine the list of selected emblems */
  children = gtk_container_get_children (GTK_CONTAINER (chooser->table));
  for (lp = children; lp != NULL; lp = lp->next)
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lp->data)))
      {
        emblem_name = g_object_get_data (G_OBJECT (lp->data), I_("thunar-emblem"));
        emblem_names = g_list_append (emblem_names, g_strdup (emblem_name));
      }
  g_list_free (children);

  /* setup the new emblem name list for the file */
  thunar_file_set_emblem_names (chooser->file, emblem_names);

  /* release the emblem name list */
  g_list_foreach (emblem_names, (GFunc) g_free, NULL);
  g_list_free (emblem_names);
}



static void
thunar_emblem_chooser_file_changed (ThunarFile          *file,
                                    ThunarEmblemChooser *chooser)
{
  const gchar *emblem_name;
  GList       *emblem_names;
  GList       *children;
  GList       *lp;

  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_EMBLEM_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->file == file);

  /* temporarily reset the file attribute */
  chooser->file = NULL;

  /* determine the emblems set for the file */
  emblem_names = thunar_file_get_emblem_names (file);

  /* toggle the button states appropriately */
  children = gtk_container_get_children (GTK_CONTAINER (chooser->table));
  for (lp = children; lp != NULL; lp = lp->next)
    {
      emblem_name = g_object_get_data (G_OBJECT (lp->data), I_("thunar-emblem"));
      if (g_list_find_custom (emblem_names, emblem_name, (GCompareFunc) strcmp) != NULL)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lp->data), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lp->data), FALSE);
    }
  g_list_free (children);

  /* reset the file attribute */
  chooser->file = file;
}



static void
thunar_emblem_chooser_theme_changed (GtkIconTheme        *icon_theme,
                                     ThunarEmblemChooser *chooser)
{
  _thunar_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  _thunar_return_if_fail (THUNAR_IS_EMBLEM_CHOOSER (chooser));
  _thunar_return_if_fail (chooser->icon_theme == icon_theme);

  /* drop the current buttons */
  gtk_container_foreach (GTK_CONTAINER (chooser->table), (GtkCallback) gtk_widget_destroy, NULL);

  /* create buttons for the new theme */
  thunar_emblem_chooser_create_buttons (chooser);
}



static void
thunar_emblem_chooser_create_buttons (ThunarEmblemChooser *chooser)
{
  GtkWidget *button;
  GList     *emblems;
  GList     *lp;

  /* determine the emblems for the icon theme */
  emblems = gtk_icon_theme_list_icons (chooser->icon_theme, "Emblems");

  /* sort the emblem list */
  emblems = g_list_sort (emblems, (GCompareFunc) g_ascii_strcasecmp);

  /* create buttons for the emblems */
  for (lp = emblems; lp != NULL; lp = lp->next)
    {
      /* skip special emblems, as they cannot be selected */
      if (strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK) != 0
          && strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_READ) != 0
          && strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_WRITE) != 0
          && strcmp (lp->data, THUNAR_FILE_EMBLEM_NAME_DESKTOP) != 0)
        {
          /* create a button and add it to the table */
          button = thunar_emblem_chooser_create_button (chooser, lp->data);
          if (G_LIKELY (button != NULL))
            {
              gtk_container_add (GTK_CONTAINER (chooser->table), button);
              gtk_widget_show (button);
            }
        }
      g_free (lp->data);
    }
  g_list_free (emblems);

  /* be sure to update the buttons according to the selected file */
  if (G_LIKELY (chooser->file != NULL))
    thunar_emblem_chooser_file_changed (chooser->file, chooser);
}



static GtkWidget*
thunar_emblem_chooser_create_button (ThunarEmblemChooser *chooser,
                                     const gchar         *emblem)
{
  GtkIconInfo *info;
  const gchar *name;
  GtkWidget   *button = NULL;
  GtkWidget   *image;
  GtkWidget   *label;
  GtkWidget   *vbox;
  GdkPixbuf   *icon;

  /* lookup the icon info for the emblem */
  info = gtk_icon_theme_lookup_icon (chooser->icon_theme, emblem, 48, 0);
  if (G_UNLIKELY (info == NULL))
    return NULL;

  /* try to load the icon */
  icon = gtk_icon_info_load_icon (info, NULL);
  if (G_UNLIKELY (icon == NULL))
    goto done;

  /* determine the display name for the emblem */
  name = gtk_icon_info_get_display_name (info);
  if (G_UNLIKELY (name == NULL))
    name = (strncmp (emblem, "emblem-", 7) == 0) ? emblem + 7 : emblem;

  /* allocate the button */
  button = gtk_check_button_new ();
  GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
  g_object_set_data_full (G_OBJECT (button), I_("thunar-emblem"), g_strdup (emblem), g_free);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (thunar_emblem_chooser_button_toggled), chooser);

  /* allocate the box */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_size_group_add_widget (chooser->size_group, vbox);
  gtk_container_add (GTK_CONTAINER (button), vbox);
  gtk_widget_show (vbox);

  /* allocate the image */
  image = gtk_image_new_from_pixbuf (icon);
  gtk_box_pack_start (GTK_BOX (vbox), image, TRUE, TRUE, 0);
  gtk_widget_show (image);

  /* allocate the label */
  label = gtk_label_new (name);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_object_unref (G_OBJECT (icon));
done:
  gtk_icon_info_free (info);
  return button;
}



/**
 * thunar_emblem_chooser_new:
 *
 * Allocates a new #ThunarEmblemChooser.
 *
 * Return value: the newly allocated #ThunarEmblemChooser.
 **/
GtkWidget*
thunar_emblem_chooser_new (void)
{
  return g_object_new (THUNAR_TYPE_EMBLEM_CHOOSER, NULL);
}



/**
 * thunar_emblem_chooser_get_file:
 * @chooser : a #ThunarEmblemChooser.
 *
 * Returns the #ThunarFile associated with
 * the @chooser or %NULL.
 *
 * Return value: the #ThunarFile associated
 *               with @chooser.
 **/
ThunarFile*
thunar_emblem_chooser_get_file (const ThunarEmblemChooser *chooser)
{
  _thunar_return_val_if_fail (THUNAR_IS_EMBLEM_CHOOSER (chooser), NULL);
  return chooser->file;
}



/**
 * thunar_emblem_chooser_set_file:
 * @chooser : a #ThunarEmblemChooser.
 * @file    : a #ThunarFile or %NULL.
 *
 * Associates @chooser with @file.
 **/
void
thunar_emblem_chooser_set_file (ThunarEmblemChooser *chooser,
                                ThunarFile          *file)
{
  _thunar_return_if_fail (THUNAR_IS_EMBLEM_CHOOSER (chooser));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  if (G_LIKELY (chooser->file != file))
    {
      /* disconnect from the previous file (if any) */
      if (G_LIKELY (chooser->file != NULL))
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (chooser->file), thunar_emblem_chooser_file_changed, chooser);
          g_object_unref (G_OBJECT (chooser->file));
        }

      /* activate the new file */
      chooser->file = file;

      /* connect to the new file (if any) */
      if (G_LIKELY (file != NULL))
        {
          g_object_ref (G_OBJECT (file));
          thunar_emblem_chooser_file_changed (file, chooser);
          g_signal_connect (G_OBJECT (file), "changed", G_CALLBACK (thunar_emblem_chooser_file_changed), chooser);
        }
    }
}


