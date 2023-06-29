"""Protobuf to PTI converter.

This module converters Protobuf objects to PTI format for PyPower to ingest
and perform a power flow solve, then converts the results back to protobuf
again to publish out to Gryffin.
"""
import math
from numpy import array
from pybennu.protobuf.bennu_pb2 import PowerTransmission

class PTIManager:
    """Helper class for managing PTI <--> Protobuf references"""

    def __init__(self, system):
        """Expects a Protobuf PowerTransmission to initialize indexes from"""

        if not isinstance(system, PowerTransmission):
            raise AttributeError('PTIManager: system must be of type bennu.protobuf.PowerTransmission')

        self.__initialize_indexes(system)
        self.__initialize_data(system)

    def pb_to_pti(self, system):
        for gen in system.generators:
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

        for load in system.loads:
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

        for shunt in system.shunts:
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

        for branch in system.branches:
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

    def pti_to_pb(self, devices, results):
        for idx, data in enumerate(results['bus']):
            name = self.busidxnam[idx]
            bus = devices[name]

            if data[1] == 4:
                bus.active = False
            else:
                bus.active = True

            if data[1] != 3:
                bus.slack = False
            else:
                bus.slack = True

            bus.voltage = data[7]

            bus.angle = data[8]
            bus.load_mw = data[2]
            bus.load_mvar = data[3]
            bus.shunt_mvar = data[5]

            bus.gen_mw = 0.0
            bus.gen_mvar = 0.0

            if data[2] != 0 or data[3] != 0:
                assert bus.name in self.busnamlodnam

                load_name = self.busnamlodnam[bus.name]
                load = devices[load_name]

                load.mw = data[2]
                load.mvar = data[3]

            if data[5] != 0:
                assert bus.name in self.busnamshtnam

                shunt_name = self.busnamshtnam[bus.name]
                shunt = devices[shunt_name]

                shunt.actual_mvar = data[5]

        for idx, data in enumerate(results['gen']):
            name = self.genidxnam[idx]
            gen = devices[name]

            if data[7] <= 0:
                gen.active = False
            else:
                gen.active = True

            gen.voltage = data[5]
            gen.mw = data[1]
            gen.mvar = data[2]

            bus_idx = self.busnumidx[data[0]]
            bus_name = self.busidxnam[bus_idx]
            assert bus_name
            bus = devices[bus_name]

            bus.gen_mw += data[1]
            bus.gen_mvar += data[2]

        for idx, data in enumerate(results['branch']):
            name = self.bchidxnam[idx]
            branch = devices[name]

            if data[10] == 0:
                branch.active = False
            else:
                branch.active = True

            if len(data) > 13:
                mw = data[13]
                mvar = data[14]

                branch.mva = math.sqrt(math.pow(mw, 2) + math.pow(mvar, 2))

        return

    def __initialize_indexes(self, system):
        """
        When initially created, Protobuf PowerTransmission objects are 'fully loaded'
        with information. i.e. they include things like reference bus numbers. However,
        when providing updates, the ONLY piece of of information required is the (ID)
        of the object. Thus, when we get the initial system we need to create index
        references to all required message objects based on name, such that when an
        update message comes in with JUST the name we can still operate.

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

        buses = []
        gens = []
        branches = []

        for bus in system.buses:
            assert bus.HasField('name')
            assert bus.HasField('number')

            data = [0.0] * 13
            data[0] = bus.number

            buses.append(data)
            idx = len(buses) - 1

            self.busnumidx[bus.number] = idx
            self.busnamidx[bus.name] = idx
            self.busidxnam[idx] = bus.name

        for gen in system.generators:
            assert gen.HasField('name')
            assert gen.HasField('bus')

            data = [0.0] * 21
            data[0] = gen.bus

            gens.append(data)
            idx = len(gens) - 1

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

            branches.append(data)
            idx = len(branches) - 1

            self.bchnamidx[branch.name] = idx
            self.bchidxnam[idx] = branch.name

        self.ppc = {'version': '2', 'baseMVA': 100.0,
                    'bus': array(buses), 'gen': array(gens),
                    'branch': array(branches)}

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

        return
