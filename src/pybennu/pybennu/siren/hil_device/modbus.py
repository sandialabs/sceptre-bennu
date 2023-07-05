#!/usr/bin/python3
import json
import logging
import sys
import time
import contextlib
import threading
from pymodbus.client.sync import ModbusTcpClient
from pymodbus.payload import BinaryPayloadDecoder
from pymodbus.constants import Endian

from math import atan
from pybennu.siren.hil_device.hil_device import HIL_Device
from pybennu.siren.hil_device.hil_device import MsgType

class modbus(HIL_Device):
    def __init__(self, device_config, device_name, level_name, device_id):
        self.device_name = device_name
        self.setup_logger(level_name)
        self.device_ip = device_id
        self.device_unit = 1
        if ':' in device_id:
            self.device_ip = device_id.split(':')[0]
            self.device_unit = int(device_id.split(':')[1])
        self.read_config(device_config)

        # TODO: Error handling for modbus connection
        self.modbus_client = ModbusTcpClient(self.device_ip)
        self.modbus_client.debug_enabled = False
        logging.getLogger('pymodbus.transaction').setLevel('INFO')
        logging.getLogger('pymodbus.factory').setLevel('INFO')
        logging.getLogger('pymodbus.framer.socket_framer').setLevel('INFO')
        logging.getLogger('pymodbus.client.sync').setLevel('INFO')
        logging.getLogger('pymodbus.payload').setLevel('INFO')
        if self.modbus_client.connect():
            self.logger.debug(f'Connected to Modbus at: IP = {self.device_ip}; unit = {self.device_unit}')
        else:
            self.logger.error(f'Could not connect to Modbus at: IP = {self.device_ip}; unit = {self.device_unit}')
            return

        self.run_thread = True
        self.stream_thread = threading.Thread(target=self.run_heartbeat, args=())
        self.stream_thread.daemon = True
        self.stream_thread.start()

    def __del__(self):
        pass

    def cleanup(self):
        self.modbus_client.close()
        pass

    def read_config(self, device_config):
        with open(device_config) as config_file:
            self.config = json.load(config_file)
        
        if "heartbeat" in self.config:
            self.heartbeat_interval = self.config["heartbeat"]["interval"]
            self.heartbeat_register = self.config["heartbeat"]["register"]
    
    def run_heartbeat(self):
        if self.heartbeat_interval > 0:
            heartbeat_value = 0
            while True:
                time.sleep(self.heartbeat_interval)
                self.write_digital("heartbeat", heartbeat_value)
                self.logger.debug(f'Modbus Heartbeat {heartbeat_value}')
                heartbeat_value = 1 - heartbeat_value #toggle between 1 and 0

    def config_to_analog(self, *args):
        raise NotImplementedError

    def config_to_digital(self, *args):
        raise NotImplementedError

    def read_analog(self, io_point):
        result = None
        if io_point in self.config:
            register = self.config[io_point]["register"]
            scale = 1
            if 'scale' in self.config[io_point]:
                scale = self.config[io_point]['scale']
            modbus_result = self.modbus_client.read_holding_registers(register, 2, unit=self.device_unit)
            try:
                #int_list = []
                #for register in modbus_result.registers:
                #    int_list.append(int(register))

                binary_decoder = BinaryPayloadDecoder.fromRegisters(modbus_result.registers, byteorder=Endian.Big, wordorder=Endian.Auto)
                unscaled = binary_decoder.decode_32bit_float()
                result = scale * unscaled
                self.logger.debug(f'Read Modbus Analog: {register} = {unscaled} -> {result}')
            except:
                self.logger.error(f"Failed to read holding register {register}")
                result = None
        else:
            self.logger.error(f'{io_point} does not exist in the modbus configuration')

        return result

    def write_analog(self, io_point, volts):
        raise NotImplementedError

    def read_digital(self, io_point):
        raise NotImplementedError

    def write_digital(self, io_point, volts):
        register = self.config[io_point]["register"]
        modbus_result = self.modbus_client.write_coil(register, volts, unit=self.device_unit)
        self.logger.debug(f'Write Coil: {register} -> {volts}; Result {modbus_result}')

    def write_waveform(self, io_point, freq):
        raise NotImplementedError
