import logging

import pythoncom
import win32com.client
from win32com.client import VARIANT

from pybennu.providers.utils.helics_helper import HelicsFederate

logger = logging.getLogger('helics')
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.INFO)


class PowerWorldHelics(HelicsFederate):
    """ PowerWorld-based implementation of an electric power transmission
        system
    """

    def __init__(self, case_filename, config,
                 oneline_filename=None, debug=False):
        self.__has_failed = False
        if debug:
            logger.setLevel(logging.DEBUG)

        # Initialize HelicsFederate
        HelicsFederate.__init__(self, config)

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

        # connect to SimAuto
        self.pw = win32com.client.Dispatch('pwrworld.SimulatorAuto')

        # make UI visible
        self.pw.UIVisible = 1

        # open PowerWorld case file
        try:
            self.pw.OpenCase(case_filename)
        except IOError:
            print(f'Failed to open the Power World case file: {case_filename}.')

        # open PowerWorld oneline file
        if oneline_filename:
            try:
                self.pw.RunScriptCommand('OpenOneline({}, '', YES)'.format(oneline_filename))
            except IOError:
                print(f'Failed to open the Power World oneline file: {oneline_filename}.')

        # enter run mode and animate oneline
        self.pw.RunScriptCommand('EnterMode(RUN)')
        self.pw.RunScriptCommand('Animate(YES)')

    # Read tag (value=None)
    # Write tag (value=<bool or double>)
    def tag(self, tag, value=None):
        if value is not None:
            self.write(tag, value)
        else:
            return self.read(tag)

    def write(self, tag, value):
        if self.__has_failed:
            print('Solution is in failed state. Check PowerWorld log for details.')
        else:
            name, field = tag.split('.', 1)
            kind, name = name.split('-', 1)
            kind = 'gen' if 'gen' in kind else kind

            if value in ['true', 'false', True, False]:
                value = str(value).lower()
                if field in ['slack', 'agc', 'avr']:
                    val = 'YES' if value == 'true' else 'NO'
                elif field == 'active':
                    if kind == 'bus':
                        val = 'Connected' if value == 'true' else 'Disconnected'
                    else:
                        val = 'Closed' if value == 'true' else 'Open'
                else:
                    print(f'Error: Invalid value for tag -- {tag}')
            else:
                val = value

            self._change_single_element(kind, name, field, val)

        # try to solve power flow after update, handle failure
        status = self.pw.RunScriptCommand('SolvePowerFlow(RECTNEWT)')
        if any(errMsg in status[0].lower() for errMsg in ['error', 'err']):
            print(f'{status[0]}')
            self.__has_failed = True
            print('Failed to find solution. Check PowerWorld log for details.')
        else:
            print('Successful PowerWorld command: SolvePowerFlow')

    def read(self, tag):
        # tag = line-650632.kW
        # kind  = line
        # name  = 650632
        # field = kW
        name, field = tag.split('.', 1)
        kind, name = name.split('-', 1)
        kind = 'gen' if 'gen' in kind else kind
        pw_data = self._get_single_element(kind, name)
        val = pw_data[field]
        if val.lower() in ['true', 'false']:
            value = True if val.lower() == 'true' else False
        else:
            value = float(val)
        return value

    def _change_single_element(self, kind, name, field, value):
        # Retrieves parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and zeroes and should be
        # the same length as field_list. Need to get current values first using
        # GetParametersSingleElement
        field_list, value_list = self._get_field_value_lists(kind, name)
        fieldarray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, field_list)
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        results = self.pw.GetParametersSingleElement(kind, fieldarray, valuearray)

        # Update value list to have actual values
        value_list = list(results[1])

        # Sets parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and change value and
        # should be the same length as field_list
        scada_field_list = self.scada_fields[kind]
        idx = scada_field_list.index(field)
        value_list[idx] = value
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        results = self.pw.ChangeParametersSingleElement(kind, fieldarray, valuearray)
        if results[0] != '':
            raise AttributeError('Error executing command: ChangeParametersSingleElement')
        else:
            print(f'Successfully changed element: {kind}-{name}.{field} = {value}')

    def _get_single_element(self, kind, name):
        field_list, value_list = self._get_field_value_lists(kind, name)
        fieldarray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, field_list)
        valuearray = VARIANT(pythoncom.VT_VARIANT | pythoncom.VT_ARRAY, value_list)
        # Retrieves parameter data according to the fields specified in field_list.
        # value_list consists of identifying parameter values and zeroes and should be
        # the same length as field_list
        results = self.pw.GetParametersSingleElement(kind, fieldarray, valuearray)
        if results[0] != '':
            raise AttributeError('Error executing command: GetParametersSingleElement')
        else:
            # trim whitespace and fix 'None'
            data = list((i.strip() if i else '0.0') for i in results[1])
            data = [x.replace('YES','true').replace('NO','false').replace('Connected','true').replace('Disconnected','false').replace('Closed','true').replace('Open','false') for x in data]
            # fix up field list to match SCADA side
            field_list = self.scada_fields[kind]
            # zip fields+data into dictionary e.g. {'x': 1, 'y': 2, ...}
            element_data = dict(zip(field_list, data))
            return element_data

    def _parse_name(self, name):
        # name = 1_1001-2002
        parts = [i.split('_') for i in name.split('-')] # [['1', '1001'], ['2002']]
        flat = [i for sub in parts for i in sub] # ['1', '1001', '2002']
        return flat

    def _get_field_value_lists(self, kind, name):
        field_list = self.case_object_fields[kind]
        value_list = [0] * len(field_list)
        parts = self._parse_name(name)
        if kind == 'branch':  # name = 1_1001-2002
            value_list[0] = parts[1] # src
            value_list[1] = parts[2] # dst
            value_list[2] = parts[0] # id (circuit)
        elif kind in ['gen','load','shunt']: # name = 1_bus-1001
            value_list[0] = parts[2] # busno
            value_list[1] = parts[0] # id
        else: # name = 1001
            value_list[0] = name
        return (field_list, value_list)
