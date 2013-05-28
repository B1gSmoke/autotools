/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MURPHY_LUA_OBJECT_H__
#define __MURPHY_LUA_OBJECT_H__

#include <lualib.h>
#include <lauxlib.h>

#define MRP_LUA_ENTER                           \
    mrp_debug("enter")

#define MRP_LUA_LEAVE(_v)                       \
    do {                                        \
        mrp_debug("leave (%d)", (_v));          \
        return (_v);                            \
    } while (0)

#define MRP_LUA_LEAVE_NOARG                     \
    mrp_debug("leave")


#define MRP_LUA_CLASSID_ROOT              "LuaBook."

#define MRP_LUA_CLASS(_name, _constr)     & _name ## _ ## _constr ## _class_def
#define MRP_LUA_CLASS_SIMPLE(_name)       & _name ## _class_def

#define MRP_LUA_METHOD_LIST(...)          { __VA_ARGS__  {NULL, NULL}}

#define MRP_LUA_METHOD(_name, _func)      { # _name, _func } ,
#define MRP_LUA_METHOD_CONSTRUCTOR(_func) { "new", _func } ,

#define MRP_LUA_OVERRIDE_GETFIELD(_func)  { "__index", _func } ,
#define MRP_LUA_OVERRIDE_SETFIELD(_func)  { "__newindex", _func } ,
#define MRP_LUA_OVERRIDE_CALL(_func)      { "__call", _func } ,
#define MRP_LUA_OVERRIDE_STRINGIFY(_func) { "__tostring", _func } ,
#define MRP_LUA_OVERRIDE_GETLENGTH(_func) { "__len", _func } ,

#define MRP_LUA_METHOD_LIST_TABLE(_name, ... ) \
    static luaL_reg _name[] = {                \
        __VA_ARGS__                            \
        { NULL, NULL }                         \
    }


#define MRP_LUA_CLASS_DEF(_name, _constr, _type, _destr, _methods, _overrides)\
    static mrp_lua_classdef_t _name ## _ ## _constr ## _class_def = {         \
        .class_name    = # _name ,                                            \
        .class_id      = MRP_LUA_CLASSID_ROOT # _name "_" # _constr,          \
        .constructor   = # _name "." # _constr,                               \
        .destructor    = _destr,                                              \
        .userdata_id   = MRP_LUA_CLASSID_ROOT #_name "." #_constr ".userdata",\
        .userdata_size = sizeof(_type),                                       \
        .methods       = _methods,                                            \
        .overrides     = _overrides                                           \
    }

#define MRP_LUA_CLASS_DEF_SIMPLE(_name, _type, _destr, _methods, _overrides) \
    static luaL_reg _name ## _class_methods[]   = _methods;             \
    static luaL_reg _name ## _class_overrides[] = _overrides;           \
                                                                        \
    static mrp_lua_classdef_t _name ## _class_def = {                   \
        .class_name    = # _name ,                                      \
        .class_id      = MRP_LUA_CLASSID_ROOT # _name,                  \
        .constructor   = # _name,                                       \
        .destructor    = _destr,                                        \
        .userdata_id   = MRP_LUA_CLASSID_ROOT # _name ".userdata",      \
        .userdata_size = sizeof(_type),                                 \
        .methods       = _name ## _class_methods,                       \
        .overrides     = _name ## _class_overrides                      \
    }

#define MRP_LUA_FOREACH_FIELD(_L, _i, _n, _l)                           \
    for (lua_pushnil(_L);                                               \
                                                                        \
         lua_next(_L, _i) &&                                            \
         (_n = (lua_type(_L, -2) == LUA_TSTRING) ?                      \
          lua_tolstring(_L, -2, &_l) : ""       ) &&                    \
         ((lua_type(_L, -2) == LUA_TSTRING) || !( _l = 0));             \
                                                                        \
         lua_pop(_L, 1))

typedef struct mrp_lua_classdef_s     mrp_lua_classdef_t;
typedef enum   mrp_lua_event_type_e   mrp_lua_event_type_t;

enum mrp_lua_event_type_e {
    MRP_LUA_OBJECT_DESTRUCTION = 1,
};

struct mrp_lua_classdef_s {
    const char   *class_name;
    const char   *class_id;
    const char   *constructor;
    void        (*destructor)(void *);
    const char   *userdata_id;
    size_t        userdata_size;
    luaL_reg     *methods;
    luaL_reg     *overrides;
};

void  mrp_lua_create_object_class(lua_State *L, mrp_lua_classdef_t *class);
void  mrp_lua_get_class_table(lua_State *L, mrp_lua_classdef_t *def);

void *mrp_lua_create_object(lua_State *L, mrp_lua_classdef_t *class,
                            const char *name, int);
void  mrp_lua_set_object_name(lua_State  *L, mrp_lua_classdef_t *def,
                              const char *name);
void mrp_lua_set_object_index(lua_State *L, mrp_lua_classdef_t *def, int idx);

void  mrp_lua_destroy_object(lua_State *L, const char *name,int, void *object);

int   mrp_lua_find_object(lua_State *L, mrp_lua_classdef_t *def,
                          const char *name);

void *mrp_lua_check_object(lua_State *L, mrp_lua_classdef_t *def, int argnum);
void *mrp_lua_to_object(lua_State *L, mrp_lua_classdef_t *def, int idx);
int   mrp_lua_push_object(lua_State *L, void *object);

mrp_lua_classdef_t *mrp_lua_get_object_classdef(void *);


#endif  /* __MURPHY_LUA_OBJECT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
