# Verification image for bennu: ubuntu 22.04 + build.yml deps + pytest.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive PIP_DISABLE_PIP_VERSION_CHECK=1
RUN apt-get update && apt-get install --no-install-recommends -y \
      build-essential cmake g++ gcc libasio-dev libboost-date-time-dev \
      libboost-filesystem-dev libboost-program-options-dev libboost-python-dev \
      libboost-system-dev libboost-thread-dev libfreetype6-dev libssl-dev libzmq5-dev \
      ca-certificates wget git rsync ccache pkg-config python3 python3-pip python3-venv \
      curl make clang-format-15 clang-tidy-15 \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir pytest pyzmq "loguru>=0.7,<1" pytest-cov ruff "codespell>=2.2.4" vulture \
      "pydantic>2.0.0,<3.0.0" pydantic-settings "pymodbus>=3.8.0,<=3.9.2" py-expression-eval==0.3.14 \
    && update-alternatives --install /usr/local/bin/clang-format clang-format /usr/bin/clang-format-15 100 \
    && update-alternatives --install /usr/local/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-15 100

ENV CCACHE_DIR=/cache/ccache
