/***

Copyright (C) 2015, 2016 Teclib'

This file is part of Armadito core.

Armadito core is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Armadito core is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Armadito core.  If not, see <http://www.gnu.org/licenses/>.

***/

#ifndef __LIBARMADITO_INFO_H_
#define __LIBARMADITO_INFO_H_

#include <time.h>
#include <stddef.h>

enum a6o_update_status {
	A6O_UPDATE_OK,
	A6O_UPDATE_LATE,
	A6O_UPDATE_CRITICAL,
	A6O_UPDATE_NON_AVAILABLE,
};

struct a6o_base_info {
	const char *name;
	time_t base_update_ts;
	const char *version;
	size_t signature_count;
	const char *full_path;
};

struct a6o_module_info {
	const char *name;
	enum a6o_update_status mod_status;
	time_t mod_update_ts;
	/* NULL terminated array of pointers to struct base_info */
	struct a6o_base_info **base_infos;
};

#endif
