'''
NAME
    PowerWorld Dynamic Studio-based power solver provider.

DESCRIPTION
    This module instantiates a PowerWorld Dynamic Studio-based power solver
    for use in SCEPTRE environments. The PowerWorldDynamics class inherits
    from the KronosProvider class which abstracts away the communication
    interface code that all providers require.

    Because this is a dynamic power simulation, this provider continually
    self publishes as PowerWorld calculates new results.

    Similar to other providers, a local copy of the current system state is
    maintained. However, it differs from other providers in that the Protobuf
    messages are not used for the local copy.

    As updates come in to be processed, they are directly sent to PowerWorld
    Dynamic Studio to be included in the current dynamic analysis being done.

TODO
    * Support setting generator real power output in _process_update.
      * For SunLamp, it needs to support receiving a percentage value that
        represents percent of maximum output, which means we also need to be
        able to request the max output setting for each generator when the
        provider starts up.
'''

import argparse, math, sys, threading, time

from pybennu.distributed.provider import Provider
import pybennu.distributed.swig._Endpoint as E

from pybennu.providers.power.solvers.pwds.mock_api import PWDSAPI as MOCK_PWDSAPI
from pybennu.providers.power.solvers.pwds.pwds_api import PWDSAPI


class PowerWorldDynamics(Provider):
    def __init__(self, server_endpoint, publish_endpoint, pwds_endpoint, debug, **kwargs):
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock  = threading.Lock()
        self.__debug = debug

        need = None

        if 'objects-file' in kwargs:
            need = {'bus': [], 'generator': [], 'load': [], 'shunt': [], 'branch': []}

            with open(kwargs['objects-file'], 'r') as f:
                for _, line in enumerate(f):
                    line = line.strip()
                    devtype, _ = line.split('-', 1)

                    need[devtype].append(line)

        sleep_seconds = 10

        while(True):
            try:
                if 'mock' == pwds_endpoint:
                    self.api = MOCK_PWDSAPI(('localhost', 5557))
                else:
                    self.api = PWDSAPI((pwds_endpoint, 5557))

                self.dictionary = self.api.get_system_dictionary()
                break
            except Exception as ex:
                print(f'Unable to connect to PWDS API on {pwds_endpoint}.')
                print(f'Trying again in {sleep_seconds} seconds...')
                print(f'Exception was: {ex}')

                time.sleep(sleep_seconds)

        self.elements = {}
        self.tags     = []

        self.bus_name    = lambda x: f'bus-{x.number}'
        self.gen_name    = lambda x: f'generator-{x.device_id}_bus-{x.bus}'
        self.load_name   = lambda x: f'load-{x.device_id}_bus-{x.bus}'
        self.shunt_name  = lambda x: f'shunt-{x.device_id}_bus-{x.bus}'
        self.branch_name = lambda x: f'branch-{x.circuit_id}_bus-{x.from_bus}-{x.to_bus}'

        if need:
            self.dictionary['buses'][:]    = [x for x in self.dictionary['buses']    if self.bus_name(x)    in need['bus']]
            self.dictionary['gens'][:]     = [x for x in self.dictionary['gens']     if self.gen_name(x)    in need['generator']]
            self.dictionary['loads'][:]    = [x for x in self.dictionary['loads']    if self.load_name(x)   in need['load']]
            self.dictionary['shunts'][:]   = [x for x in self.dictionary['shunts']   if self.shunt_name(x)  in need['shunt']]
            self.dictionary['branches'][:] = [x for x in self.dictionary['branches'] if self.branch_name(x) in need['branch']]

        for bus in self.dictionary['buses']:
            name = self.bus_name(bus)

            self.elements[name] = bus

            self.tags.append(f'{name}.number')
            self.tags.append(f'{name}.active')
            self.tags.append(f'{name}.voltage')
            self.tags.append(f'{name}.voltage_angle')
            self.tags.append(f'{name}.freq')
            self.tags.append(f'{name}.base_kv')
            #self.tags.append(f'{name}.gen_mw')
            #self.tags.append(f'{name}.gen_mvar')
            #self.tags.append(f'{name}.load_mw')
            #self.tags.append(f'{name}.load_mvar')
            #self.tags.append(f'{name}.shunt_mvar')

        for gen in self.dictionary['gens']:
            name = self.gen_name(gen)

            self.elements[name] = gen

            self.tags.append(f'{name}.bus')
            self.tags.append(f'{name}.active')
            self.tags.append(f'{name}.voltage')
            self.tags.append(f'{name}.voltage_setpoint')
            self.tags.append(f'{name}.voltage_angle')
            self.tags.append(f'{name}.current')
            self.tags.append(f'{name}.base_kv')
            self.tags.append(f'{name}.mw')
            self.tags.append(f'{name}.mvar')
            self.tags.append(f'{name}.mw_max')
            self.tags.append(f'{name}.mw_min')
            self.tags.append(f'{name}.freq')

        for load in self.dictionary['loads']:
            name = self.load_name(load)

            self.elements[name] = load

            self.tags.append(f'{name}.bus')
            self.tags.append(f'{name}.active')
            self.tags.append(f'{name}.voltage')
            self.tags.append(f'{name}.mw')
            self.tags.append(f'{name}.mvar')
            self.tags.append(f'{name}.current')

        for shunt in self.dictionary['shunts']:
            name = self.shunt_name(shunt)

            self.elements[name] = shunt

            self.tags.append(f'{name}.bus')
            self.tags.append(f'{name}.active')
            self.tags.append(f'{name}.nominal_mvar')
            self.tags.append(f'{name}.actual_mvar')

        for branch in self.dictionary['branches']:
            name = self.branch_name(branch)

            self.elements[name] = branch

            self.tags.append(f'{name}.source')
            self.tags.append(f'{name}.target')
            self.tags.append(f'{name}.circuit')
            self.tags.append(f'{name}.active')
            self.tags.append(f'{name}.source_mw')
            self.tags.append(f'{name}.source_mvar')
            self.tags.append(f'{name}.target_mw')
            self.tags.append(f'{name}.target_mvar')
            self.tags.append(f'{name}.mva')
            self.tags.append(f'{name}.mva_max')
            self.tags.append(f'{name}.current')


    def get_publish_message(self):
        msg = ''

        system = self.api.get_system_data(
            self.dictionary['buses'],
            self.dictionary['gens'],
            self.dictionary['loads'],
            self.dictionary['shunts'],
            self.dictionary['branches'],
        )

        for b in system['buses']:
            name = self.bus_name(b)

            msg += f'{name}.number:{b.number},'
            msg += f'{name}.voltage:{b.voltage_pu},'
            msg += f'{name}.voltage_angle:{b.voltage_angle},'
            msg += f'{name}.freq:{b.freq},'
            msg += f'{name}.active:{str(b.status != 0).lower()},'
            msg += f'{name}.base_kv:{self.elements[name].nominal_kv},'

        for g in system['gens']:
            name = self.gen_name(g)

            msg += f'{name}.bus:{g.bus},'
            msg += f'{name}.active:{str(g.status != 0).lower()},'
            msg += f'{name}.voltage:{g.voltage_pu},'
            msg += f'{name}.voltage_setpoint:{g.voltage_pu_setpoint},'
            msg += f'{name}.voltage_angle:{g.voltage_angle},'
            msg += f'{name}.base_kv:{g.voltage_kv},'
            msg += f'{name}.mw:{g.mw},'
            msg += f'{name}.mvar:{g.mvar},'
            msg += f'{name}.mw_max:{self.elements[name].mw_max},'
            msg += f'{name}.mw_min:{self.elements[name].mw_min},'
            msg += f'{name}.freq:{g.freq},'

            phase_kv = g.voltage_pu * g.voltage_kv * math.sqrt(3)

            if phase_kv != 0:
                i = (g.mw * 1000) / phase_kv
                msg += f'{name}.current:{i},'
            else:
                msg += f'{name}.current:{0},'


        for l in system['loads']:
            name = self.load_name(l)

            msg += f'{name}.bus:{l.bus},'
            msg += f'{name}.active:{str(l.status != 0).lower()},'
            msg += f'{name}.voltage:{l.voltage_pu},'
            msg += f'{name}.mw:{l.mw},'
            msg += f'{name}.mvar:{l.mvar},'
            denom = l.voltage_kv * l.voltage_pu * math.sqrt(3)
            if denom != 0:
                msg += f'{name}.current:{(l.mw * 1000) / denom},'
            else:
                msg += f'{name}.current:{0},'

        for s in system['shunts']:
            name = self.shunt_name(s)

            msg += f'{name}.bus:{s.bus},'
            msg += f'{name}.active:{str(s.status != 0).lower()},'
            msg += f'{name}.nominal_mvar:{s.nominal_mvar},'
            msg += f'{name}.actual_mvar:{s.mvar},'

        for b in system['branches']:
            name = self.branch_name(b)

            msg += f'{name}.active:{str(b.status != 0).lower()},'
            msg += f'{name}.source:{b.from_bus},'
            msg += f'{name}.target:{b.to_bus},'
            msg += f'{name}.circuit:{b.circuit_id},'
            msg += f'{name}.source_mw:{b.mw_from},'
            msg += f'{name}.source_mvar:{b.mvar_from},'
            msg += f'{name}.target_mw:{b.mw_to},'
            msg += f'{name}.target_mvar:{b.mvar_to},'
            msg += f'{name}.mva:{b.mva_to},'
            msg += f'{name}.mva_max:{self.elements[name].mva_limit},'
            msg += f'{name}.current:{b.amps_to},'

        return msg


    def periodic_publish(self):
        while True:
            if self.__debug:
                print('Periodically publishing data')

            self.__publish_data()
            time.sleep(2)


    def __publish_data(self):
        with self.__lock:
            msg = self.get_publish_message()

            if self.__debug:
                print(msg)

            self.publish(msg)


    # Must return 'ACK=tag1,tag2,...' or 'ERR=<error message>'
    def query(self):
        if self.__debug:
            print('PowerWorldDynamics::query ---- received query request')

        result = 'ACK='
        for t in self.tags:
            result += t + ','

        return result


    # Must return 'ACK=<value>' or 'ERR=<error message>'
    def read(self, tag):
        if self.__debug:
            print('PowerWorldDynamics::read ---- received read for tag %s' % tag)

        with self.__lock:
            if tag in self.tags:
                name, field = tag.split('.')
                ttype = name.split('-')[0]

                # TODO: move this down into if statements, and provide
                # empty array for all object types not needed.
                system = self.api.get_system_data(
                    self.dictionary['buses'],
                    self.dictionary['gens'],
                    self.dictionary['loads'],
                    self.dictionary['shunts'],
                    self.dictionary['branches'],
                )

                if 'bus' == ttype:
                    for b in system['buses']:
                        if f'bus-{b.number}' == name:
                            if field in b:
                                return f'ACK={b[field]}'
                            else:
                                return f'ERR={field} not found in {name}'
                    return f'ERR=Bus {name} not found'
                elif 'generator' == ttype:
                    for g in system['gens']:
                        if f'generator-{g.device_id}_bus-{g.bus}' == name:
                            if field in g:
                                return f'ACK={g[field]}'
                            else:
                                return f'ERR={field} not found in {name}'
                    return f'ERR=Generator {name} not found'
                elif 'load' == ttype:
                    for l in system['loads']:
                        if f'load-{l.device_id}_bus-{l.bus}' == name:
                            if field in l:
                                return f'ACK={l[field]}'
                            else:
                                return f'ERR={field} not found in {name}'
                    return f'ERR=Load {name} not found'
                elif 'shunt' == ttype:
                    for s in system['shunts']:
                        if f'shunt-{s.device_id}_bus-{s.bus}' == name:
                            if field in s:
                                return f'ACK={s[field]}'
                            else:
                                return f'ERR={field} not found in {name}'
                    return f'ERR=Shunt {name} not found'
                elif 'branch' == ttype:
                    for b in system['branches']:
                        if f'branch-{b.circuit_id}_bus-{b.from_bus}-{b.to_bus}' == name:
                            if field in b:
                                return f'ACK={b[field]}'
                            else:
                                return f'ERR={field} not found in {name}'
                    return f'ERR=Branch {name} not found'
                else:
                    return f'ERR=Unknown device type {ttype}'
            else:
                return 'ERR=Tag not found'


    # Must return 'ACK=<success message>' or 'ERR=<error message>'
    def write(self, tags):
        with self.__lock:
            for tag, value in tags.items():
                if self.__debug:
                    print('PowerWorldDynamics::write ---- received write for tag %s -- %s' % (tag, value))

                if tag in self.tags:
                    name, field = tag.split('.')
                    ttype = name.split('-')[0]

                    if value.lower() == 'true':
                        value = True
                    elif value.lower() == 'false':
                        value = False
                    else:
                        value = float(value)

                    if 'bus' == ttype:
                        return f'ERR={field} is not writable for {name}'
                    elif 'generator' == ttype:
                        if 'active' == field:
                            if True == value:
                                self.api.connect_generator(self.elements[name])
                            else:
                                self.api.disconnect_generator(self.elements[name])
                        elif 'voltage' == field:
                            if 0 != value:
                                self.api.set_generator_pu_voltage(self.elements[name], value)
                        elif 'mw' == field:
                            if 0 != value:
                                self.api.set_generator_mw_output(self.elements[name], value)
                        else:
                            return f'ERR={field} is not writable for {name}'
                    elif 'load' == ttype:
                        if 'active' == field:
                            if True == value:
                                self.api.connect_load(self.elements[name])
                            else:
                                self.api.disconnect_load(self.elements[name])
                        else:
                            return f'ERR={field} is not writable for {name}'
                    elif 'shunt' == ttype:
                        if 'active' == field:
                            if True == value:
                                self.api.connect_shunt(self.elements[name])
                            else:
                                self.api.disconnect_shunt(self.elements[name])
                        else:
                            return f'ERR={field} is not writable for {name}'
                    elif 'branch' == ttype:
                        if 'active' == field:
                            if True == value:
                                self.api.connect_branch(self.elements[name])
                            else:
                                self.api.disconnect_branch(self.elements[name])
                        else:
                            return f'ERR={field} is not writable for {name}'
                    else:
                        return f'ERR=Unknown device type {ttype}'

                    return 'ACK=Success processing PWDS write command'
                else:
                    return f'ERR=Tag {tag} not found'


if __name__ == '__main__':
    try:
        program = 'PowerWorld Dynamic Studio-based power solver provider for bennu'

        parser = argparse.ArgumentParser(description=program)
        parser.add_argument('-d', '--debug', action='store_true',
                            help='print debugging information')
        parser.add_argument('-s', '--server-endpoint', default='tcp://127.0.0.1:5555',
                            help='server listening endpoint')
        parser.add_argument('-p', '--publish-endpoint', default='udp://239.0.0.1:40000',
                            help='publishing endpoint')
        parser.add_argument('-w', '--pwds-endpoint', default='127.0.0.1',
                            help='server IP address of PowerWorld Dynamic Studio (default:127.0.0.1), type "mock" to use a mock API for debugging')
        parser.add_argument('-o', '--objects-file', default='',
                            help='file containing newline-delimited list of device names to publish')

        args = parser.parse_args()

        server_endpoint = E.new_Endpoint()
        publish_endpoint = E.new_Endpoint()
        E.Endpoint_str_set(server_endpoint, args.server_endpoint)
        E.Endpoint_str_set(publish_endpoint, args.publish_endpoint)

        kwargs = {}

        if args.objects_file:
            kwargs['objects-file'] = args.objects_file

        pw = PowerWorldDynamics(server_endpoint, publish_endpoint, args.pwds_endpoint, args.debug, **kwargs)
        pw.run()
    except KeyboardInterrupt:
        sys.exit()
