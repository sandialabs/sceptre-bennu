# syntax=docker/dockerfile:1
FROM ubuntu:latest as base

#install C dependencies
RUN apt-get update && apt-get upgrade -y
RUN apt install -y build-essential cmake git wget libfreetype6-dev liblapack-dev libboost-dev \
  && rm -rf /var/lib/apt/lists/*

#install ZMQ
RUN apt install libzmq3-dev

# copy over bennu source code
COPY CMakeLists.txt /bennu/CMakeLists.txt
COPY src/bennu      /bennu/src/bennu
COPY src/deps       /bennu/src/deps
COPY src/gobennu    /bennu/src/gobennu
COPY test           /bennu/test

# copy files from git into new mount point at /bennu and copies these files
# into the newly created volume
VOLUME /bennu

#install python
FROM python:latest as python-env

#install helics
RUN pip install helics

#install go
FROM golang:latest as golang-env

ENTRYPOINT /bin/bash
