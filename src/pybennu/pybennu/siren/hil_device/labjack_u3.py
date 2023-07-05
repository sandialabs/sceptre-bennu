#!/usr/bin/python3
import json
import pkg_resources as pkg
from os.path import join

from pybennu.siren.hil_device import virtualsensor
from pybennu.siren.hil_device import HIL_Device
from u3 import U3


io_points = ['FIO0', 'FIO1', 'FIO2', 'FIO3', 'FIO4', 'FIO5', 'FIO6', 'FIO7',
             'EIO0', 'EIO1', 'EIO2', 'EIO3', 'EIO4', 'EIO5', 'EIO6', 'EIO7',
             'CIO0', 'CIO1', 'CIO2', 'CIO3']


class LabJackU3(HIL_Device):
    """U3-LV implementation of the HIL_Device base class.

    Attributes:
        device: An instantiated new U3 object from the U3 module.
        label_mapping: The mapping of labels to IO points.

    References:  
        For possible values of ModBus registers see
        https://labjack.com/support/software/api/modbus/ud-modbus

        For possible values of value see
        https://labjack.com/support/datasheets/u3/appendix-a

        For possible values of label/io_point see
        https://labjack.com/support/datasheets/u3/hardware-description/ain/channel_numbers
    """

    def __init__(self, device_config):
        """Inits U3."""
        self.device = U3()

        with open(device_config) as f:
            self.label_mapping = json.load(f)

        self.device.configU3(FIOAnalog = 255, EIOAnalog = 255)
        for key in self.label_mapping['to_gryffin'].keys():
            if 'digital' in key:
                io_point = io_points.index(self.label_mapping['to_gryffin'][key])
                self.device.configDigital(io_point)
        for key in self.label_mapping['to_hardware'].keys():
            if 'digital' in key:
                io_point = io_points.index(self.label_mapping['to_hardware'][key])
                self.device.configDigital(io_point)

    def config_to_analog(self, *args):
        """Configures specified IO points to analog.

        If args is empty, sets all IO points to analog.

        Args:
            args: An argument list of IO points to be configured to analog. Possible values include [0, 20).
        """
        self.device.configAnalog(args)

    def config_to_digital(self, *args):
        """Configures specified IO points to digital.

        If args is empty, sets all IO points to digital.

        Args:
            args: An argument list of IO points to be configured to digital. Possible values include [0, 20).
        """
        self.device.configDigital(args)

    def read_analog(self, label):
        """Reads the value of an IO point.

        Args:
            label: The IO point to read from. String.

        Returns:
            A float of what was read.
        """
        io_point = io_points.index(self.label_mapping[label])
        return self.device.getAIN(io_point)

    def write_analog(self, label, value):
        """Writes a value to an IO point.

        Args:
            label: The IO point to write to. Int.
            value: The value the IO point will be written to. Float.
        """
        if (isinstance(self.label_mapping[label], int)):
            register = self.label_mapping[label]
            min_data = 0
            max_data = 330
            min_voltage = 0
            max_voltage = 3.3
        else:
            register = self.label_mapping[label]['label']
            min_data = self.label_mapping[label]['min_data']
            max_data = self.label_mapping[label]['max_data']
            min_voltage = self.label_mapping[label]['min_voltage']
            max_voltage = self.label_mapping[label]['max_voltage']

        scaled_value = virtualsensor(value, min_data, max_data, min_voltage, max_voltage)
        self.device.writeRegister(register, value)

    def read_digital(self, label):
        """Reads the status of an IO point.

        Args:
            label: The IO point to read from. String.

        Returns:
            An int of what was read.
        """
        io_point = io_points.index(self.label_mapping[label])
        return self.device.getDIOState(io_point)

    def write_digital(self, label, status):
        """Sets the status of an IO point.

        Args:
            label: The IO point to set. String.
            status: The status the IO point will be set to. Bool.
        """
        io_point = io_points.index(self.label_mapping[label])
        self.device.setDOState(io_point, status)

    def __del__(self):
        """Closes the device so another program can use it."""
        #self.device.close() TODO(ask mksavic why this doesn't work)
        pass


HIL_Device.register(LabJackU3)
