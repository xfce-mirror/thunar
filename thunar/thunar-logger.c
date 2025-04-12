/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Philip Waritschlager <philip@waritschlager.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "thunar/thunar-logger.h"

#include "thunar/thunar-preferences.h"
#include "thunar/thunar-application.h"
#include <glib/gstdio.h>
#include "thunar/thunar-private.h"

/**
 * SECTION:thunar-logger
 * @Short_description: Manages the logging of string-based events to a log file for debugging purposes.
 * @Title: ThunarLogger
 *
 * The single #ThunarLogger instance writes to a file if configured to do so globally. This should not be confused with the operation log, the internal name for the undo/redo queue (see #_ThunarJobOperationHistory). */

static void
thunar_logger_finalize (GObject *object);



struct _ThunarLogger
{
  GObject            __parent__;

  ThunarPreferences *preferences;
  FILE              *file;
};

static ThunarLogger *logger;

G_DEFINE_TYPE (ThunarLogger, thunar_logger, G_TYPE_OBJECT)



static void
thunar_logger_class_init (ThunarLoggerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_logger_finalize;
}



static void
thunar_logger_init (ThunarLogger *self)
{
  gchar             *path;

  /* grab a reference on the preferences */
  self->preferences = thunar_preferences_get ();

  path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "Thunar/debug.log", TRUE);
  if (G_UNLIKELY (path == NULL)) {
    g_free(path);
    return;
  }

  self->file = g_fopen(path, "a");
  g_free(path);
}


static void
thunar_logger_finalize (GObject *object)
{
  ThunarLogger *_logger = THUNAR_LOGGER (object);

  _thunar_return_if_fail (THUNAR_IS_LOGGER (_logger));

  if (_logger->file != NULL) {
    fclose(_logger->file);
    _logger->file = NULL;
  }
  /* release the preferences reference */
  g_object_unref (_logger->preferences);

  (*G_OBJECT_CLASS (thunar_logger_parent_class)->finalize) (object);
}


/**
 * thunar_logger_get_default:
 *
 * Returns a reference to the default #ThunarLogger instance.
 *
 * The caller is responsible to free the returned instance
 * using g_object_unref() when no longer needed.
 *
 * Return value: the default #ThunarLogger instance.
 **/
ThunarLogger *
thunar_logger_get_default (void)
{
  if (G_UNLIKELY (logger == NULL))
    {
      /* allocate the default monitor */
      logger = g_object_new (THUNAR_TYPE_LOGGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (logger),
                                 (gpointer) &logger);
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (logger));
    }

  return logger;
}



void
thunar_logger_println (const gchar *format, ...)
{
  va_list    args;
  gboolean  *enabled;
  GDateTime *date_time;
  gchar     *time_str;

  if (G_UNLIKELY (logger->file == NULL))
    return;

  g_object_get (G_OBJECT (logger->preferences), "misc-logger", &enabled, NULL);
  if (!enabled)
    return;

  date_time = g_date_time_new_now_local();
  time_str = g_date_time_format(date_time, "%F %T");
  fprintf(logger->file, "[%s] ", time_str);
  g_free(time_str);
  g_date_time_unref(date_time);

  va_start(args, format);
  vfprintf(logger->file, format, args);
  fprintf(logger->file, "\n");
  va_end(args);

  fflush(logger->file);
}
