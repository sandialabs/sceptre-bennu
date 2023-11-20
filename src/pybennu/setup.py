#!/usr/bin/env python3
"""Python setup module for pybennu."""
import itertools
import os
import sys
import site
from subprocess import call

from setuptools import setup, find_packages
from setuptools.command.develop import develop
from setuptools.command.egg_info import egg_info
from setuptools.command.install import install

"""
BEGIN CUSTOM INSTALL COMMANDS
These classes are used to hook into setup.py's install process.
Depending on the context:
$ pip install my-package

Can yield `setup.py install`, `setup.py egg_info`, or `setup.py develop`
"""
def binaries_dir():
    """Return the installation directory, or None"""
    if '--user' in sys.argv:
        paths = (site.getusersitepackages(),)
    else:
        py_version = '%s.%s' % (sys.version_info[0], sys.version_info[1])
        paths = (s % (py_version) for s in (
            sys.prefix + '/lib/python%s/dist-packages/',
            sys.prefix + '/lib/python%s/site-packages/',
            sys.prefix + '/local/lib/python%s/dist-packages/',
            sys.prefix + '/local/lib/python%s/site-packages/',
            '/Library/Python/%s/site-packages/',
        ))
        paths = itertools.chain(paths,
                                iter([sys.prefix + '\Lib\site-packages']))
    for path in paths:
        if os.path.exists(path):
            return path
    return None


def custom_install_command():
    if 'win' in sys.platform:
        paths = os.listdir('deps/win/site-packages')
        print("INSTALLING PYZMQ FILES...")
        for p in paths:
            src = '.\deps\win\site-packages\%s' % p
            dst = binaries_dir() + '\\' + p
            call(['xcopy', '/sivyq', src, dst])
        print("INSTALLING PYZMQ LIB...")
        zmq = './deps/win/site-packages/zmq/'
        lib = [x for x in os.listdir(zmq) if 'dll' in x][0]
        src = '.\deps\win\site-packages\zmq\%s' % lib
        dst = 'C:\Windows\\*'
        print(src, dst)
        call(['xcopy', '/sivyq', src, dst])
    else:
        paths = os.listdir('deps/linux/site-packages')
        print("INSTALLING PYZMQ FILES...")
        for p in paths:
            src = './deps/linux/site-packages/%s' % p
            dst = binaries_dir()
            call(['cp', '-rf', src, dst])
        print("INSTALLING PYZMQ LIB...")
        paths = os.listdir('deps/linux/x86_64-linux-gnu')
        for p in paths:
            src = 'deps/linux/x86_64-linux-gnu/%s' % p
            dst = '/usr/lib/x86_64-linux-gnu'
            call(['cp', '-af', src, dst])


class CustomInstallCommand(install):
    def run(self):
        install.run(self)
        custom_install_command()


class CustomDevelopCommand(develop):
    def run(self):
        develop.run(self)
        custom_install_command()


class CustomEggInfoCommand(egg_info):
    def run(self):
        egg_info.run(self)
        custom_install_command()


"""
END CUSTOM INSTALL COMMANDS
"""


# If you need something in a newer version of a package, increase the version pinned here
requires = [
    'elasticsearch>=5.3.0',  # ==7.14.0
    'helics~=2.7.1',
    'matplotlib>=1.5.3',  # ~=3.4.3
    'networkx>=1.11',  # ==2.6.2
    'numpy>=1.11.2',  # >=1.11.2  ~=1.21.2
    'opendssdirect.py~=0.6.1',
    'py-expression-eval~=0.3.14',
    'PYPOWER>=5.0.1',  # ==5.1.15
    'pyserial>=3.4',  # >=3.4
    'PyYAML>=3.12',  # pyyaml>=3.12  ==5.4.1
    'requests>=2.20',  # ~=2.26.0
    'scipy>=0.18.1',  # ~=1.7.1
]

if 'linux' in sys.platform:
    requires.extend([
        'bitarray',  # ==2.3.2
        'sysv_ipc',  # ==1.1.0
    ])


# Required for .deb to work properly. When using 'pip3 install ...',
# these data_files are not used.
data_files = [
    ('/root/', ['deps/linux/x86_64-linux-gnu/libzmq.a']),
    ('/root/', ['deps/linux/x86_64-linux-gnu/libzmq.so.5.1.5'])
] if 'linux' in sys.platform else []


entries = {
    'console_scripts': [
        'pybennu-power-solver             = pybennu.providers.power.power_daemon:server',
        'pybennu-power-load-forecaster    = pybennu.providers.power.utils.power_load_forecaster:server',
        'pybennu-sel-ams-reader           = pybennu.providers.power.utils.sel.sel_ams_reader:server',
        'pybennu-sel-ams-writer           = pybennu.providers.power.utils.sel.sel_ams_writer:server',
        'pybennu-groundtruth-monitor      = pybennu.analytics.groundtruth_monitor:main',
        'pybennu-test-ep-server           = pybennu.executables.pybennu_test_ep_server:main',
        'pybennu-test-ep-server-helics    = pybennu.executables.pybennu_test_ep_server_helics:main',
        'pybennu-test-subscriber          = pybennu.executables.pybennu_test_subscriber:main',
        'pybennu-probe                    = pybennu.executables.pybennu_probe:main',
        'pybennu-alicanto                 = pybennu.executables.pybennu_alicanto:main',
        'pybennu-siren                    = pybennu.siren.siren:main'
    ]
}


setup(
    cmdclass                = {
        'install': CustomInstallCommand,
        'develop': CustomDevelopCommand,
        'egg_info': CustomEggInfoCommand,
    },
    name                    = 'pybennu',
    # NOTE: as of setuptools 66, "version" must be PEP 440-compliant string.
    # In other words, a git commit hash is not valid for "version" anymore.
    # https://peps.python.org/pep-0440/
    # TODO: move to pyproject.toml, use setuptools_scm to do version handling
    # https://github.com/pypa/setuptools_scm
    version                 = '0.0.0',
    description             = 'bennu python providers and utilities',
    url                     = 'https://github.com/sandialabs/sceptre-bennu.git',
    author                  = 'Sandia National Laboratories',
    author_email            = 'emulytics@sandia.gov',
    license                 = 'GPLv3',
    platforms               = 'Linux',
    classifiers             = [
        'Development Status :: 5 - Production/Stable',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
    ],
    python_requires         = '>=3.8',
    entry_points            = entries,
    data_files              = data_files,
    packages                = find_packages(),
    install_requires        = requires,
    include_package_data    = True,
)
