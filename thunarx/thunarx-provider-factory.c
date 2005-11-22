/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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
#include <thunarx/thunarx-alias.h>



/* "provider cache" cleanup interval (in ms) */
#define THUNARX_PROVIDER_FACTORY_INTERVAL (60 * 1000)



static void     thunarx_provider_factory_class_init     (ThunarxProviderFactoryClass *klass);
static void     thunarx_provider_factory_init           (ThunarxProviderFactory      *manager);
static void     thunarx_provider_factory_finalize       (GObject                     *object);
static void     thunarx_provider_factory_add            (ThunarxProviderFactory      *manager,
                                                         ThunarxProviderModule       *module);
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

  GList               *modules;   /* the list of provider modules */

  ThunarxProviderInfo *infos;     /* provider types and cached provider references */
  gint                 n_infos;   /* number of items in the infos array */

  gint                 timer_id;  /* GSource timer to cleanup cached providers */
};



static GObjectClass *thunarx_provider_factory_parent_class;



GType
thunarx_provider_factory_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarxProviderFactoryClass),
        NULL,
        NULL,
        (GClassInitFunc) thunarx_provider_factory_class_init,
        NULL,
        NULL,
        sizeof (ThunarxProviderFactory),
        0,
        (GInstanceInitFunc) thunarx_provider_factory_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarxProviderFactory"), &info, 0);
    }

  return type;
}



static void
thunarx_provider_factory_class_init (ThunarxProviderFactoryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunarx_provider_factory_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunarx_provider_factory_finalize;
}



static void
thunarx_provider_factory_init (ThunarxProviderFactory *factory)
{
  ThunarxProviderModule *module;
  const gchar           *name;
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
              /* allocate a new module for the file */
              module = thunarx_provider_module_new (name);

              /* try to load the module */
              if (g_type_module_use (G_TYPE_MODULE (module)))
                {
                  /* add the module to our list of managed modules */
                  thunarx_provider_factory_add (factory, module);

                  /* don't unuse the type plugin if it should be resident in memory */
                  if (G_LIKELY (!thunarx_provider_plugin_get_resident (THUNARX_PROVIDER_PLUGIN (module))))
                    g_type_module_unuse (G_TYPE_MODULE (module));
                }
              else
                {
                  /* drop the reference on the module */
                  g_object_unref (G_OBJECT (module));
                }
            }
        }

      g_dir_close (dp);
    }

  /* start the "provider cache" cleanup timer */
  factory->timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNARX_PROVIDER_FACTORY_INTERVAL,
                                          thunarx_provider_factory_timer, factory,
                                          thunarx_provider_factory_timer_destroy);
}



static void
thunarx_provider_factory_finalize (GObject *object)
{
  ThunarxProviderFactory *factory = THUNARX_PROVIDER_FACTORY (object);
  gint                    n;

  /* stop the "provider cache" cleanup timer */
  if (G_LIKELY (factory->timer_id >= 0))
    g_source_remove (factory->timer_id);

  /* release provider infos */
  for (n = 0; n < factory->n_infos; ++n)
    if (factory->infos[n].provider != NULL)
      g_object_unref (factory->infos[n].provider);
  g_free (factory->infos);

  /* release the modules */
  g_list_foreach (factory->modules, (GFunc) g_object_unref, NULL);
  g_list_free (factory->modules);

  (*G_OBJECT_CLASS (thunarx_provider_factory_parent_class)->finalize) (object);
}



static void
thunarx_provider_factory_add (ThunarxProviderFactory *factory,
                              ThunarxProviderModule  *module)
{
  const GType *types;
  gint         n_types;

  /* add the module to our internal list */
  factory->modules = g_list_prepend (factory->modules, module);

  /* determines the types provided by the module */
  thunarx_provider_module_list_types (module, &types, &n_types);

  /* add the types provided by the extension */
  factory->infos = g_realloc (factory->infos, sizeof (ThunarxProviderInfo) * (factory->n_infos + n_types));
  for (; n_types-- > 0; ++types)
    {
      factory->infos[factory->n_infos].provider = NULL;
      factory->infos[factory->n_infos].type = *types;
      ++factory->n_infos;
    }
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
  THUNARX_PROVIDER_FACTORY (user_data)->timer_id = -1;
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
  static ThunarxProviderFactory *default_factory = NULL;

  /* allocate the default factory instance on-demand */
  if (G_UNLIKELY (default_factory == NULL))
    default_factory = g_object_new (THUNARX_TYPE_PROVIDER_FACTORY, NULL);

  /* take a reference on the default factory for the caller */
  return g_object_ref (G_OBJECT (default_factory));
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
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
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
  gint                 n;

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

  return providers;
}



#define __THUNARX_PROVIDER_FACTORY_C__
#include <thunarx/thunarx-aliasdef.c>
