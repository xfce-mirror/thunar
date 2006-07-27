/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-interactive-job.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-simple-job.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#include <gobject/gvaluecollector.h>



static void thunar_vfs_simple_job_class_init (ThunarVfsSimpleJobClass *klass);
static void thunar_vfs_simple_job_finalize   (GObject                 *object);
static void thunar_vfs_simple_job_execute    (ThunarVfsJob            *job);



struct _ThunarVfsSimpleJobClass
{
  ThunarVfsInteractiveJobClass __parent__;
};

struct _ThunarVfsSimpleJob
{
  ThunarVfsInteractiveJob __parent__;
  ThunarVfsSimpleJobFunc  func;
  GValue                 *param_values;
  guint                   n_param_values;
};



static GObjectClass *thunar_vfs_simple_job_parent_class;



GType
thunar_vfs_simple_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (THUNAR_VFS_TYPE_INTERACTIVE_JOB, 
                                                 "ThunarVfsSimpleJob",
                                                 sizeof (ThunarVfsSimpleJobClass),
                                                 thunar_vfs_simple_job_class_init,
                                                 sizeof (ThunarVfsSimpleJob),
                                                 NULL, 0);
    }

  return type;
}



static void
thunar_vfs_simple_job_class_init (ThunarVfsSimpleJobClass *klass)
{
  ThunarVfsJobClass *thunarvfs_job_class;
  GObjectClass      *gobject_class;

  /* determine the parent type class */
  thunar_vfs_simple_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_simple_job_finalize;

  thunarvfs_job_class = THUNAR_VFS_JOB_CLASS (klass);
  thunarvfs_job_class->execute = thunar_vfs_simple_job_execute;
}



static void
thunar_vfs_simple_job_finalize (GObject *object)
{
  ThunarVfsSimpleJob *simple_job = THUNAR_VFS_SIMPLE_JOB (object);

  /* release the param values */
  _thunar_vfs_g_value_array_free (simple_job->param_values, simple_job->n_param_values);

  (*G_OBJECT_CLASS (thunar_vfs_simple_job_parent_class)->finalize) (object);
}



static void
thunar_vfs_simple_job_execute (ThunarVfsJob *job)
{
  ThunarVfsSimpleJob *simple_job = THUNAR_VFS_SIMPLE_JOB (job);
  GError             *error = NULL;

  /* try to execute the job using the supplied function */
  if (!(*simple_job->func) (job, simple_job->param_values, simple_job->n_param_values, &error))
    {
      /* forward the error to the client */
      _thunar_vfs_job_error (job, error);
      g_error_free (error);
    }
}



/**
 * thunar_vfs_simple_job_launch:
 * @func           : the #ThunarVfsSimpleJobFunc to execute the job.
 * @n_param_values : the number of parameters to pass to the @func.
 * @...            : a list of #GType and parameter pairs (exactly
 *                   @n_param_values pairs) that are passed to @func.
 *
 * Allocates a new #ThunarVfsSimpleJob, which executes the specified
 * @func with the specified parameters.
 *
 * For example the listdir @func expects a #ThunarVfsPath for the
 * folder to list, so the call to thunar_vfs_simple_job_launch()
 * would look like this:
 *
 * <informalexample><programlisting>
 * thunar_vfs_simple_job_launch (_thunar_vfs_io_jobs_listdir, 1,
 *                               THUNAR_VFS_TYPE_PATH, path);
 * </programlisting></informalexample>
 *
 * The caller is responsible to release the returned object using
 * thunar_vfs_job_unref() when no longer needed.
 *
 * Return value: the launched #ThunarVfsJob.
 **/
ThunarVfsJob*
thunar_vfs_simple_job_launch (ThunarVfsSimpleJobFunc func,
                              guint                  n_param_values,
                              ...)
{
  ThunarVfsSimpleJob *simple_job;
  va_list             var_args;
  GValue             *value;
  gchar              *error_message;

  /* allocate and initialize the simple job */
  simple_job = g_object_new (THUNAR_VFS_TYPE_SIMPLE_JOB, NULL);
  simple_job->func = func;
  simple_job->param_values = g_new0 (GValue, n_param_values);
  simple_job->n_param_values = n_param_values;

  /* collect the parameters */
  va_start (var_args, n_param_values);
  for (value = simple_job->param_values; value < simple_job->param_values + n_param_values; ++value)
    {
      /* initialize the value to hold the next parameter */
      g_value_init (value, va_arg (var_args, GType));

      /* collect the value from the stack */
      G_VALUE_COLLECT (value, var_args, 0, &error_message);

      /* check if an error occurred */
      if (G_UNLIKELY (error_message != NULL))
        {
          g_error ("%s: %s", G_STRLOC, error_message);
          g_free (error_message);
        }
    }
  va_end (var_args);

  /* launch the job */
  return thunar_vfs_job_launch (THUNAR_VFS_JOB (simple_job));
}



#define __THUNAR_VFS_SIMPLE_JOB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
