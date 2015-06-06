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

#include <efndef.h>
#include <signal.h>
#include <iodef.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>

#include "gtm_inet.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "cli.h"
#include "error.h"
#include "io.h"
#include "iottdef.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "desblk.h"		/* for desblk structure */
#include "util.h"
#include "dse.h"
#include "patcode.h"
#include "generic_exit_handler.h"
#include "dfntmpmbx.h"
#include "ladef.h"
#include "ast_init.h"
#include "init_secshr_addrs.h"
#include "dse_exit.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"

GBLDEF block_id		patch_curr_blk;

GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF desblk		exi_blk;
GBLREF int4		exi_condition;
GBLREF int4 		lkid;
GBLREF boolean_t        dse_running;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr          *gd_header;
GBLREF gd_addr          *original_header;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short		crash_count;
GBLREF bool		wide_out;
GBLREF spdesc		rts_stringpool, stringpool;
GBLREF boolean_t        	write_after_image;

error_def(ERR_CTRLC);

extern int		DSE_CMD();
extern int		CLI$DCL_PARSE();
extern int		CLI$DISPATCH();

$DESCRIPTOR	(prompt, "DSE> ");

static readonly mstr lnm$group = {9,  "LNM$GROUP"};
static void dse_process(void);

void dse(void)
{
	$DESCRIPTOR	(desc, "SYS$OUTPUT");
	char		buff[MAX_LINE];
	$DESCRIPTOR	(command, buff);
	unsigned short	outlen;
	uint4		status;
	int4		sysout_channel;
	t_cap		t_mode;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(DSE_IMAGE);
	gtm_env_init();	/* read in all environment variables */
	TREF(transform) = TRUE;
	TREF(no_spangbls) = TRUE;	/* dse operates on a per-region basis irrespective of global mapping in gld */
	util_out_open(0);
	SET_EXIT_HANDLER(exi_blk, generic_exit_handler, exi_condition);	/* Establish exit handler */
	ESTABLISH(util_base_ch);
	status =lp_id(&lkid);
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
	INVOKE_INIT_SECSHR_ADDRS;
	dfntmpmbx(lnm$group.len, lnm$group.addr);
	ast_init();
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	initialize_pattern_table();
	gvinit();
	region_init(FALSE);
	sys$assign(&desc, &sysout_channel, 0, 0);
	if (sysout_channel)
	{
		sys$qiow(EFN$C_ENF, sysout_channel, IO$_SENSEMODE, 0, 0, 0, &t_mode, 12, 0, 0, 0, 0);
		if (t_mode.pg_width >= 132)
			wide_out = TRUE;
		else
			wide_out = FALSE;
	} else
		wide_out = FALSE;
	if (cs_addrs)
		crash_count = cs_addrs->critical->crashcnt;
	patch_curr_blk = get_dir_root();
	REVERT;
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	dse_ctrlc_setup();
	/* Since DSE operates on a region-by-region basis (for the most part), do not use a global directory at all from now on */
	original_header = gd_header;
	gd_header = NULL;
	status = lib$get_foreign(&command, 0, &outlen, 0);
	if ((status & 1) && outlen > 0)
	{
		command.dsc$w_length = outlen;
		status = CLI$DCL_PARSE(&command, &DSE_CMD, &lib$get_input, 0, 0);
		if (RMS$_EOF == status)
			dse_exit();
		else if (CLI$_NORMAL == status)
		{
			ESTABLISH(util_ch);
			CLI$DISPATCH();
			REVERT;
		}
	}
	for (;;)
		dse_process();
}

static void dse_process(void)
{
	uint4	status;

	ESTABLISH(util_ch);
	status = CLI$DCL_PARSE(0, &DSE_CMD, &lib$get_input, &lib$get_input, &prompt);
	if (RMS$_EOF == status)
		dse_exit();
	else if (CLI$_NORMAL == status)
		CLI$DISPATCH();
	if (util_interrupt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
}
