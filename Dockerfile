FROM ubuntu

RUN apt-get update
RUN apt-get -y install bmake gcc strace python3 liburing-dev libaio-dev

ADD Makefile release.sh debug.sh vk_test_echo.in.txt vk_test_http11.in.txt pipeline.py *.[ch] .

RUN ./debug.sh bmake clean

RUN ./debug.sh bmake vk_test_http11_service

RUN cp vk_test_http11_service vk_test_http11_service.debug

RUN ./release.sh bmake clean

RUN ./release.sh bmake vk_test_http11_service

RUN echo kernel.yama.ptrace_scope = 0 > /etc/sysctl.d/10-ptrace.conf

CMD ["/vk_test_http11_service"]

EXPOSE 8081/tcp
