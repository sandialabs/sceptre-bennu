import argparse
import os
import platform
import signal
import sys
import time
from configparser import ConfigParser, NoOptionError
from pathlib import Path

from loguru import logger

import pybennu.distributed.swig._Endpoint as E
from pybennu.log import enable_debug, setup_logging
from pybennu.providers.utils.daemon import Daemon
from pybennu.settings import load_settings_yaml


class ServerDaemon(Daemon):
    def __init__(self, args: argparse.Namespace) -> None:
        signal.signal(signal.SIGINT, self.handle_exit)  # ctrl + c

        self.system: str = platform.system().lower()
        self.debug: bool = args.debug

        # only care about the config file when starting
        if args.operation in ("start", "restart"):
            if os.path.exists(args.config_file):
                self.config_file = os.path.abspath(args.config_file)
            else:
                logger.error(
                    f"The file {args.config_file} does not exist - "
                    "please provide a valid config file."
                )
                sys.exit(-1)

        Daemon.__init__(
            self,
            f"/var/run/bennu-{args.env}.pid",
            "/dev/null",
            f"/var/log/bennu-{args.env}.out",
            f"/var/log/bennu-{args.env}.err",
        )

    def run(self) -> None:
        config = ConfigParser()
        config.read(self.config_file)

        try:
            debug = self.debug
            if "debug" in config["power-solver-service"]:
                debug = config.get("power-solver-service", "debug").strip().lower()
                debug = self.debug or debug == "true"  # -v wins over config
        except NoOptionError:
            logger.error(
                "The following must be defined in the configuration file: 'debug'"
            )
            sys.exit(-1)
        if debug:
            enable_debug()

        self.running = False

        if "power-solver-service" in config.sections():
            server_endpoint = E.new_Endpoint()
            publish_endpoint = E.new_Endpoint()

            try:
                solver = config.get("power-solver-service", "solver-type").strip()
            except NoOptionError:
                logger.error("'solver-type' must be defined in the configuration file")
                sys.exit(-1)

            #########################################
            ################ Dummy ##################
            #########################################
            if solver == "Dummy":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', 'publish-"
                        "endpoint'"
                    )
                    sys.exit(-1)

                from pybennu.executables.pybennu_test_ep_server import (
                    ElectricPowerService,
                )

                self.solver = ElectricPowerService(
                    server_endpoint, publish_endpoint, debug
                )

            #########################################
            ################ PyPower ################
            #########################################
            elif solver == "PyPower":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint")
                        .strip()
                        .replace("*;", ""),
                    )
                    case_file = config.get("power-solver-service", "case-file").strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint,"
                        "'publish-endpoint', 'case-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.power.solvers.python_power import PyPower

                self.solver = PyPower(
                    server_endpoint, publish_endpoint, case_file, debug
                )

            #########################################
            ############## PowerWorld ###############
            #########################################
            elif solver == "PowerWorld":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )
                    case_file = config.get("power-solver-service", "case-file").strip()
                    oneline_file = config.get(
                        "power-solver-service", "oneline-file"
                    ).strip()
                    objects_file = config.get(
                        "power-solver-service", "objects-file"
                    ).strip()
                    noise = config.get("power-solver-service", "noise").strip().lower()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', "
                        "'publish-endpoint', 'case-file', "
                        "'oneline-file', 'objects-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.power.solvers.power_world import PowerWorld

                self.solver = PowerWorld(
                    server_endpoint,
                    publish_endpoint,
                    case_file,
                    oneline_file,
                    objects_file,
                    debug,
                    noise,
                )

            #########################################
            ########### PowerWorldHelics ############
            #########################################
            elif solver == "PowerWorldHelics":
                try:
                    case_file = config.get("power-solver-service", "case-file").strip()
                    oneline_file = config.get(
                        "power-solver-service", "oneline-file"
                    ).strip()
                    config_file = config.get(
                        "power-solver-service", "config-file"
                    ).strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'case-file', "
                        "'oneline-file', 'config-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.power.solvers.power_world_helics import (
                    PowerWorldHelics,
                )

                self.solver = PowerWorldHelics(
                    case_file, config_file, oneline_file, debug
                )

            #########################################
            ################# PWDS ##################
            #########################################
            elif solver == "PowerWorldDynamics":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )

                    pwds_endpoint = config.get(
                        "power-solver-service", "pwds-endpoint"
                    ).strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', "
                        "'publish-endpoint', 'pwds-endpoint'"
                    )
                    sys.exit(-1)

                kwargs = {}

                if config.has_option("power-solver-service", "objects-file"):
                    kwargs["objects-file"] = config.get(
                        "power-solver-service", "objects-file"
                    ).strip()

                from pybennu.providers.power.solvers.pwds.power_world_dynamics import (
                    PowerWorldDynamics,
                )

                self.solver = PowerWorldDynamics(
                    server_endpoint, publish_endpoint, pwds_endpoint, debug, **kwargs
                )

            #########################################
            ################ OpenDSS ################
            #########################################
            elif solver == "OpenDSS":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )
                    case_file = config.get("power-solver-service", "case-file").strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', "
                        "'publish-endpoint', 'case-file'"
                    )
                    sys.exit(-1)

                jitter = (
                    config.get("power-solver-service", "jitter", fallback="enabled")
                    .strip()
                    .lower()
                )
                clear = (
                    config.get(
                        "power-solver-service", "clear-circuit", fallback="disabled"
                    )
                    .strip()
                    .lower()
                )

                # Because OpenDSS won't create it's own directories (whyyyyyy???)
                os.makedirs("/home/root/Documents/OpenDSS", exist_ok=True)
                os.makedirs("/root/Documents/OpenDSS", exist_ok=True)
                os.makedirs("/Documents/OpenDSS", exist_ok=True)

                kwargs = {"debug": debug, "jitter": jitter, "clear_circuit": clear}

                from pybennu.providers.power.solvers.opendss import OpenDSS

                self.solver = OpenDSS(
                    server_endpoint, publish_endpoint, case_file, **kwargs
                )

            #########################################
            ################# RTDS ##################
            #########################################
            elif solver == "RTDS":
                from pybennu.providers.power.solvers.rtds import RTDS

                config_path = Path(
                    config.get("power-solver-service", "config-file").strip()
                )
                settings = load_settings_yaml(config_path)  # type: PybennuSettings

                E.Endpoint_str_set(server_endpoint, settings.server_endpoint)
                E.Endpoint_str_set(publish_endpoint, settings.publish_endpoint)

                self.solver = RTDS(server_endpoint, publish_endpoint, settings)

            #########################################
            ############ Generic Python #############
            #########################################
            elif solver == "GenericPython":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )
                    python_sim = config.get(
                        "power-solver-service", "simulation-file"
                    ).strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the configuration file: 'server-endpoint', 'publish-endpoint', and 'simulation-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.power.solvers.generic_python.generic_python import (
                    GenericPython,
                )

                self.solver = GenericPython(
                    server_endpoint, publish_endpoint, python_sim, debug
                )
            #########################################
            ################# Siren #################
            #########################################
            elif solver == "Siren":
                from pybennu.providers.power.solvers.siren import Siren

                config_path = Path(
                    config.get("power-solver-service", "config-file").strip()
                )
                settings = load_settings_yaml(config_path)  # type: PybennuSettings

                E.Endpoint_str_set(server_endpoint, settings.server_endpoint)
                E.Endpoint_str_set(publish_endpoint, settings.publish_endpoint)

                self.solver = Siren(server_endpoint, publish_endpoint, settings)

            #########################################
            ################ OpalRT #################
            #########################################
            elif solver == "OPALRT":
                from pybennu.providers.power.solvers.opal_rt import OPALRT

                config_path = Path(
                    config.get("power-solver-service", "config-file").strip()
                )
                settings = load_settings_yaml(config_path)  # type: PybennuSettings

                E.Endpoint_str_set(server_endpoint, settings.server_endpoint)
                E.Endpoint_str_set(publish_endpoint, settings.publish_endpoint)

                self.solver = OPALRT(server_endpoint, publish_endpoint, settings)

            #########################################
            ################ HELICS #################
            #########################################
            elif solver == "Helics":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )

                    config_file = config.get(
                        "power-solver-service", "config-file"
                    ).strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', "
                        "'publish-endpoint', 'config-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.power.solvers.helics import Helics

                self.solver = Helics(
                    server_endpoint, publish_endpoint, config_file, debug
                )

            #########################################
            ############### ALICANTO ################
            #########################################
            elif solver == "Alicanto":
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )

                    config_file = config.get(
                        "power-solver-service", "config-file"
                    ).strip()
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', "
                        "'publish-endpoint', 'config-file'"
                    )
                    sys.exit(-1)

                from pybennu.providers.alicanto.alicanto import Alicanto

                self.solver = Alicanto(
                    server_endpoint, publish_endpoint, config_file, debug
                )

            #########################################
            ############### Default #################
            #########################################
            else:
                try:
                    E.Endpoint_str_set(
                        server_endpoint,
                        config.get("power-solver-service", "server-endpoint").strip(),
                    )
                    E.Endpoint_str_set(
                        publish_endpoint,
                        config.get("power-solver-service", "publish-endpoint").strip(),
                    )
                except NoOptionError:
                    logger.error(
                        "The following must be defined in the "
                        "configuration file: 'server-endpoint', 'publish-"
                        "endpoint'"
                    )
                    sys.exit(-1)

                from pybennu.executables.pybennu_test_ep_server import (
                    ElectricPowerService,
                )

                self.solver = ElectricPowerService(
                    server_endpoint, publish_endpoint, debug
                )

        self.running = True
        logger.info("Starting {} (debug={})", type(self.solver).__name__, debug)
        self._solver_run()

        while self.running:
            time.sleep(1)

    def handle_exit(self, signum, stack) -> None:
        logger.info("Stopping power solver service...")
        self.running = False
        sys.exit()

    def _solver_run(self) -> None:
        self.solver.run()


# Note: this function is called from setup.py, as 'pybennu-power-solver'
def server():
    parser = argparse.ArgumentParser(
        description="SCEPTRE Power Solver Service",
    )
    parser.add_argument(
        "-c",
        "--config-file",
        dest="config_file",
        default="/etc/sceptre/config.ini",
        help="Config file to load (.ini)",
    )
    parser.add_argument(
        "-d",
        "--daemonize",
        dest="daemonize",
        action="store_true",
        help="Run as a daemon",
    )
    parser.add_argument(
        "-e",
        "--env",
        dest="env",
        default="pybennu",  # previous default: sceptre
        help="Environment for daemon to run in, this determines names of files in /var, e.g. /var/log/bennu-pybennu.err for --env=pybennu",
    )
    parser.add_argument(
        "operation",
        metavar="OPERATION",
        type=str,
        choices=["start", "stop", "restart", "status"],
        help="Daemon operation - accepts any of these values: start, stop, restart, status",
    )
    parser.add_argument(
        "-v",
        "--debug",
        dest="debug",
        action="store_true",
        help="Enable debug option",
    )

    args = parser.parse_args()
    setup_logging(
        "pybennu-power-solver",
        debug=args.debug,
        file_by_default=args.operation in ("start", "restart"),
    )

    handle = ServerDaemon(args)

    if handle.system == "linux":
        if args.operation == "start":
            if args.daemonize:
                handle.start()
                pid = handle.get_pid()
                if not pid:
                    print("Unable to start as a daemon")
            else:
                handle.run()
        elif args.operation == "stop":
            handle.stop()
        elif args.operation == "restart":
            print("Restarting daemon")
            pid = handle.get_pid()
            if not pid:
                print("Daemon is not already running - cannot restart")
            else:
                handle.restart()
        elif args.operation == "status":
            print("Viewing daemon status")
            pid = handle.get_pid()
            if not pid:
                print("Daemon is not running")
            else:
                print(f"Daemon is running as PID {pid}")
    else:
        if args.operation == "start":
            handle.run()
        elif args.operation == "stop":
            handle.running = False
        elif args.operation == "restart":
            print("Restarting")
            handle.running = False
            time.sleep(5)
            handle.run()
        elif args.operation == "status":
            if handle.running:
                print("Daemon is running")
            else:
                print("Daemon is not running")

    sys.exit(0)
