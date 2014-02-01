#include "e.h"
#include "e_mod_main.h"
#include "keybuf_util.h"

#include <ctype.h>
#include <string.h>

typedef struct _Hint Hint;
typedef struct _Pager_Popup Pager_Popup;
typedef struct _Pager Pager;
typedef struct _Pager_Desk Pager_Desk;
typedef struct _Pager_Win Pager_Win;

struct _Hint
{
   E_Client    *client;
   E_Desk      *desk;
   Evas_Object *popup;
   Evas        *evas;
   Evas_Object *bgobj;
   char        *label;
   Ecore_Timer *timer;
   void (*popupshow)(Hint *h);
   void (*popuphide)(Hint *h);

   Pager_Win   *pwin;
};

struct _Pager_Popup
{
   Evas_Object *bgobj;
   Evas_Object *popup;
   Evas_Object *otable;
   E_Zone      *zone;
};

struct _Pager
{
   Pager_Popup *popup;
   E_Desk      *curdesk;
   Eina_List   *desks;

   Evas_Coord w, h;
   int xnum, ynum;
};

struct _Pager_Desk
{
   Pager       *pager;
   E_Desk      *desk;
   Eina_List   *wins;
   Evas_Object *odesk;
   Evas_Object *olayout;
   int xpos, ypos, urgent;
   int current : 1;
};

struct _Pager_Win
{
   E_Client    *client;
   Pager_Desk  *desk;
   Evas_Object *owindow;
   Evas_Object *oicon;
   Evas_Object *ohint;
};

typedef struct _Keymap Keymap;

struct _Keymap
{
   char *buf;
   Eina_Bool (*key_down)(Ecore_Event_Key *ev, Keymap *k);
};

typedef struct _Cwin
{
   Evas_Object *bgobj;
   Evas_Object *popup;
} Cwin;

static Eina_Bool _keybuf_hint_cb_timeout(void *data);
static int _keybuf_hint_cb_sort(const void *d1, const void *d2);
static Hint *_keybuf_hint_new(E_Client *ec, E_Desk *desk, void (*popupshow)(Hint *h), void (*popuphide)(Hint *h));
static char *_keybuf_hint_labelify(int n, const char *labels);
static void _keybuf_hint_show(Hint *h, char *labelstring);
static void _keybuf_hint_del(Hint *h);
static void _keybuf_hint_select(Hint *old, Hint *new);
static Pager *_keybuf_pager_new_raw(E_Desk *curdesk);
static Pager *_keybuf_pager_new(E_Desk *curdesk, E_Zone *zone, int xnum, int ynum, Evas_Coord w, Evas_Coord h);
static void _keybuf_pager_del(Pager *p);
static Pager_Popup *_keybuf_pager_popup_new(E_Zone *zone);
static void _keybuf_pager_popup_del(Pager_Popup *pp);
static void _keybuf_pager_popup_show(Pager_Popup *pp, Evas_Coord width, Evas_Coord height);
static Pager_Desk *_keybuf_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos);
static void _keybuf_pager_desk_del(Pager_Desk *pd);
static void _keybuf_pager_desk_select(Pager_Desk *pd);
static Pager_Win *_keybuf_pager_window_new(Pager_Desk *pd, E_Client *ec);
static void _keybuf_pager_window_del(Pager_Win *pw);
static void _keybuf_key_clear(char *ptr);
static void _keybuf_key_compose(char *ptr, const char *compose);
static void _keybuf_update(char *param, Eina_Bool (*showp)(const Hint *h, const void *param), Eina_Bool focusp);
static void _keybuf_activate(Eina_List *matches);
static void _keybuf_focus(void);
static void _keybuf_focus_aux(Hint *h);
static Eina_Bool _keybuf_update_cb_label_prefix_p(const Hint *h, const void *param);
static Eina_Bool _keybuf_update_cb_window_match_p(const Hint *h, const void *param);
static Eina_Bool _keybuf_cb_key_down(void *data, int type, void *event);
static Eina_Bool _keybuf_keymap_key_down_normal(Ecore_Event_Key *ev, Keymap *k);
static Eina_Bool _keybuf_keymap_key_down_command(Ecore_Event_Key *ev, Keymap *k);
static void _keybuf_cmd_show(void);
static void _keybuf_cmd_hide(void);
static void _keybuf_cmd_del_cmd(Cwin *c);
static void _keybuf_cmd_update_command_text(const char *str);

/* local subsystem globals */
static Ecore_X_Window input_window = 0;
static char *key_buf = NULL;
static char *key_buf_cmd = NULL;
static Eina_List *handlers = NULL;
static Eina_List *hints = NULL;
static Hint *hselected = NULL;
static Keymap *curkeymap = NULL;
static Eina_List *cwins = NULL;
static Eina_List *pagers = NULL;

#define KEYBUFLEN 2048

int
keybuf_show(E_Zone *zone, Eina_List *(*show)(E_Zone *zone))
{
   input_window = ecore_x_window_input_new(zone->comp->win, 0, 0, 1, 1);
   ecore_x_window_show(input_window);
   e_grabinput_get(input_window, 0, input_window);

   pagers = (*show)(zone);
   if (!pagers) return 0;

   key_buf = malloc(KEYBUFLEN);
   if (!key_buf) return 0;
   key_buf[0] = 0;

   key_buf_cmd = malloc(KEYBUFLEN);
   if (!key_buf_cmd) return 0;
   key_buf_cmd[0] = 0;

   curkeymap = E_NEW(Keymap, 1);
   curkeymap->key_down = _keybuf_keymap_key_down_normal;
   curkeymap->buf = key_buf;

   handlers = eina_list_append
      (handlers, ecore_event_handler_add
       (ECORE_EVENT_KEY_DOWN, _keybuf_cb_key_down, NULL));

   _keybuf_activate(hints);
   return 0;
}

void
keybuf_hide(void)
{
   Ecore_Event_Handler *ev;
   Hint *h;
   Cwin *c;
   Pager *p;

   EINA_LIST_FREE(pagers, p)
      _keybuf_pager_del(p);
   eina_list_free(pagers);

   EINA_LIST_FREE(handlers, ev)
      ecore_event_handler_del(ev);
   eina_list_free(handlers);

   ecore_x_window_free(input_window);
   e_grabinput_release(input_window, input_window);
   input_window = 0;

   free(key_buf);
   key_buf = NULL;

   free(key_buf_cmd);
   key_buf_cmd = NULL;

   EINA_LIST_FREE(cwins, c)
      _keybuf_cmd_del_cmd(c);
   eina_list_free(cwins);

   EINA_LIST_FREE(hints, h)
     {
	if (hselected != h)
	  _keybuf_hint_del(h);
     }
   eina_list_free(hints);

   if (hselected)
     hselected->timer = ecore_timer_add(0.25,
					_keybuf_hint_cb_timeout,
					hselected);

   hselected = NULL;
}

static Eina_Bool
_keybuf_hint_cb_timeout(void *data)
{
   Hint *h;

   h = data;
   h->timer = NULL;
   _keybuf_hint_del(h);

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_keybuf_show_aux(Eina_List *hs)
{
   Eina_List *l;
   Hint *h;
   int i = 0;
   int c = eina_list_count(hs);

   if (c == 0)
     {
	keybuf_hide();
	return EINA_FALSE;
     }

   hints = eina_list_sort(hs, c, _keybuf_hint_cb_sort);

   EINA_LIST_FOREACH(hints, l, h)
      _keybuf_hint_show
      (h, _keybuf_hint_labelify(i++, "AOEUIDHTNS-;QJKXBMWVZ',.PYFGCRL=\\"));

   return EINA_TRUE;
}

static Eina_List *
_keybuf_hint_list_hints(E_Desk *desk, Eina_List *acc, Eina_List *(*accfun)(Eina_List *acc, E_Client *ec, E_Desk *desk, void *data), void *data)
{
   E_Client *ec;

   E_CLIENT_FOREACH(desk->zone->comp, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (ec->new_client) continue;
        acc = (*accfun)(acc, ec, desk, data);
     }
   return acc;
}

static void
_keybuf_hint_popup_show_current_desk(Hint *h)
{
   evas_object_show(h->popup);
}

static void
_keybuf_hint_popup_hide(Hint *h)
{
   evas_object_hide(h->popup);
}

static Eina_List *
_keybuf_hint_list_hints_current_desk_accum(Eina_List *acc, E_Client *ec, E_Desk *desk, void *data __UNUSED__)
{
   if ((ec->desk != desk) && (!ec->sticky)) return acc;
   return eina_list_append
      (acc, _keybuf_hint_new(ec, desk,
			     _keybuf_hint_popup_show_current_desk,
			     _keybuf_hint_popup_hide));
}

struct hints_pager
{
   Eina_List *hints;
   Pager *pager;
};

static struct hints_pager *
_keybuf_hint_list_hints_current_desk(E_Desk *curdesk)
{
   struct hints_pager *hp;

   hp = E_NEW(struct hints_pager, 1);
   hp->hints = _keybuf_hint_list_hints
      (curdesk, NULL, _keybuf_hint_list_hints_current_desk_accum, NULL);
   hp->pager = _keybuf_pager_new_raw(curdesk);
   return hp;
}

static void
_keybuf_hint_popup_show_current_zone(Hint *hi)
{
   if (hi->desk == e_desk_current_get
       (e_util_zone_current_get(e_manager_current_get())))
     evas_object_show(hi->popup);

   if (hi->pwin) evas_object_show(hi->pwin->ohint);
}

static void
_keybuf_hint_popup_hide_current_zone(Hint *hi)
{
   evas_object_hide(hi->popup);

   if (hi->pwin) evas_object_hide(hi->pwin->ohint);
}

static Eina_List *
_keybuf_hint_list_hints_current_zone_accum(Eina_List *acc, E_Client *ec, E_Desk *desk, void *data)
{
   Pager_Desk *pd;
   Hint *hi;

   if ((ec->desk != desk) && (!ec->sticky)) return acc;

   pd = data;
   hi = _keybuf_hint_new(ec, desk,
			 _keybuf_hint_popup_show_current_zone,
			 _keybuf_hint_popup_hide_current_zone);
   if (pd && hi)
     {
	Pager_Win *pw;

	pw = _keybuf_pager_window_new(pd, ec);
	if (pw)
	  {
	     pd->wins = eina_list_append(pd->wins, pw);
	     // TODO: badness
	     hi->pwin = pw;
	  }
     }
   return eina_list_append(acc, hi);
}

static struct hints_pager *
_keybuf_hint_list_hints_current_zone(E_Desk *curdesk, E_Zone *zone)
{
   struct hints_pager *hp;
   Eina_List *xs = NULL;
   Pager *p;
   int x, y, xnum, ynum, width, height;

   e_zone_desk_count_get(zone, &xnum, &ynum);
   height = 60;
   width = height * (zone->w * xnum) / (zone->h * ynum);
   hp = E_NEW(struct hints_pager, 1);
   p = _keybuf_pager_new(curdesk, zone, xnum, ynum, width, height);
   hp->pager = p;
   for (x = 0; x < xnum; x++)
     {
	for (y = 0; y < ynum; y++)
	  {
	     E_Desk *desk;
	     Pager_Desk *pd;

	     desk = e_desk_at_xy_get(zone, x, y);
	     pd = _keybuf_pager_desk_new(p, desk, x, y);
	     if (pd)
	       {
		  p->desks = eina_list_append(p->desks, pd);
		  xs = _keybuf_hint_list_hints
		     (desk, xs,
		      _keybuf_hint_list_hints_current_zone_accum, pd);
		  if (desk == curdesk)
		    _keybuf_pager_desk_select(pd);
	       }
	  }
     }
   hp->hints = xs;
   return hp;
}

Eina_List *
keybuf_show_cb_current_desk(E_Zone *zone)
{
   struct hints_pager *hp;
   Eina_List *ps = NULL;

   hp = _keybuf_hint_list_hints_current_desk(e_desk_current_get(zone));
   if (!hp) return NULL;
   if (_keybuf_show_aux(hp->hints))
     ps = eina_list_append(ps, hp->pager);

   E_FREE(hp);
   return ps;
}

Eina_List *
keybuf_show_cb_current_zone(E_Zone *zone)
{
   struct hints_pager *hp;
   Eina_List *ps = NULL;

   hp = _keybuf_hint_list_hints_current_zone(e_desk_current_get(zone), zone);
   if (!hp) return NULL;
   if (_keybuf_show_aux(hp->hints))
     {
	ps = eina_list_append(ps, hp->pager);
	_keybuf_pager_popup_show(hp->pager->popup, hp->pager->w, hp->pager->h);
     }

   E_FREE(hp);
   return ps;
}

static int
_keybuf_hint_cb_sort(const void *d1, const void *d2)
{
   const Hint *h1, *h2;

   if (!d1) return 1;
   if (!d2) return -1;

   h1 = d1, h2 = d2;
   return
      (h1->client->x +
       h1->desk->zone->x * h1->desk->zone->w +
       h1->desk->x * h1->desk->zone->w +
       h1->client->w / 2) -
      (h2->client->x +
       h2->desk->zone->x * h2->desk->zone->w +
       h2->desk->x * h2->desk->zone->w +
       h2->client->w / 2);
}

static char *
_keybuf_hint_labelify(int n, const char *labels)
{
   Eina_Strbuf *s = eina_strbuf_new();
   int b = strlen(labels);
   char *r = NULL;

   do
     {
	if (!eina_strbuf_prepend_char(s, labels[n % b])) return NULL;
	n = floor(n / b);
     }
   while (n > 0);

   r = eina_strbuf_string_steal(s);
   eina_strbuf_free(s);
   return r;
}

static Hint *
_keybuf_hint_new(E_Client *ec, E_Desk *desk, void (*popupshow)(Hint *h), void (*popuphide)(Hint *h))
{
   Hint *hi;

   hi = E_NEW(Hint, 1);
   hi->client = ec;
   hi->desk = desk;
   hi->popupshow = popupshow;
   hi->popuphide = popuphide;

   e_object_ref(E_OBJECT(ec));
   e_object_ref(E_OBJECT(desk));
   return hi;
}

static void
_keybuf_hint_show(Hint *hi, char *labelstring)
{
   Evas_Coord w, h;
   Evas_Object *o = NULL;
   char buf[PATH_MAX];

   o = edje_object_add(hi->client->zone->comp->evas);
   hi->popup = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(hi->popup, E_LAYER_CLIENT_POPUP);
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/hint"))
     edje_object_file_set(o, buf, "e/modules/keybuf/hint");
   edje_object_part_text_set(o, "e.text.label", labelstring);
   edje_object_size_min_calc(o, &w, &h);
   evas_object_resize(o, w, h);

   hi->bgobj = o;
   hi->label = labelstring;

   evas_object_resize(hi->popup, w, h);
   e_comp_object_util_center_on(hi->popup, hi->client->frame);

   (*hi->popupshow)(hi);

   /* TODO: badness */
   if (hi->pwin)
     edje_object_part_text_set(hi->pwin->ohint, "e.text.label", hi->label);

}

static void
_keybuf_hint_del(Hint *h)
{
   if (h->timer) ecore_timer_del(h->timer);
   e_object_unref(E_OBJECT(h->client));
   e_object_unref(E_OBJECT(h->desk));
   free(h->label);
   evas_object_hide(h->popup);
   evas_object_del(h->popup);
   evas_object_hide(h->bgobj);
   evas_object_del(h->bgobj);
   E_FREE(h);
   h = NULL;
}

static void
_keybuf_hint_select(Hint *old, Hint *new)
{
   Eina_List *l;
   Pager *p;

   if (old && old != new)
     {
	edje_object_signal_emit(old->bgobj, "e,state,unselected", "e");
	if (old->pwin)
	  edje_object_signal_emit(old->pwin->ohint, "e,state,unselected", "e");
     }

   if (new && old != new)
     {
	edje_object_signal_emit(new->bgobj, "e,state,selected", "e");
	if (new->pwin)
	  edje_object_signal_emit(new->pwin->ohint, "e,state,selected", "e");
     }
}

static Pager *
_keybuf_pager_new_raw(E_Desk *curdesk)
{
   Pager *p;

   p = E_NEW(Pager, 1);
   p->curdesk = curdesk;
   e_object_ref(E_OBJECT(curdesk));
   return p;
}

static Pager *
_keybuf_pager_new(E_Desk *curdesk, E_Zone *zone, int xnum, int ynum, Evas_Coord w, Evas_Coord h)
{
   Pager *p;

   p = _keybuf_pager_new_raw(curdesk);
   if (zone)
     {
	p->popup = _keybuf_pager_popup_new(zone);
	p->xnum = xnum;
	p->ynum = ynum;
	p->w = w;
	p->h = h;
     }
   return p;
}

static void
_keybuf_pager_del(Pager *p)
{
   Pager_Desk *pd;

   if (p->popup) _keybuf_pager_popup_del(p->popup);
   if (p->desks)
     {
	EINA_LIST_FREE(p->desks, pd)
	   _keybuf_pager_desk_del(pd);
	eina_list_free(p->desks);
     }
   e_object_unref(E_OBJECT(p->curdesk));
   E_FREE(p);
}

static Pager_Popup *
_keybuf_pager_popup_new(E_Zone *zone)
{
   Pager_Popup *pp;
   E_Desk *desk;
   int x, y;

   pp = E_NEW(Pager_Popup, 1);
   if (!pp) return NULL;
   pp->zone = zone;

   pp->otable = e_table_add(pp->zone->comp->evas);
   e_table_homogenous_set(pp->otable, 1);

   return pp;
}

static void
_keybuf_pager_popup_del(Pager_Popup *pp)
{
   evas_object_hide(pp->popup);
   evas_object_del(pp->bgobj);
   evas_object_del(pp->otable);
   evas_object_del(pp->popup);
   E_FREE(pp);
}

static void
_keybuf_pager_popup_show(Pager_Popup *pp, Evas_Coord width, Evas_Coord height)
{
   Evas_Object *o = NULL;
   Evas_Coord w, h, zx, zy, zw, zh;
   E_Desk *desk;
   char buf[PATH_MAX];

   evas_object_move(pp->otable, 0, 0);
   evas_object_resize(pp->otable, width, height);

   o = edje_object_add(pp->zone->comp->evas);
   pp->popup = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(pp->popup, E_LAYER_CLIENT_POPUP);
   pp->bgobj = o;
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/pager/popup"))
     edje_object_file_set(o, buf, "e/modules/keybuf/pager/popup");
   desk = e_desk_current_get(pp->zone);
   if (desk)
     edje_object_part_text_set(o, "e.text.label", desk->name);

   edje_extern_object_min_size_set(pp->otable, width, height);
   edje_object_part_swallow(o, "e.swallow.content", pp->otable);
   edje_object_size_min_calc(o, &w, &h);
   evas_object_move(o, 0, 0);
   evas_object_resize(o, w, h);

   e_zone_useful_geometry_get(pp->zone, &zx, &zy, &zw, &zh);
   zx -= pp->zone->x;
   zy -= pp->zone->y;
   evas_object_geometry_set(pp->popup,
                            zx + ((zw - w) / 2),
                            zy + ((zh - h) * 3 / 4),
                            w, h);
   evas_object_show(pp->popup);
}

static Pager_Desk *
_keybuf_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos)
{
   Pager_Desk *pd;
   Evas_Object *o;
   char buf[PATH_MAX];

   pd = E_NEW(Pager_Desk, 1);
   if (!pd) return NULL;

   pd->xpos = xpos;
   pd->ypos = ypos;
   pd->desk = desk;
   e_object_ref(E_OBJECT(desk));
   pd->pager = p;

   o = edje_object_add(evas_object_evas_get(p->popup->otable));
   pd->odesk = o;
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/pager/desk"))
     edje_object_file_set(o, buf, "e/modules/keybuf/pager/desk");

   e_table_pack(p->popup->otable, o, xpos, ypos, 1, 1);
   e_table_pack_options_set(o, 1, 1, 1, 1, 0.5, 0.5, 0, 0, -1, -1);
   evas_object_show(o);

   o = e_layout_add(evas_object_evas_get(p->popup->otable));
   pd->olayout = o;
   e_layout_virtual_size_set(o, desk->zone->w, desk->zone->h);
   edje_object_part_swallow(pd->odesk, "e.swallow.content", pd->olayout);
   evas_object_show(o);
   return pd;
}

static void
_keybuf_pager_desk_del(Pager_Desk *pd)
{
   Pager_Win *w;

   evas_object_del(pd->odesk);
   evas_object_del(pd->olayout);
   EINA_LIST_FREE(pd->wins, w)
      _keybuf_pager_window_del(w);
   eina_list_free(pd->wins);
   e_object_unref(E_OBJECT(pd->desk));
   E_FREE(pd);
}

static void
_keybuf_pager_desk_select(Pager_Desk *pd)
{
   Eina_List *l;
   Pager_Desk *pd2;

   if (pd->current) return;

   EINA_LIST_FOREACH(pd->pager->desks, l, pd2)
     {
	if (pd == pd2)
	  {
	     pd2->current = 1;
	     evas_object_raise(pd2->odesk);
	     edje_object_signal_emit(pd2->odesk, "e,state,selected", "e");
	  }
	else
	  {
	     if (pd2->current)
	       {
		  pd2->current = 0;
		  edje_object_signal_emit(pd2->odesk,
					  "e,state,unselected", "e");
	       }
	  }
     }
}

static Pager_Win *
_keybuf_pager_window_new(Pager_Desk *pd, E_Client *ec)
{
   Pager_Win *pw;
   Evas_Object *o;
   char buf[PATH_MAX];

   if (!ec) return NULL;
   pw = E_NEW(Pager_Win, 1);
   if (!pw) return NULL;

   pw->client = ec;
   e_object_ref(E_OBJECT(pw->client));
   pw->desk = pd;

   o = edje_object_add(evas_object_evas_get(pd->pager->popup->otable));
   pw->owindow = o;
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/pager/window"))
     edje_object_file_set(o, buf, "e/modules/keybuf/pager/window");
   //if (visible) evas_object_show(o);
   evas_object_show(o);

   e_layout_pack(pd->olayout, pw->owindow);
   e_layout_child_raise(pw->owindow);

   o = e_client_icon_add(pw->client,
                         evas_object_evas_get(pd->pager->popup->otable));
   if (o)
     {
	pw->oicon = o;
	evas_object_show(o);
	edje_object_part_swallow(pw->owindow, "e.swallow.icon", o);
     }
   if (pw->client->icccm.urgent)
     {
	if (!(pw->client->iconic))
	  edje_object_signal_emit(pd->odesk, "e.state.urgent", "e");
	edje_object_signal_emit(pw->owindow, "e.state.urgent", "e");
     }
   evas_object_show(o);

   e_layout_child_move(pw->owindow,
		       pw->client->x - pw->client->zone->x,
		       pw->client->y - pw->client->zone->y);
   e_layout_child_resize(pw->owindow, pw->client->w, pw->client->h);

   o = edje_object_add(evas_object_evas_get(pw->desk->pager->popup->otable));
   pw->ohint = o;
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/pager/hint"))
     edje_object_file_set(o, buf, "e/modules/keybuf/pager/hint");

   e_layout_pack(pw->desk->olayout, pw->ohint);
   e_layout_child_raise(pw->ohint);

   e_layout_child_move(pw->ohint,
		       pw->client->x - pw->client->zone->x,
		       pw->client->y - pw->client->zone->y);
   /* TODO: calc */
     {
	Evas_Coord vw;
	int cw;

	e_layout_virtual_size_get(pw->desk->olayout, &vw, NULL);
	//vw : ? = pw->desk->pager->w / xnum : 48
	printf("***%d\n", vw * 48 * pw->desk->pager->xnum / pw->desk->pager->w);
	//cw = vw * 48 * 3 * pw->desk->pager->xnum / pw->desk->pager->w / 4;
	cw = vw * 48 * pw->desk->pager->xnum / pw->desk->pager->w / 2;
	printf("***%d\n", cw);
	e_layout_child_resize(pw->ohint, cw, cw);
     }
   //e_layout_child_resize(pw->ohint, 208, 208);
   evas_object_show(o);

   return pw;
}

static void
_keybuf_pager_window_del(Pager_Win *pw)
{
   if (pw->owindow) evas_object_del(pw->owindow);
   if (pw->oicon) evas_object_del(pw->oicon);
   if (pw->ohint) evas_object_del(pw->ohint);
   e_object_unref(E_OBJECT(pw->client));
   E_FREE(pw);
}

static Eina_Bool
_keybuf_cb_key_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev = event;

   if (ev->event_window != input_window) return ECORE_CALLBACK_PASS_ON;

   if (ev->modifiers)
     {
	Eina_List *l;
	E_Config_Binding_Key *binding;
	E_Binding_Modifier mod;

	EINA_LIST_FOREACH(e_bindings->key_bindings, l, binding)
	  {
	     if (binding->action && strcmp(binding->action, "keybuf")) continue;

	     mod = 0;

	     if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
	       mod |= E_BINDING_MODIFIER_SHIFT;
	     if (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)
	       mod |= E_BINDING_MODIFIER_CTRL;
	     if (ev->modifiers & ECORE_EVENT_MODIFIER_ALT)
	       mod |= E_BINDING_MODIFIER_ALT;
	     if (ev->modifiers & ECORE_EVENT_MODIFIER_WIN)
	       mod |= E_BINDING_MODIFIER_WIN;

	     if (!(binding->key && (!strcmp(binding->key, ev->keyname)) &&
		   (((unsigned int)binding->modifiers == mod) ||
		    (binding->any_mod))))
	       continue;

	     keybuf_hide();

	     return ECORE_CALLBACK_PASS_ON;
	  }
     }

   return curkeymap->key_down(ev, curkeymap);
}

static Eina_Bool
_keybuf_keymap_key_down_normal(Ecore_Event_Key *ev, Keymap *k)
{
   if (!strcmp(ev->key, "Escape"))
     {
	keybuf_hide();
	return ECORE_CALLBACK_PASS_ON;
     }
   else if (!strcmp(ev->key, "u") &&
	    (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL))
     {
	_keybuf_key_clear(k->buf);
	_keybuf_update(k->buf, _keybuf_update_cb_label_prefix_p, EINA_FALSE);
     }
   else if (!strcmp(ev->key, "slash"))
     {
	k->buf = key_buf_cmd;
	k->key_down = _keybuf_keymap_key_down_command;
	k->buf[0] = '/'; k->buf[1] = 0;
	_keybuf_cmd_show();
     }
   else if (!strcmp(ev->key, "Return"))
     {
	_keybuf_focus();
     }
   else
     {
	if (ev->compose)
	  {
	     _keybuf_key_compose(k->buf, ev->compose);
	     _keybuf_update(k->buf, _keybuf_update_cb_label_prefix_p,
			    EINA_FALSE);
	  }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_keybuf_key_clear(char *ptr)
{
   if (ptr[0] != 0)
     ptr[0] = 0;
}

static void
_keybuf_key_compose(char *ptr, const char *compose)
{
   if ((strlen(ptr) < (KEYBUFLEN - strlen(compose))))
     strcat(ptr, compose);
}

static Eina_Bool
_keybuf_update_cb_label_prefix_p(const Hint *h, const void *param)
{
   const char *buf = param;

   if (buf[0] == 0 || keybuf_util_str_prefix_p(h->label, buf, strncasecmp))
     return EINA_TRUE;

   return EINA_FALSE;
}

static void
_keybuf_update(char *param, Eina_Bool (*showp)(const Hint *h, const void *param), Eina_Bool focusp)
{
   Eina_List *l;
   Hint *h;
   Eina_List *hs = NULL;
   int c;

   EINA_LIST_FOREACH(hints, l, h)
     {
	if ((*showp)(h, param))
	  {
	     (*h->popupshow)(h);
	     hs = eina_list_append(hs, h);
	  }
	else
	  (*h->popuphide)(h);
     }

   c = eina_list_count(hs);
   if (hs && c > 0)
     {
	_keybuf_activate(hs);
	eina_list_free(hs);
	if (focusp || c ==  1) _keybuf_focus();
     }
}

static Eina_Bool
_keybuf_find_focused_border_cb(const void *data, const void *param)
{
   const Hint *h = data;
   const E_Client *focused = param;

   if (focused->sticky)
     {
	/* TODO: consider multiple zones */
	if ((h->client == focused) &&
	    (h->desk == e_desk_current_get
	     (e_util_zone_current_get(e_manager_current_get()))))
	  return EINA_TRUE;
     }
   else
     {
	if (h->client == focused)
	  return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_keybuf_activate(Eina_List *matches)
{
   Hint *old = hselected;

   hselected = NULL;

   if (matches && eina_list_count(matches) > 0)
     {
	if (curkeymap->buf[0] == 0) /* TODO */
	  {
	     E_Client *ec = NULL;

	     ec = e_client_focused_get();
	     if (ec)
	       hselected = keybuf_util_find(matches,
					    _keybuf_find_focused_border_cb,
					    ec);
	  }
	if (!hselected)
	  hselected = eina_list_data_get(matches);
     }

   if (hselected) _keybuf_hint_select(old, hselected);
}

static void
_keybuf_focus(void)
{
   if (!hselected) return;
   _keybuf_focus_aux(hselected);
   keybuf_hide();
}

static void
_keybuf_focus_aux(Hint *h)
{
   evas_object_show(h->popup);

   if (h->desk != e_desk_current_get
       (e_util_zone_current_get(e_manager_current_get())))
     e_desk_show(h->desk);

   edje_object_signal_emit(h->bgobj, "e,state,focused", "e");

   e_util_pointer_center(h->client);
   evas_object_raise(h->client->frame);
   evas_object_focus_set(h->client->frame, 1);
}

static Eina_Bool
_keybuf_keymap_key_down_command(Ecore_Event_Key *ev, Keymap *k)
{
   if (!strcmp(ev->key, "Escape"))
     {
	_keybuf_cmd_hide();
	k->buf = key_buf;
	k->key_down = _keybuf_keymap_key_down_normal;
	return ECORE_CALLBACK_PASS_ON;
     }
   else if (!strcmp(ev->key, "Return"))
     {
	_keybuf_focus();
     }
   else
     {
	if (ev->compose)
	  {
	     _keybuf_key_compose(k->buf, ev->compose);
	     _keybuf_cmd_update_command_text(k->buf);
	     _keybuf_update(k->buf, _keybuf_update_cb_window_match_p,
			    EINA_FALSE);
	  }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Cwin *
_keybuf_cmd_show1(E_Zone *zone)
{
   Evas_Coord w, h;
   Evas_Object *p = NULL;
   Evas_Object *o = NULL;
   char buf[PATH_MAX];
   Cwin *cwin = NULL;

   o = edje_object_add(zone->comp->evas);
   p = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(p, E_LAYER_CLIENT_POPUP);
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"e/modules/keybuf/cwin"))
     edje_object_file_set(o, buf, "e/modules/keybuf/cwin");
   edje_object_size_min_calc(o, &w, &h);
   evas_object_move(o, 0, 0);
   evas_object_resize(o, w, h);
   evas_object_show(o);
   //e_popup_content_set(p, o);
   evas_object_geometry_set(p,
                            ((zone->w - w) / 2),
                            ((zone->h - h) / 4 * 3),
                            w, h);
   evas_object_show(p);

   cwin = E_NEW(Cwin, 1);
   cwin->bgobj = o;
   cwin->popup = p;
   return cwin;
}

void *
_keybuf_cmd_show2(E_Zone *zone, void *a)
{
   Eina_List *as = a;

   return eina_list_append(as, _keybuf_cmd_show1(zone));
}

static void
_keybuf_cmd_show()
{
   cwins = keybuf_util_accum_zone(_keybuf_cmd_show2, cwins);
   _keybuf_cmd_update_command_text(curkeymap->buf);
}

static void
_keybuf_cmd_hide(void)
{
   Cwin *c;

   EINA_LIST_FREE(cwins, c)
      _keybuf_cmd_del_cmd(c);
}

static void
_keybuf_cmd_del_cmd(Cwin *c)
{
   evas_object_hide(c->popup);

   evas_object_del(c->bgobj);
   evas_object_del(c->popup);
   free(c);
   c = NULL;
}

static void
_keybuf_cmd_update_command_text(const char *str)
{
   Eina_List *l;
   Cwin *c;

   EINA_LIST_FOREACH(cwins, l, c)
      edje_object_part_text_set(c->bgobj, "e.text.command", str);
}

static Eina_Bool
_keybuf_update_cb_window_match_p_aux(const char *s, char *q)
{
   int qlen = strlen(q);

   if (eina_str_has_prefix(q, "^"))
     {
	if (qlen > 1)
	  {
	     if (keybuf_util_str_pred_p(q + 1, isupper))
	       return eina_str_has_prefix(s, q + 1);
	     else
	       return keybuf_util_str_prefix_p(s, q + 1, strncasecmp);
	  }
	else
	  return EINA_TRUE;
     }

   if (eina_str_has_suffix(q, "$"))
     {
	if (qlen > 1)
	  {
	     char *qq = strdup(q);
	     Eina_Bool ret;

	     if (!qq) return EINA_TRUE;

	     qq[qlen - 1] = '\0';
	     if (keybuf_util_str_pred_p(qq, isupper))
	       ret = eina_str_has_suffix(s, qq);
	     else
	       ret = keybuf_util_str_suffix_p(s, qq, strncasecmp);
	     free(qq);
	     return ret;
	  }
	else
	  return EINA_TRUE;
     }

   char *ret = NULL;
   if (keybuf_util_str_pred_p(q, isupper))
     ret = strstr(s, q);
   else
     ret = strcasestr(s, q);
   return ret ? EINA_TRUE : EINA_FALSE;
}

static Eina_Bool
_keybuf_update_cb_window_match_p(const Hint *h, const void *param)
{
   Eina_Bool matchp = EINA_FALSE;
   int i;
   const char *buf = param;
   char **ss = eina_str_split(buf+1, " ", 0);
   if (!ss) return EINA_FALSE;
#define MATBREAK(prop, query) \
   if ((h->client->prop) && \
       (_keybuf_update_cb_window_match_p_aux(h->client->prop, query))) \
     matchp = EINA_TRUE; \
   if (matchp) break;
   for (i = 0; ss[i]; i++)
     {
	MATBREAK(icccm.name, ss[i]);
	MATBREAK(icccm.class, ss[i]);
	MATBREAK(icccm.title, ss[i]);
	/*MATBREAK(desk.name, ss[i]);*/
     }
#undef MATBREAK
   free(ss[0]); free(ss);
   return matchp;
}
