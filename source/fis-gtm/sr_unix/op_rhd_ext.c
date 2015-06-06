/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef AUTORELINK_SUPPORTED /* entire file */
#include "gtm_string.h"

#include <rtnhdr.h>
#include "op.h"
#include "relinkctl.h"
#include "min_max.h"
#include "zbreak.h"

GBLREF z_records 	zbrk_recs;	/* ZBREAKs in effect */

/* Rebuffering macro for routine and label name for use when needed. Note we don't even do the
 * MV_FORCE_STR() on the given mval until we know we are going to use it.
 */
#define REBUFFER_MIDENT(MVAL, NEWMVAL, BUFFER)				\
{									\
	MV_FORCE_STR(MVAL);						\
	*(NEWMVAL) = *(MVAL);						\
	(NEWMVAL)->str.len = MIN(MAX_MIDENT_LEN, (NEWMVAL)->str.len);	\
	memcpy((BUFFER), (NEWMVAL)->str.addr, (NEWMVAL)->str.len);	\
	(NEWMVAL)->str.addr = (char *)&(BUFFER);			\
}

/* Routine called from both generated code and internally to check if a given routine/label need to be auto(re)linked and
 * do so if needbe.
 *
 * Parameters:
 *   - rtnname - address of mval pointing to text of routine name.
 *   - lblname - address of mval pointing to text of label name.
 *   - rhd     - address of routine header (if filled in in linkage table or NULL if not).
 *   - lnr     - address of linenumber table entry (offset) associated with text label or NULL if not yet linked.
 *
 * Return value:
 *   - routine header address of current routine.
 */
rhdtyp *op_rhd_ext(mval *rtname, mval *lbname, rhdtyp *rhd, void *lnr)
{
	lnr_tabent      **lnrptr;
	char		rtnname_buff[MAX_MIDENT_LEN], lblname_buff[MAX_MIDENT_LEN];
	mval		rtnname, lblname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Future TODO:
	 *   Remove the two additional opcodes and revert back to the single call opcode (op_call, op_extexfun, etc).
	 *   This would have positive performance enhancements through less generated code and shorter call path if doable.
	 */
	if (NULL == rhd)
	{	/* Routine is not yet linked - perform auto-ZLINK */
		REBUFFER_MIDENT(rtname, &rtnname, rtnname_buff);
		REBUFFER_MIDENT(lbname, &lblname, lblname_buff);
		rhd = op_rhdaddr1(&rtnname);
		op_labaddr(rhd, &lblname, 0); 	/* Offset != 0 would not go through op_rhd_ext */
		TREF(lab_lnr) = &((TREF(lab_proxy)).lnr_adr);
		/* lab_proxy now set by op_labaddr; ready for op_lab_ext next */
		return rhd;
	}
	/* Routine is already linked, but we need to check if a new version is available. This involves traversing the
	 * "validation linked list", looking for changes in different $ZROUTINES entries. But we also need to base our
	 * checks on the most recent version of the routine loaded. Note autorelink is only possible when no ZBREAKs are
	 * defined in the given routine.
	 */
	if (!rhd->has_ZBREAK)
	{	/* Only look for autorelink when no ZBREAKs are defined in this routine */
		rhd = rhd->current_rhead_adr;		/* Update rhd to most currently linked version */
		if ((NULL != rhd->zhist) && need_relink(rhd, (zro_hist *)rhd->zhist))
		{	/* Relink appears to be needed */
			REBUFFER_MIDENT(rtname, &rtnname, rtnname_buff);
			REBUFFER_MIDENT(lbname, &lblname, lblname_buff);
			op_zlink(&rtnname, NULL);
			rhd = rhd->current_rhead_adr;		/* Pickup routine header of new version to avoid lookup */
			assert((NULL == rhd->zhist) || (((zro_hist *)(rhd->zhist))->zroutines_cycle == TREF(set_zroutines_cycle)));
			op_labaddr(rhd, &lblname, 0);
			TREF(lab_lnr) = &((TREF(lab_proxy)).lnr_adr);
			return rhd;
		}
	}
	/* Linked routine is already the latest */
	TREF(lab_lnr) = lnr;
	return rhd;
}
#endif /* AUTORELINK_SUPPORTED */
