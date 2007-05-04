/*
 * daemon/worker.h - worker that handles a pending list of requests.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file describes the worker structure that holds a list of 
 * pending requests and handles them.
 */

#ifndef DAEMON_WORKER_H
#define DAEMON_WORKER_H

#include "config.h"
#include "util/netevent.h"
#include "util/locks.h"
#include "util/alloc.h"
#include "util/data/msgreply.h"
#include "daemon/stats.h"
struct listen_dnsport;
struct outside_network;
struct config_file;
struct daemon;
struct listen_port;
struct ub_randstate;
struct region;

/** size of table used for random numbers. large to be more secure. */
#define RND_STATE_SIZE 256

/** worker commands */
enum worker_commands {
	/** make the worker quit */
	worker_cmd_quit
};

/** information per query that is in processing */
struct work_query {
	/** next query in freelist */
	struct work_query* next;
	/** the worker for this query */
	struct worker* worker;
	/** the query reply destination, packet buffer and where to send. */
	struct comm_reply query_reply;
	/** the query_info structure from the query */
	struct query_info qinfo;
	/** hash value of the query qinfo */
	hashvalue_t query_hash;
	/** next query in all-list */
	struct work_query* all_next;
	/** id of query, in network byteorder. */
	uint16_t query_id;
	/** flags uint16 from query */
	uint16_t query_flags;
};

/**
 * Structure holding working information for unbound.
 * Holds globally visible information.
 */
struct worker {
	/** the thread number (in daemon array). First in struct for debug. */
	int thread_num;
	/** global shared daemon structure */
	struct daemon* daemon;
	/** thread id */
	ub_thread_t thr_id;
	/** fd 0 of socketpair, write commands for worker to this one */
	int cmd_send_fd;
	/** fd 1 of socketpair, worker listens on this one */
	int cmd_recv_fd;
	/** the event base this worker works with */
	struct comm_base* base;
	/** the frontside listening interface where request events come in */
	struct listen_dnsport* front;
	/** the backside outside network interface to the auth servers */
	struct outside_network* back;
	/** the signal handler */
	struct comm_signal* comsig;
	/** commpoint to listen to commands. */
	struct comm_point* cmd_com;

	/** number of requests currently active */
	size_t num_requests;
	/** number of requests that can be handled by this worker */
	size_t request_size;
	/** the free working queries */
	struct work_query* free_queries;
	/** list of all working queries */
	struct work_query* all_queries;

	/** address to forward to */
	struct sockaddr_storage fwd_addr;
	/** length of fwd_addr */
	socklen_t fwd_addrlen;

	/** random() table for this worker. */
	struct ub_randstate* rndstate;
	/** do we need to restart (instead of exit) ? */
	int need_to_restart;
	/** allocation cache for this thread */
	struct alloc_cache alloc;
	/** per thread statistics */
	struct server_stats stats;
	/** thread scratch region */
	struct region* scratchpad;
};

/**
 * Create the worker structure. Bare bones version, zeroed struct,
 * with backpointers only. Use worker_init on it later.
 * @param daemon: the daemon that this worker thread is part of.
 * @param id: the thread number from 0.. numthreads-1.
 * @return: the new worker or NULL on alloc failure.
 */
struct worker* worker_create(struct daemon* daemon, int id);

/**
 * Initialize worker.
 * Allocates event base, listens to ports
 * @param worker: worker to initialize, created with worker_create.
 * @param cfg: configuration settings.
 * @param ports: list of shared query ports.
 * @param buffer_size: size of datagram buffer.
 * @param do_sigs: if true, worker installs signal handlers.
 * @return: false on error.
 */
int worker_init(struct worker* worker, struct config_file *cfg, 
	struct listen_port* ports, size_t buffer_size, int do_sigs);

/**
 * Make worker work.
 */
void worker_work(struct worker* worker);

/**
 * Delete worker.
 */
void worker_delete(struct worker* worker);

/**
 * Set forwarder
 * @param worker: the worker to modify.
 * @param ip: the server name.
 * @param port: port on server or NULL for default 53.
 * @return: false on error.
 */
int worker_set_fwd(struct worker* worker, const char* ip, int port);

/**
 * Send a command to a worker. Uses blocking writes.
 * @param worker: worker to send command to.
 * @param buffer: an empty buffer to use.
 * @param cmd: command to send.
 */
void worker_send_cmd(struct worker* worker, ldns_buffer* buffer,
        enum worker_commands cmd);

/**
 * Worker signal handler function. User argument is the worker itself.
 * @param sig: signal number.
 * @param arg: the worker (main worker) that handles signals.
 */
void worker_sighandler(int sig, void* arg);

#endif /* DAEMON_WORKER_H */
