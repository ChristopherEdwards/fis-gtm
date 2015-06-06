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

#include "gtm_tls.h"
#include "have_crit.h"

/* This file defines wrapper functions that defer interrupts, invoke the SSL/TLS function and enable interrupts. This guarantees
 * that system calls invoked by the SSL/TLS library (or OpenSSL) are not interrupted by internal (like SIGALRM) or external
 * signals (like SIGTERM) thereby avoiding any deadlocks. Note that this file has to be maintained in sync with gtm_tls_interface.h.
 */

const char		*intrsafe_gtm_tls_get_error(void)
{
	const char	*rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_get_error_fptr)();
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_errno(void)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_errno_fptr)();
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

gtm_tls_ctx_t		*intrsafe_gtm_tls_init(int version, int flags)
{
	gtm_tls_ctx_t	*rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_init_fptr)(version, flags);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

void			intrsafe_gtm_tls_prefetch_passwd(gtm_tls_ctx_t *tls_ctx, char *env_name)
{
	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	(*gtm_tls_prefetch_passwd_fptr)(tls_ctx, env_name);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
}

gtm_tls_socket_t	*intrsafe_gtm_tls_socket(gtm_tls_ctx_t *ctx, gtm_tls_socket_t *prev_socket, int sockfd, char *id, int flags)
{
	gtm_tls_socket_t	*rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_socket_fptr)(ctx, prev_socket, sockfd, id, flags);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_connect(gtm_tls_socket_t *socket)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_connect_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_accept(gtm_tls_socket_t *socket)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_accept_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_renegotiate(gtm_tls_socket_t *socket)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_renegotiate_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_get_conn_info(gtm_tls_socket_t *socket, gtm_tls_conn_info *conn_info)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_get_conn_info_fptr)(socket, conn_info);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_send(gtm_tls_socket_t *socket, char *buf, int send_len)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_send_fptr)(socket, buf, send_len);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_recv(gtm_tls_socket_t *socket, char *buf, int recv_len)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_recv_fptr)(socket, buf, recv_len);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

int			intrsafe_gtm_tls_cachedbytes(gtm_tls_socket_t *socket)
{
	int		rv;

	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	rv = (*gtm_tls_cachedbytes_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	return rv;
}

void			intrsafe_gtm_tls_socket_close(gtm_tls_socket_t *socket)
{
	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	(*gtm_tls_socket_close_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
}

void			intrsafe_gtm_tls_session_close(gtm_tls_socket_t **socket)
{
	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	(*gtm_tls_session_close_fptr)(socket);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
}

void			intrsafe_gtm_tls_fini(gtm_tls_ctx_t **ctx)
{
	DEFER_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
	(*gtm_tls_fini_fptr)(ctx);
	ENABLE_INTERRUPTS(INTRPT_IN_TLS_FUNCTION);
}

