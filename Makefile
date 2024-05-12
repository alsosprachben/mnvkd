

SRCS=vk_*.c
OBJS=vk_kern.o vk_fd_table.o vk_fd.o vk_io_future.o vk_signal.o vk_stack.o vk_wrapguard.o vk_heap.o vk_pool.o vk_proc.o vk_proc_local.o vk_future.o vk_thread.o vk_socket.o vk_vectoring.o vk_pipe.o vk_server.o vk_accepted.o vk_service.o vk_main.o vk_main_local.o

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

vk_test_signal: vk_signal.c
	${CC} ${CFLAGS} -DVK_SIGNAL_TEST -o ${@} ${>}

vk_test_cr:             vk_test_cr.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_log:            vk_test_log.c                                 vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_exec:           vk_test_exec.c                                vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_mem:           vk_test_mem.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_ft:            vk_test_ft.c                                   vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_ft2:           vk_test_ft2.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_ft3:           vk_test_ft3.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_err:           vk_test_err.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_read:		  vk_test_read.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_write:		  vk_test_write.c                                 vk.a
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

# vk_test_signal
vk_test_signal.out.txt: vk_test_signal
	./vk_test_signal > vk_test_signal.out.txt

vk_test_signal.valid.txt:
	cp vk_test_signal.out.txt vk_test_signal.valid.txt

vk_test_signal.passed: vk_test_signal.out.txt vk_test_signal.valid.txt
	diff -q vk_test_signal.out.txt vk_test_signal.valid.txt && touch "${@}"

# vk_test_cr
vk_test_cr.out.txt: vk_test_cr
	./vk_test_cr > vk_test_cr.out.txt

vk_test_cr.valid.txt:
	cp vk_test_cr.out.txt vk_test_cr.valid.txt

vk_test_cr.passed: vk_test_cr.out.txt vk_test_cr.valid.txt
	diff -q vk_test_cr.out.txt vk_test_cr.valid.txt && touch "${@}"

# vk_test_log
vk_test_log.out.txt: vk_test_log
	./vk_test_log 2> vk_test_log.out.txt

vk_test_log.valid.txt:
	cp vk_test_log.out.txt vk_test_log.valid.txt

vk_test_log.passed: vk_test_log.out.txt vk_test_log.valid.txt
	diff -q vk_test_log.out.txt vk_test_log.valid.txt && touch "${@}"

# vk_test_exec
vk_test_exec.out.txt: vk_test_exec
	./vk_test_exec > vk_test_exec.out.txt

vk_test_exec.valid.txt:
	cp vk_test_exec.out.txt vk_test_exec.valid.txt

vk_test_exec.passed: vk_test_exec.out.txt vk_test_exec.valid.txt
	diff -q vk_test_exec.out.txt vk_test_exec.valid.txt && touch "${@}"

# vk_test_mem
vk_test_mem.out.txt: vk_test_mem
	./vk_test_mem > vk_test_mem.out.txt

vk_test_mem.valid.txt:
	cp vk_test_mem.out.txt vk_test_mem.valid.txt

vk_test_mem.passed: vk_test_mem.out.txt vk_test_mem.valid.txt
	diff -q vk_test_mem.out.txt vk_test_mem.valid.txt && touch "${@}"

# vk_test_ft
vk_test_ft.out.txt: vk_test_ft
	./vk_test_ft > vk_test_ft.out.txt

vk_test_ft.valid.txt:
	cp vk_test_ft.out.txt vk_test_ft.valid.txt

vk_test_ft.passed: vk_test_ft.out.txt vk_test_ft.valid.txt
	diff -q vk_test_ft.out.txt vk_test_ft.valid.txt && touch "${@}"

# vk_test_ft2
vk_test_ft2.out.txt: vk_test_ft2
	./vk_test_ft2 > vk_test_ft2.out.txt

vk_test_ft2.valid.txt:
	cp vk_test_ft2.out.txt vk_test_ft2.valid.txt

vk_test_ft2.passed: vk_test_ft2.out.txt vk_test_ft2.valid.txt
	diff -q vk_test_ft2.out.txt vk_test_ft2.valid.txt && touch "${@}"

# vk_test_ft3
vk_test_ft3.out.txt: vk_test_ft3
	./vk_test_ft3 > vk_test_ft3.out.txt

vk_test_ft3.valid.txt:
	cp vk_test_ft3.out.txt vk_test_ft3.valid.txt

vk_test_ft3.passed: vk_test_ft3.out.txt vk_test_ft3.valid.txt
	diff -q vk_test_ft3.out.txt vk_test_ft3.valid.txt && touch "${@}"

# vk_test_err
vk_test_err.out.txt: vk_test_err
	./vk_test_err 2>&1 | grep ': ERR ' > vk_test_err.out.txt

vk_test_err.valid.txt:
	cp vk_test_err.out.txt vk_test_err.valid.txt

vk_test_err.passed: vk_test_err.out.txt vk_test_err.valid.txt
	diff -q vk_test_err.out.txt vk_test_err.valid.txt && touch "${@}"

# vk_test_write
vk_test_write.out.txt: vk_test_write
	./vk_test_write > vk_test_write.out.txt

vk_test_write.valid.txt:
	cp vk_test_write.out.txt vk_test_write.valid.txt

vk_test_write.passed: vk_test_write.out.txt vk_test_write.valid.txt
	diff -q vk_test_write.out.txt vk_test_write.valid.txt && touch "${@}"

# vk_test_read
vk_test_read.in.txt: vk_test_write
	./vk_test_write > vk_test_read.in.txt

vk_test_read.out.txt: vk_test_read vk_test_read.in.txt
	./vk_test_read < vk_test_read.in.txt > vk_test_read.out.txt

vk_test_read.valid.txt:
	cp vk_test_read.out.txt vk_test_read.valid.txt

vk_test_read.passed: vk_test_read.out.txt vk_test_read.valid.txt
	diff -q vk_test_read.out.txt vk_test_read.valid.txt && touch "${@}"

test: vk_test_echo.passed vk_test_http11_cli.passed vk_test_signal.passed vk_test_cr.passed vk_test_log.passed vk_test_exec.passed vk_test_mem.passed vk_test_ft.passed vk_test_ft2.passed vk_test_ft3.passed vk_test_err.passed vk_test_write.passed vk_test_read.passed

test_all: test vk_test_http11_cli.passed1m

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm -f *.o *.a vk_test_echo_service vk_test_echo_cli vk_test_http11_service vk_test_http11_cli vk_test_signal vk_test_cr vk_test_log vk_test_exec vk_test_mem vk_test_ft vk_test_ft2 vk_test_ft3 vk_test_err vk_test_write vk_test_read

clean_all: clean
	rm -f *.out.txt *.passed