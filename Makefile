SRCS=vk_*.c

vk_state: vk_state.debug
	cp ${@}.debug ${@}
	strip ${@}

vk_state.debug: 
	cc -Wall -g3 -O0 -DTEST_STATE -o ${@} vk_*.c

.depend:
	touch "${@}"
	makedepend -f"${@}" -- ${CFLAGS} -- ${SRCS}

.PHONY: depend

depend: .depend

.include ".depend"

clean:
	rm vk_state vk_state.debug
