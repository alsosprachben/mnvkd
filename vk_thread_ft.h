#ifndef VK_THREAD_FT_H
#define VK_THREAD_FT_H

/* receive message */
#define vk_recv(recv_msg)                                                                                              \
	((recv_msg) = vk_future_get(vk_ft_dequeue(that)))

/* call coroutine, passing pointers to messages to and from it */
#define vk_request(there, request_ft_ptr, send_msg, recv_msg)                                                          \
	do {                                                                                                           \
		vk_future_bind((request_ft_ptr), that);                                                                \
		vk_future_resolve((request_ft_ptr), (void*)(send_msg));                                                \
		vk_ft_enqueue((there), (request_ft_ptr));                                                              \
		vk_call(there);                                                                                        \
		*(request_ft_ptr) = *vk_ft_dequeue(that);                                                              \
		(recv_msg) = vk_future_get(request_ft_ptr);                                                            \
	} while (0)

/* pause coroutine for request, receiving message on play */
#define vk_listen(recv_ft_ptr, recv_msg)                                                                               \
	do {                                                                                                           \
		while ((recv_ft_ptr = vk_ft_peek(that)) == NULL) {                                                     \
			vk_yield(VK_PROC_LISTEN);                                                                      \
		}                                                                                                      \
		vk_recv(recv_msg);                                                                                     \
	} while (0)

/* play the coroutine of the resolved future */
#define vk_respond(send_ft_ptr, send_msg)                                                                              \
	do {                                                                                                           \
                vk_future_resolve((send_ft_ptr), (void *)(send_msg));                                                  \
		vk_ft_enqueue(((send_ft_ptr)->vk), (send_ft_ptr));                                                     \
		vk_play((send_ft_ptr)->vk);                                                                            \
	} while (0)


/* async call coroutine, passing pointer to message to it, but without a return
 * vk_play() is before vk_future_bind,
 * in case the parent is specified from send_ft_ptr, so
 * it gets played before re-assignment
 */
#define vk_send(there, send_ft_ptr, send_msg)                                                                          \
	do {                                                                                                           \
		vk_play(there);                                                                                        \
		vk_ft_enqueue((there), (send_ft_ptr));                                                                 \
		vk_future_bind((send_ft_ptr), that);                                                                   \
		vk_future_resolve((send_ft_ptr), (void*)(send_msg));                                                   \
	} while (0)

#endif
