"""
Low-level API for talking to PowerWorld Dynamics Studio (beta).

See pysceptre/docs/PWDS-Protocol.pdf for details on the protocol.
"""

from collections import namedtuple


class PWDSAPI:
    # PWDS object name constants
    BUS = "Bus"
    GEN = "Gen"
    LOAD = "Load"
    SHUNT = "Shunt"
    BRANCH = "Branch"

    # represents a PWDS message returned to API caller
    Message = namedtuple("Message", "version, msg_type, src_id, user_id, payload")

    # represents a generic PWDS ObjectType
    ObjectType = namedtuple(
        "ObjectType",
        "name, count, byte_fields, word_fields, int_fields, float_fields, double_fields, string_fields",
    )

    # represent specific PWDS dictionary objects
    Bus = namedtuple(
        "Bus", "number, area, zone, substn, nominal_kv, max_pu, min_pu, name"
    )
    Gen = namedtuple(
        "Gen",
        "bus, area, zone, volt_pu_setpoint, mw_setpoint, mva_base, mw_max, mw_min, device_id",
    )
    Load = namedtuple("Load", "bus, area, zone, device_id")
    Shunt = namedtuple("Shunt", "bus, area, zone, nominal_mvar, device_id")
    Branch = namedtuple(
        "Branch",
        "option, branch_type, section, from_bus, to_bus, multi_from, multi_to, mva_limit, circuit_id",
    )

    # represent specific PWDS data objects
    BusData = namedtuple("BusData", "number, status, voltage_pu, voltage_angle, freq")
    GenData = namedtuple(
        "GenData",
        "bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, voltage_pu_setpoint, mw, mvar, mva, mw_setpoint, freq",
    )
    LoadData = namedtuple(
        "LoadData",
        "bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, mw, mvar, mva, freq",
    )
    ShuntData = namedtuple(
        "ShuntData",
        "bus, device_id, status, bus_status, voltage_pu, voltage_angle, voltage_kv, mvar, nominal_mvar, freq",
    )
    BranchData = namedtuple(
        "BranchData",
        "from_bus, to_bus, circuit_id, status, mw_from, mvar_from, mva_from, amps_from, mw_to, mvar_to, mva_to, amps_to",
    )

    def __init__(self, endpoint):
        self.system_data_request_id = None

    def get_system_dictionary(self):
        objects = {}

        buses = []
        buses.append(
            self.Bus(
                1,  # number,
                1,  # area,
                1,  # zone,
                1,  # substn,
                1.0,  # nominal_kv,
                2.0,  # max_pu,
                1.0,  # min_pu,
                "bus-1",  # name
            )
        )
        objects["buses"] = buses

        gens = []
        gens.append(
            self.Gen(
                1,  # bus
                1,  # area
                1,  # zone
                1.0,  # volt_pu_setpoint
                1.0,  # mw_setpoint
                1.0,  # mva_base
                2.0,  # mw_max
                1.0,  # mw_min
                "gen-1",  # device_id
            )
        )
        objects["gens"] = gens

        loads = []
        loads.append(
            self.Load(
                1,  # bus
                1,  # area
                1,  # zone
                "load-1",  # device_id
            )
        )
        objects["loads"] = loads

        shunts = []
        shunts.append(
            self.Shunt(
                1,  # bus
                1,  # area
                1,  # zone
                1.0,  # nominal_mvar
                "shunt-1",  # device_id
            )
        )
        objects["shunts"] = shunts

        branches = []
        branches.append(
            self.Branch(
                1,  # option
                1,  # branch_type
                1,  # section
                1,  # from_bus
                1,  # to_bus
                1,  # multi_from
                1,  # multi_to
                1.0,  # mva_limit
                "branch-1",  # circuit_id
            )
        )
        objects["branches"] = branches

        return objects

    def get_system_data(self, buses, gens, loads, shunts, branches):
        data = {}

        for o in buses:
            data["buses"] = []

            data["buses"].append(
                self.BusData(
                    number=o.number,
                    voltage_pu=1.0,
                    voltage_angle=1.0,
                    freq=1.0,
                    status=1,
                    # gen_mw        = genmw,
                    # gen_mvar      = genmvar,
                    # load_mw       = loadmw,
                    # load_mvar     = loadmvar,
                    # shunt_mw      = shuntmw,
                    # shunt_mvar    = shuntmvar
                )
            )

        for o in gens:
            data["gens"] = []

            data["gens"].append(
                self.GenData(
                    bus=o.bus,
                    device_id=o.device_id,
                    voltage_pu=1.0,
                    voltage_angle=1.0,
                    voltage_kv=1.0,
                    freq=1.0,
                    bus_status=1,
                    status=1,
                    mw=1.0,
                    mvar=1.0,
                    mva=1.0,
                    voltage_pu_setpoint=1.0,
                    mw_setpoint=1.0,
                )
            )

        for o in loads:
            data["loads"] = []

            data["loads"].append(
                self.LoadData(
                    bus=o.bus,
                    device_id=o.device_id,
                    voltage_pu=1.0,
                    voltage_angle=1.0,
                    voltage_kv=1.0,
                    freq=1.0,
                    bus_status=1,
                    status=1,
                    mw=1.0,
                    mvar=1.0,
                    mva=1.0,
                )
            )

        for o in shunts:
            data["shunts"] = []

            data["shunts"].append(
                self.ShuntData(
                    bus=o.bus,
                    device_id=o.device_id,
                    voltage_pu=1.0,
                    voltage_angle=1.0,
                    voltage_kv=1.0,
                    freq=1.0,
                    bus_status=1,
                    status=1,
                    mvar=1.0,
                    nominal_mvar=1.0,
                )
            )

        for o in branches:
            data["branches"] = []

            data["branches"].append(
                self.BranchData(
                    from_bus=o.from_bus,
                    to_bus=o.to_bus,
                    circuit_id=o.circuit_id,
                    status=1,
                    mw_from=1.0,
                    mvar_from=1.0,
                    mva_from=1.0,
                    amps_from=1.0,
                    mw_to=1.0,
                    mvar_to=1.0,
                    mva_to=1.0,
                    amps_to=1.0,
                )
            )

        return data

    def connect_generator(self, gen):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError("function attribute must be either a Gen or GenData")
        print(f"generator {gen.device_id} connected")

    def disconnect_generator(self, gen):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError("function attribute must be either a Gen or GenData")
        print(f"generator {gen.device_id} disconnected")

    def set_generator_pu_voltage(self, gen, vpu):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError("function attribute must be either a Gen or GenData")
        print(f"generator {gen.device_id} pu_voltage set to {vpu}")

    def set_generator_mw_output(self, gen, mw):
        if not isinstance(gen, self.Gen) and not isinstance(gen, self.GenData):
            raise RuntimeError("function attribute must be either a Gen or GenData")
        print(f"generator {gen.device_id} mw_output set to {mw}")

    def connect_load(self, load):
        if not isinstance(load, self.Load) and not isinstance(load, self.LoadData):
            raise RuntimeError("function attribute must be either a Load or LoadData")
        print(f"load {load.device_id} connected")

    def disconnect_load(self, load):
        if not isinstance(load, self.Load) and not isinstance(load, self.LoadData):
            raise RuntimeError("function attribute must be either a Load or LoadData")
        print(f"load {load.device_id} disconnected")

    def connect_shunt(self, shunt):
        if not isinstance(shunt, self.Shunt) and not isinstance(shunt, self.ShuntData):
            raise RuntimeError("function attribute must be either a Shunt or ShuntData")
        print(f"shunt {shunt.device_id} connected")

    def disconnect_shunt(self, shunt):
        if not isinstance(shunt, self.Shunt) and not isinstance(shunt, self.ShuntData):
            raise RuntimeError("function attribute must be either a Shunt or ShuntData")
        print(f"shunt {shunt.device_id} disconnected")

    def connect_branch(self, branch):
        if not isinstance(branch, self.Branch) and not isinstance(
            branch, self.BranchData
        ):
            raise RuntimeError(
                "function attribute must be either a Branch or BranchData"
            )
        print(f"branch {branch.circuit_id} connected")

    def disconnect_branch(self, branch):
        if not isinstance(branch, self.Branch) and not isinstance(
            branch, self.BranchData
        ):
            raise RuntimeError(
                "function attribute must be either a Branch or BranchData"
            )
        print(f"branch {branch.circuit_id} disconnected")
