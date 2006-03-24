/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-size-label.h>
#include <thunar/thunar-throbber.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void     thunar_size_label_class_init            (ThunarSizeLabelClass *klass);
static void     thunar_size_label_init                  (ThunarSizeLabel      *size_label);
static void     thunar_size_label_finalize              (GObject              *object);
static void     thunar_size_label_get_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         GValue               *value,
                                                         GParamSpec           *pspec);
static void     thunar_size_label_set_property          (GObject              *object,
                                                         guint                 prop_id,
                                                         const GValue         *value,
                                                         GParamSpec           *pspec);
static void     thunar_size_label_file_changed          (ThunarFile           *file,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_error                 (ThunarVfsJob         *job,
                                                         const GError         *error,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_finished              (ThunarVfsJob         *job,
                                                         ThunarSizeLabel      *size_label);
static void     thunar_size_label_status_ready          (ThunarVfsJob         *job,
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
  GtkHBox       __parent__;
  ThunarVfsJob *job;

  ThunarFile   *file;

  GtkWidget    *label;
  GtkWidget    *throbber;

  /* the throbber animation is started after a timeout */
  gint          animate_timer_id;
};



static GObjectClass *thunar_size_label_parent_class;



GType
thunar_size_label_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarSizeLabelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_size_label_class_init,
        NULL,
        NULL,
        sizeof (ThunarSizeLabel),
        0,
        (GInstanceInitFunc) thunar_size_label_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_HBOX, I_("ThunarSizeLabel"), &info, 0);
    }

  return type;
}



static void
thunar_size_label_class_init (ThunarSizeLabelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_size_label_parent_class = g_type_class_peek_parent (klass);

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
  size_label->animate_timer_id = -1;

  gtk_widget_push_composite_child ();

  /* configure the box */
  gtk_box_set_spacing (GTK_BOX (size_label), 6);

  /* add the throbber widget */
  size_label->throbber = thunar_throbber_new ();
  gtk_box_pack_start (GTK_BOX (size_label), size_label->throbber, FALSE, FALSE, 0);
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
      thunar_vfs_job_cancel (THUNAR_VFS_JOB (size_label->job));
      g_object_unref (G_OBJECT (size_label->job));
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



static void
thunar_size_label_file_changed (ThunarFile      *file,
                                ThunarSizeLabel *size_label)
{
  ThunarVfsFileSize size;
  GError           *error = NULL;
  gchar            *size_humanized;
  gchar            *size_string;
  gchar            *text;

  g_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  g_return_if_fail (size_label->file == file);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* be sure to cancel the animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id >= 0))
    g_source_remove (size_label->animate_timer_id);

  /* cancel the pending job (if any) */
  if (G_UNLIKELY (size_label->job != NULL))
    {
      g_signal_handlers_disconnect_matched (G_OBJECT (size_label->job), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
      thunar_vfs_job_cancel (THUNAR_VFS_JOB (size_label->job));
      g_object_unref (G_OBJECT (size_label->job));
      size_label->job = NULL;
    }

  /* be sure to stop and hide the throbber */
  thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
  gtk_widget_hide (size_label->throbber);

  /* check if the file is a directory */
  if (thunar_file_is_directory (file))
    {
      /* schedule a new job to determine the total size of the directory */
      size_label->job = thunar_vfs_deep_count (thunar_file_get_path (file), &error);
      if (G_UNLIKELY (size_label->job == NULL))
        {
          /* display the error to the user */
          gtk_label_set_text (GTK_LABEL (size_label->label), error->message);
          g_error_free (error);
        }
      else
        {
          /* connect to the job */
          g_signal_connect (G_OBJECT (size_label->job), "error", G_CALLBACK (thunar_size_label_error), size_label);
          g_signal_connect (G_OBJECT (size_label->job), "finished", G_CALLBACK (thunar_size_label_finished), size_label);
          g_signal_connect (G_OBJECT (size_label->job), "status-ready", G_CALLBACK (thunar_size_label_status_ready), size_label);

          /* tell the user that we started calculation */
          gtk_label_set_text (GTK_LABEL (size_label->label), _("Calculating..."));
        }
    }
  else
    {
      /* determine the size of the file */
      size = thunar_file_get_size (file);

      /* determine the size in bytes */
      text = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) size);
      size_string = g_strdup_printf (_("%s Bytes"), text);
      g_free (text);

      /* check if the file is larger that 1kB */
      if (G_LIKELY (size > 1024ul))
        {
          /* prepend the humanized size */
          size_humanized = thunar_vfs_humanize_size (size, NULL, 0);
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
thunar_size_label_error (ThunarVfsJob    *job,
                         const GError    *error,
                         ThunarSizeLabel *size_label)
{
  g_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (size_label->job == job);

  /* setup the error text as label */
  gtk_label_set_text (GTK_LABEL (size_label->label), error->message);
}



static void
thunar_size_label_finished (ThunarVfsJob    *job,
                            ThunarSizeLabel *size_label)
{
  g_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (size_label->job == job);

  /* be sure to cancel the animate timer */
  if (G_UNLIKELY (size_label->animate_timer_id >= 0))
    g_source_remove (size_label->animate_timer_id);

  /* stop and hide the throbber */
  thunar_throbber_set_animated (THUNAR_THROBBER (size_label->throbber), FALSE);
  gtk_widget_hide (size_label->throbber);

  /* disconnect from the job */
  g_signal_handlers_disconnect_matched (G_OBJECT (size_label->job), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, size_label);
  g_object_unref (G_OBJECT (size_label->job));
  size_label->job = NULL;
}



static void
thunar_size_label_status_ready (ThunarVfsJob    *job,
                                guint64          total_size,
                                guint            file_count,
                                guint            directory_count,
                                guint            unreadable_directory_count,
                                ThunarSizeLabel *size_label)
{
  gchar *size_string;
  gchar *text;
  guint  n;

  g_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  g_return_if_fail (THUNAR_VFS_IS_JOB (job));
  g_return_if_fail (size_label->job == job);

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
  size_string = thunar_vfs_humanize_size (total_size, NULL, 0);
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
  g_return_val_if_fail (THUNAR_IS_SIZE_LABEL (size_label), NULL);
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
  g_return_if_fail (THUNAR_IS_SIZE_LABEL (size_label));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

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


