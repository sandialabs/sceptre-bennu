# create python build image
FROM ubuntu:20.04 AS pybuilder

ENV DEBIAN_FRONTEND noninteractive

RUN apt update \
  && apt install -y \
    build-essential cmake git wget python3-dev python3-pip \
    libfreetype6-dev liblapack-dev libboost-dev \
  && rm -rf /var/lib/apt/lists/*

# setup ZMQ
ENV ZMQ_VERSION 4.3.4
RUN wget -O zmq.tgz https://github.com/zeromq/libzmq/releases/download/v4.3.4/zeromq-4.3.4.tar.gz \
  && mkdir -p /tmp/zmq \
  && tar -C /tmp/zmq -xvzf zmq.tgz \
  && rm zmq.tgz \
  && cd /tmp/zmq/zeromq-${ZMQ_VERSION} \
  && ./configure --enable-drafts \
  && make -j$(nproc) install

# setup Helics (needed for pybennu)
ENV HELICS_VERSION 2.7.1
RUN wget -O helics.tgz https://github.com/GMLC-TDC/HELICS/releases/download/v${HELICS_VERSION}/Helics-v${HELICS_VERSION}-source.tar.gz \
  && mkdir -p /tmp/helics \
  && tar -C /tmp/helics -xzf helics.tgz \
  && rm helics.tgz \
  && mkdir -p /tmp/helics/build && cd /tmp/helics/build \
  && cmake -D HELICS_USE_SYSTEM_ZEROMQ_ONLY=ON .. \
  && make -j$(nproc) install

RUN python3 -m pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org pyzmq~=20.0.0 --install-option=--enable-drafts

RUN wget -O pyhelics.tgz https://github.com/GMLC-TDC/pyhelics/releases/download/v${HELICS_VERSION}/helics-${HELICS_VERSION}.tar.gz \
  && mkdir -p /tmp/pyhelics \
  && tar -C /tmp/pyhelics -xzf pyhelics.tgz \
  && rm pyhelics.tgz \
  && cd /tmp/pyhelics/helics-${HELICS_VERSION} \
  && sed -i 's/helics-apps/helics-apps~=2.7.1/' setup.py \
  && python3 -m pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org .

ADD docker/vendor /tmp/bennu/vendor
WORKDIR /tmp/bennu/vendor/helics-helper
RUN python3 -m pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org .

# install Python bennu package
ADD src/pybennu /tmp/bennu/src/pybennu
WORKDIR /tmp/bennu/src/pybennu
RUN python3 -m pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org .

# create final image
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt update \
  && apt install -y \
    # bennu
    build-essential ca-certificates cmake cmake-curses-gui \
    git wget pkg-config libasio-dev libsodium-dev \
    libboost-system-dev libboost-filesystem-dev \
    libboost-thread-dev libboost-serialization-dev \
    libboost-date-time-dev libboost-program-options-dev \
    libboost-iostreams-dev libsodium23 libfreetype6 \
    liblapack3 libzmq5-dev \
    # fpm
    libffi-dev ruby-dev ruby-ffi \
    # python
    python3-dev python3-pip python3-setuptools python3-wheel \
  && rm -rf /var/lib/apt/lists/*

# setup Go
ENV GOLANG_VERSION 1.16.5
RUN wget -O go.tgz https://golang.org/dl/go${GOLANG_VERSION}.linux-amd64.tar.gz \
  && tar -C /usr/local -xzf go.tgz && rm go.tgz
ENV GOPATH /go
ENV PATH $GOPATH/bin:/usr/local/go/bin:$PATH

# copy over custom-built libraries
COPY --from=pybuilder /usr/local /usr/local
ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}

# copy over bennu source code
ADD cmake          /tmp/bennu/cmake
ADD CMakeLists.txt /tmp/bennu/CMakeLists.txt
ADD src/bennu      /tmp/bennu/src/bennu
ADD src/deps       /tmp/bennu/src/deps
ADD src/gobennu    /tmp/bennu/src/gobennu
ADD test           /tmp/bennu/test

#set necessary vars
ENV GOPROXY "http://proxy.golang.org"
ENV  GONOSUMDB "proxy.golang.org/*,github.com,github.com/*,gopkg.in,gopkg.in/*,golang.org/*,golang.org"
ENV  GOPRIVACY "proxy.golang.org/*,github.com,github.com/*,gopkg.in,gopkg.in/*,golang.org/*,golang.org"
ENV  GOINSECURE "proxy.golang.org/*,github.com,github.com/*,gopkg.in,gopkg.in/*,golang.org/*,golang.org"

# install C++ and Golang bennu package
WORKDIR /tmp/bennu/build
RUN cmake -D BUILD_GOBENNU=ON ../ && make -j$(nproc) install \
  && rm -rf /tmp/*

RUN gem install fpm
RUN pip3 install --trusted-host pypy.org --trusted-host files.pythonhosted.org -U aptly-ctl pip setuptools twine wheel

WORKDIR /root
CMD /bin/bash
