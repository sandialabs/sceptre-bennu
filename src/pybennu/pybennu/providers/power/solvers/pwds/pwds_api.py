"""
Low-level API for talking to PowerWorld Dynamics Studio.

See pysceptre/docs/PWDS-Protocol.pdf for details on the protocol.
"""

import socket, struct

from collections import namedtuple
from select import select

class PWDSAPI:
    # PWDS data type constants
    BYTE      = 0
    CARDINAL  = 1
    INTEGER   = 2
    FLOAT     = 3
    DOUBLE    = 4
    WIDE_CHAR = 5
    WORD      = 6
    SMALL_INT = 7
    STRING    = 8
    PAYLOAD   = 9


    # tracks the size of each PWDS data type defined above. the data type const
    # values above correspond to the indexes in this list.
    SIZES = [
        1, # byte      - ummm... a byte
        4, # cardinal  - unsigned 4 byte int
        4, # integer   - signed 4 byte int
        4, # float     - single precision
        8, # double    - double precision
        2, # wide_char - unicode
        2, # word      - unsigned 2 byte int
        2, # small_int - signed 2 byte int
        2, # string    - `word` representing remaining length of string
        4  # payload   - `cardinal` representing remaining length of payload
    ]


    # PWDS object name constants
    BUS    = 'Bus'
    GEN    = 'Gen'
    LOAD   = 'Load'
    SHUNT  = 'Shunt'
    BRANCH = 'Branch'


    # represents a PWDS message returned to API caller
    Message = namedtuple('Message', 'version, msg_type, src_id, user_id, payload')

    # represents a generic PWDS ObjectType
    ObjectType = namedtuple('ObjectType', 'name, count, byte_fields, word_fields, int_fields, float_fields, double_fields, string_fields')

    # represent specific PWDS dictionary objects
    Bus    = namedtuple('Bus',    'number, area, zone, substn, nominal_kv, max_pu, min_pu, name')
    Gen    = namedtuple('Gen',    'bus, area, zone, volt_pu_setpoint, mw_setpoint, mva_base, mw_max, mw_min, device_id')
    Load   = namedtuple('Load',   'bus, area, zone, device_id')
    Shunt  = namedtuple('Shunt',  'bus, area, zone, nominal_mvar, device_id')
    Branch = namedtuple('Branch', 'option, branch_type, section, from_bus, to_bus, multi_from, multi_to, mva_limit, circuit_id')

    # represent specific PWDS data objects
    #BusData    = namedtuple('BusData',    'number, status, voltage_pu, voltage_angle, gen_mw, gen_mvar, load_mw, load_mvar, shunt_mw, shunt_mvar, freq')
    BusData    = namedtuple('BusData',    'number, status, voltage_pu, voltage_angle, freq')
    GenData    = namedtuple('GenData',    'bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, voltage_pu_setpoint, mw, mvar, mva, mw_setpoint, freq')
    LoadData   = namedtuple('LoadData',   'bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, mw, mvar, mva, freq')
    ShuntData  = namedtuple('ShuntData',  'bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, mvar, nominal_mvar, freq')
    BranchData = namedtuple('BranchData', 'from_bus, to_bus, circuit_id, status, mw_from, mvar_from, mva_from, amps_from, mw_to, mvar_to, mva_to, amps_to')


    def __init__(self, endpoint):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        if isinstance(endpoint, str):
            self.s.connect((endpoint, 5557))
        else:
            # `endpoint` expected to be `(addr, port)` tuple
            self.s.connect(endpoint)


        self.s.setblocking(0)

        msg = self.get_response()
        self.src_id = msg.src_id

        # ID used when making `tcmGetDataByID` calls. the first time we get
        # data, we use `tcmGetData` with a unique ID (per client) for the
        # request, and set this variable to the ID. subsequent requests can use
        # `tcmGetDataByID` with the unique ID.
        self.system_data_request_id = None


    # returns the next index to use and the value for the data type.
    def __parse_type(self, dtype, idx, data):
        end = idx + self.SIZES[dtype]

        if dtype == self.BYTE:
            return end, struct.unpack('!b', data[idx:end])[0]
        elif dtype == self.CARDINAL:
            return end, struct.unpack('!I', data[idx:end])[0]
        elif dtype == self.INTEGER:
            return end, struct.unpack('!i', data[idx:end])[0]
        elif dtype == self.FLOAT:
            return end, struct.unpack('!f', data[idx:end])[0]
        elif dtype == self.DOUBLE:
            return end, struct.unpack('!d', data[idx:end])[0]
        elif dtype == self.WIDE_CHAR:
            return end, struct.unpack('!H', data[idx:end])[0] # ***ADDED***
        elif dtype == self.WORD:
            return end, struct.unpack('!H', data[idx:end])[0]
        elif dtype == self.SMALL_INT:
            return end, struct.unpack('!h', data[idx:end])[0]
        elif dtype == self.STRING:
            # size gives us the number of wide characters in the string
            size = struct.unpack('!H', data[idx:end])[0]

            # `new_end` is what we originally thought was the end plus the
            # length of the string. each wide character in the string is 2
            # bytes.
            new_end = end + 2 * size

            # start reading at what we thought was the end originally, since in
            # the case of strings that's the end of the byte that represents
            # the size of the rest of the string.
            return new_end, data[end:new_end].decode('utf-16BE')
        elif dtype == self.PAYLOAD:
            # size gives us the number of bytes in the payload
            size = struct.unpack('!I', data[idx:end])[0]

            # `new_end` is what we originally thought was the end plus the size
            # of the payload.
            new_end = end + size

            # start reading at what we thought was the end originally, since in
            # the case of payloads that's the end of the byte that represents
            # the size of the rest of the payload.
            return new_end, data[end:new_end]
    

    def __parse_key(self, idx, data):
        end = idx + 2
        return end, struct.unpack('!' + 2 * 'B', data[idx:end])
 

    def __build_type(self, dtype, value):
        if dtype == self.BYTE:
            return struct.pack('!b', value)
        elif dtype == self.CARDINAL:
            return struct.pack('!I', value)
        elif dtype == self.INTEGER:
            return struct.pack('!i', value)
        elif dtype == self.FLOAT:
            return struct.pack('!f', value)
        elif dtype == self.DOUBLE:
            return struct.pack('!d', value)
        elif dtype == self.WIDE_CHAR:
            return struct.pack('!H', value) # ***ADDED***
        elif dtype == self.WORD:
            return struct.pack('!H', value)
        elif dtype == self.SMALL_INT:
            return struct.pack('!h', value)
        elif dtype == self.STRING:
            # since each character in the string is represented as a wide
            # character, and a wide character is 2 bytes, we need to divide the
            # length by 2 to get the number of wide characters.
            size = struct.pack('!H', int(len(value.encode('utf-16BE')) / 2))
            return b''.join([size, value.encode('utf-16BE')])
        elif dtype == self.PAYLOAD:
            if len(value) == 0:
                return struct.pack('!I', 0)
            else:
                return b''.join([struct.pack('!I', len(value)), value])


    def __build_key(self, version):
        return b''.join([struct.pack('!B', 111), struct.pack('!B', version)])


    def __build_request_message(self, msg_type, payload): #changed from msg_type to mtype
        # TODO: should version, and/or source ID be configurable at the
        # instance level?
        #msg = self.Message(version = 2, msg_type = mtype, src_id = 'vpdc', user_id = self.src_id, payload = b'') #should this be self.Message or PWDSAPI.Message

        key      = self.__build_key(2)
        msg_type = self.__build_type(self.WORD,    msg_type)
        src_id   = self.__build_type(self.STRING,  'vpdc')
        user_id  = self.__build_type(self.STRING,  self.src_id)
        payload  = self.__build_type(self.PAYLOAD, payload)

        # the `4` value here represents what size the framesize will be
        size      = 4 + len(key) + len(msg_type) + len(src_id) + len(user_id) + len(payload)
        framesize = self.__build_type(self.CARDINAL, size)

        # note that payload size gets incorporated into payload in the
        # `__build_type` function.
        return b''.join([key, framesize, msg_type, src_id, user_id, payload])


    # returns the next index to use and the object type.
    def parse_object_type(self, idx, data):
        idx, name  = self.__parse_type(self.STRING,   idx, data)
        idx, count = self.__parse_type(self.CARDINAL, idx, data)

        # these six calls could be done in a single call w/ something like
        # `struct.unpack('!'+6*'H', data[idx:idx+12])`
        # but making the six calls is easier to read/comprehend.
        idx, byte_fields   = self.__parse_type(self.WORD, idx, data)
        idx, word_fields   = self.__parse_type(self.WORD, idx, data)
        idx, int_fields    = self.__parse_type(self.WORD, idx, data)
        idx, float_fields  = self.__parse_type(self.WORD, idx, data)
        idx, double_fields = self.__parse_type(self.WORD, idx, data)
        idx, string_fields = self.__parse_type(self.WORD, idx, data)

        return idx, self.ObjectType(name, count, byte_fields, word_fields, int_fields, float_fields, double_fields, string_fields) #self.ObjectType or PWDSAPI.ObectType


    # returns the next index to use. used to parse objects that are to be
    # ignored. this is necessary because the object may contain strings, which
    # have unknown lengths at runtime.
    def parse_ignore(self, idx, obj, data):
        for _ in range(obj.count):
            idx += obj.byte_fields   * 1
            idx += obj.word_fields   * 2
            idx += obj.int_fields    * 4
            idx += obj.float_fields  * 4
            idx += obj.double_fields * 8

            if obj.string_fields != 0:
                for _ in range(obj.string_fields):
                    # the extra two bytes being skipped is for the 2-byte word
                    # representing the size of the string.
                    idx += 2 + 2*struct.unpack('!H', data[idx:idx+2])[0] #ADDED idx+2 instead of just 2


        return idx
        
# ADDED this function because the recv_n_bytes is special

    def recv_n_bytes(self, n):
        """ Convenient method for receiving exactly n bytes from self.socket
            (assuming it's open and connected).
        """
        data = b''
        while len(data) < n:
            ready=select([self.s], [], [], .01)
            if ready[0]:
                chunk = self.s.recv(n - len(data))
                if chunk == '':
                    break
                data = b''.join([data,chunk])
        return data


    def get_response(self):
        # Every message PWDS sends has a header prefixing the payload. The
        # first 6 bytes of the header are always fixed (well, really the first
        # 8 are fixed, but we'll use 6 to keep it simple since the first 6 give
        # us the size of the rest of the message).
        data = self.recv_n_bytes(6)

        # reset the index since we received new data.
        idx = 0

        # the first element of `key` should always be `111`. the second element
        # of `key` represents the version of the PWDS API.
        idx, key = self.__parse_key(idx, data)

        # `framesize` represents the total number of bytes in the message,
        # including itself and the key. So, the size of the rest of the message
        # is represented by `framesize - 6`.
        idx, framesize = self.__parse_type(self.CARDINAL, idx, data)

        # read the rest of the message off the wire.
        data = self.recv_n_bytes(framesize - 6)

        # reset the index since we received new data.
        idx = 0

        idx, msg_type = self.__parse_type(self.WORD,    idx, data)
        idx, src_id   = self.__parse_type(self.STRING,  idx, data)
        idx, user_id  = self.__parse_type(self.STRING,  idx, data)
        idx, payload  = self.__parse_type(self.PAYLOAD, idx, data)
        return self.Message(key[1], msg_type, src_id, user_id, payload) #self.Message or PWDSAPI.Message


    # right now this only grabs buses, gens, loads, shunts, and branches
    def get_system_dictionary(self):
        # `msg_type` 14 is `tcmDictionary`
        self.send_request(14, b'')

        msg = self.get_response()
        if msg.msg_type != 15:
            raise RuntimeError('expected msg type 15 (dsmDictionary)')


        # dictionary to store results in
        objects = {}

        # skip getting 16-byte case GUID
        idx, count = self.__parse_type(self.WORD, 16, msg.payload)

        # loop through and parse each object type
        for _ in range(count):
            idx, obj = self.parse_object_type(idx, msg.payload)
            if obj.name == self.BUS:
                buses = []

                for _ in range(obj.count):
                    idx, number = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, area   = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, zone   = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, substn = self.__parse_type(self.INTEGER, idx, msg.payload)

                    idx, nominal_kv = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, max_pu     = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, min_pu     = self.__parse_type(self.FLOAT, idx, msg.payload)

                    idx, name = self.__parse_type(self.STRING, idx, msg.payload)

                    buses.append(self.Bus(number, area, zone, substn, nominal_kv, max_pu, min_pu, name))


                objects['buses'] = buses
            elif obj.name == self.GEN:
                gens = []

                for _ in range(obj.count):
                    idx, _ = self.__parse_type(self.BYTE, idx, msg.payload)
                    idx, _ = self.__parse_type(self.BYTE, idx, msg.payload)
                    idx, _ = self.__parse_type(self.BYTE, idx, msg.payload)

                    idx, bus  = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, area = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, zone = self.__parse_type(self.INTEGER, idx, msg.payload)

                    idx, volt_pu_setpoint = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, mw_setpoint      = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, mva_base         = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, mw_max           = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, mw_min           = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, _                = self.__parse_type(self.FLOAT, idx, msg.payload)

                    idx, device_id = self.__parse_type(self.STRING, idx, msg.payload)
                    device_id = device_id.strip()

                    gens.append(self.Gen(bus, area, zone, volt_pu_setpoint, mw_setpoint, mva_base, mw_max, mw_min, device_id))


                objects['gens'] = gens
            elif obj.name == self.LOAD:
                loads = []

                for _ in range(obj.count):
                    idx, bus  = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, area = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, zone = self.__parse_type(self.INTEGER, idx, msg.payload)

                    idx, _ = self.__parse_type(self.FLOAT, idx, msg.payload)

                    idx, device_id = self.__parse_type(self.STRING, idx, msg.payload)
                    device_id = device_id.strip()

                    loads.append(self.Load(bus, area, zone, device_id))


                objects['loads'] = loads
            elif obj.name == self.SHUNT:
                shunts = []

                for _ in range(obj.count):
                    idx, bus  = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, area = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, zone = self.__parse_type(self.INTEGER, idx, msg.payload)

                    idx, nominal_mvar = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, device_id    = self.__parse_type(self.STRING, idx, msg.payload)
                    device_id = device_id.strip()

                    shunts.append(self.Shunt(bus, area, zone, nominal_mvar, device_id))


                objects['shunts'] = shunts
            elif obj.name == self.BRANCH:
                branches = []

                for _ in range(obj.count):
                    idx, option      = self.__parse_type(self.BYTE, idx, msg.payload)
                    idx, branch_type = self.__parse_type(self.BYTE, idx, msg.payload)
                    idx, section     = self.__parse_type(self.BYTE, idx, msg.payload)

                    idx, from_bus   = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, to_bus     = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, multi_from = self.__parse_type(self.INTEGER, idx, msg.payload)
                    idx, multi_to   = self.__parse_type(self.INTEGER, idx, msg.payload)

                    idx, mva_limit  = self.__parse_type(self.FLOAT, idx, msg.payload)
                    idx, circuit_id = self.__parse_type(self.STRING, idx, msg.payload)

                    branches.append(self.Branch(option, branch_type, section, from_bus, to_bus, multi_from, multi_to, mva_limit, circuit_id))


                objects['branches'] = branches
            else:
                idx = self.parse_ignore(idx, obj, msg.payload)

        return objects


    # right now this only grabs buses, gens, loads, shunts, and branches
    def get_system_data(self, buses, gens, loads, shunts, branches):
        # array to hold contents of payload that will be sent to PWDS.
        payload = []
        
        # `system_data_request_id` will not be None if we've already built and
        # made this request once before. if that's the case, short-circuit
        # building it again.
        if self.system_data_request_id is not None:
            payload.append(self.__build_type(self.SMALL_INT, self.system_data_request_id))

            # `msg_type` 12 is `tcmGetDataByID`
            self.send_request(12, b''.join(payload))
        else: # build request
            # unique (per client) request identifier. we'll use this to store the
            # request server-side for future calls.
            payload.append(self.__build_type(self.SMALL_INT, 1))

            # number of object types included in this request. we only care about 5
            # object types right now: buses, gens, loads, shunts, branches
            payload.append(self.__build_type(self.WORD, 5)) #ADDED 1 for test, instead of 5

            # ~~~ begin building requests for each object type ~~~

            # ~~~ BUSES ~~~

            payload.append(self.__build_type(self.STRING, 'Bus'))
            payload.append(self.__build_type(self.WORD,   4)) #changed from 10 to 1
            payload.append(self.__build_type(self.STRING, 'Vpu'))
            payload.append(self.__build_type(self.STRING, 'Vangle'))
            payload.append(self.__build_type(self.STRING, 'FreqHz'))
            payload.append(self.__build_type(self.STRING, 'Status'))
            #payload.append(self.__build_type(self.STRING, 'GenMW')) # for some reason the following values do not return
            #payload.append(self.__build_type(self.STRING, 'GenMvar'))
            #payload.append(self.__build_type(self.STRING, 'LoadMW'))
            #payload.append(self.__build_type(self.STRING, 'LoadMvar'))
            #payload.append(self.__build_type(self.STRING, 'ShuntMW'))
            #payload.append(self.__build_type(self.STRING, 'ShuntMvar'))

            ###ADDED changed to just 1 bus
            #payload.append(self.__build_type(self.INTEGER, 1))
            
            payload.append(self.__build_type(self.INTEGER, len(buses)))

            # set the ID for each bus, which is simply the bus number
            
            ###ADDED testing for 1 bus
            #payload.append(self.__build_type(self.INTEGER, 5381))
            
            for o in buses:  
                payload.append(self.__build_type(self.INTEGER, o.number))


            # ~~~ GENS ~~~

            payload.append(self.__build_type(self.STRING, 'Gen'))
            payload.append(self.__build_type(self.WORD,   11))
            payload.append(self.__build_type(self.STRING, 'Vpu'))
            payload.append(self.__build_type(self.STRING, 'Vangle'))
            payload.append(self.__build_type(self.STRING, 'KV'))
            payload.append(self.__build_type(self.STRING, 'FreqHz'))
            payload.append(self.__build_type(self.STRING, 'BusStatus'))
            payload.append(self.__build_type(self.STRING, 'Status'))
            payload.append(self.__build_type(self.STRING, 'MW'))
            payload.append(self.__build_type(self.STRING, 'Mvar'))
            payload.append(self.__build_type(self.STRING, 'MVA'))
            payload.append(self.__build_type(self.STRING, 'VpuSetpoint'))
            payload.append(self.__build_type(self.STRING, 'MWSetpoint'))

            payload.append(self.__build_type(self.INTEGER, len(gens)))

            # set the ID for each gen, which is the connected bus number and the
            # device ID.
            for o in gens:
                payload.append(self.__build_type(self.INTEGER, o.bus))
                payload.append(self.__build_type(self.STRING,  o.device_id))


            # ~~~ LOADS ~~~

            payload.append(self.__build_type(self.STRING, 'Load'))
            payload.append(self.__build_type(self.WORD,   9))
            payload.append(self.__build_type(self.STRING, 'vpu'))
            payload.append(self.__build_type(self.STRING, 'vangle'))
            payload.append(self.__build_type(self.STRING, 'KV'))
            payload.append(self.__build_type(self.STRING, 'FreqHz'))
            payload.append(self.__build_type(self.STRING, 'busstatus'))
            payload.append(self.__build_type(self.STRING, 'status'))
            payload.append(self.__build_type(self.STRING, 'mw'))
            payload.append(self.__build_type(self.STRING, 'mvar'))
            payload.append(self.__build_type(self.STRING, 'mva'))

            payload.append(self.__build_type(self.INTEGER, len(loads)))

            # set the ID for each load, which is the connected bus number and the
            # device ID.
            for o in loads:
                payload.append(self.__build_type(self.INTEGER, o.bus))
                payload.append(self.__build_type(self.STRING,  o.device_id))


            # ~~~ SHUNTS ~~~

            payload.append(self.__build_type(self.STRING, 'Shunt'))
            payload.append(self.__build_type(self.WORD,   8))
            payload.append(self.__build_type(self.STRING, 'vpu'))
            payload.append(self.__build_type(self.STRING, 'vangle'))
            payload.append(self.__build_type(self.STRING, 'kv'))
            payload.append(self.__build_type(self.STRING, 'freqhz'))
            payload.append(self.__build_type(self.STRING, 'busstatus'))
            payload.append(self.__build_type(self.STRING, 'status'))
            payload.append(self.__build_type(self.STRING, 'mvar'))
            payload.append(self.__build_type(self.STRING, 'mvarnom'))

            payload.append(self.__build_type(self.INTEGER, len(shunts)))

            # set the ID for each shunt, which is the connected bus number and the
            # device ID.
            for o in shunts:
                payload.append(self.__build_type(self.INTEGER, o.bus))
                payload.append(self.__build_type(self.STRING,  o.device_id))


            # ~~~ BRANCHES ~~~

            payload.append(self.__build_type(self.STRING, 'Branch'))
            payload.append(self.__build_type(self.WORD,   9))
            payload.append(self.__build_type(self.STRING, 'status'))
            payload.append(self.__build_type(self.STRING, 'mwfrom'))
            payload.append(self.__build_type(self.STRING, 'mvarfrom'))
            payload.append(self.__build_type(self.STRING, 'mvafrom'))
            payload.append(self.__build_type(self.STRING, 'ampsfrom'))
            payload.append(self.__build_type(self.STRING, 'mwto'))
            payload.append(self.__build_type(self.STRING, 'mvarto'))
            payload.append(self.__build_type(self.STRING, 'mvato'))
            payload.append(self.__build_type(self.STRING, 'ampsto'))

            payload.append(self.__build_type(self.INTEGER, len(branches)))

            # set the ID for each branch, which is the from bus number, to bus
            # number, and the circuit ID.
            for o in branches:
                payload.append(self.__build_type(self.INTEGER, o.from_bus))
                payload.append(self.__build_type(self.INTEGER, o.to_bus))
                payload.append(self.__build_type(self.STRING,  o.circuit_id))


            # ~~~ end building requests for each object type ~~~

            # `msg_type` 11 is `tcmGetData`
            self.send_request(11, b''.join(payload))


        msg = self.get_response()

        if msg.msg_type != 2:
            raise RuntimeError('expected msg type 2 (dsmSimulationData)')


        # ~~~ begin parsing responses for each object type ~~~

        # note that the response values will be in the same order defined in
        # the request above, and will all be floats.

        # dictionary to store results in
        data = {'buses': [], 'gens': [], 'loads': [], 'shunts': [], 'branches': []}
        
        # we skip the first 12 bytes of the dsmSimulationData response message
        # since we don't really care about the ID (we already know it),
        # simulation status, SOC, and microseconds count.
        idx = 12
        #print("data received =")
        #print(msg)

        # ~~~ BUSES ~~~

        for o in buses:
            #print("NEW BUS")
            idx, vpu       = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, vangle    = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, freqhz    = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, status    = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, genmw     = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, genmvar   = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, loadmw    = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, loadmvar  = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, shuntmw   = self.__parse_type(self.FLOAT, idx, msg.payload)
            #idx, shuntmvar = self.__parse_type(self.FLOAT, idx, msg.payload)

            data['buses'].append(self.BusData( #changed to self.BusData from BusData
                number        = o.number,
                voltage_pu    = vpu,
                voltage_angle = vangle,
                freq          = freqhz,
                status        = status              
                #gen_mw        = genmw,
                #gen_mvar      = genmvar,
                #load_mw       = loadmw,
                #load_mvar     = loadmvar,
                #shunt_mw      = shuntmw,
                #shunt_mvar    = shuntmvar
            ))


        # ~~~ GENS ~~~

        for o in gens:
            idx, vpu         = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, vangle      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, kv          = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, freqhz      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, busstatus   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, status      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mw          = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvar        = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mva         = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, vpusetpoint = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mwsetpoint  = self.__parse_type(self.FLOAT, idx, msg.payload)

            data['gens'].append(self.GenData(
                bus                 = o.bus,
                device_id           = o.device_id,
                voltage_pu          = vpu,
                voltage_angle       = vangle,
                voltage_kv          = kv,
                freq                = freqhz,
                bus_status          = busstatus,
                status              = status,
                mw                  = mw,
                mvar                = mvar,
                mva                 = mva,
                voltage_pu_setpoint = vpusetpoint,
                mw_setpoint         = mwsetpoint
            ))


        # ~~~ LOADS ~~~

        for o in loads:
            idx, vpu         = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, vangle      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, kv          = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, freqhz      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, busstatus   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, status      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mw          = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvar        = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mva         = self.__parse_type(self.FLOAT, idx, msg.payload)

            data['loads'].append(self.LoadData(
                bus                 = o.bus,
                device_id           = o.device_id,
                voltage_pu          = vpu,
                voltage_angle       = vangle,
                voltage_kv          = kv,
                freq                = freqhz,
                bus_status          = busstatus,
                status              = status,
                mw                  = mw,
                mvar                = mvar,
                mva                 = mva
            ))


        # ~~~ SHUNTS ~~~

        for o in shunts:
            idx, vpu         = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, vangle      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, kv          = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, freqhz      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, busstatus   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, status      = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvar        = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvarnom     = self.__parse_type(self.FLOAT, idx, msg.payload)

            data['shunts'].append(self.ShuntData(
                bus                 = o.bus,
                device_id           = o.device_id,
                voltage_pu          = vpu,
                voltage_angle       = vangle,
                voltage_kv          = kv,
                freq                = freqhz,
                bus_status          = busstatus,
                status              = status,
                mvar                = mvar,
                nominal_mvar        = mvarnom
            ))


        # ~~~ BRANCHES ~~~

        for o in branches:
            idx, status   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mwfrom   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvarfrom = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvafrom  = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, ampsfrom = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mwto     = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvarto   = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, mvato    = self.__parse_type(self.FLOAT, idx, msg.payload)
            idx, ampsto   = self.__parse_type(self.FLOAT, idx, msg.payload)

            data['branches'].append(self.BranchData(
                from_bus   = o.from_bus,
                to_bus     = o.to_bus,
                circuit_id = o.circuit_id,
                status     = status,
                mw_from    = mwfrom,
                mvar_from  = mvarfrom,
                mva_from   = mvafrom,
                amps_from  = ampsfrom,
                mw_to      = mwto,
                mvar_to    = mvarto,
                mva_to     = mvato,
                amps_to    = ampsto
            ))

        
        # ~~~ end parsing responses for each object type ~~~

        # since the request was successful, and thus should have been stored
        # server-side, set the request ID so we can short-circuit it on future
        # calls.
        self.system_data_request_id = 1

        return data


    def connect_generator(self, gen):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData): ### ADDED self to all these Data ones
            raise RuntimeError('function attribute must be either a Gen or GenData')


        # randomly chosen, hard-coded ID for connecting generators
        payload = [self.__build_type(self.SMALL_INT, 30)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this generator
        payload.append(self.__build_type(self.STRING,  'Gen'))
        payload.append(self.__build_type(self.INTEGER, gen.bus))
        payload.append(self.__build_type(self.STRING,  gen.device_id))

        # close the generator to connect it to the bus
        payload.append(self.__build_type(self.STRING, 'CLOSE'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def disconnect_generator(self, gen):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError('function attribute must be either a Gen or GenData')


        # randomly chosen, hard-coded ID for disconnecting generators
        payload = [self.__build_type(self.SMALL_INT, 31)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this generator
        payload.append(self.__build_type(self.STRING,  'Gen'))
        payload.append(self.__build_type(self.INTEGER, gen.bus))
        payload.append(self.__build_type(self.STRING,  gen.device_id))

        # open the generator to disconnect it from the bus
        payload.append(self.__build_type(self.STRING, 'OPEN'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def set_generator_pu_voltage(self, gen, vpu):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError('function attribute must be either a Gen or GenData')


        # randomly chosen, hard-coded ID for setting generator per-unit voltage
        payload = [self.__build_type(self.SMALL_INT, 32)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this generator
        payload.append(self.__build_type(self.STRING,  'Gen'))
        payload.append(self.__build_type(self.INTEGER, gen.bus))
        payload.append(self.__build_type(self.STRING,  gen.device_id))

        cmd = 'SET Exciter_Setpoint {:f} pu'.format(vpu)
        payload.append(self.__build_type(self.STRING, cmd))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def set_generator_mw_output(self, gen, mw):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError('function attribute must be either a Gen or GenData')


        # randomly chosen, hard-coded ID for setting generator MW output
        payload = [self.__build_type(self.SMALL_INT, 33)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this generator
        payload.append(self.__build_type(self.STRING,  'Gen'))
        payload.append(self.__build_type(self.INTEGER, gen.bus))
        payload.append(self.__build_type(self.STRING,  gen.device_id))

        cmd = 'SET Power {:f} MW'.format(mw)
        payload.append(self.__build_type(self.STRING, cmd))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def connect_load(self, load):
        if not isinstance(load, self.Load) and not isinstance(load, self.LoadData):
            raise RuntimeError('function attribute must be either a Load or LoadData')


        # randomly chosen, hard-coded ID for connecting loads
        payload = [self.__build_type(self.SMALL_INT, 40)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this load
        payload.append(self.__build_type(self.STRING,  'Load'))
        payload.append(self.__build_type(self.INTEGER, load.bus))
        payload.append(self.__build_type(self.STRING,  load.device_id))

        # close the load to connect it to the bus
        payload.append(self.__build_type(self.STRING, 'CLOSE'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def disconnect_load(self, load):
        if not isinstance(load, self.Load) and not isinstance(load, self.LoadData):
            raise RuntimeError('function attribute must be either a Load or LoadData')


        # randomly chosen, hard-coded ID for disconnecting loads
        payload = [self.__build_type(self.SMALL_INT, 41)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this load
        payload.append(self.__build_type(self.STRING,  'Load'))
        payload.append(self.__build_type(self.INTEGER, load.bus))
        payload.append(self.__build_type(self.STRING,  load.device_id))

        # open the load to disconnect it from the bus
        payload.append(self.__build_type(self.STRING, 'OPEN'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def connect_shunt(self, shunt):
        if not isinstance(shunt, self.Shunt) and not isinstance(shunt, self.ShuntData):
            raise RuntimeError('function attribute must be either a Shunt or ShuntData')


        # randomly chosen, hard-coded ID for connecting shunts
        payload = [self.__build_type(self.SMALL_INT, 50)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this shunt
        payload.append(self.__build_type(self.STRING,  'Shunt'))
        payload.append(self.__build_type(self.INTEGER, shunt.bus))
        payload.append(self.__build_type(self.STRING,  shunt.device_id))

        # close the shunt to disconnect it to the bus
        payload.append(self.__build_type(self.STRING, 'CLOSE'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def disconnect_shunt(self, shunt):
        if not isinstance(shunt, self.Shunt) and not isinstance(shunt, self.ShuntData):
            raise RuntimeError('function attribute must be either a Shunt or ShuntData')


        # randomly chosen, hard-coded ID for disconnecting shunts
        payload = [self.__build_type(self.SMALL_INT, 51)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this shunt
        payload.append(self.__build_type(self.STRING,  'Shunt'))
        payload.append(self.__build_type(self.INTEGER, shunt.bus))
        payload.append(self.__build_type(self.STRING,  shunt.device_id))

        # open the shunt to disconnect it from the bus
        payload.append(self.__build_type(self.STRING, 'OPEN'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def connect_branch(self, branch):
        if not isinstance(branch, self.Branch) and not isinstance(branch, self.BranchData):
            raise RuntimeError('function attribute must be either a Branch or BranchData')


        # randomly chosen, hard-coded ID for connecting branches
        payload = [self.__build_type(self.SMALL_INT, 60)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this branch
        payload.append(self.__build_type(self.STRING,  'Branch'))
        payload.append(self.__build_type(self.INTEGER, branch.from_bus))
        payload.append(self.__build_type(self.INTEGER, branch.to_bus))
        payload.append(self.__build_type(self.STRING,  branch.circuit_id))

        # close the branch at both ends to connect it
        payload.append(self.__build_type(self.STRING, 'CLOSE BOTH'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def disconnect_branch(self, branch):
        if not isinstance(branch, self.Branch) and not isinstance(branch, self.BranchData):
            raise RuntimeError('function attribute must be either a Branch or BranchData')


        # randomly chosen, hard-coded ID for disconnecting branches
        payload = [self.__build_type(self.SMALL_INT, 61)]

        # these are both set to 0 to cause the command to execute immediately.
        payload.append(self.__build_type(self.CARDINAL, 0))
        payload.append(self.__build_type(self.INTEGER,  0))

        # set the object type and identifier for this branch
        payload.append(self.__build_type(self.STRING,  'Branch'))
        payload.append(self.__build_type(self.INTEGER, branch.from_bus))
        payload.append(self.__build_type(self.INTEGER, branch.to_bus))
        payload.append(self.__build_type(self.STRING,  branch.circuit_id))

        # open the branch at both ends to disconnect it
        payload.append(self.__build_type(self.STRING, 'OPEN BOTH'))

        # `msg_type` 13 is `tcmCommand`
        self.send_request(13, b''.join(payload))

        msg = self.get_response()

        # dsmOK (16), dsmError (17)
        if msg.msg_type != 16:
            raise RuntimeError('expected msg type 16 (dsmOK)')


    def send_request(self, msg_type, payload):
        msg = self.__build_request_message(msg_type, payload)
        #print(msg)
        self.s.sendall(msg) #changed packet to msg
