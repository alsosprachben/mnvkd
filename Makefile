SRCS=vk_*.c

vk_test: vk_test.debug
	cp ${@}.debug ${@}
	strip ${@}

vk_test.debug: vk_*.c
	cc -Wall -g3 -O0 -DTEST_STATE -o ${@} vk_*.c

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend

depend: .depend

.if exists(.depend)
.include ".depend"
.endif

clean:
	rm vk_test vk_test.debug
