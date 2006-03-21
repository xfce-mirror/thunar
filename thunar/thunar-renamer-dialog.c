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

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-renamer-model.h>
#include <thunar/thunar-renamer-progress.h>



static void     thunar_renamer_dialog_class_init  (ThunarRenamerDialogClass *klass);
static void     thunar_renamer_dialog_init        (ThunarRenamerDialog      *renamer_dialog);
static void     thunar_renamer_dialog_finalize    (GObject                  *object);
static void     thunar_renamer_dialog_response    (GtkDialog                *dialog,
                                                   gint                      response);
static void     thunar_renamer_dialog_help        (ThunarRenamerDialog      *renamer_dialog);
static void     thunar_renamer_dialog_save        (ThunarRenamerDialog      *renamer_dialog);
static void     thunar_renamer_dialog_notify_page (GtkNotebook              *notebook,
                                                   GParamSpec               *pspec,
                                                   ThunarRenamerDialog      *renamer_dialog);



struct _ThunarRenamerDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarRenamerDialog
{
  ThunarAbstractDialog __parent__;

  ThunarRenamerModel  *model;

  GtkTooltips         *tooltips;

  GtkWidget           *tree_view;
  GtkWidget           *progress;
};



static GObjectClass *thunar_renamer_dialog_parent_class;



GType
thunar_renamer_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarRenamerDialogClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_renamer_dialog_class_init,
        NULL,
        NULL,
        sizeof (ThunarRenamerDialog),
        0,
        (GInstanceInitFunc) thunar_renamer_dialog_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_TYPE_ABSTRACT_DIALOG, I_("ThunarRenamerDialog"), &info, 0);
    }

  return type;
}



static void
thunar_renamer_dialog_class_init (ThunarRenamerDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_renamer_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_renamer_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_renamer_dialog_response;
}



static gint
trd_renamer_compare (gconstpointer a,
                     gconstpointer b)
{
  return g_utf8_collate (thunarx_renamer_get_name (THUNARX_RENAMER (a)), thunarx_renamer_get_name (THUNARX_RENAMER (b)));
}



static void
thunar_renamer_dialog_init (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxProviderFactory *provider_factory;
  GtkTreeViewColumn      *column;
  GtkCellRenderer        *renderer;
  GtkSizeGroup           *size_group;
  const gchar            *active_str;
  GHashTable             *settings;
  GEnumClass             *klass;
  GtkWidget              *notebook;
  GtkWidget              *button;
  GtkWidget              *mcombo;
  GtkWidget              *rcombo;
  GtkWidget              *frame;
  GtkWidget              *image;
  GtkWidget              *label;
  GtkWidget              *hbox;
  GtkWidget              *mbox;
  GtkWidget              *rbox;
  GtkWidget              *vbox;
  GtkWidget              *swin;
  XfceRc                 *rc;
  gchar                 **entries;
  GList                  *providers;
  GList                  *renamers = NULL;
  GList                  *lp;
  gint                    active = 0;
  gint                    n;

  /* allocate a new bulk rename model */
  renamer_dialog->model = thunar_renamer_model_new ();

  /* allocate tooltips for the dialog */
  renamer_dialog->tooltips = gtk_tooltips_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (renamer_dialog->tooltips));

  /* determine the list of available renamers (using the renamer providers) */
  provider_factory = thunarx_provider_factory_get_default ();
  providers = thunarx_provider_factory_list_providers (provider_factory, THUNARX_TYPE_RENAMER_PROVIDER);
  for (lp = providers; lp != NULL; lp = lp->next)
    {
      /* determine the renamers for this provider */
      renamers = g_list_concat (renamers, thunarx_renamer_provider_get_renamers (lp->data));

      /* release this provider */
      g_object_unref (G_OBJECT (lp->data));
    }
  g_object_unref (G_OBJECT (provider_factory));
  g_list_free (providers);

  /* initialize the dialog */
  gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (renamer_dialog), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (renamer_dialog), 510, 490);
  gtk_window_set_title (GTK_WINDOW (renamer_dialog), _("Rename Multiple Files"));

  /* add the "Rename Files" button */
  button = gtk_dialog_add_button (GTK_DIALOG (renamer_dialog), _("_Rename Files"), GTK_RESPONSE_ACCEPT);
  exo_binding_new (G_OBJECT (renamer_dialog->model), "can-rename", G_OBJECT (button), "sensitive");
  gtk_dialog_set_default_response (GTK_DIALOG (renamer_dialog), GTK_RESPONSE_ACCEPT);
  gtk_tooltips_set_tip (renamer_dialog->tooltips, button, _("Click here to actually rename the files listed above to their new names."), NULL);

  /* create the main vbox */
  mbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (mbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (renamer_dialog)->vbox), mbox, TRUE, TRUE, 0);
  gtk_widget_show (mbox);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (mbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  renamer_dialog->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (renamer_dialog->model));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (renamer_dialog->tree_view), THUNAR_RENAMER_MODEL_COLUMN_OLDNAME);
  gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (renamer_dialog->tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (swin), renamer_dialog->tree_view);
  gtk_widget_show (renamer_dialog->tree_view);

  /* create the tree view column for the old file name */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_spacing (column, 2);
  gtk_tree_view_column_set_min_width (column, 100);
  gtk_tree_view_column_set_title (column, _("Name"));
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_fixed_width (column, 225);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  renderer = g_object_new (THUNAR_TYPE_ICON_RENDERER, "size", 16, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer, "file", THUNAR_RENAMER_MODEL_COLUMN_FILE, NULL);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "ellipsize", PANGO_ELLIPSIZE_END, "ellipsize-set", TRUE, NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", THUNAR_RENAMER_MODEL_COLUMN_OLDNAME, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (renamer_dialog->tree_view), column);

  /* create the tree view column for the new file name */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_min_width (column, 75);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_title (column, _("New Name"));
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           "ellipsize-set", TRUE,
                           "foreground", "Red",
                           "weight", PANGO_WEIGHT_SEMIBOLD,
                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "foreground-set", THUNAR_RENAMER_MODEL_COLUMN_CONFLICT,
                                       "text", THUNAR_RENAMER_MODEL_COLUMN_NEWNAME,
                                       "weight-set", THUNAR_RENAMER_MODEL_COLUMN_CONFLICT,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (renamer_dialog->tree_view), column);

  /* create the vbox for the renamer parameters */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (mbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  /* create the hbox for the renamer selection */
  rbox = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), rbox, FALSE, FALSE, 0);
  gtk_widget_show (rbox);

  /* create the rename progress bar */
  renamer_dialog->progress = thunar_renamer_progress_new ();
  gtk_box_pack_start (GTK_BOX (vbox), renamer_dialog->progress, FALSE, FALSE, 0);
  exo_binding_new_with_negation (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (rbox), "visible");
  exo_binding_new_with_negation (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (swin), "sensitive");
  exo_binding_new (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (renamer_dialog->model), "frozen");

  /* synchronize the height of the progress bar and the rbox with the combos */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  gtk_size_group_add_widget (size_group, renamer_dialog->progress);
  gtk_size_group_add_widget (size_group, rbox);
  g_object_unref (G_OBJECT (size_group));

  /* create the frame for the renamer widgets */
  frame = g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_ETCHED_IN, NULL);
  exo_binding_new_with_negation (G_OBJECT (renamer_dialog->progress), "visible", G_OBJECT (frame), "sensitive");
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  /* check if we have any renamers */
  if (G_LIKELY (renamers != NULL))
    {
      /* sort the renamers by name */
      renamers = g_list_sort (renamers, trd_renamer_compare);

      /* open the config file, xfce_rc_config_open() should never return NULL */
      rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/renamerrc", TRUE);
      xfce_rc_set_group (rc, "Configuration");

      /* create the renamer combo box for the renamer selection */
      rcombo = gtk_combo_box_new_text ();
      for (lp = renamers; lp != NULL; lp = lp->next)
        gtk_combo_box_append_text (GTK_COMBO_BOX (rcombo), thunarx_renamer_get_name (lp->data));
      gtk_box_pack_start (GTK_BOX (rbox), rcombo, FALSE, FALSE, 0);
      gtk_widget_show (rcombo);

      /* add a "Help" button */
      button = gtk_button_new ();
      gtk_tooltips_set_tip (renamer_dialog->tooltips, button, _("Click here to view the documentation for the selected rename operation."), NULL);
      g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_renamer_dialog_help), renamer_dialog);
      gtk_box_pack_start (GTK_BOX (rbox), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      image = gtk_image_new_from_stock (GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_widget_show (image);

      /* create the name/suffix/both combo box */
      mcombo = gtk_combo_box_new_text ();
      klass = g_type_class_ref (THUNAR_TYPE_RENAMER_MODE);
      active_str = xfce_rc_read_entry_untranslated (rc, "LastActiveMode", "");
      for (active = 0, n = 0; n < klass->n_values; ++n)
        {
          if (exo_str_is_equal (active_str, klass->values[n].value_name))
            active = n;
          gtk_combo_box_append_text (GTK_COMBO_BOX (mcombo), _(klass->values[n].value_nick));
        }
      exo_mutual_binding_new (G_OBJECT (renamer_dialog->model), "mode", G_OBJECT (mcombo), "active");
      gtk_box_pack_end (GTK_BOX (rbox), mcombo, FALSE, FALSE, 0);
      gtk_combo_box_set_active (GTK_COMBO_BOX (mcombo), active);
      g_type_class_unref (klass);
      gtk_widget_show (mcombo);

      /* allocate the notebook with the renamers */
      notebook = gtk_notebook_new ();
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
      gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
      g_signal_connect (G_OBJECT (notebook), "notify::page", G_CALLBACK (thunar_renamer_dialog_notify_page), renamer_dialog);
      gtk_container_add (GTK_CONTAINER (frame), notebook);
      gtk_widget_show (notebook);

      /* determine the last active renamer (as string) */
      active_str = xfce_rc_read_entry_untranslated (rc, "LastActiveRenamer", "");

      /* add the renamers to the notebook */
      for (active = 0, lp = renamers; lp != NULL; lp = lp->next)
        {
          /* append the renamer to the notebook... */
          gtk_notebook_append_page (GTK_NOTEBOOK (notebook), lp->data, NULL);

          /* check if this page should be active by default */
          if (exo_str_is_equal (G_OBJECT_TYPE_NAME (lp->data), active_str))
            active = g_list_position (renamers, lp);

          /* try to load the settings for the renamer */
          entries = xfce_rc_get_entries (rc, G_OBJECT_TYPE_NAME (lp->data));
          if (G_LIKELY (entries != NULL))
            {
              /* add the settings to a hash table */
              xfce_rc_set_group (rc, G_OBJECT_TYPE_NAME (lp->data));
              settings = g_hash_table_new (g_str_hash, g_str_equal);
              for (n = 0; entries[n] != NULL; ++n)
                g_hash_table_insert (settings, entries[n], (gchar *) xfce_rc_read_entry_untranslated (rc, entries[n], NULL));

              /* let the renamer load the settings */
              thunarx_renamer_load (THUNARX_RENAMER (lp->data), settings);

              /* cleanup */
              g_hash_table_destroy (settings);
              g_strfreev (entries);
            }

          /* ...and make sure its visible */
          gtk_widget_show (lp->data);
        }

      /* default to the first renamer */
      gtk_combo_box_set_active (GTK_COMBO_BOX (rcombo), active);

      /* connect the combo box to the notebook */
      exo_binding_new (G_OBJECT (rcombo), "active", G_OBJECT (notebook), "page");

      /* close the config handle */
      xfce_rc_close (rc);
    }
  else
    {
      /* make the other widgets insensitive */
      gtk_widget_set_sensitive (swin, FALSE);

      /* display an error to the user */
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
      gtk_container_add (GTK_CONTAINER (frame), hbox);
      gtk_widget_show (hbox);

      image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      /* TRANSLATORS: You can test this string by temporarily removing thunar-sbr.* from $libdir/thunarx-1/,
       *              and opening the multi rename dialog by selecting multiple files and pressing F2.
       */
      label = gtk_label_new (_("No renamer modules were found on your system. Please check your\n"
                               "installation or contact your system administrator. If you install Thunar\n"
                               "from source, be sure to enable the \"Simple Builtin Renamers\" plugin."));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
      gtk_label_set_selectable (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
    }

  /* release the renamers list */
  g_list_foreach (renamers, (GFunc) g_object_unref, NULL);
  g_list_free (renamers);
}



static void
thunar_renamer_dialog_finalize (GObject *object)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (object);

  /* release the shared tooltips */
  g_object_unref (G_OBJECT (renamer_dialog->tooltips));

  /* release our bulk rename model */
  g_object_unref (G_OBJECT (renamer_dialog->model));

  (*G_OBJECT_CLASS (thunar_renamer_dialog_parent_class)->finalize) (object);
}



static void
thunar_renamer_dialog_response (GtkDialog *dialog,
                                gint       response)
{
  ThunarRenamerDialog *renamer_dialog = THUNAR_RENAMER_DIALOG (dialog);
  GtkTreeIter          iter;
  ThunarFile          *file;
  GList               *pair_list = NULL;
  gchar               *name;

  /* grab an additional reference on the dialog */
  g_object_ref (G_OBJECT (dialog));

  /* check if we should rename */
  if (G_LIKELY (response == GTK_RESPONSE_ACCEPT))
    {
      /* save the current settings */
      thunar_renamer_dialog_save (renamer_dialog);

      /* display the rename progress bar */
      gtk_widget_show (renamer_dialog->progress);

      /* determine the iterator for the first item */
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (renamer_dialog->model), &iter))
        {
          /* determine the tree row references for the rows that should be renamed */
          do
            {
              /* determine the file and newname for the row */
              gtk_tree_model_get (GTK_TREE_MODEL (renamer_dialog->model), &iter,
                                  THUNAR_RENAMER_MODEL_COLUMN_NEWNAME, &name,
                                  THUNAR_RENAMER_MODEL_COLUMN_FILE, &file,
                                  -1);

              /* check if this row should be renamed */
              if (G_LIKELY (name != NULL && *name != '\0'))
                pair_list = g_list_append (pair_list, thunar_renamer_pair_new (file, name));

              /* cleanup */
              g_object_unref (file);
              g_free (name);
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (renamer_dialog->model), &iter));
        }

      /* perform the rename (returns when done) */
      thunar_renamer_progress_run (THUNAR_RENAMER_PROGRESS (renamer_dialog->progress), pair_list);

      /* hide the rename progress bar */
      gtk_widget_hide (renamer_dialog->progress);

      /* release the pairs */
      thunar_renamer_pair_list_free (pair_list);
    }

  /* hide and destroy the dialog window */
  gtk_widget_hide (GTK_WIDGET (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));

  /* release the additional reference on the dialog */
  g_object_unref (G_OBJECT (dialog));
}



static void
trd_save_foreach (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  xfce_rc_write_entry (user_data, key, value);
}



static void
thunar_renamer_dialog_help (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxRenamer *renamer;
  const gchar    *help_url = NULL;
  GdkScreen      *screen;
  GError         *error = NULL;

  g_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* check if the active renamer provides a help URL */
  renamer = thunar_renamer_model_get_renamer (renamer_dialog->model);
  if (G_LIKELY (renamer != NULL))
    help_url = thunarx_renamer_get_help_url (renamer);

  /* check if we have a specific URL */
  if (G_LIKELY (help_url == NULL))
    {
      /* open the general documentation if no specific URL */
      thunar_dialogs_show_help (GTK_WIDGET (renamer_dialog), "bulk-rename", NULL);
    }
  else
    {
      /* determine the dialog's screen */
      screen = gtk_widget_get_screen (GTK_WIDGET (renamer_dialog));

      /* try to launch the specific URL */
      if (!exo_url_show_on_screen (help_url, NULL, screen, &error))
        {
          /* tell the user that we failed */
          thunar_dialogs_show_error (GTK_WIDGET (renamer_dialog), error, _("Failed to open the documentation browser"));

          /* release the error */
          g_error_free (error);
        }
    }
}



static void
thunar_renamer_dialog_save (ThunarRenamerDialog *renamer_dialog)
{
  ThunarxRenamer *renamer;
  GHashTable     *settings;
  GEnumClass     *klass;
  GEnumValue     *value;
  XfceRc         *rc;

  g_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));

  /* try to open the config handle for writing */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/renamerrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    return;

  /* determine the currently active renamer */
  renamer = thunar_renamer_model_get_renamer (renamer_dialog->model);
  if (G_LIKELY (renamer != NULL))
    {
      /* save general settings first */
      xfce_rc_set_group (rc, "Configuration");

      /* remember current mode as last active */
      klass = g_type_class_ref (THUNAR_TYPE_RENAMER_MODE);
      value = g_enum_get_value (klass, thunar_renamer_model_get_mode (renamer_dialog->model));
      if (G_LIKELY (value != NULL))
        xfce_rc_write_entry (rc, "LastActiveMode", value->value_name);
      g_type_class_unref (klass);

      /* remember renamer as last active */
      xfce_rc_write_entry (rc, "LastActiveRenamer", G_OBJECT_TYPE_NAME (renamer));

      /* save the renamer's settings */
      settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      xfce_rc_delete_group (rc, G_OBJECT_TYPE_NAME (renamer), TRUE);
      xfce_rc_set_group (rc, G_OBJECT_TYPE_NAME (renamer));
      thunarx_renamer_save (renamer, settings);
      g_hash_table_foreach (settings, trd_save_foreach, rc);
      g_hash_table_destroy (settings);
    }

  /* close the config handle */
  xfce_rc_close (rc);
}



static void
thunar_renamer_dialog_notify_page (GtkNotebook         *notebook,
                                   GParamSpec          *pspec,
                                   ThunarRenamerDialog *renamer_dialog)
{
  GtkWidget *renamer;
  gint       page;

  g_return_if_fail (THUNAR_IS_RENAMER_DIALOG (renamer_dialog));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* determine the currently active renamer page */
  page = gtk_notebook_get_current_page (notebook);
  if (G_LIKELY (page >= 0))
    {
      /* determine the renamer for that page... */
      renamer = gtk_notebook_get_nth_page (notebook, page);

      /* ...and set that renamer for the model */
      thunar_renamer_model_set_renamer (renamer_dialog->model, THUNARX_RENAMER (renamer));
    }
}



/**
 * thunar_renamer_dialog_new:
 *
 * Allocates a new #ThunarRenamerDialog instance.
 *
 * Return value: the newly allocated #ThunarRenamerDialog
 *               instance.
 **/
GtkWidget*
thunar_renamer_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_RENAMER_DIALOG, NULL);
}



/**
 * thunar_show_renamer_dialog:
 * @parent : the #GtkWidget or the #GdkScreen on which to open the
 *           dialog. May also be %NULL in which case the default
 *           #GdkScreen will be used.
 * @files  : the list of #ThunarFile<!---->s to rename.
 *
 * Convenience function to display a #ThunarRenamerDialog with
 * the given parameters.
 **/
void
thunar_show_renamer_dialog (gpointer parent,
                            GList   *files)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GtkWidget         *window = NULL;
  GList             *lp;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the bulk rename dialog */
  dialog = g_object_new (THUNAR_TYPE_RENAMER_DIALOG,
                         "screen", screen,
                         NULL);

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && GTK_WIDGET_TOPLEVEL (window)))
    {
      /* dialog is transient for toplevel window, but not modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* append all specified files to the dialog's model */
  for (lp = files; lp != NULL; lp = lp->next)
    thunar_renamer_model_append (THUNAR_RENAMER_DIALOG (dialog)->model, lp->data);

  /* let the application handle the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* display the dialog */
  gtk_widget_show (dialog);
}


