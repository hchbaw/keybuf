#ifndef KEYBUF_UTIL_H
#define KEYBUF_UTIL_H

#include <e.h>

Eina_Bool keybuf_util_str_prefix_p(const char *str, const char *prefix, int (*ncmp)(const char *s1, const char *s2, size_t n));
Eina_Bool keybuf_util_str_suffix_p(const char *str, const char *suffix, int (*ncmp)(const char *s1, const char *s2, size_t n));
Eina_Bool keybuf_util_str_pred_p(const char *str, int (*pred)(int c));
Eina_List *keybuf_util_member(const Eina_List *list, Eina_Bool (*pred)(const void *data, const void *param), const void *param);
void *keybuf_util_find(const Eina_List *list, Eina_Bool (*pred)(const void *data, const void *param), const void *param);
void *keybuf_util_accum_zone(void * (*proc)(E_Zone *zone, void *a), void *a);;

#endif
