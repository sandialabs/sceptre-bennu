import atexit
import logging
import time

from pymodbus.client import ModbusTcpClient
from pymodbus.payload import BinaryPayloadDecoder, BinaryPayloadBuilder
from pymodbus.constants import Endian
from pymodbus.exceptions import ModbusException
from pymodbus import pymodbus_apply_logging_config

from pybennu.settings import ModbusRegister


class ModbusWrapper:
    """
    Wrapper class for Modbus operations.
    """
    def __init__(self, ip: str, port: int, timeout: float = 5.0, debug: bool = False):
        self.ip: str = ip
        self.port: int = port
        self.timeout: float = timeout
        self.debug: bool = debug
        self.connected: bool = False

        self.client: ModbusTcpClient = ModbusTcpClient(
            host=self.ip,
            port=self.port,
            timeout=self.timeout,
        )

        # DEBUG level for pymodbus outputs for EVERY request, only use
        # if you're debugging a really hairy modbus issue.
        # Otherwise, we set WARNING+ normally, and INFO+ if debug is set.
        # This must be called because logging basicConfig() in the provider
        # will result in the pymodbus logger also getting configured, and
        # it defaults to DEBUG level.
        if self.debug:
            pymodbus_apply_logging_config(logging.INFO)
        else:
            pymodbus_apply_logging_config(logging.WARNING)

        # Close modbus connection on exit.
        # register the "at exit" handler here instead of
        # on connection so it only gets registered once
        # for a given client. Calling close() if connection
        # isn't opened is safe for pymodbus
        # (i looked at the source ok).
        atexit.register(self.client.close)

        # Configure logging
        self.log: logging.Logger = logging.getLogger(f"Modbus [{self!s}]")
        self.log.setLevel(logging.DEBUG if self.debug else logging.INFO)
        self.log.info(f"Initialized {self!r}")

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(ip={self.ip}, port={self.port}, timeout={self.timeout})"

    def __str__(self) -> str:
        return f"{self.ip}:{self.port}"

    def connect(self) -> bool:
        return self.client.connect()

    def disconnect(self) -> None:
        self.client.close()

    def retry_until_connected(self, delay: float = 5.0) -> None:
        """
        Retry connecting to server until the connection is successful.
        """
        self.disconnect()

        while True:
            try:
                if self.connect():
                    self.log.info("Successfully reconnected!")
                    return
            except ModbusException:
                pass

            self.log.warning(f"Failed to connect, waiting {delay} seconds before retrying connection...")
            self.disconnect()
            time.sleep(delay)

    @staticmethod
    def decode_float(registers: list) -> float:
        """
        Function to DECODE a 32-bit float from two 16-bit registers.
        """
        decoder = BinaryPayloadDecoder.fromRegisters(registers, byteorder=Endian.BIG, wordorder=Endian.BIG)
        return decoder.decode_32bit_float()

    @staticmethod
    def encode_float(value: float) -> list:
        """
        Function to ENCODE a 32-bit float into two 16-bit registers.
        """
        builder = BinaryPayloadBuilder(byteorder=Endian.BIG, wordorder=Endian.BIG)
        builder.add_32bit_float(value)
        return builder.to_registers()

    def read_register(self, r: ModbusRegister):
        """
        Read a value from a Modbus register.

        Args:
            r: Register to read from

        Returns:
            The value read

        Raises:
            ValueError: an attempt was made to read a write-only register
            TypeError: incorrect reg_type
            pymodbus.exceptions.ModbusException: error with Modbus communications
        """
        # TODO: configurable logging levels. As this is, if there are 197 registers,
        # this will emit thousands of messages per second...not useful.
        # if self.debug:
        #     self.log.debug(f"Reading {r.reg_type} register {r.name} (num: {r.num})")

        if "r" not in r.access:
            raise ValueError(f"Attempt to read write-only register {r.name} (access: {r.access})")

        # TODO: what exception is raised by PyModbus if response is an error?
        # TODO: handle different register widths (16 bit vs 32 bit)
        # TODO: read multiple registers
        if r.reg_type == 'input':
            # read 2 registers for a 32-bit register (2x 16-bit)
            result = self.client.read_input_registers(r.num, 2)
            return self.decode_float(result.registers)
        elif r.reg_type == 'holding':
            result = self.client.read_holding_registers(r.num, 2)
            return self.decode_float(result.registers)
        elif r.reg_type == 'discrete':
            result = self.client.read_discrete_inputs(r.num, 1)
            return result.bits[0]
        elif r.reg_type == 'coil':
            result = self.client.read_coils(r.num, 1)
            return result.bits[0]
        else:
            raise TypeError(f"Unknown reg_type: {r.reg_type} (register: {r})")

    def write_register(self, r: ModbusRegister, value) -> None:
        """
        Write a value to a Modbus register.

        Args:
            r: Register to write to
            value: the raw value to write

        Raises:
            ValueError: an attempt was made to write a read-only register
            TypeError: incorrect reg_type
            NotImplementedError: reg_type that isn't implemented yet
            pymodbus.exceptions.ModbusException: error with Modbus communications
        """
        if self.debug:
            self.log.debug(f"Writing {r.reg_type} register {r.name} with value '{value}' (num: {r.num})")

        if "w" not in r.access:
            raise ValueError(f"Attempt to write to read-only register {r.name} (access: {r.access})")

        # TODO: types other than float and bool
        # TODO: other types of registers
        # TODO: what exception is raised by PyModbus if response is an error?
        if r.reg_type == 'holding':  # Holding Registers
            encoded_value = self.encode_float(value)
            self.client.write_registers(r.num, encoded_value)
        elif r.reg_type == 'coil':  # Coil
            self.client.write_coil(r.num, value)
        elif r.reg_type in ['discrete', 'input']:
            raise NotImplementedError(f"reg_type {r.reg_type} is not supported for writing for register {r.name}")
        else:
            raise TypeError(f"Unknown reg_type: {r.reg_type} (register: {r})")
