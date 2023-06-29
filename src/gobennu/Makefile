SHELL   := /bin/bash
COMMIT  := $(shell git log -1 --format="%h")
SOURCES := $(shell find . \( -name '*.go' \))

bin/gobennu: $(SOURCES)
	mkdir -p bin
	go build -a -ldflags="-X 'bennu/version.Number=git-$(COMMIT)' -s -w" -trimpath -o bin/gobennu cmd/main.go
