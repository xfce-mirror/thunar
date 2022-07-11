/* vi:set et ai sw=2 sts=2 ts=2: */
/*
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
#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-job-operation.h>

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

/* Job operation properties */
enum
{
  PROP_0,
  PROP_OPERATION_KIND,
  PROP_SOURCE_FILE_LIST,
  PROP_TARGET_FILE_LIST,
  N_PROPERTIES,
};

static void                   thunar_job_operation_dispose            (GObject          *object);
static void                   thunar_job_operation_finalize           (GObject          *object);
static void                   thunar_job_operation_get_property       (GObject          *object,
                                                                       guint             prop_id,
                                                                       GValue           *value,
                                                                       GParamSpec       *pspec);
static void                   thunar_job_operation_set_property       (GObject          *object,
                                                                       guint             prop_id,
                                                                       const GValue     *value,
                                                                       GParamSpec       *pspec);
static ThunarJobOperation    *thunar_job_operation_new_invert         (ThunarJobOperation *job_operation);
static void                   thunar_job_operation_execute            (ThunarJobOperation *job_operation);
static gint                   is_ancestor                             (gconstpointer descendant,
                                                                       gconstpointer ancestor);

struct _ThunarJobOperation
{
  GObject __parent__;

  ThunarJobOperationKind operation_kind;
  GList *source_file_list;
  GList *target_file_list;
};

G_DEFINE_TYPE (ThunarJobOperation, thunar_job_operation, G_TYPE_OBJECT)

static GList *job_operation_list = NULL;
static GParamSpec *job_operation_props[N_PROPERTIES] = { NULL, };

static void
thunar_job_operation_class_init (ThunarJobOperationClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_job_operation_dispose;
  gobject_class->finalize = thunar_job_operation_finalize;
  gobject_class->get_property = thunar_job_operation_get_property;
  gobject_class->set_property = thunar_job_operation_set_property;

  /**
   * ThunarJobOperation:operation-kind:
   *
   * The kind of the operation performed.
   */
  job_operation_props[PROP_OPERATION_KIND] =
    g_param_spec_enum ("operation-kind",
                       "Operation kind",
                       "The kind of the operation performed.",
                       THUNAR_TYPE_JOB_OPERATION_KIND,
                       THUNAR_JOB_OPERATION_KIND_COPY,
                       EXO_PARAM_READWRITE);

  /**
   * ThunarJobOperation:source-file-list:
   *
   * Pointer to the GList containing the source files involved in the operation.
   */
  job_operation_props[PROP_SOURCE_FILE_LIST] =
    g_param_spec_pointer ("source-file-list",
                          "Source file list",
                          "Pointer to the GList containing the source files involved in the operation.",
                          EXO_PARAM_READWRITE);

  /**
   * ThunarJobOperation:target-file-list:
   *
   * Pointer to the GList containing the target files involved in the operation.
   */
  job_operation_props[PROP_TARGET_FILE_LIST] =
    g_param_spec_pointer ("target-file-list",
                          "Target file list",
                          "Pointer to the GList containing the target files involved in the operation.",
                          EXO_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, job_operation_props);
}

static void
thunar_job_operation_init (ThunarJobOperation *self)
{
  self->operation_kind = THUNAR_JOB_OPERATION_KIND_COPY;
  self->source_file_list = NULL;
  self->target_file_list = NULL;
}

static void
thunar_job_operation_dispose (GObject *object)
{
  ThunarJobOperation *op;

  op = THUNAR_JOB_OPERATION (object);

  g_list_free_full (op->source_file_list, g_object_unref);
  g_list_free_full (op->target_file_list, g_object_unref);

  (*G_OBJECT_CLASS (thunar_job_operation_parent_class)->dispose) (object);
}

static void
thunar_job_operation_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_job_operation_parent_class)->finalize) (object);
}

static void
thunar_job_operation_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarJobOperation *self = THUNAR_JOB_OPERATION (object);

  switch (prop_id)
    {
    case PROP_OPERATION_KIND:
      g_value_set_enum (value, self->operation_kind);
      break;

    case PROP_SOURCE_FILE_LIST:
      g_value_set_pointer (value, self->source_file_list);
      break;

    case PROP_TARGET_FILE_LIST:
      g_value_set_pointer (value, self->target_file_list);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_job_operation_set_property  (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
  {
    ThunarJobOperation *self = THUNAR_JOB_OPERATION (object);

    switch (prop_id)
      {
      case PROP_OPERATION_KIND:
        self->operation_kind = g_value_get_enum (value);
        break;

      case PROP_SOURCE_FILE_LIST:
        self->source_file_list = g_value_get_pointer (value);
        break;

      case PROP_TARGET_FILE_LIST:
        self->target_file_list = g_value_get_pointer (value);
        break;
  
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
      }
  }


/**
 * thunar_job_operation_new:
 * @kind: The kind of operation being created.
 *
 * Creates a new #ThunarJobOperation of the given kind. Should be unref'd by the caller after use.
 *
 * Return value: the newly created #ThunarJobOperation
 **/
ThunarJobOperation *
thunar_job_operation_new (ThunarJobOperationKind kind)
{
  ThunarJobOperation *operation;

  operation = g_object_new (THUNAR_TYPE_JOB_OPERATION,
                            "operation-kind", kind,
                            NULL);

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

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));
  g_assert (G_IS_FILE (source_file));
  g_assert (G_IS_FILE (target_file));

  /* If the current file is a descendant of any of the already given files,
   * don't register it.
   * Note that source will be the second argument to is_ancestor */
  if (g_list_find_custom (job_operation->source_file_list, source_file, is_ancestor) != NULL)
    return;

  job_operation->source_file_list = g_list_append (job_operation->source_file_list, g_object_ref (source_file));
  job_operation->target_file_list = g_list_append (job_operation->target_file_list, g_object_ref (target_file));
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
  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  /* do not register an 'empty' job operation */
  if (job_operation->source_file_list == NULL && job_operation->target_file_list == NULL)
    return;

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

  /* do nothing in case there is no job operation to undo */
  if (job_operation_list == NULL)
    return;

  /* the 'marked' operation */
  operation_marker = job_operation_list->data;


  inverted_operation = thunar_job_operation_new_invert (operation_marker);
  g_object_ref (inverted_operation);

  thunar_job_operation_execute (inverted_operation);

  g_object_unref (operation_marker);
  g_object_unref (inverted_operation);

  /* set the marked operation to null to now remove the last operation. */
  job_operation_list = NULL;
}

/* thunar_job_operation_new_invert:
 * @job_operation: a #ThunarJobOperation
 *
 * Creates a new job operation which is the inverse of @job_operation.
 *
 * Return value: a newly created #ThunarJobOperation which is the inverse of @job_operation
 **/
ThunarJobOperation *
thunar_job_operation_new_invert (ThunarJobOperation *job_operation)
{
  ThunarJobOperation *inverted_operation;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_COPY:
        inverted_operation = g_object_new (THUNAR_TYPE_JOB_OPERATION,
                                           "operation-kind", THUNAR_JOB_OPERATION_KIND_DELETE,
                                           NULL);
        inverted_operation->source_file_list = thunar_g_list_copy_deep (job_operation->target_file_list);
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
  ThunarFile        *thunar_file;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  application = thunar_application_get ();

  switch (job_operation->operation_kind)
    {
      case THUNAR_JOB_OPERATION_KIND_DELETE:
        for (GList *elem = job_operation->source_file_list; elem != NULL; elem = elem->next)
          {
            g_assert (G_IS_FILE (elem->data));

            thunar_file = thunar_file_get (elem->data, &error);
            g_assert (THUNAR_IS_FILE (thunar_file));

            thunar_file_list = g_list_append (thunar_file_list, thunar_file);

            if (error != NULL)
              {
                g_warning ("Failed to convert GFile to ThunarFile: %s", error->message);
                g_clear_error (&error);
              }
          }

        thunar_application_unlink_files (application, NULL, thunar_file_list, TRUE);

        thunar_g_list_free_full (thunar_file_list);
        break;

      default:
        g_assert_not_reached ();
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

#ifndef NDEBUG /* temporary debugging code */
void
thunar_job_operation_debug_print (void)
{
  ThunarJobOperation *op;
  GValue val = G_VALUE_INIT;
  gint index;

  g_print ("Thunar Job Operation Debug\n"
           "--------------------------\n");

  /* print the last debug operation, if there is any */
  if (job_operation_list == NULL)
  {
    g_print ("No job operation listed so far.\n");
    return;
  }

  op = job_operation_list->data;
  g_assert (THUNAR_IS_JOB_OPERATION (op));

  g_value_init (&val, G_TYPE_ENUM);
  g_object_get_property (G_OBJECT (op), "operation-kind", &val);
  g_print ("operation-kind: %s\n", g_enum_to_string (THUNAR_TYPE_JOB_OPERATION_KIND, g_value_get_enum (&val)));

  index = 0;
  for (GList *elem = op->source_file_list; elem != NULL; index++, elem = elem->next)
    {
      GFile *file = elem->data;
      g_assert (G_IS_FILE (file));
      g_print ("source file %d: %s\n", index, g_file_get_uri (file));
    }

  index = 0;
  for (GList *elem = op->target_file_list; elem != NULL; index++, elem = elem->next)
    {
      GFile *file = elem->data;
      g_assert (G_IS_FILE (file));
      g_print ("target file %d: %s\n", index, g_file_get_uri (file));
    }

  return;
}
#endif /* NDEBUG */
