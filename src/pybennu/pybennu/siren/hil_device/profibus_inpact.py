import json
import struct
import pkg_resources as pkg
from os.path  import join, isfile

from bitarray import bitarray
from sysv_ipc import SharedMemory as smem

from pybennu.siren.hitl_device.hitl_device import HITL_Device


class ProfibusInpact(HITL_Device):

    def __init__(self, device_config):
        if not isfile(device_config):
            device_config = pkg.resource_filename('pybennu',
                join('siren', 'hitl_device', 'configs', 'profibus_inpact.json'))
        with open(device_config) as file_:
            conf = json.load(file_)
        self.label_map = conf['label_map']
        self.slot_map = {int(k): v for k, v in conf['slot_map'].items()}
        self.slots = {k: smem(v['memid']) for k, v in self.slot_map.items()}

    def read_analog(self, io_point_label):
        slot, datatype, size, channel = self.parse_label(io_point_label)
        digital = struct.unpack(size*datatype, self.slots[slot].read())[channel]
        return self.d_to_a(digital, slot, datatype, channel)

    def d_to_a(self, digital, slot, datatype, channel):
        mind = self.slot_map[slot]['min_data'][channel]
        maxd = self.slot_map[slot]['max_data'][channel]
        minv = self.slot_map[slot]['min_volt'][channel]
        maxv = self.slot_map[slot]['max_volt'][channel]
        # convert to scaled from 16 bit unsigned int
        if datatype == 'H':
            scaled = self.virtualsensor(digital, 0, 2**15-1, minv, maxv)
        else:
            raise NotImplementedError
        return self.virtualsensor(scaled, minv, maxv, mind, maxd)

    def write_analog(self, io_point_label, value):
        slot, datatype, size, channel = self.parse_label(io_point_label)
        data = list(struct.unpack(size*datatype, self.slots[slot].read()))
        data[channel] = self.a_to_d(value, slot, datatype, channel)
        self.slots[slot].write(struct.pack(size*datatype, *data))

    def a_to_d(self, analog, slot, datatype, channel):
        mind = self.slot_map[slot]['min_data'][channel]
        maxd = self.slot_map[slot]['max_data'][channel]
        minv = self.slot_map[slot]['min_volt'][channel]
        maxv = self.slot_map[slot]['max_volt'][channel]
        scaled = self.virtualsensor(analog, mind, maxd, minv, maxv)
        # convert to 16 bit unsigned int
        if datatype == 'H':
            digital = self.virtualsensor(scaled, minv, maxv, 0, 2**15-1)
            return int(digital)
        else:
            raise NotImplementedError

    def read_digital(self, io_point_label):
        slot, datatype, size, channel = self.parse_label(io_point_label)
        slot_data = bitarray(endian='little')
        slot_data.frombytes(self.slots[slot].read())
        if datatype == 'I':
            base = 3
        else:
            raise NotImplementedError
        channel = 8*(base-int(channel/8))  + channel%8
        return slot_data[channel]

    def write_digital(self, io_point_label, value):
        slot, datatype, size, channel = self.parse_label(io_point_label)
        slot_data = bitarray(endian='little')
        slot_data.frombytes(self.slots[slot].read())
        if datatype == 'I':
            base = 3
        else:
            raise NotImplementedError
        channel = 8*(base-int(channel/8))  + channel%8
        slot_data[channel] = value
        self.slots[slot].write(slot_data.tobytes())

    def parse_label(self, label):
        slot = self.label_map[label]['slot']
        datatype = self.slot_map[slot]['datatype']
        size = self.slot_map[slot]['size']
        channel = self.label_map[label]['channel']
        return slot, datatype, size, channel

    def config_to_analog(self, *args):
        pass

    def config_to_digital(self, *args):
        pass

    def __del__(self):
        pass


HITL_Device.register(ProfibusInpact)
