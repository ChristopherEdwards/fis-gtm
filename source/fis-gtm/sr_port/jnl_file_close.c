/****************************************************************
 *								*
 *	Copyright 2003, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>		/* for offsetof, needed for JNL_FILE_TAIL_PRESERVE */

#include "gtm_time.h"
#include "gtm_string.h"

#if defined(UNIX)
#include "gtm_unistd.h"
#include "aswp.h"
#include "lockconst.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "performcaslatchcheck.h"
#include "wcs_sleep.h"
#include "gt_timer.h"
#include "wbox_test_init.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "gtmio.h"
#include "repl_sp.h"	/* F_CLOSE */
#include "iosp.h"	/* for SS_NORMAL */
#include "ccp.h"
#include "send_msg.h"
#include "eintr_wrappers.h"
#include "anticipatory_freeze.h"
#include "error.h"

#ifdef UNIX
#include "wcs_clean_dbsync.h"
#endif

#if defined(VMS)
GBLREF	short	astq_dyn_avail;
static	const	unsigned short	zero_fid[3];
#endif

GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		in_jnl_file_autoswitch;

error_def(ERR_JNLCLOSE);
error_def(ERR_JNLFLUSH);
error_def(ERR_JNLFSYNCERR);
error_def(ERR_JNLWRERR);
error_def(ERR_PREMATEOF);
error_def(ERR_TEXT);

void	jnl_file_close(gd_region *reg, bool clean, bool dummy)
{
	jnl_file_header		*header;
	unsigned char		hdr_base[REAL_JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	struct_jrec_eof		eof_record;
	off_jnl_t		eof_addr;
	uint4			status, read_write_size;
	int			rc, save_errno, idx;
	uint4			jnl_fs_block_size;
	boolean_t		was_in_jnl_file_autoswitch, was_last_eof_written, in_tail;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(!clean || csa->now_crit || (csd->clustered && (CCST_CLOSED == csa->nl->ccp_state)));
	DEBUG_ONLY(
		if (clean)
			ASSERT_JNLFILEID_NOT_NULL(csa);
	)
	jpc = csa->jnl;
#if defined(UNIX)
	if (csa->dbsync_timer)
		CANCEL_DBSYNC_TIMER(csa);
#elif defined(VMS)
	/* See comment about ordering of the two statements below, in similar code in gds_rundown */
	if (csa->dbsync_timer)
	{
		csa->dbsync_timer = FALSE;
		++astq_dyn_avail;
	}
	sys$cantim(csa, PSL$C_USER);	/* cancel all dbsync-timers for this region */
#endif
	if ((NULL == jpc) || (NOJNL == jpc->channel))
		return;
	jb = jpc->jnl_buff;
	jnl_fs_block_size = jb->fs_block_size;
	header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_base, jnl_fs_block_size));
	if (clean)
	{
		was_last_eof_written = jb->last_eof_written;
		if (!jgbl.mur_extract && !was_last_eof_written)
		{
			was_in_jnl_file_autoswitch = in_jnl_file_autoswitch;
			/* We don't want to switch while closing, so set in_jnl_file_autoswitch,
			 * which allows writing to the tail.
			 */
			in_jnl_file_autoswitch = TRUE;
			if (!was_in_jnl_file_autoswitch)
			{
				assert(!jgbl.save_dont_reset_gbl_jrec_time);
				/* Make sure the condition handler can restore the correct value */
				jgbl.save_dont_reset_gbl_jrec_time = jgbl.dont_reset_gbl_jrec_time;
				ESTABLISH(jnl_file_autoswitch_ch);
			}
			/* Otherwise the caller which set was_in_jnl_file_autoswitch also should have
			 * established the condition handler already.
			 */
			/* It is possible we still have not written a PINI record in this journal file
			 * (e.g. mupip extend saw the need to do jnl_file_extend inside jnl_write while
			 * trying to write a PINI record). Write a PINI record in that case before closing
			 * the journal file that way the EOF record will have a non-zero pini_addr.
			 * However, if we are already in the tail due to a prior disruption, skip the
			 * pini/pfin and just write the eof.
			 */
			DEBUG_ONLY(jpc->status = SS_NORMAL);
			in_tail = (jb->freeaddr > jb->filesize - JNL_FILE_TAIL_PRESERVE);
			if ((0 == jpc->pini_addr) && !in_tail)
			{
				jnl_put_jrt_pini(csa);
				jnl_put_jrt_pfin(csa);
			}
			jnl_write_eof_rec(csa, &eof_record);
			if (!was_in_jnl_file_autoswitch)
			{
				jgbl.dont_reset_gbl_jrec_time = jgbl.save_dont_reset_gbl_jrec_time;
				jgbl.save_dont_reset_gbl_jrec_time = FALSE;
				REVERT;
			} else
				/* We were called in autoswitch, so prevent further records from being
				 * written to this file by marking it as last_eof_written.
				 */
				jb->last_eof_written = TRUE;
			in_jnl_file_autoswitch = was_in_jnl_file_autoswitch;
		}
		if (SS_NORMAL != (jpc->status = jnl_flush(reg)))
		{
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during jnl_file_close"),
				jpc->status);
			assert(FALSE);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
				ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during jnl_file_close"),
				jpc->status);
		}
		assert(jb->dskaddr == jb->freeaddr);
		UNIX_ONLY(jnl_fsync(reg, jb->dskaddr);)
		UNIX_ONLY(assert(jb->freeaddr == jb->fsync_dskaddr);)
		eof_addr = jb->freeaddr - EOF_RECLEN;
		read_write_size = ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size);
		assert((unsigned char *)header + read_write_size <= ARRAYTOP(hdr_base));
		DO_FILE_READ(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
		if (SYSCALL_SUCCESS(jpc->status))
		{
			if (!jgbl.mur_extract && !was_last_eof_written)
			{
				/* If was_last_eof_written is TRUE, we didn't write an eof, so nothing to update. */
				assert(header->end_of_data <= eof_addr);
				header->end_of_data = eof_addr;
				header->eov_timestamp = eof_record.prefix.time;
				assert(header->eov_timestamp >= header->bov_timestamp);
				header->eov_tn = eof_record.prefix.tn;
				assert(header->eov_tn >= header->bov_tn);
				header->end_seqno = eof_record.jnl_seqno;
			}
#			ifdef UNIX
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				header->strm_end_seqno[idx] = csd->strm_reg_seqno[idx];
			if (jgbl.forw_phase_recovery)
			{	/* If MUPIP JOURNAL -ROLLBACK, might need some adjustment. See macro definition for comments */
				MUR_ADJUST_STRM_REG_SEQNO_IF_NEEDED(csd, header->strm_end_seqno);
			}
#			endif
			header->last_eof_written = jb->last_eof_written;
			header->crash = FALSE;
			JNL_DO_FILE_WRITE(csa, csd->jnl_file_name, jpc->channel,
				0, header, read_write_size, jpc->status, jpc->status2);
			if (SYSCALL_ERROR(jpc->status))
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLWRERR, 2, JNL_LEN_STR(csd), jpc->status);
			}
			UNIX_ONLY(
				GTM_JNL_FSYNC(csa, jpc->channel, rc);
				if (-1 == rc)
				{
					save_errno = errno;
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync during jnl_file_close"), save_errno);
					assert(FALSE);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync during jnl_file_close"), save_errno);
				}
			)
		}
		/* jnl_file_id should be nullified only after the jnl file header has been written to disk.
		 * Nullifying the jnl_file_id signals that the jnl file has been switched. The replication source server
		 * assumes that the jnl file has been completely written to disk (including the header) before the switch is
		 * signalled.
		 */
		NULLIFY_JNL_FILE_ID(csa);
		jb->cycle++;	/* increment shared cycle so all future callers of jnl_ensure_open recognize journal switch */
	}
	JNL_FD_CLOSE(jpc->channel, rc);	/* sets jpc->channel to NOJNL */
#ifdef UNIX
	GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_JNLCLOSE, rc, EIO);
#endif
	jpc->cycle--;	/* decrement cycle so jnl_ensure_open() knows to reopen the journal */
	VMS_ONLY(jpc->qio_active = FALSE;)
	jpc->pini_addr = 0;
	if (clean && (SS_NORMAL != jpc->status || SS_NORMAL != rc))
	{
		status = jpc->status;	/* jnl_send_oper resets jpc->status, so save it */
		jnl_send_oper(jpc, ERR_JNLCLOSE);
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLCLOSE, 2, JNL_LEN_STR(csd), status);
	}
}
