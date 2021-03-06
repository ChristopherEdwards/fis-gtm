/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <fab.h>
#include <rab.h>
#include <rmsdef.h>

#include "compiler.h"
#include <rtnhdr.h>
#include "srcline.h"
#include "zroutines.h"
#include "op.h"
#include "gt_timer.h"
#include "zbreak.h"
#include "hashtab_mname.h"
#include "rtn_src_chksum.h"

#define RT_TBL_SZ 20

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret, rhdtyp **rtn_vec)
{
	struct FAB		fab;
	struct RAB		rab;
	struct NAM		nam;
	bool			added;
	unsigned char		buff[MAX_SRCLINE], *cp1, *cp2, *cp3;
	unsigned char		es[255], srcnamebuf[SIZEOF(mident_fixed) + STR_LIT_LEN(DOTM)];
	boolean_t		badfmt, found;
	int			*lt_ptr, tmp_int, status;
	uint4			lcnt, srcstat, *src_tbl;
	mstr			src;
	rhdtyp			*rtn_vector;
	zro_ent			*srcdir;
	mstr			*base, *current, *top;
	ht_ent_mname		*tabent;
	var_tabent		rtnent;
	gtm_rtn_src_chksum_ctx	checksum_ctx;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	srcstat = 0;
	*srcret = NULL;
	if (NULL == (TREF(rt_name_tbl)).base)
		init_hashtab_mname(TADR(rt_name_tbl), RT_TBL_SZ, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	assert(routine->mvtype & MV_STR);
	if (NULL == (rtn_vector = find_rtn_hdr(&routine->str)))	/* Note assignment */
	{
		op_zlink(routine, NULL);
		rtn_vector = find_rtn_hdr(&routine->str);
		if (!rtn_vector)
		{
			if (NULL != rtn_vec)
				*rtn_vec = NULL;
			return OBJMODMISS;
		}
	}
	if (!rtn_vector->src_full_name.len)
	{
		if (NULL != rtn_vec)
			*rtn_vec = NULL;
		return SRCNOTAVAIL;
	}
	if (NULL != rtn_vec)
		*rtn_vec = rtn_vector;
	rtnent.var_name = rtn_vector->routine_name;
	COMPUTE_HASH_MNAME(&rtnent);
	added = add_hashtab_mname(TADR(rt_name_tbl), &rtnent, NULL, &tabent);
	src_tbl = (uint4 *)tabent->value;
	if (added || 0 == tabent->value)
	{
		fab = cc$rms_fab;
		fab.fab$l_fna = rtn_vector->src_full_name.addr;
		fab.fab$b_fns = rtn_vector->src_full_name.len;
		fab.fab$l_nam = &nam;
		nam = cc$rms_nam;
		nam.nam$l_esa = es;
		nam.nam$b_ess = SIZEOF(es);
		status = sys$parse(&fab);
		if (!(status & 1))
			found = FALSE;
		else
		{
			status = sys$search(&fab);
			if (!(status & 1))
				found = FALSE;
			else
				found = TRUE;
		}
		if (!found)
		{
			tmp_int = rtn_vector->routine_name.len;
			memcpy (srcnamebuf, rtn_vector->routine_name.addr, tmp_int);
			if ('%' == srcnamebuf[0])	/* percents are translated to _ on filenames */
				srcnamebuf[0] = '_';
			MEMCPY_LIT(&srcnamebuf[tmp_int], DOTM);
			src.addr = srcnamebuf;
			src.len = tmp_int + STR_LIT_LEN(DOTM);
			zro_search(0, 0, &src, &srcdir);
			if (srcdir)
			{
				fab.fab$l_fna = srcnamebuf;
				fab.fab$b_fns = tmp_int + 2;
				fab.fab$l_dna = srcdir->str.addr;
				fab.fab$b_dns = srcdir->str.len;
				found = TRUE;
			}
		}
		if (!found)
			srcstat |= SRCNOTFND;
		else
		{
			fab.fab$b_fac = FAB$M_GET;
			fab.fab$b_shr = FAB$M_SHRGET;
			for (lcnt = 0;  lcnt < MAX_FILE_OPEN_TRIES;  lcnt++)
			{
				status = sys$open(&fab);
				if (RMS$_FLK != status)
					break;
				hiber_start(WAIT_FOR_FILE_TIME);
			}
			if (RMS$_NORMAL != status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			rab = cc$rms_rab;
			rab.rab$l_fab = &fab;
			rab.rab$l_ubf = buff;
			rab.rab$w_usz = SIZEOF(buff);
			if (RMS$_NORMAL != (status = sys$connect(&rab)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
		}

		tmp_int = found ? rtn_vector->lnrtab_len : 0;
		assert((found && tmp_int >= 1) || (0 == tmp_int));
		/* first two words are the status code and the number of entries */
		src_tbl = (uint4 *)malloc(tmp_int * SIZEOF(mstr) + SIZEOF(uint4) * 2);
		base = (mstr *)(src_tbl + 2);
		*(src_tbl + 1) = tmp_int;	/* So zlput_rname knows how big we are */
		badfmt = FALSE;
		rtn_src_chksum_init(&checksum_ctx);
		for (current = base + 1, top = base + tmp_int;  current < top;  current++)
		{
			status = sys$get(&rab);
			if (RMS$_EOF == status)
			{
				badfmt = TRUE;
				break;
			} else  if (RMS$_NORMAL != status)
			{
				free(src_tbl);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
			}
			if (rab.rab$w_rsz)
			{
				/* set cp1 = to start of line separator */
				for (cp1 = buff , cp2 = cp1 + rab.rab$w_rsz;
					cp1 < cp2 && (' ' != *cp1) && ('\t' != *cp1);  cp1++)
						;
				/* calculate checksum */
				rtn_src_chksum_line(&checksum_ctx, buff, cp2 - buff);
				current->len = rab.rab$w_rsz;
				current->addr = malloc(rab.rab$w_rsz);
				memcpy(current->addr, buff, rab.rab$w_rsz);
			} else
			{
				current->addr = malloc(1);
				current->addr[0] = ' ';
				current->len = 1;
			}
		}
		if (found)
		{
			*base = *(base + 1);
			if (!badfmt)
			{
				status = sys$get(&rab);
				rtn_src_chksum_digest(&checksum_ctx);
				if ((RMS$_EOF != status)
					|| !rtn_src_chksum_match(get_ctx_checksum(&checksum_ctx), get_rtnhdr_checksum(rtn_vector)))
					badfmt = TRUE;
			}
			sys$close(&fab);
			if (badfmt)
				srcstat |= CHECKSUMFAIL;
		}
		*src_tbl = srcstat;
		tabent->value = (char *)src_tbl;
	}
	srcstat |= *src_tbl;
	lt_ptr = find_line_addr(rtn_vector, &label->str, 0, NULL);
	if (!lt_ptr)
		srcstat |= LABELNOTFOUND;
	else  if (!(srcstat & (SRCNOTFND | SRCNOTAVAIL)))
	{
		tmp_int = (int)(lt_ptr - (int *)LNRTAB_ADR(rtn_vector));
		tmp_int += offset;
		if (0 == tmp_int)
			srcstat |= ZEROLINE;
		else  if (tmp_int < 0)
			srcstat |= NEGATIVELINE;
		else  if (tmp_int >= rtn_vector->lnrtab_len)
			srcstat |= AFTERLASTLINE;
		else	/* successfully located line */
			*srcret = ((mstr *)(src_tbl + 2)) + tmp_int;
	}
	return srcstat;
}

void free_src_tbl(rhdtyp *rtn_vector)
{
	ht_ent_mname    *tabent;
	mname_entry	key;
	uint4		entries;
	mstr		*curline;
	uint4		*src_tbl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If source has been read in for old routine, free space. Since routine name is the key, do this before
	   (in USHBIN builds) we release the literal text section as part of the releasable read-only section.
	 */
	tabent = NULL;
	if (NULL != (TREF(rt_name_tbl)).base)
	{
		key.var_name = rtn_vector->routine_name;
		COMPUTE_HASH_MNAME(&key);
		tabent = lookup_hashtab_mname(TADR(rt_name_tbl), &key);
		if ((NULL != tabent) && tabent->value)
		{
			src_tbl = (uint4 *)tabent->value;
			/* Must delete the entries piece-meal */
			entries = *(src_tbl + 1);
			/* Don't count line 0 which we bypass */
			if (0 != entries)
				entries--;
			/* curline start is 2 uint4s into src_tbl and then space past line 0 or
			   we end up freeing the storage for line 0/1 twice since they have the
			   same pointers.
			*/
			for (curline = RECAST(mstr *)(src_tbl + 2) + 1; 0 != entries; --entries, ++curline)
			{
				assert(curline->len);
				free(curline->addr);
			}
			free(tabent->value);
			/* Comment below kept intact from when UNIX also used a $TEXT hash table. [BC 9/13]
			 *
			 * Note that there are two possible ways to proceed here to clear this entry:
			 *   1. Just clear the value as we do below.
			 *   2. Use the DELETE_HTENT() macro to remove the entry entirely from the hash table.
			 *
			 * We choose #1 since a routine that had had its source loaded is likely to have it reloaded
			 * and if the source load rtn has to re-add the key, it won't reuse the deleted key (if it
			 * remained a deleted key) until all other hashtable slots have been used up (creating a long
			 * collision chain). A deleted key may not remain a deleted key if it was reached with no
			 * collisions but will instead be turned into an unused key and be immediately reusable.
			 * But since it is likely to be reused, we just zero the entry but this creates a necessity
			 * that the key be maintained. If this is a non-USBHIN platform, everything stays around
			 * anyway so that's not an issue. However, in a USHBIN platform, the literal storage the key
			 * is pointing to gets released. For that reason, in the USHBIN processing section below, we
			 * update the key to point to the newly loaded module's routine name.
			 */
			tabent->value = NULL;
		}
	}
}
