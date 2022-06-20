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
#include <thunar/thunar-job-operation.h>

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

struct _ThunarJobOperation
{
  GObject __parent__;

  ThunarJobOperationKind operation_kind;
  GList *source_file_list;
  GList *target_file_list;
};

G_DEFINE_TYPE (ThunarJobOperation, thunar_job_operation, G_TYPE_OBJECT)

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
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * ThunarJobOperation:target-file-list:
   *
   * Pointer to the GList containing the target files involved in the operation.
   */
  job_operation_props[PROP_TARGET_FILE_LIST] =
    g_param_spec_pointer ("target-file-list",
                          "Target file list",
                          "Pointer to the GList containing the target files involved in the operation.",
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

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
        g_free (self->source_file_list);
        self->source_file_list = g_value_get_pointer (value);
        break;

      case PROP_TARGET_FILE_LIST:
        g_free (self->target_file_list);
        self->target_file_list = g_value_get_pointer (value);
        break;
  
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
      }
  }

void
thunar_job_operation_register (ThunarJobOperationKind operation_kind,
                               GList                 *source_file_list,
                               GList                 *target_file_list)
{
  ThunarApplication     *application;
  ThunarJobOperation    *operation;
  GList                 *operation_list;

#ifndef NDEBUG /*temporary testing code */
  g_print ("thunar_job_operation_register\n"
           "-----------------------------\n");
  /* g_print ("source_file_list base glist pointer address: %p\n", source_file_list); */
  for (GList *elem = source_file_list; elem; elem = elem->next)
  {
    g_assert (G_IS_FILE (elem->data));
    g_print ("source file pointer address: %p\n", elem->data);
  }

  /* g_print ("target_file_list base glist pointer address: %p\n", target_file_list); */
  for (GList *elem = target_file_list; elem; elem = elem->next)
  {
    g_assert (G_IS_FILE (elem->data));
    g_print ("target file pointer address: %p\n", elem->data);
  }

  g_print ("------------------------------\n");
#endif

  application = thunar_application_get ();

  operation = g_object_new (THUNAR_TYPE_JOB_OPERATION,
                            "operation-kind", operation_kind,
                            "source-file-list", source_file_list,
                            "target-file-list", target_file_list,
                            NULL);

  /* Returns NULL for an unknown key , which is itself a valid empty list. */
  operation_list = g_object_get_data (G_OBJECT (application), "thunar-job-operation-list");
  operation_list = g_list_prepend (operation_list, operation);

  g_object_set_data (G_OBJECT (application), "thunar-job-operation-list", operation_list);
}

GList *
thunar_job_operation_get_current_list ()
{
  ThunarApplication     *application;
  GList                 *operation_list;

  application = thunar_application_get ();
  operation_list = g_object_get_data (G_OBJECT (application), "thunar-job-operation-list");

  return operation_list;
}
