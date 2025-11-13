![](./.files/images/sceptre.png)

# bennu

bennu is a modeling and simulation application for ICS/SCADA devices.

---

## Building bennu with Docker

Example of using Docker to perform builds. This is easier to do as it eliminates issues with dependencies and versions on the host OS.

```sh
git clone https://github.com/sandialabs/sceptre-bennu.git
cd sceptre-bennu/

# Build the Docker image
# To change the base image, use REGISTRY_IMAGE build arg
# For example, to build with Ubuntu 22.04 (instead of 20.04), add:
#   --build-arg REGISTRY_IMAGE="ubuntu:22.04" \
docker build --pull --tag sceptre-bennu:testing .

# Run the desired executable
# This can also be copied out of the image, including shared libraries, to another system using
docker run -it sceptre-bennu:testing bennu-field-deviced --help
```

## Installation (Ubuntu)

### From source

-   Install dependencies

    ```sh
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

-   Build and install bennu

    ```sh
    mkdir build
    cd build
    cmake ../
    make -j$(nproc)
    sudo make install
    ```

    > Note if building behind a proxy server, edit `src/pybennu/Makefile`, uncomment lines 124-126, and add appropriate proxy information.

### From releases

-   Download and install `bennu.deb` from the latest release:

    ```sh
    wget https://github.com/sandialabs/sceptre-bennu/releases/latest/download/bennu.deb
    sudo apt install ./bennu.deb
    ```

# pybennu

Pybennu is a Python library and set of applications for interacting with various simulation providers, hardware-in-the-loop (`pybennu-siren`) and bennu field devices.  
The pybennu source is under `src/pybennu`.

## Installation (Ubuntu)

### From source

-   Install dependencies

    ```sh
    sudo apt install build-essential cmake git wget python3-dev python3-pip \
        libfreetype6-dev liblapack-dev libboost-dev \
        ninja-build pipx
    ```

-   Build and install libzmq and helics libraries

    ```sh
    cd src/pybennu/
    # build and install libzmq with draft support
    make libzmq-install

    # build and install helics (after installing libzmq)
    make helics-install
    ```

-   Install pybennu

    > Note that the pybennu installation forces pyzmq and pyhelics to be built from source. This is done so that they are built against the libzmq library with draft support installed above. This also means that the libzmq and helics libraries always need to be installed **before** installing pybennu.

    ```sh
    cd src/pybennu

    # It is recommended to install in a python virtual environment
    # Depending on your pip version, you may first need to upgrade with: pip install -U pip
    pip install .
    ```

### From releases (x86_64 builds only)

-   Install dependencies

    ```sh
    sudo apt install git wget python3-dev python3-pip cmake ninja-build libboost-dev
    ```

-   Download and install libzmq and helics builds

    ```sh
    wget https://github.com/sandialabs/sceptre-bennu/releases/latest/download/helics_3.6.1-1_amd64.deb
    wget https://github.com/sandialabs/sceptre-bennu/releases/latest/download/libzmq_4.3.4-1_amd64.deb
    sudo apt install ./libzmq_4.3.4-1_amd64.deb
    sudo apt install ./helics_3.6.1-1_amd64.deb
    ```

-   Download and install the pybennu wheel

    > Note that even the pybennu wheel installation forces pyzmq and pyhelics to be built from source. This is done so that they are built against the libzmq library with draft support installed above. This also means that the libzmq and helics libraries always need to be installed **before** installing pybennu.

    ```sh
    wget https://github.com/sandialabs/sceptre-bennu/releases/latest/download/pybennu-6.0.0-py3-none-any.whl

    # It is recommended to install in a python virtual environment
    # Depending on your pip version, you may first need to upgrade with: pip install -U pip
    pip install pybennu-6.0.0-py3-none-any.whl
    ```

## Other helpful pybennu build tools

-   Show makefile options

    ```sh
    make help
    ```
    ```
    Usage: make <command> [ARGUMENT1=value1] [ARGUMENT2=value2]

    Commands:

    help           # Show this help

    --- Library dependencies (debs) ---

    libzmq-deb     # Build the libzmq Debian package with draft support.

    libzmq-install # Install the libzmq deb

    helics-deb     # Build helics deb package

    helics-install # Install helics deb package

    --- Python packaging ---

    dist           # Build pybennu wheel

    sdist          # Build pybennu source distribution

    wheelhouse     # Create wheelhouse of pybennu and all dependencies in a .tar.gz

    --- Cleanup ---

    clean          # Clean up build and dist files
    ```

-   build pybennu wheel

    ```sh
    # requires pipx to be installed
    make dist
    ```

-   build wheels for pybennu and all dependencies

    ```sh
    # you may need to upgrade pip first with: pip install -U pip
    make wheelhouse
    ```
