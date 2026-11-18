# Verification image for bennu: ubuntu 22.04 (default; BASE_IMAGE selects another release) + build.yml deps + pytest.
# NOTE: on non-22.04 bases tar is shimmed to bsdtar (/usr/local/bin/tar): GNU tar 1.35 on Ubuntu 26.04 fails under Docker Desktop Rosetta
# (linux/amd64 on Apple silicon) with "Cannot open: Function not implemented" for any file inside a subdirectory.
ARG BASE_IMAGE=ubuntu:22.04
FROM ${BASE_IMAGE}
ARG CLANG_VERSION=15
ARG BSDTAR_SHIM=
ENV DEBIAN_FRONTEND=noninteractive PIP_DISABLE_PIP_VERSION_CHECK=1
RUN apt-get update && apt-get install --no-install-recommends -y \
      build-essential cmake g++ gcc libasio-dev libboost-date-time-dev \
      libboost-filesystem-dev libboost-program-options-dev libboost-python-dev \
      libboost-system-dev libboost-thread-dev libfreetype6-dev libssl-dev libzmq3-dev \
      ca-certificates wget git rsync ccache pkg-config python3 python3-pip python3-venv \
      curl make clang-format-${CLANG_VERSION} clang-tidy-${CLANG_VERSION} \
      procps libarchive-tools \
    && { [ -z "$BSDTAR_SHIM" ] || ln -s /usr/bin/bsdtar /usr/local/bin/tar; } \
    && rm -rf /var/lib/apt/lists/*

# venv, not --break-system-packages: PEP 668 (24.04+) marks system python externally managed.
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir pytest pyzmq pyyaml "loguru>=0.7,<1" pytest-cov ruff "codespell>=2.2.4" vulture \
      "pydantic>2.0.0,<3.0.0" pydantic-settings "pymodbus>=3.8.0,<=3.9.2" py-expression-eval==0.3.14 "opendssdirect.py~=0.9.4" \
    && update-alternatives --install /usr/local/bin/clang-format clang-format /usr/bin/clang-format-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/local/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-${CLANG_VERSION} 100

ENV PATH=/opt/venv/bin:$PATH CCACHE_DIR=/cache/ccache
