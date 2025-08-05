"""
SCEPTRE Provider for Siren

## Overview

Data is read from and written to predefined hardware (defined in the siren/hil_device directory)
and published by this siren provider.

"""

import contextlib
import importlib
import inspect
import json
import logging
import math
import threading
import time
from collections import deque

from pybennu.distributed.provider import Provider
from pybennu.settings import PybennuSettings

LEVELS = {
    "debug": logging.DEBUG,  # 10
    "info": logging.INFO,  # 20
    "warning": logging.WARNING,  # 30
    "filter": 31,
    "error": logging.ERROR,  # 40
    "critical": logging.CRITICAL,  # 50
}

logging.addLevelName(LEVELS["filter"], "FILTER")
logging.basicConfig(
    format="%(asctime)s %(name)-12s  %(levelname)-8s  %(message)s",
    level=logging.DEBUG,
    datefmt="%Y-%m-%d %H:%M:%S",
)


class CircularBuffer:
    def __init__(self, max_size):
        self.max = max_size
        self.cur_size = 0
        self.buffer = deque()
        self.sum = 0

    def add(self, value):
        if len(self.buffer) == self.max:
            self.sum -= self.buffer.popleft()
        self.buffer.append(value)
        self.sum += value
        self.cur_size = len(self.buffer)


class device_config:
    def __init__(self, _name):
        self.name = _name
        self.polling_time = 0.5
        self.type = ""
        self.id = ""
        self.config_file = ""
        self.server_endpoint = "tcp://127.0.0.1:5555"
        self.publish_endpoint = "udp://239.0.0.1:40000"
        self.filter = "/"
        self.level_name = ""
        self.to_endpoint = {}
        self.to_device = {}


class Siren(Provider):
    def __init__(self, server_endpoint, publish_endpoint, settings):
        """
        Args:
            server_endpoint: IP address for the provider server to listen on
            publish_endpoint: IP address for the provider to publish values to
            settings: (PybennuSettings) The settings found in the siren_config.yaml
                      file generated from the scenario yaml
        """
        super().__init__(server_endpoint, publish_endpoint)
        self.conf: PybennuSettings = settings
        self.setup_logger()
        siren_config = self.conf.siren.siren_json
        passed_devices = None
        self.setup_siren_config(
            siren_config, passed_devices
        )  # Populates self.device_configs (dict)

        self.devices = {}
        self.to_endpoint_states = (
            {}
        )  # Holds hardware values before written to internal_state
        self.to_hardware_threads = []
        self.from_hardware_threads = []

        for device_name, dev_config in self.device_configs.items():
            self.devices[device_name] = self.create_device(dev_config)
            self.to_endpoint_states[device_name] = dict.fromkeys(
                dev_config.to_endpoint.keys()
            )

            # Writes values to hardware from internal state.
            # Replaces _subscription_handler functionality
            if dev_config.to_device.keys():
                thread = threading.Thread(
                    target=self.write_to_hardware, args=(device_name,)
                )
                thread.daemon = True
                self.to_hardware_threads.append(thread)

            # Writes value to internal state from hardware
            # Replaces _to_provider functionality
            if dev_config.to_endpoint.keys():
                thread = threading.Thread(
                    target=self.update_from_hardware, args=(device_name,)
                )
                thread.daemon = True
                self.from_hardware_threads.append(thread)

        self.run_thread = True
        self.state_lock = threading.Lock()
        self.set_internal_state()

    def set_internal_state(self):
        """
        Create and Initialize the internal state dictionary of the Provider.

        This self.internal_state variable should be only be accessed with the self.state_lock
        """
        self.internal_state = {}
        self.ac_values = {}
        self.rms_tags = set()
        for tag in self.conf.siren.tags:
            self.internal_state[tag.name] = str(tag.initial_value)
            if tag.publish_rms:
                self.rms_tags.add(tag.name)
                self.ac_values[tag.name] = CircularBuffer(
                    self.conf.siren.rms_buffer_size
                )

    def setup_siren_config(self, config, devices):
        """
        Create device config dictionaries for each device in the siren.json

        Args:
            config: (str) Path to the siren.json config file
            devices: Specific devices to create
        """
        with open(config, "r") as f:
            self.siren_config = json.load(f)

        self.device_configs = {}
        for key, conf in self.siren_config.items():
            device = device_config(key)
            if devices is None or device.name in devices:
                if "device_type" in conf:
                    device.type = conf["device_type"]
                if "device_id" in conf:
                    device.id = conf["device_id"]
                if "device_config_file" in conf:
                    device.config_file = conf["device_config_file"]
                if "server-endpoint" in conf:
                    device.server_endpoint = conf["server-endpoint"]
                if "publish-endpoint" in conf:
                    device.publish_endpoint = conf["publish-endpoint"]
                if "filter" in conf:
                    device.filter = conf["filter"]
                if "device_polling_time" in conf:
                    device.polling_time = conf["device_polling_time"]
                if "to_endpoint" in conf:
                    device.to_endpoint = conf["to_endpoint"]
                if "to_device" in conf:
                    device.to_device = conf["to_device"]
                if "level_name" in conf:
                    device.level_name = conf["level_name"]

                self.device_configs[key] = device
                self.logger.debug(
                    f"""Configuration
                        Log Level: {device.level_name}
                        Device Type: {device.type}
                        Device Id: {device.id}
                        Device Config File: {device.config_file}
                        Server Endpoint: {device.server_endpoint}
                        Publish Endpoint: {device.publish_endpoint}
                        Filter: {device.filter}
                        Device Polling Time: {device.polling_time}
                        To Endpoint: {device.to_endpoint}
                        To Device: {device.to_device}"""
                )

    def setup_logger(self):
        """
        Creates the logger object at the right level
        """
        self.logger = logging.getLogger(__class__.__name__)
        if self.conf.debug:
            level = logging.DEBUG
        else:
            level = logging.INFO
        self.logger.setLevel(level)

    def run(self) -> None:
        """
        Start & Run the Siren Provider.

        Called by the ServerDaemon object (power-daemon.py)
        """
        for thread in self.from_hardware_threads:
            thread.start()
        for thread in self.to_hardware_threads:
            thread.start()
        super().run()

    def cleanup(self):
        self.logger.info(f"Cleaning up {self.device.device_name}.")
        self.run_thread = False
        for thread in self.from_hardware_threads:
            thread.join()
        for thread in self.to_hardware_threads:
            thread.join()
        self.device.cleanup()

    def create_device(self, device_config):
        """
        There should only be one device class the siren.hil_device.* module.  Otherwise
        getmembers(...) will return all the classes in that module and they may
        not necessarily be the device class you want.

        Classes nested in the device class are ok and preferred
        """
        path = "pybennu.siren.hil_device.{}".format(device_config.type)
        module = importlib.import_module(path)
        classes = inspect.getmembers(module, inspect.isclass)
        for _class in classes:
            if _class[1].__module__ == path:
                device = _class[1](
                    device_config.config_file,
                    device_config.name,
                    device_config.level_name,
                    device_config.id,
                )
                return device

    def _is_outside_deadband(self, old_value, new_value, deadband):
        """Check if a new value is outside of a deadband threshold from and old value
        Returns true  if the difference between new_value and old_value are
        greater than the deadband, or if the old_value is None.
        """
        if old_value is None:
            return True
        return abs(new_value - old_value) > deadband

    def write_analog_to_internal_state(self, tag, value):
        """
        Update an analog tag's value in the Provider's internal state.

        Args:
            tag: (str) The name of the tag to update
            value: (int/float) The value that the tag should be updated to
        """
        if tag in self.internal_state:
            if tag in self.rms_tags:
                value = self._ac_to_dc(tag, value)
            with self.state_lock:
                self.internal_state[tag] = str(value)

    def write_digital_to_internal_state(self, tag, value):
        """
        Update a digital tag's value in the Provider's internal state.

        Args:
            tag: (str) The name of the tag to update
            value: assumed to be bool
        """
        if tag in self.internal_state:
            with self.state_lock:
                self.internal_state[tag] = value

    def write_to_hardware(self, device_name):
        """
        Writes physical data (e.g. analog) to the connected hardware based on values in the internal state (IS).

        The flow is [Client] -> SirenProvider -> Hardware

        Args:
            device_name: (str) The name of the hardware device
        """
        while self.run_thread:
            with self.state_lock:
                for tag, label in self.device_configs[device_name].to_device.items():
                    value = self.internal_state[tag]
                    if value.lower() == "false":
                        value = False
                        field = "status"
                    elif value.lower() == "true":
                        value = True
                        field = "status"
                    else:
                        value = float(value)
                        field = "value"
                    self.devices[device_name].logger.log(
                        LEVELS["debug"], f"TO HARDWARE {tag}:{value} with field:{field}"
                    )
                    self._to_hardware(device_name, field, label, value)
            time.sleep(1)

    def update_from_hardware(self, dev):
        """
        Receives data from the outputs of the connected hardware and updates internal state (IS).

        The flow is Hardware -> SirenProvider -> Multi-cast

        Args:
            dev: (str) The name of the hardware device
        """
        time.sleep(
            5
        )  # before polling the devices sleep so that siren has a chance to get the values from the provider
        filters = self.device_configs[dev].to_endpoint.keys()
        while self.run_thread:

            for _filter in filters:
                label = self.device_configs[dev].to_endpoint[_filter]
                if "analog" in label:
                    data = self.devices[dev].read_analog(label)
                    # set deadband if given in label mapping
                    deadband = (
                        self.devices[dev].label_mapping[label].get("deadband", 0.0)
                    )
                    if data is not None:
                        if _filter in self.to_endpoint_states[
                            dev
                        ] and self._is_outside_deadband(
                            self.to_endpoint_states[dev][_filter], data, deadband
                        ):
                            self.to_endpoint_states[dev][_filter] = data
                            self.devices[dev].logger.log(
                                LEVELS["debug"],
                                f"FROM HARDWARE TO INTERNAL STATE: {label}:{data}",
                            )
                            with contextlib.redirect_stdout(None):
                                self.write_analog_to_internal_state(_filter, data)

                elif "digital" in label:
                    data = self.devices[dev].read_digital(label)
                    if data is not None:
                        if (
                            _filter in self.to_endpoint_states[dev]
                            and self.to_endpoint_states[dev][_filter] != data
                        ):
                            self.to_endpoint_states[dev][_filter] = data
                            self.devices[dev].logger.log(
                                LEVELS["debug"],
                                f"FROM HARDWARE TO INTERNAL STATE {label}:{data}",
                            )
                            with contextlib.redirect_stdout(None):
                                self.write_digital_to_internal_state(_filter, data)

                elif "flip_flop" in label:
                    data = self.devices[dev].read_flip_flop(label)
                    if data is not None:
                        if (
                            _filter in self.to_endpoint_states[dev]
                            and self.to_endpoint_states[dev][_filter] != data
                        ):
                            self.to_endpoint_states[dev][_filter] = data
                            self.devices[dev].logger.log(
                                LEVELS["debug"],
                                f"FROM HARDWARE TO INTERNAL STATE {label}:{data}",
                            )
                            with contextlib.redirect_stdout(None):
                                self.write_digital_to_internal_state(_filter, data)

                else:
                    raise NotImplementedError

            time.sleep(self.device_configs[dev].polling_time)

    def periodic_publish(self) -> None:
        """
        Publishes the values saved in the internal state periodically.
        """
        self.logger.info(f"Periodic Publish @ {self.conf.siren.publish_rate} seconds")
        rate = self.conf.siren.publish_rate
        while True:
            with self.state_lock:
                tags = []
                for key, val in self.internal_state.items():
                    tags.append(f"{key}:{str(val).lower()}")
                msg = f"Write={{{','.join(tags)}}}"
                self.logger.debug(msg)
                self.publish(msg)

            time.sleep(rate)

    def query(self) -> str:
        """
        Returns a string containing all the internal state keys (tags).
        """
        with self.state_lock:
            msg = f"ACK={','.join(self.internal_state.keys())}"
        return msg

    def read(self, tag: str) -> str:
        """
        Returns the value of a tag from the internal state.

        Args:
            tag: (str) The name of the tag
        """
        if tag not in self.internal_state:
            msg = "ERR=Unknown Tag"
            self.log.critical(f"{msg} (tag: {tag})")
            return msg
        msg = f"ACK={self.internal_state[tag]}"
        return msg

    def write(self, tags: dict) -> str:
        """
        Writes the value(s) of {tag:value} pairs from a client into internal state.

        Args:
            tags: (dict) Dict of {tag:value} pairs to update internal state with.
        """

        for tag in tags.keys():
            if tag not in self.internal_state.keys():
                msg = f"ERR: Unknown tag '{tag}'"
                self.log.error(f"{msg} (tags being written {tags})")
                return msg

        with self.state_lock:
            self.internal_state.update(tags)

        msg = "ACK=Wrote Tags"
        return msg

    def _to_hardware(self, device_name, field, label, data):
        """Sends data to the inputs of the connected hardware.

        The flow is [Client] -> SirenProvider -> Hardware.

        Args:
            device_name: (str) The name of the hardware device to send to
            field: Either "status" (digital) or "value" (analog).
            label: The IO point to write to.
            data: The value which the IO point will be written to.
        """
        if field == "status":
            self.devices[device_name].write_digital(label, data)
        elif field == "value":
            if "waveform" in label:
                self.devices[device_name].write_waveform(label, data)
            else:
                # This could lead to issues because for the LJ labels are either
                # "analog" or "digital", but for the AMS labels are tags.
                self.devices[device_name].write_analog(label, data)
        else:
            raise NotImplementedError

    def _ac_to_dc(self, tag, value):
        """
        Get the RMS value for a tag given a new value.

        Args:
            tag: (str) The name the tag. Assumed to be a valid tag in the self.rms_tags set
            value: (float/int) The new AC reading to be taken into account for an updated RMS
        """
        self.ac_values[tag].add(pow(value, 2))
        return self._calc_rms(
            self.ac_values[tag].sum,
            self.ac_values[tag].cur_size,
        )

    def _calc_rms(self, sum_squares, count):
        """
        Calculate and return the RMS for a sum of AC values squared.

        Args:
            sum_squares: (float) The sum of each AC reading squared. E.g. (2^2 + 3^2 + 1^2)
            count: (int) The number of AC readings entailed in the sum_squares argument
        """
        return math.sqrt(sum_squares / count)
