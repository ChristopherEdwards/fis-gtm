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
#include "io.h"
#include "iousdef.h"

GBLREF io_pair		io_curr_device;

void ious_iocontrol(mstr *mn, int4 argcnt, va_list args)
{
	return;
}

void ious_dlr_device(mstr *d)
{
	d->len = 0;
	return;
}

void ious_dlr_key(mstr *d)
{
	d->len = 0;
	return;
}
