.SUFFIXES:
.SUFFIXES: .rst .pdf

##
# Tools
##

packagetools =
FPM = fpm
ifeq (,$(shell which ${FPM}))
	packagetools += $(FPM)
endif

pytools =
PYTHON = python3
ifeq (,$(shell which ${PYTHON}))
	pytools += $(PYTHON)
endif
PIP = pip3
ifeq (,$(shell which ${PIP}))
	pytools += $(PIP)
endif

##
# Variables
##

BUILD_DIR = $(CURDIR)/build
DIST_DIR = $(CURDIR)/dist
POST = $(CURDIR)/postinst

##
# Packaging
##

define PACKAGE_DESCRIPTION
Modeling and simulation application for SCADA/ICS.
endef
PACKAGE_SECTION = python
PACKAGE_PRIORITY = optional
PACKAGE_COMPRESSION = bzip2
# 6.0.1707507794.8a88a442
PACKAGE_FULL_VERSION := 6.0.$(shell date +%s).$(shell git rev-parse --short HEAD)
PACKAGE_NAME = pybennu
PACKAGE_FILENAME = $(PACKAGE_NAME)_$(PACKAGE_FULL_VERSION)_amd64.deb
PACKAGE_VENDOR = SNL
PACKAGE_MAINTAINER = Sandia National Laboratories <emulytics@sandia.gov>
PACKAGE_LICENSE = GPLv3
PACKAGE_URL = https://github.com/sandialabs/sceptre-bennu
PACKAGE_ARCHITECTURE = amd64

##
# Utility functions
##

# Iterate values of $(1) and prefix each of it with $(2).
# When using with an empty scalars ("") returns nothing instead of
# "$(2) ''".
define iter
	$(if $(1),$(foreach item,$(1),$(2) '$(item)'),)
endef

##
# Targets
##
.PHONY: packagetools pytools root clean clean-build clean-pyc deb dist sdist install install-dev uninstall wheelhouse bull

help:
	@echo "clean		- Remove python and build file artifacts."
	@echo "deb			- Create a Debian package."
	@echo "dist 		- Create a src and whl distribution."
	@echo "sdist		- Create a src distribution (sdist)."
	@echo "install		- Install the project in the current python environment."
	@echo "install-dev	- Install the project in editable (-e) mode."
	@echo "uninstall	- Uninstall the project."
	@echo "wheelhouse	- Create a wheelhouse (deps included) archive."
	@echo "bull			- bull."

packagetools:
ifdef packagetools
	$(error Cannot find packagetools: fpm (Install with 'sudo gem install fpm'))
endif

pytools:
ifdef pytools
	$(error Cannot find pytools: $(pytools).)
endif

root:
ifneq (root,$(shell whoami))
	$(error Must run as root.)
endif

clean: clean-pyc clean-build

clean-build:
	@rm -rfv "$(BUILD_DIR)"
	@rm -rfv "$(DIST_DIR)"
	@rm -rfv *.egg-info
	@rm -rfv *.dist-info
	@rm -rfv .eggs

clean-pyc:
#	@find . -name "*.py[cod]" -exec rm -fv {} +
	@find . -name "*~" -exec rm -fv {} +
	@find . -name "__pycache__" -exec rm -rfv {} +
	@find . -name ".cache" -exec rm -rfv {} +

deb: packagetools
	@mkdir -p $(DIST_DIR)
	@$(FPM) \
		--verbose \
		-f \
		-s python \
		-t deb \
		-d cmake \
		-d gcc \
		-d g++ \
		-d make \
		-d python3-dev \
		-d python3-pip \
		-d python3-setuptools \
		-d wget \
		-d libboost-python-dev \
		-d libzmq5-dev \
		-p '$(subst __colon__,:,$(DIST_DIR)/$(PACKAGE_FILENAME))' \
		--name '$(PACKAGE_NAME)' \
		$(call iter,$(PACKAGE_VENDOR),--vendor) \
		--maintainer '$(PACKAGE_MAINTAINER)' \
		--license '$(PACKAGE_LICENSE)' \
		--description '$(PACKAGE_DESCRIPTION)' \
		--category '$(PACKAGE_SECTION)' \
		$(call iter,$(PACKAGE_URL),--url) \
		--deb-no-default-config-files \
		--version '$(PACKAGE_FULL_VERSION)' \
		--architecture '$(PACKAGE_ARCHITECTURE)' \
		--python-bin python3 \
		--python-pip pip3 \
		--python-install-bin /usr/local/bin \
		--python-install-lib /usr/lib/python3/dist-packages \
		--no-python-dependencies \
		--after-install $(POST) \
		setup.py

dist: pytools
	@python3 setup.py sdist bdist_wheel

sdist: pytools
	@python3 setup.py sdist

install: root pytools
	@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org .

install-dev: pytools
	@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org -e .

uninstall: root pytools
	@pip3 uninstall $(PACKAGE_NAME)

wheelhouse: pytools
	@pip3 wheel --wheel-dir=dist/$(PACKAGE_NAME)/ .
	@tar -C dist/ -cvzf dist/$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION)-wheelhouse.tar.gz $(PACKAGE_NAME)/
	@rm -rf dist/$(PACKAGE_NAME)/

bull: pytools
	@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org colorama > /dev/null 2>&1
	@$(BULL)
define BULL_PYSCRIPT
from colorama import init, Fore, Back, Style

bull = [r"""
 (___)
 (o o)_____/
  @@ `    \\
   \ ____, /
   //    //
  ^^    ^^
"""]

init()

def colorize(chars, color):
	for char in chars:
		bull[0] = bull[0].replace(char, color+char+Style.RESET_ALL)

colorize("o", Fore.RED)
colorize("()", Style.BRIGHT)
colorize("_/\\`,", Fore.YELLOW+Style.DIM)
colorize("@", Style.DIM+Fore.RED)

print(bull[0])
endef
export BULL_PYSCRIPT
BULL := python3 -c "$$BULL_PYSCRIPT"
