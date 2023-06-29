"""Python-Power converter utility.

This module instantiates a converter tool that converts pypower power
system models from protobuf format to PTI format and back.
"""

import math
import sys
import os
import importlib
from numpy import array

from pypower.int2ext import int2ext
from pybennu.protobuf.power_transmission_pb2 import PowerSystem

class PyPowerConverter:
    """Converter class for pypower power system models."""
    def __init__(self):
        self.system = PowerSystem()

    def convert(self, model):
        """Execute conversion from protobuf to PTI or vice versa."""
        if '.pb' in model:
            pti_model = self.pb_to_pti(model)
            return pti_model
        else:
            pb_model = self.pti_to_pb(model)
            return pb_model

    def pb_to_pti(self, model):
        """Convert power system model from protobuf to PTI."""
        try:
            with open(model, 'rb') as file:
                self.system.ParseFromString(file.read())
        except IOError:
            print('Failed to parse the protobuf file ' + model + '.')

        self.__initialize_indexes(self.system)
        self.__initialize_data(self.system)

        for gen in self.system.generators:
            assert gen.name in self.gennamidx
            idx = self.gennamidx[gen.name]
            assert idx is not None

            data = self.ppc['gen'][idx]
            assert data.any()

            data[1] = gen.mw
            data[5] = gen.voltage

            if gen.active:
                data[7] = 1
            else:
                data[7] = 0

        demands = {}

        for load in self.system.loads:
            assert load.name in self.lodnambusnam
            bus = self.lodnambusnam[load.name]
            assert bus
            idx = self.busnamidx[bus]
            assert idx is not None

            if not idx in demands:
                demands[idx] = {'mw': 0.0, 'mvar': 0.0}

            if load.active:
                demands[idx]['mw'] += load.mw
                demands[idx]['mvar'] += load.mvar

        for bus, demand in demands.items():
            self.ppc['bus'][bus][2] = demand['mw']
            self.ppc['bus'][bus][3] = demand['mvar']

        capacities = {}

        for shunt in self.system.shunts:
            assert shunt.name in self.shtnambusnam
            bus = self.shtnambusnam[shunt.name]
            assert bus
            idx = self.busnamidx[bus]
            assert idx is not None

            if not idx in capacities:
                capacities[idx] = 0.0

            if shunt.active:
                capacities[idx] += shunt.nominal_mvar

        for bus, capacity in capacities.items():
            self.ppc['bus'][bus][5] = capacity

        for branch in self.system.branches:
            assert branch.name in self.bchnamidx
            idx = self.bchnamidx[branch.name]
            assert idx is not None

            data = self.ppc['branch'][idx]
            assert data.any()

            if branch.active:
                data[10] = 1
            else:
                data[10] = 0

        return self.ppc

    def pti_to_pb(self, ppc):
        """Convert power system model from PTI to protobuf."""
        system = PowerSystem()

        system.name = 'power_system'

        g = {}
        for idx, data in enumerate(ppc['bus']):
            bus = system.buses.add()
            bus.name = 'bus-' + str(int(data[0]))

            bus.number = int(data[0])

            if data[1] == 4:
                bus.active = False
            else:
                bus.active = True

            if data[1] == 3:
                bus.slack = True
            else:
                bus.slack = False

            bus.voltage = data[7]
            bus.angle = data[8]
            bus.load_mw = data[2]
            bus.load_mvar = data[3]
            bus.shunt_mvar = data[5]
            bus.base_kv = data[9]

            bus.gen_mw = 0.0
            bus.gen_mvar = 0.0

            g[int(data[0])] = 0
            if data[2] != 0 or data[3] != 0:
                load = system.loads.add()
                load.name = 'load-1_' + bus.name
                load.mw = data[2]
                load.mvar = data[3]
                load.bus = int(data[0])
                load.active = True
                load.bid = 1.0

            if data[5] != 0:
                shunt = system.shunts.add()
                shunt.name = 'shunt-1_' + bus.name
                shunt.actual_mvar = data[5]
                shunt.nominal_mvar = data[5]
                shunt.bus = int(data[0])
                shunt.active = True

        for idx, data in enumerate(ppc['gen']):
            gen = system.generators.add()
            bus_name = 'bus-' + str(int(data[0]))

            g[int(data[0])] += 1
            gen.name = 'generator-' + str(g[int(data[0])]) + '_' + bus_name
            gen.bus = int(data[0])

            if data[7] <= 0:
                gen.active = False
            else:
                gen.active = True

            gen.voltage = data[5]
            gen.mw = data[1]
            gen.mvar = data[2]
            gen.mvar_max = data[3]
            gen.mvar_min = data[4]
            gen.mw_max = data[8]
            gen.mw_min = data[9]
            gen.active = True
            gen.bid = 1.0

            for bus in system.buses:
                if bus_name == bus.name:
                    bus.gen_mw += data[1]
                    bus.gen_mvar += data[2]
                    break

        br = 1
        tempBname='' # This is a quick fix for having 2 // branches between 2 buses - note: this does not extend to more than 2 // branches between 2 buses
        for idx, data in enumerate(ppc['branch']):
            branch = system.branches.add()
            branch.name = 'branch-1_' + str(int(data[0])) + '-' + str(int(data[1]))
            if branch.name == tempBname:
                branch.name = 'branch-2_' + str(int(data[0])) + '-' + str(int(data[1]))
            tempBname=branch.name
            if data[10] == 0:
                branch.active = False
            else:
                branch.active = True

            branch.source = int(data[0])
            branch.target = int(data[1])
            branch.resistance = data[2]
            branch.reactance = data[3]
            branch.charging = data[4]
            branch.mva_max = data[5]
            branch.mva_max = data[6]
            branch.mva_max = data[7]
            branch.turns_ratio = data[8]
            branch.phase_angle = data[9]

            if len(data) > 13:
                mw = data[13]
                mvar = data[14]
                branch.mva = math.sqrt(math.pow(mw, 2) + math.pow(mvar, 2))
            br += 1

        return system

    def __initialize_indexes(self, system):
        """
        When initially created, Protobuf PowerSystem objects are 'fully loaded' with
        information... i.e. they include things like reference bus numbers. However,
        when providing updates, the ONLY piece of data that required is the device
        (ID) of the object. Thus, when we get the initial system we need to create
        index references to all required message objects based on name, such that
        when an update message comes in with JUST the name we can still operate.
        Example:
          Bus:
            name:   bus101
            number: 101
          Generator:
            name: generator1_bus101
            bus:  101
          Load:
            name: load1_bus101
            bus:  101
        """

        self.busnumidx = {}
        self.busnamidx = {}
        self.busidxnam = {}
        self.gennamidx = {}
        self.genidxnam = {}
        self.bchnamidx = {}
        self.bchidxnam = {}
        self.lodnambusnam = {}
        self.busnamlodnam = {}
        self.shtnambusnam = {}
        self.busnamshtnam = {}

        self.buses = []
        self.gens = []
        self.branches = []

        for bus in system.buses:
            assert bus.HasField('name')
            assert bus.HasField('number')

            data = [0.0] * 13
            data[0] = bus.number

            self.buses.append(data)
            idx = len(self.buses) - 1

            self.busnumidx[bus.number] = idx
            self.busnamidx[bus.name] = idx
            self.busidxnam[idx] = bus.name

        for gen in system.generators:
            assert gen.HasField('name')
            assert gen.HasField('bus')

            data = [0.0] * 21
            data[0] = gen.bus

            self.gens.append(data)
            idx = len(self.gens) - 1

            self.gennamidx[gen.name] = idx
            self.genidxnam[idx] = gen.name

        for load in system.loads:
            assert load.HasField('name')
            assert load.HasField('bus')

            idx = self.busnumidx[load.bus]
            nam = self.busidxnam[idx]
            self.lodnambusnam[load.name] = nam
            self.busnamlodnam[nam] = load.name

        for shunt in system.shunts:
            assert shunt.HasField('name')
            assert shunt.HasField('bus')

            idx = self.busnumidx[shunt.bus]
            nam = self.busidxnam[idx]
            self.shtnambusnam[shunt.name] = nam
            self.busnamshtnam[nam] = shunt.name

        for branch in system.branches:
            assert branch.HasField('name')
            assert branch.HasField('source')
            assert branch.HasField('target')

            data = [0.0] * 13
            data[0] = branch.source
            data[1] = branch.target

            self.branches.append(data)
            idx = len(self.branches) - 1

            self.bchnamidx[branch.name] = idx
            self.bchidxnam[idx] = branch.name

        self.ppc = {'version': '2', 'baseMVA': 100.0,
                    'bus': array(self.buses), 'gen': array(self.gens),
                    'branch': array(self.branches)}
        return self.ppc

    def __initialize_data(self, system):
        for bus in system.buses:
            assert bus.HasField('name')
            assert bus.HasField('active')
            assert bus.HasField('slack')
            assert bus.HasField('voltage')
            assert bus.HasField('angle')
            assert bus.HasField('base_kv')

            idx = self.busnamidx[bus.name]
            assert idx is not None

            data = self.ppc['bus'][idx]
            assert data.any()

            if bus.active:
                if bus.slack:
                    data[1] = 3
                else:
                    data[1] = 1
            else:
                data[1] = 4

            data[6] = 1
            data[7] = bus.voltage
            data[8] = bus.angle
            data[9] = bus.base_kv
            data[10] = 1
            data[11] = 0.9
            data[12] = 1.1
        for load in system.loads:
            assert load.HasField('bus')
            assert load.HasField('mw')
            assert load.HasField('mvar')

            bus = self.busnumidx[load.bus]
            assert bus is not None
            self.ppc['bus'][bus][2] += load.mw
            self.ppc['bus'][bus][3] += load.mvar


        for gen in system.generators:
            assert gen.HasField('name')
            assert gen.HasField('mw')
            assert gen.HasField('mvar')
            assert gen.HasField('mvar_max')
            assert gen.HasField('mvar_min')
            assert gen.HasField('voltage')
            assert gen.HasField('active')
            assert gen.HasField('bus')
            assert gen.HasField('mw_max')
            assert gen.HasField('mw_min')

            idx = self.gennamidx[gen.name]
            assert idx is not None

            data = self.ppc['gen'][idx]
            assert data.any()

            data[1] = gen.mw
            data[2] = gen.mvar
            data[3] = gen.mvar_max
            data[4] = gen.mvar_min
            data[5] = gen.voltage
            data[6] = 100.0

            if gen.active:
                data[7] = 1


                bus = self.busnumidx[gen.bus]
                assert bus is not None
                if self.ppc['bus'][bus][1] != 3:
                    self.ppc['bus'][bus][1] = 2
            else:
                data[7] = 0

            data[8] = gen.mw_max
            data[9] = gen.mw_min

        for shunt in system.shunts:
            assert shunt.HasField('bus')
            assert shunt.HasField('nominal_mvar')

            bus = self.busnumidx[shunt.bus]
            assert bus is not None
            self.ppc['bus'][bus][5] += shunt.nominal_mvar

        for branch in system.branches:
            assert branch.HasField('name')
            assert branch.HasField('resistance')
            assert branch.HasField('reactance')
            assert branch.HasField('charging')
            assert branch.HasField('mva_max')
            assert branch.HasField('turns_ratio')
            assert branch.HasField('phase_angle')
            assert branch.HasField('active')

            idx = self.bchnamidx[branch.name]
            assert idx is not None

            data = self.ppc['branch'][idx]
            assert data.any()

            data[2] = branch.resistance
            data[3] = branch.reactance
            data[4] = branch.charging
            data[5] = branch.mva_max
            data[6] = branch.mva_max
            data[7] = branch.mva_max
            data[8] = branch.turns_ratio
            data[9] = branch.phase_angle

            if branch.active:
                data[10] = 1
            else:
                data[10] = 0

            data[11] = -360
            data[12] = 360

        return self.ppc

if __name__ == '__main__':
    if len(sys.argv) == 2 and os.path.isfile(sys.argv[1]):
        system_file = str(sys.argv[1])
    else:
        if len(sys.argv) != 2:
            raise AttributeError('Script takes only 1 argumnet, \
                                 the power system file you wish to convert.')
        elif not os.path.isfile(sys.argv[1]):
            raise IOError('Power system file does not exist.')
        print('Command line usage: python converter.py </path/to/system/definition/file>')

    system_filename = os.path.splitext(system_file)[0]
    system_filename = system_filename.replace('-', '_')
    converter = PyPowerConverter()

    if '.pb' in system_file:
        pti = str(converter.convert(system_file))
        try:
            with open('pti_' + system_filename + ".py", "w") as f:
                f.write('from numpy import array\n\ndef power_system():\n\n')
                f.write('\tppc = ' + pti)
                f.write('\n\n\treturn ppc')
        except IOError:
            print("Error opening file " + system_filename + '.')
        print(pti)
    else:
        #I'm assuming that the PTI file has a function named
        #'power_system' that defines the ppc dictionary.
        exec('from ' + system_filename + ' import power_system')
        ppc = power_system()
        pb_obj = converter.convert(ppc)
        try:
            with open(system_filename + ".pb", "wb") as f:
                f.write(pb_obj.SerializeToString())
        except IOError:
            print('Error opening file ' + system_filename + '.')
        print(pb_obj)
