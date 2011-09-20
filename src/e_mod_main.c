#include "e.h"
#include "e_mod_main.h"

static void _keybuf_action_keybuf_cb(E_Object *obj, const char *params);

Config *keybuf_config = NULL;

static E_Action *act = NULL;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Keybuf" };

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[4096];

   snprintf(buf, sizeof(buf), "%s/locale", e_module_dir_get(m));
   bindtextdomain(PACKAGE, buf);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   keybuf_config = E_NEW(Config, 1);
   keybuf_config->module = m;

   act = e_action_add("keybuf");
   if (act)
     {
	act->func.go = _keybuf_action_keybuf_cb;
	e_action_predef_name_set(_("Keybuf"),
				 _("Activate Keybuf"),
				 "keybuf",
				 NULL, NULL, 0);
     }
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   free(keybuf_config);
   keybuf_config = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   return 1;
}

static void
_keybuf_action_keybuf_cb(E_Object *obj, const char *params)
{
   E_Zone *zone = NULL;

   if (obj)
     {
	if (obj->type == E_MANAGER_TYPE)
	  zone = e_util_zone_current_get((E_Manager *)obj);
	else if (obj->type == E_CONTAINER_TYPE)
	  zone = e_util_zone_current_get(((E_Container *)obj)->manager);
	else if (obj->type == E_ZONE_TYPE)
	  zone = e_util_zone_current_get(((E_Zone *)obj)->container->manager);
	else
	  zone = e_util_zone_current_get(e_manager_current_get());
     }
   if (!zone) zone = e_util_zone_current_get(e_manager_current_get());
   if (zone) keybuf_show(zone);
}
