#!/usr/bin/env bash

set -ex

mkdir -p ./build
cd ./build
cmake ../
make -j "$(nproc)"
sudo make package
cp ./*.deb ../bennu.deb
cd ../
