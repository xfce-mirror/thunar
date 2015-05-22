/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-206 Benedikt Meurer <benny@xfce.org>
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

#include <gdk/gdk.h>

#include <thunarx/thunarx-private.h>
#include <thunarx/thunarx-provider-factory.h>
#include <thunarx/thunarx-provider-module.h>
#include <thunarx/thunarx-provider-plugin.h>



/* "provider cache" cleanup interval (in seconds) */
#define THUNARX_PROVIDER_FACTORY_INTERVAL (45)



static void     thunarx_provider_factory_finalize       (GObject                     *object);
static void     thunarx_provider_factory_add            (ThunarxProviderFactory      *factory,
                                                         ThunarxProviderModule       *module);
static GList   *thunarx_provider_factory_load_modules   (ThunarxProviderFactory      *factory);
static gboolean thunarx_provider_factory_timer          (gpointer                     user_data);
static void     thunarx_provider_factory_timer_destroy  (gpointer                     user_data);



typedef struct
{
  GObject *provider;  /* cached provider reference or %NULL */
  GType    type;      /* provider GType */
} ThunarxProviderInfo;

struct _ThunarxProviderFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarxProviderFactory
{
  GObject __parent__;

  ThunarxProviderInfo *infos;     /* provider types and cached provider references */
  gint                 n_infos;   /* number of items in the infos array */

  guint                timer_id;  /* GSource timer to cleanup cached providers */
};



static GList *thunarx_provider_modules = NULL; /* list of active provider modules */



G_DEFINE_TYPE (ThunarxProviderFactory, thunarx_provider_factory, G_TYPE_OBJECT)



static void
thunarx_provider_factory_class_init (ThunarxProviderFactoryClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunarx_provider_factory_finalize;
}



static void
thunarx_provider_factory_init (ThunarxProviderFactory *factory)
{
}



static void
thunarx_provider_factory_finalize (GObject *object)
{
  ThunarxProviderFactory *factory = THUNARX_PROVIDER_FACTORY (object);
  gint                    n;

  /* stop the "provider cache" cleanup timer */
  if (G_LIKELY (factory->timer_id != 0))
    g_source_remove (factory->timer_id);

  /* release provider infos */
  for (n = 0; n < factory->n_infos; ++n)
    if (factory->infos[n].provider != NULL)
      g_object_unref (factory->infos[n].provider);
  g_free (factory->infos);

  (*G_OBJECT_CLASS (thunarx_provider_factory_parent_class)->finalize) (object);
}



static void
thunarx_provider_factory_add (ThunarxProviderFactory *factory,
                              ThunarxProviderModule  *module)
{
  const GType *types;
  gint         n_types;

  /* determines the types provided by the module */
  thunarx_provider_module_list_types (module, &types, &n_types);

  /* add the types provided by the extension */
  factory->infos = g_renew (ThunarxProviderInfo, factory->infos, factory->n_infos + n_types);
  for (; n_types-- > 0; ++types)
    {
      factory->infos[factory->n_infos].provider = NULL;
      factory->infos[factory->n_infos].type = *types;
      ++factory->n_infos;
    }
}



static GList*
thunarx_provider_factory_load_modules (ThunarxProviderFactory *factory)
{
  ThunarxProviderModule *module;
  const gchar           *name;
  GList                 *modules = NULL;
  GList                 *lp;
  GDir                  *dp;

  dp = g_dir_open (THUNARX_DIRECTORY, 0, NULL);
  if (G_LIKELY (dp != NULL))
    {
      /* determine the types for all existing plugins */
      for (;;)
        {
          /* read the next entry from the directory */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;

          /* check if this is a valid plugin file */
          if (g_str_has_suffix (name, "." G_MODULE_SUFFIX))
            {
              /* check if we already have that module */
              for (lp = thunarx_provider_modules; lp != NULL; lp = lp->next)
                if (g_str_equal (G_TYPE_MODULE (lp->data)->name, name))
                  break;

              /* use or allocate a new module for the file */
              if (G_UNLIKELY (lp != NULL))
                {
                  /* just use the existing module */
                  module = THUNARX_PROVIDER_MODULE (lp->data);
                }
              else
                {
                  /* allocate the new module and add it to our list */
                  module = thunarx_provider_module_new (name);
                  thunarx_provider_modules = g_list_prepend (thunarx_provider_modules, module);
                }

              /* try to load the module */
              if (g_type_module_use (G_TYPE_MODULE (module)))
                {
                  /* add the types provided by the module */
                  thunarx_provider_factory_add (factory, module);

                  /* add the module to our list */
                  modules = g_list_prepend (modules, module);
                }
            }
        }

      g_dir_close (dp);
    }

  return modules;
}



static gboolean
thunarx_provider_factory_timer (gpointer user_data)
{
  ThunarxProviderFactory *factory = THUNARX_PROVIDER_FACTORY (user_data);
  ThunarxProviderInfo    *info;
  gint                    n;

  GDK_THREADS_ENTER ();

  /* drop all providers for which only we keep a reference */
  for (n = factory->n_infos; --n >= 0; )
    {
      info = factory->infos + n;
      if (info->provider != NULL && info->provider->ref_count == 1)
        {
          g_object_unref (info->provider);
          info->provider = NULL;
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunarx_provider_factory_timer_destroy (gpointer user_data)
{
  THUNARX_PROVIDER_FACTORY (user_data)->timer_id = 0;
}



/**
 * thunarx_provider_factory_get_default:
 *
 * Returns a reference to the default #ThunarxProviderFactory
 * instance.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: a reference to the default
 *               #ThunarxProviderFactory instance.
 **/
ThunarxProviderFactory*
thunarx_provider_factory_get_default (void)
{
  static ThunarxProviderFactory *factory = NULL;

  /* allocate the default factory instance on-demand */
  if (G_UNLIKELY (factory == NULL))
    {
      factory = g_object_new (THUNARX_TYPE_PROVIDER_FACTORY, NULL);
      g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
    }
  else
    {
      /* take a reference on the default factory for the caller */
      g_object_ref (G_OBJECT (factory));
    }

  return factory;
}



/**
 * thunarx_provider_factory_list_providers:
 * @factory : a #ThunarxProviderFactory instance.
 * @type    : the provider #GType.
 *
 * Returns all providers of the given @type.
 *
 * The caller is responsible to release the returned
 * list of providers using code like this:
 * <informalexample><programlisting>
 * g_list_free_full (list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the of providers for @type.
 **/
GList*
thunarx_provider_factory_list_providers (ThunarxProviderFactory *factory,
                                         GType                   type)
{
  ThunarxProviderInfo *info;
  GList               *providers = NULL;
  GList               *modules = NULL;
  GList               *lp;
  gint                 n;

  /* check if the cleanup timer is running (and thereby the factory is initialized) */
  if (G_UNLIKELY (factory->timer_id == 0))
    {
      /* load all available modules (and thereby initialize the factory) */
      modules = thunarx_provider_factory_load_modules (factory);

      /* start the "provider cache" cleanup timer */
      factory->timer_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, THUNARX_PROVIDER_FACTORY_INTERVAL,
                                                      thunarx_provider_factory_timer, factory,
                                                      thunarx_provider_factory_timer_destroy);
    }

  /* determine all available providers for the type */
  for (info = factory->infos, n = factory->n_infos; --n >= 0; ++info)
    if (G_LIKELY (g_type_is_a (info->type, type)))
      {
        /* allocate the provider on-demand */
        if (G_UNLIKELY (info->provider == NULL))
          {
            info->provider = g_object_new (info->type, NULL);
            if (G_UNLIKELY (info->provider == NULL))
              continue;
          }

        /* take a reference for the caller */
        g_object_ref (info->provider);

        /* add the provider to the result list */
        providers = g_list_append (providers, info->provider);
      }

  /* check if we were initialized by this method invocation */
  if (G_UNLIKELY (modules != NULL))
    {
      /* unload all non-persistent modules */
      for (lp = modules; lp != NULL; lp = lp->next)
        if (!thunarx_provider_plugin_get_resident (lp->data))
          g_type_module_unuse (G_TYPE_MODULE (lp->data));

      g_list_free (modules);
    }

  return providers;
}
