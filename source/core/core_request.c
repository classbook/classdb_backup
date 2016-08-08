/**
 * @file core_request.c
 * @author Kishore
 * @date March 25, 2016, 8:58 PM
 * 
 * @brief 
 * 
 * 
 */

#include "headers/core_request.h"
#include "headers/core_epoll.h"
#include "headers/core_memory.h"
#include "common/headers/common_utility.h"
#include "headers/core_ansi.h"
#include "libraries/headers/lib_http.h"

request_t *list = NULL;
static int first_free_unit = 0;

static int n_open_connections=0;
/**
 * Initializes the request module. This function should be called
 * prior to calling any other function in this module. It initializes
 * the internal data structures.
 */
void request_init(void) {
	ansi_sinfo("Initializing Request Controller Module...");
	list = malloc(sizeof (request_t) * REQUEST_MAXREQUESTS);
	int i = 0;
	for (i = 0; i < REQUEST_MAXREQUESTS; i++) {
		list[i]._next = i + 1;
		list[i].free = TRUE;
	}
	list[REQUEST_MAXREQUESTS - 1]._next = -1;
	ansi_info("\rOK >> Request Controller.\n");
}

/**
 * Creates a new epoll instance and returns the epoll file descriptor.
 * @returns epoll file descriptor.
 */
int request_store() {
	int epfd = epoll_create1(0);
	assert(epfd != -1);
	return epfd;
}

/**
 * Wait for events to occur on fds stored in reqfd and store event
 * related data in req array buffer.
 * @param reqfd : requests store id.
 * @param req : request_get_t structure where the events list should be stored.
 * @param wtime : wait till events occur, or timeout...
 * @returns number of descriptors that have events occured.
 */
inline int request_wait(int reqfd, request_get_t *req, int req_len, int wtime) {
	struct epoll_event events[req_len];
	int status = epoll_wait(reqfd, events, req_len, wtime);

	if (status == -1)
		return -1;

	int i = 0;
	for (i = 0; i < status; i++) {
		request_t *r = list + events[i].data.fd;
		req[i].req_id = events[i].data.fd;
		assert(!(r->free));
		req[i].data = r->data;
		req[i].mem_index = r->mem_index;
		req[i].fd = r->fd;
		req[i].events = events[i].events;
		req[i].http = &(r->http);
	}
	return status;
}

/**
 * Adds file descriptor `fd` to the interest list of `reqfd`.
 * @param reqfd : The epoll file descriptor.
 * @param fd : File descriptor whose events needs to be watched.
 * @param interest : combination of w and r, showing interest.
 * @param create_memory : Boolean, whether to create memory instance or not.
 * @param data : Additional data if any to bind with request.
 * @returns request identifier on success and -1 on error.
 */
request_get_t request_add(int reqfd, int fd, char *interest, int create_memory, int data) {
	request_get_t req;
	if (first_free_unit == -1) {
		ansi_error("No of requests overflown.\n");
		req.req_id = -1;
		return req;
	}
	assert(list[first_free_unit].free);
	
	epoll_add(reqfd, fd, interest, first_free_unit);
	list[first_free_unit].fd = fd;

	if (create_memory){
		list[first_free_unit].mem_index = memory_create();
		list[first_free_unit].http = http_init(
			memory_pointer_start(list[first_free_unit].mem_index)
		);
	}
	else{
		list[first_free_unit].mem_index = -1;
	}

	list[first_free_unit].data = data;
	list[first_free_unit].free = FALSE;
	
	req.data = data;
	req.events = -1;
	req.fd = fd;
	req.mem_index = list[first_free_unit].mem_index;
	req.req_id = first_free_unit;
	
	first_free_unit = list[first_free_unit]._next;

	n_open_connections++;
	return req;
}

/**
 * Modifies the interest list of file descriptor in `request_id` to `interest`.
 * @param reqfd : Epoll file descriptor.
 * @param request_id : Internal id whose interest list needs to be modified.
 * @param interest : Combination of 'r' and 'w' showing the interest.
 * @returns boolean success or failure.
 */
int request_modify(int req_fd, int request_id, char* interest, int data) {
	if (list[request_id].free)
		return -1;
	list[request_id].data = data;
	return epoll_modify(req_fd, list[request_id].fd, request_id, interest);
}

/**
 * Deletes file descriptor from interest list.
 * @param reqfd : Epoll file descriptor.
 * @param request_index : Request identifier.
 */
void request_delete(int req_fd, int request_index) {
	assert(!list[request_index].free);
	list[request_index].free = TRUE;
	list[request_index]._next = first_free_unit;
	if (list[request_index].mem_index != -1) {
		memory_delete(list[request_index].mem_index);
	}
	if (epoll_remove(req_fd, list[request_index].fd)==-1){
		ansi_error("Error trying to remove epoll from epoll instance.\n");
		exit(1);
	};
	first_free_unit = request_index;
	n_open_connections--;
}


void request_debug()
{
	ansi_uline_text("REQUEST : %d open connections\n", n_open_connections);
}