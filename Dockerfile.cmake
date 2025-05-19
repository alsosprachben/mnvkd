FROM ubuntu:24.04

RUN apt-get update
RUN apt-get -y install ninja-build cmake gcc strace python3

ADD CMakeLists.txt static-triplets vk_test_echo.in.txt vk_test_http11.in.txt pipeline.py *.[ch] .

RUN cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build

RUN echo kernel.yama.ptrace_scope = 0 > /etc/sysctl.d/10-ptrace.conf

CMD ["/build/vk_test_http11_service"]

EXPOSE 8081/tcp
