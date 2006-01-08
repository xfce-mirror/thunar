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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo.h>

#include <tdb/tdb.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DEFAULT_FOLDERS_FIRST,
  PROP_DEFAULT_SHOW_HIDDEN,
  PROP_DEFAULT_TEXT_BESIDE_ICONS,
  PROP_DEFAULT_VIEW,
  PROP_LAST_LOCATION_BAR,
  PROP_LAST_SIDE_PANE,
  PROP_LAST_VIEW,
  PROP_MISC_RECURSIVE_PERMISSIONS,
  N_PROPERTIES,
};



static void thunar_preferences_class_init   (ThunarPreferencesClass *klass);
static void thunar_preferences_init         (ThunarPreferences      *preferences);
static void thunar_preferences_finalize     (GObject                *object);
static void thunar_preferences_get_property (GObject                *object,
                                             guint                   prop_id,
                                             GValue                 *value,
                                             GParamSpec             *pspec);
static void thunar_preferences_set_property (GObject                *object,
                                             guint                   prop_id,
                                             const GValue           *value,
                                             GParamSpec             *pspec);



struct _ThunarPreferencesClass
{
  GObjectClass __parent__;
};

struct _ThunarPreferences
{
  GObject __parent__;

  /* the database context */
  TDB_CONTEXT *context;
};



static GObjectClass *thunar_preferences_parent_class;



GType
thunar_preferences_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPreferencesClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_preferences_class_init,
        NULL,
        NULL,
        sizeof (ThunarPreferences),
        0,
        (GInstanceInitFunc) thunar_preferences_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarPreferences"), &info, 0);
    }

  return type;
}



static void
thunar_preferences_class_init (ThunarPreferencesClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_preferences_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_finalize;
  gobject_class->get_property = thunar_preferences_get_property;
  gobject_class->set_property = thunar_preferences_set_property;

  /* register additional transformation functions */
  thunar_g_initialize_transformations ();

  /**
   * ThunarPreferences::default-folders-first:
   *
   * Whether to sort folders before files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_FOLDERS_FIRST,
                                   g_param_spec_boolean ("default-folders-first",
                                                         "default-folders-first",
                                                         "default-folders-first",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::default-show-hidden:
   *
   * Whether to show hidden files by default in new windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_SHOW_HIDDEN,
                                   g_param_spec_boolean ("default-show-hidden",
                                                         "default-show-hidden",
                                                         "default-show-hidden",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::default-text-beside-icons:
   *
   * Whether the icon view should display the file names beside the
   * file icons instead of below the file icons.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_TEXT_BESIDE_ICONS,
                                   g_param_spec_boolean ("default-text-beside-icons",
                                                         "default-text-beside-icons",
                                                         "default-text-beside-icons",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::default-view:
   *
   * The name of the widget class, which should be used for the
   * view pane in new #ThunarWindow<!---->s or "void" to use the
   * last selected view from the "last-view" preference.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_VIEW,
                                   g_param_spec_string ("default-view",
                                                        "default-view",
                                                        "default-view",
                                                        "void",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::last-location-bar:
   *
   * The name of the widget class, which should be used for the
   * location bar in #ThunarWindow<!---->s or "void" to hide the
   * location bar.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_LOCATION_BAR,
                                   g_param_spec_string ("last-location-bar",
                                                        "last-location-bar",
                                                        "last-location-bar",
                                                        "ThunarLocationButtons",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::last-side-pane:
   *
   * The name of the widget class, which should be used for the
   * side pane in #ThunarWindow<!---->s or "void" to hide the
   * side pane completely.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SIDE_PANE,
                                   g_param_spec_string ("last-side-pane",
                                                        "last-side-pane",
                                                        "last-side-pane",
                                                        "ThunarShortcutsPane",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences::last-view:
   *
   * The name of the widget class, which should be used for the
   * main view component in #ThunarWindow<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_VIEW,
                                   g_param_spec_string ("last-view",
                                                        "last-view",
                                                        "last-view",
                                                        "ThunarIconView",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-recursive-permissions:
   *
   * Whether to apply permissions recursively everytime the
   * permissions are altered by the user.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_RECURSIVE_PERMISSIONS,
                                   g_param_spec_enum ("misc-recursive-permissions",
                                                      "misc-recursive-permissions",
                                                      "misc-recursive-permissions",
                                                      THUNAR_TYPE_RECURSIVE_PERMISSIONS,
                                                      THUNAR_RECURSIVE_PERMISSIONS_ASK,
                                                      EXO_PARAM_READWRITE));
}



static void
thunar_preferences_init (ThunarPreferences *preferences)
{
  gchar *path;

  /* determine the path to the preferences database */
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/preferences.tdb", TRUE);
  if (G_UNLIKELY (path == NULL))
    {
      path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/", FALSE);
      g_warning (_("Failed to create the Thunar configuration directory in %s"), path);
      g_free (path);
      return;
    }

  /* try to open the preferences database file */
  preferences->context = tdb_open (path, 0, TDB_DEFAULT, O_CREAT | O_RDWR, 0600);
  if (G_UNLIKELY (preferences->context == NULL))
    g_warning (_("Failed to open preferences database in %s: %s"), path, g_strerror (errno));

  /* release the path */
  g_free (path);
}



static void
thunar_preferences_finalize (GObject *object)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);

  /* close the database (if open) */
  if (G_LIKELY (preferences->context != NULL))
    tdb_close (preferences->context);

  (*G_OBJECT_CLASS (thunar_preferences_parent_class)->finalize) (object);
}



static inline void
value_take_string (GValue  *value,
                   gpointer data)
{
  if (G_LIKELY (g_mem_is_system_malloc ()))
    {
      g_value_take_string (value, data);
    }
  else
    {
      g_value_set_string (value, data);
      free (data);
    }
}



static void
thunar_preferences_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  TDB_DATA           data;
  GValue             tmp = { 0, };

  g_return_if_fail (prop_id > PROP_0 && prop_id < N_PROPERTIES);

  /* check if we have a database handle */
  if (G_LIKELY (preferences->context != NULL))
    {
      /* use the param spec's name as key */
      data.dptr = pspec->name;
      data.dsize = strlen (data.dptr);

      /* lookup the data for the key */
      data = tdb_fetch (preferences->context, data);
      if (data.dptr != NULL && data.dsize > 0 && data.dptr[data.dsize - 1] == '\0')
        {
          /* check if we have a string or can transform */
          if (G_VALUE_TYPE (value) == G_TYPE_STRING)
            {
              value_take_string (value, data.dptr);
              return;
            }
          else if (g_value_type_transformable (G_TYPE_STRING, G_VALUE_TYPE (value)))
            {
              /* try to transform the string */
              g_value_init (&tmp, G_TYPE_STRING);
              value_take_string (&tmp, data.dptr);
              if (g_value_transform (&tmp, value))
                {
                  g_value_unset (&tmp);
                  return;
                }
              g_value_unset (&tmp);

              /* data.dptr was freed by the g_value_unset() call */
              data.dptr = NULL;
            }
        }

      /* release the data (if any) */
      if (data.dptr != NULL)
        free (data.dptr);
    }

  /* set the default value */
  g_param_value_set_default (pspec, value);
}



static void
thunar_preferences_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  TDB_DATA           data;
  TDB_DATA           key;
  GValue             dst = { 0, };

  g_return_if_fail (prop_id > PROP_0 && prop_id < N_PROPERTIES);

  /* save the new value if we have a database handle */
  if (G_LIKELY (preferences->context != NULL))
    {
      /* generate the key for the operation */
      key.dptr = pspec->name;
      key.dsize = strlen (key.dptr);

      /* check if we have the default value here */
      if (g_param_value_defaults (pspec, (GValue *) value))
        {
          /* just delete as we don't store defaults */
          tdb_delete (preferences->context, key);
        }
      else
        {
          /* transform the value to a string */
          g_value_init (&dst, G_TYPE_STRING);
          if (!g_value_transform (value, &dst))
            g_warning ("Unable to transform %s to %s", g_type_name (G_VALUE_TYPE (value)), g_type_name (G_VALUE_TYPE (&dst)));

          /* setup the data value (we also store the '\0' byte) */
          data.dptr = (gchar *) g_value_get_string (&dst);
          data.dsize = strlen (data.dptr) + 1;

          /* perform the store operation */
          tdb_store (preferences->context, key, data, TDB_REPLACE);

          /* release the string */
          g_value_unset (&dst);
        }
    }
}



/**
 * thunar_preferences_get:
 *
 * Queries the global #ThunarPreferences instance, which is shared
 * by all modules. The function automatically takes a reference
 * for the caller, so you'll need to call g_object_unref() when
 * you're done with it.
 *
 * Return value: the global #ThunarPreferences instance.
 **/
ThunarPreferences*
thunar_preferences_get (void)
{
  static ThunarPreferences *preferences = NULL;

  if (G_UNLIKELY (preferences == NULL))
    {
      preferences = g_object_new (THUNAR_TYPE_PREFERENCES, NULL);
      g_object_add_weak_pointer (G_OBJECT (preferences),
                                 (gpointer) &preferences);
    }
  else
    {
      g_object_ref (G_OBJECT (preferences));
    }

  return preferences;
}


