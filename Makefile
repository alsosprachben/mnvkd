SRCS=vk_*.c
OBJS=vk_kern.o vk_fd_table.o vk_fd.o vk_io_future.o vk_signal.o vk_stack.o vk_wrapguard.o vk_heap.o vk_pool.o vk_proc.o vk_proc_local.o vk_future.o vk_thread.o vk_socket.o vk_vectoring.o vk_pipe.o vk_server.o vk_accepted.o vk_service.o

all: vk_test vk_http11

vk.a: ${OBJS}
	ar -r ${@} ${>}

vk_test: vk_test.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_http11: vk_http11.c vk_rfc.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend

depend: .depend

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm -f *.o *.a vk_test vk_http11
