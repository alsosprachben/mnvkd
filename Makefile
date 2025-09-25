

SRCS=vk_*.c
# comprise vk.a
OBJS=\
	vk_kern.o \
	vk_fd_table.o \
	vk_fd.o \
	vk_io_future.o \
	vk_signal.o \
	vk_stack.o \
	vk_wrapguard.o \
	vk_heap.o \
	vk_pool.o \
	vk_proc.o \
	vk_proc_local.o \
	vk_future.o \
	vk_thread.o \
	vk_thread_io.o \
	vk_socket.o \
	vk_vectoring.o \
	vk_pipe.o \
	vk_server.o \
	vk_accepted.o \
	vk_service.o \
	vk_server.o \
	vk_client.o \
	vk_connection.o \
	vk_io_op.o \
	vk_io_queue.o \
	vk_io_exec.o \
	vk_io_batch_sync.o \
	vk_sys_caps.o \
	vk_main.o \
	vk_main_local.o \
	vk_main_local_isolate.o \
	vk_isolate.o \
	vk_huge.o


# OS-specific valid file names
UNAME_S!= uname -s

.if ${UNAME_S} == "Linux"
VALID_SIGNAL=vk_test_signal.valid.txt
VALID_ERR2=vk_test_err2.valid.txt
.elif ${UNAME_S} == "Darwin"
VALID_SIGNAL=vk_test_signal.valid.macos.txt
VALID_ERR2=vk_test_err2.valid.macos.txt
.else
.warning Unknown OS "${UNAME_S}", defaulting to Linux names
VALID_SIGNAL=vk_test_signal.valid.txt
VALID_ERR2=vk_test_err2.valid.txt
.endif

all: test vk_test_echo_service vk_test_http11_service vk_test_redis_service

vk.a: ${OBJS}
	ar -r ${@} ${>}

vk_test_echo_service:   vk_test_echo_service.c   vk_echo.c            vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_echo_cli:       vk_test_echo_cli.c       vk_echo.c            vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_redis_service:   vk_test_redis_service.c   vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_redis_cli:       vk_test_redis_cli.c       vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_redis_client_cli:	vk_test_redis_client_cli.c       vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_redis_client:   vk_test_redis_client.c   vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_redis_client_encode:   vk_test_redis_client_encode.c   vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_redis_client_decode:   vk_test_redis_client_decode.c   vk_redis.c   vk.a
	${CC} ${CFLAGS} -o ${@} ${>} -lsqlite3

vk_test_http11_service: vk_test_http11_service.c vk_http11.c vk_rfc.c vk_fetch.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_http11_cli:     vk_test_http11_cli.c     vk_http11.c vk_rfc.c vk_fetch.c vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_signal:         vk_signal.c
	${CC} ${CFLAGS} -DVK_SIGNAL_TEST -o ${@} ${>}

vk_test_isolate_thread: vk_test_isolate_thread.c vk_isolate.c vk.a
	${CC} ${CFLAGS} -ldl -o ${@} ${>}

vk_test_isolate_thread2: vk_test_isolate_thread2.c vk_main_local_isolate.c vk_isolate.c vk.a
	${CC} ${CFLAGS} -ldl -o ${@} ${>}

vk_test_cr:             vk_test_cr.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_log:            vk_test_log.c                                 vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_exec:           vk_test_exec.c                                vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_io_queue:       vk_test_io_queue.c                            vk.a
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

vk_test_err2:		   vk_test_err2.c                                 vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_read:		  vk_test_read.c                                  vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_write:		  vk_test_write.c                                 vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_forward:	  vk_test_forward.c                               vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_pollread:	  vk_test_pollread.c                              vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

vk_test_aio_cli:	  vk_test_aio_cli.c                               vk.a
	${CC} ${CFLAGS} -o ${@} ${>}

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend test

depend: .depend

# vk_test_echo
vk_test_echo.out.txt: vk_test_echo_cli vk_test_echo.in.txt
	./vk_test_echo_cli < vk_test_echo.in.txt > vk_test_echo.out.txt

vk_test_echo.valid.txt:
	cp vk_test_echo.out.txt "${@}"

vk_test_echo.passed: vk_test_echo.out.txt vk_test_echo.valid.txt
	diff -q vk_test_echo.out.txt vk_test_echo.valid.txt && touch "${@}"

vk_test_redis.out.txt: vk_test_redis_cli vk_test_redis.in.txt
	./vk_test_redis_cli < vk_test_redis.in.txt > vk_test_redis.out.txt

vk_test_redis.valid.txt:
	cp vk_test_redis.out.txt "${@}"

vk_test_redis_cli.passed: vk_test_redis.out.txt vk_test_redis.valid.txt
	diff -q vk_test_redis.out.txt vk_test_redis.valid.txt && touch "${@}"

vk_test_redis_client.out.txt: vk_test_redis_service vk_test_redis_client_cli vk_test_redis_client.in.txt vk_test_redis_client_encode vk_test_redis_client_decode vk_test_redis_client.sh
	./vk_test_redis_client.sh

# client CLI (no network): human -> RESP -> server CLI -> human
vk_test_redis_client_cli.out.txt: vk_test_redis_client_encode vk_test_redis_cli vk_test_redis_client_decode vk_test_redis_client.in.txt
	./vk_test_redis_client_encode < vk_test_redis_client.in.txt | ./vk_test_redis_cli | ./vk_test_redis_client_decode > vk_test_redis_client_cli.out.txt

vk_test_redis_client.valid.txt:
	cp vk_test_redis_client.out.txt "${@}"

vk_test_redis_client_cli.passed: vk_test_redis_client_cli.out.txt vk_test_redis_client.valid.txt
	diff -q vk_test_redis_client_cli.out.txt vk_test_redis_client.valid.txt && touch "${@}"

# networked client + server pass check
vk_test_redis_client_network.passed: vk_test_redis_client.out.txt vk_test_redis_client.valid.txt
	diff -q vk_test_redis_client.out.txt vk_test_redis_client.valid.txt && touch "${@}"

# vk_test_http11
vk_test_http11.in3.txt: pipeline.py
	./pipeline.py 3 GET > "${@}"

vk_test_http11.in1m.txt: pipeline.py
	./pipeline.py 1000000 GET > "${@}"

vk_test_http11.in3post.txt: pipeline.py
	./pipeline.py 3 POST > "${@}"

vk_test_http11.in1mpost.txt: pipeline.py
	./pipeline.py 1000000 POST > "${@}"

vk_test_http11.in3chunked.txt: pipeline.py
	./pipeline.py 3 POST-chunked > "${@}"

vk_test_http11.in1mchunked.txt: pipeline.py
	./pipeline.py 1000000 POST-chunked > "${@}"

vk_test_http11_cli.out3.txt: vk_test_http11_cli vk_test_http11.in3.txt
	./vk_test_http11_cli< vk_test_http11.in3.txt > vk_test_http11_cli.out3.txt

vk_test_http11_cli.out1m.txt: vk_test_http11_cli vk_test_http11.in1m.txt
	./vk_test_http11_cli < vk_test_http11.in1m.txt > vk_test_http11_cli.out1m.txt

vk_test_http11_cli.out3post.txt: vk_test_http11_cli vk_test_http11.in3post.txt
	./vk_test_http11_cli< vk_test_http11.in3post.txt > vk_test_http11_cli.out3post.txt

vk_test_http11_cli.out1mpost.txt: vk_test_http11_cli vk_test_http11.in1mpost.txt
	./vk_test_http11_cli < vk_test_http11.in1mpost.txt > vk_test_http11_cli.out1mpost.txt

vk_test_http11_cli.out3chunked.txt: vk_test_http11_cli vk_test_http11.in3chunked.txt
	./vk_test_http11_cli< vk_test_http11.in3chunked.txt > vk_test_http11_cli.out3chunked.txt

vk_test_http11_cli.out1mchunked.txt: vk_test_http11_cli vk_test_http11.in1mchunked.txt
	./vk_test_http11_cli < vk_test_http11.in1mchunked.txt > vk_test_http11_cli.out1mchunked.txt

vk_test_http11_service.out3.txt: vk_test_http11_service vk_test_http11.in3.txt
	./vk_test_http11_service.sh

vk_test_http11.valid3.txt:
	cp vk_test_http11_cli.out3.txt "${@}"

vk_test_http11.valid1m.txt:
	cp vk_test_http11_cli.out1m.txt "${@}"

vk_test_http11.valid3post.txt:
	cp vk_test_http11_cli.out3post.txt "${@}"

vk_test_http11.valid1mpost.txt:
	cp vk_test_http11_cli.out1mpost.txt "${@}"

vk_test_http11.valid3chunked.txt:
	cp vk_test_http11_cli.out3chunked.txt "${@}"

vk_test_http11.valid1mchunked.txt:
	cp vk_test_http11_cli.out1mchunked.txt "${@}"

vk_test_http11_cli.passed3: vk_test_http11_cli.out3.txt vk_test_http11.valid3.txt
	diff -q vk_test_http11_cli.out3.txt vk_test_http11.valid3.txt && touch "${@}"

vk_test_http11_cli.passed1m: vk_test_http11_cli.out1m.txt vk_test_http11.valid1m.txt
	diff -q vk_test_http11_cli.out1m.txt vk_test_http11.valid1m.txt && touch "${@}"

vk_test_http11_cli.passed3post: vk_test_http11_cli.out3post.txt vk_test_http11.valid3post.txt
	diff -q vk_test_http11_cli.out3post.txt vk_test_http11.valid3post.txt && touch "${@}"

vk_test_http11_cli.passed1mpost: vk_test_http11_cli.out1mpost.txt vk_test_http11.valid1mpost.txt
	diff -q vk_test_http11_cli.out1mpost.txt vk_test_http11.valid1mpost.txt && touch "${@}"

vk_test_http11_cli.passed3chunked: vk_test_http11_cli.out3chunked.txt vk_test_http11.valid3chunked.txt
	diff -q vk_test_http11_cli.out3chunked.txt vk_test_http11.valid3chunked.txt && touch "${@}"

vk_test_http11_cli.passed1mchunked: vk_test_http11_cli.out1mchunked.txt vk_test_http11.valid1mchunked.txt
	diff -q vk_test_http11_cli.out1mchunked.txt vk_test_http11.valid1mchunked.txt && touch "${@}"

vk_test_http11_service.passed: vk_test_http11_service.out3.txt vk_test_http11.valid3.txt
	diff -q vk_test_http11_service.out3.txt vk_test_http11.valid3.txt && touch "${@}"

# vk_test_signal
vk_test_signal.out.txt: vk_test_signal
	./vk_test_signal > vk_test_signal.out.txt

$(VALID_SIGNAL): vk_test_signal.out.txt
	cp vk_test_signal.out.txt $@

vk_test_signal.passed: vk_test_signal.out.txt $(VALID_SIGNAL)
	diff -q vk_test_signal.out.txt $(VALID_SIGNAL) && touch $@

# vk_test_isolate_thread
vk_test_isolate_thread.out.txt: vk_test_isolate_thread
	./vk_test_isolate_thread > vk_test_isolate_thread.out.txt

vk_test_isolate_thread.valid.txt:
	cp vk_test_isolate_thread.out.txt vk_test_isolate_thread.valid.txt

vk_test_isolate_thread.passed: vk_test_isolate_thread.out.txt vk_test_isolate_thread.valid.txt
	diff -q vk_test_isolate_thread.out.txt vk_test_isolate_thread.valid.txt && touch "${@}"

# vk_test_isolate_thread2
vk_test_isolate_thread2.out.txt: vk_test_isolate_thread2
	./vk_test_isolate_thread2 > vk_test_isolate_thread2.out.txt

vk_test_isolate_thread2.valid.txt:
	cp vk_test_isolate_thread2.out.txt vk_test_isolate_thread2.valid.txt

vk_test_isolate_thread2.passed: vk_test_isolate_thread2.out.txt vk_test_isolate_thread2.valid.txt
	diff -q vk_test_isolate_thread2.out.txt vk_test_isolate_thread2.valid.txt && touch "${@}"

# vk_test_cr
vk_test_cr.out.txt: vk_test_cr
	./vk_test_cr > vk_test_cr.out.txt

vk_test_cr.valid.txt:
	cp vk_test_cr.out.txt vk_test_cr.valid.txt

vk_test_cr.passed: vk_test_cr.out.txt vk_test_cr.valid.txt
	diff -q vk_test_cr.out.txt vk_test_cr.valid.txt && touch "${@}"

# vk_test_log
vk_test_log.out.txt: vk_test_log
	./vk_test_log 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_log.out.txt

vk_test_log.valid.txt:
	cp vk_test_log.out.txt vk_test_log.valid.txt

vk_test_log.passed: vk_test_log.out.txt vk_test_log.valid.txt
	diff -q vk_test_log.out.txt vk_test_log.valid.txt && touch "${@}"

# vk_test_exec
vk_test_exec.out.txt: vk_test_exec
	./vk_test_exec 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_exec.out.txt

vk_test_exec.valid.txt:
	cp vk_test_exec.out.txt vk_test_exec.valid.txt

vk_test_exec.passed: vk_test_exec.out.txt vk_test_exec.valid.txt
	diff -q vk_test_exec.out.txt vk_test_exec.valid.txt && touch "${@}"

# vk_test_io_queue
vk_test_io_queue.passed: vk_test_io_queue
	./vk_test_io_queue
	touch ${@}

# vk_test_mem
vk_test_mem.out.txt: vk_test_mem
	./vk_test_mem 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_mem.out.txt

vk_test_mem.valid.txt:
	cp vk_test_mem.out.txt vk_test_mem.valid.txt

vk_test_mem.passed: vk_test_mem.out.txt vk_test_mem.valid.txt
	diff -q vk_test_mem.out.txt vk_test_mem.valid.txt && touch "${@}"

# vk_test_ft
vk_test_ft.out.txt: vk_test_ft
	./vk_test_ft 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_ft.out.txt

vk_test_ft.valid.txt:
	cp vk_test_ft.out.txt vk_test_ft.valid.txt

vk_test_ft.passed: vk_test_ft.out.txt vk_test_ft.valid.txt
	diff -q vk_test_ft.out.txt vk_test_ft.valid.txt && touch "${@}"

# vk_test_ft2
vk_test_ft2.out.txt: vk_test_ft2
	./vk_test_ft2 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_ft2.out.txt

vk_test_ft2.valid.txt:
	cp vk_test_ft2.out.txt vk_test_ft2.valid.txt

vk_test_ft2.passed: vk_test_ft2.out.txt vk_test_ft2.valid.txt
	diff -q vk_test_ft2.out.txt vk_test_ft2.valid.txt && touch "${@}"

# vk_test_ft3
vk_test_ft3.out.txt: vk_test_ft3
	./vk_test_ft3 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_ft3.out.txt

vk_test_ft3.valid.txt:
	cp vk_test_ft3.out.txt vk_test_ft3.valid.txt

vk_test_ft3.passed: vk_test_ft3.out.txt vk_test_ft3.valid.txt
	diff -q vk_test_ft3.out.txt vk_test_ft3.valid.txt && touch "${@}"

# vk_test_err
vk_test_err.out.txt: vk_test_err
	./vk_test_err 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_err.out.txt

vk_test_err.valid.txt:
	cp vk_test_err.out.txt vk_test_err.valid.txt

vk_test_err.passed: vk_test_err.out.txt vk_test_err.valid.txt
	diff -q vk_test_err.out.txt vk_test_err.valid.txt && touch "${@}"

# vk_test_err2
vk_test_err2.out.txt: vk_test_err2
	./vk_test_err2 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_err2.out.txt

$(VALID_ERR2): vk_test_err2.out.txt
	cp vk_test_err2.out.txt $@

vk_test_err2.passed: vk_test_err2.out.txt $(VALID_ERR2)
	diff -q vk_test_err2.out.txt $(VALID_ERR2) && touch $@

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
	./vk_test_read < vk_test_read.in.txt 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_read.out.txt

vk_test_read.valid.txt:
	cp vk_test_read.out.txt vk_test_read.valid.txt

vk_test_read.passed: vk_test_read.out.txt vk_test_read.valid.txt
	diff -q vk_test_read.out.txt vk_test_read.valid.txt && touch "${@}"

# vk_test_pollread
vk_test_pollread.out.txt: vk_test_pollread
	{ printf "message 1"; sleep 1; printf "message 2"; } | ./vk_test_pollread 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_pollread.out.txt

vk_test_pollread.valid.txt:
	cp vk_test_pollread.out.txt vk_test_pollread.valid.txt

vk_test_pollread.passed: vk_test_pollread.out.txt vk_test_pollread.valid.txt
	diff -q vk_test_pollread.out.txt vk_test_pollread.valid.txt && touch "${@}"

vk_test_aio_cli.in.txt:
	dd if=/dev/zero bs=1 count=8192 2>/dev/null | tr '\0' 'a' > "${@}"

vk_test_aio_cli.out.txt: vk_test_aio_cli vk_test_aio_cli.in.txt
	./vk_test_aio_cli < vk_test_aio_cli.in.txt > vk_test_aio_cli.out.txt

vk_test_aio_cli.valid.txt: vk_test_aio_cli.in.txt
	cp vk_test_aio_cli.in.txt "${@}"

vk_test_aio_cli.passed: vk_test_aio_cli.out.txt vk_test_aio_cli.valid.txt
	diff -q vk_test_aio_cli.out.txt vk_test_aio_cli.valid.txt && touch "${@}"

# vk_test_forward
vk_test_forward.out.txt: vk_test_forward vk_test_read.in.txt
	./vk_test_forward < vk_test_read.in.txt 2>&1 | grep ': LOG ' | sed -e 's/.*LOG //' > vk_test_forward.out.txt

vk_test_forward.valid.txt:
	cp vk_test_forward.out.txt vk_test_forward.valid.txt

vk_test_forward.passed: vk_test_forward.out.txt vk_test_forward.valid.txt
	diff -q vk_test_forward.out.txt vk_test_forward.valid.txt && touch "${@}"

# local benchmark
vk_test_http11_service_launch: vk_test_http11_service
	VK_POLL_DRIVER=OS VK_POLL_METHOD=EDGE_TRIGGERED  ./vk_test_http11_service

/usr/bin/node:
	sudo apt install -y nodejs

vk_test_httpexpress_service_launch: /usr/bin/node
	node ./expresshw.js

/usr/bin/go:
	sudo apt install -y golang

~/go/bin/fortio: /usr/bin/go
	go install fortio.org/fortio@latest

vk_test_http11_fortio.json: ~/go/bin/fortio vk_test_http11_service
	~/go/bin/fortio load -c=10000 -qps=0 -t=30s -timeout=60s -r=0.0001 -json=vk_test_http11_fortio.json http://localhost:8081/

vk_test_http11_service_report: ~/go/bin/fortio vk_test_http11_fortio.json
	~/go/bin/fortio report -json vk_test_http11_fortio.json

vk_test_httpexpress_fortio.json: ~/go/bin/fortio vk_test_http11_service
	~/go/bin/fortio load -c=10000 -qps=0 -t=30s -timeout=60s -r=0.0001 -json=vk_test_httpexpress_fortio.json http://localhost:3000/

vk_test_httpexpress_service_report: ~/go/bin/fortio vk_test_httpexpress_fortio.json
	~/go/bin/fortio report -json vk_test_httpexpress_fortio.json

test: \
    vk_test_echo.passed \
    vk_test_redis_cli.passed \
    vk_test_redis_client_cli.passed \
    vk_test_http11_cli.passed3 \
	vk_test_http11_cli.passed3post \
	vk_test_http11_cli.passed3chunked \
	vk_test_signal.passed \
	vk_test_cr.passed \
	vk_test_log.passed \
	vk_test_exec.passed \
	vk_test_mem.passed \
	vk_test_ft.passed \
	vk_test_ft2.passed \
	vk_test_ft3.passed \
	vk_test_err.passed \
	vk_test_err2.passed \
	vk_test_write.passed \
	vk_test_read.passed \
	vk_test_forward.passed \
	vk_test_pollread.passed \
	vk_test_aio_cli.passed \
	vk_test_isolate_thread.passed \
	vk_test_isolate_thread2.passed

test_all: test test_servers \
	vk_test_http11_cli.passed1m \
	vk_test_http11_cli.passed1mpost \
	vk_test_http11_cli.passed1mchunked \

test_servers: vk_test_http11_service vk_test_redis_service \
    vk_test_redis_client_network.passed \
    vk_test_http11_service.passed \
    vk_test_io_queue.passed

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm -f *.o *.a \
                vk_test_echo_service \
                vk_test_echo_cli \
               vk_test_redis_service \
               vk_test_redis_cli \
               vk_test_redis_client_cli \
               vk_test_redis_client \
               vk_test_redis_client_encode \
               vk_test_redis_client_decode \
               vk_test_redis_client \
                vk_test_http11_service \
                vk_test_http11_cli \
                vk_test_signal \
		vk_test_cr \
		vk_test_log \
		vk_test_exec \
		vk_test_io_queue \
		vk_test_mem \
		vk_test_ft \
		vk_test_ft2 \
		vk_test_ft3 \
		vk_test_err \
		vk_test_err2 \
		vk_test_write \
		vk_test_read \
		vk_test_forward \
			vk_test_pollread \
			vk_test_aio_cli \
			vk_test_isolate_thread

clean_all: clean
	rm -f *.out*.txt *.passed
