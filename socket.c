/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>

#include "log.h"
#include "job.h"
#include "socket.h"

/* The main kqueue descriptor used by launchd */
static int parent_kqfd;

/* The kqueue descriptor dedicated to socket activation */
static int socket_kqfd;

struct job_manifest_socket *
job_manifest_socket_new()
{
	struct job_manifest_socket *jms;

	jms = calloc(1, sizeof(*jms));
	if (!jms) return NULL;

	jms->sd = -1;
	jms->sock_type = SOCK_STREAM;
	jms->sock_passive = true;
	jms->sock_family = PF_INET;

	return (jms);
}

void job_manifest_socket_free(struct job_manifest_socket *jms)
{
	if (!jms) return;
	if (jms->sd >= 0) (void) close(jms->sd);
	free(jms->label);
	free(jms->sock_node_name);
	free(jms->sock_service_name);
	free(jms->sock_path_name);
	free(jms->secure_socket_with_key);
	free(jms->multicast_group);
	free(jms);
}

int job_manifest_socket_open(struct job_manifest_socket *jms)
{
	struct kevent kev;
	struct sockaddr_in sa;
	int sd = -1;

	if (jms->sock_type != SOCK_STREAM || !jms->sock_passive
			|| jms->sock_family != PF_INET || jms->sock_node_name) {
		log_error("FIXME -- not implemented yet");
		return -1;
	}

	sd = socket(jms->sock_family, jms->sock_type, 0);
	if (sd < 0) {
		log_errno("socket(2)");
		goto err_out;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = jms->sock_family;
	sa.sin_addr.s_addr = INADDR_ANY; /* TODO: use jms->sock_node_name */
	sa.sin_port = htons(jms->port);

	if (bind(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		log_errno("bind(2)");
		goto err_out;
	}

	/* TODO: make the backlog configurable */
	if (listen(sd, 500) < 0) {
		log_errno("listen(2)");
		goto err_out;
	}

	EV_SET(&kev, sd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(socket_kqfd, &kev, 1, NULL, 0, NULL) < 0) {
		log_errno("kevent(2)");
		goto err_out;
	}

	jms->sd = sd;

	return 0;

err_out:
	if (sd >= 0) (void) close(sd);
	return -1;
}

/* Given that the caller has filled in the sock_service_name variable, convert
 * this into a port number.
 */
int job_manifest_socket_get_port(struct job_manifest_socket *jms)
{
	struct servent *se;

	/* TODO: detect the protocol by looking at sock_family, and pass to getservbyname() */
	se = getservbyname(jms->sock_service_name, NULL);
	if (se) {
		jms->port = ntohs(se->s_port);
		/* TODO: copy the protocol from se to jms */
	} else {
		if (sscanf(jms->sock_service_name, "%d", &jms->port) != 1) {
			return -1;
		}
	}
	log_debug("converted service name '%s' to port %d",
			jms->sock_service_name, jms->port);
	return 0;
}

void setup_socket_activation(int kqfd)
{
	struct kevent kev;

	parent_kqfd = kqfd;
	socket_kqfd = kqueue();
	if (socket_kqfd < 0) abort();

	EV_SET(&kev, socket_kqfd, EVFILT_READ, EV_ADD, 0, 0, &setup_socket_activation);
	if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0) abort();
}

int socket_activation_handler()
{
	struct kevent kev;

	for (;;) {
		if (kevent(socket_kqfd, NULL, 0, &kev, 1, NULL) < 1) {
			if (errno == EINTR) {
				continue;
			} else {
				log_errno("kevent");
				return -1;
			}
		}
		break;
	}

	/* FIXME: this module has no visibility into all jobs :( */

	return 0;
}

