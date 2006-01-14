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
 *
 * The basic idea for the icon factory implementation was borrowed from
 * Nautilus initially, but the implementation is very different from
 * what Nautilus does.
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

#include <thunar/thunar-gdk-pixbuf-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-thumbnail-frame.h>
#include <thunar/thunar-thumbnail-generator.h>



/* the timeout until the sweeper is run (in ms) */
#define THUNAR_ICON_FACTORY_SWEEP_TIMEOUT (30 * 1000)

/* the maximum length of the recently used list */
#define MAX_RECENTLY (128u)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ICON_THEME,
  PROP_SHOW_THUMBNAILS,
};



typedef struct _ThunarIconKey ThunarIconKey;



static void       thunar_icon_factory_class_init            (ThunarIconFactoryClass *klass);
static void       thunar_icon_factory_init                  (ThunarIconFactory      *factory);
static void       thunar_icon_factory_dispose               (GObject                *object);
static void       thunar_icon_factory_finalize              (GObject                *object);
static void       thunar_icon_factory_get_property          (GObject                *object,
                                                             guint                   prop_id,
                                                             GValue                 *value,
                                                             GParamSpec             *pspec);
static void       thunar_icon_factory_set_property          (GObject                *object,
                                                             guint                   prop_id,
                                                             const GValue           *value,
                                                             GParamSpec             *pspec);
static gboolean   thunar_icon_factory_changed               (GSignalInvocationHint  *ihint,
                                                             guint                   n_param_values,
                                                             const GValue           *param_values,
                                                             gpointer                user_data);
static gboolean   thunar_icon_factory_sweep_timer           (gpointer                user_data);
static void       thunar_icon_factory_sweep_timer_destroy   (gpointer                user_data);
static GdkPixbuf *thunar_icon_factory_load_from_file        (ThunarIconFactory      *factory,
                                                             const gchar            *path,
                                                             gint                    size);
static GdkPixbuf *thunar_icon_factory_lookup_icon           (ThunarIconFactory      *factory,
                                                             const gchar            *name,
                                                             gint                    size,
                                                             gboolean                wants_default);
static void       thunar_icon_factory_mark_recently_used    (ThunarIconFactory      *factory,
                                                             GdkPixbuf              *pixbuf);
static guint      thunar_icon_key_hash                      (gconstpointer           data);
static gboolean   thunar_icon_key_equal                     (gconstpointer           a,
                                                             gconstpointer           b);
static GdkPixbuf *thunar_icon_factory_load_fallback         (gint                    size);



struct _ThunarIconFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarIconFactory
{
  GObject __parent__;

  ThunarThumbnailGenerator *thumbnail_generator;
  ThunarVfsThumbFactory    *thumbnail_factory;

  GdkPixbuf                *recently[MAX_RECENTLY];  /* ring buffer */
  guint                     recently_pos;            /* insert position */

  GHashTable               *icon_cache;

  GtkIconTheme             *icon_theme;

  gboolean                  show_thumbnails;

  gint                      changed_idle_id;
  gint                      sweep_timer_id;

  gulong                    changed_hook_id;
};

struct _ThunarIconKey
{
  gchar *name;
  gint   size;
};



static GObjectClass *thunar_icon_factory_parent_class = NULL;
static GQuark        thunar_icon_factory_quark = 0;
static GQuark        thunar_file_thumb_path_quark = 0;



GType
thunar_icon_factory_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarIconFactoryClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_icon_factory_class_init,
        NULL,
        NULL,
        sizeof (ThunarIconFactory),
        0,
        (GInstanceInitFunc) thunar_icon_factory_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarIconFactory"), &info, 0);
    }

  return type;
}



static void
thunar_icon_factory_class_init (ThunarIconFactoryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_icon_factory_parent_class = g_type_class_peek_parent (klass);

  /* setup the thunar-file-thumb-path quark */
  thunar_file_thumb_path_quark = g_quark_from_static_string ("thunar-file-thumb-path");

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_icon_factory_dispose;
  gobject_class->finalize = thunar_icon_factory_finalize;
  gobject_class->get_property = thunar_icon_factory_get_property;
  gobject_class->set_property = thunar_icon_factory_set_property;

  /**
   * ThunarIconFactory:icon-theme:
   *
   * The #GtkIconTheme on which the given #ThunarIconFactory instance operates
   * on.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_THEME,
                                   g_param_spec_object ("icon-theme",
                                                        _("Icon theme"),
                                                        _("The icon theme used by the icon factory"),
                                                        GTK_TYPE_ICON_THEME,
                                                        EXO_PARAM_READABLE));

  /**
   * ThunarIconFactory:show-thumbnails:
   *
   * Whether this #ThunarIconFactory will try to generate and load thumbnails
   * when loading icons for #ThunarFile<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_THUMBNAILS,
                                   g_param_spec_boolean ("show-thumbnails",
                                                         _("Show Thumbnails"),
                                                         _("Show Thumbnails"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_icon_factory_init (ThunarIconFactory *factory)
{
  /* initialize GSource ids */
  factory->changed_idle_id = -1;
  factory->sweep_timer_id = -1;

  /* connect emission hook for the "changed" signal on the GtkIconTheme class. We use the emission
   * hook way here, because that way we can make sure that the icon cache is definetly cleared
   * before any other part of the application gets notified about the icon theme change.
   */
  factory->changed_hook_id = g_signal_add_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME),
                                                         0, thunar_icon_factory_changed, factory, NULL);

  /* allocate the hash table for the icon cache */
  factory->icon_cache = g_hash_table_new_full (thunar_icon_key_hash, thunar_icon_key_equal, g_free, g_object_unref);
}



static void
thunar_icon_factory_dispose (GObject *object)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  g_return_if_fail (THUNAR_IS_ICON_FACTORY (factory));

  if (G_UNLIKELY (factory->changed_idle_id >= 0))
    g_source_remove (factory->changed_idle_id);

  if (G_UNLIKELY (factory->sweep_timer_id >= 0))
    g_source_remove (factory->sweep_timer_id);

  (*G_OBJECT_CLASS (thunar_icon_factory_parent_class)->dispose) (object);
}



static void
thunar_icon_factory_finalize (GObject *object)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);
  guint              n;

  g_return_if_fail (THUNAR_IS_ICON_FACTORY (factory));

  /* clear the recently used list */
  for (n = 0; n < MAX_RECENTLY; ++n)
    if (G_LIKELY (factory->recently[n] != NULL))
      g_object_unref (G_OBJECT (factory->recently[n]));

  /* clear the icon cache hash table */
  g_hash_table_destroy (factory->icon_cache);

  /* disconnect from the thumbnail factory */
  if (G_LIKELY (factory->thumbnail_factory != NULL))
    g_object_unref (G_OBJECT (factory->thumbnail_factory));

  /* disconnect from the thumbnail generator */
  if (G_LIKELY (factory->thumbnail_generator != NULL))
    g_object_unref (G_OBJECT (factory->thumbnail_generator));

  /* remove the "changed" emission hook from the GtkIconTheme class */
  g_signal_remove_emission_hook (g_signal_lookup ("changed", GTK_TYPE_ICON_THEME), factory->changed_hook_id);

  /* disconnect from the associated icon theme (if any) */
  if (G_LIKELY (factory->icon_theme != NULL))
    {
      g_object_set_qdata (G_OBJECT (factory->icon_theme), thunar_icon_factory_quark, NULL);
      g_object_unref (G_OBJECT (factory->icon_theme));
    }

  (*G_OBJECT_CLASS (thunar_icon_factory_parent_class)->finalize) (object);
}



static void
thunar_icon_factory_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  switch (prop_id)
    {
    case PROP_ICON_THEME:
      g_value_set_object (value, factory->icon_theme);
      break;

    case PROP_SHOW_THUMBNAILS:
      g_value_set_boolean (value, factory->show_thumbnails);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_factory_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (object);

  switch (prop_id)
    {
    case PROP_SHOW_THUMBNAILS:
      factory->show_thumbnails = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_icon_factory_changed (GSignalInvocationHint *ihint,
                             guint                  n_param_values,
                             const GValue          *param_values,
                             gpointer               user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);
  guint              n;

  /* drop all items from the recently used list */
  for (n = 0; n < MAX_RECENTLY; ++n)
    if (G_LIKELY (factory->recently[n] != NULL))
      {
        g_object_unref (G_OBJECT (factory->recently[n]));
        factory->recently[n] = NULL;
      }

  /* drop all items from the icon cache */
  g_hash_table_foreach_remove (factory->icon_cache, (GHRFunc) exo_noop_true, NULL);

  /* keep the emission hook alive */
  return TRUE;
}



static gboolean
thunar_icon_check_sweep (ThunarIconKey *key,
                         GdkPixbuf     *pixbuf)
{
  return (G_OBJECT (pixbuf)->ref_count == 1);
}



static gboolean
thunar_icon_factory_sweep_timer (gpointer user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), FALSE);

  GDK_THREADS_ENTER ();

  /* ditch all icons whose ref_count is 1 */
  g_hash_table_foreach_remove (factory->icon_cache, (GHRFunc) thunar_icon_check_sweep, factory);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_icon_factory_sweep_timer_destroy (gpointer user_data)
{
  THUNAR_ICON_FACTORY (user_data)->sweep_timer_id = -1;
}



static inline gboolean
thumbnail_needs_frame (const GdkPixbuf *thumbnail,
                       gint             width,
                       gint             height)
{
  const guchar *pixels;
  gint          rowstride;
  gint          n;

  /* don't add frames to small thumbnails */
  if (width < THUNAR_THUMBNAIL_SIZE && height < THUNAR_THUMBNAIL_SIZE)
    return FALSE;

  /* always add a frame to thumbnails w/o alpha channel */
  if (G_LIKELY (!gdk_pixbuf_get_has_alpha (thumbnail)))
    return TRUE;

  /* get a pointer to the thumbnail data */
  pixels = gdk_pixbuf_get_pixels (thumbnail);

  /* check if we have a transparent pixel on the first row */
  for (n = width * 4; n > 0; n -= 4)
    if (pixels[n - 1] < 255u)
      return FALSE;

  /* determine the rowstride */
  rowstride = gdk_pixbuf_get_rowstride (thumbnail);

  /* skip the first row */
  pixels += rowstride;

  /* check if we have a transparent pixel in the first or last column */
  for (n = height - 2; n > 0; --n, pixels += rowstride)
    if (pixels[3] < 255u || pixels[width * 4 - 1] < 255u)
      return FALSE;

  /* check if we have a transparent pixel on the last row */
  for (n = width * 4; n > 0; n -= 4)
    if (pixels[n - 1] < 255u)
      return FALSE;

  return TRUE;
}



static GdkPixbuf*
thunar_icon_factory_load_from_file (ThunarIconFactory *factory,
                                    const gchar       *path,
                                    gint               size)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *frame;
  GdkPixbuf *tmp;
  gboolean   needs_frame;
  gint       max_width;
  gint       max_height;
  gint       width;
  gint       height;

  /* try to load the image from the file */
  pixbuf = gdk_pixbuf_new_from_file (path, NULL);
  if (G_LIKELY (pixbuf != NULL))
    {
      /* determine the dimensions of the pixbuf */
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);

      /* check if we want to add a frame to the image (we really don't
       * want to do this for icons displayed in the details view).
       */
      needs_frame = (strstr (path, G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S) != NULL)
                 && (size >= 36) && thumbnail_needs_frame (pixbuf, width, height);

      /* be sure to make framed thumbnails fit into the size */
      if (G_LIKELY (needs_frame))
        {
          max_width = size - (3 + 6);
          max_height = size - (3 + 6);
        }
      else
        {
          max_width = size;
          max_height = size;
        }

      /* scale down the icon (if required) */
      if (G_LIKELY (width > max_width || height > max_height))
        {
          /* scale down to the required size */
          tmp = exo_gdk_pixbuf_scale_down (pixbuf, TRUE, max_height, max_height);
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = tmp;
        }

      /* add a frame around thumbnail (large) images */
      if (G_LIKELY (needs_frame))
        {
          /* add a frame to the thumbnail */
          frame = gdk_pixbuf_new_from_inline (-1, thunar_thumbnail_frame, FALSE, NULL);
          tmp = thunar_gdk_pixbuf_frame (pixbuf, frame, 3, 3, 6, 6);
          g_object_unref (G_OBJECT (pixbuf));
          g_object_unref (G_OBJECT (frame));
          pixbuf = tmp;
        }
    }

  return pixbuf;
}



static GdkPixbuf*
thunar_icon_factory_lookup_icon (ThunarIconFactory *factory,
                                 const gchar       *name,
                                 gint               size,
                                 gboolean           wants_default)
{
  ThunarIconKey  lookup_key;
  ThunarIconKey *key;
  GtkIconInfo   *icon_info;
  GdkPixbuf     *pixbuf = NULL;

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);
  g_return_val_if_fail (size > 0, NULL);

  /* prepare the lookup key */
  lookup_key.name = (gchar *) name;
  lookup_key.size = size;

  /* check if we already have a cached version of the icon */
  if (!g_hash_table_lookup_extended (factory->icon_cache, &lookup_key, NULL, (gpointer) &pixbuf))
    {
      /* check if we have to load a file instead of a themed icon */
      if (G_UNLIKELY (g_path_is_absolute (name)))
        {
          pixbuf = thunar_icon_factory_load_from_file (factory, name, size);
        }
      else
        {
          /* check if the icon theme contains an icon of that name */
          icon_info = gtk_icon_theme_lookup_icon (factory->icon_theme, name, size, 0);
          if (G_LIKELY (icon_info != NULL))
            {
              /* try to load the icon returned from the icon theme */
              pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
              gtk_icon_info_free (icon_info);
            }
        }

      /* use fallback icon if no pixbuf could be loaded */
      if (G_UNLIKELY (pixbuf == NULL))
        {
          /* check if we are allowed to return the fallback icon */
          if (!wants_default)
            return NULL;
          else
            return thunar_icon_factory_load_fallback (size);
        }

      /* generate a key for the new cached icon */
      key = g_malloc (sizeof (ThunarIconKey) + strlen (name) + 1);
      key->name = ((gchar *) key) + sizeof (ThunarIconKey);
      key->size = size;
      strcpy (key->name, name);

      /* insert the new icon into the cache */
      g_hash_table_insert (factory->icon_cache, key, pixbuf);
    }

  /* add the icon to the recently used list */
  thunar_icon_factory_mark_recently_used (factory, pixbuf);

  return g_object_ref (G_OBJECT (pixbuf));
}



static void
thunar_icon_factory_mark_recently_used (ThunarIconFactory *factory,
                                        GdkPixbuf         *pixbuf)
{
  guint n;

  g_return_if_fail (THUNAR_IS_ICON_FACTORY (factory));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  /* check if the icon is already on the list */
  for (n = 0; n < MAX_RECENTLY; ++n)
    if (G_UNLIKELY (factory->recently[n] == pixbuf))
      return;

  /* ditch the previous item on the current insert position,
   * which - if present - is the oldest item in the list.
   */
  if (G_LIKELY (factory->recently[factory->recently_pos] != NULL))
    g_object_unref (G_OBJECT (factory->recently[factory->recently_pos]));

  /* insert the new pixbuf into the list */
  factory->recently[factory->recently_pos] = pixbuf;
  g_object_ref (G_OBJECT (pixbuf));

  /* advance the insert position */
  factory->recently_pos = (factory->recently_pos + 1) % MAX_RECENTLY;

  /* schedule the sweeper */
  if (G_UNLIKELY (factory->sweep_timer_id < 0))
    {
      factory->sweep_timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_ICON_FACTORY_SWEEP_TIMEOUT,
                                                    thunar_icon_factory_sweep_timer, factory,
                                                    thunar_icon_factory_sweep_timer_destroy);
    }
}



static guint
thunar_icon_key_hash (gconstpointer data)
{
  const ThunarIconKey *key = data;
  const gchar         *p;
  guint                h;

  h = (guint) key->size << 5;

  for (p = key->name; *p != '\0'; ++p)
    h = (h << 5) - h + *p;

  return h;
}



static gboolean
thunar_icon_key_equal (gconstpointer a,
                       gconstpointer b)
{
  const ThunarIconKey *a_key = a;
  const ThunarIconKey *b_key = b;

  /* compare sizes first */
  if (a_key->size != b_key->size)
    return FALSE;

  /* do a full string comparison on the names */
  return (strcmp (a_key->name, b_key->name) == 0);
}



static GdkPixbuf*
thunar_icon_factory_load_fallback (gint size)
{
  static const gchar THUNAR_FALLBACK_ICON_PATH[] = DATADIR "/pixmaps/Thunar/Thunar-fallback-icon.png";
  GdkPixbuf         *pixbuf;
  GError            *error = NULL;

  /* try to load the fallback icon */
  pixbuf = gdk_pixbuf_new_from_file_at_scale (THUNAR_FALLBACK_ICON_PATH, size, size, TRUE, &error);

  /* verify that it was loaded */
  if (G_UNLIKELY (pixbuf == NULL))
    {
      g_error (_("Failed to load fallback icon from `%s' (%s). Check your installation!"), THUNAR_FALLBACK_ICON_PATH, error->message);
      g_error_free (error);
    }

  return pixbuf;
}



/**
 * thunar_icon_factory_get_default:
 *
 * Returns the #ThunarIconFactory that operates on the default #GtkIconTheme.
 * The default #ThunarIconFactory instance will be around for the time the
 * programs runs, starting with the first call to this function.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the #ThunarIconFactory for the default icon theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_default (void)
{
  static ThunarIconFactory *factory = NULL;

  if (G_UNLIKELY (factory == NULL))
    {
      factory = thunar_icon_factory_get_for_icon_theme (gtk_icon_theme_get_default ());
      g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
    }
  else
    {
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunar_icon_factory_get_for_icon_theme:
 * @icon_theme : a #GtkIconTheme instance.
 *
 * Determines the proper #ThunarIconFactory to be used with the specified
 * @icon_theme and returns it.
 *
 * You need to explicitly free the returned #ThunarIconFactory object
 * using g_object_unref() when you are done with it.
 *
 * Return value: the #ThunarIconFactory for @icon_theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_for_icon_theme (GtkIconTheme *icon_theme)
{
  ThunarPreferences *preferences;
  ThunarIconFactory *factory;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* generate the quark on-demand */
  if (G_UNLIKELY (thunar_icon_factory_quark == 0))
    thunar_icon_factory_quark = g_quark_from_static_string ("thunar-icon-factory");

  /* check if the given icon theme already knows about an icon factory */
  factory = g_object_get_qdata (G_OBJECT (icon_theme), thunar_icon_factory_quark);
  if (G_UNLIKELY (factory == NULL))
    {
      /* allocate a new factory and connect it to the icon theme */
      factory = g_object_new (THUNAR_TYPE_ICON_FACTORY, NULL);
      factory->icon_theme = g_object_ref (G_OBJECT (icon_theme));
      g_object_set_qdata (G_OBJECT (factory->icon_theme), thunar_icon_factory_quark, factory);

      /* connect the "show-thumbnails" property to the global preference */
      preferences = thunar_preferences_get ();
      g_object_set_data_full (G_OBJECT (factory), I_("thunar-preferences"), preferences, g_object_unref);
      exo_binding_new (G_OBJECT (preferences), "misc-show-thumbnails", G_OBJECT (factory), "show-thumbnails");
    }
  else
    {
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunar_icon_factory_get_icon_theme:
 * @factory : a #ThunarIconFactory instance.
 *
 * Returns the #GtkIconTheme associated with @factory.
 *
 * Return value: the #GtkIconTheme associated with @factory.
 **/
GtkIconTheme*
thunar_icon_factory_get_icon_theme (const ThunarIconFactory *factory)
{
  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  return factory->icon_theme;
}



/**
 * thunar_icon_factory_load_icon:
 * @factory       : a #ThunarIconFactory instance.
 * @name          : name of the icon to load.
 * @size          : desired icon size.
 * @attach_points : location to store the attach points to or %NULL.
 * @wants_default : %TRUE to return the fallback icon if no icon of @name
 *                  is found in the @factory.
 *
 * Looks up the icon named @name in the icon theme associated with @factory
 * and returns a pixbuf for the icon at the given @size. This function will
 * return a default fallback icon if the requested icon could not be found
 * and @wants_default is %TRUE, else %NULL will be returned in that case.
 *
 * Call g_object_unref() on the returned pixbuf when you are
 * done with it.
 *
 * Return value: the pixbuf for the icon named @name at @size.
 **/
GdkPixbuf*
thunar_icon_factory_load_icon (ThunarIconFactory        *factory,
                               const gchar              *name,
                               gint                      size,
                               ThunarEmblemAttachPoints *attach_points,
                               gboolean                  wants_default)
{
  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (size > 0, NULL);
  
  /* cannot happen unless there's no XSETTINGS manager
   * for the default screen, but just in case...
   */
  if (G_UNLIKELY (name == NULL || *name == '\0'))
    {
      /* check if the caller will happly accept the fallback icon */
      if (G_LIKELY (wants_default))
        return thunar_icon_factory_load_fallback (size);
      else
        return NULL;
    }

  /* lookup the icon */
  return thunar_icon_factory_lookup_icon (factory, name, size, wants_default);
}



/**
 * thunar_icon_factory_load_file_icon:
 * @factory    : a #ThunarIconFactory instance.
 * @file       : a #ThunarFile.
 * @icon_state : the desired icon state.
 * @icon_size  : the desired icon size.
 *
 * The caller is responsible to free the returned object using
 * g_object_unref() when no longer needed.
 *
 * Return value: the #GdkPixbuf icon.
 **/
GdkPixbuf*
thunar_icon_factory_load_file_icon (ThunarIconFactory  *factory,
                                    ThunarFile         *file,
                                    ThunarFileIconState icon_state,
                                    gint                icon_size)
{
  ThunarFileThumbState thumb_state;
  ThunarVfsInfo       *info;
  const gchar         *icon_name;
  GdkPixbuf           *icon;
  gchar               *thumb_path;

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  g_return_val_if_fail (icon_size > 0, NULL);

  /* check if there's a custom icon for the file */
  icon_name = thunar_file_get_custom_icon (file);
  if (G_UNLIKELY (icon_name != NULL))
    {
      /* try to load the icon */
      icon = thunar_icon_factory_lookup_icon (factory, icon_name, icon_size, FALSE);
      if (G_LIKELY (icon != NULL))
        return icon;
    }

  /* check if thumbnails are enabled and we can display a thumbnail for the item */
  if (G_LIKELY (factory->show_thumbnails && thunar_file_is_regular (file)))
    {
      /* determine the thumbnail state */
      thumb_state = thunar_file_get_thumb_state (file);

      /* check if we haven't yet determine the thumbnail state */
      if (thumb_state == THUNAR_FILE_THUMB_STATE_UNKNOWN)
        {
          /* determine the ThunarVfsInfo for the file */
          info = thunar_file_get_info (file);

          /* allocate the thumbnail factory and thumbnail generator on-demand */
          if (G_UNLIKELY (factory->thumbnail_factory == NULL))
            {
              /* allocate the thumbnail factory */
              factory->thumbnail_factory = thunar_vfs_thumb_factory_new ((THUNAR_THUMBNAIL_SIZE > 128)
                                                                        ? THUNAR_VFS_THUMB_SIZE_LARGE
                                                                        : THUNAR_VFS_THUMB_SIZE_NORMAL);

              /* allocate the thumbnail generator */
              factory->thumbnail_generator = thunar_thumbnail_generator_new (factory->thumbnail_factory);
            }

          /* try to load an existing thumbnail for the file */
          thumb_path = thunar_vfs_thumb_factory_lookup_thumbnail (factory->thumbnail_factory, info);

          /* check if we can generate a thumbnail in case there's none yet */
          if (G_UNLIKELY (thumb_path == NULL && thunar_vfs_thumb_factory_can_thumbnail (factory->thumbnail_factory, info)))
            {
              /* schedule the thumbnail loading for the file */
              thunar_thumbnail_generator_enqueue (factory->thumbnail_generator, file);

              /* set the thumbnail state to "loading" */
              thumb_state = THUNAR_FILE_THUMB_STATE_LOADING;
            }

          if (G_LIKELY (thumb_path != NULL))
            {
              thumb_state = THUNAR_FILE_THUMB_STATE_READY;
              g_object_set_qdata_full (G_OBJECT (file), thunar_file_thumb_path_quark, thumb_path, g_free);
            }
          else if (thumb_state != THUNAR_FILE_THUMB_STATE_LOADING)
            {
              thumb_state = THUNAR_FILE_THUMB_STATE_NONE;
            }

          /* apply the new state */
          thunar_file_set_thumb_state (file, thumb_state);
        }

      /* check if we have a thumbnail path loaded */
      if (thumb_state == THUNAR_FILE_THUMB_STATE_READY)
        {
          thumb_path = g_object_get_qdata (G_OBJECT (file), thunar_file_thumb_path_quark);
          if (G_LIKELY (thumb_path != NULL))
            {
              // FIXME: Check mtime and URI for the returned icon
              icon = thunar_icon_factory_lookup_icon (factory, thumb_path, icon_size, FALSE);
              if (G_LIKELY (icon != NULL))
                return icon;
            }
        }

      /* check if we are currently loading a thumbnail */
      if (G_UNLIKELY (thumb_state == THUNAR_FILE_THUMB_STATE_LOADING))
        {
          /* check if the icon theme supports the loading icon */
          icon = thunar_icon_factory_lookup_icon (factory, "gnome-fs-loading-icon", icon_size, FALSE);
          if (G_LIKELY (icon != NULL))
            return icon;
        }
    }

  /* lookup the icon name for the icon in the given state */
  icon_name = thunar_file_get_icon_name (file, icon_state, factory->icon_theme);

  /* load the icon of the given name */
  return thunar_icon_factory_load_icon (factory, icon_name, icon_size, NULL, TRUE);
}




