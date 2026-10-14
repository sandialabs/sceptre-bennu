.ONESHELL:
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.PHONY: all check clean clean-build clean-pyc coverage dist format help install install-dev lint sdist test wheelhouse
.DEFAULT_GOAL := help

BUILD_DIR = $(CURDIR)/build
DIST_DIR = $(CURDIR)/dist
PACKAGE_MAINTAINER = Sandia National Laboratories <emulytics@sandia.gov>
NPROCS := $(shell nproc)
ZMQ_VERSION ?= 4.3.4
ZMQ_PREFIX ?= /usr/local
HELICS_VERSION ?= 3.6.1

# Use pipx to run latest versions of pip and uv
PIP = pipx run --spec pip python -m pip
UV = pipx run uv

# Define a helper for checking command existence
check-command = @if ! command -v $(1) > /dev/null; then \
		echo "Error: '$(1)' not found. Please run 'make install-dev' to install development tools, or ensure it is in your PATH."; \
		exit 1; \
	fi

# Show help message
help:
	@echo "Available targets:"
	@echo ""
	@echo "Development:"
	@echo "  all         - Run all tools (format, lint, test)"
	@echo "  check       - Run all checks without fixing (ruff, codespell, vulture)"
	@echo "  coverage    - Run unit tests with coverage report"
	@echo "  format      - Format code (ruff)"
	@echo "  lint        - Lint code and fix issues (ruff, codespell)"
	@echo "  test        - Run unit tests (use TEST=... for specific test)"
	@echo ""
	@echo "Packaging:"
	@echo "  dist        - Build pybennu wheel"
	@echo "  sdist       - Build pybennu source distribution"
	@echo "  wheelhouse  - Build wheelhouse of pybennu and its dependencies"
	@echo "  libzmq-deb, libzmq-install, helics-deb, helics-install - dependency debs"
	@echo ""
	@echo "Installation:"
	@echo "  install     - Install package in editable mode"
	@echo "  install-dev - Install development tools (ruff, codespell, vulture, pytest)"
	@echo ""
	@echo "Cleanup:"
	@echo "  clean       - Clean build and lint/test artifacts"
	@echo ""
	@echo "Help:"
	@echo "  help        - Show this help message"

##
## --- Development ---
##

# Run all tools (format, lint, test)
all: format lint test

# Run linting checks (no fix) - suitable for CI
check:
	$(call check-command,ruff)
	$(call check-command,codespell)
	$(call check-command,vulture)
	ruff check .
	ruff format --check .
	codespell --config ../../.codespellrc .
	vulture

# Run unit tests with coverage report
coverage:
	$(call check-command,pytest)
	PYTHONPATH=$(CURDIR) pytest --cov=pybennu --cov-report=term-missing --cov-report=xml:coverage.xml $(TEST)

# Format code (ruff)
format:
	$(call check-command,ruff)
	ruff check --fix .
	ruff format .

# Install package in editable mode (builds pyzmq/helics from source; needs libzmq/helics debs)
install:
	$(call check-command,pip)
	PIP_BREAK_SYSTEM_PACKAGES=1 pip install -e "."

# Install only the dev tools (the full ".[dev]" extra would also build pyzmq/helics from git)
install-dev:
	$(call check-command,pip)
	PIP_BREAK_SYSTEM_PACKAGES=1 pip install "codespell>=2.2.4" pytest pytest-cov ruff vulture pyzmq

# Lint code (ruff, codespell) and fix issues
lint:
	$(call check-command,ruff)
	$(call check-command,codespell)
	ruff check . --fix
	codespell --config ../../.codespellrc . -w

# Run unit tests
test:
	$(call check-command,pytest)
	PYTHONPATH=$(CURDIR) pytest $(TEST)

##
## --- Library dependencies (debs) ---
##

libzmq-build: build/zeromq-$(ZMQ_VERSION)
build/zeromq-$(ZMQ_VERSION):
	# Libzmq source at https://github.com/zeromq/libzmq
	@echo "Building libzmq package with draft support"
	mkdir -p build
	cd build
	# Download and extract the source code
	wget https://github.com/zeromq/libzmq/releases/download/v$(ZMQ_VERSION)/zeromq-$(ZMQ_VERSION).tar.gz -O libzmq.tar.gz
	tar -xzf libzmq.tar.gz
	rm -f libzmq.tar.gz
	cd zeromq-$(ZMQ_VERSION)

	# Configure and build the library
	./configure --prefix="$(ZMQ_PREFIX)" --enable-drafts
	make -j$(NRPOCS)

# Build the libzmq Debian package with draft support.
libzmq-deb: build/libzmq_$(ZMQ_VERSION)-1_amd64.deb
build/libzmq_$(ZMQ_VERSION)-1_amd64.deb: build/zeromq-$(ZMQ_VERSION)
	# Libzmq source at https://github.com/zeromq/libzmq
	# Create the Debian package using checkinstall
	cd build/zeromq-$(ZMQ_VERSION)
	echo "ZeroMQ is a high-performance asynchronous messaging library." >> ./description-pak
	sudo checkinstall --pkgname=libzmq --pkgversion=$(ZMQ_VERSION) \
	  --provides=libzmq --requires=libc6 \
	  --install=no \
	  --default \
	  --pkglicense=MPL-2.0 \
	  --maintainer="$(PACKAGE_MAINTAINER)" \
	  --pkgsource=https://github.com/zeromq/libzmq
	sudo mv libzmq_$(ZMQ_VERSION)-1_amd64.deb ..
	sudo touch ../libzmq_$(ZMQ_VERSION)-1_amd64.deb

# Install the libzmq deb
libzmq-install: build/libzmq-install
build/libzmq-install: build/libzmq_$(ZMQ_VERSION)-1_amd64.deb
	sudo dpkg -i build/libzmq_$(ZMQ_VERSION)-1_amd64.deb
	# flag that install is completed for future make tasks
	touch build/libzmq-install

helics-build: build/helics/build
build/helics/build: build/libzmq-install
	mkdir -p ./build/helics
	cd ./build/helics
	wget -O helics.tgz https://github.com/GMLC-TDC/HELICS/releases/download/v$(HELICS_VERSION)/Helics-v$(HELICS_VERSION)-source.tar.gz
	tar -xaf helics.tgz
	rm helics.tgz
	mkdir -p build
	touch build
	cd build
	cmake -D HELICS_USE_SYSTEM_ZEROMQ_ONLY=ON ..
	make -j$(NRPOCS)

# Build helics deb package
helics-deb: build/helics_$(HELICS_VERSION)-1_amd64.deb
build/helics_$(HELICS_VERSION)-1_amd64.deb: build/helics/build
	cd build/helics/build
	echo "HELICS binaries and libraries - Source: https://github.com/GMLC-TDC/HELICS/" >> ./description-pak
	sudo checkinstall --pkgname=helics --pkgversion=$(HELICS_VERSION) \
	  --provides=helics --requires=libc6 \
	  --install=no \
	  --default \
	  --pkglicense=BSD-3-Clause \
	  --maintainer="$(PACKAGE_MAINTAINER)" \
	  --pkgsource=https://github.com/GMLC-TDC/HELICS/
	sudo mv helics_$(HELICS_VERSION)-1_amd64.deb ../..
	sudo touch ../../helics_$(HELICS_VERSION)-1_amd64.deb

# Install helics deb package
helics-install: build/helics-install
build/helics-install: build/helics_$(HELICS_VERSION)-1_amd64.deb
	sudo dpkg -i build/helics_$(HELICS_VERSION)-1_amd64.deb
	# flag that install is completed for future make tasks
	touch build/helics-install

##
## --- Python packaging ---
##

# Build pybennu wheel
dist:
	$(UV) build --wheel

# Build pybennu source distribution
sdist:
	$(UV) build --sdist

# Create wheelhouse of pybennu and all dependencies in a .tar.gz
# Build and install libzmq and helics debs first so wheels
# are built against the proper libraries
wheelhouse:
	$(PIP) wheel --wheel-dir=dist/pybennu/ .
	tar -C dist/ -cvzf dist/pybennu-wheelhouse.tar.gz pybennu/
	rm -rf dist/pybennu/

##
## --- Cleanup ---
##
# Clean up build and dist files
clean: clean-pyc clean-build

clean-build:
	@rm -rfv "$(BUILD_DIR)"
	@rm -rfv "$(DIST_DIR)"
	@rm -rfv *.egg-info
	@rm -rfv *.dist-info
	@rm -rfv .eggs

clean-pyc:
	@rm -rf .ruff_cache .pytest_cache htmlcov .coverage coverage.xml
	@find . -name "*~" -exec rm -fv {} +
	@find . -name "__pycache__" -exec rm -rfv {} +
	@find . -name ".cache" -exec rm -rfv {} +