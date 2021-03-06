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

#include "gtm_time.h"
#include <sys/time.h>

#include "dollarh.h"
#include "gtmimagename.h"
#include "have_crit.h"

void dollarh(time_t intime, uint4 *days, time_t *seconds)
{
	int4		tdays;
	int		isdst;
	struct tm	*ttime;
	time_t		mktime_ret;
#ifdef DEBUG
	static uint4	old_days = 0, old_seconds = 0;
	static time_t	old_intime = 0;
	static int	old_isdst;
#endif
	/* The comment below is now obsolete as GTM-6802 took care of protecting time-related functions from nested
	 * invocations, making the presence of sigprocmasks unnecessary. If several years pass without more incidents
	 * of time-caused hangs, remove the comment. (SOPINI, 2013/02/20)
	 *
	 * When dollarh() is processing and a signal occurs, the signal processing can eventually lead to nested system time
	 * routine calls.  If a signal arrives during a system time call (__tzset() in linux) we end up in a generic signal
	 * handler which invokes syslog which in turn tries to call the system time routine (__tz_convert() in linux) which
	 * seems to get suspended presumably waiting for the same interlock that __tzset() has already obtained.  A work around
	 * is to block signals (SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT, SIGALRM) during the function and then restore them
	 * at the end. [C9D06-002271] [C9I03-002967].
	 */
	GTM_LOCALTIME(ttime, &intime);		/* represent intime as local time in case of offsets from UTC other than hourly */
	*seconds  = (time_t)(ttime->tm_hour * HOUR) + (ttime->tm_min * MINUTE) + ttime->tm_sec;
	isdst = ttime->tm_isdst;
	GTM_GMTIME(ttime, &intime);		/* represent intime as UTC */
	ttime->tm_isdst = isdst; 		/* use GTM_LOCALTIME to tell mktime whether daylight savings needs to be applied */
	GTM_MKTIME(mktime_ret, ttime);
	assert((time_t)-1 != mktime_ret);
	/* adjust relative to UTC - have to round the other way if intime is less than or equal to the UTC offset */
	tdays = (int4)(intime - mktime_ret);
	tdays = (int4)(((intime + tdays - (((intime + tdays) > 0) ? 0 : (ONEDAY - 1))) / ONEDAY) + DAYS);
	*days = (uint4)tdays;				/* use temp local in case the caller has overlapped arguments */
	DEBUG_ONLY(old_seconds = (uint4)*seconds; old_days = *days; old_intime = intime; old_isdst = isdst;)
	return;
}
