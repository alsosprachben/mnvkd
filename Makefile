SRCS=vk_*.c

all: vk_test.debug vk_test.release

vk_test.debug: vk_test.debug.symbols
	cp ${@}.symbols ${@}
	strip ${@}

vk_test.release: vk_test.release.symbols
	cp ${@}.symbols ${@}
	strip ${@}

vk_test.debug.symbols: vk_*.c
	cc -Wall -g3 -O0 -DDEBUG=1 -o ${@} vk_*.c

vk_test.release.symbols: vk_*.c
	cc -Wall -g3 -Os -flto -o ${@} vk_*.c

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend

depend: .depend

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm vk_test.debug vk_test.release vk_test.debug.symbols vk_test.release.symbols
