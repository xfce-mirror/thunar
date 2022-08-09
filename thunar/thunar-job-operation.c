/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-job-operation.h>
#include <thunar/thunar-private.h>

/**
 * SECTION:thunar-job-operation
 * @Short_description: Manages the logging of job operations (copy, move etc.) and undoing and redoing them
 * @Title: ThunarJobOperation
 *
 * The #ThunarJobOperation class represents a single 'job operation', a file operation like copying, moving
 * etc. that can be logged centrally and undone.
 *
 * The @job_operation_list is a #GList of such job operations. It is not necessary that @job_operation_list
 * points to the head of the list; it points to the 'marked operation', the operation that reflects
 * the latest state of the operation history.
 * Usually, this will be the latest performed operation, which hasn't been undone yet.
 */

static void                   thunar_job_operation_dispose            (GObject            *object);
static void                   thunar_job_operation_finalize           (GObject            *object);
static ThunarJobOperation    *thunar_job_operation_new_invert         (ThunarJobOperation *job_operation);
static void                   thunar_job_operation_execute            (ThunarJobOperation *job_operation);
static gint                   is_ancestor                             (gconstpointer       descendant,
                                                                       gconstpointer       ancestor);



struct _ThunarJobOperation
{
  GObject                 __parent__;

  ThunarJobOperationKind  operation_kind;
  GList                  *source_file_list;
  GList                  *target_file_list;
  GList                  *overwritten_file_list;
};

G_DEFINE_TYPE (ThunarJobOperation, thunar_job_operation, G_TYPE_OBJECT)

static GList *job_operation_list = NULL;



static void
thunar_job_operation_class_init (ThunarJobOperationClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_job_operation_dispose;
  gobject_class->finalize = thunar_job_operation_finalize;
}



static void
thunar_job_operation_init (ThunarJobOperation *self)
{
  self->operation_kind = THUNAR_JOB_OPERATION_KIND_COPY;
  self->source_file_list = NULL;
  self->target_file_list = NULL;
  self->overwritten_file_list = NULL;
}



static void
thunar_job_operation_dispose (GObject *object)
{
  ThunarJobOperation *op;

  op = THUNAR_JOB_OPERATION (object);

  g_list_free_full (op->source_file_list, g_object_unref);
  g_list_free_full (op->target_file_list, g_object_unref);
  g_list_free_full (op->overwritten_file_list, g_object_unref);

  (*G_OBJECT_CLASS (thunar_job_operation_parent_class)->dispose) (object);
}



static void
thunar_job_operation_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_job_operation_parent_class)->finalize) (object);
}



/**
 * thunar_job_operation_new:
 * @kind: The kind of operation being created.
 *
 * Creates a new #ThunarJobOperation of the given kind. Should be unref'd by the caller after use.
 *
 * Return value: (transfer full): the newly created #ThunarJobOperation
 **/
ThunarJobOperation *
thunar_job_operation_new (ThunarJobOperationKind kind)
{
  ThunarJobOperation *operation;

  operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
  operation->operation_kind = kind;

  return operation;
}



/**
 * thunar_job_operation_add:
 * @job_operation: a #ThunarJobOperation
 * @source_file:   a #GFile representing the source file
 * @target_file:   a #GFile representing the target file
 *
 * Adds the specified @source_file-@target_file pair to the given job operation.
 **/
void
thunar_job_operation_add (ThunarJobOperation *job_operation,
                          GFile              *source_file,
                          GFile              *target_file)
{

  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));
  _thunar_return_if_fail (G_IS_FILE (source_file));
  _thunar_return_if_fail (G_IS_FILE (target_file));

  /* When a directory has a file operation applied to it (for e.g. deletion),
   * the operation will also automatically get applied to its descendants.
   * If the descendant of a that directory is then found, it will try to apply the operation
   * to it again then, meaning the operation is attempted multiple times on the same file.
   *
   * So to avoid such issues on executing a job operation, if the source file is
   * a descendant of an existing file, do not add it to the job operation. */
  if (g_list_find_custom (job_operation->source_file_list, source_file, is_ancestor) != NULL)
    return;

  job_operation->source_file_list = g_list_append (job_operation->source_file_list, g_object_ref (source_file));
  job_operation->target_file_list = g_list_append (job_operation->target_file_list, g_object_ref (target_file));
}



/***
 * thunar_job_operation_overwrite:
 * @job_operation:    a #ThunarJobOperation
 * @overwritten_file: a #GFile representing the file that has been overwritten
 *
 * Logs a file overwritten as a part of an operation.
 **/
void
thunar_job_operation_overwrite (ThunarJobOperation *job_operation,
                                GFile              *overwritten_file)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  job_operation->overwritten_file_list = thunar_g_list_append_deep (job_operation->overwritten_file_list, overwritten_file);
}



/**
 * thunar_job_operation_commit:
 * @job_operation: a #ThunarJobOperation
 *
 * Commits, or registers, the given thunar_job_operation, adding the job operation
 * to the job operation list.
 **/
void
thunar_job_operation_commit (ThunarJobOperation *job_operation)
{
  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  /* We only keep one job operation commited in the job operation list, so we have to free the
   * memory for the job operation in the list, if any, stored in before we commit the new one. */
  thunar_g_list_free_full (job_operation_list);
  job_operation_list = g_list_append (NULL, g_object_ref (job_operation));
}



/**
 * thunar_job_operation_undo:
 *
 * Undoes the job operation marked by the job operation list. First the marked job operation
 * is retreived, then its inverse operation is calculated, and finally this inverse operation
 * is executed.
 **/
void
thunar_job_operation_undo (void)
{
  ThunarJobOperation *operation_marker;
  ThunarJobOperation *inverted_operation;
  GEnumClass         *enum_class;
  GEnumValue         *enum_value;
  GString            *warning_body;
  gchar              *file_uri;

  /* do nothing in case there is no job operation to undo */
  if (job_operation_list == NULL)
    {
      xfce_dialog_show_warning (NULL,
                                _("No operation has been performed yet that can be undone."),
                                _("There is no operation to be undone"));
      return;
    }

  /* the 'marked' operation */
  operation_marker = job_operation_list->data;

  /* the enum value of the operation kind, which will be used to get its nick name */
  enum_class = g_type_class_ref (THUNAR_TYPE_JOB_OPERATION_KIND);
  enum_value = g_enum_get_value (enum_class, operation_marker->operation_kind);

  /* warn the user if the previous operation is empty, since then there is nothing to undo */
  if (operation_marker->source_file_list == NULL && operation_marker->target_file_list == NULL)
    {

      xfce_dialog_show_warning (NULL,
                                _("The operation you are trying to undo does not have any files "
                                  "associated with it, and thus cannot be undone. "),
                                _("%s operation cannot be undone"), enum_value->value_nick);
    }
  else
    {
      /* if there were files overwritten in the operation, warn about them */
      if (operation_marker->overwritten_file_list != NULL)
        {
          gint index;

          index = 1; /* one indexed for the dialog */
          warning_body = g_string_new (_("The following files were overwritten in the operation "
                                         "you are trying to undo and cannot be restored:\n"));

          for (GList *lp = operation_marker->overwritten_file_list; lp != NULL; lp = lp->next, index++)
            {
              file_uri = g_file_get_uri (lp->data);
              g_string_append_printf (warning_body, "%d. %s\n", index, file_uri);
              g_free (file_uri);
            }

          xfce_dialog_show_warning (NULL,
                                    warning_body->str,
                                    _("%s operation can only be partially undone"), enum_value->value_nick);

          g_string_free (warning_body, TRUE);
        }

      inverted_operation = thunar_job_operation_new_invert (operation_marker);
      thunar_job_operation_execute (inverted_operation);
      g_object_unref (inverted_operation);
    }

  /* Completely clear the job operation list on undo, this is because we only store the single
   * most recent operation, and we do not want it to be available to undo *again* after it has
   * already been undone once. */
  thunar_g_list_free_full (job_operation_list);
  job_operation_list = NULL;
}



/* thunar_job_operation_can_undo:
 *
 * Returns whether or not there is an operation on the job operation list that can be undone.
 **/
gboolean
thunar_job_operation_can_undo (void)
{
  ThunarJobOperation *operation_marker;

  if (job_operation_list == NULL)
    return FALSE;
 
  operation_marker = job_operation_list->data;

  if (operation_marker->source_file_list == NULL && operation_marker->target_file_list == NULL)
    return FALSE;

  return TRUE;
}



/* thunar_job_operation_new_invert:
 * @job_operation: a #ThunarJobOperation
 *
 * Creates a new job operation which is the inverse of @job_operation.
 * Should be unref'd by the caller after use.
 *
 * Return value: (transfer full): a newly created #ThunarJobOperation which is the inverse of @job_operation
 **/
ThunarJobOperation *
thunar_job_operation_new_invert (ThunarJobOperation *job_operation)
{
  ThunarJobOperation *inverted_operation;

  _thunar_return_val_if_fail (THUNAR_IS_JOB_OPERATION (job_operation), NULL);

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_COPY:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_DELETE;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_MOVE:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_MOVE;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_RENAME:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION, NULL);
        inverted_operation->operation_kind = THUNAR_JOB_OPERATION_KIND_RENAME;
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
        inverted_operation->target_file_list = thunar_g_list_copy_deep (job_operation->source_file_list);
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return inverted_operation;
}



/* thunar_job_operation_execute:
 * @job_operation: a #ThunarJobOperation
 *
 * Executes the given @job_operation, depending on what kind of an operation it is.
 **/
void
thunar_job_operation_execute (ThunarJobOperation *job_operation)
{
  ThunarApplication *application;
  GList             *thunar_file_list = NULL;
  GError            *error            = NULL;
  ThunarJob         *job              = NULL;
  ThunarFile        *thunar_file;
  GFile             *parent_dir;
  gchar             *display_name;

  _thunar_return_if_fail (THUNAR_IS_JOB_OPERATION (job_operation));

  application = thunar_application_get ();

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_DELETE:
        for (GList *lp = job_operation->source_file_list; lp != NULL; lp = lp->next)
          {
            if (!G_IS_FILE (lp->data))
              {
                g_warning ("One of the files in the job operation list was not a valid GFile");
                continue;
              }

            thunar_file = thunar_file_get (lp->data, &error);

            if (error != NULL)
              {
                g_warning ("Failed to convert GFile to ThunarFile: %s", error->message);
                g_clear_error (&error);
              }

            if (!THUNAR_IS_FILE (thunar_file))
              {
                g_warning ("One of the files in the job operation list did not convert to a valid ThunarFile");
                continue;
              }

            thunar_file_list = g_list_append (thunar_file_list, thunar_file);
          }

        thunar_application_unlink_files (application, NULL, thunar_file_list, TRUE);

        thunar_g_list_free_full (thunar_file_list);
        break;

      case THUNAR_JOB_OPERATION_KIND_MOVE:
        /* ensure that all the targets have parent directories which exist */
        for (GList *lp = job_operation->target_file_list; lp != NULL; lp = lp->next)
          {
            parent_dir = g_file_get_parent (lp->data);
            g_file_make_directory_with_parents (parent_dir, NULL, &error);
            g_object_unref (parent_dir);

            if (error != NULL)
            {
              /* there is no issue if the target directory already exists */
              if (error->code == G_IO_ERROR_EXISTS)
                {
                  g_clear_error (&error);
                  continue;
                }

              /* output the error message to console otherwise and abort */
              g_warning ("Error while moving files: %s\n"
                         "Aborting operation\n",
                         error->message);
              g_clear_error (&error);
              g_object_unref (application);
              return;
            }
          }

        thunar_application_move_files (application, NULL,
                                       job_operation->source_file_list, job_operation->target_file_list,
                                       THUNAR_OPERATION_LOG_NO_OPERATIONS, NULL);
        break;

      case THUNAR_JOB_OPERATION_KIND_RENAME:
        for (GList *slp = job_operation->source_file_list, *tlp = job_operation->target_file_list;
             slp != NULL && tlp != NULL;
             slp = slp->next, tlp = tlp->next)
          {
            display_name = thunar_g_file_get_display_name (tlp->data);
            thunar_file = thunar_file_get (slp->data, &error);

            if (error != NULL)
              {
                g_warning ("Error while renaming files: %s\n", error->message);
                g_clear_error (&error);

                g_free (display_name);
                g_object_unref (thunar_file);

                continue;
              }

            job = thunar_io_jobs_rename_file (thunar_file, display_name, THUNAR_OPERATION_LOG_NO_OPERATIONS);
            exo_job_launch (EXO_JOB (job));

            g_free (display_name);
            g_object_unref (thunar_file);
          }
        break;

      default:
        _thunar_assert_not_reached ();
        break;
    }

  g_object_unref (application);
}



/* is_ancestor:
 * @ancestor:     potential ancestor of @descendant. A #GFile
 * @descendant:   potential descendant of @ancestor. A #GFile
 *
 * Helper function for #g_list_find_custom.
 *
 * Return value: %0 if @ancestor is actually the ancestor of @descendant,
 *               %1 otherwise
 **/
static gint
is_ancestor (gconstpointer ancestor,
             gconstpointer descendant)
{
  if (thunar_g_file_is_descendant (G_FILE (descendant), G_FILE (ancestor)))
    return 0;
  else
    return 1;
}
