/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"
#include "stringpool.h"

GBLREF io_pair		io_curr_device;
#define CR 13

int iorm_rdone(mint *x, int4 timeout)
{
	io_desc		*iod;
	d_rm_struct	*d_rm;
	struct RAB	*rab;
	int4		stat;
	int		ret;

	error_def(ERR_IOEOF);

	assert(timeout >= 0);
	iod = io_curr_device.in;
	d_rm = (d_rm_struct *)io_curr_device.in->dev_sp;
	assert(d_rm->inbuf_pos >= d_rm->inbuf);
	assert(d_rm->inbuf_pos <= d_rm->inbuf_top);
	/* if write buffer not empty, and there was no error */
	if ((FAB$M_PUT == d_rm->r.rab$l_ctx) && d_rm->outbuf != d_rm->outbuf_pos
		&& !iod->dollar.za)
		iorm_wteol(1, iod);
	d_rm->r.rab$l_ctx = FAB$M_GET;
	rab = &d_rm->r;
	ret = TRUE;
	if (d_rm->inbuf_pos == d_rm->inbuf_top || d_rm->inbuf_pos == d_rm->inbuf)
	{	/* no buffered input left */
		rab->rab$l_ubf = d_rm->inbuf;
		d_rm->l_usz = iod->width;
		stat = iorm_get(iod, timeout);
		switch (stat)
		{
		case RMS$_NORMAL:
			d_rm->inbuf_top = d_rm->inbuf + d_rm->l_rsz;
			*d_rm->inbuf_top++ = CR;
			d_rm->inbuf_pos = d_rm->inbuf;
			*x = (mint)*(unsigned char *)d_rm->inbuf_pos++;
			iod->dollar.x = 1;
			iod->dollar.y++;
			iod->dollar.za = 0;
			break;
		case RMS$_TMO:
			*x = (mint)-1;
			iod->dollar.za = 9;
			ret = FALSE;
			break;
		case RMS$_EOF:
			*x = (mint)0;
			if (iod->dollar.zeof)
			{
				iod->dollar.za = 9;
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			}
			iod->dollar.x = 0;
			iod->dollar.y++;
			iod->dollar.za = 0;
			iod->dollar.zeof = TRUE;
			if (iod->error_handler.len > 0)
				rts_error(VARLSTCNT(1) ERR_IOEOF);
			break;
		default:
			d_rm->inbuf_pos = d_rm->inbuf;
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(2) stat, rab->rab$l_stv);
		}
	} else
	{
		*x = (mint)*(unsigned char *)d_rm->inbuf_pos++;
		iod->dollar.x++;
		iod->dollar.za = 0;
	}
	return ret;
}
