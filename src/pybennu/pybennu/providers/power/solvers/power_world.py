"""
NAME
    PowerWorld steady-state power solver provider.

DESCRIPTION
    This module instantiates a PowerWorld power solver for use in
    SCEPTRE environments. The PowerWorld class inherits from the
    Provider class which abstracts away the communication
    interface code that all providers require.

FUNCTIONS
    The following functions are implemented by the PowerWorld module.

    __init__(self, server_endpoint, publish_endpoint, case, oneline=None,
                 debug='false', noise='normal')

        Instance initialization function. Configures the Provider
        parent class, initializes the PowerWorld interface, loads the given
        PowerWorld case file (*.PWB), and initializes the system.

        Args:
            * name - name to reference this solver by in SCEPTRE
            * server_endpoint - IP address for system updates
            * publish_endpoint - IP address for publishing
            * case - path to PowerWorld case configuration file
            * oneline - path to PowerWorld oneline configuration file
            * debug - turn on or off debug output
            * noise - level of noise to generate (uniform, normal,
              lognormal, gamma, weibull)

        Returns: none

    create()

        Creates the internal representation of the system under test as a
        Python dictionary object, initializing it with the names of each
        element in the PowerWorld circuit.

        Args: none

        Returns: none

    query()

        Collects the entire sytem state when a query message received by
        `Provider`. If no errors, returns the entire system state string as the
        message object.

        Args: none

        Returns:
            * A message with the system state data or an error message

    read()

        Read a specific data object (tag) in the power system.

        Args:
            * tag - The tag (<element>.<property>) to read

        Returns:
            * A message with data for the specific tag or an error message

    write(self, tag, value)

        Write a new value for a specific tag by using the PowerWorld API to
        modify the system state.

        Args:
            * tag - The tag (<element>.<property>) to modify in PowerWorld
            * value - The value to update the object in PowerWorld

        Returns:
            * An ACK or error message

    periodic_publish():
        Loop that calls `publish()` every 1 second. Called during __init__
        or `Provider`.

        Args: none

        Returns: none

    _get_system_state()

        Gets the most current system state from `self.elements` and populates the
        `self.temp_sytem` with additional noise if desired.

        Args: none

        Returns:
            * containers - Raw data in dictionaries reflecting the system state

    _pack_data():

        Takes in raw data containers (i.e., dictionaries) from the current system
        state and preps the data with the correct formatting for publish
        messages.

        Args:
            * containers - Raw data reflecting the system state

        Returns:
            * A text string of all the data in the form
              "<tag>:<value>,<tag>:<value>,...". For example:
              "load-1_bus-101.mw:87.55, bus-101.active:true, ..."

    __update()

        Reads circuit elements from PowerWorld and updates the internal
        representation of the system under test. Blocked by the mutex lock if
        a publish message is currently being built in the `periodic_publish`
        method.

        Args: none

        Returns: none

    _change_multi_element()

        Changes the state of PowerWorld objects in the system under test. This
        method requires you to specify the fields that uniquely IDs the object
        and the field you want to changes (e.g., active), as well as the
        corresponding values for the fields.

        Args:
            * devtype - The device type that you want to receive data on.
            Possible options include "bus", "gen", "load", "shunt", and
            "branch"
            * fields - an array of the case object field types

        Returns: none

    _get_multi_element()
        Gets all the physical data associated with a given object. Available
        case object fields are identified in the self.case_object_fields
        variable, which defines what data will be returned by PowerWorld.

        Args:
            * devtype - The device type that you want to receive data on.
            Possible options include "bus", "gen", "load", "shunt", and
            "branch"

        Returns:
            * A dictionary with the parsed data received from PowerWorld

    _add_noise()

        This method is used to add noise to the steady-state power system data.
        Multiplies element data by random number between 0.99 and 1.01 over a
        normal or uniform distribution.

        Args:
            * value - Steady-state element floating point data values

        Returns:
            * Element floating point data value with added noise

    _close()

        Closes the PowerWorld case currently running.

        Args: none

        Returns: none

NOTES
    The PowerWorld COM interface case object fields are well documented. You
    can export the case object fields from the PowerWorld UI by clicking on
    the "Window" tab of the ribbon, then on the right side of the ribbon
    selecting "Export Case Object Fields..." under the "Auxiliary Files"
    section.

    You can also find more information regarding PowerWorld's COM interface
    by visiting the site below.

    https://www.powerworld.com/WebHelp/Default.htm
"""

import copy
import random
import threading
import time

import pythoncom
import win32com.client
from win32com.client import VARIANT

from pybennu.distributed.provider import Provider

class PowerWorld(Provider):
    """PowerWorld-based implementation of an electric power transmission
    system"""

    def __init__(self, server_endpoint, publish_endpoint, case_filename,
                 oneline_filename=None, objects_filename=None,
                 debug=False, noise='normal'):
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock = threading.Lock()
        self.__has_failed = False
        self.__has_purged = False
        self.debug = debug

        # probablity density function for random noise
        if noise not in ('none', 'uniform', 'normal', 'lognormal', 'gamma', 'weibull'):
            raise ValueError('Noise PDF not a valid type! Choose from uniform, '
                             'normal, lognormal, gamma, weibull')
        else:
            if noise.lower() == 'none':
                self.noise = None
            else:
                self.noise = noise

        # define SCADA field mappings for each device type
        self.scada_fields = {'bus': ['busnum', 'base_kv', 'slack', 'active',
                                     'voltage', 'angle', 'gen_mw',
                                     'gen_mvar', 'load_mw', 'load_mvar',
                                     'shunt_mvar'],
                             'gen': ['busnum', 'id', 'active', 'voltage', 'mw',
                                     'mvar', 'mw_max', 'mw_min', 'mvar_max',
                                     'mvar_min', 'agc', 'avr'],
                             'load': ['busnum', 'id', 'active', 'mw',
                                      'mvar'],
                             'shunt': ['busnum', 'id', 'active', 'actual_mvar',
                                       'nominal_mvar'],
                             'branch': ['busnumfrom', 'busnumto',
                                        'circuit', 'active', 'current_from', 'current',
                                        'target_mw', 'target_mvar', 'source_mw',
                                        'source_mvar', 'mva', 'mva_max',
                                        'phase_angle', 'charging', 'r', 'x'],
                             'transformer': ['busnumfrom', 'busnumto',
                                             'circuit', 'active', 'voltage_source',
                                             'voltage_target', 'nominal_kv_source',
                                             'nominal_kv_target', 'phase_angle_source',
                                             'voltage_angle_target', 'current_source',
                                             'current_target', 'source_mw', 'target_mw',
                                             'source_mvar', 'target_mvar', 'tap']}

        # define PW fields for each device type
        self.case_object_fields = {'bus': ['busnum', 'nomkv', 'slack', 'status',
                                           'buspuvolt', 'busangle', 'genmw',
                                           'genmvar', 'loadmw', 'loadmvar',
                                           'shuntmvar'],
                                   'gen': ['busnum', 'id', 'status', 'vpu', 'mw',
                                           'mvar', 'mwmax', 'mwmin', 'mvarmax',
                                           'mvarmin', 'agc', 'avr'],
                                   'load': ['busnum', 'id', 'status', 'mw',
                                            'mvar'],
                                   'shunt': ['busnum', 'id', 'status', 'mvar',
                                             'mvarnom'],
                                   'branch': ['busnumfrom', 'busnumto',
                                              'circuit', 'status', 'ampsfrom', 'ampsto',
                                              'mwto', 'mvarto', 'mwfrom',
                                              'mvarfrom', 'mvato', 'mvamax',
                                              'vanglediff',
                                              'chargingampstoopenendedfrom',
                                              'r', 'x'],
                                   'transformer': ['busnumfrom', 'busnumto',
                                                   'circuit', 'status', 'vpufrom',
                                                   'vputo', 'nomkvfrom',
                                                   'nomkvto', 'vanglefrom',
                                                   'vangleto', 'ampsfrom',
                                                   'ampsto', 'mwfrom', 'mwto',
                                                   'mvarfrom', 'mvarto', 'tap']}

        # used to map circuit elements to their names
        self.elements = {}
        self.temp_system = {}

        # connect to SimAuto
        self.pw = win32com.client.Dispatch('pwrworld.SimulatorAuto')

        # make UI visible
        self.pw.UIVisible = 1

        # open PowerWorld case file
        try:
            self.pw.OpenCase(case_filename)
        except IOError:
            print('Failed to open the Power World case file: ' + case_filename + '.')

        # open PowerWorld oneline file
        if oneline_filename:
            try:
                self.pw.RunScriptCommand('OpenOneline({}, '', YES)'.format(oneline_filename))
            except IOError:
                print('Failed to open the Power World oneline file: ' + oneline_filename + '.')

        # enter run mode and animate oneline
        self.pw.RunScriptCommand('EnterMode(RUN)')
        self.pw.RunScriptCommand('Animate(YES)')

        with open(objects_filename, 'r') as f_:
            lines = f_.readlines()
        self.need = []
        for line in lines:
            line = line.strip()
            devtype, id_ = line.split('-', 1)
            devtype = 'gen' if 'gen' in devtype else devtype
            self.need.append((line,devtype,id_))

        # initialze the system under test using data from PowerWorld
        self.create()

    def create(self):
        """Creates the internal representation of the system under test as a
        Python dictionary object, initializing it with the names of each
        element in the self.need points list."""

        if self.debug:
            print('Creating system under test')
        for i in self.need: # ex: i = ('branch-1_1001-1002', 'branch', '1_1001-1002')
            data = self._get_single_element(i[1], i[2])
            self.elements[i[0]] = data

    def query(self):
        """Implements the query command interface for receiving the
        available system tags when requested by field devices or probes.
        When a query command is received, all available PowerWorld points
        of the system under test will be collected and returned."""

        if self.debug:
            print('Processing query')
# TODO: this returns too much data right now. Instead, we'll just return
# the subset of tags in self.need
#        for i in self.case_object_fields:
#            self._get_multi_element('branch')
        tags = ''
        for item in self.need:
            fields = self.scada_fields[item[1]]
            tags += ','.join(['{}.{}'.format(item[0],f) for f in fields])
        msg = 'ACK={}'.format(tags)
        return msg

    def read(self, tag):
        """Implements the read command interface for receiving the PowerWorld
        model when commanded by field devices or probes. When a read command
        is received, the internal representation of the specific element of
        the system under test will be collected and returned."""

        if self.debug:
            print('Processing read request')
        try:
            element, field = tag.strip('\x00').split('.')
            devtype, id_ = element.split('-', 1)
            devtype = 'gen' if 'gen' in devtype else devtype
            data = self._get_single_element(devtype, id_)
            value = data[field]
            msg = 'ACK={}'.format(value) if value else 'ERR={}'.format("No data found.")
        except Exception as err:
            msg = 'ERR={}'.format(err)
        return msg

    def write(self, tags):
        """Implements the write command interface for updating the PowerWorld
        model when commanded by field devices or probes. When a write command
        is received, the internal representation of the system under test will
        be updated and published after the solve completes."""

        if self.debug:
            print('Processing write command')
            
        if self.__has_failed:
            return 'ERR=Solution in failed state.'
        else:
            try:
                for tag, value in tags.items():
                    if self.debug:
                        print('PowerWorld.write ---- received write command for: '
                            + tag + ' with value ' + value + ' -> ', end='')

                    element, field = tag.strip('\x00').split('.')
                    devtype, id_ = element.split('-', 1)
                    devtype = 'gen' if 'gen' in devtype else devtype

                    if value in ['true', 'false']:
                        if field in ['slack', 'agc', 'avr']:
                            val = 'YES' if value == 'true' else 'NO'
                        elif field == 'active':
                            if devtype == 'bus':
                                val = 'Connected' if value == 'true' else 'Disconnected'
                            else:
                                val = 'Closed' if value == 'true' else 'Open'
                        else:
                            return 'ERR=Invalid value for tag: {}'.format(tag)
                    else:
                        val = value

                    self._change_single_element(devtype, id_, field, val)

                #try to solve power flow after update, handle failure
                status = self.pw.RunScriptCommand('SolvePowerFlow(RECTNEWT)')
                if any(errMsg in status[0].lower() for errMsg in ['error', 'err']):
                    print(f'{status[0]}')
                    self.__has_failed = True
                    msg = 'ERR=Failed to find solution. Check PowerWorld log for details.'
                else:
                    self.__update()
                    msg = 'ACK=Success processing PowerWorld write command'

            except Exception as err:
                print("ERROR: Provider failed to process write message with exception: %s" % str(err))
                msg = 'ERR=Provider failed to process write message with exception: {}'.format(err)

            return msg

    def periodic_publish(self):
        """Loop that calls `self.publish` every second."""

        while True:
            state = self._get_system_state()
            msg = self._pack_data(state)
            try:
                self.publish(msg)
            except Exception as e:
                print("provider periodic_publish error: {}".format(e))
            time.sleep(1)
            
    def _value_type(self, value):
        # Utility function - return type (class) of value
        return float if '.' in value else (bool if value.lower() in ['true', 'false'] else int)

    def _get_system_state(self):
        """Gets the system state as self.elements. If noise is required, a
        deep copy of the current self.elements is created and noise is added"""
        
        if self.debug:
            print("Collecting latest system state")
        
        #reset state for failed solution
        if self.__has_failed:
            if not self.__has_purged:
                with self.__lock:
                    reset_value = {float: '0.0', int: '0', bool: 'false'}
                    self.elements = {name: {field: reset_value[self._value_type(value)] for field, value in fields.items()} for name, fields in self.elements.items()}
                self.__has_purged = True
            state = self.elements
            return state

        if self.noise:
            with self.__lock:
                # creating temp system to add noise to the steady state power system
                # data so that `self.elements` is preserved.
                temp_elem = copy.deepcopy(self.elements)
            for name,data in temp_elem.items(): # ex: i={'bus-1001': {...},...}
                for field,value in data.items():
                    try:
                        val = float(value)
                        temp_elem[name][field] = str(self._add_noise(val))
                    except ValueError:
                        continue
            state = temp_elem
        else:
            state = self.elements
        return state

    def _pack_data(self, state):
        """Preps data with the correct formatting for publish messages."""

        if self.debug:
            print("Packing data")
        with self.__lock:
            str_data = ''
            # eg. {'bus-101': {...}, 'branch-1_1001-2002': {...}, ...}
            for name,data in state.items():
                for field,value in data.items():
                    str_data += '{}.{}:{},'.format(name, field, value)
        return str_data

    def __update(self):
        """Reads circuit elements from PowerWorld and updates the internal
        representation of the system under test."""

        if self.debug:
            print('Updating system under test')

        # if this solver is self publishing, the periodic publish will run in a
        # separate thread, so we need a mutex around reading and writing data
        # from the internal representation of the system under test. This lock
        # is also used in the get_publish_message function.

        with self.__lock:
            for i in self.need: # ex: i = ('branch-1_1001-1002', 'branch', '1_1001-1002')
                data = self._get_single_element(i[1], i[2])
                self.elements[i[0]] = data

    # TODO: was going to use this to build a list of every <tag> available in the
    # pw system to return for 'query()', but it's too much data. For now,
    # we'll just return all the points in the internal self.elements
    def _get_multi_element(self, devtype):
        """Method to get the all the physical data associated with a given
        object. Available case object fields are identified in the
        self.case_object_fields variable, which defines what data will be
        returned by PowerWorld."""

        field_list = self.case_object_fields[devtype]
        fieldarray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, field_list)
        results = self.pw.GetParametersMultipleElement(devtype, fieldarray, '')

        # Need to be smarter here to differentiate errors from no results
        if results[0] != '':
            items = None
        else:
            data = results[1]
            items = []
            if devtype == 'bus':
                items = ['bus-{}'.format(i.strip()) for i in data[0]]
            elif devtype in ['gen', 'load', 'shunt']:
                items = ['{}-{}_bus-{}'.format(devtype,i.strip(),j.strip()) for i in data[1] for j in data[0]]
            elif devtype == 'branch':
                items = ['branch-{}_{}-{}'.format(i.strip(),j.strip(),k.strip()) for i in data[2] for j in data[0] for k in data[1]]
            else:
                items = []
        return items

    def _change_single_element(self, devtype, id_, field, value):
        #need to get current values first
        # Retrieves parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and zeroes and should be
        # the same length as field_list
        field_list, value_list = self._get_field_value_lists(devtype, id_)
        fieldarray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, field_list)
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        results = self.pw.GetParametersSingleElement(devtype, fieldarray, valuearray)
        
        #update value list to have actual values
        value_list = list(results[1])

        #change the value that we are interested in 
        # Sets parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and change value and
        # should be the same length as field_list
        scada_field_list = self.scada_fields[devtype]
        idx = scada_field_list.index(field)
        value_list[idx] = value
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        results = self.pw.ChangeParametersSingleElement(devtype, fieldarray, valuearray)
        if results[0] != '':
            raise AttributeError('Error executing command!')
        else:
            return True

    def _get_single_element(self, devtype, id_):
        field_list, value_list = self._get_field_value_lists(devtype, id_)
        fieldarray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, field_list)
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        # Retrieves parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and zeroes and should be
        # the same length as field_list
        results = self.pw.GetParametersSingleElement(devtype, fieldarray, valuearray)
        if results[0] != '':
            raise AttributeError('Error executing command!')
        else:
            # trim whitespace and fix 'None'
            data = list((i.strip() if i else '0.0') for i in results[1])
            data = [x.replace('YES','true').replace('NO','false').replace('Connected','true').replace('Disconnected','false').replace('Closed','true').replace('Open','false') for x in data]
            # fix up field list to match SCADA side
            field_list = self.scada_fields[devtype]
            # zip fields+data into dictionary e.g. {'x': 1, 'y': 2, ...}
            element_data = dict(zip(field_list, data))
            return element_data

    def _parse_id(self, id_):
        # id_ = 1_1001-2002
        parts = [i.split('_') for i in id_.split('-')] # [['1', '1001'], ['2002']]
        flat = [i for sub in parts for i in sub] # ['1', '1001', '2002']
        return flat

    def _get_field_value_lists(self, devtype, id_):
        field_list = self.case_object_fields[devtype]
        value_list = [0] * len(field_list)
        parts = self._parse_id(id_)
        if devtype == 'branch':  # id_ = 1_1001-2002
            value_list[0] = parts[1] # src
            value_list[1] = parts[2] # dst
            value_list[2] = parts[0] # id (circuit)
        elif devtype in ['gen','load','shunt']: # id_ = 1_bus-1001
            value_list[0] = parts[2] # busno
            value_list[1] = parts[0] # id
        else: # id_ = 1001
            value_list[0] = id_
        return (field_list, value_list)

    def _add_noise(self, value):
        """Generates a random number between 0.99 and 1.01 using the defined
        pdf and returns system values with added noise."""

        if value == 0.0:
            noise = random.uniform(-0.1, 0.1)
        else:
            if self.noise == 'uniform':
                noise = value * random.uniform(0.99, 1.01)
            elif self.noise == 'normal':
                noise = value * random.normalvariate(1, 0.01)
        return noise

    def _close(self):
        """Close the PowerWorld case."""

        self.pw.CloseCase()
        return True
