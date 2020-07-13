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

static struct json_object *jobj;

struct lang_cp {
    const char *lang;
    const char *cp;
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
    got = json_pointer_get(jso, "/codepage", &obj);
    if (got < 0)
	return JSON_C_VISIT_RETURN_ERROR;
    cp->cp = json_object_get_string(obj);
    return JSON_C_VISIT_RETURN_STOP;
}

static const char *json_get_charset_for_lang(const char *path,
	const char *lc)
{
    struct lang_cp cp;
    json_object *obj2;
    int got;

    jobj = json_object_from_file(path);
    if (!jobj)
	return NULL;
    got = json_pointer_get(jobj, "/locales", &obj2);
    if (got < 0)
	return NULL;
    cp.lang = lc;
    cp.cp = NULL;
    got = json_c_visit(obj2, 0, visit, &cp);
    if (got < 0)
	return NULL;
    return cp.cp;
}

static void locale_scrub(void)
{
    const char *lang = getenv("LANG");
    char *l2;
    char *dot;
    char *path;
    const char *cp;

    if (!lang)
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
    cp = get_charset_for_lang(path, l2);
    if (cp)
        set_internal_charset(cp);
    else
        error("Can't find codepage for \"%s\".\n"
              "Please add the mapping to locales.conf and send patch.\n",
              l2);
    free(path);
}

CONSTRUCTOR(static void init(void))
{
    get_charset_for_lang = json_get_charset_for_lang;
    register_config_scrub(locale_scrub);
}
