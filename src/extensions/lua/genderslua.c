
/*****************************************************************************
 *  Copyright (C) 2018 SUSE LLC
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
#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <lua.h>
#include <genders.h>

typedef struct {
    genders_t * handle;
    char      *name;
 } lgenders_userdata_t;
