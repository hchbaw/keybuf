#include "e.h"
#include "e_mod_main.h"
#include "keybuf_util.h"

#include <ctype.h>
#include <string.h>

typedef struct _Hint
{
   Evas_Object *bgobj;
   E_Popup     *popup;
   E_Border    *border;
   char        *label;
   int         focusstacknum;
   Ecore_Timer *timer;
} Hint;

typedef struct _Keymap Keymap;

struct _Keymap
{
   char *buf;
   Eina_Bool (*key_down)(Ecore_Event_Key *ev, Keymap *k);
};

typedef struct _Cwin
{
   Evas_Object *bgobj;
   E_Popup     *popup;
} Cwin;

static void _keybuf_hint_show(E_Desk *curdesk, Eina_Bool (*pred)(const E_Border *bd, const E_Desk *curdesk));
static Eina_Bool _keybuf_hint_cb_timeout(void *data);
static Eina_Bool _keybuf_hint_hintable_p(const E_Border *bd, const E_Desk *curdesk);
static int _keybuf_hint_cb_hint_sort(const void *d1, const void *d2);
static char *_keybuf_hint_labelify(int n, const char *labels);
static Hint *_keybuf_hint_show_hint(E_Border *bd, int focusstacknum, char *labelstring);
static void _keybuf_hint_del_hint(Hint *h);
static void _keybuf_key_clear(char *ptr);
static void _keybuf_key_compose(char *ptr, const char *compose);
static void _keybuf_update(char *param, Eina_Bool (*pred)(const Hint *h, const void *param), Eina_Bool focusp);
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

#define KEYBUFLEN 2048

int
keybuf_show(E_Zone *zone)
{
   input_window = ecore_x_window_input_new(zone->container->win, 0, 0, 1, 1);
   ecore_x_window_show(input_window);
   e_grabinput_get(input_window, 0, input_window);

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

   _keybuf_hint_show(e_desk_current_get(zone),
		     _keybuf_hint_hintable_p);

   _keybuf_activate(hints);
   return 0;
}

void
keybuf_hide(void)
{
   Ecore_Event_Handler *ev;
   Hint *h;
   Cwin *c;

   EINA_LIST_FREE(handlers, ev)
      ecore_event_handler_del(ev);

   ecore_x_window_free(input_window);
   e_grabinput_release(input_window, input_window);
   input_window = 0;

   free(key_buf);
   key_buf = NULL;

   free(key_buf_cmd);
   key_buf_cmd = NULL;

   EINA_LIST_FREE(cwins, c)
      _keybuf_cmd_del_cmd(c);

   EINA_LIST_FREE(hints, h)
     {
	if (hselected != h)
	  _keybuf_hint_del_hint(h);
     }
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
   _keybuf_hint_del_hint(h);

   return ECORE_CALLBACK_CANCEL;
}

struct ord_border
{
   E_Border *bd;
   int ord;
};

static void
_keybuf_hint_show(E_Desk *curdesk, Eina_Bool (*pred)(const E_Border *bd, const E_Desk *curdesk))
{
   Eina_List *l;
   E_Border *bd;
   Eina_List *bs = NULL;
   struct ord_border *ob = NULL;
   int i = 0;

   EINA_LIST_FOREACH(e_border_focus_stack_get(), l, bd)
      if ((*pred)(bd, curdesk))
	{
	   ob = E_NEW(struct ord_border, 1);
	   ob->bd = bd;
	   ob->ord = i;
	   bs = eina_list_sorted_insert(bs, _keybuf_hint_cb_hint_sort, ob);
	   i++;
	}

   if (eina_list_count(bs) == 0)
     {
	keybuf_hide();
	return;
     }

   i = 0;
   EINA_LIST_FOREACH(bs, l, ob)
     {
	Hint *h = NULL;

	h = _keybuf_hint_show_hint(ob->bd, ob->ord,
				   _keybuf_hint_labelify(i, "AOEUI"));
	free(ob);
	if (h)
	  {
	     hints = eina_list_append(hints, h);
	     i++;
	  }
     }
}

static Eina_Bool
_keybuf_hint_hintable_p(const E_Border *bd, const E_Desk *curdesk)
{
   if (bd->desk == curdesk) return EINA_TRUE;
   if (bd->sticky == 1) return EINA_TRUE;
   return EINA_FALSE;
}

static int
_keybuf_hint_cb_hint_sort(const void *d1, const void *d2)
{
   const struct ord_border *o1, *o2;

   if (!d1) return 1;
   if (!d2) return -1;

   o1 = d1, o2 = d2;
   return
      ((o1->bd->x + o1->bd->fx.x) - o1->bd->zone->x + o1->bd->w / 2) -
      ((o2->bd->x + o2->bd->fx.x) - o2->bd->zone->x + o2->bd->w / 2);
}

static char *
_keybuf_hint_labelify(int n, const char *labels)
{
   Eina_Strbuf *s = eina_strbuf_new();
   int b = strlen(labels);
   char *r = NULL;

   do
     {
	if (!eina_strbuf_append_char(s, labels[n % b])) return NULL;
	n = floor(n / b);
     }
   while (n > 0);

   r = eina_strbuf_string_steal(s);
   eina_strbuf_free(s);
   return r;
}

static Hint *
_keybuf_hint_show_hint(E_Border *bd, int focusstacknum, char *labelstring)
{
   Evas_Coord w, h;
   E_Popup *p = NULL;
   Evas_Object *o = NULL;
   char buf[PATH_MAX];
   Hint *hi = NULL;

   if (!labelstring) return NULL;

   p = e_popup_new(bd->zone, 0, 0, 1, 1);
   o = edje_object_add(p->evas);
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"modules/keybuf/hint"))
     edje_object_file_set(o, buf, "modules/keybuf/hint");
   edje_object_part_text_set(o, "e.text.label", labelstring);
   edje_object_size_min_calc(o, &w, &h);
   evas_object_move(o, 0, 0);
   evas_object_resize(o, w, h);
   evas_object_show(o);
   e_popup_edje_bg_object_set(p, o);
   e_popup_move_resize(p,
		       (bd->x + bd->fx.x) - (p->zone->x) + (bd->w - w) / 2,
		       (bd->y + bd->fx.y) - (p->zone->y) + (bd->h - h) / 2,
		       w, h);
   e_popup_show(p);

   hi = E_NEW(Hint, 1);
   hi->bgobj = o;
   hi->popup = p;
   hi->border = bd;
   hi->label = labelstring;
   hi->focusstacknum = focusstacknum;
   hi->timer = NULL;
   e_object_ref(E_OBJECT(bd));
   return hi;
}

static void
_keybuf_hint_del_hint(Hint *h)
{
   e_popup_hide(h->popup);

   if (h->timer) ecore_timer_del(h->timer);
   evas_object_del(h->bgobj);
   e_object_del(E_OBJECT(h->popup));
   e_object_unref(E_OBJECT(h->border));
   free(h->label);
   free(h);
   h = NULL;
}

static Eina_Bool
_keybuf_cb_key_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev = event;

   if (ev->event_window != input_window) return ECORE_CALLBACK_PASS_ON;

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
			    EINA_TRUE);
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
_keybuf_update(char *param, Eina_Bool (*pred)(const Hint *h, const void *param), Eina_Bool focusp)
{
   Eina_List *l;
   Hint *h;
   Eina_List *hs = NULL;

   EINA_LIST_FOREACH(hints, l, h)
     {
	if ((*pred)(h, param))
	  {
	     e_popup_show(h->popup);
	     hs = eina_list_append(hs, h);
	  }
	else
	  e_popup_hide(h->popup);
     }

   if (hs && eina_list_count(hs) > 0)
     {
	_keybuf_activate(hs);
	eina_list_free(hs);
	if (focusp) _keybuf_focus();
     }
}

static Eina_Bool
_keybuf_find_focused_border_cb(const void *data, const void *param)
{
   const Hint *h = data;
   const E_Border *focused = param;

   if (h->border == focused) return EINA_TRUE;
   return EINA_FALSE;
}

static void
_keybuf_activate(Eina_List *matches)
{
   Hint *old = hselected;

   hselected = NULL;

   if (matches && eina_list_count(matches) > 0)
     {
	if (curkeymap->buf[0] == 0)
	  {
	     hselected = keybuf_util_find(matches,
					  _keybuf_find_focused_border_cb,
					  (void *)e_border_focused_get());
	  }
	else
	  hselected = eina_list_data_get(matches);
     }

   if (old && old != hselected)
     edje_object_signal_emit(old->bgobj, "e,state,unselected", "e");

   if (hselected && old != hselected)
     edje_object_signal_emit(hselected->bgobj, "e,state,selected", "e");
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
   edje_object_signal_emit(h->bgobj, "e,state,focused", "e");

   ecore_x_pointer_warp(h->border->zone->container->win,
			h->border->x + (h->border->w / 2),
			h->border->y + (h->border->h / 2));
   e_border_raise(h->border);
   e_border_focus_set(h->border, 1, 1);
}

static Eina_Bool
_keybuf_keymap_key_down_command(Ecore_Event_Key *ev, Keymap *k)
{
   if (!strcmp(ev->key, "Escape"))
     {
	_keybuf_cmd_hide();
	/* TODO: update matches if it should */
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
   E_Popup *p = NULL;
   Evas_Object *o = NULL;
   char buf[PATH_MAX];
   Cwin *cwin = NULL;

   p = e_popup_new(zone, 0, 0, 1, 1);
   o = edje_object_add(p->evas);
   snprintf(buf, sizeof(buf), "%s/keybuf.edj",
	    e_module_dir_get(keybuf_config->module));
   if (!e_theme_edje_object_set(o, "base/theme/modules/keybuf",
				"modules/keybuf/cwin"))
     edje_object_file_set(o, buf, "modules/keybuf/cwin");
   edje_object_size_min_calc(o, &w, &h);
   evas_object_move(o, 0, 0);
   evas_object_resize(o, w, h);
   evas_object_show(o);
   e_popup_edje_bg_object_set(p, o);
   e_popup_move_resize(p,
		       ((zone->w - w) / 2),
		       ((zone->h - h) / 4 * 3),
		       w, h);
   e_popup_show(p);

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
   e_popup_hide(c->popup);

   evas_object_del(c->bgobj);
   e_object_del(E_OBJECT(c->popup));
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
   if ((h->border->prop) && \
       (_keybuf_update_cb_window_match_p_aux(h->border->prop, query))) \
     matchp = EINA_TRUE; \
   if (matchp) break;
   for (i = 0; ss[i]; i++)
     {
	MATBREAK(client.icccm.name, ss[i]);
	MATBREAK(client.icccm.class, ss[i]);
	MATBREAK(client.icccm.title, ss[i]);
	/*MATBREAK(desk.name, ss[i]);*/
     }
#undef MATBREAK
   free(ss[0]); free(ss);
   return matchp;
}
