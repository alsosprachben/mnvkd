FROM ubuntu

RUN apt-get update
RUN apt-get -y install bmake gcc strace

ADD Makefile release.sh debug.sh *.[ch] ./

RUN ./debug.sh bmake 

RUN cp vk_http11 vk_http11.debug

RUN ./debug.sh bmake clean

RUN ./release.sh bmake

RUN echo kernel.yama.ptrace_scope = 0 > /etc/sysctl.d/10-ptrace.conf

CMD ["/vk_http11"]

EXPOSE 8080/tcp