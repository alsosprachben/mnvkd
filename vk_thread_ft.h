#ifndef VK_THREAD_FT_H
#define VK_THREAD_FT_H

/* call coroutine, passing pointer to message to it, but without a return */
#define vk_spawn(there, return_ft_ptr, send_msg) do { \
	(return_ft_ptr)->vk = that;                       \
	(return_ft_ptr)->msg = (void *) (send_msg);       \
	vk_ft_enqueue((there), (return_ft_ptr));          \
	vk_play(there);                                   \
} while (0)

/* call coroutine, passing pointers to messages to and from it */
#define vk_request(there, return_ft_ptr, send_msg, recv_msg) do { \
	(return_ft_ptr)->vk = that;                 \
	(return_ft_ptr)->msg = (void *) (send_msg); \
	vk_ft_enqueue((there), (return_ft_ptr));    \
	vk_call(there);                             \
	recv_msg = (return_ft_ptr)->msg;            \
} while (0)

/* pause coroutine for request, receiving message on play */
#define vk_listen(recv_ft_ptr) do { \
	while ((recv_ft_ptr = vk_ft_dequeue(that)) == NULL) { \
		vk_yield(VK_PROC_LISTEN); \
	} \
} while (0)

/* play the coroutine of the resolved future */
#define vk_respond(send_ft_ptr) do { \
	vk_play((send_ft_ptr)->vk);      \
} while (0)

#endif