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
  PROP_FILES,
};



typedef struct _ThunarSizeLabelFile ThunarSizeLabelFile;



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
static void     thunar_size_label_file_changed          (ThunarFile           *changed_file,
                                                         ThunarSizeLabelFile  *file);
static void     thunar_size_label_error                 (ExoJob               *job,
                                                         const GError         *error,
                                                         ThunarSizeLabelFile  *file);
static void     thunar_size_label_finished              (ExoJob               *job,
                                                         ThunarSizeLabelFile  *file);
static void     thunar_size_label_status_update         (ThunarDeepCountJob   *job,
                                                         guint64               total_size,
                                                         guint                 file_count,
                                                         guint                 directory_count,
                                                         guint                 unreadable_directory_count,
                                                         ThunarSizeLabelFile  *file);
static gboolean thunar_size_label_animate_timer         (gpointer              user_data);
static void     thunar_size_label_animate_timer_destroy (gpointer              user_data);



struct _ThunarSizeLabelClass
{
  GtkHBoxClass __parent__;
};

struct _ThunarSizeLabel
{
  GtkHBox             __parent__;

  GList              *files;

  GtkWidget          *label;
  GtkWidget          *throbber;

  /* the throbber animation is started after a timeout */
  guint               animate_timer_id;
};

struct _ThunarSizeLabelFile
{
  ThunarSizeLabel    *size_label;

  ThunarFile         *file;
  ThunarDeepCountJob *job;

  /* results of the deep-count job */
  guint64             total_size;
  guint               file_count;
  guint               directory_count;
  guint               unreadable_directory_count;
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
                                   PROP_FILES,
                                   g_param_spec_boxed ("files", "files", "files",
                                                       THUNARX_TYPE_FILE_INFO_LIST,
                                                       EXO_PARAM_WRITABLE));
}



static void
thunar_size_label_init (ThunarSizeLabel *size_label)
{
  GtkWidget *ebox;

  gtk_widget_push_composite_child ();

  /* configure the box */
  gtk_box_set_spacing (GTK_BOX (size_label), 6);

  /* add an evenbox for the throbber */
  ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  g_signal_connect (G_OBJECT (ebox), "button-press-event", G_CALLBACK (thunar_size_label_button_press_event), size_label);
  gtk_widget_set_tooltip_text (ebox, _("Click here to stop calculating the total size of the folder."));
  gtk_box_pack_start (GTK_BOX (size_label), ebox, FALSE, FALSE, 0);

  /* add the throbber widget */
  size_label->throbber = thunar_throbber_new ();
  exo_binding_new (G_OBJECT (size_label->throbber), "visible", G_OBJECT (ebox), "visible");
  gtk_container_add (GTK_CONTAINER (ebox), size_label->throbber);

  /* add the label widget */
  size_label->label = gtk_label_new (_("Calculating..."));
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

  /* reset the file property */
  thunar_size_label_set_files (size_label, NULL);

  /* be sure to cancel any pending animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id != 0))
    g_source_remove (size_label->animate_timer_id);

  (*G_OBJECT_CLASS (thunar_size_label_parent_class)->finalize) (object);
}



static void
thunar_size_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
    case PROP_FILES:
      thunar_size_label_set_files (size_label, g_value_get_boxed (value));
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
  GList               *lp;
  ThunarSizeLabelFile *file;

  _thunar_return_val_if_fail (GTK_IS_EVENT_BOX (ebox), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), FALSE);

  /* left button press on the throbber cancels the calculation */
  if (G_LIKELY (event->button == 1))
    {
      /* be sure to cancel the animate timer */
      if (G_UNLIKELY (size_label->animate_timer_id != 0))
        g_source_remove (size_label->animate_timer_id);

      /* cancel all pending jobs (if any) */
      for (lp = size_label->files; lp != NULL; lp = lp->next)
        {
          file = lp->data;
          if (G_UNLIKELY (file->job != NULL))
            {
              g_signal_handlers_disconnect_matched (file->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file);
              exo_job_cancel (EXO_JOB (file->job));
              g_object_unref (file->job);
              file->job = NULL;
            }
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



static void
thunar_size_label_update (ThunarSizeLabel *size_label)
{
  GList               *lp;
  ThunarSizeLabelFile *file;
  guint64              total_size = 0;
  guint                file_count = 0;
  guint                directory_count = 0;
  guint                unreadable_directory_count = 0;
  gchar               *size_string;
  gchar               *text;
  guint                n;
  gchar               *unreable_text;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));

  for (lp = size_label->files; lp != NULL; lp = lp->next)
    {
      file = lp->data;

      total_size += file->total_size;
      file_count += file->file_count;
      directory_count += file->directory_count;
      unreadable_directory_count += file->unreadable_directory_count;
    }

  /* check if this is a single file or a directory/multiple files */
  if (file_count == 1 && directory_count == 0)
    {
      size_string = g_format_size_full (total_size, G_FORMAT_SIZE_LONG_FORMAT);
      gtk_label_set_text (GTK_LABEL (size_label->label), size_string);
      g_free (size_string);
    }
  else
    {
      n = file_count + directory_count + unreadable_directory_count;

      size_string = g_format_size (total_size);
      text = g_strdup_printf (ngettext ("%u item, totalling %s", "%u items, totalling %s", n), n, size_string);
      g_free (size_string);

      if (unreadable_directory_count > 0)
        {
          /* TRANSLATORS: this is shows if during the deep count size
           * directories were not accessible */
          unreable_text = g_strconcat (text, "\n", _("(some contents unreadable)"), NULL);
          g_free (text);
          text = unreable_text;
        }

      gtk_label_set_text (GTK_LABEL (size_label->label), text);
      g_free (text);
    }
}



static void
thunar_size_label_file_changed (ThunarFile          *changed_file,
                                ThunarSizeLabelFile *file)
{
  ThunarSizeLabel *size_label = file->size_label;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (THUNAR_IS_FILE (changed_file));
  _thunar_return_if_fail (file->file == changed_file);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (file->job != NULL))
    {
      g_signal_handlers_disconnect_matched (file->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file);
      exo_job_cancel (EXO_JOB (file->job));
      g_object_unref (file->job);
      file->job = NULL;
    }

  /* reset the size values */
  file->total_size = 0;
  file->file_count = 0;
  file->directory_count = 0;
  file->unreadable_directory_count = 0;

  /* check if the file is a directory */
  if (thunar_file_is_directory (changed_file))
    {
      /* schedule a new job to determine the total size of the directory (not following symlinks) */
      file->job = thunar_deep_count_job_new (thunar_file_get_file (changed_file), G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);
      g_signal_connect (file->job, "error", G_CALLBACK (thunar_size_label_error), file);
      g_signal_connect (file->job, "finished", G_CALLBACK (thunar_size_label_finished), file);
      g_signal_connect (file->job, "status-update", G_CALLBACK (thunar_size_label_status_update), file);

      /* launch the job */
      exo_job_launch (EXO_JOB (file->job));
    }
  else
    {
      /* update the data property */
      file->total_size = thunar_file_get_size (changed_file);
      file->file_count = 1;

      /* update the label */
      thunar_size_label_update (size_label);
    }
}



static void
thunar_size_label_error (ExoJob              *job,
                         const GError        *error,
                         ThunarSizeLabelFile *file)
{
  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (file->size_label));
  _thunar_return_if_fail (file->job == THUNAR_DEEP_COUNT_JOB (job));

  /* setup the error text as label */
  gtk_label_set_text (GTK_LABEL (file->size_label->label), error->message);
}



static void
thunar_size_label_finished (ExoJob              *job,
                            ThunarSizeLabelFile *file)
{
  ThunarSizeLabel *size_label = file->size_label;
  GList           *lp;

  _thunar_return_if_fail (THUNAR_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (file->job == THUNAR_DEEP_COUNT_JOB (job));

  /* disconnect from the job */
  g_signal_handlers_disconnect_matched (file->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file);
  g_object_unref (file->job);
  file->job = NULL;

  /* look if there are jobs pending */
  for (lp = size_label->files; lp != NULL; lp = lp->next)
    if (((ThunarSizeLabelFile *) lp->data)->job != NULL)
      break;

  if (lp == NULL)
    {
      /* be sure to cancel the animate timer */
      if (G_UNLIKELY (size_label->animate_timer_id != 0))
        g_source_remove (size_label->animate_timer_id);

      /* stop and hide the throbber */
      thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
      gtk_widget_hide (size_label->throbber);
    }
}



static void
thunar_size_label_status_update (ThunarDeepCountJob  *job,
                                 guint64              total_size,
                                 guint                file_count,
                                 guint                directory_count,
                                 guint                unreadable_directory_count,
                                 ThunarSizeLabelFile *file)
{
  ThunarSizeLabel *size_label = file->size_label;

  _thunar_return_if_fail (THUNAR_IS_DEEP_COUNT_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (file->job == job);

  /* check if the animate timer is already running */
  if (G_UNLIKELY (size_label->animate_timer_id == 0))
    {
      /* schedule the animate timer to animate and display the throbber after 1s */
      size_label->animate_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, thunar_size_label_animate_timer,
                                                         size_label, thunar_size_label_animate_timer_destroy);
    }

  /* update the structure values */
  file->total_size = total_size;
  file->file_count = file_count;
  file->directory_count = directory_count;
  file->unreadable_directory_count = unreadable_directory_count;

  /* update the label */
  thunar_size_label_update (size_label);
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
  THUNAR_SIZE_LABEL (user_data)->animate_timer_id = 0;
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
 * thunar_size_label_set_files:
 * @size_label : a #ThunarSizeLabel.
 * @files      : a list of #ThunarFile's or %NULL.
 *
 * Sets @file as the #ThunarFile displayed by the @size_label.
 **/
void
thunar_size_label_set_files (ThunarSizeLabel *size_label,
                             GList           *files)
{
  GList               *lp;
  ThunarSizeLabelFile *file;

  _thunar_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  _thunar_return_if_fail (files == NULL || THUNAR_IS_FILE (files->data));

  /* disconnect from the previous files */
  for (lp = size_label->files; lp != NULL; lp = lp->next)
    {
      file = lp->data;
      _thunar_assert (THUNAR_IS_FILE (file->file));

      g_signal_handlers_disconnect_by_func (G_OBJECT (file->file), thunar_size_label_file_changed, file);

      /* cleanup file data */
      g_object_unref (G_OBJECT (file->file));
      g_slice_free (ThunarSizeLabelFile, file);
    }

  g_list_free (size_label->files);
  size_label->files = NULL;

  /* connect to the new file */
  for (lp = files; lp != NULL; lp = lp->next)
    {
      _thunar_assert (THUNAR_IS_FILE (lp->data));

      file = g_slice_new0 (ThunarSizeLabelFile);
      file->size_label = size_label;
      file->file = g_object_ref (G_OBJECT (lp->data));

      size_label->files = g_list_prepend (size_label->files, file);

      thunar_size_label_file_changed (file->file, file);
      g_signal_connect (G_OBJECT (file->file), "changed", G_CALLBACK (thunar_size_label_file_changed), file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (size_label), "files");
}


