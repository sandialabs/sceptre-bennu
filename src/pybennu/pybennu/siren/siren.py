#!/usr/bin/python3
import argparse
import atexit
import contextlib
import importlib
import inspect
import inspect
import json
import logging
import os
import signal
import sys
import threading

from pybennu.distributed.client import Client
from pybennu.distributed.subscriber import Subscriber
import pybennu.distributed.swig._Endpoint as E

from time import sleep


LEVELS = {'debug': logging.DEBUG,       # 10
          'info': logging.INFO,         # 20
          'warning': logging.WARNING,   # 30
          'filter': 31,
          'error': logging.ERROR,       # 40
          'critical': logging.CRITICAL  # 50
         }

logging.addLevelName(LEVELS['filter'], 'FILTER')
logging.basicConfig(format='%(asctime)s %(name)-12s  %(levelname)-8s  %(message)s', level=logging.DEBUG, datefmt='%Y-%m-%d %H:%M:%S')

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
        self.level_name = ''
        self.to_endpoint = {}
        self.to_device = {}


class siren_start():
    '''
    Class used to maintain all of the siren device threads.
    '''

    def __init__(self, config, devices, level_name):
        """Inits the siren subscriber."""
        self.setup_logger(level_name)
        self._parse_configs(config, devices)

        self.siren_objs = {}
        for device_name in self.devices:
            self.logger.debug(f'Created siren object {device_name}.')
            siren_obj = siren(self.devices[device_name], self.devices[device_name].level_name)
            self.siren_objs[device_name] = siren_obj
            thread = threading.Thread(target=self.run_siren, args=[siren_obj])
            thread.daemon = True
            thread.start()

        atexit.register(self.cleanup)

    def setup_logger(self, level_name):
        self.logger = logging.getLogger(__class__.__name__)
        level = LEVELS.get(level_name, logging.NOTSET)
        self.logger.setLevel(level)

    '''
    Just an empty loop to keep the main thread running, while each
    individual device thread runs.  Each device thread is a daemon thread.
    This way when Ctrl-C is pressed quitting the main thread the
    individual device threads will be closed.

    If this isn't used and deamon = false, the program exits immediately
    after starting.

    If this isn't used and daemon = true, multiple errors are thrown when
    ctrl-c is hit. The specific errors encountered
    are dependent on timing.
    '''
    def run(self):
        while True:
            sleep(1)

    def cleanup(self):
        for device_name in self.siren_objs:
            self.siren_objs[device_name].cleanup()

    def _parse_configs(self, config, devices):
        with open(config) as g:
            self.config = json.load(g)

        self.devices = {}
        for key in self.config:
            device = device_config(key)
            if devices is None or device.name in devices:
                if 'device_type' in self.config[key]:
                    device.type = self.config[key]['device_type']
                if 'device_id' in self.config[key]:
                    device.id = self.config[key]['device_id']
                if 'device_config_file' in self.config[key]:
                    device.config_file = self.config[key]['device_config_file']
                if 'server-endpoint' in self.config[key]:
                    device.server_endpoint = self.config[key]['server-endpoint']
                if 'publish-endpoint' in self.config[key]:
                    device.publish_endpoint = self.config[key]['publish-endpoint']
                if 'filter' in self.config[key]:
                    device.filter = self.config[key]['filter']
                if 'device_polling_time' in self.config[key]:
                    device.polling_time = self.config[key]['device_polling_time']
                if 'to_endpoint' in self.config[key]:
                    device.to_endpoint = self.config[key]['to_endpoint']
                if 'to_device' in self.config[key]:
                    device.to_device = self.config[key]['to_device']
                if 'level_name' in self.config[key]:
                    device.level_name = self.config[key]['level_name']

                self.devices[key] = device

                self.logger.debug(f'''Configuration
                        Log Level: {device.level_name}
                        Device Type: {device.type}
                        Device Id: {device.id}
                        Device Config File: {device.config_file}
                        Server Endpoint: {device.server_endpoint}
                        Publish Endpoint: {device.publish_endpoint}
                        Filter: {device.filter}
                        Device Polling Time: {device.polling_time}
                        To Endpoint: {device.to_endpoint}
                        To Device: {device.to_device}''')

    def run_siren(self, siren_obj):
        siren_obj.start()


class siren(Subscriber, Client):
    def __init__(self, device_config, level_name):
        self.setup_logger(level_name)
        self.device_config = device_config

        # Create a state table for data being sent back to provider
        # this way only if a filters state has changed will a message be sent to provider
        self.to_endpoint_states = dict.fromkeys(self.device_config.to_endpoint.keys())

        self.run_thread = True

        self.create_device()

        publish_endpoint = E.new_Endpoint()
        server_endpoint = E.new_Endpoint()
        E.Endpoint_str_set(publish_endpoint, self.device_config.publish_endpoint)
        E.Endpoint_str_set(server_endpoint, self.device_config.server_endpoint)
        Subscriber.__init__(self, publish_endpoint)
        Client.__init__(self, server_endpoint)

        # Overwrite the subscriber's subscription handler with siren's handler.
        self.subscription_handler = self._subscription_handler

        self.__lock = threading.Lock()
        self.thread = threading.Thread(target=self._to_provider)
        self.thread.daemon = True

    def setup_logger(self, level_name):
        self.logger = logging.getLogger(__class__.__name__)
        level = LEVELS.get(level_name, logging.NOTSET)
        self.logger.setLevel(level)

    def start(self):
        self.thread.start()
        self.run()

    def cleanup(self):
        self.logger.info(f'Cleaning up {self.device.device_name}.')
        self.run_thread = False
        self.thread.join()
        self.device.cleanup()

    def create_device(self):
        '''
        There should only be one device class the siren.hil_device.* module.  Otherwise
        getmembers(...) will return all the classes in that module and they may
        not necessarily be the device class you want.

        Classes nested in the device class are ok and preferred
        '''
        path = 'pybennu.siren.hil_device.{}'.format(self.device_config.type)
        module = importlib.import_module(path)
        classes = inspect.getmembers(module, inspect.isclass)
        for _class in classes:
            if _class[1].__module__ == path:
                self.device = _class[1](
                    self.device_config.config_file,
                    self.device_config.name,
                    self.device_config.level_name,
                    self.device_config.id)
                break

    def _is_outside_deadband(self, old_value, new_value, deadband):
        """Check if a new value is outside of a deadband threshold from and old value
        Returns true  if the difference between new_value and old_value are 
        greater than the deadband, or if the old_value is None.
        """
        if old_value is None:
            return True

        return abs(new_value - old_value) > deadband

    def _subscription_handler(self, message):
        """Receive Subscription message
        This method gets called by the Subscriber base class in its run()
        method when a message from a publisher is received.

        Ex message: "load-1_bus-101.mw:999.000,load-1_bus-101.active:true,"

        Args:
            message (str): published zmq message as a string
        """
        with self.__lock:
            points = message.split(',')
            points = points[:-1] # remove last element since it might be empty
            for point in points:
                if point == "":
                    continue
                split = point.split(':')
                tag = split[0]
                value = split[1]
                if tag not in self.device_config.to_device:
                    continue
                label = self.device_config.to_device[tag]

                # Here is a default range for the sensor data for the case that the configs do not
                # include the min and max for sensor data. This is desirble to keep as an option.
                #sensor_range = [-100, 100]
                # if filter has a list associated with it instead of just a lebel, then use the second entry in the list
                # as the min value for sensor data and third entry as the max val. The label will be the first entry.
                #if type(label) == type(sensor_range):
                #    sensor_range[0] = label[1]
                #    sensor_range[1] = label[2]
                #    label = label[0]
                # Find an approprate scaled voltage to output
                #voltage = virtualsensor(data, sensor_range[0], sensor_range[1], 0, 3.2)

                if value.lower() == 'false':
                    value = False
                    field = 'status'
                elif value.lower() == 'true':
                    value = True
                    field = 'status'
                else:
                    value = float(value)
                    field = 'value'
                self.device.logger.log(LEVELS['debug'], f'TO HARDWARE {tag}:{value} with field:{field}')
                self._to_hardware(field, label, value)

    def _to_provider(self):
        """Receives data from the outputs of the connected hardware.

        The flow is hardware -> siren -> provider.
        """
        sleep(5)  # before polling the devices sleep so that siren has a chance to get the values from the provider
        filters = self.device_config.to_endpoint.keys()
        while self.run_thread:
            with self.__lock:
                for _filter in filters:
                    label = self.device_config.to_endpoint[_filter]
                    if 'analog' in label:
                        data = self.device.read_analog(label)
                        # set deadband if given in label mapping
                        deadband = self.device.label_mapping[label].get("deadband", 0.0)
                        if data is not None:
                            if (
                                _filter in self.to_endpoint_states
                                and self._is_outside_deadband(self.to_endpoint_states[_filter], data, deadband)
                            ):
                                self.to_endpoint_states[_filter] = data
                                self.device.logger.log(LEVELS['debug'], f'TO PROVIDER {label}:{data}')
                                with contextlib.redirect_stdout(None):
                                    self.write_analog_point(_filter, data)
                    elif 'digital' in label:
                        data = self.device.read_digital(label)
                        if data is not None:
                            if _filter in self.to_endpoint_states and self.to_endpoint_states[_filter] != data:
                                self.to_endpoint_states[_filter] = data
                                self.device.logger.log(LEVELS['debug'], f'TO PROVIDER {label}:{data}')
                                with contextlib.redirect_stdout(None):
                                    self.write_digital_point(_filter, data)
                    elif 'flip_flop' in label:
                        data = self.device.read_flip_flop(label)
                        if data is not None:
                            if _filter in self.to_endpoint_states and self.to_endpoint_states[_filter] != data:
                                self.to_endpoint_states[_filter] = data
                                self.device.logger.log(LEVELS['debug'], f'TO PROVIDER {label}:{data}')
                                with contextlib.redirect_stdout(None):
                                    self.write_digital_point(_filter, data)
                    else:
                        raise NotImplementedError

                    
            sleep(self.device_config.polling_time)



    def _to_hardware(self, field, label, data):
        """Sends data to the inputs of the connected hardware.

        The flow is provider -> siren -> hardware.

        Args:
            field: Either "status" (digital) or "value" (analog).
            label: The IO point to write to.
            data: The value which the IO point will be written to.
        """
        if field == 'status':
            self.device.write_digital(label, data)
        elif field == 'value':
            if 'waveform' in label:
                self.device.write_waveform(label, data)
            else:
                # This could lead to issues because for the LJ labels are either
                # "analog" or "digital", but for the AMS labels are tags.
                self.device.write_analog(label, data)
        else:
            raise NotImplementedError


def main():
    signal.signal(signal.SIGINT, on_exit)
    signal.signal(signal.SIGTERM, on_exit)

    # Setup command line arguments.
    parser = argparse.ArgumentParser()
    parser.add_argument('-c',
                        '--config',
                        required=True,
                        action='store',
                        help='siren config file')
    parser.add_argument('-D',
                        '--devices',
                        nargs='+',
                        required=False,
                        help='space-separated list of expected devices')
    args = parser.parse_args()

    # Start the subscriber.
    ss = siren_start(args.config, args.devices, 'critical')
    ss.run()


def on_exit(signum, stack):
    goodbye = """                             _ _
          __ _  ___   ___   __| | |__  _   _  ___
         / _` |/ _ \ / _ \ / _` | '_ \| | | |/ _ \\
        | (_| | (_) | (_) | (_| | |_) | |_| |  __/
         \__, |\___/ \___/ \__,_|_.__/ \__, |\___|
         |___/                         |___/
                    siren [sahy-ruh n]
              """
    sys.exit(goodbye)


if __name__ == "__main__":
    main()
