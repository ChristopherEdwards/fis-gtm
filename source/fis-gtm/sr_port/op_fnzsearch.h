/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FNZSEARCH_H_INCLUDED
#define FNZSEARCH_H_INCLUDED

/* Define stream limits and special stream values used internally */

#define MAX_STRM_CT	256

#define STRM_ZRUPDATE   -1	/* Stream used by ZRUPDATE command when processing wildcards */
#define STRM_COMP_SRC	-2	/* Stream used by compile_source_file() */

#endif
