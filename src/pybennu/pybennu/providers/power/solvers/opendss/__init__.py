"""
NAME
    OpenDSS-based power solver provider.

DESCRIPTION
    This module instantiates an OpenDSS-based power solver for use in
    SCEPTRE environments. The OpenDSS class inherits from the Provider
    class which abstracts away the communication interface code that all
    providers require.

FUNCTIONS
    The following functions are implemented by the OpenDSS module.


    def __init__(self, server_endpoint, publish_endpoint, filename, **kwargs)
    def query(self)
    def read(self, tag)
    def write(self, tags)
    def periodic_publish(self)
    def __pack_message(self)
    def __create(self)
    def __update(self)
    def __solve(self, first='false')
    def __run_command(self, text)
    def __initialize_phases(self)
    def __get_voltcurpow_1_terminal(self, phases)
    def __get_voltcurpow_2_terminal(self, phases)
    def __compile_circuit(self, filename=None)
    def __save_and_clear_circuit(self)
    def __gen_rand_num(self)

    __init__(name, gryffin_endpoint, filename, **kwargs)

        Instance initialization function. Configures the Publisher
        parent class, initializes the OpenDSS interface, and loads the
        given OpenDSS configuration file.

        Args:
            * server_endpoint - IP address to listen on
            * publish_endpoint - multicast address to publish to
            * filename - path to OpenDSS configuration file
            * **kwargs - expected keyword args include:
              * debug - self-explanatory
              * clear_circuit - save/load circuit to/from file each update
              * jitter - add jitter to published data
              * self_publish - self-explanatory

        Returns: none

    __compile_circuit(filename)

        Compiles the circuit defined at the given filename in OpenDSS. If no
        filename is provided, and clear_circuit is enabled, it will compile
        the latest saved circuit located in the instance's temp directory. If
        clear_circuit is disabled, this is a no-op.

        Args:
            * filename - path to OpenDSS circuit file to compile.

        Returns: none

    __save_and_clear_circuit()

        Saves the current OpenDSS circuit to file in the instance's temp
        directory to be compiled again on the next update by
        __compile_circuit. If clear_circuit is disabled, this is a no-op.

        Args: none

        Returns: none

    create()

        Creates the internal representation of the system under test using the
        data classes defined in this module, initializing it with the names of
        each element in the OpenDSS circuit.

        Args: none

        Returns: none

    pack_message()

        Packs each element in the system under test into a publish
        string message. Blocked by the mutex lock if the internal
        representation of the system under test is being updated in the
        `__update` function.

        Args: none

        Returns:
            * message string to publish

    publish_data()

        Publishes the latest known state of the system under test using
        the `publish` method implemented in the Provider.

        Args: none

        Returns: none

    periodic_publish_data()

        Runs a loop that calls `publish_data` every 2 seconds. This
        function is run in a separate thread in the Provider.

        Args: none

        Returns: none

    _process_update(data)

        Implements the GryffinProvider interface, updating the OpenDSS active
        circuit with updates passed in and triggering a solve. The internal
        representation of the system under test will then be updated and
        published after the solve completes.

        Args:
            * data -  updates to circuit elements.

        Returns:
            * boolean representing if update was successful

    __update()

        Reads circuit elements from OpenDSS and updates the internal
        representation of the system under test. Blocked by the mutex lock if
        a publish message is currently being built in the
        `pack_message` function.

        Args: none

        Returns: none

    __gen_rand_num()

        Generates a random number between 0.99 and 1.01 over a normal
        distribution. This number is used to add noise to the steady-state
        power system data.

        Args: none

        Returns: float randomized between 0.99 and 1.01


    __solve(first)

        Triggers a solve in OpenDSS and checks for any errors. It then calls
        the `__update` function to update the internal representation of the
        system under test.

        Args:
            * first - whether or not this is the first solve being called.

        Returns: none

    __run_command(text)

        Run a command using the text interface of OpenDSS.

        Args:
            * The text of the command you want to execute.

        Returns: none

    __initialize_phases():

        Identify and initialize phases for all elements in the internal
        representation of the system under test.

        Args: none

        Returns: none

    __get_voltcurpow_1_terminal(phases)

        Get the voltage, current, and power data for the currently active
        element. This function is for single terminal elements only.

        Args:
            * The base64-encoded phases associated with the current element

        Returns:
            * A list of the voltage, current, and power (magnitude and angle)

    __get_voltcurpow_2_terminal(phases)

        Get the voltage, current, and power data for the currently active
        element. This function is for two terminal elements only.

        Args:
            * The base64-encoded phases associated with the current element

        Returns:
            * A list of the voltage, current, and power (magnitude and angle)
              for terminal 1
            * A list of the voltage, current, and power (magnitude and angle)
              for terminal 2


NOTES
    This solver uses the OpenDSSDirect.py wrapper around the
    libopendssdirect.so shared object. The OpenDSSDirect.py documentation is
    located at the following URL:

    https://nrel.github.io/OpenDSSDirect.py.

    The low-level OpenDSS interface function descriptions and parameter options
    are documented in a PDF located at the following URL:

    https://sf.net/p/electricdss/code/HEAD/tree/trunk/Distrib/Doc/OpenDSS_Direct_DLL.pdf.
"""

import base64, copy, json, os, random, shutil, tempfile, threading, time

from datetime import datetime
from timeit   import default_timer as timer

import opendssdirect       as dss
import opendssdirect.utils as dss_utils

from pybennu.distributed.provider import Provider
from pybennu.providers.power      import PowerSolverError

import pybennu.providers.power.solvers.opendss.data as models

class OpenDSS(Provider):
    """OpenDSS-based implementation of an electric power distribution system"""

    def __init__(self, server_endpoint, publish_endpoint, filename, **kwargs):
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock = threading.Lock()

        self.__debug = False
        if kwargs['debug']: self.__debug = True

        print('Using OpenDSS version {}'.format(dss.Basic.Version()))

        # Detect if a single config file was provided (assumes the .dss
        # or .DSS extension). If not, assume an archive was provided
        # with a `master.dss` file that imports all the other required
        # config files.
        path, ext = os.path.splitext(filename)

        if not ext:
            raise RuntimeError('config file has no extension')

        # If the filename extension is not `.dss` or `.DSS`, then assume
        # a config archive has been provided.
        if ext not in ['.dss', '.DSS']:
            # Get the name and directory location of the archive file.
            name = os.path.basename(path)
            directory = os.path.dirname(path)

            try:
                # Unpack the archive, letting `shutil` determine the
                # archive format based on the filename extension, into
                # the same directory the archive lives.
                shutil.unpack_archive(filename, directory)
            except ValueError:
                raise RuntimeError('unknown format used for config archive')

            # Assume the archive was created with a directory that
            # has the same name as the archive itself. For example,
            # if the provided filename is /path/to/config.tgz,
            # assume that unpacking the archive will result in a
            # /path/to/config/ directory that contains a master.dss
            # file in it.
            filename = os.path.join(directory, name, 'master.dss')

            if not os.path.isfile(filename):
                raise RuntimeError('config archive does not contain master.dss file')

        # enable/disable saving and clearing circuit
        self.clear_circuit = kwargs['clear_circuit'] == 'enabled'

        if self.clear_circuit:
            now  = datetime.now()
            noon = now.replace(hour=12, minute=0, second=0)

            self.__time_offset = int(now.timestamp() - noon.timestamp())
            self.__solve_count = 0

        # compile the provided circuit
        self.__compile_circuit(filename)

        # used to map circuit elements to their names
        self.elements = {}

        # initialze the system under test using data from OpenDSS
        self.__create()

        # enable/disable published data jitter
        self.jitter = kwargs['jitter'] == 'enabled'

    def query(self):
        """Implements the query command interface for receiving the
        available system tags when requested by field devices or probes.
        When a query command is received, all available OpenDSS points
        of the system under test will be collected and returned."""

        if self.__debug: print('Processing query command')

        msg = []

        for name in self.elements:
            device = self.elements[name]

            for field in device.__dataclass_fields__:
                msg.append(f'{name}.{field}')

        return f"ACK={','.join(msg)}"

    def read(self, tag):
        """Implements the read command interface for receiving the
        OpenDSS model when commanded by field devices or probes. When a
        read command is received, the internal representation of the
        specific element of the system under test will be collected and
        returned."""

        if self.__debug: print('Processing read command')

        try:
            device, field = tag.split('.')

            if not device in self.elements:
                return 'ERR=Unknown device {}'.format(device)

            value = getattr(self.elements[device], field)

            return 'ACK={}'.format(value)
        except AttributeError:
            return 'ERR=Unknown field {} for device {}'.format(field, device)
        except Exception as err:
            return 'ERR={}'.format(err)

    def write(self, tags):
        """Implements the write command interface for updating the
        OpenDSS model when commanded by field devices or probes. When a
        write command is received, the internal representation of the
        system under test will be updated and published after the solve
        completes."""

        if self.__debug: print('Processing write command')

        # Compile circuit definition living in currently tracked temp directory.
        self.__compile_circuit()

        try:
            for tag, value in tags.items():
                device, field = tag.split('.')

                if not device in self.elements:
                   raise NameError('{} is not an element in the current system'.format(device))

                if value.lower() == 'true':
                    value = True
                elif value.lower() == 'false':
                    value = False
                else:
                    value = float(value)

                setattr(self.elements[device], field, value)

            # UPDATE GENERATORS
            if isinstance(self.elements[device], models.GeneratorData):
                dss.Circuit.SetActiveClass('generator')

                gen = self.elements[device]

                # remove the 'generator_' qualifier added during create
                name = gen.name.replace('generator_', '')

                # set the current generator as the active element
                dss.Circuit.SetActiveElement(name)

                if gen.active:
                    # enable generator element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable generator element of active circuit
                    dss.Circuit.Disable(name)

                dss.Generators.kW(gen.active_power_output)

            # UPDATE LOADS
            if isinstance(self.elements[device], models.LoadData):
                dss.Circuit.SetActiveClass('load')

                load = self.elements[device]

                # remove the 'load_' qualifier added during create
                name = load.name.replace('load_', '')

                # set the current load as the active element
                dss.Circuit.SetActiveElement(name)

                if load.active:
                    # enable load element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable load element of active circuit
                    dss.Circuit.Disable(name)

            # UPDATE SHUNTS
            if isinstance(self.elements[device], models.ShuntData):
                dss.Circuit.SetActiveClass('capacitor')

                shunt = self.elements[device]

                # remove the 'shunt_' qualifier added during create
                name = shunt.name.replace('shunt_', '')

                # set the current shunt as the active element
                dss.Circuit.SetActiveElement(name)

                if shunt.active:
                    # enable capacitor element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable capacitor element of active circuit
                    dss.Circuit.Disable(name)

            # UPDATE BRANCHES
            if isinstance(self.elements[device], models.BranchData):
                dss.Circuit.SetActiveClass('line')

                branch = self.elements[device]

                # remove the 'branch_' qualifier added during create
                name = branch.name.replace('branch_', '')

                # set the current branch as the active element
                dss.Circuit.SetActiveElement(name)

                if branch.active:
                    # enable line element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable line element of active circuit
                    dss.Circuit.Disable(name)

            # UPDATE TRANSFORMERS
            if isinstance(self.elements[device], models.TransformerData):
                dss.Circuit.SetActiveClass('transformer')

                transformer = self.elements[device]

                # remove the 'transformer_' qualifier added during create
                name = transformer.name.replace('transformer_', '')

                # set the current transformer as the active element
                dss.Circuit.SetActiveElement(name)

                if transformer.active:
                    # enable line element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable line element of active circuit
                    dss.Circuit.Disable(name)

                # set the active winding to 1
                dss.Transformers.Wdg(1)
                # Set the tap setting of the transformer winding 1
                dss.Transformers.Tap(transformer.tap_setting_wdg1)
                # set the active winding to 2
                dss.Transformers.Wdg(2)
                # Set the tap setting of the transformer winding 2
                dss.Transformers.Tap(transformer.tap_setting_wdg2)

            # UPDATE INVERTERS
            if isinstance(self.elements[device], models.InverterData):
                dss.Circuit.SetActiveClass('pvsystem')

                inverter = self.elements[device]

                # remove the 'inverter_' qualifier added during create
                name = inverter.name.replace('inverter_', '')

                # set the current inverter as the active element
                dss.Circuit.SetActiveElement(name)

                if inverter.active:
                    # enable line element of active circuit
                    dss.Circuit.Enable(name)
                else:
                    # disable line element of active circuit
                    dss.Circuit.Disable(name)

                # grabbing the volt/var curve points as defined in the
                # current internal representation of the inverter curve,
                # then updating the specific point provided in the
                # update
                volt_array = [inverter.curve_volt_pt_1, inverter.curve_volt_pt_2,
                              inverter.curve_volt_pt_3, inverter.curve_volt_pt_4,
                              inverter.curve_volt_pt_5, inverter.curve_volt_pt_6]

                var_array = [inverter.curve_var_pt_1, inverter.curve_var_pt_2,
                             inverter.curve_var_pt_3, inverter.curve_var_pt_4,
                             inverter.curve_var_pt_5, inverter.curve_var_pt_6]

                self.__run_command('edit xycurve.{} xarray={} yarray={}'.
                                   format(inverter.volt_var_curve_name,
                                          volt_array, var_array))

            self.__solve()
            return 'ACK=Success processing OpenDSS write command'

        except Exception as err:
            msg = 'OpenDSS failed to process update message with exception: {}'.format(err)
            print('ERROR: processing write command - {}'.format(msg))
            return 'ERR={}'.format(msg)

    def periodic_publish(self):
        """Loop that calls `self._publish_data` every two seconds."""

        while True:
            if self.__debug: print('Periodically publishing data')

            msg = self.__pack_message()

            try:
                # The publish function is implemented by the Provider.
                self.publish(msg)
            except Exception as ex:
                print('provider periodic publish error: {}'.format(ex))

            time.sleep(2)

    def __pack_message(self):
        """Packs each element in the system under test into a publish
        message."""

        if self.__debug: print('Creating publish message')

        # if this solver is self publishing, the periodic publish will run in a
        # separate thread, so we need a mutex around reading and writing data
        # from the internal representation of the system under test. This lock
        # is also used in the __update function.

        # creating temp elements to add noise to the steady state power system
        # data, this way the self.elements is preserved.
        temp_elements = copy.deepcopy(self.elements)

        with self.__lock:
            for device in temp_elements:
                if isinstance(temp_elements[device], models.BusData):
                    bus = temp_elements[device]

                    if self.__debug: print(bus)

                    bus.voltage_mag_p1 = bus.voltage_mag_p1 * self.__gen_rand_num()
                    bus.voltage_ang_p1 = bus.voltage_ang_p1 * self.__gen_rand_num()
                    bus.voltage_mag_p2 = bus.voltage_mag_p2 * self.__gen_rand_num()
                    bus.voltage_ang_p2 = bus.voltage_ang_p2 * self.__gen_rand_num()
                    bus.voltage_mag_p3 = bus.voltage_mag_p3 * self.__gen_rand_num()
                    bus.voltage_ang_p3 = bus.voltage_ang_p3 * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.GeneratorData):
                    gen = temp_elements[device]

                    if self.__debug: print(gen)

                    phases = json.loads(base64.b64decode(gen.phases).decode())
                    if phases['p1']:
                        gen.voltage_mag_p1 = gen.voltage_mag_p1 * self.__gen_rand_num()
                        gen.voltage_ang_p1 = gen.voltage_ang_p1 * self.__gen_rand_num()
                        gen.current_mag_p1 = gen.current_mag_p1 * self.__gen_rand_num()
                        gen.current_ang_p1 = gen.current_ang_p1 * self.__gen_rand_num()
                        gen.real_power_p1 = gen.real_power_p1 * self.__gen_rand_num()
                        gen.reactive_power_p1 = gen.reactive_power_p1 * self.__gen_rand_num()
                    if phases['p2']:
                        gen.voltage_mag_p2 = gen.voltage_mag_p2 * self.__gen_rand_num()
                        gen.voltage_ang_p2 = gen.voltage_ang_p2 * self.__gen_rand_num()
                        gen.current_mag_p2 = gen.current_mag_p2 * self.__gen_rand_num()
                        gen.current_ang_p2 = gen.current_ang_p2 * self.__gen_rand_num()
                        gen.real_power_p2 = gen.real_power_p2 * self.__gen_rand_num()
                        gen.reactive_power_p2 = gen.reactive_power_p2 * self.__gen_rand_num()
                    if phases['p3']:
                        gen.voltage_mag_p3 = gen.voltage_mag_p3 * self.__gen_rand_num()
                        gen.voltage_ang_p3 = gen.voltage_ang_p3 * self.__gen_rand_num()
                        gen.current_mag_p3 = gen.current_mag_p3 * self.__gen_rand_num()
                        gen.current_ang_p3 = gen.current_ang_p3 * self.__gen_rand_num()
                        gen.real_power_p3 = gen.real_power_p3 * self.__gen_rand_num()
                        gen.reactive_power_p3 = gen.reactive_power_p3 * self.__gen_rand_num()
                    if phases['pn']:
                        gen.voltage_mag_pn = gen.voltage_mag_pn * self.__gen_rand_num()
                        gen.voltage_ang_pn = gen.voltage_ang_pn * self.__gen_rand_num()
                        gen.current_mag_pn = gen.current_mag_pn * self.__gen_rand_num()
                        gen.current_ang_pn = gen.current_ang_pn * self.__gen_rand_num()
                        gen.real_power_pn = gen.real_power_pn * self.__gen_rand_num()
                        gen.reactive_power_pn = gen.reactive_power_pn * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.LoadData):
                    load = temp_elements[device]

                    if self.__debug: print(load)

                    phases = json.loads(base64.b64decode(load.phases).decode())
                    if phases['p1']:
                        load.voltage_mag_p1 = load.voltage_mag_p1 * self.__gen_rand_num()
                        load.voltage_ang_p1 = load.voltage_ang_p1 * self.__gen_rand_num()
                        load.current_mag_p1 = load.current_mag_p1 * self.__gen_rand_num()
                        load.current_ang_p1 = load.current_ang_p1 * self.__gen_rand_num()
                        load.real_power_p1 = load.real_power_p1 * self.__gen_rand_num()
                        load.reactive_power_p1 = load.reactive_power_p1 * self.__gen_rand_num()
                    if phases['p2']:
                        load.voltage_mag_p2 = load.voltage_mag_p2 * self.__gen_rand_num()
                        load.voltage_ang_p2 = load.voltage_ang_p2 * self.__gen_rand_num()
                        load.current_mag_p2 = load.current_mag_p2 * self.__gen_rand_num()
                        load.current_ang_p2 = load.current_ang_p2 * self.__gen_rand_num()
                        load.real_power_p2 = load.real_power_p2 * self.__gen_rand_num()
                        load.reactive_power_p2 = load.reactive_power_p2 * self.__gen_rand_num()
                    if phases['p3']:
                        load.voltage_mag_p3 = load.voltage_mag_p3 * self.__gen_rand_num()
                        load.voltage_ang_p3 = load.voltage_ang_p3 * self.__gen_rand_num()
                        load.current_mag_p3 = load.current_mag_p3 * self.__gen_rand_num()
                        load.current_ang_p3 = load.current_ang_p3 * self.__gen_rand_num()
                        load.real_power_p3 = load.real_power_p3 * self.__gen_rand_num()
                        load.reactive_power_p3 = load.reactive_power_p3 * self.__gen_rand_num()
                    if phases['pn']:
                        load.voltage_mag_pn = load.voltage_mag_pn * self.__gen_rand_num()
                        load.voltage_ang_pn = load.voltage_ang_pn * self.__gen_rand_num()
                        load.current_mag_pn = load.current_mag_pn * self.__gen_rand_num()
                        load.current_ang_pn = load.current_ang_pn * self.__gen_rand_num()
                        load.real_power_pn = load.real_power_pn * self.__gen_rand_num()
                        load.reactive_power_pn = load.reactive_power_pn * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.ShuntData):
                    shunt = temp_elements[device]

                    if self.__debug: print(shunt)

                    phases = json.loads(base64.b64decode(shunt.phases).decode())
                    if phases['p1']:
                        shunt.voltage_mag_p1 = shunt.voltage_mag_p1 * self.__gen_rand_num()
                        shunt.voltage_ang_p1 = shunt.voltage_ang_p1 * self.__gen_rand_num()
                        shunt.current_mag_p1 = shunt.current_mag_p1 * self.__gen_rand_num()
                        shunt.current_ang_p1 = shunt.current_ang_p1 * self.__gen_rand_num()
                        shunt.real_power_p1 = shunt.real_power_p1 * self.__gen_rand_num()
                        shunt.reactive_power_p1 = shunt.reactive_power_p1 * self.__gen_rand_num()
                    if phases['p2']:
                        shunt.voltage_mag_p2 = shunt.voltage_mag_p2 * self.__gen_rand_num()
                        shunt.voltage_ang_p2 = shunt.voltage_ang_p2 * self.__gen_rand_num()
                        shunt.current_mag_p2 = shunt.current_mag_p2 * self.__gen_rand_num()
                        shunt.current_ang_p2 = shunt.current_ang_p2 * self.__gen_rand_num()
                        shunt.real_power_p2 = shunt.real_power_p2 * self.__gen_rand_num()
                        shunt.reactive_power_p2 = shunt.reactive_power_p2 * self.__gen_rand_num()
                    if phases['p3']:
                        shunt.voltage_mag_p3 = shunt.voltage_mag_p3 * self.__gen_rand_num()
                        shunt.voltage_ang_p3 = shunt.voltage_ang_p3 * self.__gen_rand_num()
                        shunt.current_mag_p3 = shunt.current_mag_p3 * self.__gen_rand_num()
                        shunt.current_ang_p3 = shunt.current_ang_p3 * self.__gen_rand_num()
                        shunt.real_power_p3 = shunt.real_power_p3 * self.__gen_rand_num()
                        shunt.reactive_power_p3 = shunt.reactive_power_p3 * self.__gen_rand_num()
                    if phases['pn']:
                        shunt.voltage_mag_pn = shunt.voltage_mag_pn * self.__gen_rand_num()
                        shunt.voltage_ang_pn = shunt.voltage_ang_pn * self.__gen_rand_num()
                        shunt.current_mag_pn = shunt.current_mag_pn * self.__gen_rand_num()
                        shunt.current_ang_pn = shunt.current_ang_pn * self.__gen_rand_num()
                        shunt.real_power_pn = shunt.real_power_pn * self.__gen_rand_num()
                        shunt.reactive_power_pn = shunt.reactive_power_pn * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.BranchData):
                    branch = temp_elements[device]

                    if self.__debug: print(branch)

                    phases = json.loads(base64.b64decode(branch.phases).decode())
                    if phases['p1']:
                        branch.voltage_mag_src_p1 = branch.voltage_mag_src_p1 * self.__gen_rand_num()
                        branch.voltage_ang_src_p1 = branch.voltage_ang_src_p1 * self.__gen_rand_num()
                        branch.current_mag_src_p1 = branch.current_mag_src_p1 * self.__gen_rand_num()
                        branch.current_ang_src_p1 = branch.current_ang_src_p1 * self.__gen_rand_num()
                        branch.real_power_src_p1 = branch.real_power_src_p1 * self.__gen_rand_num()
                        branch.reactive_power_src_p1 = branch.reactive_power_src_p1 * self.__gen_rand_num()
                        branch.voltage_mag_dst_p1 = branch.voltage_mag_dst_p1 * self.__gen_rand_num()
                        branch.voltage_ang_dst_p1 = branch.voltage_ang_dst_p1 * self.__gen_rand_num()
                        branch.current_mag_dst_p1 = branch.current_mag_dst_p1 * self.__gen_rand_num()
                        branch.current_ang_dst_p1 = branch.current_ang_dst_p1 * self.__gen_rand_num()
                        branch.real_power_dst_p1 = branch.real_power_dst_p1 * self.__gen_rand_num()
                        branch.reactive_power_dst_p1 = branch.reactive_power_dst_p1 * self.__gen_rand_num()
                    if phases['p2']:
                        branch.voltage_mag_src_p2 = branch.voltage_mag_src_p2 * self.__gen_rand_num()
                        branch.voltage_ang_src_p2 = branch.voltage_ang_src_p2 * self.__gen_rand_num()
                        branch.current_mag_src_p2 = branch.current_mag_src_p2 * self.__gen_rand_num()
                        branch.current_ang_src_p2 = branch.current_ang_src_p2 * self.__gen_rand_num()
                        branch.real_power_src_p2 = branch.real_power_src_p2 * self.__gen_rand_num()
                        branch.reactive_power_src_p2 = branch.reactive_power_src_p2 * self.__gen_rand_num()
                        branch.voltage_mag_dst_p2 = branch.voltage_mag_dst_p2 * self.__gen_rand_num()
                        branch.voltage_ang_dst_p2 = branch.voltage_ang_dst_p2 * self.__gen_rand_num()
                        branch.current_mag_dst_p2 = branch.current_mag_dst_p2 * self.__gen_rand_num()
                        branch.current_ang_dst_p2 = branch.current_ang_dst_p2 * self.__gen_rand_num()
                        branch.real_power_dst_p2 = branch.real_power_dst_p2 * self.__gen_rand_num()
                        branch.reactive_power_dst_p2 = branch.reactive_power_dst_p2 * self.__gen_rand_num()
                    if phases['p3']:
                        branch.voltage_mag_src_p3 = branch.voltage_mag_src_p3 * self.__gen_rand_num()
                        branch.voltage_ang_src_p3 = branch.voltage_ang_src_p3 * self.__gen_rand_num()
                        branch.current_mag_src_p3 = branch.current_mag_src_p3 * self.__gen_rand_num()
                        branch.current_ang_src_p3 = branch.current_ang_src_p3 * self.__gen_rand_num()
                        branch.real_power_src_p3 = branch.real_power_src_p3 * self.__gen_rand_num()
                        branch.reactive_power_src_p3 = branch.reactive_power_src_p3 * self.__gen_rand_num()
                        branch.voltage_mag_dst_p3 = branch.voltage_mag_dst_p3 * self.__gen_rand_num()
                        branch.voltage_ang_dst_p3 = branch.voltage_ang_dst_p3 * self.__gen_rand_num()
                        branch.current_mag_dst_p3 = branch.current_mag_dst_p3 * self.__gen_rand_num()
                        branch.current_ang_dst_p3 = branch.current_ang_dst_p3 * self.__gen_rand_num()
                        branch.real_power_dst_p3 = branch.real_power_dst_p3 * self.__gen_rand_num()
                        branch.reactive_power_dst_p3 = branch.reactive_power_dst_p3 * self.__gen_rand_num()
                    if phases['pn']:
                        branch.voltage_mag_src_pn = branch.voltage_mag_src_pn * self.__gen_rand_num()
                        branch.voltage_ang_src_pn = branch.voltage_ang_src_pn * self.__gen_rand_num()
                        branch.current_mag_src_pn = branch.current_mag_src_pn * self.__gen_rand_num()
                        branch.current_ang_src_pn = branch.current_ang_src_pn * self.__gen_rand_num()
                        branch.real_power_src_pn = branch.real_power_src_pn * self.__gen_rand_num()
                        branch.reactive_power_src_pn = branch.reactive_power_src_pn * self.__gen_rand_num()
                        branch.voltage_mag_dst_pn = branch.voltage_mag_dst_pn * self.__gen_rand_num()
                        branch.voltage_ang_dst_pn = branch.voltage_ang_dst_pn * self.__gen_rand_num()
                        branch.current_mag_dst_pn = branch.current_mag_dst_pn * self.__gen_rand_num()
                        branch.current_ang_dst_pn = branch.current_ang_dst_pn * self.__gen_rand_num()
                        branch.real_power_dst_pn = branch.real_power_dst_pn * self.__gen_rand_num()
                        branch.reactive_power_dst_pn = branch.reactive_power_dst_pn * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.TransformerData):
                    transformer = temp_elements[device]

                    if self.__debug: print(transformer)

                    phases = json.loads(base64.b64decode(transformer.phases).decode())
                    if phases['wdg1_p1']:
                        transformer.voltage_mag_wdg1_p1 = transformer.voltage_mag_wdg1_p1 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg1_p1 = transformer.voltage_ang_wdg1_p1 * self.__gen_rand_num()
                        transformer.current_mag_wdg1_p1 = transformer.current_mag_wdg1_p1 * self.__gen_rand_num()
                        transformer.current_ang_wdg1_p1 = transformer.current_ang_wdg1_p1 * self.__gen_rand_num()
                    if phases['wdg2_p1']:
                        transformer.voltage_mag_wdg2_p1 = transformer.voltage_mag_wdg2_p1 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg2_p1 = transformer.voltage_ang_wdg2_p1 * self.__gen_rand_num()
                        transformer.current_mag_wdg2_p1 = transformer.current_mag_wdg2_p1 * self.__gen_rand_num()
                        transformer.current_ang_wdg2_p1 = transformer.current_ang_wdg2_p1 * self.__gen_rand_num()
                    if phases['wdg3_p1']:
                        transformer.voltage_mag_wdg3_p1 = transformer.voltage_mag_wdg3_p1 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg3_p1 = transformer.voltage_ang_wdg3_p1 * self.__gen_rand_num()
                        transformer.current_mag_wdg3_p1 = transformer.current_mag_wdg3_p1 * self.__gen_rand_num()
                        transformer.current_ang_wdg3_p1 = transformer.current_ang_wdg3_p1 * self.__gen_rand_num()
                    if phases['wdg1_p2']:
                        transformer.voltage_mag_wdg1_p2 = transformer.voltage_mag_wdg1_p2 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg1_p2 = transformer.voltage_ang_wdg1_p2 * self.__gen_rand_num()
                        transformer.current_mag_wdg1_p2 = transformer.current_mag_wdg1_p2 * self.__gen_rand_num()
                        transformer.current_ang_wdg1_p2 = transformer.current_ang_wdg1_p2 * self.__gen_rand_num()
                    if phases['wdg2_p2']:
                        transformer.voltage_mag_wdg2_p2 = transformer.voltage_mag_wdg2_p2 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg2_p2 = transformer.voltage_ang_wdg2_p2 * self.__gen_rand_num()
                        transformer.current_mag_wdg2_p2 = transformer.current_mag_wdg2_p2 * self.__gen_rand_num()
                        transformer.current_ang_wdg2_p2 = transformer.current_ang_wdg2_p2 * self.__gen_rand_num()
                    if phases['wdg3_p2']:
                        transformer.voltage_mag_wdg3_p2 = transformer.voltage_mag_wdg3_p2 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg3_p2 = transformer.voltage_ang_wdg3_p2 * self.__gen_rand_num()
                        transformer.current_mag_wdg3_p2 = transformer.current_mag_wdg3_p2 * self.__gen_rand_num()
                        transformer.current_ang_wdg3_p2 = transformer.current_ang_wdg3_p2 * self.__gen_rand_num()
                    if phases['wdg1_p3']:
                        transformer.voltage_mag_wdg1_p3 = transformer.voltage_mag_wdg1_p3 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg1_p3 = transformer.voltage_ang_wdg1_p3 * self.__gen_rand_num()
                        transformer.current_mag_wdg1_p3 = transformer.current_mag_wdg1_p3 * self.__gen_rand_num()
                        transformer.current_ang_wdg1_p3 = transformer.current_ang_wdg1_p3 * self.__gen_rand_num()
                    if phases['wdg2_p3']:
                        transformer.voltage_mag_wdg2_p3 = transformer.voltage_mag_wdg2_p3 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg2_p3 = transformer.voltage_ang_wdg2_p3 * self.__gen_rand_num()
                        transformer.current_mag_wdg2_p3 = transformer.current_mag_wdg2_p3 * self.__gen_rand_num()
                        transformer.current_ang_wdg2_p3 = transformer.current_ang_wdg2_p3 * self.__gen_rand_num()
                    if phases['wdg3_p3']:
                        transformer.voltage_mag_wdg3_p3 = transformer.voltage_mag_wdg3_p3 * self.__gen_rand_num()
                        transformer.voltage_ang_wdg3_p3 = transformer.voltage_ang_wdg3_p3 * self.__gen_rand_num()
                        transformer.current_mag_wdg3_p3 = transformer.current_mag_wdg3_p3 * self.__gen_rand_num()
                        transformer.current_ang_wdg3_p3 = transformer.current_ang_wdg3_p3 * self.__gen_rand_num()
                    if phases['wdg1_pn']:
                        transformer.voltage_mag_wdg1_pn = transformer.voltage_mag_wdg1_pn * self.__gen_rand_num()
                        transformer.voltage_ang_wdg1_pn = transformer.voltage_ang_wdg1_pn * self.__gen_rand_num()
                        transformer.current_mag_wdg1_pn = transformer.current_mag_wdg1_pn * self.__gen_rand_num()
                        transformer.current_ang_wdg1_pn = transformer.current_ang_wdg1_pn * self.__gen_rand_num()
                    if phases['wdg2_pn']:
                        transformer.voltage_mag_wdg2_pn = transformer.voltage_mag_wdg2_pn * self.__gen_rand_num()
                        transformer.voltage_ang_wdg2_pn = transformer.voltage_ang_wdg2_pn * self.__gen_rand_num()
                        transformer.current_mag_wdg2_pn = transformer.current_mag_wdg2_pn * self.__gen_rand_num()
                        transformer.current_ang_wdg2_pn = transformer.current_ang_wdg2_pn * self.__gen_rand_num()
                    if phases['wdg3_pn']:
                        transformer.voltage_mag_wdg3_pn = transformer.voltage_mag_wdg3_pn * self.__gen_rand_num()
                        transformer.voltage_ang_wdg3_pn = transformer.voltage_ang_wdg3_pn * self.__gen_rand_num()
                        transformer.current_mag_wdg3_pn = transformer.current_mag_wdg3_pn * self.__gen_rand_num()
                        transformer.current_ang_wdg3_pn = transformer.current_ang_wdg3_pn * self.__gen_rand_num()

                    if phases['wdg1_p1'] or phases['wdg2_p1'] or phases['wdg3_p1']:
                        transformer.real_power_p1 = transformer.real_power_p1 * self.__gen_rand_num()
                        transformer.reactive_power_p1 = transformer.reactive_power_p1 * self.__gen_rand_num()
                    if phases['wdg1_p2'] or phases['wdg2_p2'] or phases['wdg3_p2']:
                        transformer.real_power_p2 = transformer.real_power_p2 * self.__gen_rand_num()
                        transformer.reactive_power_p2 = transformer.reactive_power_p2 * self.__gen_rand_num()
                    if phases['wdg1_p3'] or phases['wdg2_p3'] or phases['wdg3_p3']:
                        transformer.real_power_p3 = transformer.real_power_p3 * self.__gen_rand_num()
                        transformer.reactive_power_p3 = transformer.reactive_power_p3 * self.__gen_rand_num()
                    if phases['wdg1_pn'] or phases['wdg2_pn'] or phases['wdg3_pn']:
                        transformer.real_power_pn = transformer.real_power_pn * self.__gen_rand_num()
                        transformer.reactive_power_pn = transformer.reactive_power_pn * self.__gen_rand_num()

                if isinstance(temp_elements[device], models.InverterData):
                    inverter = temp_elements[device]

                    if self.__debug: print(inverter)

                    phases = json.loads(base64.b64decode(inverter.phases).decode())
                    if phases['p1']:
                        inverter.voltage_mag_p1 = inverter.voltage_mag_p1 * self.__gen_rand_num()
                        inverter.voltage_ang_p1 = inverter.voltage_ang_p1 * self.__gen_rand_num()
                        inverter.current_mag_p1 = inverter.current_mag_p1 * self.__gen_rand_num()
                        inverter.current_ang_p1 = inverter.current_ang_p1 * self.__gen_rand_num()
                        inverter.real_power_p1 = inverter.real_power_p1 * self.__gen_rand_num()
                        inverter.reactive_power_p1 = inverter.reactive_power_p1 * self.__gen_rand_num()
                    if phases['p2']:
                        inverter.voltage_mag_p2 = inverter.voltage_mag_p2 * self.__gen_rand_num()
                        inverter.voltage_ang_p2 = inverter.voltage_ang_p2 * self.__gen_rand_num()
                        inverter.current_mag_p2 = inverter.current_mag_p2 * self.__gen_rand_num()
                        inverter.current_ang_p2 = inverter.current_ang_p2 * self.__gen_rand_num()
                        inverter.real_power_p2 = inverter.real_power_p2 * self.__gen_rand_num()
                        inverter.reactive_power_p2 = inverter.reactive_power_p2 * self.__gen_rand_num()
                    if phases['p3']:
                        inverter.voltage_mag_p3 = inverter.voltage_mag_p3 * self.__gen_rand_num()
                        inverter.voltage_ang_p3 = inverter.voltage_ang_p3 * self.__gen_rand_num()
                        inverter.current_mag_p3 = inverter.current_mag_p3 * self.__gen_rand_num()
                        inverter.current_ang_p3 = inverter.current_ang_p3 * self.__gen_rand_num()
                        inverter.real_power_p3 = inverter.real_power_p3 * self.__gen_rand_num()
                        inverter.reactive_power_p3 = inverter.reactive_power_p3 * self.__gen_rand_num()
                    if phases['pn']:
                        inverter.voltage_mag_pn = inverter.voltage_mag_pn * self.__gen_rand_num()
                        inverter.voltage_ang_pn = inverter.voltage_ang_pn * self.__gen_rand_num()
                        inverter.current_mag_pn = inverter.current_mag_pn * self.__gen_rand_num()
                        inverter.current_ang_pn = inverter.current_ang_pn * self.__gen_rand_num()
                        inverter.real_power_pn = inverter.real_power_pn * self.__gen_rand_num()
                        inverter.reactive_power_pn = inverter.reactive_power_pn * self.__gen_rand_num()

                    inverter.current_mag_total = inverter.current_mag_total * self.__gen_rand_num()
                    inverter.real_power_total = inverter.real_power_total * self.__gen_rand_num()
                    inverter.reactive_power_total = inverter.reactive_power_total * self.__gen_rand_num()

        if self.__debug: print('Packing message')

        msg = []

        for name in temp_elements:
            device = temp_elements[name]

            for field in device.__dataclass_fields__:
                value = getattr(device, field)
                msg.append(f'{name}.{field}:{value}')

        print('Done packing message.')

        return ','.join(msg)

    def __create(self):
        """Creates the internal representation of the system under test using
        the data classes defined in this module, initializing it with the names
        of each element in the OpenDSS circuit."""

        if self.__debug: print('Creating system under test')

        # BUSES
        for name in dss.Circuit.AllBusNames():
            if name:
                dss.Circuit.SetActiveBus(name)
                bus = models.BusData(name = f'bus_{name}')

                if self.__debug: print(bus)

                self.elements[bus.name] = bus

        # GENERATORS
        # set the active class to 'Generator'
        dss.Circuit.SetActiveClass('generator')

        for name in dss.Generators.AllNames():
            # set the current generator as the active element
            dss.Circuit.SetActiveElement(name)

            gen = models.GeneratorData(
                name = f'generator_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                bus = dss.CktElement.BusNames()[0].split('.')[0],
                rated_apparent_power = dss.Generators.kVARated(),
            )


            if self.__debug: print(gen)

            self.elements[gen.name] = gen

        # LOADS
        # set the active class to 'Load'
        dss.Circuit.SetActiveClass('load')

        for name in dss.Loads.AllNames():
            # set the current load as the active element
            dss.Circuit.SetActiveElement(name)

            load = models.LoadData(
                name = f'load_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                bus = dss.CktElement.BusNames()[0].split('.')[0],
            )

            if self.__debug: print(load)

            self.elements[load.name] = load

        # SHUNTS
        # set the active class to 'Capacitor'
        dss.Circuit.SetActiveClass('capacitor')

        for name in dss.Capacitors.AllNames():
            # set the current capacitor as the active element
            dss.Circuit.SetActiveElement(name)

            shunt = models.ShuntData(
                name = f'shunt_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                bus = dss.CktElement.BusNames()[0].split('.')[0],
                rated_reactive_power = dss.Capacitors.kvar(),
            )

            if self.__debug: print(shunt)

            self.elements[shunt.name] = shunt

        # BRANCHES
        # set the active class to 'Line'
        dss.Circuit.SetActiveClass('line')

        for name in dss.Lines.AllNames():
            # set the current line as the active element
            dss.Circuit.SetActiveElement(name)

            branch = models.BranchData(
                name = f'branch_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                source_bus = dss.CktElement.BusNames()[0].split('.')[0],
                dest_bus = dss.CktElement.BusNames()[1].split('.')[0],
                # Assuming intrinsic properties of branches will never change,
                # statically add them during the create step.
                resistance = dss.Lines.R1(),
                reactance = dss.Lines.X1(),
                charging = dss.Lines.C1(),
            )

            if self.__debug: print(branch)

            self.elements[branch.name] = branch

        # TRANSFORMERS
        # set the active class to 'Transformer'
        dss.Circuit.SetActiveClass('transformer')

        for name in dss.Transformers.AllNames():
            # set the current transformer as the active element
            dss.Circuit.SetActiveElement(name)

            transformer = models.TransformerData(
                name = f'transformer_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                winding1_bus = dss.CktElement.BusNames()[0].split('.')[0],
                winding2_bus = dss.CktElement.BusNames()[1].split('.')[0],
                # Assuming intrinsic properties of transformers will never
                # change, statically add them during the create step.
                num_windings = dss.Transformers.NumWindings(),
            )

            # select winding 1
            dss.Transformers.Wdg(1)
            transformer.kva_rating_wdg1 = dss.Transformers.kVA()
            pattern = dss.Transformers.IsDelta()
            if pattern == 1:
                transformer.winding_pattern_wdg1 = 'delta'
            else:
                transformer.winding_pattern_wdg1 = 'wye'
            transformer.num_taps_wdg1 = dss.Transformers.NumTaps()
            transformer.min_tap_wdg1 = dss.Transformers.MinTap()
            transformer.max_tap_wdg1 = dss.Transformers.MaxTap()
            transformer.tap_setting_wdg1 = dss.Transformers.Tap()
            transformer.wdg1_resistance = dss.Transformers.R()

            # select winding 2
            dss.Transformers.Wdg(2)
            transformer.kva_rating_wdg2 = dss.Transformers.kVA()
            pattern = dss.Transformers.IsDelta()
            if pattern == 1:
                transformer.winding_pattern_wdg2 = 'delta'
            else:
                transformer.winding_pattern_wdg2 = 'wye'
            transformer.num_taps_wdg2 = dss.Transformers.NumTaps()
            transformer.min_tap_wdg2 = dss.Transformers.MinTap()
            transformer.max_tap_wdg2 = dss.Transformers.MaxTap()
            transformer.tap_setting_wdg2 = dss.Transformers.Tap()
            transformer.wdg2_resistance = dss.Transformers.R()
            transformer.reactance_wdg1_wdg2 = dss.Transformers.Xhl()

            # if it's a 3 winding transformer
            if transformer.num_windings > 2:
                dss.Transformers.Wdg(3) # activate winding 3, then check R
                transformer.wdg3_resistance = dss.Transformers.R()
                transformer.reactance_wdg1_wdg3 = dss.Transformers.Xht()
                transformer.reactance_wdg2_wdg3 = dss.Transformers.Xlt()

            if self.__debug: print(transformer)

            self.elements[transformer.name] = transformer

        # INVERTERS
        # First, get controller names; set the active class to
        # 'INVcontrol'
        dss.Circuit.SetActiveClass('invcontrol')
        # get list of all inverter controller names
        inv_controllers = dss.ActiveClass.AllNames()

        for name in dss.PVsystems.AllNames():
            # set the active class to 'PVsystem'
            dss.Circuit.SetActiveClass('pvsystem')

            # set the current inverter as the active element
            dss.Circuit.SetActiveElement(name)

            inverter = models.InverterData(
                name = f'inverter_{name}',
                # Sometimes BusNames() will return not just the bus name, but
                # also the phases in dot notation. So... split based on the dot
                # then grab the actual number.
                bus = dss.CktElement.BusNames()[0].split('.')[0],
                rated_apparent_power = dss.PVsystems.kVARated(),
            )


            # set the active class to 'INVcontrol'
            dss.Circuit.SetActiveClass('invcontrol')

            # the API is kinda limited for inverter controllers, so
            # I'm just looping through all the controller names and
            # matching the controller to the inverter manually
            for controller in inv_controllers:
                # set the current controller as the active element
                dss.Circuit.SetActiveElement(controller)
                # now check the current active controller properties
                # to see if it's associated with the current inverter
                if name in dss.Properties.Value('1').lower():
                    inverter.controller_name = controller
                    # if controller mode is 'VOLTVAR', then volt/var is enabled
                    inverter.volt_var_enable = dss.Properties.Value('2') == 'VOLTVAR'
                    inverter.volt_var_curve_name = dss.Properties.Value('4').lower()
                    # '23' is the properties index for VV_RefReactivePower
                    # if VV_RefReactivePower = 'VARMAX_WATTS, then value = 2
                    # if VV_RefReactivePower = 'VARAVAL_WATTS, then value = 3
                    # if VV_RefReactivePower = 'VARMAX_VARS, don't care
                    # this is to support Sunspec 126.DeptRef standard
                    if dss.Properties.Value('23') == 'VARMAX_WATTS':
                        inverter.volt_var_reference = 2
                    elif dss.Properties.Value('23') == 'VARAVAL_WATTS':
                        inverter.volt_var_reference = 3

                    # the dss_utils.Iterator returns a handle to the function
                    # that is specified by name on the given module. this
                    # function handle can then be called to get the actual value
                    # for the named function. for example, the variable `name`
                    # below is a handle to the `dss.XYCurves.Name()` function.
                    # the iterator takes care of calling `dss.XYCurves.First()`
                    # and `dss.XYCurves.Next()` on each iteration of the for-
                    # loop, so calling `name()` returns the name of the currently
                    # active generator being iterated over. one caveat with the
                    # iterator utility is that it only loops over ENABLED
                    # elements. because of this, we're using the
                    # 'dss.Circuit.SetActiveClass()' and
                    # 'dss.Circuit.SetActiveElement()' methods.

                    for xy_name in dss_utils.Iterator(dss.XYCurves, 'Name'):
                        # check for the volt/var curve name
                        # associated with this inverter
                        if xy_name() == inverter.volt_var_curve_name:
                            volt_points = dss.XYCurves.XArray()
                            inverter.curve_volt_pt_1 = volt_points[0]
                            inverter.curve_volt_pt_2 = volt_points[1]
                            inverter.curve_volt_pt_3 = volt_points[2]
                            inverter.curve_volt_pt_4 = volt_points[3]
                            inverter.curve_volt_pt_5 = volt_points[4]
                            inverter.curve_volt_pt_6 = volt_points[5]
                            var_points = dss.XYCurves.YArray()
                            inverter.curve_var_pt_1 = var_points[0]
                            inverter.curve_var_pt_2 = var_points[1]
                            inverter.curve_var_pt_3 = var_points[2]
                            inverter.curve_var_pt_4 = var_points[3]
                            inverter.curve_var_pt_5 = var_points[4]
                            inverter.curve_var_pt_6 = var_points[5]

            if self.__debug: print(inverter)

            self.elements[inverter.name] = inverter

        # we want to execute the very first solve without performing an update,
        # so first is set to 'true'. this is because we first need to
        # initialize the phases before we can do an update. the inverter updates
        # specifically need the phase information to update the data.
        self.__solve(first='true')

    def __update(self):
        """Reads circuit elements from OpenDSS and updates the internal
        representation of the system under test."""

        if self.__debug: print('Updating system under test')

        # if this solver is self publishing, the periodic publish will run in a
        # separate thread, so we need a mutex around reading and writing data
        # from the internal representation of the system under test. This lock
        # is also used in the pack_message function.
        with self.__lock:
            # BUSES
            print('Updating buses...')
            for name in dss.Circuit.AllBusNames():
                if name:
                    bus = self.elements['bus_{}'.format(name)]

                    assert bus

                    dss.Circuit.SetActiveBus(name)
                    voltages = dss.Bus.puVmagAngle()

                    # Each bus will either be single phase or
                    # three phase. Thus, the first two indexes of the
                    # `voltages` variable should always be set.
                    bus.voltage_mag_p1 = voltages[0]
                    bus.voltage_ang_p1 = voltages[1]

                    # If this bus is single phase, trying to read
                    # from `voltages` at index 2 will result in an
                    # `IndexError`. If that happens, just continue on.
                    try:
                        bus.voltage_mag_p2 = voltages[2]
                        bus.voltage_ang_p2 = voltages[3]
                        bus.voltage_mag_p3 = voltages[4]
                        bus.voltage_ang_p3 = voltages[5]
                    except IndexError:
                        pass

                    if self.__debug: print(bus)

            # GENERATORS
            print('Updating generators...')
            # set the active class to 'Generator'
            dss.Circuit.SetActiveClass('generator')

            for name in dss.Generators.AllNames():
                # set the current generator as the active element
                dss.Circuit.SetActiveElement(name)

                gen = self.elements['generator_{}'.format(name)]

                assert gen

                # determine if currently active circuit element (in this
                # case, the currently active generator) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                gen.active = enabled == 1

                gen.active_power_output = dss.Generators.kW()

                voltcurpow = self.__get_voltcurpow_1_terminal(gen.phases)

                if 'p1' in voltcurpow:
                    gen.voltage_mag_p1 = round(voltcurpow['p1'][0], 3)
                    gen.voltage_ang_p1 = round(voltcurpow['p1'][1], 3)
                    gen.current_mag_p1 = round(voltcurpow['p1'][2], 3)
                    gen.current_ang_p1 = round(voltcurpow['p1'][3], 3)
                    gen.real_power_p1 = round(voltcurpow['p1'][4], 3)
                    gen.reactive_power_p1 = round(voltcurpow['p1'][5], 3)
                if 'p2' in voltcurpow:
                    gen.voltage_mag_p2 = round(voltcurpow['p2'][0], 3)
                    gen.voltage_ang_p2 = round(voltcurpow['p2'][1], 3)
                    gen.current_mag_p2 = round(voltcurpow['p2'][2], 3)
                    gen.current_ang_p2 = round(voltcurpow['p2'][3], 3)
                    gen.real_power_p2 = round(voltcurpow['p2'][4], 3)
                    gen.reactive_power_p2 = round(voltcurpow['p2'][5], 3)
                if 'p3' in voltcurpow:
                    gen.voltage_mag_p3 = round(voltcurpow['p3'][0], 3)
                    gen.voltage_ang_p3 = round(voltcurpow['p3'][1], 3)
                    gen.current_mag_p3 = round(voltcurpow['p3'][2], 3)
                    gen.current_ang_p3 = round(voltcurpow['p3'][3], 3)
                    gen.real_power_p3 = round(voltcurpow['p3'][4], 3)
                    gen.reactive_power_p3 = round(voltcurpow['p3'][5], 3)
                if 'pn' in voltcurpow:
                    gen.voltage_mag_pn = round(voltcurpow['pn'][0], 3)
                    gen.voltage_ang_pn = round(voltcurpow['pn'][1], 3)
                    gen.current_mag_pn = round(voltcurpow['pn'][2], 3)
                    gen.current_ang_pn = round(voltcurpow['pn'][3], 3)
                    gen.real_power_pn = round(voltcurpow['pn'][4], 3)
                    gen.reactive_power_pn = round(voltcurpow['pn'][5], 3)

                if self.__debug: print(gen)

            # LOADS
            print('Updating loads...')
            # set the active class to 'Load'
            dss.Circuit.SetActiveClass('load')

            for name in dss.Loads.AllNames():
                # set the current load as the active element
                dss.Circuit.SetActiveElement(name)

                load = self.elements['load_{}'.format(name)]

                assert load

                # determine if currently active circuit element (in this
                # case, the currently active load) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                load.active = enabled == 1

                voltcurpow = self.__get_voltcurpow_1_terminal(load.phases)

                if 'p1' in voltcurpow:
                    load.voltage_mag_p1 = round(voltcurpow['p1'][0], 3)
                    load.voltage_ang_p1 = round(voltcurpow['p1'][1], 3)
                    load.current_mag_p1 = round(voltcurpow['p1'][2], 3)
                    load.current_ang_p1 = round(voltcurpow['p1'][3], 3)
                    load.real_power_p1 = round(voltcurpow['p1'][4], 3)
                    load.reactive_power_p1 = round(voltcurpow['p1'][5], 3)
                if 'p2' in voltcurpow:
                    load.voltage_mag_p2 = round(voltcurpow['p2'][0], 3)
                    load.voltage_ang_p2 = round(voltcurpow['p2'][1], 3)
                    load.current_mag_p2 = round(voltcurpow['p2'][2], 3)
                    load.current_ang_p2 = round(voltcurpow['p2'][3], 3)
                    load.real_power_p2 = round(voltcurpow['p2'][4], 3)
                    load.reactive_power_p2 = round(voltcurpow['p2'][5], 3)
                if 'p3' in voltcurpow:
                    load.voltage_mag_p3 = round(voltcurpow['p3'][0], 3)
                    load.voltage_ang_p3 = round(voltcurpow['p3'][1], 3)
                    load.current_mag_p3 = round(voltcurpow['p3'][2], 3)
                    load.current_ang_p3 = round(voltcurpow['p3'][3], 3)
                    load.real_power_p3 = round(voltcurpow['p3'][4], 3)
                    load.reactive_power_p3 = round(voltcurpow['p3'][5], 3)
                if 'pn' in voltcurpow:
                    load.voltage_mag_pn = round(voltcurpow['pn'][0], 3)
                    load.voltage_ang_pn = round(voltcurpow['pn'][1], 3)
                    load.current_mag_pn = round(voltcurpow['pn'][2], 3)
                    load.current_ang_pn = round(voltcurpow['pn'][3], 3)
                    load.real_power_pn = round(voltcurpow['pn'][4], 3)
                    load.reactive_power_pn = round(voltcurpow['pn'][5], 3)

                if self.__debug: print(load)

            # SHUNTS
            print('Updating shunts...')
            # set the active class to 'Capacitor'
            dss.Circuit.SetActiveClass('capacitor')

            for name in dss.Capacitors.AllNames():
                # set the current capacitor as the active element
                dss.Circuit.SetActiveElement(name)

                shunt = self.elements['shunt_{}'.format(name)]

                assert shunt

                # Determine if currently active circuit element (in this
                # case, the currently active shunt) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                shunt.active = enabled == 1

                voltcurpow = self.__get_voltcurpow_1_terminal(shunt.phases)

                if 'p1' in voltcurpow:
                    shunt.voltage_mag_p1 = round(voltcurpow['p1'][0], 3)
                    shunt.voltage_ang_p1 = round(voltcurpow['p1'][1], 3)
                    shunt.current_mag_p1 = round(voltcurpow['p1'][2], 3)
                    shunt.current_ang_p1 = round(voltcurpow['p1'][3], 3)
                    shunt.real_power_p1 = round(voltcurpow['p1'][4], 3)
                    shunt.reactive_power_p1 = round(voltcurpow['p1'][5], 3)
                if 'p2' in voltcurpow:
                    shunt.voltage_mag_p2 = round(voltcurpow['p2'][0], 3)
                    shunt.voltage_ang_p2 = round(voltcurpow['p2'][1], 3)
                    shunt.current_mag_p2 = round(voltcurpow['p2'][2], 3)
                    shunt.current_ang_p2 = round(voltcurpow['p2'][3], 3)
                    shunt.real_power_p2 = round(voltcurpow['p2'][4], 3)
                    shunt.reactive_power_p2 = round(voltcurpow['p2'][5], 3)
                if 'p3' in voltcurpow:
                    shunt.voltage_mag_p3 = round(voltcurpow['p3'][0], 3)
                    shunt.voltage_ang_p3 = round(voltcurpow['p3'][1], 3)
                    shunt.current_mag_p3 = round(voltcurpow['p3'][2], 3)
                    shunt.current_ang_p3 = round(voltcurpow['p3'][3], 3)
                    shunt.real_power_p3 = round(voltcurpow['p3'][4], 3)
                    shunt.reactive_power_p3 = round(voltcurpow['p3'][5], 3)
                if 'pn' in voltcurpow:
                    shunt.voltage_mag_pn = round(voltcurpow['pn'][0], 3)
                    shunt.voltage_ang_pn = round(voltcurpow['pn'][1], 3)
                    shunt.current_mag_pn = round(voltcurpow['pn'][2], 3)
                    shunt.current_ang_pn = round(voltcurpow['pn'][3], 3)
                    shunt.real_power_pn = round(voltcurpow['pn'][4], 3)
                    shunt.reactive_power_pn = round(voltcurpow['pn'][5], 3)

                if self.__debug: print(shunt)

            # BRANCHES
            print('Updating branches...')
            # set the active class to 'Line'
            dss.Circuit.SetActiveClass('line')

            for name in dss.Lines.AllNames():
                # set the current line as the active element
                dss.Circuit.SetActiveElement(name)

                branch = self.elements['branch_{}'.format(name)]

                assert branch

                # determine if currently active circuit element (in this
                # case, the currently active branch) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                branch.active = enabled == 1

                # for now, I'm assuming OpenDSS assigns phase numbers based
                # based on the source bus, e.g., consider a single phase
                # branch that starts at bus 1, phase 1 and ends at bus 2,
                # phase 3, then the branch phase that will hold the voltage,
                # current, and power output will be considered phase 1, not
                # phase 3.
                voltcurpow_src, voltcurpow_dst = self.__get_voltcurpow_2_terminal(branch.phases)

                if 'p1' in voltcurpow_src:
                    branch.voltage_mag_src_p1 = round(voltcurpow_src['p1'][0], 3)
                    branch.voltage_ang_src_p1 = round(voltcurpow_src['p1'][1], 3)
                    branch.current_mag_src_p1 = round(voltcurpow_src['p1'][2], 3)
                    branch.current_ang_src_p1 = round(voltcurpow_src['p1'][3], 3)
                    branch.real_power_src_p1 = round(voltcurpow_src['p1'][4], 3)
                    branch.reactive_power_src_p1 = round(voltcurpow_src['p1'][5], 3)
                    branch.voltage_mag_dst_p1 = round(voltcurpow_dst['p1'][0], 3)
                    branch.voltage_ang_dst_p1 = round(voltcurpow_dst['p1'][1], 3)
                    branch.current_mag_dst_p1 = round(voltcurpow_dst['p1'][2], 3)
                    branch.current_ang_dst_p1 = round(voltcurpow_dst['p1'][3], 3)
                    branch.real_power_dst_p1 = round(voltcurpow_dst['p1'][4], 3)
                    branch.reactive_power_dst_p1 = round(voltcurpow_dst['p1'][5], 3)
                if 'p2' in voltcurpow_src:
                    branch.voltage_mag_src_p2 = round(voltcurpow_src['p2'][0], 3)
                    branch.voltage_ang_src_p2 = round(voltcurpow_src['p2'][1], 3)
                    branch.current_mag_src_p2 = round(voltcurpow_src['p2'][2], 3)
                    branch.current_ang_src_p2 = round(voltcurpow_src['p2'][3], 3)
                    branch.real_power_src_p2 = round(voltcurpow_src['p2'][4], 3)
                    branch.reactive_power_src_p2 = round(voltcurpow_src['p2'][5], 3)
                    branch.voltage_mag_dst_p2 = round(voltcurpow_dst['p2'][0], 3)
                    branch.voltage_ang_dst_p2 = round(voltcurpow_dst['p2'][1], 3)
                    branch.current_mag_dst_p2 = round(voltcurpow_dst['p2'][2], 3)
                    branch.current_ang_dst_p2 = round(voltcurpow_dst['p2'][3], 3)
                    branch.real_power_dst_p2 = round(voltcurpow_dst['p2'][4], 3)
                    branch.reactive_power_dst_p2 = round(voltcurpow_dst['p2'][5], 3)
                if 'p3' in voltcurpow_src:
                    branch.voltage_mag_src_p3 = round(voltcurpow_src['p3'][0], 3)
                    branch.voltage_ang_src_p3 = round(voltcurpow_src['p3'][1], 3)
                    branch.current_mag_src_p3 = round(voltcurpow_src['p3'][2], 3)
                    branch.current_ang_src_p3 = round(voltcurpow_src['p3'][3], 3)
                    branch.real_power_src_p3 = round(voltcurpow_src['p3'][4], 3)
                    branch.reactive_power_src_p3 = round(voltcurpow_src['p3'][5], 3)
                    branch.voltage_mag_dst_p3 = round(voltcurpow_dst['p3'][0], 3)
                    branch.voltage_ang_dst_p3 = round(voltcurpow_dst['p3'][1], 3)
                    branch.current_mag_dst_p3 = round(voltcurpow_dst['p3'][2], 3)
                    branch.current_ang_dst_p3 = round(voltcurpow_dst['p3'][3], 3)
                    branch.real_power_dst_p3 = round(voltcurpow_dst['p3'][4], 3)
                    branch.reactive_power_dst_p3 = round(voltcurpow_dst['p3'][5], 3)
                if 'pn' in voltcurpow_src:
                    branch.voltage_mag_src_pn = round(voltcurpow_src['pn'][0], 3)
                    branch.voltage_ang_src_pn = round(voltcurpow_src['pn'][1], 3)
                    branch.current_mag_src_pn = round(voltcurpow_src['pn'][2], 3)
                    branch.current_ang_src_pn = round(voltcurpow_src['pn'][3], 3)
                    branch.real_power_pn = round(voltcurpow_src['pn'][4], 3)
                    branch.reactive_power_pn = round(voltcurpow_src['pn'][5], 3)
                    branch.voltage_mag_dst_pn = round(voltcurpow_dst['pn'][0], 3)
                    branch.voltage_ang_dst_pn = round(voltcurpow_dst['pn'][1], 3)
                    branch.current_mag_dst_pn = round(voltcurpow_dst['pn'][2], 3)
                    branch.current_ang_dst_pn = round(voltcurpow_dst['pn'][3], 3)
                    branch.real_power_dst_pn = round(voltcurpow_dst['pn'][4], 3)
                    branch.reactive_power_dst_pn = round(voltcurpow_dst['pn'][5], 3)

                if self.__debug: print(branch)

            # TRANSFORMERS
            print('Updating transformers...')
            # set the active class to 'Transformer'
            dss.Circuit.SetActiveClass('transformer')

            for name in dss.Transformers.AllNames():
                # set the current transformer as the active element
                dss.Circuit.SetActiveElement(name)

                transformer = self.elements['transformer_{}'.format(name)]

                assert transformer

                # determine if currently active circuit element (in this
                # case, the currently active transformer) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                transformer.active = enabled == 1

                # set the active winding to 1
                dss.Transformers.Wdg(1)
                transformer.tap_setting_wdg1 = dss.Transformers.Tap()
                # set the active winding to 2
                dss.Transformers.Wdg(2)
                transformer.tap_setting_wdg2 = dss.Transformers.Tap()

                # note that I'm using the same 2 terminal voltcurpow method
                # here because I'm not yet supporting 3-winding transformers.
                # the transformer data class already includes data points for
                # all three windings, but I'm not yet populating the data.
                # I'll add support when we encounter a case with 3 windings.
                # I'm also defining the real and reactive power for each phase
                # as the real and reactive power calculated on winding 1 of
                # the transformer. assumption is that the power seen between
                # winding 1 and winding 2 will be practically identical

                voltcurpow_wdg1, voltcurpow_wdg2 = self.__get_voltcurpow_2_terminal(transformer.phases)

                if 'p1' in voltcurpow_wdg1:
                    transformer.voltage_mag_wdg1_p1 = round(voltcurpow_wdg1['p1'][0], 3)
                    transformer.voltage_ang_wdg1_p1 = round(voltcurpow_wdg1['p1'][1], 3)
                    transformer.current_mag_wdg1_p1 = round(voltcurpow_wdg1['p1'][2], 3)
                    transformer.current_ang_wdg1_p1 = round(voltcurpow_wdg1['p1'][3], 3)
                    transformer.real_power_p1 = round(voltcurpow_wdg1['p1'][4], 3)
                    transformer.reactive_power_p1 = round(voltcurpow_wdg1['p1'][5], 3)
                    transformer.voltage_mag_wdg2_p1 = round(voltcurpow_wdg2['p1'][0], 3)
                    transformer.voltage_ang_wdg2_p1 = round(voltcurpow_wdg2['p1'][1], 3)
                    transformer.current_mag_wdg2_p1 = round(voltcurpow_wdg2['p1'][2], 3)
                    transformer.current_ang_wdg2_p1 = round(voltcurpow_wdg2['p1'][3], 3)
                if 'p2' in voltcurpow_wdg1:
                    transformer.voltage_mag_wdg1_p2 = round(voltcurpow_wdg1['p2'][0], 3)
                    transformer.voltage_ang_wdg1_p2 = round(voltcurpow_wdg1['p2'][1], 3)
                    transformer.current_mag_wdg1_p2 = round(voltcurpow_wdg1['p2'][2], 3)
                    transformer.current_ang_wdg1_p2 = round(voltcurpow_wdg1['p2'][3], 3)
                    transformer.real_power_p2 = round(voltcurpow_wdg1['p2'][4], 3)
                    transformer.reactive_power_p2 = round(voltcurpow_wdg1['p2'][5], 3)
                    transformer.voltage_mag_wdg2_p2 = round(voltcurpow_wdg2['p2'][0], 3)
                    transformer.voltage_ang_wdg2_p2 = round(voltcurpow_wdg2['p2'][1], 3)
                    transformer.current_mag_wdg2_p2 = round(voltcurpow_wdg2['p2'][2], 3)
                    transformer.current_ang_wdg2_p2 = round(voltcurpow_wdg2['p2'][3], 3)
                if 'p3' in voltcurpow_wdg1:
                    transformer.voltage_mag_wdg1_p3 = round(voltcurpow_wdg1['p3'][0], 3)
                    transformer.voltage_ang_wdg1_p3 = round(voltcurpow_wdg1['p3'][1], 3)
                    transformer.current_mag_wdg1_p3 = round(voltcurpow_wdg1['p3'][2], 3)
                    transformer.current_ang_wdg1_p3 = round(voltcurpow_wdg1['p3'][3], 3)
                    transformer.real_power_p3 = round(voltcurpow_wdg1['p3'][4], 3)
                    transformer.reactive_power_p3 = round(voltcurpow_wdg1['p3'][5], 3)
                    transformer.voltage_mag_wdg2_p3 = round(voltcurpow_wdg2['p3'][0], 3)
                    transformer.voltage_ang_wdg2_p3 = round(voltcurpow_wdg2['p3'][1], 3)
                    transformer.current_mag_wdg2_p3 = round(voltcurpow_wdg2['p3'][2], 3)
                    transformer.current_ang_wdg2_p3 = round(voltcurpow_wdg2['p3'][3], 3)
                if 'pn' in voltcurpow_wdg1:
                    transformer.voltage_mag_wdg1_pn = round(voltcurpow_wdg1['pn'][0], 3)
                    transformer.voltage_ang_wdg1_pn = round(voltcurpow_wdg1['pn'][1], 3)
                    transformer.current_mag_wdg1_pn = round(voltcurpow_wdg1['pn'][2], 3)
                    transformer.current_ang_wdg1_pn = round(voltcurpow_wdg1['pn'][3], 3)
                    transformer.real_power_pn = round(voltcurpow_wdg1['pn'][4], 3)
                    transformer.reactive_power_pn = round(voltcurpow_wdg1['pn'][5], 3)
                    transformer.voltage_mag_wdg2_pn = round(voltcurpow_wdg2['pn'][0], 3)
                    transformer.voltage_ang_wdg2_pn = round(voltcurpow_wdg2['pn'][1], 3)
                    transformer.current_mag_wdg2_pn = round(voltcurpow_wdg2['pn'][2], 3)
                    transformer.current_ang_wdg2_pn = round(voltcurpow_wdg2['pn'][3], 3)

                if self.__debug: print(transformer)

            # INVERTERS
            print('Updating inverters...\n')
            # set the active class to 'PVsystem'
            dss.Circuit.SetActiveClass('pvsystem')

            for name in dss.PVsystems.AllNames():
                # set the current inverter as the active element
                dss.Circuit.SetActiveElement(name)

                inverter = self.elements['inverter_{}'.format(name)]

                assert inverter

                # determine if currently active circuit element (in this
                # case, the currently active generator) is enabled
                # (1 = enabled, 0 = disabled)
                enabled = dss.CktElement.Enabled()
                inverter.active = enabled

                voltcurpow = self.__get_voltcurpow_1_terminal(inverter.phases)

                if 'p1' in voltcurpow:
                    inverter.voltage_mag_p1 = round(voltcurpow['p1'][0], 3)
                    inverter.voltage_ang_p1 = round(voltcurpow['p1'][1], 3)
                    inverter.current_mag_p1 = round(voltcurpow['p1'][2], 3)
                    inverter.current_ang_p1 = round(voltcurpow['p1'][3], 3)
                    inverter.real_power_p1 = round((voltcurpow['p1'][4])*1000, 3)
                    inverter.reactive_power_p1 = round((voltcurpow['p1'][5])*1000, 3)
                if 'p2' in voltcurpow:
                    inverter.voltage_mag_p2 = round(voltcurpow['p2'][0], 3)
                    inverter.voltage_ang_p2 = round(voltcurpow['p2'][1], 3)
                    inverter.current_mag_p2 = round(voltcurpow['p2'][2], 3)
                    inverter.current_ang_p2 = round(voltcurpow['p2'][3], 3)
                    inverter.real_power_p2 = round((voltcurpow['p2'][4])*1000, 3)
                    inverter.reactive_power_p2 = round((voltcurpow['p2'][5])*1000, 3)
                if 'p3' in voltcurpow:
                    inverter.voltage_mag_p3 = round(voltcurpow['p3'][0], 3)
                    inverter.voltage_ang_p3 = round(voltcurpow['p3'][1], 3)
                    inverter.current_mag_p3 = round(voltcurpow['p3'][2], 3)
                    inverter.current_ang_p3 = round(voltcurpow['p3'][3], 3)
                    inverter.real_power_p3 = round((voltcurpow['p3'][4])*1000, 3)
                    inverter.reactive_power_p3 = round((voltcurpow['p3'][5])*1000, 3)
                if 'pn' in voltcurpow:
                    inverter.voltage_mag_pn = round(voltcurpow['pn'][0], 3)
                    inverter.voltage_ang_pn = round(voltcurpow['pn'][1], 3)
                    inverter.current_mag_pn = round(voltcurpow['pn'][2], 3)
                    inverter.current_ang_pn = round(voltcurpow['pn'][3], 3)
                    inverter.real_power_pn = round((voltcurpow['pn'][4])*1000, 3)
                    inverter.reactive_power_pn = round((voltcurpow['pn'][5])*1000, 3)

                inverter.current_mag_total = inverter.current_mag_p1 + \
                                             inverter.current_mag_p2 + \
                                             inverter.current_mag_p3

                inverter.real_power_total = inverter.real_power_p1 + \
                                            inverter.real_power_p2 + \
                                            inverter.real_power_p3

                inverter.reactive_power_total = inverter.reactive_power_p1 + \
                                                inverter.reactive_power_p2 + \
                                                inverter.reactive_power_p3

                # TODO: need to figure out a better way of
                # selecting the XYCurve associated with the
                # current inverter other than looping through
                # all of them and matching on the name
                for xy_name in dss_utils.Iterator(dss.XYCurves, 'Name'):
                    # check for the volt/var curve name
                    # associated with this inverter
                    if xy_name() == inverter.volt_var_curve_name:
                        volt_points = dss.XYCurves.XArray()
                        inverter.curve_volt_pt_1 = volt_points[0]
                        inverter.curve_volt_pt_2 = volt_points[1]
                        inverter.curve_volt_pt_3 = volt_points[2]
                        inverter.curve_volt_pt_4 = volt_points[3]
                        inverter.curve_volt_pt_5 = volt_points[4]
                        inverter.curve_volt_pt_6 = volt_points[5]
                        var_points = dss.XYCurves.YArray()
                        inverter.curve_var_pt_1 = var_points[0]
                        inverter.curve_var_pt_2 = var_points[1]
                        inverter.curve_var_pt_3 = var_points[2]
                        inverter.curve_var_pt_4 = var_points[3]
                        inverter.curve_var_pt_5 = var_points[4]
                        inverter.curve_var_pt_6 = var_points[5]

                if self.__debug: print(inverter)

    def __solve(self, first='false'):
        """Triggers a solve in OpenDSS and checks for any errors. It then
        calls the `__update` function to update the internal representation
        of the system under test."""

        print('Solving circuit (first solve: {})'.format(first))

        if self.clear_circuit and first == 'false':
            self.__solve_count += 1

            secs = int(time.time() - self.__time_offset)
            hour = int(secs / 3600)

            hour %= 24
            secs %= 3600

            self.__run_command('set mode=duty controlmode=static hour={} sec={} maxcontroliter=1000'.format(hour, secs))

            print('Solve parameters configured (clearing circuit)')

        start = timer()
        dss.Solution.Solve()
        end = timer()

        print('Solving circuit took {} seconds'.format(end - start))

        if self.clear_circuit and first == 'false':
            event_log_path = '/etc/sceptre/log/opendss-event-log.{}.txt'.format(self.__solve_count)

            print('Writing OpenDSS Event Log to {}'.format(event_log_path))

            with open(event_log_path, 'w') as log:
                for line in dss.Solution.EventLog():
                    log.write('{}\n'.format(line))

        res = dss.Error.Description()
        if res:
            raise PowerSolverError('Error solving circuit: %s' % res)

        converged = dss.Solution.Converged()
        if not converged:
            raise PowerSolverError('OpenDSS model failed to converge!')

        # if its the very first solve, initialize phase data.
        # otherwise, perform an update.
        if first == 'true':
            self.__initialize_phases()
        else:
            self.__update()

        self.__save_and_clear_circuit()

    def __run_command(self, text):
        """Run a command using the text interface of OpenDSS."""

        res = dss_utils.run_command(text)
        if res:
            # Could be done better
            if not res.startswith('Circuit saved in directory'):
                raise PowerSolverError(res)

        # the run_command function doesn't always return errors encountered.
        res = dss.Error.Description()
        if res:
            # Could be done better
            if not res.startswith('Circuit saved in directory'):
                raise PowerSolverError(res)

    def __compile_circuit(self, filename=None):
        if not filename:
            if not self.clear_circuit: return

            filename = self.temp_dir.name + '/Master.DSS'

            if not os.path.isfile(filename):
                raise RuntimeError('temp directory does not contain Master.DSS file')

        start = timer()

        print('Compiling {}'.format(filename))
        self.__run_command('Compile {}'.format(filename))

        end = timer()
        print('Compiling circuit took {} seconds'.format(end - start))

    def __save_and_clear_circuit(self):
        if not self.clear_circuit: return

        start = timer()

        # Save circuit to a temp directory
        self.temp_dir = tempfile.TemporaryDirectory()
        print('Saving circuit to {}'.format(self.temp_dir.name))
        self.__run_command('Save Circuit Dir={}'.format(self.temp_dir.name))

        # Rename BusVoltageBases.DSS to .dss since Linux is case sensitive and
        # the Master.DSS file references .dss.
        base_name = self.temp_dir.name + '/BusVoltageBases'
        os.rename(base_name + '.DSS', base_name + '.dss')

        print('Clearing circuit')
        dss.Basic.ClearAll()

        end = timer()
        print('Saving and clearing circuit took {} seconds'.format(end - start))

    def __initialize_phases(self):
        """Identifies and initializes phases for all elements in the internal
        representation of the system under test. This only needs to be run
        once immediately after initialization of the system element names and
        the initial solve."""

        print('Initializing element phases.')

        # GENERATORS
        # set the active class to 'Generator'
        dss.Circuit.SetActiveClass('generator')

        for name in dss.Generators.AllNames():
            # set the current generator as the active element
            dss.Circuit.SetActiveElement(name)

            gen = self.elements['generator_{}'.format(name)]
            assert gen

            # creating a dictionary to identify which phases this
            # generator is connected to, then converting to json to
            # store the data class
            phases = dict.fromkeys(['p1', 'p2', 'p3', 'pn'], False)

            # if the element is enabled, then we can use 'NodeOrder()' to
            # get phase information, if not, try to infer it from it's bus
            # information
            if dss.CktElement.Enabled():
                element_phases = dss.CktElement.NodeOrder()
            else:
                element_phases = list(map(int, dss.Properties.Value('2').split('.')[1:]))
                # if element_phases is nothing, it's 3 phase
                if not element_phases:
                    element_phases = [1, 2, 3]

            for phase in element_phases:
                if phase == 0:
                    phases['pn'] = True
                else:
                    phases['p{}'.format(phase)] = True
            gen.phases = base64.b64encode(json.dumps(phases).encode())

        # LOADS
        # set the active class to 'Load'
        dss.Circuit.SetActiveClass('load')

        for name in dss.Loads.AllNames():
            # set the current load as the active element
            dss.Circuit.SetActiveElement(name)

            load = self.elements['load_{}'.format(name)]
            assert load

            # creating a dictionary to identify which phases this
            # load is connected to, then converting to json to
            # store in the data class
            phases = dict.fromkeys(['p1', 'p2', 'p3', 'pn'], False)

            # if the element is enabled, then we can use 'NodeOrder()' to
            # get phase information, if not, try to infer it from it's bus
            # information
            if dss.CktElement.Enabled():
                element_phases = dss.CktElement.NodeOrder()
            else:
                element_phases = list(map(int, dss.Properties.Value('2').split('.')[1:]))
                # if element_phases is nothing, it's 3 phase
                if not element_phases:
                    element_phases = [1, 2, 3]

            for phase in element_phases:
                if phase == 0:
                    phases['pn'] = True
                else:
                    phases['p{}'.format(phase)] = True
            load.phases = base64.b64encode(json.dumps(phases).encode())

        # SHUNTS
        # set the active class to 'Capacitor'
        dss.Circuit.SetActiveClass('capacitor')

        for name in dss.Capacitors.AllNames():
            # set the current capacitor as the active element
            dss.Circuit.SetActiveElement(name)

            shunt = self.elements['shunt_{}'.format(name)]
            assert shunt

            # creating a dictionary to identify which phases this
            # shunt is connected to, then converting to json to
            # store in the data class
            phases = dict.fromkeys(['p1', 'p2', 'p3', 'pn'], False)

            # if the element is enabled, then we can use 'NodeOrder()' to
            # get phase information, if not, try to infer it from it's bus
            # information
            if dss.CktElement.Enabled():
                element_phases = dss.CktElement.NodeOrder()
            else:
                element_phases = list(map(int, dss.Properties.Value('1').split('.')[1:]))
                # how I'm deciding phases is really weak for shunts. need
                # to improve, but for now just looking at the length of
                # the 'bus2' property. this will only work for shunt
                # capacitors
                if not element_phases:
                    bus2_phases = list(map(int, dss.Properties.Value('2').split('.')[1:]))
                    element_phases = [i for i in range(1, len(bus2_phases)+1)]

            for phase in dss.CktElement.NodeOrder():
                if phase == 0:
                    phases['pn'] = True
                else:
                    phases['p{}'.format(phase)] = True
            shunt.phases = base64.b64encode(json.dumps(phases).encode())

        # BRANCHES
        # set the active class to 'Line'
        dss.Circuit.SetActiveClass('line')

        for name in dss.Lines.AllNames():
            # set the current line as the active element
            dss.Circuit.SetActiveElement(name)

            branch = self.elements['branch_{}'.format(name)]
            assert branch

            # creating a dictionary to identify which phases this
            # branch is connected to, then converting to json to
            # store in the data class. I'm also just enumerating
            # the phases from one side of the branch, assuming that
            # the other side of the branch will have the same number
            # of phases.

            phases = dict.fromkeys(['p1', 'p2', 'p3', 'pn'], False)

            # if the element is enabled, then we can use 'NodeOrder()' to
            # get phase information, if not, try to infer it from it's bus
            # information
            num_conductors = dss.CktElement.NumConductors()
            if dss.CktElement.Enabled():
                element_phases = dss.CktElement.NodeOrder()[:num_conductors]
            else:
                element_phases = list(map(int, dss.Properties.Value('1').split('.')[1:]))
                # if element_phases is nothing, it's 3 phase
                if not element_phases:
                    element_phases = [1, 2, 3]

            for phase in element_phases:
                if phase == 0:
                    phases['pn'] = True
                else:
                    phases['p{}'.format(phase)] = True
            branch.phases = base64.b64encode(json.dumps(phases).encode())

        # TRANSFORMERS
        # set the active class to 'Transformer'
        dss.Circuit.SetActiveClass('transformer')

        for name in dss.Transformers.AllNames():
            # set the current transformer as the active element
            dss.Circuit.SetActiveElement(name)

            transformer = self.elements['transformer_{}'.format(name)]
            assert transformer

            # creating a dictionary to identify which phases this
            # transformer is connected to, then converting to json to
            # store in the data class
            phases = dict.fromkeys(['wdg1_p1', 'wdg1_p2', 'wdg1_p3', 'wdg1_pn',
                                    'wdg2_p1', 'wdg2_p2', 'wdg2_p3', 'wdg2_pn',
                                    'wdg3_p1', 'wdg3_p2', 'wdg3_p3', 'wdg3_pn'],
                                   False)

            # here I'm trying to extract the phases per winding. This only
            # works when the NodeOrder() method returns the same number of
            # conductors for each winding. It seems that even if you don't
            # specify a neutral conductor on one side of the transformer
            # but do not on the other, then NodeOrder() should return the
            # neutral conductor on both sides. if the element is enabled,
            # then we can use 'NodeOrder()' to get the phase info, if not,
            # try to infer the phases from the buses it's connected to.
            if dss.CktElement.Enabled():
                phase_order = dss.CktElement.NodeOrder()
                num_conductors = dss.CktElement.NumConductors()
                phases_by_winding = [phase_order[i:i+num_conductors] for i in \
                                    range(0, len(phase_order), num_conductors)]
            else:
                dss.Transformers.Wdg(1)
                phases_wdg1 = list(map(int, dss.Properties.Value('4').split('.')[1:]))
                # if phases_wdg1 is nothing, it's 3 phase
                if not phases_wdg1:
                    phases_wdg1 = [1, 2, 3]
                dss.Transformers.Wdg(2)
                phases_wdg2 = list(map(int, dss.Properties.Value('4').split('.')[1:]))
                # if phases_wdg2 is nothing, it's 3 phase
                if not phases_wdg2:
                    phases_wdg2 = [1, 2, 3]
                phases_by_winding = [phases_wdg1, phases_wdg2]

            for num, winding in enumerate(phases_by_winding):
                for phase in winding:
                    if phase == 0:
                        phases['wdg{}_pn'.format(num+1)] = True
                    else:
                        phases['wdg{}_p{}'.format(num+1, phase)] = True
            transformer.phases = base64.b64encode(json.dumps(phases).encode())

        # INVERTERS
        # set the active class to 'PVsystem'
        dss.Circuit.SetActiveClass('pvsystem')

        for name in dss.PVsystems.AllNames():
            # set the current inverter as the active element
            dss.Circuit.SetActiveElement(name)

            inverter = self.elements['inverter_{}'.format(name)]
            assert inverter

            # creating a dictionary to identify which phases this
            # generator is connected to, then converting to json to
            # store in the data class
            phases = dict.fromkeys(['p1', 'p2', 'p3', 'pn'], False)

            # if the element is enabled, then we can use 'NodeOrder()' to
            # get phase information, if not, try to infer it from it's bus
            # information
            if dss.CktElement.Enabled():
                element_phases = dss.CktElement.NodeOrder()
            else:
                element_phases = list(map(int, dss.Properties.Value('2').split('.')[1:]))
                # if element_phases is nothing, than it means it's 3 phase
                if not element_phases:
                    element_phases = [1, 2, 3]

            for phase in element_phases:
                if phase == 0:
                    phases['pn'] = True
                else:
                    phases['p{}'.format(phase)] = True
            inverter.phases = base64.b64encode(json.dumps(phases).encode())

        # executing a system update because the first solve from the create
        # step did not perform an update on the system. the reason is because
        # before you can run the 'dss.CktElement.NodeOrder' to get the phases,
        # you first need to do a preliminary solve and you can't yet update
        # until the phases are initialized (the 'self.__update()' method
        # depends on having the phase information). once the phases have been
        # defined for each data class, then we can execute an update. this
        # only happens during initial startup
        self.__update()

    def __get_voltcurpow_1_terminal(self, phases):
        """Method to get the voltage, current, and power of the currently active
        circuit element, per phase.  This method is for devices with just one
        terminal, such as generators and loads."""

        # if the current element is disabled, you can't query for voltage,
        # current, or power through OpenDSS, so then assume all is 0
        if dss.CktElement.Enabled():
            phase_order = [str(i) for i in dss.CktElement.NodeOrder()]

            voltages = dss.CktElement.VoltagesMagAng()
            currents = dss.CktElement.CurrentsMagAng()
            powers = dss.CktElement.Powers()

            # grouping all voltages and currents per phase (mag,ang)
            voltages_by_phase = [voltages[x:x+2] for x in range(0, len(voltages), 2)]
            currents_by_phase = [currents[x:x+2] for x in range(0, len(currents), 2)]
            # mapping voltages and currents to phase numbers
            voltages_per_phase = dict(zip(phase_order, voltages_by_phase))
            currents_per_phase = dict(zip(phase_order, currents_by_phase))
            # do the same with powers
            powers_by_phase = [powers[x:x+2] for x in range(0, len(powers), 2)]
            powers_per_phase = dict(zip(phase_order, powers_by_phase))

            # combine voltages, currents, and powers
            voltcurpow = {k:voltages_per_phase[k] + currents_per_phase[k] +
                          powers_per_phase[k] for k in voltages_per_phase}
            voltcurpow = {'p' + i.replace('0', 'n'): voltcurpow[i] for i in voltcurpow.keys()}
        else:
            voltcurpow = {}
            element_phases = json.loads(base64.b64decode(phases).decode())
            for phase, value in element_phases.items():
                if value:
                    voltcurpow[phase] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]

        return voltcurpow

    def __get_voltcurpow_2_terminal(self, phases):
        """Method to get the voltage, current, and power of the currently
        active circuit element, per phase and per terminal. This method is
        for devices with two terminals, such as branches and transformers."""

        # if the current element is disabled, you can't query for voltage,
        # current, or power through OpenDSS, so then assume all is 0
        if dss.CktElement.Enabled():
            phase_order = [str(i) for i in dss.CktElement.NodeOrder()]
            num_conductors = dss.CktElement.NumConductors()

            voltages = dss.CktElement.VoltagesMagAng()
            currents = dss.CktElement.CurrentsMagAng()
            powers = dss.CktElement.Powers()

            # grouping all voltages per phase (mag,ang)
            voltages_by_phase = [voltages[x:x+2] for x in range(0, len(voltages), 2)]
            # split voltages up by terminal
            voltages_by_phase_src = voltages_by_phase[:num_conductors]
            voltages_by_phase_dst = voltages_by_phase[-num_conductors:]
            # mapping voltages to phase numbers for each terminal
            voltages_per_phase_src = dict(zip(phase_order[:num_conductors], voltages_by_phase_src))
            voltages_per_phase_dst = dict(zip(phase_order[-num_conductors:], voltages_by_phase_dst))
            # grouping all currents per phase (mag,ang)
            currents_by_phase = [currents[x:x+2] for x in range(0, len(currents), 2)]
            # split currents up by terminal
            currents_by_phase_src = currents_by_phase[:num_conductors]
            currents_by_phase_dst = currents_by_phase[-num_conductors:]
            # mapping currents to phase numbers for each terminal
            currents_per_phase_src = dict(zip(phase_order[:num_conductors], currents_by_phase_src))
            currents_per_phase_dst = dict(zip(phase_order[-num_conductors:], currents_by_phase_dst))
            # grouping all powers per phase (mag,ang)
            powers_by_phase = [powers[x:x+2] for x in range(0, len(powers), 2)]
            # split powers up by terminal
            powers_by_phase_src = powers_by_phase[:num_conductors]
            powers_by_phase_dst = powers_by_phase[-num_conductors:]
            # mapping powers to phase numbers for each terminal
            powers_per_phase_src = dict(zip(phase_order[:num_conductors], powers_by_phase_src))
            powers_per_phase_dst = dict(zip(phase_order[-num_conductors:], powers_by_phase_dst))

            # combine voltages, currents, and power for each terminal
            voltcurpow_src = {k:voltages_per_phase_src[k] + currents_per_phase_src[k] +
                              powers_per_phase_src[k] for k in voltages_per_phase_src}
            voltcurpow_src = {'p' + i.replace('0', 'n'): voltcurpow_src[i] for i in voltcurpow_src.keys()}
            voltcurpow_dst = {k:voltages_per_phase_dst[k] + currents_per_phase_dst[k] +
                              powers_per_phase_dst[k] for k in voltages_per_phase_dst}
            voltcurpow_dst = {'p' + i.replace('0', 'n'): voltcurpow_dst[i] for i in voltcurpow_dst.keys()}
        else:
            voltcurpow_src = {}
            voltcurpow_dst = {}
            element_phases = json.loads(base64.b64decode(phases).decode())
            for phase, value in element_phases.items():
                if value:
                    voltcurpow_src[phase] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
                    voltcurpow_dst[phase] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]

        return voltcurpow_src, voltcurpow_dst

    def __gen_rand_num(self):
        """Generate a random number between 0.99 and 1.01. Used to add noise to
        the system."""

        if not self.jitter:
            return 1.0

        rand_num = random.uniform(0.99, 1.01)
        return rand_num
