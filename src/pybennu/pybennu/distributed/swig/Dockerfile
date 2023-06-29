FROM debian:sid
ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8
#ENV http_proxy=host:port
#ENV https_proxy=host:port

RUN apt-get update \
    && apt-get install -y g++ make python3-dev swig

CMD "/bin/bash"
