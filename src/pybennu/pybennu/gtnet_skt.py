"""
Implementation of the GTNET-SKT protocol for the RTDS GTNETx2 communications module.

## GTNET-SKT protocol description

Refer to the Section 7H2 of RTDS Controls Library manual for details
("cc_man.pdf", included with the RSCAD software distribution).

**Overview**
- 32-bit (4 byte) values
- Big-endian (network byte order)
- Integers or Single-precision IEEE 751 Floating-point numbers
- List of ints/floats is packed into packet sent to the GTNET card
- The order, types, and tags they map to is defined in the "GTNET-SKT MULTI" block in the RSCAD project
- GTNET-SKT requires that ALL configured values are written in a single packet

**Channels**
- Max of 30 data points per channel
- Max of 10 channels total (30 * 10 = 300 total points per card)
- Each channel has it's own IP and port (though port could be same)

### !! WARNING !!

Too high of a write rate (>1khz?) will overload and crash the GTNET card!
If this happens, it may need to be reset by flipping the power switch on
the back of the card off, then on. It may also be enough to reset the RTDS
power using the power switch on the front of the rack.

### About writes...

As stated above, GTNET-SKT requires that ALL configured values are written in
a single packet, with the order, types, and tags determined by what's defined
in RSCAD. This is because it relies on the order of tags to unpack the packet
into values.

What this means is all writes to the socket MUST contain ALL values, and
the ORDER of those values in the packet MATTERS! Since SCEPTRE provides the
ability to write to individual tags, e.g. change value of tag 'BREAKER-1'
to a '1', this leaves us in a bit of a pickle.

To work around this issue, the provider will maintain an internal (in-memory)
state of the last known values for each tag. When a write occurs to a tag or
set of tags, the state will be updated with values from the write. Then, the
current (now updated) state will be written to the GTNET-SKT socket. At startup,
the initial state will be populated from the provider config file (.ini).

As far as I know there isn't a way to read the current state of GTNET-SKT points
unless you do some massive hacks involving PMU measurements, which add further
complexity to an already needlessly complicated RSCAD project. So, while this
solution isn't perfect, it's good enough to meet the HARMONIE project objectives
in the short-term.
"""

import logging
import socket
import struct
from time import sleep
from typing import Dict, Optional, Union
from threading import Thread, Lock

from pybennu.settings import PybennuSettings, GtnetSktTag


class GtnetSkt:

    def __init__(self, conf: PybennuSettings) -> None:
        self.conf: PybennuSettings = conf

        # Configure logging
        self.log: logging.Logger = logging.getLogger("GTNET-SKT")
        self.log.setLevel(logging.DEBUG)

        # Socket to be used for writes
        # This can be TCP (SOCK_STREAM) or UDP (SOCK_DGRAM)
        self.socket: Optional[socket.socket] = None

        # Thread lock
        self.lock: Lock = Lock()

        # Dict of tags, keyed by name
        self.tags: Dict[str, GtnetSktTag] = {
            t.name: t for t in self.conf.gtnet_skt.tags
        }
        self.log.debug(f"tags: {self.tags}")

        # Determines how struct will be packed. This will be appended to.
        self.struct_format_string: str = "!"  # '!' => network byte order

        # Tracks current state of values for all GTNET-SKT points
        # State is initialized using "initial_value" from the tag in the config
        # Refer to docstring for RTDS.write() for details
        self.state = {}  # type: Dict[str, int | float]

        # Initialize state using "initial_value" from the tag in the config
        # Also, pre-generate the struct format, since it won't change
        for gtag in self.conf.gtnet_skt.tags:
            if gtag.type == "int":
                self.state[gtag.name] = int(gtag.initial_value)
                self.struct_format_string += "i"
            elif gtag.type == "float":
                self.state[gtag.name] = float(gtag.initial_value)
                self.struct_format_string += "f"
            else:
                raise ValueError(f"invalid type {gtag.type} for GTNET-SKT tag {gtag.name}")

        if self.conf.gtnet_skt.protocol == "udp":
            self.udp_write_thread: Thread = Thread(target=self.continuous_write, daemon=True)

    def start(self):
        self.init_socket()

        # if udp, then start thread
        if self.conf.gtnet_skt.protocol == "udp":
            self.udp_write_thread.start()

    def continuous_write(self) -> None:
        """
        Continuously write values to RTDS via GTNET-SKT using UDP protocol.
        """
        sleep_for = 1.0 / float(self.conf.gtnet_skt.udp_write_rate)
        target = (str(self.conf.gtnet_skt.ip), self.conf.gtnet_skt.port)

        self.log.info(f"Starting GTNET-SKT UDP writer thread [sleep_for={sleep_for}, target={target}]")
        # self.init_socket()   # this is called in start()

        while True:
            payload = self.build_payload()
            self.socket.sendto(payload, target)
            sleep(sleep_for)

    def tcp_write_state(self):
        """
        Build and write current state over TCP, retrying until successful.
        """
        payload = self.build_payload()

        if self.conf.debug:
            self.log.debug(f"Raw GTNET TCP write payload: {payload}")

        if not self.socket:
            self.init_socket()

        sent = False
        while not sent:
            try:
                self.socket.send(payload)
                sent = True
            except Exception:
                self.log.exception("GTNET-SKT send failed, resetting connection...")
                self.init_socket()

    def build_payload(self) -> bytes:
        """
        Generate the payload bytes to be sent across the socket.
        NOTE: see docstring at top of this file for details on GTNET-SKT payload structure.
        """
        # mutex to prevent threads from writing while we're reading
        with self.lock:
            values = list(self.state.values())

        return struct.pack(self.struct_format_string, *values)

    def init_socket(self) -> None:
        """
        Initialize TCP or UDP socket, and if TCP connection fails, retry until it's successful.
        """
        self.close_socket()

        # TCP socket for synchronous connection.
        # Connection will fail if RSCAD case isn't running.
        if self.conf.gtnet_skt.protocol == "tcp":
            target = (str(self.conf.gtnet_skt.ip), self.conf.gtnet_skt.port)
            connected = False

            while not connected:  # loop to handle connection failures
                try:
                    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    self.socket.connect(target)
                    connected = True
                except Exception:
                    self.log.error(f"Failed to connect to GTNET-SKT {target}, sleeping for {self.conf.gtnet_skt.tcp_retry_delay} seconds before retrying")
                    if self.conf.debug:  # only log traceback if debugging
                        self.log.exception(f"traceback for GTNET-SKT {target}")
                    self.close_socket()
                    sleep(self.conf.gtnet_skt.tcp_retry_delay)

        # UDP socket for continually spamming data to the RTDS
        else:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close_socket(self) -> None:
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None

    def typecast_values(
        self, to_typecast: Dict[str, Union[str, int, float]]
    ) -> Dict[str, Union[int, float]]:
        typecasted = {}

        for tag, val in to_typecast.items():
            expected_type = self.tags[tag].type  # "int" or "float"

            # Validate types match what's in config, if they don't, then warn and typecast
            # NOTE: these will be strings if coming from pybennu-probe
            if not isinstance(val, str) and type(val).__name__ != expected_type:
                self.log.warning(
                    f"Data type of tag '{tag}' is '{type(val).__name__}', not "
                    f"'{expected_type}', forcing a typecast. If "
                    f"you're using 'pybennu-probe', this is normal."
                )

            # Conver booleans to integers, since gtnet-skt doesn't have a bool type
            if val is False or (isinstance(val, str) and val.lower() == "false"):
                val = "0"
            elif val is True or (isinstance(val, str) and val.lower() == "true"):
                val = "1"

            if expected_type == "int":
                typecasted[tag] = int(val)
            else:
                typecasted[tag] = float(val)

        return typecasted
