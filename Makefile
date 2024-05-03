

SRCS=vk_*.c
OBJS=vk_kern.o vk_fd_table.o vk_fd.o vk_io_future.o vk_signal.o vk_stack.o vk_wrapguard.o vk_heap.o vk_pool.o vk_proc.o vk_proc_local.o vk_future.o vk_thread.o vk_socket.o vk_vectoring.o vk_pipe.o vk_server.o vk_accepted.o vk_service.o vk_main.o

all: test vk_test_echo_service vk_test_http11_service

vk.a: ${OBJS}
	ar -r ${@} ${>}

vk_test_echo_service:   vk_test_echo_service.c   vk_echo.c            vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_echo_cli:       vk_test_echo_cli.c       vk_echo.c            vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_http11_service: vk_test_http11_service.c vk_http11.c vk_rfc.c vk_fetch.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_http11_cli:     vk_test_http11_cli.c     vk_http11.c vk_rfc.c vk_fetch.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend test

depend: .depend

# vk_test_echo
vk_test_echo.out.txt: vk_test_echo_cli vk_test_echo.in.txt
	cat vk_test_echo.in.txt | ./vk_test_echo_cli > vk_test_echo.out.txt

vk_test_echo.valid.txt:
	cp vk_test_echo.out.txt "${@}"

vk_test_echo.passed: vk_test_echo.out.txt vk_test_echo.valid.txt
	diff -q vk_test_echo.out.txt vk_test_echo.valid.txt && touch "${@}"

# vk_test_http11
vk_test_http11.in.txt: pipeline.py
	./pipeline.py 3 POST-chunked > "${@}"

vk_test_http11.in1m.txt: pipeline.py
	./pipeline.py 1000000 GET > "${@}"

vk_test_http11_cli.out.txt: vk_test_http11_cli vk_test_http11.in.txt
	cat vk_test_http11.in.txt | ./vk_test_http11_cli > vk_test_http11_cli.out.txt

vk_test_http11_cli.out1m.txt: vk_test_http11_cli vk_test_http11.in1m.txt
	cat vk_test_http11.in1m.txt | ./vk_test_http11_cli > vk_test_http11_cli.out1m.txt

vk_test_http11_service.out.txt: vk_test_http11_service vk_test_http11.in.txt
	./vk_test_http11_service &
	cat vk_test_http11.in.txt | nc localhost 8081 > vk_test_http11_service.out.txt

vk_test_http11.valid.txt:
	cp vk_test_http11_cli.out.txt "${@}"

vk_test_http11.valid1m.txt:
	cp vk_test_http11_cli.out1m.txt "${@}"

vk_test_http11_cli.passed: vk_test_http11_cli.out.txt vk_test_http11.valid.txt
	diff -q vk_test_http11_cli.out.txt vk_test_http11.valid.txt && touch "${@}"

vk_test_http11_cli.passed1m: vk_test_http11_cli.out1m.txt vk_test_http11.valid1m.txt
	diff -q vk_test_http11_cli.out1m.txt vk_test_http11.valid1m.txt && touch "${@}"

vk_test_http11_service.passed: vk_test_http11_service.out.txt vk_test_http11.valid.txt
	diff -q vk_test_http11_service.out.txt vk_test_http11.valid.txt && touch "${@}"

test: vk_test_echo.passed vk_test_http11_cli.passed vk_test_http11_cli.passed1m

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm -f *.o *.a vk_test_echo_service vk_test_echo_cli vk_test_http11_service vk_test_http11_cli
