/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "eintr_wrappers.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_pwd.h"
#include <grp.h>
#include "gtm_permissions.h"
#include "send_msg.h"

#if defined(__hpux) && defined(__hppa)
#	define LIBGTMSHR "%s/libgtmshr.sl"
#elif defined(__MVS__)
#	define LIBGTMSHR "%s/libgtmshr.dll"
#else
#	define LIBGTMSHR "%s/libgtmshr.so"
#endif
/* Return the group id of the distribution based on libgtmshr.xx[x]. If there is some
   problem accessing that file then return -1 which signals no change to group.  Otherwise,
   the pointer to the stat buffer will contain the result of the call to STAT_FILE */
int gtm_get_group_id(struct stat *stat_buff)
{
	char		*env_var;
	int		ret_stat;
	char		temp[PATH_MAX + SIZEOF("libgtmshr.dll")];

	env_var = GETENV("gtm_dist");
	if (NULL != env_var)
	{
		/* build a path to libgtmshr.so or .sl on hpux or .dll on zos */
		SNPRINTF(temp, SIZEOF(temp), LIBGTMSHR, env_var);
		STAT_FILE(temp, stat_buff, ret_stat);
		if (0 == ret_stat)
			return(stat_buff->st_gid);
	}
	/* return a -1 if $gtm_dist found or if STAT_FILE returned a -1 */
	return(-1);
}

/* Return TRUE is the "uid" parameter is a member of the "gid" group parameter.
   Return FALSE if it is not. */
int gtm_member_group_id(int uid, int gid)
{
	struct group	*grp;
	struct passwd	*pwd, *pwd2;

	/* check effective group if current effective uid */
	if ((GETEUID() == uid) && (GETEGID() == gid))
		return(TRUE);
	/* get group id for database */
	grp = getgrgid(gid);
	if (NULL == grp)
		return(FALSE); 	/* if group id not found then assume uid not a member */
	pwd = getpwuid(uid);
	if (NULL == pwd)
		return(FALSE); 	/* if user id not found then assume uid not a member */
	/* if the gid of the file is the same as the gid for the process uid we are done */
	if (gid == pwd->pw_gid)
		return(TRUE);
	else
	{
		/*
		 * Otherwise we have to compare the name stored in pwd struct
		 * with the names of the group members in the group struct.
		 */
		while (NULL != *(grp->gr_mem))
		{
			if (!strcmp(pwd->pw_name, *(grp->gr_mem++)))
				return(TRUE);
		}

		return(FALSE);
	}
}

/* Based on security rules in this routine, set *group_id of the group to be used
 * for shared resources, journals, and temp files.  If a no change then it will be set to -1.
 * Also, set *perm to the permissions to be used.  The precalculated world_write_perm (need to
 * change name to masked_permissions) is to be used in the one case indicated below.
 * If the user is root, the *user_id may be set to a target uid if needed; otherwise, it will
 * be set to -1.
 *
 * Populates pdd struct and returns negative value for error, returns non-negative for success.
 */
int gtm_permissions(struct stat *stat_buff, int *user_id, int *group_id, int *perm, enum perm_target_types target_type,
			   struct perm_diag_data *pdd)
{
	int		lib_gid = -1;
	int		use_world_writeable;
	uid_t		process_uid;
	gid_t		process_gid;
	uid_t		db_uid;
	struct stat	dist_stat_buff;
	int		opener_is_file_owner;
	int		opener_is_root;
	int		opener_in_file_group;
	int		owner_in_file_group;
	int		gtm_group_restricted;

	/* get process_uid/gid */
	process_uid = GETEUID();
	process_gid = GETEGID();
	/* get database uid */
	db_uid = stat_buff->st_uid;
	/* set variables for permission logic */
	opener_is_file_owner = (process_uid == db_uid);
	opener_is_root = (process_uid == 0);
	opener_in_file_group = gtm_member_group_id(process_uid, stat_buff->st_gid);
	owner_in_file_group = gtm_member_group_id(db_uid, stat_buff->st_gid);
	/* find restricted group, if any */
	lib_gid = gtm_get_group_id(&dist_stat_buff);
	gtm_group_restricted = (lib_gid != -1) && !(dist_stat_buff.st_mode & 01);	/* not world executable */

	/* set default gid */
#ifdef __osf__
	*user_id = process_uid;
	*group_id = process_gid;
#else
	*user_id = -1;
	*group_id = -1;
#endif
	/* set no permissions as a default in case none of our conditions match */
	*perm = 0;

	assertpro((PERM_FILE & target_type) || (PERM_IPC & target_type));
	if (0006 & stat_buff->st_mode)
	{
		/* file is accessible to other */
		if (opener_is_root)					/* otherwise, use default uid */
			*user_id = stat_buff->st_uid;
		if (opener_in_file_group || opener_is_root)		/* otherwise, use default gid */
			*group_id = stat_buff->st_gid;
		if (PERM_FILE & target_type)
			*perm = (!opener_in_file_group && !opener_is_root && (0020 & stat_buff->st_mode))
					? 0666 : (stat_buff->st_mode & 0666);
		else if (PERM_IPC & target_type)
			*perm = 0666;
	} else if (0600 & stat_buff->st_mode && !(0066 & stat_buff->st_mode))
	{
		/* file is only user accessible */
		/* use default group */
		assert(opener_is_file_owner || opener_is_root);
		if (opener_is_root)					/* otherwise, use default uid/gid */
		{
			*user_id = stat_buff->st_uid;
			*group_id = stat_buff->st_gid;
		}
		if (PERM_FILE & target_type)
			*perm = 0600;			/* read write for user */
		else if (PERM_IPC & target_type)
			*perm = 0600;			/* read write for user */
	} else if (0060 & stat_buff->st_mode && !(0606 & stat_buff->st_mode))
	{
		/* file is only group accessible */
		if (opener_is_root)					/* otherwise, use default uid */
			*user_id = stat_buff->st_uid;
		*group_id = stat_buff->st_gid;			/* use file group */
		assert(opener_in_file_group || opener_is_root);
		if (PERM_FILE & target_type)
			*perm = stat_buff->st_mode & 0060;	/* use file permissions, masked for group read/write */
		if (PERM_IPC & target_type)
			*perm = 0660;			/* read/write for group - all readers need write for ipc */
	} else
	{
		/* file is accessible to user and group */
		if (opener_is_file_owner && opener_in_file_group || opener_is_root)
		{
			if (opener_is_root)					/* otherwise, use default uid */
				*user_id = stat_buff->st_uid;
			*group_id = stat_buff->st_gid;		/* use file group */
			if (PERM_FILE & target_type)
				*perm = stat_buff->st_mode & 0660;	/* use file permissions, masked for user/group read/write */
			if (PERM_IPC & target_type)
				*perm = 0660;			/* read/write for user/group - all readers need write for ipc */
		} else
		{
			if (opener_is_file_owner && !opener_in_file_group)
			{
				if (gtm_group_restricted)
				{
					*group_id = lib_gid;			/* use restricted group */
					assert(gtm_member_group_id(process_uid, *group_id));
					if (PERM_FILE & target_type)
						*perm = 0660;			/* user/group read/write */
					if (PERM_IPC & target_type)
						*perm = 0660;	/* read/write for user/group - all readers need write for ipc */
				} else
				{
					/* use default group */
					if (PERM_FILE & target_type)
						*perm = 0666;			/* read/write for all */
					else if (PERM_IPC & target_type)
						*perm = 0666;	/* read/write for all - all readers need write for ipc */
				}
			} else if (!opener_is_file_owner && opener_in_file_group)
			{
				/* opener has access either via file group membership */
				if (owner_in_file_group)
				{
					*group_id = stat_buff->st_gid;		/* use file group */
					if (PERM_FILE & target_type)
						*perm = stat_buff->st_mode & 0660;	/* use masked file permissions */
					if (PERM_IPC & target_type)
						*perm = 0660;	/* read/write for user/group - all readers need write for ipc */

				} else if (gtm_group_restricted)
				{
					*group_id = lib_gid;			/* use restricted group */
					assert(gtm_member_group_id(process_uid, *group_id));
					if (PERM_FILE & target_type)
						*perm = 0660;			/* user/group read/write */
					if (PERM_IPC & target_type)
						*perm = 0660;	/* read/write for user/group - all readers need write for ipc */
				} else
				{
					*group_id = stat_buff->st_gid;		/* use file group */
					if (PERM_FILE & target_type)
						*perm = 0666;	/* read/write for all - ensure file owner read/write access */
					else if (PERM_IPC & target_type)
						*perm = 0666;	/* read/write for all - all readers need write for ipc */
				}
			}
		}
	}

	if (target_type & PERM_EXEC)
		*perm |= ((*perm & 0444) >> 2);	/* Grab the read bits, shift them to the exec bit position, and add them back in. */

	/* if we never set *perm, return error value */
	if (*perm == 0)
	{
		/* populate perm diag data */
		pdd->process_uid = process_uid;
		pdd->process_gid = process_gid;
		pdd->file_uid = stat_buff->st_uid;
		pdd->file_gid = stat_buff->st_gid;
		SNPRINTF(pdd->file_perm, SIZEOF(pdd->file_perm), "%04o", stat_buff->st_mode & 07777);
		pdd->lib_gid = dist_stat_buff.st_gid;
		SNPRINTF(pdd->lib_perm, SIZEOF(pdd->lib_perm), "%04o", dist_stat_buff.st_mode & 07777);
		pdd->opener_in_file_group = opener_in_file_group;
		pdd->owner_in_file_group = owner_in_file_group;

		return -1;
	} else
		return 0;
}
