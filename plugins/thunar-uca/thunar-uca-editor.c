/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#include <thunar-uca/thunar-uca-editor.h>



static const gchar   *thunar_uca_editor_get_icon_name   (const ThunarUcaEditor  *uca_editor);
static void           thunar_uca_editor_set_icon_name   (ThunarUcaEditor        *uca_editor,
                                                         const gchar            *icon_name);
static ThunarUcaTypes thunar_uca_editor_get_types       (const ThunarUcaEditor  *uca_editor);
static void           thunar_uca_editor_set_types       (ThunarUcaEditor        *uca_editor,
                                                         ThunarUcaTypes          types);
static void           thunar_uca_editor_command_clicked (ThunarUcaEditor        *uca_editor);
static void           thunar_uca_editor_icon_clicked    (ThunarUcaEditor        *uca_editor);
static void           thunar_uca_editor_constructed     (GObject                *object);



struct _ThunarUcaEditorClass
{
  GtkDialogClass __parent__;
};

struct _ThunarUcaEditor
{
  GtkDialog __parent__;

  GtkWidget   *notebook;
  GtkWidget   *name_entry;
  GtkWidget   *description_entry;
  GtkWidget   *icon_button;
  GtkWidget   *command_entry;
  GtkWidget   *sn_button;
  GtkWidget   *patterns_entry;
  GtkWidget   *directories_button;
  GtkWidget   *audio_files_button;
  GtkWidget   *image_files_button;
  GtkWidget   *text_files_button;
  GtkWidget   *video_files_button;
  GtkWidget   *other_files_button;
};



THUNARX_DEFINE_TYPE (ThunarUcaEditor, thunar_uca_editor, GTK_TYPE_DIALOG);



static void
thunar_uca_editor_class_init (ThunarUcaEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  /* vfuncs */
  object_class->constructed = thunar_uca_editor_constructed;

  /* Setup the template xml */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/xfce/thunar/uca/editor.ui");

  /* bind stuff */
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, notebook);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, description_entry);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, icon_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, command_entry);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, sn_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, patterns_entry);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, directories_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, audio_files_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, image_files_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, text_files_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, video_files_button);
  gtk_widget_class_bind_template_child (widget_class, ThunarUcaEditor, other_files_button);

  gtk_widget_class_bind_template_callback(widget_class, thunar_uca_editor_icon_clicked);
  gtk_widget_class_bind_template_callback(widget_class, thunar_uca_editor_command_clicked);
}



static void
thunar_uca_editor_init (ThunarUcaEditor *uca_editor)
{
  /* Initialize the template for this instance */
  gtk_widget_init_template (GTK_WIDGET (uca_editor));

  /* configure the dialog properties */
  gtk_dialog_add_button (GTK_DIALOG (uca_editor), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (uca_editor), _("_OK"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (uca_editor), GTK_RESPONSE_OK);
}



static void
thunar_uca_editor_constructed (GObject *object)
{
  ThunarUcaEditor *editor = THUNAR_UCA_EDITOR (object);

  G_OBJECT_CLASS (thunar_uca_editor_parent_class)->constructed (object);

  /* Visual tweaks for header-bar mode only */
  g_object_set (gtk_dialog_get_content_area (GTK_DIALOG (editor)), "border-width", 0, NULL);
  g_object_set (editor->notebook, "show-border", FALSE, NULL);
}



static void
thunar_uca_editor_command_clicked (ThunarUcaEditor *uca_editor)
{
  GtkFileFilter *filter;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar        **argv = NULL;
  gchar         *s;
  gint           argc;

  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));

  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                         GTK_WINDOW (uca_editor),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* use the bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* setup the currently selected file */
  filename = gtk_editable_get_chars (GTK_EDITABLE (uca_editor->command_entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* check if we have a file name */
      if (G_LIKELY (*filename != '\0'))
        {
          /* check if the filename is not an absolute path */
          if (G_LIKELY (!g_path_is_absolute (filename)))
            {
              /* try to lookup the filename in $PATH */
              s = g_find_program_in_path (filename);
              if (G_LIKELY (s != NULL))
                {
                  /* use the absolute path instead */
                  g_free (filename);
                  filename = s;
                }
            }

          /* check if we have an absolute path now */
          if (G_LIKELY (g_path_is_absolute (filename)))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
        }

      /* release the filename */
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* determine the path to the selected file */
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

      /* check if we need to quote the filename */
      if (!g_shell_parse_argv (filename, &argc, &argv, NULL) || argc > 1)
        {
          /* shell is unable to interpret properly without quoting */
          s = g_shell_quote (filename);
          g_free (filename);
          filename = s;
        }
      g_strfreev (argv);

      /* append %f to filename, user may change that afterwards */
      s = g_strconcat (filename, " %f", NULL);
      gtk_entry_set_text (GTK_ENTRY (uca_editor->command_entry), s);
      g_free (filename);
      g_free (s);
    }

  gtk_widget_destroy (chooser);
}



static void
thunar_uca_editor_icon_clicked (ThunarUcaEditor *uca_editor)
{
  const gchar *name;
  GtkWidget   *chooser;
  gchar       *title;
  gchar       *icon;

  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));

  /* determine the name of the action being edited */
  name = gtk_entry_get_text (GTK_ENTRY (uca_editor->name_entry));
  if (G_UNLIKELY (name == NULL || *name == '\0'))
    name = _("Unknown");

  /* allocate the chooser dialog */
  title = g_strdup_printf (_("Select an Icon for \"%s\""), name);
  chooser = exo_icon_chooser_dialog_new (title, GTK_WINDOW (uca_editor),
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         _("_OK"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  g_free (title);

  /* setup the currently selected icon */
  icon = g_object_get_data (G_OBJECT (uca_editor->icon_button), "thunar-uca-icon-name");
  if (G_LIKELY (icon != NULL && *icon != '\0'))
    exo_icon_chooser_dialog_set_icon (EXO_ICON_CHOOSER_DIALOG (chooser), icon);

  /* run the icon chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      /* remember the selected icon from the chooser */
      icon = exo_icon_chooser_dialog_get_icon (EXO_ICON_CHOOSER_DIALOG (chooser));
      thunar_uca_editor_set_icon_name (uca_editor, icon);
      g_free (icon);
    }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}



static const gchar*
thunar_uca_editor_get_icon_name (const ThunarUcaEditor *uca_editor)
{
  g_return_val_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor), NULL);
  return g_object_get_data (G_OBJECT (uca_editor->icon_button), "thunar-uca-icon-name");
}



static void
thunar_uca_editor_set_icon_name (ThunarUcaEditor *uca_editor,
                                 const gchar     *icon_name)
{
  GIcon     *icon = NULL;
  GtkWidget *image;
  GtkWidget *label;

  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));

  /* drop the previous button child */
  if (gtk_bin_get_child (GTK_BIN (uca_editor->icon_button)) != NULL)
    gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (uca_editor->icon_button)));

  /* setup the icon button */
  if (icon_name != NULL)
    icon = g_icon_new_for_string (icon_name, NULL);

  if (G_LIKELY (icon != NULL))
    {
      /* setup an image for the icon */
      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
      g_object_set (image, "icon-size", GTK_ICON_SIZE_DND, NULL);
      gtk_container_add (GTK_CONTAINER (uca_editor->icon_button), image);
      gtk_widget_show (image);

      /* remember the name for the icon */
      g_object_set_data_full (G_OBJECT (uca_editor->icon_button), "thunar-uca-icon-name", g_strdup (icon_name), g_free);

      /* release the icon */
      g_object_unref (G_OBJECT (icon));
    }
  else
    {
      /* reset the remembered icon name */
      g_object_set_data (G_OBJECT (uca_editor->icon_button), "thunar-uca-icon-name", NULL);

      /* setup a label to tell that no icon was selected */
      label = gtk_label_new (_("No icon"));
      gtk_container_add (GTK_CONTAINER (uca_editor->icon_button), label);
      gtk_widget_show (label);
    }
}



static ThunarUcaTypes
thunar_uca_editor_get_types (const ThunarUcaEditor *uca_editor)
{
  ThunarUcaTypes types = 0;

  g_return_val_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor), 0);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->directories_button)))
    types |= THUNAR_UCA_TYPE_DIRECTORIES;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->audio_files_button)))
    types |= THUNAR_UCA_TYPE_AUDIO_FILES;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->image_files_button)))
    types |= THUNAR_UCA_TYPE_IMAGE_FILES;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->text_files_button)))
    types |= THUNAR_UCA_TYPE_TEXT_FILES;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->video_files_button)))
    types |= THUNAR_UCA_TYPE_VIDEO_FILES;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->other_files_button)))
    types |= THUNAR_UCA_TYPE_OTHER_FILES;

  return types;
}



static void
thunar_uca_editor_set_types (ThunarUcaEditor *uca_editor,
                             ThunarUcaTypes   types)
{
  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->directories_button), (types & THUNAR_UCA_TYPE_DIRECTORIES));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->audio_files_button), (types & THUNAR_UCA_TYPE_AUDIO_FILES));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->image_files_button), (types & THUNAR_UCA_TYPE_IMAGE_FILES));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->text_files_button),  (types & THUNAR_UCA_TYPE_TEXT_FILES));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->video_files_button), (types & THUNAR_UCA_TYPE_VIDEO_FILES));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->other_files_button), (types & THUNAR_UCA_TYPE_OTHER_FILES));
}



/**
 * thunar_uca_editor_load:
 * @uca_editor  : a #ThunarUcaEditor.
 * @uca_model   : a #ThunarUcaModel.
 * @iter        : a #GtkTreeIter.
 *
 * Loads @uca_editor with the data from @uca_model
 * at the specified @iter.
 **/
void
thunar_uca_editor_load (ThunarUcaEditor *uca_editor,
                        ThunarUcaModel  *uca_model,
                        GtkTreeIter     *iter)
{
  ThunarUcaTypes types;
  gchar         *description;
  gchar         *patterns;
  gchar         *command;
  gchar         *icon_name;
  gchar         *name;
  gboolean       startup_notify;

  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));
  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter != NULL);

  /* determine the current values from the model */
  gtk_tree_model_get (GTK_TREE_MODEL (uca_model), iter,
                      THUNAR_UCA_MODEL_COLUMN_DESCRIPTION, &description,
                      THUNAR_UCA_MODEL_COLUMN_PATTERNS, &patterns,
                      THUNAR_UCA_MODEL_COLUMN_COMMAND, &command,
                      THUNAR_UCA_MODEL_COLUMN_TYPES, &types,
                      THUNAR_UCA_MODEL_COLUMN_ICON_NAME, &icon_name,
                      THUNAR_UCA_MODEL_COLUMN_NAME, &name,
                      THUNAR_UCA_MODEL_COLUMN_STARTUP_NOTIFY, &startup_notify,
                      -1);

  /* setup the new selection */
  thunar_uca_editor_set_types (uca_editor, types);

  /* setup the new icon */
  thunar_uca_editor_set_icon_name (uca_editor, icon_name);

  /* apply the new values */
  gtk_entry_set_text (GTK_ENTRY (uca_editor->description_entry), (description != NULL) ? description : "");
  gtk_entry_set_text (GTK_ENTRY (uca_editor->patterns_entry), (patterns != NULL) ? patterns : "");
  gtk_entry_set_text (GTK_ENTRY (uca_editor->command_entry), (command != NULL) ? command : "");
  gtk_entry_set_text (GTK_ENTRY (uca_editor->name_entry), (name != NULL) ? name : "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uca_editor->sn_button), startup_notify);

  /* cleanup */
  g_free (description);
  g_free (patterns);
  g_free (command);
  g_free (icon_name);
  g_free (name);
}



/**
 * thunar_uca_editor_save:
 * @uca_editor : a #ThunarUcaEditor.
 * @uca_model  : a #ThunarUcaModel.
 * @iter       : a #GtkTreeIter.
 *
 * Stores the current values from the @uca_editor in
 * the @uca_model at the item specified by @iter.
 **/
void
thunar_uca_editor_save (ThunarUcaEditor *uca_editor,
                        ThunarUcaModel  *uca_model,
                        GtkTreeIter     *iter)
{
  g_return_if_fail (THUNAR_UCA_IS_EDITOR (uca_editor));
  g_return_if_fail (THUNAR_UCA_IS_MODEL (uca_model));
  g_return_if_fail (iter != NULL);

  thunar_uca_model_update (uca_model, iter,
                           gtk_entry_get_text (GTK_ENTRY (uca_editor->name_entry)),
                           NULL, /* don't touch the unique id */
                           gtk_entry_get_text (GTK_ENTRY (uca_editor->description_entry)),
                           thunar_uca_editor_get_icon_name (uca_editor),
                           gtk_entry_get_text (GTK_ENTRY (uca_editor->command_entry)),
                           gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uca_editor->sn_button)),
                           gtk_entry_get_text (GTK_ENTRY (uca_editor->patterns_entry)),
                           thunar_uca_editor_get_types (uca_editor));
}


