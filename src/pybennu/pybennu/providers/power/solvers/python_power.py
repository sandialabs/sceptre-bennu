"""PyPower-based power solver provider.

This module instantiates a PyPower-based power solver for use in SCEPTRE
environments.  The PyPowerSolver class inherits from the Provider
class which abstracts away the communication interface code that all
providers require.
"""
import copy
import sys
import time
import random
import threading
import argparse
from configparser import ConfigParser
from pypower.api import ppoption, runpf, loadcase
from pybennu.distributed.provider import Provider

class PyPower(Provider):
    """PyPower-based implementation of a grid simulation interface."""

    def __init__(self, server_endpoint, publish_endpoint, case_filename, debug=False):
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock = threading.Lock()

        self.debug = debug
        
        #self.system is a case file format dictionary of the case file
        self.system = loadcase(case_filename)

        self.elements = {'bus':{},'load':{},'shunt':{},'gen':{},'branch':{}}
        # keep a dictionary for each device type so that there is correlation between
        # device name e.g. 'load-1_bus-1' and which index idx in self.system['bus'][idx]
        # gets the row associated with the device
        self.busidx = {}
        self.genidx = {}
        self.branchidx = {}

        # use this bus to track when pypower solve fails to converge
        self.elements['bus']['bus-alert'] = {}
        self.elements['bus']['bus-alert']['active'] = False

        self.create()

    def _pcf_to_dict(self):
        """Update PyPower's internal dictionary representation based on PyPower's internal pypower case dictionary"""
        for idx,data in enumerate(self.system['bus']):
            bus_name = 'bus-' + str(int(data[0])) # the pti format bus number seems to be stored as a float
            
            if data[1] != 3:
                bus_slack = False
            else:
                bus_slack = True

            if data[1] == 4:
                bus_active = False
            else:
                bus_active = True


            bus_voltage = data[7]

            bus_angle = data[8]
            bus_base_kv = data[9]
            bus_load_mw = data[2]
            bus_load_mvar = data[3]
            bus_shunt_mvar = data[5]
            # initialize these to 0 and adjust when looking through generator PTI data
            bus_gen_mw = 0.0
            bus_gen_mvar = 0.0
            
            self.busidx[bus_name] = idx
            self.elements['bus'][bus_name] = {}
            self.elements['bus'][bus_name]['voltage'] = bus_voltage
            self.elements['bus'][bus_name]['angle'] = bus_angle
            self.elements['bus'][bus_name]['gen_mw'] = bus_gen_mw
            self.elements['bus'][bus_name]['gen_mvar'] = bus_gen_mvar
            self.elements['bus'][bus_name]['load_mw'] = bus_load_mw
            self.elements['bus'][bus_name]['load_mvar'] = bus_load_mvar
            self.elements['bus'][bus_name]['shunt_mvar'] = bus_shunt_mvar
            self.elements['bus'][bus_name]['base_kv'] = bus_base_kv
            self.elements['bus'][bus_name]['active'] = bus_active
            self.elements['bus'][bus_name]['slack'] = bus_slack

            if data[2] != 0 or data[3] != 0:
                load_name = 'load-1_' + bus_name
                load_mw = data[2]
                load_mvar = data[3]
                load_bus = int(data[0])
                load_active = True
                self.elements['load'][load_name] = {}
                self.elements['load'][load_name]['bus'] = load_bus
                self.elements['load'][load_name]['mw'] = load_mw
                self.elements['load'][load_name]['mvar'] = load_mvar
                self.elements['load'][load_name]['active'] = load_active


            if data[5] != 0:
                shunt_name = 'shunt-1_' + bus_name
                shunt_actual_mvar = data[5]
                shunt_nominal_mvar = data[5]
                shunt_bus = int(data[0])
                shunt_active = True
                self.elements['shunt'][shunt_name] = {}
                self.elements['shunt'][shunt_name]['bus'] = shunt_bus
                self.elements['shunt'][shunt_name]['actual_mvar'] = shunt_actual_mvar
                self.elements['shunt'][shunt_name]['nominal_mvar'] = shunt_nominal_mvar
                self.elements['shunt'][shunt_name]['active'] = shunt_active

        
        g = {} # keep track of multiple generators with bus numbers as keys
        #pass thru once to initialize
        for data in self.system['gen']:
            g[int(data[0])] = 0
        for idx,data in enumerate(self.system['gen']):
            bus_name = 'bus-' + str(int(data[0]))
            g[int(data[0])] += 1

            gen_name = 'generator-' + str(g[int(data[0])]) + '_' + bus_name
            gen_bus = int(data[0])

            if data[7] <= 0:
                gen_active = False
            else:
                gen_active = True

            gen_voltage = data[5]
            gen_mw = data[1]
            gen_mvar = data[2]
            gen_mvar_max = data[3]
            gen_mvar_min = data[4]
            gen_mw_max = data[8]
            gen_mw_min = data[9]
            
            self.genidx[gen_name] = idx
            self.elements['gen'][gen_name] = {}
            self.elements['gen'][gen_name]['mw'] = gen_mw
            self.elements['gen'][gen_name]['mvar'] = gen_mvar
            self.elements['gen'][gen_name]['mw_max'] = gen_mw_max
            self.elements['gen'][gen_name]['mw_min'] = gen_mw_min
            self.elements['gen'][gen_name]['mvar_max'] = gen_mvar_max
            self.elements['gen'][gen_name]['mvar_min'] = gen_mvar_min
            self.elements['gen'][gen_name]['voltage'] = gen_voltage
            self.elements['gen'][gen_name]['active'] = gen_active

            for bus in self.elements['bus']:
                if bus_name == bus:
                    self.elements['bus'][bus]['gen_mw'] += data[1]
                    self.elements['bus'][bus]['gen_mvar'] += data[2]
                    break


            
        b = {} #keep track of multiple branches with from,to bus numbers as keys e.g. b = {(0,1):0,(0,2):0}
        for data in self.system['branch']:
            b[int(data[0]),int(data[1])] = 0
        for idx,data in enumerate(self.system['branch']):
            b[int(data[0]),int(data[1])] += 1
            
            # branch-#_from-to
            branch_name = 'branch-' + str(b[int(data[0]),int(data[1])]) + '_' + str(int(data[0])) + '-' + str(int(data[1]))

            if data[10] == 0:
                branch_active = False
            else:
                branch_active = True

            branch_source = int(data[0])
            branch_target = int(data[1])
            branch_resistance = data[2]
            branch_reactance = data[3]
            branch_charging = data[4]
            branch_mva_max = data[5]
            branch_mva_max = data[6]
            branch_mva_max = data[7]
            branch_turns_ratio = data[8]
            branch_phase_angle = data[9]
            
            # 'current', 'source_mw', 'target_mw' not found in case file format

            self.branchidx[branch_name] = idx
            self.elements['branch'][branch_name] = {}
            self.elements['branch'][branch_name]['source'] = branch_source
            self.elements['branch'][branch_name]['target'] = branch_target
            self.elements['branch'][branch_name]['resistance'] = branch_resistance
            self.elements['branch'][branch_name]['reactance'] = branch_reactance
            self.elements['branch'][branch_name]['charging'] = branch_charging
            self.elements['branch'][branch_name]['mva_max'] = branch_mva_max
            self.elements['branch'][branch_name]['turns_ratio'] = branch_turns_ratio
            self.elements['branch'][branch_name]['phase_angle'] = branch_phase_angle
            self.elements['branch'][branch_name]['active'] = branch_active

    def _dict_to_pcf(self):
        """Make updates to the local pypower case format dictionary based on the local internal dictionary, elements.
        This method is used during write command handler method to update the pcf dictionary before running a power flow on it in __solve
        """
        for bus in self.elements['bus']:
            if bus != 'bus-alert':
                index = self.busidx[bus]
                data = self.system['bus'][index]
                if self.elements['bus'][bus]['active']:
                    if self.elements['bus'][bus]['slack']:
                        data[1] = 3
                    else:
                        data[1] = 1
                else:
                    data[1] = 4

                data[2] = self.elements['bus'][bus]['load_mw']
                data[3] = self.elements['bus'][bus]['load_mvar']
                data[5] = self.elements['bus'][bus]['shunt_mvar']
                data[7] = self.elements['bus'][bus]['voltage']
                data[8] = self.elements['bus'][bus]['angle']
                data[9] = self.elements['bus'][bus]['base_kv']

        for gen in self.elements['gen']:
            index = self.genidx[gen]
            data = self.system['gen'][index]
            data[1] = self.elements['gen'][gen]['mw']
            data[2] = self.elements['gen'][gen]['mvar']
            data[3] = self.elements['gen'][gen]['mvar_max']
            data[4] = self.elements['gen'][gen]['mvar_min']
            data[5] = self.elements['gen'][gen]['voltage']
            data[8] = self.elements['gen'][gen]['mw_max']
            data[9] = self.elements['gen'][gen]['mw_min']
            if self.elements['gen'][gen]['active']:
                data[7] = 1
            else:
                data[7] = 0

        for branch in self.elements['branch']:
            index = self.branchidx[branch]
            data = self.system['branch'][index]
            data[2] = self.elements['branch'][branch]['resistance']
            data[3] = self.elements['branch'][branch]['reactance']
            data[4] = self.elements['branch'][branch]['charging']
            data[5] = self.elements['branch'][branch]['mva_max']
            data[6] = self.elements['branch'][branch]['mva_max']
            data[7] = self.elements['branch'][branch]['mva_max']
            data[8] = self.elements['branch'][branch]['turns_ratio']
            data[9] = self.elements['branch'][branch]['phase_angle']
            if self.elements['branch'][branch]['active']:
                data[10] = 1
            else:
                data[10] = 0



    def create(self):
        """Initialize the power system under study."""
        with self.__lock:
            self._pcf_to_dict()
        self.__solve()
        return
    
    def query(self):
        """Implements the query command interface for receiving the
        available system tags when requested by field devices or probes.
        When a query command is received, all available PyPower points
        of the system under test will be collected and returned."""

        if self.debug:
            print('Processing query')
        tags = ''
        with self.__lock:
            for dev_type in self.elements:
                for dev_name,fields in self.elements[dev_type].items():
                    for field,value in fields.items():
                        tags += dev_name + '.' + field + ','
        msg = 'ACK={}'.format(tags)
        return msg

    def read(self, tag):
        """Implements the read command interface for receiving the PyPower
        model when commanded by field devices or probes. When a read command
        is received, the internal representation of the specific element of
        the system under test will be collected and returned."""

        if self.debug:
            print('Processing read request')
        try:
            element, field = tag.strip('\x00').split('.')
            devtype, id_ = element.split('-', 1)
            if devtype == "generator":
                devtype = "gen"
            with self.__lock:
                value = self.elements[devtype][element][field]
            reply = element + '.' + field + ':' +str(value).lower()
            msg = 'ACK={}'.format(reply) if str(value) else 'ERR={}'.format("No data found.")
        except Exception as err:
            msg = 'ERR={}'.format(err)
        return msg

    def write(self, tags):
        """Implements the write command interface for updating the PyPower
        model when commanded by field devices or probes. When a write command
        is received, the internal representation of the system under test will
        be updated and published after the solve completes."""

        if self.debug:
            print('Processing write command')

        try:
            for tag, value in tags.items():
                if self.debug:
                    print('PyPower.write ---- received write command for: '
                        + tag + ' with value ' + value + ' -> ', end='')

                element, field = tag.strip('\x00').split('.')
                devtype, _ = element.split('-', 1)
                devtype = 'gen' if 'gen' in devtype else devtype

                if value in ['true', 'false']:
                    if field in ['slack','active']:
                        val = True if value == 'true' else False
                    else:
                        return 'ERR=Invalid value for tag: {}'.format(tag)
                else:
                    val = float(value)

                with self.__lock:
                    self.elements[devtype][element][field] = val
                    self._dict_to_pcf()

            self.__solve()

            msg = 'ACK=Success processing PyPower write command'
        except Exception as err:
            print("ERROR: Provider failed to process write message with exception: %s" % str(err))
            msg = 'ERR=Provider failed to process write message with exception: {} on line {}'.format(err,sys.exc_info()[-1].tb_lineno)

        return msg

    def periodic_publish(self):
        """Loop that calls `self.publish` every second."""

        while True:
            msg = self._pack_data()
            try:
                self.publish(msg)
            except Exception as e:
                print("provider periodic_publish error: {}".format(e))
            time.sleep(1)

    def _add_noise(self,value):
        """Generate a random number between 0.99 and 1.01. Used to add noise to the system."""
        if value == 0.0:
            rand_num = random.uniform(-0.1,0.1)
        else:
            rand_num = value * random.uniform(0.99, 1.01)
        return rand_num

    def _pack_data(self):
        """Pack system data into a string"""
        # Creating temp system to add noise to the steady state
        # power system data. Doing this so that the self.elements
        # is preserved.

        with self.__lock:
            temp_elem = copy.deepcopy(self.elements)
        
        for dev_name,fields in temp_elem['bus'].items():
            for field,value in fields.items():
                if field not in ['base_kv','active','slack']:
                    temp_elem['bus'][dev_name][field] = self._add_noise(temp_elem['bus'][dev_name][field])
        for dev_name,fields in temp_elem['load'].items():
            for field,value in fields.items():
                if field not in ['bus','active']:
                    temp_elem['load'][dev_name][field] = self._add_noise(temp_elem['load'][dev_name][field])

        for dev_name,fields in temp_elem['shunt'].items():
            for field,value in fields.items():
                if field not in ['bus','active']:
                    temp_elem['shunt'][dev_name][field] = self._add_noise(temp_elem['shunt'][dev_name][field])

        for dev_name,fields in temp_elem['gen'].items():
            for field,value in fields.items():
                if field not in ['mw_max','mw_min','mvar_max','mvar_min','active']:
                    temp_elem['gen'][dev_name][field] = self._add_noise(temp_elem['gen'][dev_name][field])
        msg = ''
        for dev_type in temp_elem:
            for dev_name,fields in temp_elem[dev_type].items():
                for field,value in fields.items():
                    msg += '{}.{}:{},'.format(dev_name, field, str(value).lower())
        return msg


    def __solve(self, **kwargs):
        options = ppoption(**kwargs)
        options['OUT_ALL'] = 1
        #options['PF_MAX_IT'] = 20
        #enforce q lims seems to cause index error for 300 bus case
        #options['ENFORCE_Q_LIMS'] = 1
        try:
            results, success = runpf(self.system, options)
            if not success:
                print('Failed to solve for new power system')
                self.elements['bus']['bus-alert']['active'] = True
                return
            #sys.stdout.flush()
            with self.__lock:
                self.system = results
                self._pcf_to_dict()
                self.elements['bus']['bus-alert']['active'] = False
        except Exception as err:
            print('Failed to solve for new power system')
            print('Error: ' + str(err))
            self.elements['bus']['bus-alert']['active'] = True
        return
