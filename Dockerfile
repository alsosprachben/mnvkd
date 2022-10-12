FROM ubuntu

RUN apt-get update
RUN apt-get -y install bmake gcc

ADD Makefile release.sh debug.sh *.[ch] .

RUN ./release.sh bmake

CMD ./vk_http11

EXPOSE 8080/tcp