#!/usr/bin/python3

import json
import math
import sys

from labjack import ljm
from pybennu.siren.hil_device.hil_device import HIL_Device
from pybennu.siren.hil_device.hil_device import MsgType


class LabJackT7(HIL_Device):
    def __init__(self, device_config, device_name, level_name, device_id):
        self.device_name = device_name
        self.device_id = device_id
        self.flip_flop_state = {}

        with open(device_config) as f:
            self.label_mapping = json.load(f)
            self.validate_label_mapping()

        self.setup_logger(level_name)

        # Open the first LabJack T7 device found.
        try:
            id = 'Any'
            if self.device_id is not None:
                id = self.device_id
            print("Opening labjack connection")
            self.handle = ljm.openS("T7", "ANY", id)
        except ljm.ljm.LJMError as e:
            self.logger.error(e)
            sys.exit()

        # Streaming configurations.
        self.freq = 0
        self.streaming = False
        try:
            ljm.eStreamStop(self.handle)
        except Exception as e:
            pass

        self.logger.info(ljm.getHandleInfo(self.handle))

    @property
    def config_to_analog(self, *args):
        # LabJack T7 uses the LJM library so this function is not needed.
        raise AttributeError(
            "'LabJackT7' object has no attribute 'config_to_analog'")

    @property
    def config_to_digital(self, *args):
        # LabJack T7 uses the LJM library so this function is not needed.
        raise AttributeError(
            "'LabJackT7' object has no attribute 'config_to_digital'")

    def read_analog(self, label):
        try:
            pin = self.label_mapping[label]["pin"]
            voltage = ljm.eReadName(self.handle, pin)
            return self.voltage_to_data(label, voltage)
        except Exception as e:
            self.logger.error(e)

    def write_analog(self, label, data):
        try:
            pin = self.label_mapping[label]["pin"]
            voltage = self.data_to_voltage(label, data)
            ljm.eWriteName(self.handle, pin, voltage)
        except Exception as e:
            self.logger.error(e)

    def read_digital(self, label):
        try:
            pin = self.label_mapping[label]
            # Check Pin type. Analog Inputs can be used like Digital Inputs
            if pin.startswith("AIN"):
                return 1 if (ljm.eReadName(self.handle, pin) >= 2.5) else 0
            else:
                return ljm.eReadName(self.handle, pin)
        except Exception as e:
            self.logger.error(e)

    def convert_volts_to_digital(self, volts):
        digital = 0
        if volts > 3.5:
            digital = 1
        return digital

    def read_flip_flop(self, label):
        try:
            pins = self.label_mapping[label]
            if isinstance(pins, list):
                close_pin = 0
                trip_pin = 0
                for idx in range(0, len(pins), 2):
                    close_val = ljm.eReadName(self.handle, pins[idx])
                    close_digital = self.convert_volts_to_digital(close_val)
                    close_pin = close_pin | close_digital
                    trip_pin = trip_pin | self.convert_volts_to_digital(ljm.eReadName(self.handle, pins[idx + 1]))
                if pins[0] not in self.flip_flop_state:
                    self.flip_flop_state[pins[0]] = 0
                flip_flop_value = self.flippity_floppity(close_pin, trip_pin, self.flip_flop_state[pins[0]])
                self.flip_flop_state[pins[0]] = flip_flop_value
                return flip_flop_value
            else:
                self.logger.error('Flip flop configuration types should be a list of pairs, i.e. [close1, trip1, close2, trip2')
        except Exception as e:
            self.logger.error(e)
        return 0

    def flippity_floppity(self, close, trip, ff_state):
        x = 0
        if close == 1 or ff_state == 1:
            x = 1
        if x == 1 and trip == 1:
            x = 0

        return x

    def write_digital(self, label, status):
        try:
            pin = self.label_mapping[label]
            voltage = 1 if status else 0
            # Currently have not used TDAC tag, but should work fine
            if pin.startswith("TDAC"):
                voltage = 10 if status else 0
            ljm.eWriteName(self.handle, pin, voltage)
        except Exception as e:
            self.logger.error(e)

    def write_waveform(self, label, freq):
        try:
            # No change in frequency.
            if self.freq == freq:
                return

            # Stop the stream, then restart it with the new frequency.
            if self.streaming:
                ljm.eStreamStop(self.handle)

            scale = 1
            if 'scale' in self.label_mapping[label]:
                scale = self.label_mapping[label]['scale']

            # How many points from a single wave cycle to calculate.
            shape = self.label_mapping[label]['shape']
            if shape == 'sine':
                resolution = 100
            elif shape == 'square':
                resolution = 2
            else:
                raise NotImplementedError()

            # Raise centerline since LJ only does [0, 5] volts.
            centerline = 2.5

            # The amount of times the output pin will change state per second.
            scan_rate = resolution * (freq * scale)

            # Calculate the samples of the wave.
            if shape == 'sine':
                amplitude = 2
                samples = [((math.sin(2 * math.pi * x / resolution) * amplitude) + centerline) for x in range(resolution)]
            elif shape == 'square':
                samples = [(x * 4.5) for x in range(resolution)]
            else:
                raise NotImplementedError

            # Setup stream.
            pin = self.label_mapping[label]['pin']
            ljm.eWriteName(self.handle, 'STREAM_OUT0_TARGET', ljm.nameToAddress(pin)[0])
            ljm.eWriteName(self.handle, 'STREAM_OUT0_BUFFER_SIZE', 512) # 256 values
            ljm.eWriteName(self.handle, 'STREAM_OUT0_ENABLE', 1)
            ljm.eWriteNameArray(self.handle, 'STREAM_OUT0_BUFFER_F32', resolution, samples)
            ljm.eWriteName(self.handle, 'STREAM_OUT0_SET_LOOP', 1)
            ljm.eReadName(self.handle, 'STREAM_OUT0_BUFFER_STATUS')
            ljm.eWriteName(self.handle, 'STREAM_OUT0_LOOP_SIZE', resolution)

            # Start stream.
            ljm.eStreamStart(self.handle, 1, 1, [4800], scan_rate)

            # Update member variables.
            self.freq = freq
            self.streaming = True
        except Exception as e:
            self.logger.error(e)

    def cleanup(self):
        self.logger.info('Closing connection.')
        ljm.close(self.handle)

    def __del__(self):
        pass


HIL_Device.register(LabJackT7)
