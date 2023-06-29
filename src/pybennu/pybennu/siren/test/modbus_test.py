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
from pybennu.siren.hil_device.BcdPayloadDecoder import BcdPayloadDecoder
from pybennu.siren.hil_device.BcdPayloadEncoder import BcdPayloadEncoder
from struct import pack

from math import atan
from pybennu.siren.hil_device.hil_device import HIL_Device
from pybennu.siren.hil_device.hil_device import MsgType

'''
agc: Fan Power
active: apply load
freq: load setpoint

mw: Watts
'''

'''
from pymodbus.client.sync import ModbusTcpClient
import time
 
SEL = '10.113.5.110'
 
client = ModbusTcpClient(SEL)
client.connect()
state = False
while not state:
    print("Tripping the SEL-351S relay")
    client.write_coil(48,1)
    time.sleep(1)
    result = client.read_coils(48,1)
    if hasattr(result,'bits'):
        state = result.bits[0]
        if state:
            print("SEL-351S relay was tripped")
client.close()

'''

class modbus(HIL_Device):
    def __init__(self, device_config, device_name, level_name, device_id):
        self.device_name = device_name
        self.setup_logger(level_name)
        self.device_ip = device_id
        self.device_unit = 1
        if ':' in device_id:
            self.device_ip = device_id.split(':')[0]
            self.device_unit = int(device_id.split(':')[1])
            print(f'IP = {self.device_ip}; unit = {self.device_unit}')
        self.read_config(device_config)

        # TODO: Error handling for modbus connection
        self.modbus_client = ModbusTcpClient(self.device_ip)
        self.modbus_client.debug_enabled = False
        logging.getLogger('pymodbus.transaction').setLevel('INFO')
        logging.getLogger('pymodbus.factory').setLevel('INFO')
        logging.getLogger('pymodbus.framer.socket_framer').setLevel('INFO')
        logging.getLogger('pymodbus.client.sync').setLevel('INFO')
        logging.getLogger('pymodbus.payload').setLevel('INFO')
        self.modbus_client.connect() # return true if successful, false otherwise
        
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
                heartbeat_value = 1 - heartbeat_value #toggle between 1 and 0

    def config_to_analog(self, *args):
        raise NotImplementedError

    def config_to_digital(self, *args):
        raise NotImplementedError

    def read_analog(self, io_point):
        #range_start = 1000
        #range_end = 9000
        #registers = [2625,2689,2691,2693,2695,2697,2699,2945,2947,2949,2951,2953,2955,2657,2659]
        registers = [2696,2697,2698]
        #registers = [2697]
        print(f'Start: {registers}')
        #for i in range(range_start,range_end):
        for i in registers:
            holding_register = self.modbus_client.read_holding_registers(i, 2, unit=self.device_unit)
            # 480.5 in 32 bit float, https://www.binaryconvert.com/convert_float.html
            # First number is the 2 left most bytes, the second number is the 2 right most bytes
            #int_list = [17392, 16384]
            
            #int_list1 = [b'\x01',b'\xE0']
            try:
                
                int_list = []
                for register in holding_register.registers:
                    int_list.append(int(register))
                

                binary_decoder = BinaryPayloadDecoder.fromRegisters(int_list, byteorder=Endian.Big, wordorder=Endian.Auto)
                float_result = binary_decoder.decode_32bit_float()

                if i in [2625,2697,2699]:
                    #int_list = [0, 8192]
                    binary_decoder = BinaryPayloadDecoder.fromRegisters(holding_register.registers, byteorder=Endian.Big, wordorder=Endian.Auto)
                    ## Option 1 ##
                    bcd_decoder = BcdPayloadDecoder(binary_decoder._payload)
                    bcd_result1 = bcd_decoder.decode_int(4)

                    ## Option 2 ##
                    binary_bcd_decoder = BcdPayloadDecoder.fromRegisters(int_list, Endian.Big)
                    bcd_decoder = BcdPayloadDecoder(binary_bcd_decoder._payload)
                    bcd_result2 = bcd_decoder.decode_int(4)

                    '''
                    ## Option 3 ##
                    bcd_result3 = None
                    if isinstance(registers, list): # repack into flat binary
                        payload = b''.join(pack('>H', x) for x in registers)
                        bcd_decoder = BcdPayloadDecoder(payload)
                        bcd_result3 = bcd_decoder.decode_int(4)

                    ## Option 4 ##
                    bcd_result4 = None
                    if isinstance(registers, list): # repack into flat binary
                        payload = b''.join(pack('!H', x) for x in registers)
                        bcd_decoder = BcdPayloadDecoder(payload)
                        bcd_result4 = bcd_decoder.decode_int(4)
                    '''

                    #print(f'{i}(2 register): BCD1 = {bcd_result1}; BCD2 = {bcd_result2}; BCD3 = {bcd_result3}; BCD4 = {bcd_result4}')
                    print(f'{i}(2 register): BCD1 = {bcd_result1}; BCD2 = {bcd_result2}; List = {holding_register.registers}')
                else:
                    print(f'{i}(2 register): payload_float={float_result}')
            except Exception as e:
                print(f"ERROR {e}")
                self.logger.error("Failed to read holding register")
                result = None
        print('End')

        return None

    def write_analog(self, io_point, volts):
        return
        bcd_encode = BcdPayloadEncoder()
        bcd_encode.add_number(152)
        payload = bcd_encode.build()
        print(f'Write Payload: {payload}')

        register = 2625 #self.config[io_point]["register"]
        value = payload #volts

        '''
        if 'bcd' in self.config[io_point] and self.config[io_point]['bcd']:
            value = self.convert_to_bcd(int(volts))
            print(f'*****convert to bcd**** {volts} -> {value}')
        
        modbus_result = self.modbus_client.write_registers(register, int(value), unit=self.device_unit)
        '''
        #Abate modbus_result = self.modbus_client.write_registers(register, payload, unit=self.device_unit)
        #Abate print(f'Write register result = {modbus_result}')
        print(f'Write Register: IO_Point - {io_point}, Register - {register}, Value - {value}')

    def read_digital(self, io_point):
        raise NotImplementedError

    def write_digital(self, io_point, volts):
        register = self.config[io_point]["register"]
        #modbus_result = self.modbus_client.write_coil(register, volts, unit=self.device_unit)
        #print(f'Write coil result = {modbus_result}')
        #print(f'Write Coil: IO_Point - {io_point}, Register - {register}, Value - {volts}')

    def write_waveform(self, io_point, freq):
        raise NotImplementedError

    '''
    def convert_to_bcd(self, decimal):
        """ Converts a decimal value to a bcd value
        :param value: The decimal value to to pack into bcd
        :returns: The number in bcd form
        """
        place, bcd = 0, 0
        while decimal > 0:
            nibble = decimal % 10
            bcd += int(nibble) << int(place)
            decimal /= 10
            place += 4
        return bcd
    
    def convert_from_bcd(self, bcd):
        """ Converts a bcd value to a decimal value
        :param value: The value to unpack from bcd
        :returns: The number in decimal form
        """
        place, decimal = 1, 0
        while bcd > 0:
            nibble = bcd & 0xf
            decimal += nibble * int(place)
            bcd >>= 4
            place *= 10
        return decimal
    '''
