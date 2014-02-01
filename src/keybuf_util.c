#include <e.h>
#include "keybuf_util.h"

typedef int (*strncmpfn)(const char *s1, const char *s2, size_t n);
typedef int (*substrpred)(const char *str, const char *substr, size_t str_len, size_t substr_len, strncmpfn ncmp);

Eina_Bool
keybuf_util_str_substr_p(const char *str, const char *substr, strncmpfn ncmp, substrpred substrp)
{
   size_t str_len;
   size_t substr_len;

   str_len = strlen(str);
   substr_len = eina_strlen_bounded(substr, str_len);
   if (substr_len == (size_t)-1)
      return EINA_FALSE;

   return (*substrp)(str, substr, str_len, substr_len, ncmp);
}

int
keybuf_util_str_prefix_pred(const char *str, const char *substr, size_t str_len, size_t substr_len, strncmpfn ncmp)
{
   return (*ncmp)(str, substr, substr_len) == 0;
}

Eina_Bool
keybuf_util_str_prefix_p(const char *str, const char *prefix, strncmpfn ncmp)
{
   return keybuf_util_str_substr_p(str, prefix, ncmp,
				   keybuf_util_str_prefix_pred);
}

int
keybuf_util_str_suffix_pred(const char *str, const char *substr, size_t str_len, size_t substr_len, strncmpfn ncmp)
{
   return (*ncmp)(str + str_len - substr_len, substr, substr_len) == 0;
}

Eina_Bool
keybuf_util_str_suffix_p(const char *str, const char *suffix, strncmpfn ncmp)
{
   return keybuf_util_str_substr_p(str, suffix, ncmp,
				   keybuf_util_str_suffix_pred);
}

Eina_Bool
keybuf_util_str_pred_p(const char *str, int (*pred)(int c))
{
   while (*str)
     {
	if ((*pred)(*str))
	  return EINA_TRUE;
	str++;
     }
   return EINA_FALSE;
}

Eina_List *
keybuf_util_member(const Eina_List *list, Eina_Bool (*pred)(const void *data, const void *param), const void *param)
{
   const Eina_List *l;
   void *list_data;

   EINA_LIST_FOREACH(list, l, list_data)
     {
	if ((*pred)(list_data, param))
	  return (Eina_List *)l;
     }

   return NULL;
}

void *
keybuf_util_find(const Eina_List *list, Eina_Bool (*pred)(const void *data, const void *param), const void *param)
{
   Eina_List *l = NULL;

   l = keybuf_util_member(list, pred, param);
   if (l) return eina_list_data_get(l);
   return NULL;
}

void *
keybuf_util_accum_zone(void * (*proc)(E_Zone *zone, void *a), void *a)
{
   Eina_List *lc, *lz;
   E_Comp *c;
   E_Zone *z;

   EINA_LIST_FOREACH(e_comp_list(), lc, c)
     {
        EINA_LIST_FOREACH(c->zones, lz, z)
          {
             a = (*proc)(z, a);
          }
     }

   return a;
}
