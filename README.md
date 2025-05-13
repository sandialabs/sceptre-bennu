![](./.files/images/sceptre.png)

# bennu

bennu is a modeling and simulation application for ICS/SCADA devices.

---

## Building bennu with Docker
Example of using Docker to perform builds. This is easier to do as it eliminates issues with dependencies and versions on the host OS.

```bash
git clone https://github.com/sandialabs/sceptre-bennu.git
cd sceptre-bennu/

# Build the Docker image
# To change the base image, use REGISTRY_IMAGE build arg
# For example, to build with Ubuntu 22.04 (instead of 20.04), add:
#   --build-arg REGISTRY_IMAGE="ubuntu:22.04" \
docker build \
  --pull \
  --tag sceptre-bennu:testing \
  .

# Run the desired executable
# This can also be copied out of the image, including shared libraries, to another system using
docker run -it sceptre-bennu:testing bennu-field-deviced --help
```

## Installation (Ubuntu)
- Install dependencies

```bash
apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    g++ \
    gcc \
    libasio-dev \
    libboost-date-time-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libfreetype6-dev \
    libssl-dev \
    libzmq5-dev
```

- Build and install bennu

```bash
mkdir build
cd build
cmake ../
make -j$(nproc)
sudo make install
```
> Note if building behind a proxy server, edit `src/pybennu/Makefile`, uncomment lines 124-126, and add appropriate proxy information.
