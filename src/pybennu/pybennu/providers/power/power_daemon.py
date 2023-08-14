import argparse
import os
import platform
import sys
import signal
import time

from configparser import ConfigParser, NoOptionError

from pybennu.providers.utils.daemon import Daemon
import pybennu.distributed.swig._Endpoint as E


class ServerDaemon(Daemon):
    def __init__(self, args):
        signal.signal(signal.SIGINT,  self.handle_exit) # ctrl + c
        self.system = platform.system().lower()
        self.debug = args.debug
        # only care about the config file when starting
        if args.operation in ('start', 'restart'):
            if args.config_file:
                config_file = args.config_file
            else:
                config_file = f'/etc/sceptre/{args.env}.ini'

            if os.path.exists(config_file):
                self.config_file = os.path.abspath(config_file)
            else:
                print(f"\nThe file {config_file} does not exist - "
                       "please provide a valid config file.\n")
                sys.exit(-1)

        Daemon.__init__(self, f'/var/run/bennu-{args.env}.pid', '/dev/null',
                        f'/var/log/bennu-{args.env}.out',
                        f'/var/log/bennu-{args.env}.err')

    def run(self):
        config = ConfigParser()
        config.read(self.config_file)

        try:
            debug = self.debug
            if 'debug' in config['power-solver-service']:
                debug = config.get('power-solver-service', 'debug').strip().lower()
                debug = True if debug == 'true' else False
        except NoOptionError:
            print("\nERROR: The following must be defined in the "
                  "configuration file: 'debug'\n")
            sys.exit(-1)

        self.running = False

        if 'power-solver-service' in config.sections():
            server_endpoint = E.new_Endpoint()
            publish_endpoint = E.new_Endpoint()
            try:
                solver = config.get('power-solver-service', 'solver-type').strip()
            except NoOptionError:
                print("\nERROR: 'solver-type' must be defined in the "
                      "configuration file\n")
                sys.exit(-1)
            #########################################
            ################ Dummy ##################
            #########################################
            if solver == 'Dummy':
                try:
                    E.Endpoint_str_set(server_endpoint,
                        config.get('power-solver-service',
                                   'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                        config.get('power-solver-service',
                                   'publish-endpoint').strip())
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', 'publish-"
                          "endpoint'\n")
                    sys.exit(-1)

                from pybennu.executables.pybennu_test_ep_server \
                   import ElectricPowerService
                self.solver = ElectricPowerService(server_endpoint,
                                                   publish_endpoint,
                                                   debug)
            #########################################
            ################ PyPower ################
            #########################################
            elif solver == 'PyPower':
                try:
                    E.Endpoint_str_set(server_endpoint,
                        config.get('power-solver-service',
                                   'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                        config.get('power-solver-service',
                                   'publish-endpoint').strip().replace("*;",""))
                    case_file = config.get('power-solver-service', 'case-file').strip()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint,"
                          "'publish-endpoint', 'case-file'\n")
                    sys.exit(-1)

                from pybennu.providers.power.solvers.python_power \
                   import PyPower
                self.solver = PyPower(server_endpoint, publish_endpoint,
                                      case_file, debug)
            #########################################
            ############## PowerWorld ###############
            #########################################
            elif solver == 'PowerWorld':
                try:
                    E.Endpoint_str_set(server_endpoint,
                                       config.get('power-solver-service',
                                                  'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                                       config.get('power-solver-service',
                                                  'publish-endpoint').strip())
                    case_file = config.get('power-solver-service',
                                           'case-file').strip()
                    oneline_file = config.get('power-solver-service',
                                              'oneline-file').strip()
                    objects_file = config.get('power-solver-service',
                                              'objects-file').strip()
                    noise = config.get('power-solver-service',
                                       'noise').strip().lower()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', "
                          "'publish-endpoint', 'case-file', "
                          "'oneline-file', 'objects-file'\n")
                    sys.exit(-1)

                from pybennu.providers.power.solvers.power_world \
                   import PowerWorld
                self.solver = PowerWorld(server_endpoint, publish_endpoint,
                                         case_file, oneline_file, objects_file,
                                         debug, noise)
            #########################################
            ########### PowerWorldHelics ############
            #########################################
            elif solver == 'PowerWorldHelics':
                try:
                    case_file = config.get('power-solver-service',
                                           'case-file').strip()
                    oneline_file = config.get('power-solver-service',
                                              'oneline-file').strip()
                    config_file = config.get('power-solver-service',
                                             'config-file').strip()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'case-file', "
                          "'oneline-file', 'config-file'\n")
                    sys.exit(-1)

                from pybennu.providers.power.solvers.power_world_helics \
                   import PowerWorldHelics
                self.solver = PowerWorldHelics(case_file, config_file,
                                               oneline_file,
                                               debug)
            #########################################
            ################# PWDS ##################
            #########################################
            elif solver == 'PowerWorldDynamics':
                try:
                    E.Endpoint_str_set(server_endpoint,
                                       config.get('power-solver-service',
                                                  'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                                       config.get('power-solver-service',
                                                  'publish-endpoint').strip())

                    pwds_endpoint = config.get('power-solver-service',
                                               'pwds-endpoint').strip()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', "
                          "'publish-endpoint', 'pwds-endpoint'\n")
                    sys.exit(-1)

                kwargs = {}

                if config.has_option('power-solver-service', 'objects-file'):
                    kwargs['objects-file'] = config.get('power-solver-service',
                                                        'objects-file').strip()

                from pybennu.providers.power.solvers.pwds.power_world_dynamics \
                   import PowerWorldDynamics
                self.solver = PowerWorldDynamics(server_endpoint,
                                                 publish_endpoint,
                                                 pwds_endpoint,
                                                 debug, **kwargs)
            #########################################
            ################ OpenDSS ################
            #########################################
            elif solver == 'OpenDSS':
                try:
                    E.Endpoint_str_set(server_endpoint,
                                       config.get('power-solver-service',
                                                  'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                                       config.get('power-solver-service',
                                                  'publish-endpoint').strip())
                    case_file = config.get('power-solver-service',
                                           'case-file').strip()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', "
                          "'publish-endpoint', 'case-file'\n")
                    sys.exit(-1)

                jitter = config.get('power-solver-service',
                                    'jitter',
                                    fallback='enabled').strip().lower()
                clear  = config.get('power-solver-service',
                                    'clear-circuit',
                                    fallback='disabled').strip().lower()

                # Because OpenDSS won't create it's own directories (whyyyyyy???)
                os.makedirs('/home/root/Documents/OpenDSS', exist_ok=True)
                os.makedirs('/root/Documents/OpenDSS',      exist_ok=True)
                os.makedirs('/Documents/OpenDSS',           exist_ok=True)

                kwargs = {
                    "debug":         debug,
                    "jitter":        jitter,
                    "clear_circuit": clear
                }

                from pybennu.providers.power.solvers.opendss \
                    import OpenDSS
                self.solver = OpenDSS(server_endpoint, publish_endpoint,
                                      case_file, **kwargs)
            elif solver == 'RTDS':
                from pybennu.providers.power.solvers.rtds import RTDS
                try:
                    E.Endpoint_str_set(server_endpoint, config.get('power-solver-service', 'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint, config.get('power-solver-service', 'publish-endpoint').strip())
                    self.solver = RTDS(server_endpoint, publish_endpoint, config, debug)
                except NoOptionError:
                    print(f"\nERROR: The following must be defined in the "
                          f"configuration file: {RTDS.REQUIRED_CONF_KEYS}\n")
                    sys.exit(-1)
            #########################################
            ################ HELICS #################
            #########################################
            elif solver == 'Helics':
                try:
                    E.Endpoint_str_set(server_endpoint,
                                       config.get('power-solver-service',
                                                  'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                                       config.get('power-solver-service',
                                                  'publish-endpoint').strip())

                    config_file = config.get('power-solver-service',
                                             'config-file').strip()
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', "
                          "'publish-endpoint', 'config-file'\n")
                    sys.exit(-1)

                from pybennu.providers.power.solvers.helics \
                    import Helics
                self.solver = Helics(server_endpoint,
                                     publish_endpoint,
                                     config_file,
                                     debug)
            #########################################
            ############### Default #################
            #########################################
            else:
                try:
                    E.Endpoint_str_set(server_endpoint,
                                       config.get('power-solver-service',
                                                  'server-endpoint').strip())
                    E.Endpoint_str_set(publish_endpoint,
                                       config.get('power-solver-service',
                                                  'publish-endpoint').strip())
                except NoOptionError:
                    print("\nERROR: The following must be defined in the "
                          "configuration file: 'server-endpoint', 'publish-"
                          "endpoint'\n")
                    sys.exit(-1)

                from pybennu.executables.pybennu_test_ep_server \
                    import ElectricPowerService
                self.solver = ElectricPowerService(server_endpoint,
                                                   publish_endpoint,
                                                   debug)

        self.running = True
        self._solver_run()

        while self.running:
            time.sleep(1)

    def handle_exit(self, signum, stack):
        print("\nStopping power solver service...\n")
        self.running = False
        sys.exit()

    def _solver_run(self):
        self.solver.run()
        return


def server():
    parser = argparse.ArgumentParser(description="SCEPTRE Power Solver Service")
    parser.add_argument('-c', '--config-file', dest='config_file',
                        help='Config file to load (.ini)')
    parser.add_argument('-d', '--daemonize', dest='daemonize',
                        action='store_true',
                        help='Run as a daemon')
    parser.add_argument('-e', '--env', dest='env',
                        default='sceptre',
                        help='Environment for daemon to run in')
    parser.add_argument('operation', metavar='OPERATION',
                        type=str, choices=['start', 'stop', 'restart', 'status'],
                        help='Daemon operation - accepts any of these values: start, stop, restart, status')
    parser.add_argument('-v', '--debug', dest='debug',
                        action='store_true',
                        help='Enable debug option')

    args = parser.parse_args()

    handle = ServerDaemon(args)
    if handle.system == 'linux':
        if args.operation == 'start':
            if args.daemonize:
                handle.start()
                pid = handle.get_pid()
                if not pid:
                    print('Unable to start as a daemon')
            else:
                handle.run()
        elif args.operation == 'stop':
            handle.stop()
        elif args.operation == 'restart':
            print('Restarting daemon')
            pid = handle.get_pid()
            if not pid:
                print('Daemon is not already running - cannot restart')
            else:
                handle.restart()
        elif args.operation == 'status':
            print('Viewing daemon status')
            pid = handle.get_pid()
            if not pid:
                print('Daemon is not running')
            else:
                print(f'Daemon is running as PID {pid}')
    else:
        if args.operation == 'start':
            handle.run()
        elif args.operation == 'stop':
            handle.running = False
        elif args.operation == 'restart':
            print('Restarting')
            handle.running = False
            time.sleep(5)
            handle.run()
        elif args.operation == 'status':
            if handle.running:
                print('Daemon is running')
            else:
                print('Daemon is not running')
    sys.exit(0)
