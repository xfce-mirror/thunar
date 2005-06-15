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

#include <exo/exo.h>

#include <thunar/thunar-fallback-icon.h>
#include <thunar/thunar-icon-factory.h>



/* the timeout until the sweeper is run (in ms) */
#define THUNAR_ICON_FACTORY_SWEEP_TIMEOUT (10 * 1000)

/* the maximum length of the recently used list */
#define MAX_RECENTLY (128u)

enum
{
  PROP_0,
  PROP_ICON_THEME,
};

enum
{
  CHANGED,
  LAST_SIGNAL,
};



typedef struct _ThunarIconKey   ThunarIconKey;
typedef struct _ThunarIconValue ThunarIconValue;



static void             thunar_icon_factory_class_init            (ThunarIconFactoryClass *klass);
static void             thunar_icon_factory_init                  (ThunarIconFactory      *factory);
static void             thunar_icon_factory_dispose               (GObject                *object);
static void             thunar_icon_factory_finalize              (GObject                *object);
static void             thunar_icon_factory_get_property          (GObject                *object,
                                                                   guint                   prop_id,
                                                                   GValue                 *value,
                                                                   GParamSpec             *pspec);
static void             thunar_icon_factory_set_property          (GObject                *object,
                                                                   guint                   prop_id,
                                                                   const GValue           *value,
                                                                   GParamSpec             *pspec);
static void             thunar_icon_factory_changed               (GtkIconTheme           *icon_theme,
                                                                   ThunarIconFactory      *factory);
static gboolean         thunar_icon_factory_changed_idle          (gpointer                user_data);
static void             thunar_icon_factory_changed_idle_destroy  (gpointer                user_data);
static gboolean         thunar_icon_factory_sweep_timer           (gpointer                user_data);
static void             thunar_icon_factory_sweep_timer_destroy   (gpointer                user_data);
static ThunarIconValue *thunar_icon_factory_lookup_icon           (ThunarIconFactory      *factory,
                                                                   const gchar            *name,
                                                                   gint                    size,
                                                                   gboolean                wants_default);
static void             thunar_icon_factory_mark_recently_used    (ThunarIconFactory      *factory,
                                                                   GdkPixbuf              *pixbuf);
static guint            thunar_icon_key_hash                      (gconstpointer           data);
static gboolean         thunar_icon_key_equal                     (gconstpointer           a,
                                                                   gconstpointer           b);
static void             thunar_icon_value_free                    (gpointer                data);



struct _ThunarIconFactoryClass
{
  GObjectClass __parent__;

  void (*changed) (ThunarIconFactory *factory);
};

struct _ThunarIconFactory
{
  GObject __parent__;

  GdkPixbuf       *recently[MAX_RECENTLY];  /* ring buffer */
  guint            recently_pos;            /* insert position */

  GHashTable      *icon_cache;

  GtkIconTheme    *icon_theme;
  ThunarIconValue *fallback_icon;

  gint             changed_idle_id;
  gint             sweep_timer_id;
};

struct _ThunarIconKey
{
  gchar *name;
  gint   size;
};

struct _ThunarIconValue
{
  GdkPixbuf *pixbuf;
};




static guint      factory_signals[LAST_SIGNAL];
static GQuark     thunar_icon_factory_quark = 0;
static GMemChunk *values_chunk;



G_DEFINE_TYPE (ThunarIconFactory, thunar_icon_factory, G_TYPE_OBJECT);



static void
thunar_icon_factory_class_init (ThunarIconFactoryClass *klass)
{
  GObjectClass *gobject_class;

  /* generate the memory chunk used for the ThunarIconValue's */
  values_chunk = g_mem_chunk_new ("ThunarIconValue chunk",
                                  sizeof (ThunarIconValue),
                                  sizeof (ThunarIconValue) * MAX_RECENTLY,
                                  G_ALLOC_AND_FREE);

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
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  /**
   * ThunarIconFactory::changed:
   * @factory : a #ThunarIconFactory.
   *
   * Emitted whenever the set of available icons changes, for example, whenever
   * the user selects a different icon theme from the Xfce settings manager.
   **/
  factory_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarIconFactoryClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_icon_factory_init (ThunarIconFactory *factory)
{
  factory->changed_idle_id = -1;
  factory->sweep_timer_id = -1;

  /* load the fallback icon */
  factory->fallback_icon = g_chunk_new (ThunarIconValue, values_chunk);
  factory->fallback_icon->pixbuf = gdk_pixbuf_new_from_inline (-1, thunar_fallback_icon, FALSE, NULL);

  /* allocate the hash table for the icon cache */
  factory->icon_cache = g_hash_table_new_full (thunar_icon_key_hash, thunar_icon_key_equal,
                                               g_free, thunar_icon_value_free);
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

  G_OBJECT_CLASS (thunar_icon_factory_parent_class)->dispose (object);
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

  /* drop the fallback icon reference */
  thunar_icon_value_free (factory->fallback_icon);

  /* disconnect from the associated icon theme (if any) */
  if (G_LIKELY (factory->icon_theme != NULL))
    {
      g_object_set_qdata (G_OBJECT (factory->icon_theme), thunar_icon_factory_quark, NULL);
      g_object_unref (G_OBJECT (factory->icon_theme));
    }

  G_OBJECT_CLASS (thunar_icon_factory_parent_class)->finalize (object);
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

  g_return_if_fail (factory->icon_theme == NULL);
  g_return_if_fail (thunar_icon_factory_quark != 0);

  switch (prop_id)
    {
    case PROP_ICON_THEME:
      /* connect to the icon-theme */
      factory->icon_theme = g_value_get_object (value);
      g_object_ref (G_OBJECT (factory->icon_theme));
      g_object_set_qdata (G_OBJECT (factory->icon_theme),
                          thunar_icon_factory_quark,
                          factory);
      g_signal_connect (G_OBJECT (factory->icon_theme), "changed",
                        G_CALLBACK (thunar_icon_factory_changed), factory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_icon_factory_changed (GtkIconTheme      *icon_theme,
                             ThunarIconFactory *factory)
{
  guint n;

  /* drop all items from the recently used list */
  for (n = 0; n < MAX_RECENTLY; ++n)
    if (G_LIKELY (factory->recently[n] != NULL))
      {
        g_object_unref (G_OBJECT (factory->recently[n]));
        factory->recently[n] = NULL;
      }

  /* drop all items from the icon cache */
  g_hash_table_foreach_remove (factory->icon_cache, (GHRFunc)gtk_true, NULL);

  /* Need to invoke the "changed" signal from an idle function
   * so other modules will definetly have noticed the icon theme
   * change as well and cleared their caches
   */
  if (G_LIKELY (factory->changed_idle_id < 0))
    {
      factory->changed_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_icon_factory_changed_idle,
                                                  factory, thunar_icon_factory_changed_idle_destroy);
    }
}



static gboolean
thunar_icon_factory_changed_idle (gpointer user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), FALSE);

  GDK_THREADS_ENTER ();

  /* emit the "changed" signal */
  g_signal_emit (G_OBJECT (factory), factory_signals[CHANGED], 0);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_icon_factory_changed_idle_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_ICON_FACTORY (user_data)->changed_idle_id = -1;
  GDK_THREADS_LEAVE ();
}



static gboolean
thunar_icon_check_sweep (ThunarIconKey   *key,
                         ThunarIconValue *value)
{
  return (G_OBJECT (value->pixbuf)->ref_count == 1);
}



static gboolean
thunar_icon_factory_sweep_timer (gpointer user_data)
{
  ThunarIconFactory *factory = THUNAR_ICON_FACTORY (user_data);

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), FALSE);

  GDK_THREADS_ENTER ();

  /* ditch all icons whose ref_count is 1 */
  g_hash_table_foreach_remove (factory->icon_cache, (GHRFunc)thunar_icon_check_sweep, factory);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_icon_factory_sweep_timer_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_ICON_FACTORY (user_data)->sweep_timer_id = -1;
  GDK_THREADS_LEAVE ();
}



static ThunarIconValue*
thunar_icon_factory_lookup_icon (ThunarIconFactory *factory,
                                 const gchar       *name,
                                 gint               size,
                                 gboolean           wants_default)
{
  ThunarIconValue *value = NULL;
  ThunarIconKey    lookup_key;
  ThunarIconKey   *key;
  GtkIconInfo     *icon_info;
  GdkPixbuf       *pixbuf = NULL;

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);
  g_return_val_if_fail (size > 0, NULL);

  lookup_key.name = (gchar *)name;
  lookup_key.size = size;

  /* check if we already have a cached version of the icon */
  if (!g_hash_table_lookup_extended (factory->icon_cache, &lookup_key, NULL, (gpointer) &value))
    {
      /* check if the icon theme contains an icon of that name */
      icon_info = gtk_icon_theme_lookup_icon (factory->icon_theme, name, size, 0);
      if (G_LIKELY (icon_info != NULL))
        {
          /* try to load the icon returned from the icon theme */
          pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
          gtk_icon_info_free (icon_info);
        }

      /* use fallback icon if no pixbuf could be loaded */
      if (G_UNLIKELY (pixbuf == NULL))
        {
          /* check if we are allowed to return the fallback icon */
          if (!wants_default)
            return NULL;

          /* already have the fallback icon loaded at 48x48 */
          if (G_LIKELY (size == 48))
            return factory->fallback_icon;

          /* scale the fallback icon to the requested size */
          pixbuf = exo_gdk_pixbuf_scale_ratio (factory->fallback_icon->pixbuf, size);
        }

      /* generate a key for the new cached icon */
      key = g_malloc (sizeof (ThunarIconKey) + strlen (name) + 1);
      key->name = ((gchar *)key) + sizeof (ThunarIconKey);
      key->size = size;
      strcpy (key->name, name);

      /* generate the value for the new cached icon */
      value = g_chunk_new (ThunarIconValue, values_chunk);
      value->pixbuf = pixbuf;

      /* insert the new icon into the cache */
      g_hash_table_insert (factory->icon_cache, key, value);
    }

  /* add the icon to the recently used list */
  thunar_icon_factory_mark_recently_used (factory, value->pixbuf);

  return value;
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
  if (factory->sweep_timer_id < 0)
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
  const gchar         *ap;
  const gchar         *bp;

  /* compare sizes first */
  if (a_key->size != b_key->size)
    return FALSE;

  /* do a full string comparison on the names */
  for (ap = a_key->name, bp = b_key->name; *ap == *bp && *ap != '\0'; ++ap, ++bp)
    ;

  return (*ap == '\0' && *bp == '\0');
}



static void
thunar_icon_value_free (gpointer data)
{
  ThunarIconValue *value = (ThunarIconValue *)data;
  g_object_unref (G_OBJECT (value->pixbuf));
  g_chunk_free (value, values_chunk);
}



/**
 * thunar_icon_factory_get_default:
 *
 * Returns the #ThunarIconFactory that operates on the default #GtkIconTheme.
 * The default #ThunarIconFactory instance will be around for the time the
 * programs runs, starting with the first call to this function.
 *
 * Note that this method does not automatically take a reference for the caller
 * on the returned object, so do NOT call #g_object_unref() on it, unless
 * you have previously called #g_object_ref() yourself.
 *
 * Return value: the #ThunarIconFactory for the default icon theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_default (void)
{
  static ThunarIconFactory *factory = NULL;
  if (G_UNLIKELY (factory == NULL))
    factory = thunar_icon_factory_get_for_icon_theme (gtk_icon_theme_get_default ());
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
 * using #g_object_unref() when you are done with it.
 *
 * Return value: the #ThunarIconFactory for @icon_theme.
 **/
ThunarIconFactory*
thunar_icon_factory_get_for_icon_theme (GtkIconTheme *icon_theme)
{
  ThunarIconFactory *factory;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* generate the quark on-demand */
  if (G_UNLIKELY (thunar_icon_factory_quark == 0))
    thunar_icon_factory_quark = g_quark_from_static_string ("thunar-icon-factory");

  /* check if the given icon theme already knows about an icon factory */
  factory = g_object_get_qdata (G_OBJECT (icon_theme), thunar_icon_factory_quark);
  if (G_UNLIKELY (factory == NULL))
    {
      factory = g_object_new (THUNAR_TYPE_ICON_FACTORY,
                              "icon-theme", icon_theme,
                              NULL);
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
thunar_icon_factory_get_icon_theme (ThunarIconFactory *factory)
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
 * Call #g_object_unref() on the returned pixbuf when you are
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
  ThunarIconValue *value;

  g_return_val_if_fail (THUNAR_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);
  g_return_val_if_fail (size > 0, NULL);
  
  /* lookup the icon */
  value = thunar_icon_factory_lookup_icon (factory, name, size, wants_default);
  if (value != NULL)
    {
      g_assert (GDK_IS_PIXBUF (value->pixbuf));

      /* take a reference for the caller */
      g_object_ref (G_OBJECT (value->pixbuf));

      return value->pixbuf;
    }

  g_assert (!wants_default);

  return NULL;
}




