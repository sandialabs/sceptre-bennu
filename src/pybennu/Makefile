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

define BROWSER_PYSCRIPT
import os, webbrowser, sys
try:
    from urllib import pathname2url
except:
    from urllib.request import pathname2url

webbrowser.open("file://" + pathname2url(os.path.abspath(sys.argv[1])))
endef
export BROWSER_PYSCRIPT
BROWSER := python -c "$$BROWSER_PYSCRIPT"
BUILD_DIR = $(CURDIR)/build
DIST_DIR = $(CURDIR)/dist
POST = $(CURDIR)/dist/postinst

##
# Packaging
##

define PACKAGE_DESCRIPTION
Modeling and simulation application for SCADA/ICS.
endef
TIMESTAMP = $(shell date +%Y%m%d_%H-%M-%S)
PACKAGE_SECTION = python
PACKAGE_PRIORITY = extra
PACKAGE_COMPRESSION = bzip2
<<<<<<< HEAD
PACKAGE_FULL_VERSION:=$(shell git describe --tags)
=======
PACKAGE_FULL_VERSION:=$(shell git describe --tags --always)
>>>>>>> upstream/main
PACKAGE_NAME = pybennu
PACKAGE_FILENAME = $(PACKAGE_NAME)_$(PACKAGE_FULL_VERSION)_amd64_$(TIMESTAMP).deb
PACKAGE_VENDOR = SNL
PACKAGE_MAINTAINER = Sandia National Laboratories <emulytics@sandia.gov>
PACKAGE_LICENSE = Proprietary
PACKAGE_URL = https://github.com/sandialabs/sceptre-bennu.git
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
.PHONY: dist test docs

help:
		@echo "clean		- Remove python, build, and test file artifacts."
		@echo "docs 		- Generate Sphinx HTML documentation and open index.html."
		@echo "install		- Install the project in the current python environment."
		@echo "uninstall 	- Uninstall the project."

helpdev:
		@echo "clean		- Remove python, build, and test file artifacts."
		@echo "coverage 	- Check code coverage with tox/pytest-cov."
		@echo "deb		- Create a Debian package."
		@echo "dist 		- Create a src and whl distribution."
		@echo "docs 		- Generate Sphinx HTML documentation and open index.html."
		@echo "install		- Install the project in the current python environment."
		@echo "install-dev	- Install the project in editable (-e) mode."
		@echo "lint 		- Check style with tox/pytest-pylint."
		@echo "test 		- Run tests on Python versions in tox.ini with tox/pytest."
		@echo "uninstall 	- Uninstall the project."
		@echo "wheelhouse   	- Create a wheelhouse (deps included) archive."

clean: root clean-pyc clean-build clean-tests

clean-build:
		@rm -rfv "$(BUILD_DIR)"
		@rm -rfv "$(DIST_DIR)"
		@rm -rfv *.egg-info
		@rm -rfv *.dist-info
		@rm -rfv .eggs

clean-pyc:
#		@find . -name "*.py[cod]" -exec rm -fv {} +
		@find . -name "*~" -exec rm -fv {} +
		@find . -name "__pycache__" -exec rm -rfv {} +
		@find . -name ".cache" -exec rm -rfv {} +

clean-tests:
		@rm -rf .tox
		@rm -rf htmlcov

coverage: install-extras
		@tox -e cov-report
		@$(BROWSER) htmlcov/index.html

deb: packagetools
		@mkdir -p dist/
		@echo 'printf "\n--- RUNNING PYBENNU AFTER INSTALL SCRIPT ---\n\n"' > $(POST)
		@echo 'mv /root/libzmq* /usr/lib/x86_64-linux-gnu' >> $(POST)
		@echo 'printf "\n\tINSTALLING ZMQ WITH DRAFT SUPPORT...\n\n"' >> $(POST)
		@echo 'wget --no-check-certificate https://raw.githubusercontent.com/zeromq/pyzmq/main/examples/draft/install.sh' >> $(POST)
		@echo 'sed -i -e "s/wget/wget --no-check-certificate/g" install.sh' >> $(POST)
		@echo 'sed -i -e "s/make -j/make -j $(nproc)/g" install.sh' >> $(POST)
		@echo 'sed -i -e "s/pip install/pip install --trusted-host pypi.org --trusted-host files.pythonhosted.org -I/g" install.sh' >> $(POST)
		@echo 'chmod +x install.sh' >> $(POST)
		@echo './install.sh' >> $(POST)
		@echo 'printf "\n\tINSTALLING HELICS/PYHELICS...\n\n"' >> $(POST)
		@echo 'printf "\n\tINSTALLING PYBENNU PIP DEPENDENCIES...\n\n"' >> $(POST)
		@echo 'pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org bitarray elasticsearch>=5.3.0 matplotlib>=1.5.3 networkx>=1.11 numpy>=1.11.2 opendssdirect.py~=0.6.1 py-expression-eval~=0.3.14 PYPOWER>=5.0.1 pyserial>=3.4 pyyaml>=3.12 requests>=2.20 scipy>=0.18.1 sysv_ipc' >> $(POST)
		@echo 'printf "\nDONE!!\n\n"' >> $(POST)
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

dist: root pytools
		@python3 setup.py sdist bdist_wheel

docs: install-extras
		@tox -e docs
		@$(BROWSER) docs/build/html/index.html

flake: install-extras
		@tox -e flake8

install: install-extras
		@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org .

install-dev: install-extras
		@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org -e .

install-extras: root pytools
		@pip3 install --trusted-host pypi.org --trusted-host files.pythonhosted.org .[extras] > /dev/null 2>&1

lint: install-extras
		@tox -e pylint

livedocs: install-extras
		@tox -e livedocs

packagetools:
ifdef packagetools
	$(error Cannot find packagetools: fpm. (Install with 'sudo gem install fpm'))
endif

pytools:
ifdef pytools
	$(error Cannot find pytools: $(pytools).)
endif

root:
ifneq (root,$(shell whoami))
	$(error Must run as root.)
endif

sdist: root pytools
		@python3 setup.py sdist

test: install-extras
		@tox

uninstall: root pytools
		@pip3 uninstall $(PACKAGE_NAME)

wheelhouse: root pytools
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
