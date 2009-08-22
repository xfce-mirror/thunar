/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-size-label.h>
#include <thunar/thunar-throbber.h>
#include <thunar/thunar-deep-count-job.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void     thunar_size_label_finalize              (GObject              *object);
static void     thunar_size_label_get_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void     thunar_size_label_set_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static gboolean thunar_size_label_button_press_event    (GtkWidget            *ebox,
                                                         GdkEventButton       *event,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_file_changed          (ThunarFile           *file,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_error                 (ExoJob               *job,
                                                         const GError         *error,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_finished              (ExoJob               *job,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_status_update         (ThunarDeepCountJob   *job,
                                                         guint64               total_size,
                                                         guint                 file_count,
                                                         guint                 directory_count,
                                                         guint                 unreadable_directory_count,
                                                         ThunarSizeLabel      *size_label);
static gboolean thunar_size_label_animate_timer         (gpointer              user_data);
static void     thunar_size_label_animate_timer_destroy (gpointer              user_data);



struct _ThunarSizeLabelClass
{
  GtkHBoxClass __parent__;
};

struct _ThunarSizeLabel
{
  GtkHBox             __parent__;

  ThunarDeepCountJob *job;

  ThunarFile         *file;

  GtkWidget          *label;
  GtkWidget          *throbber;

  /* the throbber animation is started after a timeout */
  gint                animate_timer_id;
};



G_DEFINE_TYPE (ThunarSizeLabel, thunar_size_label, GTK_TYPE_HBOX)



static void
thunar_size_label_class_init (ThunarSizeLabelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_size_label_finalize;
  gobject_class->get_property = thunar_size_label_get_property;
  gobject_class->set_property = thunar_size_label_set_property;

  /**
   * ThunarSizeLabel:file:
   *
   * The #ThunarFile whose size should be displayed
   * by this #ThunarSizeLabel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_size_label_init (ThunarSizeLabel *size_label)
{
  GtkWidget *ebox;

  size_label->animate_timer_id = -1;

  gtk_widget_push_composite_child ();

  /* configure the box */
  gtk_box_set_spacing (GTK_BOX (size_label), 6);

  /* add an evenbox for the throbber */
  ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (thunar_size_label_button_press_event), size_label);
  gtk_widget_set_tooltip_text (ebox, _("Click here to stop calculating the total size of the folder."));
  gtk_box_pack_start (GTK_BOX (size_label), ebox, FALSE, FALSE, 0);
  gtk_widget_show (ebox);

  /* add the throbber widget */
  size_label->throbber = thunar_throbber_new ();
  exo_binding_new (G_OBJECT (size_label->throbber), "visible", G_OBJECT (ebox), "visible");
  gtk_container_add (GTK_CONTAINER (ebox), size_label->throbber);
  gtk_widget_show (size_label->throbber);

  /* add the label widget */
  size_label->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (size_label->label), 0.0f, 0.5f);
  gtk_label_set_selectable (GTK_LABEL (size_label->label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (size_label->label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_pack_start (GTK_BOX (size_label), size_label->label, TRUE, TRUE, 0);
  gtk_widget_show (size_label->label);

  gtk_widget_pop_composite_child ();
}



static void
thunar_size_label_finalize (GObject *object)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (size_label->job != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (size_label->job), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
      exo_job_cancel (EXO_JOB (size_label->job));
      g_object_unref (size_label->job);
    }

  /* reset the file property */
  thunar_size_label_set_file (size_label, NULL);

  /* be sure to cancel any pending animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id >= 0))
    g_source_remove (size_label->animate_timer_id);

  (*G_OBJECT_CLASS (thunar_size_label_parent_class)->finalize) (object);
}



static void
thunar_size_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_size_label_get_file (size_label));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_size_label_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_size_label_set_file (size_label, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_size_label_button_press_event (GtkWidget       *ebox,
                                      GdkEventButton  *event,
                                      ThunarSizeLabel *size_label)
{
  _thunar_return_val_if_fail (GTK_IS_EVENT_BOX (ebox), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), FALSE);

  /* left button press on the throbber cancels the calculation */
  if (G_LIKELY (event->button == 1))
    {
      /* be sure to cancel the animate timer */
      if (G_UNLIKELY (size_label->animate_timer_id >= 0))
        g_source_remove (size_label->animate_timer_id);

      /* cancel the pending job (if any) */
      if (G_UNLIKELY (size_label->job != NULL))
        {
          g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
          exo_job_cancel (EXO_JOB (size_label->job));
          g_object_unref (size_label->job);
          size_label->job = NULL;
        }

      /* be sure to stop and hide the throbber */
      thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
      gtk_widget_hide (size_label->throbber);

      /* tell the user that the operation was canceled */
      gtk_label_set_text (GTK_LABEL (size_label->label), _("Calculation aborted"));

      /* we handled the event */
      return TRUE;
    }

  return FALSE;
}



static gchar*
tsl_format_size_string (guint64 size)
{
  GString *result;
  gchar   *grouping;
  gchar   *thousep;
  gint     ndigits = 0;

#ifdef HAVE_LOCALECONV
  grouping = localeconv ()->grouping;
  thousep = localeconv ()->thousands_sep;
#else
  grouping = "\3\0";
  thousep = ",";
#endif

  result = g_string_sized_new (32);
  do
    {
      /* prepend the next digit to the string */
      g_string_prepend_c (result, '0' + (size % 10));
      ++ndigits;
      
      /* check if we should add the thousands separator */
      if (ndigits == *grouping && *grouping != CHAR_MAX && size > 9)
        {
          g_string_prepend (result, thousep);
          ndigits = 0;

          /* if *(grouping+1) == '\0' then we have to use the
           * *grouping character (last grouping rule) for all
           * following cases.
           */
          if (*(grouping + 1) != '\0')
            ++grouping;
        }

      size /= 10;
    }
  while (size > 0);

  return g_string_free (result, FALSE);
}



static void
thunar_size_label_file_changed (ThunarFile      *file,
                                ThunarSizeLabel *size_label)
{
  guint64 size;
  gchar  *size_humanized;
  gchar  *size_string;
  gchar  *text;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* be sure to cancel the animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id >= 0))
    g_source_remove (size_label->animate_timer_id);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (size_label->job != NULL))
    {
      g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
      exo_job_cancel (EXO_JOB (size_label->job));
      g_object_unref (size_label->job);
      size_label->job = NULL;
    }

  /* be sure to stop and hide the throbber */
  thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
  gtk_widget_hide (size_label->throbber);

  /* check if the file is a directory */
  if (thunar_file_is_directory (file))
    {
      /* schedule a new job to determine the total size of the directory (not following symlinks) */
      size_label->job = thunar_deep_count_job_new (thunar_file_get_file (file), G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);
      g_signal_connect (size_label->job, "error", G_CALLBACK (thunar_size_label_error), size_label);
      g_signal_connect (size_label->job, "finished", G_CALLBACK (thunar_size_label_finished), size_label);
      g_signal_connect (size_label->job, "status-update", G_CALLBACK (thunar_size_label_status_update), size_label);

      /* tell the user that we started calculation */
      gtk_label_set_text (GTK_LABEL (size_label->label), _("Calculating..."));

      /* launch the job */
      exo_job_launch (EXO_JOB (size_label->job));
    }
  else
    {
      /* determine the size of the file */
      size = thunar_file_get_size (file);

      /* determine the size in bytes */
      text = tsl_format_size_string (size);
      size_string = g_strdup_printf (_("%s Bytes"), text);
      g_free (text);

      /* check if the file is larger that 1kB */
      if (G_LIKELY (size > 1024ul))
        {
          /* prepend the humanized size */
          size_humanized = g_format_size_for_display (size);
          text = g_strdup_printf ("%s (%s)", size_humanized, size_string);
          g_free (size_humanized);
          g_free (size_string);
          size_string = text;
        }

      /* setup the new label */
      gtk_label_set_text (GTK_LABEL (size_label->label), size_string);

      /* cleanup */
      g_free (size_string);
    }
}



static void
thunar_size_label_error (ExoJob          *job,
                         const GError    *error,
                         ThunarSizeLabel *size_label)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == THUNAR_DEEP_COUNT_JOB (job));

  /* setup the error text as label */
  gtk_label_set_text (GTK_LABEL (size_label->label), error->message);
}



static void
thunar_size_label_finished (ExoJob          *job,
                            ThunarSizeLabel *size_label)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == THUNAR_DEEP_COUNT_JOB (job));

  /* be sure to cancel the animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id >= 0))
    g_source_remove (size_label->animate_timer_id);

  /* stop and hide the throbber */
  thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
  gtk_widget_hide (size_label->throbber);

  /* disconnect from the job */
  g_signal_handlers_disconnect_matched (size_label->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
  g_object_unref (size_label->job);
  size_label->job = NULL;
}



static void
thunar_size_label_status_update (ThunarDeepCountJob *job,
                                 guint64             total_size,
                                 guint               file_count,
                                 guint               directory_count,
                                 guint               unreadable_directory_count,
                                 ThunarSizeLabel    *size_label)
{
  gchar *size_string;
  gchar *text;
  guint  n;

  _thunar_return_if_fail (THUNAR_IS_DEEP_COUNT_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (size_label->job == job);

  /* check if the animate timer is already running */
  if (G_UNLIKELY (size_label->animate_timer_id < 0))
    {
      /* schedule the animate timer to animate and display the throbber after 1s */
      size_label->animate_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, thunar_size_label_animate_timer,
                                                         size_label, thunar_size_label_animate_timer_destroy);
    }

  /* determine the total number of items */
  n = file_count + directory_count + unreadable_directory_count;

  /* update the label */
  size_string = g_format_size_for_display (total_size);
  text = g_strdup_printf (ngettext ("%u item, totalling %s", "%u items, totalling %s", n), n, size_string);
  gtk_label_set_text (GTK_LABEL (size_label->label), text);
  g_free (size_string);
  g_free (text);
}



static gboolean
thunar_size_label_animate_timer (gpointer user_data)
{
  ThunarSizeLabel *size_label = THUNAR_SIZE_LABEL (user_data);

  GDK_THREADS_ENTER ();

  /* animate and display the throbber */
  thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), TRUE);
  gtk_widget_show (size_label->throbber);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_size_label_animate_timer_destroy (gpointer user_data)
{
  THUNAR_SIZE_LABEL (user_data)->animate_timer_id = -1;
}



/**
 * thunar_size_label_new:
 * 
 * Allocates a new #ThunarSizeLabel instance.
 *
 * Return value: the newly allocated #ThunarSizeLabel.
 **/
GtkWidget*
thunar_size_label_new (void)
{
  return g_object_new (THUNAR_TYPE_SIZE_LABEL, NULL);
}



/**
 * thunar_size_label_get_file:
 * @size_label : a #ThunarSizeLabel.
 *
 * Returns the #ThunarFile for the @size_label.
 *
 * Return value: the file for @size_label.
 **/
ThunarFile*
thunar_size_label_get_file (ThunarSizeLabel *size_label)
{
  _thunar_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), NULL);
  return size_label->file;
}



/**
 * thunar_size_label_set_file:
 * @size_label : a #ThunarSizeLabel.
 * @file       : a #ThunarFile or %NULL.
 *
 * Sets @file as the #ThunarFile displayed by the @size_label.
 **/
void
thunar_size_label_set_file (ThunarSizeLabel *size_label,
                            ThunarFile      *file)
{
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already display that file */
  if (G_UNLIKELY (size_label->file == file))
    return;

  /* disconnect from the previous file */
  if (G_UNLIKELY (size_label->file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (size_label->file), thunar_size_label_file_changed, size_label);
      g_object_unref (G_OBJECT (size_label->file));
    }

  /* activate the new file */
  size_label->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      g_object_ref (G_OBJECT (file));
      thunar_size_label_file_changed (file, size_label);
      g_signal_connect (G_OBJECT (file), "changed", G_CALLBACK (thunar_size_label_file_changed), size_label);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (size_label), "file");
}


