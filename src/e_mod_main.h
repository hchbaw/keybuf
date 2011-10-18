#include "e.h"

#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

typedef struct _Config
{
   E_Module *module;
} Config;

#include <libintl.h>

#define _(str) dgettext(PACKAGE, str)

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);

int keybuf_show(E_Zone *zone, Eina_List *(*show)(E_Zone *zone));
Eina_List *keybuf_show_cb_current_desk(E_Zone *zone);
Eina_List *keybuf_show_cb_current_zone(E_Zone *zone);
void keybuf_hide(void);

extern Config *keybuf_config;

#endif
