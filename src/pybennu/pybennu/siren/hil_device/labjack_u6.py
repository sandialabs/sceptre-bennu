#!/usr/bin/python3
import json
import pkg_resources as pkg
from os.path import join
from pybennu.siren.hil_device import virtualsensor
from pybennu.siren.hil_device import HIL_Device
from time import sleep
from u6 import U6
from u6 import BitStateWrite
from u6 import BitStateRead

io_points = ['FIO0', 'FIO1', 'FIO2', 'FIO3']
i_points_a = ['AIN0', 'AIN1', 'AIN2', 'AIN3']
o_points_a = {'DAC0': 5000, 'DAC1': 5002}



class LabJackU6(HIL_Device):
    """U6 implementation of the HIL_Device base class. 
    Note that there are some minor differences from the U3 Implementation due to slightly different pins.

    Attributes:
        device: An instantiated new U6 object from the U6 module.
    """

    def __init__(self, device_config):
        """This initailizes the U6 device"""
        self.device = U6()

        with open(device_config) as f:
            self.label_mapping = json.load(f)

        self.device.configU3(FIOAnalog = 255, EIOAnalog = 255)
        for key in self.label_mapping['to_gryffin'].keys():
            if 'digital' in key and 'FIO' in self.label_mapping['to_gryffin'][key]:
                io_point = io_points.index(self.label_mapping['to_gryffin'][key])
                self.device.configDigital(io_point)
        for key in self.label_mapping['to_hardware'].keys():
            if 'digital' in key and 'FIO' in self.label_mapping['to_hardware'][key]:
                io_point = io_points.index(self.label_mapping['to_hardware'][key])
                self.device.configDigital(io_point)



    def config_to_analog(self, *args):
        """Configures all FIOs and EI0s to analog."""
        #self.device.configIO(FIOAnalog=255, EIOAnalog=255)
        #raise NotImplementedError
        self.device.configAnalog(args)

    def config_to_digital(self, *args):
        """Configures all FIOs and EIOs to digital."""
        #self.device.configIO(FIOAnalog=0, EIOAnalog=0)
        self.device.configDigital(args)

    def read_analog(self, label):
        """This is the analog read method.
        Args:
             register: AIN pin to read from. 
        returns:
             reading: returns a voltage level

        """


        i_point = i_points_a.index(self.label_mapping[label])
        return self.device.getAIN(i_point, 32)

    def write_analog(self, label, value):
        """This is the analog write method.
        Args:
             register: AIN pin to read from. 
             volts: voltage level that is written to the AIN.

        """
	        #default values
        mindata = 0
	maxdata = 10000
	minvolt = 0
	maxvolt = 3.3

        dicttype = type({dict:type})
	#If the mapping leads to another mapping, then don't use the defaults. Use the substructure.
        if (type(self.label_mapping[label]) == dicttype):
		mindata = self.label_mapping[label]["min_data"]
		print("min_data", mindata)
		maxdata = self.label_mapping[label]["max_data"]
		print("max_data", maxdata)
		minvolt = self.label_mapping[label]["min_voltage"]
		print("min_voltage", minvolt)
		maxvolt = self.label_mapping[label]["max_voltage"]
		print("max_voltage", maxvolt)
		label = self.label_mapping[label]["label"]
		label = label.encode("ascii")
		print("label", label)
        	
	voltage = virtualsensor(value, mindata, maxdata, minvolt, maxvolt)
	print("voltage", voltage)
        self.device.writeRegister(o_points_a[label], voltage)

        #self.device.writeRegister(o_points_a[self.label_mapping[label]], value)

    def read_digital(self, label):
        """This is the digital read method. Note that in this implementation,       the register is the pin number
        Args:
             register: FIO pin to write to. 
        returns:
             reading: returns a value of 0 or 1

        """
        io_point = io_points.index(self.label_mapping[label])
        return self.device.getDIState(io_point)

    def write_digital(self, label, status):
        """This is the digital write method. Note that in this       implementation, the register is the pin number 
        Args:
            pin: The FIO pin to write to.
            state: The desired state that must be written on the pin.
        """
        io_point = io_points.index(self.label_mapping[label])

        if (io_point >= 0) and (io_point <= 3):
            #self.device.setFIOState(self, pin, state)

            #self.device.setDOState(io_point, status)

            self.device.getFeedback(BitStateWrite(io_point, status))

    def __del__(self):
        """Closes the device so another program can use it."""


HIL_Device.register(LabJackU6)
