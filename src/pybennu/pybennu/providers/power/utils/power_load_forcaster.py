import os, random, signal, sys, time, zmq

from ConfigParser import ConfigParser, NoOptionError

from pybennu.base.power.protobuf         import PowerSystem
from pybennu.base.scripts                import argparse
from pybennu.base.scripts.daemon         import Daemon
from pybennu.services.majordomo.mdcliapi import MajorDomoClient
from pybennu.services.rpc.protobuf       import Request, Response

class ServerDaemon(Daemon):
    def __init__(self, args):
        if args.operation in ('start', 'restart'): # only care about the config file when starting
            if args.config_file:
                config_file = args.config_file
            else:
                config_file = '/etc/sceptre/%s.ini' % (args.env)

            if os.path.exists(config_file):
                self.config_file = os.path.abspath(config_file)
            else:
                print('\nThe file %s does not exist - please provide a valid config file.\n' % (config_file))
                sys.exit(-1)

        Daemon.__init__(self, '/var/run/bennu-%s.pid' % (args.env), '/dev/null', '/var/log/bennu-%s.out' % (args.env), '/var/log/bennu-%s.err' % (args.env))

    def run(self):
        print("\nStarting power load forecasting service...\n")

        config = ConfigParser()
        config.read(self.config_file)

        self.running = False

        if 'power-load-forecaster' in config.sections():
            broker    = config.get('power-load-forecaster', 'service-broker').strip()
            case_file = config.get('power-load-forecaster', 'save-case').strip()

            min_sleep = int(config.get('power-load-forecaster', 'min-sleep-time').strip())
            max_sleep = int(config.get('power-load-forecaster', 'max-sleep-time').strip())

            min_forecast = int(config.get('power-load-forecaster', 'min-forecast-percent').strip())
            max_forecast = int(config.get('power-load-forecaster', 'max-forecast-percent').strip())

            jitter      = int(config.get('power-load-forecaster', 'jitter').strip())
            jitter_time = int(config.get('power-load-forecaster', 'jitter-time').strip())

            client = MajorDomoClient(broker, True)

            system = PowerSystem()
            with open(case_file, 'rb') as f:
                system.ParseFromString(f.read())

        def handle_exit(signum, stack):
            print("\nStopping power load forecasting service...\n")
            self.running = False

        signal.signal(signal.SIGINT,  handle_exit)
        signal.signal(signal.SIGTERM, handle_exit)

        self.running = True

        forecast      = 1.0
        last_forecast = time.time()
        last_jitter   = time.time()
        delay         = 0

        while self.running:
            if last_forecast + delay < time.time():
                forecast = random.randint(min_forecast, max_forecast) / 100.0

                print('About to forecast loads at %f percent...' % (forecast))

                updated      = PowerSystem()
                updated.name = system.name

                for load in system.loads:
                    l      = updated.loads.add()
                    l.name = load.name
                    l.mw   = load.mw   * forecast
                    l.mvar = load.mvar * forecast

                request = Request()
                request.method = 'update-system'
                request.data   = updated.SerializeToString()

                if client.send('power-solver', request.SerializeToString()) is None:
                    print('Received invalid reply from broker... exiting')
                    self.running = False
                else:
                    print('Forecasted load in power system...')

                last_forecast = time.time()
                delay         = random.randint(min_sleep, max_sleep)

                print('Sleeping for %i seconds in load forecaster...' % (delay))
            elif last_jitter + jitter_time < time.time():
                current_jitter = random.randint(jitter * -1, jitter) / 100.0

                print('About to jitter loads by %f percent...' % (current_jitter))

                updated      = PowerSystem()
                updated.name = system.name

                for load in system.loads:
                    l      = updated.loads.add()
                    l.name = load.name
                    l.mw   = load.mw   * (forecast + current_jitter)
                    l.mvar = load.mvar * (forecast + current_jitter)

                request = Request()
                request.method = 'update-system'
                request.data   = updated.SerializeToString()

                if client.send('power-solver', request.SerializeToString()) is None:
                    print('Received invalid reply from broker... exiting')
                    self.running = False
                else:
                    print('Jittered load in power system...')

                last_jitter = time.time()

            time.sleep(1)


def server():
    parser = argparse.ArgumentParser(description="SCEPTRE Power Load Forecasting Service")
    parser.add_argument('-c', '--config-file', dest='config_file',
                        help='Config file to load (.ini)')
    parser.add_argument('-d', '--daemonize', dest='daemonize',
                        action='store_true',
                        help='Run as a daemon')
    parser.add_argument('-e', '--env', dest='env',
                        default='bennu',
                        help='Environment for daemon to run in')
    parser.add_argument('operation', metavar='OPERATION',
                        type=str, choices=['start', 'stop', 'restart', 'status'],
                        help='Daemon operation - accepts any of these values: start, stop, restart, status')

    args = parser.parse_args()

    handle = ServerDaemon(args)
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
            print('Daemon is running as PID %s' % (pid))

    sys.exit(0)
