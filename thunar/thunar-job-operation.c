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

#include <thunar/thunar-job-operation.h>
#include <thunar/thunar-enum-types.h>

/* Job operation properties */
enum
{
  PROP_0,
  PROP_OPERATION_KIND,
  PROP_SOURCE_FILE_LIST,
  PROP_TARGET_FILE_LIST,
  N_PROPERTIES,
};

static void             thunar_job_operation_finalize           (GObject          *object);
static void             thunar_job_operation_get_property       (GObject          *object,
                                                                 guint             prop_id,
                                                                 GValue           *value,
                                                                 GParamSpec       *pspec);
static void             thunar_job_operation_set_property       (GObject          *object,
                                                                 guint             prop_id,
                                                                 const GValue     *value,
                                                                 GParamSpec       *pspec);
static GList           *get_source_file_list                    (ThunarJobOperation *job_operation);
static void             set_source_file_list                    (ThunarJobOperation *job_operation,
                                                                 GList              *source_file_list);
static GList           *get_target_file_list                    (ThunarJobOperation *job_operation);
static void             set_target_file_list                    (ThunarJobOperation *job_operation,
                                                                 GList              *target_file_list);

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
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /* TODO:
   * Change to enum from string
   * */

  /**
   * ThunarJobOperation:source-file-list:
   *
   * Pointer to the GList containing the source files involved in the operation.
   */
  job_operation_props[PROP_SOURCE_FILE_LIST] =
    g_param_spec_pointer ("source-file-list",
                          "Source file list",
                          "Pointer to the GList containing the source files involved in the operation.",
                          G_PARAM_READWRITE);

  /**
   * ThunarJobOperation:target-file-list:
   *
   * Pointer to the GList containing the target files involved in the operation.
   */
  job_operation_props[PROP_TARGET_FILE_LIST] =
    g_param_spec_pointer ("target-file-list",
                          "Target file list",
                          "Pointer to the GList containing the target files involved in the operation.",
                          G_PARAM_READWRITE);

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
thunar_job_operation_finalize (GObject *object)
{

#ifndef NDEBUG /* temporary debugging code */
  g_print ("THUNAR JOB OPERATION FINALIZED!\n");
  g_print ("Operation which is finalized: %p\n", object);
#endif

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

static GList *
get_source_file_list (ThunarJobOperation *job_operation)
{
  GList *source_file_list;
  GValue val = G_VALUE_INIT;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  g_value_init (&val, G_TYPE_POINTER);

  g_object_get_property (G_OBJECT (job_operation), "source-file-list", &val);
  source_file_list = g_value_get_pointer (&val);

  return source_file_list;
}

static void
set_source_file_list (ThunarJobOperation *job_operation,
                      GList              *source_file_list)
{
  GValue val = G_VALUE_INIT;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  for (GList *elem = source_file_list; elem != NULL; elem = elem->next)
    g_assert (G_IS_FILE (elem->data));

  g_value_init (&val, G_TYPE_POINTER);
  g_value_set_pointer (&val, source_file_list);

  g_object_set_property (G_OBJECT (job_operation), "source-file-list", &val);
}

static GList *
get_target_file_list (ThunarJobOperation *job_operation)
{
  GList *target_file_list;
  GValue val = G_VALUE_INIT;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  g_value_init (&val, G_TYPE_POINTER);

  g_object_get_property (G_OBJECT (job_operation), "target-file-list", &val);
  target_file_list = g_value_get_pointer (&val);

  return target_file_list;
}

static void
set_target_file_list (ThunarJobOperation *job_operation,
                      GList              *target_file_list)
{
  GValue val = G_VALUE_INIT;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));

  for (GList *elem = target_file_list; elem != NULL; elem = elem->next)
    g_assert (G_IS_FILE (elem->data));

  g_value_init (&val, G_TYPE_POINTER);
  g_value_set_pointer (&val, target_file_list);

  g_object_set_property (G_OBJECT (job_operation), "target-file-list", &val);
}

ThunarJobOperation *
thunar_job_operation_new (ThunarJobOperationKind kind)
{
  ThunarJobOperation *operation;

  operation = g_object_new (THUNAR_TYPE_JOB_OPERATION,
                            "operation-kind", kind,
                            NULL);

  return operation;
}

void
thunar_job_operation_append (ThunarJobOperation *job_operation,
                             GFile              *source,
                             GFile              *target)
{
  GList *source_file_list;
  GList *target_file_list;

  g_assert (THUNAR_IS_JOB_OPERATION (job_operation));
  g_assert (G_IS_FILE (source));
  g_assert (G_IS_FILE (target));

  source_file_list = get_source_file_list (job_operation);
  source_file_list = g_list_append (source_file_list, source);
  set_source_file_list (job_operation, source_file_list);

  target_file_list = get_target_file_list (job_operation);
  target_file_list = g_list_append (target_file_list, target);
  set_target_file_list (job_operation, target_file_list);
}

void
thunar_job_operation_finish (ThunarJobOperation *job_operation)
{
  GList *source_file_list;
  GList *target_file_list;;

  source_file_list = get_source_file_list (job_operation);
  target_file_list = get_target_file_list (job_operation);

  /* do not register an 'empty' job operation */
  if (source_file_list == NULL && target_file_list == NULL)
    return;

  job_operation_list = g_list_append (NULL, g_object_ref (job_operation));
}

#ifndef NDEBUG /* temporary debugging code */
void
thunar_job_operation_debug_print (void)
{
  ThunarJobOperation *op;
  GValue val = G_VALUE_INIT;
  GList *source_file_list;
  GList *target_file_list;
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

  g_print ("job_operation memory address: %p\n", op);

  g_value_init (&val, G_TYPE_ENUM);
  g_object_get_property (G_OBJECT (op), "operation-kind", &val);
  g_print ("operation-kind: %s\n", g_enum_to_string (THUNAR_TYPE_JOB_OPERATION_KIND, g_value_get_enum (&val)));

  source_file_list = get_source_file_list (op);
  g_print ("source_file_list memory address: %p\n", source_file_list);
  index = 0;

  for (GList *elem = source_file_list; elem != NULL; index++, elem = elem->next)
    {
      GFile *file = elem->data;
      g_print ("source file %d's memory address: %p\n", index, file);
      g_assert (G_IS_FILE (file));
      g_print ("source file %d: %s\n", index, g_file_get_uri (file));
    }

  target_file_list = get_target_file_list (op);
  g_print ("target_file_list memory address: %p\n", target_file_list);
  index = 0;
  for (GList *elem = target_file_list; elem != NULL; index++, elem = elem->next)
    {
      GFile *file = elem->data;
      g_print ("target file %d's memory address: %p\n", index, file);
      g_assert (G_IS_FILE (file));
      g_print ("target file %d: %s\n", index, g_file_get_uri (file));
    }

  return;
}
#endif /* NDEBUG */
