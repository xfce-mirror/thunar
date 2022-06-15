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

struct _ThunarJobOperation
{
  GObject __parent__;

  ThunarJobType  type;
  GList         *source_file_list;
  GList         *target_file_list;
};

G_DEFINE_TYPE (ThunarJobOperation, thunar_job_operation, G_TYPE_OBJECT)

static void
thunar_job_operation_class_init (ThunarJobOperationClass *klass)
{
}

static void
thunar_job_operation_init (ThunarJobOperation *self)
{
  self->type = NONE;
  self->source_file_list = NULL;
  self->target_file_list = NULL;
}
