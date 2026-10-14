# Verification image for bennu: ubuntu 22.04 + build.yml deps + pytest + Go.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive PIP_DISABLE_PIP_VERSION_CHECK=1
RUN apt-get update && apt-get install --no-install-recommends -y \
      build-essential cmake g++ gcc libasio-dev libboost-date-time-dev \
      libboost-filesystem-dev libboost-program-options-dev libboost-python-dev \
      libboost-system-dev libboost-thread-dev libfreetype6-dev libssl-dev libzmq5-dev \
      ca-certificates wget git rsync ccache pkg-config python3 python3-pip python3-venv \
      curl make clang-format-15 clang-tidy-15 \
    && rm -rf /var/lib/apt/lists/*

# libzmq with drafts for gobennu (pebbe/zmq4/draft); kept out of /usr/local so the C++ build is unaffected.
ARG ZMQ_VERSION=4.3.4
RUN wget -qO /tmp/zmq.tgz https://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}/zeromq-${ZMQ_VERSION}.tar.gz \
    && tar -C /tmp -xzf /tmp/zmq.tgz && cd /tmp/zeromq-${ZMQ_VERSION} \
    && ./configure --prefix=/opt/zmq-draft --enable-drafts >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null \
    && rm -rf /tmp/zmq.tgz /tmp/zeromq-${ZMQ_VERSION}

# go.mod says 1.14; use a newer toolchain that still builds it.
ARG GO_VERSION=1.21.13
RUN wget -qO /tmp/go.tgz https://go.dev/dl/go${GO_VERSION}.linux-$(dpkg --print-architecture).tar.gz \
    && tar -C /usr/local -xzf /tmp/go.tgz && rm /tmp/go.tgz

RUN pip3 install --no-cache-dir pytest pyzmq pytest-cov ruff "codespell>=2.2.4" vulture \
    && update-alternatives --install /usr/local/bin/clang-format clang-format /usr/bin/clang-format-15 100 \
    && update-alternatives --install /usr/local/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-15 100

# lint (keep in sync with src/gobennu/Makefile install-dev and the lint workflow)
ARG GOLANGCI_LINT_VERSION=v2.11.3
RUN curl -sSfL https://raw.githubusercontent.com/golangci/golangci-lint/HEAD/install.sh \
    | sh -s -- -b /usr/local/bin ${GOLANGCI_LINT_VERSION}

ENV PATH=/usr/local/go/bin:$PATH CCACHE_DIR=/cache/ccache
