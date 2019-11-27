/*****************************************************************************
 *  Copyright (C) 2019 SUSE LLC
 *  Written by Christian Goll <cgoll@suse.com>
 *
 *  This file is part of Genders, a cluster configuration database.
 *  For details, see <http://www.llnl.gov/linux/genders/>.
 *
 *  Genders is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Genders is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Genders.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/
#ifndef GENDERSLUA_H
#define GENDERSLUA_H
#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <genders.h>

/* compatibility macros */

#if LUA_VERSION_NUM <= 502

#undef  l_mathop
#define l_mathop(op)    op

#endif

#if LUA_VERSION_NUM <= 501

#define luaL_setmetatable(L,t)          \
        luaL_getmetatable(L,t);         \
        lua_setmetatable(L,-2)

#define luaL_setfuncs(L,r,n)            \
        luaL_register(L,NULL,r)

#endif
#endif
