/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"

/* Searches a global directory map array for which map entry an input "key" falls in.
 * "key" could be an unsubscripted or subscripted global reference.
 */
gd_binding *gv_srch_map(gd_addr *addr, char *key, int key_len)
{
	int		res;
	int		low, high, mid;
	gd_binding	*map_start, *map;
	DEBUG_ONLY(
		int	dbg_match;
	)

	map_start = addr->maps;
	assert(('%' == map_start[1].gvkey.addr[0]) && (1 == map_start[1].gvname_len));
	/* We expect all callers to search for global names that start with "^%" or higher. */
	assert(0 <= memcmp(key, &(map_start[1].gvkey.addr[0]), key_len));
	low = 2;	/* get past local locks AND first map entry which is always "%" */
	high = addr->n_maps - 1;
	DEBUG_ONLY(dbg_match = -1;)
	/* At all times in the loop, "low" corresponds to the smallest possible value for "map"
	 * and "high" corresponds to the highest possible value for "map".
	 */
	do
	{
		if (low == high)
		{
			assert((-1 == dbg_match) || (low == dbg_match));
			return &map_start[low];
		}
		assert(low < high);
		mid = (low + high) / 2;
		assert(low <= mid);
		assert(mid < high);
		map = &map_start[mid];
		res = memcmp(key, &(map->gvkey.addr[0]), key_len);
		if (0 > res)
			high = mid;
		else if (0 < res)
			low = mid + 1;
		else if (key_len < (map->gvkey_len - 1))
			high = mid;
		else
		{
			assert(key_len == (map->gvkey_len - 1));
			low = mid + 1;
			DEBUG_ONLY(dbg_match = low;)
			PRO_ONLY(return &map_start[low];)
		}
	} while (TRUE);
}

/* Similar to gv_srch_map except that it does a linear search starting at a specific map and going FORWARD.
 * We expect this function to be invoked in case the caller expects the target map to be found very close to the current map.
 * This might be faster in some cases than a binary search of the entire gld map array (done by gv_srch_map).
 */
gd_binding *gv_srch_map_linear(gd_binding *start_map, char *key, int key_len)
{
	gd_binding	*map;
	int		res;
#	ifdef DEBUG
	gd_addr		*addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	map = start_map;
	DEBUG_ONLY(
		addr = TREF(gd_targ_addr);
		assert(map > addr->maps);
	)
	for ( ; ; map++)
	{
		assert(map < &addr->maps[addr->n_maps]);
		res = memcmp(key, &map->gvkey.addr[0], key_len);
		if (0 < res)
			continue;
		if (0 > res)
			break;
		/* res == 0 at this point */
		if (key_len < (map->gvkey_len - 1))
			break;
		assert(key_len == (map->gvkey_len - 1));
		map++;
		break;
	}
	return map;
}

/* Similar to gv_srch_map_linear except that it does the linear search going BACKWARD. */
gd_binding *gv_srch_map_linear_backward(gd_binding *start_map, char *key, int key_len)
{
	gd_binding	*map;
	int		res;
#	ifdef DEBUG
	gd_addr		*addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	map = start_map;
	DEBUG_ONLY(
		addr = TREF(gd_targ_addr);
		assert(map < &addr->maps[addr->n_maps]);
	)
	for ( ; ; map--)
	{
		assert(map >= addr->maps);
		res = memcmp(key, &map->gvkey.addr[0], key_len);
		if (0 < res)
			break;
		if (0 > res)
			continue;
		/* res == 0 at this point */
		if (key_len < (map->gvkey_len - 1))
			continue;
		assert(key_len == (map->gvkey_len - 1));
		break;
	}
	map++;
	assert(map > addr->maps);
	return map;
}
