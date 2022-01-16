/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: load locales map from json
 *
 * Author: Stas Sergeev
 */
#include <stdlib.h>
#include <string.h>
#include <json-c/json_util.h>
#include <json-c/json_pointer.h>
#include <json-c/json_object.h>
#include <json-c/json_visit.h>
#include "init.h"
#include "dosemu_config.h"
#include "utilities.h"
#include "translate/dosemu_charset.h"

struct lang_cp {
    const char *lang;
    const char *node;
    const char *val;
    int ival;
    int is_int;
};

static int visit(json_object *jso, int flags, json_object *parent_jso,
    const char *jso_key, size_t *jso_index, void *userarg)
{
    struct lang_cp *cp = userarg;
    json_object *obj;
    int got;
    switch (json_object_get_type(jso)) {
    case json_type_array:
	return JSON_C_VISIT_RETURN_CONTINUE;
    case json_type_object:
	break;
    default:
	return JSON_C_VISIT_RETURN_ERROR;
    }
    got = json_pointer_get(jso, "/lang", &obj);
    if (got < 0)
	return JSON_C_VISIT_RETURN_ERROR;
    if (strcmp(json_object_get_string(obj), cp->lang) != 0)
	return JSON_C_VISIT_RETURN_SKIP;
    got = json_pointer_get(jso, cp->node, &obj);
    if (got < 0)
	return JSON_C_VISIT_RETURN_ERROR;
    if (cp->is_int)
	cp->ival = json_object_get_int(obj);
    else
	cp->val = json_object_get_string(obj);
    return JSON_C_VISIT_RETURN_STOP;
}

static const char *json_get_charset_for_lang(struct json_object *jobj,
	const char *lc)
{
    struct lang_cp cp;
    int got;

    cp.lang = lc;
    cp.node = "/codepage";
    cp.is_int = 0;
    cp.val = NULL;
    got = json_c_visit(jobj, 0, visit, &cp);
    if (got < 0)
	return NULL;
    return cp.val;
}

static int json_get_country_for_lang(struct json_object *jobj,
	const char *lc)
{
    struct lang_cp cp;
    int got;

    cp.lang = lc;
    cp.node = "/country";
    cp.is_int = 1;
    cp.ival = -1;
    got = json_c_visit(jobj, 0, visit, &cp);
    if (got < 0)
	return -1;
    return cp.ival;
}

static void charset_init(void)
{
    const char *lang = getenv("LANG");
    char *l2;
    char *dot;
    char *path;
    const char *cp;
    int cntry;
    int got;
    struct json_object *jobj, *obj2;

    if (!lang || strlen(lang) < 2)
	return;
    path = assemble_path(dosemu_lib_dir_path, "locales.conf");
    if (!exists_file(path)) {
	error("Can't find %s\n", path);
	free(path);
	return;
    }
    l2 = strdup(lang);
    dot = strchr(l2, '.');
    if (dot)
        *dot = '\0';
    jobj = json_object_from_file(path);
    if (!jobj) {
	error("json: cannot parse %s\n", path);
	goto done;
    }
    got = json_pointer_get(jobj, "/locales", &obj2);
    if (got < 0) {
	error("json: no locales defined in %s\n", path);
	goto done;
    }
    cp = json_get_charset_for_lang(obj2, l2);
    if (cp)
        set_internal_charset(cp);
    else
        error("Can't find codepage for \"%s\".\n"
              "Please add the mapping to locales.conf and send patch.\n",
              l2);
    cntry = json_get_country_for_lang(obj2, l2);
    if (cntry != -1)
        set_country_code(cntry);

done:
    free(l2);
    free(path);
}

CONSTRUCTOR(static void init(void))
{
    charset_init();
}
