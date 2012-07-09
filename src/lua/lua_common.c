/*
 * Copyright (c) 2009, Rambler media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Rambler media ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Rambler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "lua_common.h"
#include "expressions.h"

/* Lua module init function */
#define MODULE_INIT_FUNC "module_init"

/* Global lua mutex */
GMutex *lua_mtx = NULL;

const luaL_reg                  null_reg[] = {
	{"__tostring", lua_class_tostring},
	{NULL, NULL}
};

/* Logger methods */
LUA_FUNCTION_DEF (logger, err);
LUA_FUNCTION_DEF (logger, warn);
LUA_FUNCTION_DEF (logger, info);
LUA_FUNCTION_DEF (logger, debug);

static const struct luaL_reg    loggerlib_m[] = {
	LUA_INTERFACE_DEF (logger, err),
	LUA_INTERFACE_DEF (logger, warn),
	LUA_INTERFACE_DEF (logger, info),
	LUA_INTERFACE_DEF (logger, debug),
	{"__tostring", lua_class_tostring},
	{NULL, NULL}
};

/* Util functions */
void
lua_newclass (lua_State * L, const gchar *classname, const struct luaL_reg *func)
{
	luaL_newmetatable (L, classname);	/* mt */
	lua_pushstring (L, "__index");
	lua_pushvalue (L, -2);		/* pushes the metatable */
	lua_settable (L, -3);		/* metatable.__index = metatable */

	lua_pushstring (L, "class");	/* mt,"__index",it,"class" */
	lua_pushstring (L, classname);	/* mt,"__index",it,"class",classname */
	lua_rawset (L, -3);			/* mt,"__index",it */

	luaL_openlib (L, NULL, func, 0);
}

gint
lua_class_tostring (lua_State * L)
{
	gchar                           buf[32];

	if (!lua_getmetatable (L, 1)) {
		goto error;
	}
	lua_pushstring (L, "__index");
	lua_gettable (L, -2);

	if (!lua_istable (L, -1)) {
		goto error;
	}
	lua_pushstring (L, "class");
	lua_gettable (L, -2);

	if (!lua_isstring (L, -1)) {
		goto error;
	}

	snprintf (buf, sizeof (buf), "%p", lua_touserdata (L, 1));

	lua_pushfstring (L, "%s: %s", lua_tostring (L, -1), buf);

	return 1;

  error:
	lua_pushstring (L, "invalid object passed to 'lua_common.c:__tostring'");
	lua_error (L);
	return 1;
}


void
lua_setclass (lua_State * L, const gchar *classname, gint objidx)
{
	luaL_getmetatable (L, classname);
	if (objidx < 0) {
		objidx--;
	}
	lua_setmetatable (L, objidx);
}

/* assume that table is at the top */
void
lua_set_table_index (lua_State * L, const gchar *index, const gchar *value)
{

	lua_pushstring (L, index);
	if (value) {
		lua_pushstring (L, value);
	}
	else {
		lua_pushnil (L);
	}
	lua_settable (L, -3);
}

static void
lua_common_log (GLogLevelFlags level, lua_State *L, const gchar *msg)
{
	lua_Debug                      d;
	gchar                           func_buf[128], *p;

	if (lua_getstack (L, 1, &d) == 1) {
		(void)lua_getinfo(L, "Sl", &d);
		if ((p = strrchr (d.short_src, '/')) == NULL) {
			p = d.short_src;
		}
		else {
			p ++;
		}
		rspamd_snprintf (func_buf, sizeof (func_buf), "%s:%d", p, d.currentline);
		if (level == G_LOG_LEVEL_DEBUG) {
			rspamd_conditional_debug(rspamd_main->logger, -1, func_buf, msg);
		}
		else {
			rspamd_common_log_function(rspamd_main->logger, level, func_buf, msg);
		}
	}
	else {
		if (level == G_LOG_LEVEL_DEBUG) {
			rspamd_conditional_debug(rspamd_main->logger, -1, __FUNCTION__, msg);
		}
		else {
			rspamd_common_log_function(rspamd_main->logger, level, __FUNCTION__, msg);
		}
	}
}

/*** Logger interface ***/
static gint
lua_logger_err (lua_State * L)
{
	const gchar                     *msg;
	msg = luaL_checkstring (L, 1);
	lua_common_log (G_LOG_LEVEL_CRITICAL, L, msg);
	return 1;
}

static gint
lua_logger_warn (lua_State * L)
{
	const gchar                     *msg;
	msg = luaL_checkstring (L, 1);
	lua_common_log (G_LOG_LEVEL_WARNING, L, msg);
	return 1;
}

static gint
lua_logger_info (lua_State * L)
{
	const gchar                     *msg;
	msg = luaL_checkstring (L, 1);
	lua_common_log (G_LOG_LEVEL_INFO, L, msg);
	return 1;
}

static gint
lua_logger_debug (lua_State * L)
{
	const gchar                     *msg;
	msg = luaL_checkstring (L, 1);
	lua_common_log (G_LOG_LEVEL_DEBUG, L, msg);
	return 1;
}


/*** Init functions ***/

gint
luaopen_rspamd (lua_State * L)
{
	luaL_openlib (L, "rspamd", null_reg, 0);
	/* make version string available to scripts */
	lua_pushstring (L, "_VERSION");
	lua_pushstring (L, RVERSION);
	lua_rawset (L, -3);

	return 1;
}

gint
luaopen_logger (lua_State * L)
{

	luaL_openlib (L, "rspamd_logger", loggerlib_m, 0);

	return 1;
}

static void
lua_add_actions_global (lua_State *L)
{
	gint							i;

	lua_newtable (L);

	for (i = METRIC_ACTION_REJECT; i <= METRIC_ACTION_NOACTION; i ++) {
		lua_pushstring (L, str_action_metric (i));
		lua_pushinteger (L, i);
		lua_settable (L, -3);
	}
	/* Set global table */
	lua_setglobal (L, "rspamd_actions");
}

void
init_lua (struct config_file *cfg)
{
	lua_State                      *L;

	L = lua_open ();
	luaL_openlibs (L);

#if ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION <= 30))
	lua_mtx = g_mutex_new ();
#else
	lua_mtx = g_malloc (sizeof (GMutex));
	g_mutex_init (lua_mtx);
#endif

	(void)luaopen_rspamd (L);
	(void)luaopen_logger (L);
	(void)luaopen_mempool (L);
	(void)luaopen_config (L);
	(void)luaopen_session (L);
	(void)luaopen_radix (L);
	(void)luaopen_hash_table (L);
	(void)luaopen_trie (L);
	(void)luaopen_task (L);
	(void)luaopen_textpart (L);
	(void)luaopen_image (L);
	(void)luaopen_url (L);
	(void)luaopen_message (L);
	(void)luaopen_classifier (L);
	(void)luaopen_statfile (L);
	(void)luaopen_glib_regexp (L);
	(void)luaopen_cdb (L);
	(void)luaopen_xmlrpc (L);
	(void)luaopen_http (L);
	(void)luaopen_redis (L);
	(void)luaopen_upstream (L);
	(void)lua_add_actions_global (L);

	cfg->lua_state = L;
	memory_pool_add_destructor (cfg->cfg_pool, (pool_destruct_func)lua_close, L);

}

gboolean
init_lua_filters (struct config_file *cfg)
{
	struct config_file            **pcfg;
	GList                          *cur, *tmp;
	struct script_module           *module;
    struct statfile                *st;
	lua_State                      *L = cfg->lua_state;

	cur = g_list_first (cfg->script_modules);
	while (cur) {
		module = cur->data;
		if (module->path) {
			if (luaL_loadfile (L, module->path) != 0) {
				msg_info ("load of %s failed: %s", module->path, lua_tostring (L, -1));
				cur = g_list_next (cur);
				return FALSE;
			}

			/* Initialize config structure */
			pcfg = lua_newuserdata (L, sizeof (struct config_file *));
			lua_setclass (L, "rspamd{config}", -1);
			*pcfg = cfg;
			lua_setglobal (L, "rspamd_config");

			/* do the call (0 arguments, N result) */
			if (lua_pcall (L, 0, LUA_MULTRET, 0) != 0) {
				msg_info ("init of %s failed: %s", module->path, lua_tostring (L, -1));
				return FALSE;
			}
			if (lua_gettop (L) != 0) {
				if (lua_tonumber (L, -1) == -1) {
					msg_info ("%s returned -1 that indicates configuration error", module->path);
					return FALSE;
				}
				lua_pop (L, lua_gettop (L));
			}
		}
		cur = g_list_next (cur);
	}
    /* Init statfiles normalizers */
    cur = g_list_first (cfg->statfiles);
    while (cur) {
        st = cur->data;
        if (st->normalizer == lua_normalizer_func) {
            tmp = st->normalizer_data;
            if (tmp && (tmp = g_list_next (tmp))) {
                if (tmp->data) {
                    /* Code must be loaded from data */
                    if (luaL_loadstring (L, tmp->data) != 0) {
                        msg_info ("cannot load normalizer code %s", tmp->data);
                        return FALSE;
                    }
                }
            }
        }
        cur = g_list_next (cur);
    }
	/* Assign state */
	cfg->lua_state = L;

	return TRUE;
}

/* Callback functions */

gint
lua_call_filter (const gchar *function, struct worker_task *task)
{
	gint                            result;
	struct worker_task            **ptask;
	lua_State                      *L = task->cfg->lua_state;

	g_mutex_lock (lua_mtx);
	lua_getglobal (L, function);
	ptask = lua_newuserdata (L, sizeof (struct worker_task *));
	lua_setclass (L, "rspamd{task}", -1);
	*ptask = task;

	if (lua_pcall (L, 1, 1, 0) != 0) {
		msg_info ("call to %s failed", function);
	}

	/* retrieve result */
	if (!lua_isnumber (L, -1)) {
		msg_info ("function %s must return a number", function);
	}
	result = lua_tonumber (L, -1);
	lua_pop (L, 1);				/* pop returned value */
	g_mutex_unlock (lua_mtx);

	return result;
}

gint
lua_call_chain_filter (const gchar *function, struct worker_task *task, gint *marks, guint number)
{
	gint                            result;
	guint                           i;
	lua_State                      *L = task->cfg->lua_state;

	g_mutex_lock (lua_mtx);
	lua_getglobal (L, function);

	for (i = 0; i < number; i++) {
		lua_pushnumber (L, marks[i]);
	}
	if (lua_pcall (L, number, 1, 0) != 0) {
		msg_info ("call to %s failed", function);
	}

	/* retrieve result */
	if (!lua_isnumber (L, -1)) {
		msg_info ("function %s must return a number", function);
	}
	result = lua_tonumber (L, -1);
	lua_pop (L, 1);				/* pop returned value */
	g_mutex_unlock (lua_mtx);

	return result;
}

/* Call custom lua function in rspamd expression */
gboolean 
lua_call_expression_func (const gchar *module, const gchar *function,
		struct worker_task *task, GList *args, gboolean *res)
{
	lua_State                      *L = task->cfg->lua_state;
	struct worker_task            **ptask;
	GList                          *cur;
	struct expression_argument     *arg;
	int                             nargs = 1, pop = 0;

	g_mutex_lock (lua_mtx);
	/* Call specified function and expect result of given expected_type */
	/* First check function in config table */
	lua_getglobal (L, "config");
	if (module != NULL && lua_istable (L, -1)) {
		lua_pushstring (L, module);
		lua_gettable (L, -2);
		if (lua_isnil (L, -1)) {
			/* Try to get global variable */
			lua_pop (L, 1);
			lua_getglobal (L, function);
		}
		else if (lua_istable (L, -1)) {
			/* Call local function in table */
			lua_pushstring (L, function);
			lua_gettable (L, -2);
			pop += 2;
		}
		else {
			msg_err ("Bad type: %s for function: %s for module: %s", lua_typename (L, lua_type (L, -1)), function, module);
		}
	}
	else {
		/* Try to get global variable */
		lua_pop (L, 1);
		lua_getglobal (L, function);
	}
	if (lua_isnil (L, -1)) {
		if (pop > 0) {
			lua_pop (L, pop);
		}
		msg_err ("function with name %s is not defined", function);
		return FALSE;
	}
	/* Now we got function in top of stack */
	ptask = lua_newuserdata (L, sizeof (struct worker_task *));
	lua_setclass (L, "rspamd{task}", -1);
	*ptask = task;
	
	/* Now push all arguments */
	cur = args;
	while (cur) {
		arg = get_function_arg (cur->data, task, FALSE);
		if (arg) {
			switch (arg->type) {
				case EXPRESSION_ARGUMENT_NORMAL:
					lua_pushstring (L, (const gchar *)arg->data);
					break;
				case EXPRESSION_ARGUMENT_BOOL:
					lua_pushboolean (L, (gboolean) GPOINTER_TO_SIZE (arg->data));
					break;
				default:
					msg_err ("cannot pass custom params to lua function");
					return FALSE;
			}
		}
		nargs ++;
		cur = g_list_next (cur);
	}

	if (lua_pcall (L, nargs, 1, 0) != 0) {
		msg_info ("call to %s failed: %s", function, lua_tostring (L, -1));
		g_mutex_unlock (lua_mtx);
		return FALSE;
	}
	pop ++;

	if (!lua_isboolean (L, -1)) {
		lua_pop (L, pop);
		msg_info ("function %s must return a boolean", function);
		g_mutex_unlock (lua_mtx);
		return FALSE;
	}
	*res = lua_toboolean (L, -1);
	lua_pop (L, pop);

	g_mutex_unlock (lua_mtx);
	return TRUE;
}


/*
 * LUA custom consolidation function
 */
struct consolidation_callback_data {
	struct worker_task             *task;
	double                          score;
	const gchar                     *func;
};

static void
lua_consolidation_callback (gpointer key, gpointer value, gpointer arg)
{
	double                          res;
	struct symbol                  *s = (struct symbol *)value;
	struct consolidation_callback_data *data = (struct consolidation_callback_data *)arg;
	lua_State                      *L = data->task->cfg->lua_state;

	g_mutex_lock (lua_mtx);
	lua_getglobal (L, data->func);

	lua_pushstring (L, (const gchar *)key);
	lua_pushnumber (L, s->score);
	if (lua_pcall (L, 2, 1, 0) != 0) {
		msg_info ("call to %s failed", data->func);
	}

	/* retrieve result */
	if (!lua_isnumber (L, -1)) {
		msg_info ("function %s must return a number", data->func);
	}
	res = lua_tonumber (L, -1);
	lua_pop (L, 1);				/* pop returned value */
	data->score += res;
	g_mutex_unlock (lua_mtx);
}

double
lua_consolidation_func (struct worker_task *task, const gchar *metric_name, const gchar *function_name)
{
	struct metric_result           *metric_res;
	double                          res = 0.;
	struct consolidation_callback_data data = { task, 0, function_name };

	if (function_name == NULL) {
		return 0;
	}

	metric_res = g_hash_table_lookup (task->results, metric_name);
	if (metric_res == NULL) {
		return res;
	}

	g_hash_table_foreach (metric_res->symbols, lua_consolidation_callback, &data);

	return data.score;
}

double 
lua_normalizer_func (struct config_file *cfg, long double score, void *params)
{
    GList                          *p = params;
    long double                    	res = score;
	lua_State                      *L = cfg->lua_state;

    /* Call specified function and put input score on stack */
    if (!p->data) {
        msg_info ("bad function name while calling normalizer");
        return score;
    }

    g_mutex_lock (lua_mtx);
    lua_getglobal (L, p->data);
    lua_pushnumber (L, score);

    if (lua_pcall (L, 1, 1, 0) != 0) {
		msg_info ("call to %s failed", p->data);
	}

	/* retrieve result */
	if (!lua_isnumber (L, -1)) {
		msg_info ("function %s must return a number", p->data);
	}
	res = lua_tonumber (L, -1);
	lua_pop (L, 1);

	g_mutex_unlock (lua_mtx);
    return res;
}


void
lua_dumpstack (lua_State *L)
{
	gint 							i, t, r = 0;
	gint 							top = lua_gettop (L);
	gchar 							buf[BUFSIZ];

	r += rspamd_snprintf (buf + r, sizeof (buf) - r, "lua stack: ");
	for (i = 1; i <= top; i++) { /* repeat for each level */
		t = lua_type (L, i);
		switch (t)
		{
		case LUA_TSTRING: /* strings */
			r += rspamd_snprintf (buf + r, sizeof (buf) - r, "str: %s", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN: /* booleans */
			r += rspamd_snprintf (buf + r, sizeof (buf) - r,lua_toboolean (L, i) ? "bool: true" : "bool: false");
			break;

		case LUA_TNUMBER: /* numbers */
			r += rspamd_snprintf (buf + r, sizeof (buf) - r, "number: %.2f", lua_tonumber (L, i));
			break;

		default: /* other values */
			r += rspamd_snprintf (buf + r, sizeof (buf) - r, "type: %s", lua_typename (L, t));
			break;

		}
		if (i < top) {
			r += rspamd_snprintf (buf + r, sizeof (buf) - r, " -> "); /* put a separator */
		}
	}
	msg_info (buf);
}
