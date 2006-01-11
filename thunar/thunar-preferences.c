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

#include <thunar-vfs/thunar-vfs.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DEFAULT_SHOW_HIDDEN,
  PROP_DEFAULT_VIEW,
  PROP_LAST_LOCATION_BAR,
  PROP_LAST_SIDE_PANE,
  PROP_LAST_VIEW,
  PROP_MISC_FOLDERS_FIRST,
  PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES,
  PROP_MISC_RECURSIVE_PERMISSIONS,
  PROP_MISC_TEXT_BESIDE_ICONS,
  N_PROPERTIES,
};



static void     thunar_preferences_class_init         (ThunarPreferencesClass *klass);
static void     thunar_preferences_init               (ThunarPreferences      *preferences);
static void     thunar_preferences_finalize           (GObject                *object);
static void     thunar_preferences_get_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       GValue                 *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_set_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       const GValue           *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_resume_monitor     (ThunarPreferences      *preferences);
static void     thunar_preferences_suspend_monitor    (ThunarPreferences      *preferences);
static void     thunar_preferences_monitor            (ThunarVfsMonitor       *monitor,
                                                       ThunarVfsMonitorHandle *handle,
                                                       ThunarVfsMonitorEvent   event,
                                                       ThunarVfsPath          *handle_path,
                                                       ThunarVfsPath          *event_path,
                                                       gpointer                user_data);
static void     thunar_preferences_queue_load         (ThunarPreferences      *preferences);
static void     thunar_preferences_queue_store        (ThunarPreferences      *preferences);
static gboolean thunar_preferences_load_idle          (gpointer                user_data);
static void     thunar_preferences_load_idle_destroy  (gpointer                user_data);
static gboolean thunar_preferences_store_idle         (gpointer                user_data);
static void     thunar_preferences_store_idle_destroy (gpointer                user_data);



struct _ThunarPreferencesClass
{
  GObjectClass __parent__;
};

struct _ThunarPreferences
{
  GObject __parent__;

  ThunarVfsMonitorHandle *handle;
  ThunarVfsMonitor       *monitor;

  GValue                  values[N_PROPERTIES];

  gboolean                loading_in_progress;

  gint                    load_idle_id;
  gint                    store_idle_id;
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
   * ThunarPreferences::misc-folders-first:
   *
   * Whether to sort folders before files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_FOLDERS_FIRST,
                                   g_param_spec_boolean ("misc-folders-first",
                                                         "misc-folders-first",
                                                         "misc-folders-first",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-horizontal-wheel-navigates:
   *
   * Whether the horizontal mouse wheel is used to navigate
   * forth and back within a Thunar view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES,
                                   g_param_spec_boolean ("misc-horizontal-wheel-navigates",
                                                         "misc-horizontal-wheel-navigates",
                                                         "misc-horizontal-wheel-navigates",
                                                         FALSE,
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

  /**
   * ThunarPreferences::misc-text-beside-icons:
   *
   * Whether the icon view should display the file names beside the
   * file icons instead of below the file icons.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_TEXT_BESIDE_ICONS,
                                   g_param_spec_boolean ("misc-text-beside-icons",
                                                         "misc-text-beside-icons",
                                                         "misc-text-beside-icons",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_preferences_init (ThunarPreferences *preferences)
{
  /* setup the preferences object */
  preferences->load_idle_id = -1;
  preferences->store_idle_id = -1;

  /* grab a reference on the VFS monitor */
  preferences->monitor = thunar_vfs_monitor_get_default ();

  /* load the settings */
  thunar_preferences_load_idle (preferences);

  /* launch the file monitor */
  thunar_preferences_resume_monitor (preferences);
}



static void
thunar_preferences_finalize (GObject *object)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  guint              n;

  /* flush preferences */
  if (G_UNLIKELY (preferences->store_idle_id >= 0))
    {
      thunar_preferences_store_idle (preferences);
      g_source_remove (preferences->store_idle_id);
    }

  /* stop any pending load idle source */
  if (G_UNLIKELY (preferences->load_idle_id >= 0))
    g_source_remove (preferences->load_idle_id);

  /* stop the file monitor */
  if (G_LIKELY (preferences->monitor != NULL))
    {
      thunar_preferences_suspend_monitor (preferences);
      g_object_unref (G_OBJECT (preferences->monitor));
    }

  /* release the property values */
  for (n = 1; n < N_PROPERTIES; ++n)
    if (G_IS_VALUE (preferences->values + n))
      g_value_unset (preferences->values + n);

  (*G_OBJECT_CLASS (thunar_preferences_parent_class)->finalize) (object);
}



static void
thunar_preferences_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  GValue            *src;

  g_return_if_fail (prop_id < N_PROPERTIES);

  src = preferences->values + prop_id;
  if (G_IS_VALUE (src))
    g_value_copy (src, value);
  else
    g_param_value_set_default (pspec, value);
}



static void
thunar_preferences_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  GValue            *dst;

  g_return_if_fail (prop_id < N_PROPERTIES);

  dst = preferences->values + prop_id;
  if (G_UNLIKELY (!G_IS_VALUE (dst)))
    g_value_init (dst, pspec->value_type);

  if (g_param_values_cmp (pspec, value, dst) != 0)
    {
      g_value_copy (value, dst);
      g_object_notify (object, pspec->name);
      thunar_preferences_queue_store (preferences);
    }
}



static void
thunar_preferences_resume_monitor (ThunarPreferences *preferences)
{
  ThunarVfsPath *path;
  gchar         *filename;

  /* verify that the monitor is suspended */
  if (G_LIKELY (preferences->handle == NULL))
    {
      /* determine the save location for thunarrc to monitor */
      filename = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", TRUE);
      if (G_LIKELY (filename != NULL))
        {
          /* determine the VFS path for the filename */
          path = thunar_vfs_path_new (filename, NULL);
          if (G_LIKELY (path != NULL))
            {
              /* add the monitor handle for the file */
              preferences->handle = thunar_vfs_monitor_add_file (preferences->monitor, path, thunar_preferences_monitor, preferences);
              thunar_vfs_path_unref (path);
            }

          /* release the filename */
          g_free (filename);
        }
    }
}



static void
thunar_preferences_suspend_monitor (ThunarPreferences *preferences)
{
  /* verify that the monitor is active */
  if (G_LIKELY (preferences->handle != NULL))
    {
      /* disconnect the handle from the monitor */
      thunar_vfs_monitor_remove (preferences->monitor, preferences->handle);
      preferences->handle = NULL;
    }
}



static void
thunar_preferences_monitor (ThunarVfsMonitor       *monitor,
                            ThunarVfsMonitorHandle *handle,
                            ThunarVfsMonitorEvent   event,
                            ThunarVfsPath          *handle_path,
                            ThunarVfsPath          *event_path,
                            gpointer                user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);

  g_return_if_fail (THUNAR_IS_PREFERENCES (preferences));
  g_return_if_fail (THUNAR_VFS_IS_MONITOR (monitor));
  g_return_if_fail (preferences->monitor == monitor);
  g_return_if_fail (preferences->handle == handle);

  /* schedule a reload whenever the file is created/changed */
  if (event == THUNAR_VFS_MONITOR_EVENT_CHANGED || event == THUNAR_VFS_MONITOR_EVENT_CREATED)
    thunar_preferences_queue_load (preferences);
}



static void
thunar_preferences_queue_load (ThunarPreferences *preferences)
{
  if (preferences->load_idle_id < 0 && preferences->store_idle_id < 0)
    {
      preferences->load_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_preferences_load_idle,
                                                   preferences, thunar_preferences_load_idle_destroy);
    }
}



static void
thunar_preferences_queue_store (ThunarPreferences *preferences)
{
  if (preferences->store_idle_id < 0 && !preferences->loading_in_progress)
    {
      preferences->store_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_preferences_store_idle,
                                                    preferences, thunar_preferences_store_idle_destroy);
    }
}



static gchar*
property_name_to_option_name (const gchar *property_name)
{
  const gchar *s;
  gboolean     upper = TRUE;
  gchar       *option;
  gchar       *t;

  option = g_new (gchar, strlen (property_name) + 1);
  for (s = property_name, t = option; *s != '\0'; ++s)
    {
      if (*s == '-')
        {
          upper = TRUE;
        }
      else if (upper)
        {
          *t++ = g_ascii_toupper (*s);
          upper = FALSE;
        }
      else
        {
          *t++ = *s;
        }
    }
  *t = '\0';

  return option;
}



static gboolean
thunar_preferences_load_idle (gpointer user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);
  const gchar       *string;
  GParamSpec       **specs;
  GParamSpec        *spec;
  XfceRc            *rc;
  GValue             dst = { 0, };
  GValue             src = { 0, };
  gchar             *option;
  guint              nspecs;
  guint              n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", TRUE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Unable to load thunar preferences.");
      return FALSE;
    }

  g_object_freeze_notify (G_OBJECT (preferences));

  xfce_rc_set_group (rc, "Configuration");

  preferences->loading_in_progress = TRUE;

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);
  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      option = property_name_to_option_name (spec->name);
      string = xfce_rc_read_entry (rc, option, NULL);
      g_free (option);

      if (G_UNLIKELY (string == NULL))
        continue;

      g_value_init (&src, G_TYPE_STRING);
      g_value_set_static_string (&src, string);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_set_property (G_OBJECT (preferences), spec->name, &src);
        }
      else if (g_value_type_transformable (G_TYPE_STRING, spec->value_type))
        {
          g_value_init (&dst, spec->value_type);
          if (g_value_transform (&src, &dst))
            g_object_set_property (G_OBJECT (preferences), spec->name, &dst);
          g_value_unset (&dst);
        }
      else
        {
          g_warning ("Unable to load property \"%s\"", spec->name);
        }

      g_value_unset (&src);
    }
  g_free (specs);

  preferences->loading_in_progress = FALSE;

  xfce_rc_close (rc);

  g_object_thaw_notify (G_OBJECT (preferences));

  return FALSE;
}



static void
thunar_preferences_load_idle_destroy (gpointer user_data)
{
  THUNAR_PREFERENCES (user_data)->load_idle_id = -1;
}



static gboolean
thunar_preferences_store_idle (gpointer user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);
  const gchar       *string;
  GParamSpec       **specs;
  GParamSpec        *spec;
  XfceRc            *rc;
  GValue             dst = { 0, };
  GValue             src = { 0, };
  gchar             *option;
  guint              nspecs;
  guint              n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Unable to store thunar preferences.");
      return FALSE;
    }

  /* suspend the monitor (hopefully tricking FAM to avoid unnecessary reloads) */
  thunar_preferences_suspend_monitor (preferences);

  xfce_rc_set_group (rc, "Configuration");

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);
  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      g_value_init (&dst, G_TYPE_STRING);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_get_property (G_OBJECT (preferences), spec->name, &dst);
        }
      else
        {
          g_value_init (&src, spec->value_type);
          g_object_get_property (G_OBJECT (preferences), spec->name, &src);
          g_value_transform (&src, &dst);
          g_value_unset (&src);
        }

      /* determine the option name for the spec */
      option = property_name_to_option_name (spec->name);

      /* store the setting */
      string = g_value_get_string (&dst);
      if (G_LIKELY (string != NULL))
        xfce_rc_write_entry (rc, option, string);

      /* cleanup */
      g_value_unset (&dst);
      g_free (option);
    }

  /* cleanup */
  xfce_rc_close (rc);
  g_free (specs);

  /* restart the monitor */
  thunar_preferences_resume_monitor (preferences);

  return FALSE;
}



static void
thunar_preferences_store_idle_destroy (gpointer user_data)
{
  THUNAR_PREFERENCES (user_data)->store_idle_id = -1;
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


