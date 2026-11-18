ARG REGISTRY_IMAGE="ubuntu:22.04"
ARG PIP_INDEX="https://pypi.org/"
ARG PIP_INDEX_URL="https://pypi.org/simple"

# create python build image
FROM ${REGISTRY_IMAGE} AS pybuilder

ENV DEBIAN_FRONTEND="noninteractive" \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    LC_ALL="C.UTF-8"

# PEP 668 (Ubuntu 24.04+) marks the system python "externally managed". These are single-purpose build/runtime
# images, and the final stage copies /usr/local (pybennu + its entry points) from pybuilder, so install into the
# system python on purpose instead of a venv (a venv would have to be relocated and put on PATH in every consumer).
# Env vars rather than --break-system-packages: pip < 23.0.1 (Ubuntu 22.04) rejects the flag but ignores unknown env.
ENV PIP_BREAK_SYSTEM_PACKAGES=1 \
    UV_BREAK_SYSTEM_PACKAGES=1

RUN apt-get update \
  && apt-get install -y \
    build-essential cmake git wget python3-dev python3-pip \
    libfreetype6-dev liblapack-dev libboost-dev \
    ninja-build pipx libarchive-tools \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# NOTE: sources are unpacked with bsdtar: GNU tar 1.35 on Ubuntu 26.04 fails under Docker Desktop Rosetta
# (linux/amd64 on Apple silicon) with "Cannot open: Function not implemented" for files in subdirectories.
# libzmq >= 4.3.5 is required for gcc 15 (4.3.4 fails the allocator_traits static_assert).
# setup ZMQ
ENV ZMQ_VERSION 4.3.5
RUN wget -qO zmq.tgz https://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}/zeromq-${ZMQ_VERSION}.tar.gz \
  && mkdir -p /tmp/zmq \
  && bsdtar -C /tmp/zmq -xzf zmq.tgz \
  && rm zmq.tgz \
  && cd /tmp/zmq/zeromq-${ZMQ_VERSION} \
  && ./configure --prefix=/usr/local --enable-drafts \
  && make -j"$(nproc)" \
  && make install

# setup Helics (needed for pybennu)
ENV HELICS_VERSION 3.6.1
RUN wget -qO helics.tgz https://github.com/GMLC-TDC/HELICS/releases/download/v${HELICS_VERSION}/Helics-v${HELICS_VERSION}-source.tar.gz \
  && mkdir -p /tmp/helics \
  && bsdtar -C /tmp/helics -xzf helics.tgz \
  && rm helics.tgz \
  && mkdir -p /tmp/helics/build && cd /tmp/helics/build \
  && cmake -D HELICS_USE_SYSTEM_ZEROMQ_ONLY=ON .. \
  && make -j"$(nproc)" install


# RUN wget -O pyhelics.tgz https://github.com/GMLC-TDC/pyhelics/releases/download/v${HELICS_VERSION}/helics-${HELICS_VERSION}.tar.gz \
#   && mkdir -p /tmp/pyhelics \
#   && tar -C /tmp/pyhelics -xzf pyhelics.tgz \
#   && rm pyhelics.tgz \
#   && cd /tmp/pyhelics/helics-${HELICS_VERSION} \
#   && sed -i 's/helics-apps/helics-apps~=2.7.1/' setup.py \
#   && python3 -m pip install --no-cache-dir .

#DEBUG build
#ADD docker/vendor /tmp/bennu/vendor
#WORKDIR /tmp/bennu/vendor/helics-helper
#RUN python3 -m pip install .

# install Python bennu package
COPY src/pybennu /tmp/bennu/src/pybennu
WORKDIR /tmp/bennu/src/pybennu
# CMAKE_POLICY_VERSION_MINIMUM: the HELICS 3.6.1 sdist that pyhelics builds bundles a libzmq subproject whose
# cmake_minimum_required is < 3.5, which CMake 4 (Ubuntu 26.04) rejects; the variable is ignored by older CMake.
RUN CMAKE_POLICY_VERSION_MINIMUM=3.5 pipx run uv pip install --no-cache --system-certs --system .


# ** create final image **
FROM ${REGISTRY_IMAGE}

ENV DEBIAN_FRONTEND="noninteractive" \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN apt-get update \
  && apt-get install -y \
    # bennu
    build-essential ca-certificates cmake cmake-curses-gui \
    git wget pkg-config libasio-dev libsodium-dev \
    libboost-system-dev libboost-filesystem-dev \
    libboost-thread-dev libboost-serialization-dev \
    libboost-date-time-dev libboost-program-options-dev \
    libboost-iostreams-dev libsodium23 libfreetype6 \
    liblapack3 libzmq3-dev \
    # fpm
    libffi-dev ruby-dev ruby-ffi \
    # python
    python3-dev python3-pip python3-setuptools python3-wheel \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# copy over custom-built libraries
COPY --from=pybuilder /usr/local /usr/local
ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib/x86_64-linux-gnu:/usr/local/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH}

# Gem install failures: https://github.com/jordansissel/fpm/issues/2048
# For now, manually install dotenv 2.8.1
# NOTE: remove existing "dotenv" executable shim installed by python-dotenv (depended on by pydantic-settings)
RUN rm -f /usr/local/bin/dotenv \
  && gem install dotenv -v 2.8.1 \
  && gem install fpm -v 1.15.1

# --ignore-installed: pip cannot uninstall the apt-owned pip/setuptools/wheel (no RECORD file; "uninstall-no-record-file"
# on 24.04+), so install the newer copies into /usr/local, which precedes /usr/lib/python3/dist-packages on sys.path.
# PIP_BREAK_SYSTEM_PACKAGES (PEP 668, 24.04+) is set on this command only so it does not leak into the shipped image.
RUN PIP_BREAK_SYSTEM_PACKAGES=1 python3 -m pip install --no-cache-dir --upgrade --ignore-installed aptly-ctl pip setuptools twine wheel

# copy over bennu source code
COPY cmake          /tmp/bennu/cmake
COPY CMakeLists.txt /tmp/bennu/CMakeLists.txt
COPY src/bennu      /tmp/bennu/src/bennu
COPY src/deps       /tmp/bennu/src/deps
COPY test           /tmp/bennu/test

# install C++ bennu package
WORKDIR /tmp/bennu/build
RUN cmake ../ \
  && make -j"$(nproc)" install \
  && rm -rf /tmp/*

WORKDIR /root
CMD ["/bin/bash"]
