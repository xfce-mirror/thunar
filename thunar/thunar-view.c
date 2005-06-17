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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-view.h>



enum
{
  FILE_ACTIVATED,
  FILE_SELECTION_CHANGED,
  LAST_SIGNAL,
};



static void thunar_view_base_init  (gpointer klass);
static void thunar_view_class_init (gpointer klass);



static guint view_signals[LAST_SIGNAL];



GType
thunar_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarViewIface),
        (GBaseInitFunc) thunar_view_base_init,
        NULL,
        (GClassInitFunc) thunar_view_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
                                     "ThunarView",
                                     &info, 0);

      g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
    }

  return type;
}



static void
thunar_view_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized))
    {
      /**
       * ThunarView:file-actived:
       * @view : a #ThunarView instance.
       * @file : a #ThunarFile instance.
       *
       * Emitted from the @view whenever the user activates a @file that
       * is currently being displayed within the @view, for example by
       * double-clicking the icon representation of @file or pressing
       * Return while the @file is selected.
       **/
      view_signals[FILE_ACTIVATED] =
        g_signal_new ("file-activated",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarViewIface, file_activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, THUNAR_TYPE_FILE);


      /**
       * ThunarView:file-selection-changed:
       * @view : a #ThunarView instance.
       *
       * Emitted by @view whenever the set of currently selected files
       * changes.
       **/
      view_signals[FILE_SELECTION_CHANGED] =
        g_signal_new ("file-selection-changed",
                      G_TYPE_FROM_INTERFACE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarViewIface, file_selection_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}



static void
thunar_view_class_init (gpointer klass)
{
  /**
   * ThunarView:model:
   *
   * The #ThunarListModel currently displayed by this #ThunarView. If
   * the property is %NULL, nothing should be displayed.
   **/
  g_object_interface_install_property (klass,
                                       g_param_spec_object ("model",
                                                            _("Model"),
                                                            _("The model currently displayed by this view"),
                                                            THUNAR_TYPE_LIST_MODEL,
                                                            EXO_PARAM_READWRITE));
}



/**
 * thunar_view_get_model:
 * @view : a #ThunarView instance.
 *
 * Returns the #ThunarListModel currently displayed by the @view or %NULL
 * if @view does not display any model currently.
 *
 * Return value: the currently displayed model of @view or %NULL.
 **/
ThunarListModel*
thunar_view_get_model (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return THUNAR_VIEW_GET_IFACE (view)->get_model (view);
}



/**
 * thunar_view_set_model:
 * @view  : a #ThunarView instance.
 * @model : the new directory to display or %NULL.
 *
 * Instructs @view to display the folder associated with 
 * @current_directory. If @current_directory is %NULL, the @view
 * should display nothing.
 **/
void
thunar_view_set_model (ThunarView      *view,
                       ThunarListModel *model)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (model == NULL || THUNAR_IS_LIST_MODEL (model));
  THUNAR_VIEW_GET_IFACE (view)->set_model (view, model);
}



/**
 * thunar_view_get_selected_files:
 * @view : a #ThunarView instance.
 *
 * Returns the list of currently selected files in @view. The elements
 * in the returned list are instance of #ThunarFile. The return list
 * will be empty if no files are selected in @view.
 *
 * The caller is responsible for freeing the returned list, which
 * can be done like this:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc)g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: a #GList containing a #ThunarFile for each selected item.
 **/
GList*
thunar_view_get_selected_files (ThunarView *view)
{
  ThunarListModel *model;
  GtkTreeIter      iter;
  GList           *list;
  GList           *lp;

  list = thunar_view_get_selected_items (view);
  if (list != NULL)
    {
      /* We use a simple trick here to avoid creating a second list and free'ing the
       * items list: We simply determine the iterator for the path at the given list
       * position, then free the path data in the list and replace it with the file
       * corresponding to the previously stored path.
       */
      model = thunar_view_get_model (view);
      for (lp = list; lp != NULL; lp = lp->next)
        {
          /* determine the tree iterator for the given path */
          gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, lp->data);
          gtk_tree_path_free (lp->data);

          /* replace the path with the file */
          lp->data = thunar_list_model_get_file (model, &iter);
        }
    }

  return list;
}



/**
 * thunar_view_get_selected_items:
 * @view : a #ThunarView instance.
 *
 * Creates a list of #GtkTreePath<!---->s of all selected items. Additionally, if
 * you are planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReferences. To do
 * this, you can use #gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: a #GList containing a #GtkTreePath for each selected row.
 **/
GList*
thunar_view_get_selected_items (ThunarView *view)
{
  g_return_val_if_fail (THUNAR_IS_VIEW (view), NULL);
  return THUNAR_VIEW_GET_IFACE (view)->get_selected_items (view);
}



/**
 * thunar_view_file_activated:
 * @view : a #ThunarView instance.
 * @file : a #ThunarFile instance.
 *
 * Emits the ::file-activated signal on @view with the specified
 * @file.
 *
 * This function is only meant to be used by #ThunarView
 * implementations.
 **/
void
thunar_view_file_activated (ThunarView *view,
                            ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));
  g_return_if_fail (THUNAR_IS_FILE (file));

  g_signal_emit (G_OBJECT (view), view_signals[FILE_ACTIVATED], 0, file);
}



/**
 * thunar_view_file_selection_changed:
 * @view : a #ThunarView instance.
 *
 * Emits the ::file-selection-changed signal on @view.
 *
 * This function is only meant to be used by #ThunarView
 * implementations.
 **/
void
thunar_view_file_selection_changed (ThunarView *view)
{
  g_return_if_fail (THUNAR_IS_VIEW (view));

  g_signal_emit (G_OBJECT (view), view_signals[FILE_SELECTION_CHANGED], 0);
}


