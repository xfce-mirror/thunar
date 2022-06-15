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

/* Job operation properties */
enum
{
  PROP_0,
  PROP_OPERATION_TYPE,
  N_PROPERTIES,
};

static void     thunar_job_operation_finalize           (GObject          *object);
static void     thunar_job_operation_get_property       (GObject          *object,
                                                         guint             prop_id,
                                                         GValue           *value,
                                                         GParamSpec       *pspec);
static void     thunar_job_operation_set_property       (GObject          *object,
                                                         guint             prop_id,
                                                         const GValue     *value,
                                                         GParamSpec       *pspec);

struct _ThunarJobOperation
{
  GObject __parent__;

  gchar *operation_type;
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
   * ThunarJobOperation:operation-type:
   *
   * The type of the operation performed.
   */
  job_operation_props[PROP_OPERATION_TYPE] =
    g_param_spec_string ("operation-type",
                         "Operation type",
                         "The type of the operation performed.",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE);
  /* DOUBT:
   * Should this be an enum instead of a string? Since we want it to be limited
   * to a specific set of values.
   * Raises question about where to put the enum definition, and what it should be named.
   * */

  g_object_class_install_properties (gobject_class, N_PROPERTIES, job_operation_props);
}

static void
thunar_job_operation_init (ThunarJobOperation *self)
{
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
    case PROP_OPERATION_TYPE:
      g_value_set_string (value, self->operation_type);
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
      case PROP_OPERATION_TYPE:
        g_free (self->operation_type);
        self->operation_type = g_value_dup_string (value);
        break;
  
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
      }
  }
