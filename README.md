![](./.files/images/sceptre.png)

bennu
======

bennu is a modeling and simulation application for ICS/SCADA devices.

---

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
