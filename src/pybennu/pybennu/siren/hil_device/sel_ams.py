#!/usr/bin/python3
import json
import logging
import re
import serial
import sys
import threading
import time

from math import atan
from pybennu.siren.hil_device.hil_device import HIL_Device
from pybennu.siren.hil_device.hil_device import MsgType


class SELAMS(HIL_Device):
    DEFAULT_STATE = '''NT
    FS01
    SN01
    TTFFFFFFFE
    AS10000000000003C
    AS20000000009603C
    AS30000000004B03C
    AS40000000000003C
    AS50000000009603C
    AS60000000004B03C
    AS70000000000003C
    AS80000000000003C
    AS90000000000003C
    ASA0000000000003C
    ASB0000000000003C
    ASC0000000000003C
    MC1
    RS048D
    RI0000
    OC000
    FI100
    IT100
    FI200
    IT200
    FI300
    IT300
    FI400
    IT400
    FI500
    IT500
    FI600
    IT600
    NS0101010101010101'''

    AMS_COMMAND_PREAMBLE = 'AN' # Preamble to each message
    AMS_OUTPUT_PREAMBLE = 'NO01' # Preamble to each output message
    AMS_VOLTAGE_CONSTANT = 8098421
    AMS_CURRENT_CONSTANT = 4049210
    # PULSE = False

    def __init__(self, device_config, device_name, level_name, device_id):
        self.device_name = device_name
        self.setup_logger(level_name)
        self.device_id = device_id
        self.read_config(device_config)
        self.flip_flop_state = False

        # Create a dictionary that will be used to keep track of the
        # state of each tag as it comes in over serial
        # key = input ID
        # value = tag state
        self.input_map = dict.fromkeys(self.inputs, False)

        self.ser = serial.Serial(device_id)
        self.ser.baudrate = self.serial_baudrate
        self.ser.timeout = self.serial_timeout

        # Configuration option for connecting to the AMS
        # This is useful when debugging and there is no AMS currently connected
        if self.connect_to_ams:
            if not self.ser.is_open:
                self.ser.open()
            self.start_ams()

        self.lock = threading.RLock()

        '''
        Create a new thread that will read the data being sent by the AMS.  The thread
        reads the serial data from the AMS and then stores it in a dictionary.  Then
        when Siren 'polls' to get the current state of an active tag, it will retrieve
        the data from that map.
        '''
        self.run_thread = True
        self.stream_thread = threading.Thread(target=self.get_streaming_data, args=())
        self.stream_thread.daemon = True
        self.stream_thread.start()

    def __del__(self):
        pass

    def cleanup(self):
        self.logger.info('STOPPING TEST')
        if self.connect_to_ams:
            self.ser.write('AT\r\n'.encode())
        self.logger.info('STOPPED!')

    def read_config(self, device_config):
        with open(device_config) as config_file:
            self.config = json.load(config_file)

            #Global config options
            self.connect_to_ams = True
            self.serial_baudrate = 9600
            self.serial_timeout = 1
            self.inputs = []
            self.outputs = {}
            self.max_output_current = 1.5
            self.max_input_current = 500

            if "config" in self.config:
                config = self.config["config"]
                if "connect_to_ams" in config:
                    self.connect_to_ams = config["connect_to_ams"]
                if "serial_baudrate" in config:
                    self.serial_baudrate = config["serial_baudrate"]
                if "serial_timeout" in config:
                    self.serial_timeout = config["serial_timeout"]
                if "inputs" in config:
                    self.inputs = config["inputs"]
                if "outputs" in config:
                    self.outputs = config["outputs"]
                if "max_output_current" in config:
                    self.max_output_current = config["max_output_current"]
                if "max_input_current" in config:
                    self.max_input_current = config["max_input_current"]

        # self.validate_outputs()
        # Build dictionaries of buses and branches that will be used
        # to write values to the SEL AMS
        self.build_bus_branch_dictionary()

        self.logger.debug(f'''Configuration
                              Port: {self.device_id}
                              Baudrate: {self.serial_baudrate}
                              Timeout: {self.serial_timeout}
                              Connect to AMS: {self.connect_to_ams}
                              Inputs: {self.inputs}
                              Output Buses: {self.buses}
                              Ouput Branches: {self.branches}''')

    def validate_outputs(self):
        '''
        Validate the output tags and their associated list of attributes

        Each tag should have 2 attributes
            1) The channel
            2) The scaling factor
        '''
        has_errors = False
        for output, output_list in self.outputs.items():
            if len(output_list) != 2:
                self.logger.error(f'{output} does not have the correct values. Each output requires a list of 2 entries [channel, scaling factor]')
                has_errors = True

            # Ensure each channel is in range.
            for ichannel in output_list[0]:
                if not(ichannel >= 0 and ichannel < 12):
                    self.logger.error(f'{output_list[0]} channel must be between 0 and 12 inclusive')
                    has_errors = True

        if has_errors:
            sys.exit()

    def build_bus_branch_dictionary(self):
        '''
        TODO: Need to rename this function and update the comment since its now
        parsing loads and generators

        Build 2 dictionaries, one to hold the buses and another to hold the branches.

        These will be used when sending data to the AMS, because Siren needs to keep track
        of the bus\branch attributes and only send them to the AMS when all of them have been
        received
        '''
        self.buses, self.generators, self.loads, self.branches = {}, {}, {}, {}
        for device_id in self.outputs:
            if 'GENERATOR' in device_id.upper() and device_id not in self.generators:
                channels = {}
                scales = {}
                for key in self.outputs[device_id]:
                    channels[key] = self.outputs[device_id][key][0]
                    scales[key] = self.outputs[device_id][key][1]
                self.generators[device_id] = SELAMS.Generator(device_id, channels, scales)
                self.logger.debug(f'Added {device_id} to generator dictionary.')
            elif 'LOAD' in device_id.upper() and device_id not in self.loads:
                channels = {}
                scales = {}
                for key in self.outputs[device_id]:
                    channels[key] = self.outputs[device_id][key][0]
                    scales[key] = self.outputs[device_id][key][1]
                self.loads[device_id] = SELAMS.Load(device_id, channels, scales)
                self.logger.debug(f'Added {device_id} to load dictionary.')
            elif 'BRANCH' in device_id.upper() and device_id not in self.branches:
                channel = self.outputs[device_id][0]
                scale = self.outputs[device_id][1]
                self.branches[device_id] = SELAMS.Branch(device_id, channel, scale)
                if not self.branches[device_id].extract_bus_ids():
                    self.logger.error(f'{device_id} is not of the correct format: branch-N_bus-NNNN-NNNN.')
                self.logger.debug(f'Added {device_id} to branch dictionary.')
            elif 'BUS' in device_id.upper() and device_id not in self.buses:
                channel = self.outputs[device_id][0]
                scale = self.outputs[device_id][1]
                self.buses[device_id] = SELAMS.Bus(device_id, channel, scale)
                self.logger.debug(f'Added {device_id} to bus dictionary')
            else:
                raise NotImplementedError

    def parse_tag(self, tag):
        results = re.search(r"/.*/(.*)/(.*)/", tag)
        if results is None or len(results.groups()) != 2:
            self.logger.error(f'{tag} is not of the correct format: /{{provider}}/{{device id}}/{{field}}/')
            return None
        return results.group(1), results.group(2)

    def start_ams(self):
        self.logger.info('STARTING NEW TEST.  Setting default state...')

        if self.connect_to_ams:
            self.ser.write('AT\r\n'.encode())

            for line in SELAMS.DEFAULT_STATE.split():
                self.ser.write((line + '\r\n').encode())

            self.ser.write('ET\r\n'.encode())

    def get_streaming_data(self):
        self.logger.debug('Get SEL AMS streaming data')

        self.fallen = []
        data = ''
        reading = False

        while self.run_thread:
            time.sleep(0.5)

            try:
                with self.lock:
                    if self.connect_to_ams:
                        data += self.ser.read(1).decode()

                        # Error handling. Temporary solution. :/
                        errors = ('ER01', 'ER02', 'ER05', 'ER08')
                        errors = [error for error in errors if (error in data)]
                        if errors:
                            self.logger.error(f'Encountered error(s) {errors}.')
                            if 'ER05' in errors:
                                self.logger.critical(f'Error ER05 caused AMS restart.')
                                self.cleanup()
                                self.start_ams()
                                data = ''
                            if 'ER08' in errors:
                                self.logger.critical(f'Error ER08 caused AMS weirdness. AMS is restarting.')
                                self.cleanup()
                                self.start_ams()
                                data = ''

                        num = self.ser.inWaiting()
                        self.logger.debug(f'Read streaming data: num->{num}, data->\'{data}\'')

                # Used for debugging when not connected to an AMS
                if not self.connect_to_ams:
                    num = 1
                    data = 'C111111222222222A1\r\n\data'

                if num and not reading:
                    reading = True
                    with self.lock:
                        if self.connect_to_ams:
                            data += self.ser.read(num).decode()
                            self.logger.debug(f'Read streaming data: data->\'{data[:-1]}')

                line = ''
                if data:
                    try:
                        line, data = data.split('\r\n', 1)
                        reading = False
                    except ValueError:
                        self.logger.warning(f'ValueError occured while splitting the serial data input {data}.')

                    if line:
                        status = self.parse_streaming_line(line)

                        self.update_input_map(status)

            except Exception as ex:
                self.logger.error(ex)

    def parse_streaming_line(self, line):
        '''
        Parse the data sent from the AMS into its components
        '''
        status = {'Type': 'Unknown', 'Raw Message': line}

        if line.startswith('TA') and len(line) == 2:
            self.logger.debug(f'Receieved \'Test Abort\' message from AMS, {line}')
            status['Type'] = 'Test Aborted'
        elif line.startswith('TC') and len(line) == 2:
            self.logger.debug(f'Receieved \'Test Completed\' message from AMS, {line}')
            status['Type'] = 'Test Completed'
        elif line[0] == 'T' and len(line) == 15:
            self.logger.debug(f'Receieved \'Current State\' message from AMS, {line}')
            status['Type'] = 'Current State'
            status['Test Execution Time (ms)'] = (int(line[1:9], 16)) / 10.0
            status['Inputs'] = bin(int(line[10:12], 16))[2:].zfill(6)[::-1]
            status['State'] = int(line[13:14], 16)
        elif line[0] == 'C' and len(line) == 18:
            self.logger.debug(f'Receieved \'State Change\' message from AMS, {line}')
            status['Type'] = 'State Change'
            status['Previous State'] = int(line[1:3], 16)
            status['Next State'] = int(line[4:6], 16)
            status['Duration of Previous State (ms)'] = (int(line[7:15], 16)) / 10.0
            status['Inputs'] = bin(int(line[16:17], 16))[2:].zfill(6)[::-1]

        return status

    def update_input_map(self, status):
        '''
        Based on the data read in from the AMS serial connection update the internal dictionary.
        '''
        if status['Type'] == 'Current State' or status['Type'] == 'State Change':
            if status['Inputs']:
                for i in range(len(status['Inputs'])):
                    if i < len(self.inputs):
                        key = self.inputs[i]
                        if key in self.input_map and self.input_map[key] != 'None':
                            state = int(status['Inputs'][i])
                            self.logger.debug(f'Input ({key}) => {state}')
                            if state == 1:
                                with self.lock:
                                    self.input_map[key] = False
                            else:
                                with self.lock:
                                    self.input_map[key] = True

                            '''
                            if state == 1 and i not in self.fallen:
                                self.fallen.append(i)
                                with self.lock:
                                    self.input_map[key] = False

                                self.logger.debug(f'Adding {key} to list of fallen devices')
                            elif state == 0 and i in self.fallen:
                                self.fallen.remove(i)
                                ''/'
                                The below hack was copied from the old version of AMS Reader and
                                kept just in case it is needed, but the code needs to be fixed to
                                work in this version
                                ''/'
                                # HACK: Checking specifically for some branches to
                                # prevent the reader from automatically reconnecting the
                                # them. Need to figure out a better way to handle relay
                                # interlocks!
                                #if device in ('branch-1_8162-2026', 'branch-1_8167-8163', 'branch-1_8165-8164'):
                                #    continue
                                with self.lock:
                                    self.input_map[key] = True

                                self.logger.debug(f'Removing {key} from list of fallen devices')
                            '''
                #print('Status Inputs: {}'.format(status['Inputs']))
                #print('Self Inputs: {}'.format(self.inputs))
                #print('Input Map: {}'.format(self.input_map))


    def config_to_analog(self, *args):
        raise NotImplementedError

    def config_to_digital(self, *args):
        raise NotImplementedError

    def read_analog(self, io_point):
        raise NotImplementedError

    def write_analog(self, io_point, volts):
        device_id, field = self.parse_tag(io_point)
        if device_id is not None and field is not None:
            if 'GENERATOR' in device_id.upper() and device_id in self.generators:
                generator = self.generators[device_id]
                generator.values[field] = volts
                amplitude = volts
                if field.upper() == 'VOLTAGE' and field in generator.channels and field in generator.scales:
                    amplitude = volts * generator.scales[field] * self.AMS_VOLTAGE_CONSTANT * generator.values['base_kv']
                    if not (0 <= amplitude and amplitude < 0xC7FCBB657):
                        self.logger.error('Voltage amplitude must be [0, 529.50]. Clipping!')
                        amplitude = 0xC7FCBB657
                    self.write_analog_helper(generator, generator.channels[field], amplitude)
                if field.upper() == 'CURRENT' and field in generator.channels and field in generator.scales:
                    volts = self.scale_current(volts, device_id)
                    amplitude = volts * generator.scales[field] * self.AMS_CURRENT_CONSTANT
                    if not(0 <= amplitude and amplitude < 0x17FCBB657):
                        self.logger.error('Current amplitude must be [0, 264.75]. Clipping!')
                        amplitude = 0x17FCBB657
                    self.write_analog_helper(generator, generator.channels[field], amplitude)
            elif 'LOAD' in device_id.upper() and device_id in self.loads:
                load = self.loads[device_id]
                load.values[field] = volts
                amplitude = volts
                if field.upper() == 'VOLTAGE' and field in load.channels and field in load.scales:
                    amplitude = volts * load.scales[field] * self.AMS_VOLTAGE_CONSTANT
                    self.write_analog_helper(load, load.channels[field], amplitude)
                elif field.upper() == 'CURRENT' and field in load.channels and field in load.scales:
                    volts = self.scale_current(volts, device_id)
                    amplitude = volts * load.scales[field] * self.AMS_CURRENT_CONSTANT
                    self.write_analog_helper(load, load.channels[field], amplitude)
            elif 'BRANCH' in device_id.upper() and device_id in self.branches:
                branch = self.branches[device_id]
                if field.upper() == 'CURRENT':
                    volts = self.scale_current(volts, device_id)
                branch.values[field] = volts
                if branch.is_valid(self.buses):
                    branch.calculate_ams_fields(self.AMS_CURRENT_CONSTANT)
                    self.write_ams_branch(branch)
            elif 'BUS' in device_id.upper() and device_id in self.buses:
                bus = self.buses[device_id]
                bus.values[field] = volts
                if bus.is_valid():
                    bus.calculate_ams_fields(self.AMS_VOLTAGE_CONSTANT)
                    self.write_ams_bus(bus)
            else:
                raise NotImplementedError

    def write_analog_helper(self, obj, channels, amplitude):
        for channel in channels:
            for _ in range(0, 3):
                cmd = self.AMS_COMMAND_PREAMBLE
                cmd += ('%X' % 1).zfill(2)                   # State
                cmd += ('%X' % channel)                      # Channel
                cmd += ('%X' % int(amplitude)).zfill(8)      # Amplitude
                cmd += ('%X' % obj.phases[_]).zfill(4)       # Phase
                cmd += ('%X' % int(obj.frequency)).zfill(2)  # Frequency
                channel += 1

                if self.connect_to_ams:
                    self.ser.write(cmd.encode() + '\r\n'.encode())
                    self.logger.debug(f'SENT ({obj.id}): {cmd}')

    def scale_current(self, current, device_id):
        result = 0
        if self.max_input_current == None and self.max_output_current == None:
            result = current
        elif self.max_output_current == None:# No max output current therefore no need to scale or clip
            result = current
        elif self.max_input_current == None:# There is a max output current but no max input current so cannot scale, but will clip the current
            self.logger.log(31, f"Current of {current} is above the max allowed ({self.max_output_current}A) (Device: {device_id})")
            result = self.max_current
        elif self.max_input_current > 0 and self.max_output_current > 0:
            result = self.linear_scale(current, 0, self.max_input_current, 0, self.max_output_current)
            self.logger.log(31, f"Current of {current} will be scaled to {result} (Device: {device_id})")
        elif current < 0:
            self.logger.filter(f"Current of {current} is below 0, setting to 0. (Device: {device_id})")
            current = 0

        return result

    def read_digital(self, io_point):
        status = False
        with self.lock:
            if io_point in self.input_map:
                status = self.input_map[io_point]

        return status

    def write_digital(self, io_point, volts):
        device_id, field = self.parse_tag(io_point)

        if field != 'active':
            self.logger.error(f'Invalid field used to write to AMS ({field}).')
            return

        if device_id is not None:
            if 'GENERATOR' in device_id.upper():
                self.generators[device_id].values[field] = volts
            elif 'LOAD' in device_id.upper():
                self.loads[device_id].values[field] = volts
            elif 'BRANCH' in device_id.upper():
                self.branches[device_id].values[field] = volts
            elif 'BUS' in device_id.upper():
                self.buses[device_id].values[field] = volts
            else:
                raise NotImplementedError

            # Write to AMS. Order can be changed.
            status = ([int(bus.values[field]) for bus in self.buses.values()] +
                      [int(generator.values[field]) for generator in self.generators.values()] +
                      [int(load.values[field]) for load in self.loads.values()] +
                      [int(branch.values[field]) for branch in self.branches.values()])
            self.write_ams_outputs(status)

    def write_ams_outputs(self, output_list):
        self.logger.info('SENDING NEW OUTPUT STATE TO AMS')

        cmd = self.AMS_OUTPUT_PREAMBLE
        outputs_str = "0b" + "".join([str(output) for output in output_list[::-1]])
        outputs_num = hex(int(outputs_str, 2)).strip('0x').zfill(3)
        cmd += outputs_num
        if self.connect_to_ams:
            self.ser.write(cmd.encode() + '\r\n'.encode())
            self.logger.debug(f'SENT: {cmd}')

    def write_ams_bus(self, bus):
        for ichannel in bus.channels:
            for _ in range(0, 3):
                self.logger.debug(f'{bus.id} -> Channel: {ichannel}, Amplitude: {bus.amplitude}, Phase: {bus.phases[_]}')
                cmd = self.AMS_COMMAND_PREAMBLE
                cmd += ('%X' % 1).zfill(2)
                cmd += ('%X' % ichannel)
                cmd += ('%X' % bus.amplitude).zfill(8)
                cmd += ('%X' % bus.phases[_]).zfill(4)
                cmd += ('%X' % int(bus.frequency)).zfill(2)
                ichannel += 1

                if self.connect_to_ams:
                    self.ser.write(cmd.encode() + '\r\n'.encode())
                    self.logger.debug(f'SENT({bus.id}): {cmd}')

    def write_ams_branch(self, branch):
        for ichannel in branch.channels:
            for _ in range(0, 3):
                self.logger.debug(f'{branch.id} -> Channel: {ichannel}, Amplitude: {branch.amplitude}, Phase: {branch.phases[_]}')
                cmd = self.AMS_COMMAND_PREAMBLE
                cmd += ('%X' % 1).zfill(2)
                cmd += ('%X' % ichannel)
                cmd += ('%X' % branch.amplitude).zfill(8)
                cmd += ('%X' % branch.phases[_]).zfill(4)
                cmd += ('%X' % int(branch.frequency)).zfill(2)
                ichannel += 1

                if self.connect_to_ams:
                    self.ser.write(cmd.encode() + '\r\n'.encode())
                    self.logger.debug(f'SENT ({branch.id}): {cmd}')

    def write_waveform(self, io_point, freq):
        raise NotImplementedError

    def read_flip_flop(self, label):
        if len(self.input_map) == 2:
            self.flip_flop_state = self.flippity_floppity(self.input_map['digital_input_1'], self.input_map['digital_input_2'], self.flip_flop_state)
            #print(f'Flip Flop State: {self.flip_flop_state}')
            return self.flip_flop_state
        else:
            self.logger.error("In order to use Flip Flops 2 inputs must be defined in the configuration")

    def flippity_floppity(self, close, trip, ff_state):
        self.logger.debug(f'Close: {close}, Trip: {trip}, FlipFlop: {ff_state}')
        close = not close
        trip = not trip
        x = False
        if close or ff_state:
            x = True
        if x and trip:
            x = False
        return x

    class Branch():
        def __init__(self, id, channels, scale):
            self.id = id
            self.logger = logging.getLogger(__class__.__name__)

            self.channels = channels
            self.frequency = 60.0
            self.phases = [0, 120, 240]
            self.scale = float(scale)

            self.bus_angle = None

            self.bus_id_1 = ""
            self.bus_id_2 = ""

            self.values = {
                'active': 0,
                'source': None,
                'target': None,
                'source_mw': None,
                'source_mvar': None,
                'target_mw': None,
                'target_mvar': None,
                'mva': None,
                'mva_max': None,
                'current': None
            }

        def extract_bus_ids(self):
            '''
            In order to calculate the values sent to the AMS, the bus the branch is connected to
            needs to be known.  The bus id shoudl be part of the branch id:
            BRANCH-<branch id>_BUS-<Bus 1 ID>-<Bus 2 ID>
            '''
            results = re.search(r"BRANCH-.*_BUS-(\d*)-(\d*)", self.id.upper())
            if results is not None and len(results.groups()) == 2:
                self.bus_id_1, self.bus_id_2 = results.group(1, 2)
                self.bus_id_1 = "BUS-" + self.bus_id_1
                self.bus_id_2 = "BUS-" + self.bus_id_2
                return True
            return False

        def is_valid(self, buses):
            for key, bus in buses.items():
                if ((self.bus_id_1.upper() in bus.id.upper() or
                     self.bus_id_2.upper() in bus.id.upper()) and
                     bus.get_angle() is not None):
                    self.bus_angle = bus.get_angle()

            if None not in (self.bus_angle, self.values['source_mw'],
                            self.values['source_mvar'], self.values['current']):
                return True
            return False


        def calculate_ams_fields(self, current_constant):
            source_mvar = self.values['source_mvar']
            source_mw = self.values['source_mw']
            if source_mw != 0:
                angle = atan(source_mvar / source_mw)
                angle = (angle * 180) / 3.14159 # Convert to degrees
                phase = int((self.bus_angle - angle) * 10) % 3600
            else:
                phase = int(self.bus_angle * 10) % 3600
            self.phases = [phase, (phase + 2400) % 3600, (phase + 1200) % 3600]

            current = self.values['current']
            if current > 1.5:
                self.logger.error(f"Branch current of {current} is above the max allowed (1.5A) (Device: {self.id})")
                current = 1.5
            elif current < 0:
                self.logger.error(f"Branch current of {current} is below 0, setting to 0. (Device: {self.id})")
                current = 0
            self.amplitude = int(current * self.scale * current_constant)
            if not(0 <= self.amplitude and self.amplitude < 0x17FCBB657):
                self.logger.error('Current amplitude must be [0, 264.75]. Clipping!')
                self.amplitude = 0x17FCBB657

    class Bus():
        def __init__(self, id, channels, scale):
            self.id = id
            self.logger = logging.getLogger(__class__.__name__)

            self.channels = channels
            self.frequency = 60.0
            self.phases = [0, 120, 240]
            self.scale = float(scale)

            self.values = {
                'number': None,
                'active': 0,
                'voltage': None,
                'voltage_angle': None,
                'angle': None,
                'base_kv': None,
                'freq': None
            }

        def get_angle(self):
            '''
            Resolve angle vs voltage_angle issue with pwds
            '''
            if self.values['voltage_angle'] is not None:
                return self.values['voltage_angle']
            elif self.values['angle'] is not None:
                return self.values['angle']
            else:
                '''
                Consider deleting this logger eventually. It'll print everytime is_valid is called below if angle hasn't been sent yet
                '''
                self.logger.debug(f"Bus angle has not been assigned yet")
                return None

        def is_valid(self):
            if None not in (self.values['voltage'], self.get_angle(), self.values['base_kv']):
                return True
            return False

        def calculate_ams_fields(self, voltage_constant):
            phase = int(self.get_angle() * 10) % 3600
            self.phases = [phase, (phase + 2400) % 3600, (phase + 1200) % 3600]

            self.amplitude = int(self.values['voltage'] * self.values['base_kv'] * 1000 * self.scale * voltage_constant)
            if not (0 <= self.amplitude and self.amplitude < 0xC7FCBB657):
                self.logger.error('Voltage amplitude must be [0, 529.50]. Clipping!')
                self.amplitude = 0xC7FCBB657

    class Generator():
        def __init__(self, id, channels, scales):
            self.id = id

            self.channels = channels
            self.frequency = 60.0
            self.phases = [0, 2400, 1200]
            self.scales = scales

            self.values = {
                'bus': None,
                'active': 0,
                'voltage': None,
                'mw': None,
                'mvar': None,
                'mw_max': None,
                'mw_min': None,
                'freq': None,
                'current': None,
                'base_kv': 0.480
            }


    class Load():
        def __init__(self, id, channels, scales):
            self.id = id

            self.channels = channels
            self.frequency = 60.0
            self.phases = [0, 2400, 1200]
            self.scales = scales

            self.values = {
                'bus': None,
                'active': 0,
                'mw': None,
                'mvar': None,
                'current': None
            }
