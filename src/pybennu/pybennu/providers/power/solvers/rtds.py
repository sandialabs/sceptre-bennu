"""
SCEPTRE Provider for the Real-Time Dynamic Simulator (RTDS).

## Overview

Data is read via C37.118 protocol from Phasor Measurement Unit (PMU) interface on the RTDS GTNET card.

Data is written via GTNET-SKT protocol to the RTDS GTNET card.


## GTNET-SKT protocol for the RTDS

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


## CSV files
Data read from the PMUs is saved to CSV files in the directory specified by the
csv-file-path configuration option, if csv-enabled is True.

CSV header example:
```
rtds_time,sceptre_time,freq,dfreq,VA_real,VA_angle,VB_real,VB_angle,VC_real,VC_angle,IA_real,IA_angle,IB_real,IB_angle,IC_real,IC_angle,NA_real,NA_angle,NA_real,NA_angle
```

CSV filename example: PMU1_BUS4-1_25-04-2022_23-49-22.csv


## Elasticsearch

Data read from the PMUs is exported to the Elasticsearch server specified by the
elastic-host configuration option, if elastic-enabled is True.

Index name: `<basename>-<YYYY.MM.DD>` (e.g. rtds-clean-2023.06.08)

Index name is configurable in power-provider_config.ini using 'elastic-index-basename'.
By default (if you copy config.ini), it's 'rtds-clean', e.g. 'rtds-clean-2023.06.08'.
If elastic-index-basename isn't set, then it defaults to 'rtds-default', e.g. 'rtds-default-2023.06.08'.

### Index mapping

| field                    | type          | example                   | description |
| ------------------------ | ------------- | ------------------------- | ----------- |
| @timestamp               | date          | 2022-04-20:11:22:33.000   | Timestamp from SCEPTRE. This should match the value of sceptre_time. |
| rtds_time                | date          | 2022-04-20:11:22:33.000   | Timestamp from RTDS. |
| sceptre_time             | date          | 2022-04-20:11:22:33.000   | Timestamp from SCEPTRE provider (the power-provider VM in the emulation). |
| event.ingested           | date          | 2022-04-20:11:22:33.000   | Timestamp of when the data was ingested into Elasticsearch. |
| ecs.version              | keyword       | 8.8.0                     | [Elastic Common Schema (ECS)](https://www.elastic.co/guide/en/ecs/current/ecs-field-reference.html) version this schema adheres to. |
| agent.type               | keyword       | rtds-sceptre-provider     | Type of system providing the data |
| agent.version            | keyword       | e28642b                   | Version of the provider. This is the version of bennu, which is usually the commit short sha. If not able to be determined, this will be `"unknown"`. |
| observer.hostname        | keyword       | power-provider            | Hostname of the system providing the data. |
| observer.geo.timezone    | keyword       | America/Denver            | Timezone of the system providing the data. |
| network.protocol         | keyword       | dnp3                      | Network protocol used to retrieve the data. Currently, this will be either dnp3 or c37.118. |
| network.transport        | keyword       | tcp                       | Transport layer (Layer 4 of OSI model) protocol used to retrieve the data. Currently, this is usually `tcp`, but it could be udp if UDP is used for C37.118 or GTNET-SKT. |
| pmu.name                 | keyword       | PMU1                      | Name of the PMU. |
| pmu.label                | keyword       | BUS4-1                    | Label for the PMU. |
| pmu.ip                   | ip            | 172.24.9.51               | IP address of the PMU. |
| pmu.port                 | integer       | 4714                      | TCP port of the PMU. |
| pmu.id                   | long          | 41                        | PDC ID of the PMU. |
| measurement.stream       | byte          | 1                         | Stream ID of this measurement from the PMU. |
| measurement.status       | keyword       | ok                        | Status of this measurement from the PMU. |
| measurement.time         | double        | 1686089097.13333          | Absolute time of when the measurement occurred. This timestamp can be used as a sequence number, and will match across all PMUs from the same RTDS case. |
| measurement.frequency    | double        | 60.06                     | Nominal system frequency. |
| measurement.dfreq        | double        | 8.835189510136843e-05     | Rate of change of frequency (ROCOF). |
| measurement.channel      | keyword       | PHASOR CH 1:VA            | Channel name of this measurement from the PMU. |
| measurement.phasor.id    | byte          | 0                         | ID of the phasor. For example, if there are 4 phasors, then the ID of the first phasor will be 0. |
| measurement.phasor.real  | double        | 132786.5                  | Phase magnitude? |
| measurement.phasor.angle | double        | -1.5519471168518066       | Phase angle? |

## Notes

NOTE: in the bennu VM, rtds.py is located in dist-packages.
Just in case you need to modify it on the fly ;)
/usr/lib/python3/dist-packages/pybennu/providers/power/solvers/rtds.py

If observing C37.118 traffic in Wireshark, configure manual decode for
each PMU port and set the protocol to "SYNCHROPHASOR" (synphasor).
Wireshark -> "Analyze..." -> "Decode As..."
"""

import atexit
import json
import logging
import os
import platform
import re
import socket
import struct
import sys
import threading
import queue
from concurrent.futures import ThreadPoolExecutor
from configparser import ConfigParser
from datetime import datetime, timezone
from io import TextIOWrapper
from pathlib import Path
from pprint import pformat
from subprocess import check_output
from time import sleep
from typing import Any, Dict, List, Optional, Literal

from elasticsearch import Elasticsearch, helpers

from pybennu.distributed.provider import Provider
from pybennu.pypmu.synchrophasor.frame import CommonFrame, DataFrame, HeaderFrame
from pybennu.pypmu.synchrophasor.pdc import Pdc

# TODO: support PMU "digital" fields (mm["digital"])
# TODO: support PMU "analog" fields, current handling is a hack for HARMONIE LDRD
# TODO: push state of GTNET-SKT values to Elasticsearch
# TODO: log most messages to a file with Rotating handler to avoid filling up log (since bennu isn't very smart and won't rotate the file)
# TODO: add field to Elastic data differentiating which SCEPTRE experiment data is associated with

def str_to_bool(val: str) -> bool:
    """
    Convert a string representation of truth to :obj:`True` or :obj:`False`.

    True values are: 'y', 'yes', 't', 'true', 'on', '1', 'enable', 'enabled'

    False values are: 'n', 'no', 'f', 'false', 'off', '0', 'disable', 'disabled'

    Args:
        val: String to evaluate

    Returns:
        The boolean representation of the string

    Raises:
        ValueError: val is anything other than a boolean value
    """
    val = val.strip().lower()
    if val in ("y", "yes", "t", "true", "on", "1", "enable", "enabled"):
        return True
    elif val in ("n", "no", "f", "false", "off", "0", "disable", "disabled"):
        return False
    else:
        raise ValueError(f"invalid bool string {val}")


# datetime.utcnow() returns a naive datetime objects (no timezone info).
# when .timestamp() is called on these objects, they get interpreted as
# local time, not UTC time, which leads to incorrect timestamps.
# Further reading: https://blog.miguelgrinberg.com/post/it-s-time-for-a-change-datetime-utcnow-is-now-deprecated
def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def utc_now_formatted() -> str:
    return utc_now().strftime("%d-%m-%Y_%H-%M-%S")


class RotatingCSVWriter:
    """
    Writes data to CSV files, creating a new file when a limit is reached.
    """

    def __init__(
        self,
        name: str,
        csv_dir: Path,
        header: List[str],
        filename_base: str = "rtds_pmu_data",
        rows_per_file: int = 1000000,
        max_files: int = 0
    ) -> None:
        self.name: str = name
        self.csv_dir: Path = csv_dir
        self.header: List[str] = header
        self.filename_base: str = filename_base
        self.max_rows: int = rows_per_file
        self.max_files: int = max_files
        self.files_written: List[Path] = []
        self.rows_written: int = 0
        self.current_path: Optional[Path] = None
        self.fp: Optional[TextIOWrapper] = None

        self.log = logging.getLogger(f"{self.__class__.__name__} [{self.name}]")
        self.log.setLevel(logging.DEBUG)

        if not self.csv_dir.exists():
            self.csv_dir.mkdir(parents=True, exist_ok=True, mode=0o666)  # rw-rw-rw-

        self.log.info(f"CSV output directory: {self.csv_dir}")
        self.log.info(f"CSV header: {self.header}")

        # ensure data is written on exit
        atexit.register(self._close_file)

        # initial file rotation
        self.rotate()

    def _close_file(self) -> None:
        if self.fp and not self.fp.closed:
            self.fp.flush()
            os.fsync(self.fp.fileno())  # ensure data is written to disk
            self.fp.close()

    def rotate(self) -> None:
        self._close_file()  # close current CSV before starting new one
        if self.current_path:
            self.log.debug(f"Wrote {self.rows_written} rows and {self.current_path.stat().st_size} bytes to {self.current_path}")
            self.files_written.append(self.current_path)

        timestamp = utc_now_formatted()
        filename = f"{self.filename_base}_{timestamp}.csv"
        self.current_path = Path(self.csv_dir, filename)
        self.log.info(f"Rotating CSV file to {self.current_path}")
        if self.current_path.exists():
            self.log.error(f"{self.current_path} already exists! Overwriting...")

        # Set file permissions: User/Group/World Read/Write (rw-rw-rw-)
        self.current_path.touch(mode=0o666, exist_ok=True)
        self.fp = self.current_path.open("w", encoding="utf-8", newline="\n")
        self._emit(self.header)  # Write CSV header
        self.rows_written = 0  # Reset row counter

        if self.max_files and len(self.files_written) > self.max_files:
            oldest = self.files_written.pop(0)  # oldest file is first in list
            self.log.info(f"Removing CSV file {oldest}")
            oldest.unlink()  # delete the file

    def _emit(self, data: list) -> None:
        """Write comma-separated list of values."""
        for i, column in enumerate(data):
            self.fp.write(str(column))
            if i < len(data) - 1:
                self.fp.write(",")
        self.fp.write("\n")

    def write(self, data: list) -> None:
        """Write data to CSV file."""
        if self.rows_written == self.max_rows:
            self.rotate()

        if len(data) != len(self.header):
            raise RuntimeError(f"length of CSV data ({len(data)}) does not match length of CSV header ({len(self.header)})")
        assert len(data) == len(self.header)

        self._emit(data)
        self.rows_written += 1


class PMU:
    """
    Wrapper class for a Phasor Measurement Unit (PMU).

    Polls for data using C37.118 protocol, utilizing the pypmu library under-the-hood.
    """
    def __init__(
        self,
        ip: str,
        port: int,
        pdc_id: int,
        name: str = "",
        label: str = "",
        protocol: Literal["tcp", "udp"] = "tcp"
    ) -> None:
        self.ip: str = ip
        self.port: int = port
        self.pdc_id: int = pdc_id
        self.name: str = name
        self.label: str = label
        self.protocol: Literal["tcp", "udp"] = protocol

        # TODO: UDP isn't implemented yet in pyPMU
        if self.protocol not in ["tcp", "udp"]:
            raise ValueError(f"PMU protocol must be 'tcp' or 'udp', got {self.protocol}")

        # Configure PDC instance (pypmu.synchrophasor.pdc.Pdc)
        self.pdc = Pdc(
            pdc_id=self.pdc_id,
            pmu_ip=self.ip,
            pmu_port=self.port,
            method=self.protocol
        )

        self.pdc_header: Optional[HeaderFrame] = None
        self.pdc_config: Optional[CommonFrame] = None
        self.channel_names: List[str] = []
        self.csv_writer: Optional[RotatingCSVWriter] = None
        self.station_name: str = ""

        # Configure logging
        self.log: logging.Logger = logging.getLogger(f"PMU [{str(self)}]")
        self.log.setLevel(logging.DEBUG)
        self.pdc.logger = logging.getLogger(f"Pdc [{str(self)}]")
        # NOTE: pypmu logs a LOT of stuff at DEBUG, leave at INFO
        # unless you're doing deep debugging of the pypmu code.
        self.pdc.logger.setLevel(logging.INFO)
        self.log.info(f"Initialized {repr(self)}")

    def __repr__(self) -> str:
        return f"PMU(ip={self.ip}, port={self.port}, pdc_id={self.pdc_id}, name={self.name}, label={self.label}, protocol={self.protocol})"

    def __str__(self) -> str:
        if self.name and self.label:
            return f"{self.name}_{self.label}"
        elif self.name:
            return self.name
        else:
            return f"{self.ip}:{self.port}_{self.pdc_id}"

    def run(self) -> None:
        """Connect to PMU."""
        self.pdc.run()

        # NOTE (03/30/2022): some SEL PDCs respond to header requests and don't need them
        try:
            self.pdc_header = self.pdc.get_header()  # Get header message from PMU
            self.log.debug(f"PMU header: {self.pdc_header.__dict__}")
        except Exception as ex:
            self.log.warning(f"Failed to get header: {ex} (device may be a SEL PDC, or something else happened)")

        self.pdc_config = self.pdc.get_config()  # Get configuration from PMU
        self.log.debug(f"PMU config: {self.pdc_config.__dict__}")
        if "_station_name" in self.pdc_config.__dict__:
            self.station_name = self.pdc_config.__dict__["_station_name"].strip()
            self.log.info(f"PMU Station Name: {self.station_name}")
        else:
            self.log.warning("No station_name from PMU, which is a bit odd")

        # Raw: ["PHASOR CH 1:VA  ", "PHASOR CH 2:VB  ", "PHASOR CH 3:VC  ",
        #       "PHASOR CH 4:IA  ", "PHASOR CH 5:IB  ", "PHASOR CH 6:IC  "]
        # Post-processing: ["VA", "VB", "VC", "IA", "IB", "IC"]
        def _process_name(cn: str) -> str:
            """Strip 'PHASOR CH *' from channel names, so we get a nice 'VA', 'IA', etc."""
            return re.sub(r"PHASOR CH \d\:", "", cn.strip(), re.IGNORECASE | re.ASCII).strip()
        self.channel_names = []  # type: List[str]
        for channel in self.pdc_config.__dict__.get("_channel_names", []):
            if isinstance(channel, list):
                # NOTE (03/30/2022): channel names can be lists of strings instead of strings
                for n in channel:
                    self.channel_names.append(_process_name(n))
            else:
                self.channel_names.append(_process_name(channel))
        self.log.debug(f"Channel names: {self.channel_names}")

    def start(self) -> None:
        self.pdc.start()

    def get_data_frame(self) -> Optional[Dict[str, Any]]:
        data = self.pdc.get()  # Keep receiving data
        if not data:
            self.log.error("Failed to get data from PMU!")
            return None

        if not isinstance(data, DataFrame):
            self.log.critical(f"Invalid type {type(data).__name__}. Raw data: {repr(data)}")
            return None

        data_frame = data.get_measurements()

        if data_frame["pmu_id"] != self.pdc_id:
            self.log.warning(f"The received PMU ID {data_frame['pmu_id']} is not same as configured ID {self.pdc_id}")

        return data_frame


class RTDS(Provider):
    """
    SCEPTRE Provider for the Real-Time Dynamic Simulator (RTDS).
    """

    REQUIRED_CONF_KEYS = [  # NOTE: these are checked in ../power_daemon.py
        "server-endpoint", "publish-endpoint", "publish-rate", "rtds-retry-delay", "rtds-rack-ip",
        "rtds-pmu-ips", "rtds-pmu-ports", "rtds-pmu-names", "rtds-pmu-labels", "rtds-pdc-ids",
        "csv-enabled", "csv-file-path", "csv-rows-per-file", "csv-max-files",
        "gtnet-skt-ip", "gtnet-skt-port", "gtnet-skt-protocol", "gtnet-skt-tag-names",
        "gtnet-skt-tag-types", "gtnet-skt-initial-values",
        "elastic-enabled", "elastic-host",
    ]

    def __init__(
        self,
        server_endpoint,
        publish_endpoint,
        config: ConfigParser,
        debug: bool = False
    ) -> None:
        Provider.__init__(self, server_endpoint, publish_endpoint)

        # Thread locks
        self.__lock = threading.Lock()
        self.__gtnet_lock = threading.Lock()

        # Load configuration values
        self.config: ConfigParser = config
        self.debug = debug  # type: bool
        self.publish_rate = float(self._conf("publish-rate"))  # type: float

        # RTDS config
        self.retry_delay = float(self._conf("rtds-retry-delay"))  # type: float
        self.rack_ip = self._conf("rtds-rack-ip")  # type: str
        self.pmu_ips = self._conf("rtds-pmu-ips", is_list=True)  # type: List[str]
        self.pmu_names = self._conf("rtds-pmu-names", is_list=True)  # type: List[str]
        self.pmu_ports = self._conf("rtds-pmu-ports", is_list=True, convert=int)  # type: List[int]
        self.pmu_labels = self._conf("rtds-pmu-labels", is_list=True)  # type: List[str]
        self.pdc_ids = self._conf("rtds-pdc-ids", is_list=True, convert=int)  # type: List[int]

        # CSV config
        self.csv_enabled = str_to_bool(self._conf("csv-enabled"))  # type: bool
        self.csv_path = Path(self._conf("csv-file-path")).expanduser().resolve()  # type: Path
        self.csv_rows_per_file = int(self._conf("csv-rows-per-file"))  # type: int
        self.csv_max_files = int(self._conf("csv-max-files"))  # type: int

        # GTNET-SKT config
        self.gtnet_skt_ip = self._conf("gtnet-skt-ip")  # type: str
        self.gtnet_skt_port = int(self._conf("gtnet-skt-port"))  # type: int
        self.gtnet_skt_protocol = self._conf("gtnet-skt-protocol").lower()  # type: str
        self.gtnet_skt_tag_names = self._conf("gtnet-skt-tag-names", is_list=True)  # type: List[str]
        self.gtnet_skt_tag_types = self._conf("gtnet-skt-tag-types", is_list=True, convert=lambda x: x.lower())  # type: List[str]
        self.gtnet_skt_initial_values = self._conf("gtnet-skt-initial-values", is_list=True, convert=lambda x: x.lower())  # type: List[str]

        # backwards compatibility with old topologies
        if self._has_conf("gtnet-skt-tcp-retry-delay"):
            self.gtnet_skt_tcp_retry_delay = float(self._conf("gtnet-skt-tcp-retry-delay"))
        else:
            self.gtnet_skt_tcp_retry_delay = 1.0

        if self._has_conf("gtnet-skt-udp-write-rate"):
            self.gtnet_skt_udp_write_rate = int(self._conf("gtnet-skt-udp-write-rate"))
        else:
            self.gtnet_skt_udp_write_rate = 30

        # Elastic config
        self.elastic_enabled = str_to_bool(self._conf("elastic-enabled"))  # type: bool
        self.elastic_host = self._conf("elastic-host")  # type: str
        if self._has_conf("elastic-index-basename"):
            self.elastic_index_basename = self._conf("elastic-index-basename")  # type: str
        else:
            self.elastic_index_basename = "rtds-default"

        # rtds-pmu-protocols
        if self._has_conf("rtds-pmu-protocols"):
            self.pmu_protocols = self._conf("rtds-pmu-protocols", is_list=True)  # type: List[str]
        else:
            self.pmu_protocols = ["tcp" for _ in range(len(self.pmu_ips))]
        if not all(p in ["tcp", "udp"] for p in self.pmu_protocols):
            raise ValueError("rtds-pmu-protocols must be either 'tcp' or 'udp'")

        # Validate configuration values
        if self.retry_delay <= 0.0:
            raise ValueError(f"'rtds-retry-delay' value must be a positive float, not {self.retry_delay}")
        if self.rack_ip.count(".") != 3:
            raise ValueError(f"invalid IP for 'rtds-rack-ip': {self.rack_ip}")
        if self.csv_rows_per_file <= 0:
            raise ValueError(f"'csv-rows-per-file' must be a positive integer, not {self.csv_rows_per_file}")
        if not self.pmu_ips or any(x.count(".") != 3 for x in self.pmu_ips):
            raise ValueError(f"invalid value(s) for 'rtds-pmu-ips': {self.pmu_ips}")
        if not (len(self.pmu_ips) == len(self.pmu_names) == len(self.pmu_ports) == len(self.pmu_labels) == len(self.pdc_ids) == len(self.pmu_protocols)):
            raise ValueError("lengths of pmu configuration options don't match, are you missing a pmu in one of the options?")

        # Validate GTNET-SKT configuration values only if there are values configured
        if self.gtnet_skt_tag_types:
            if self.gtnet_skt_ip.count(".") != 3:
                raise ValueError(f"invalid IP for 'gtnet-skt-ip': {self.gtnet_skt_ip}")
            if self.gtnet_skt_protocol not in ["tcp", "udp"]:
                raise ValueError(f"invalid protocol '{self.gtnet_skt_protocol}' for 'gtnet_skt_protocol', must be 'tcp' or 'udp'")
            if len(self.gtnet_skt_tag_names) != len(self.gtnet_skt_tag_types):
                raise ValueError("length of 'gtnet-skt-tag-names' doesn't match length of 'gtnet-skt-tag-types'")
            if len(self.gtnet_skt_initial_values) != len(self.gtnet_skt_tag_types):
                raise ValueError("length of 'gtnet-skt-initial-values' doesn't match length of 'gtnet-skt-tag-types'")
            if any(t not in ["int", "float"] for t in self.gtnet_skt_tag_types):
                raise ValueError("invalid type present in 'gtnet-skt-tag-types', only 'int' or 'float' are allowed")
            if self.gtnet_skt_tcp_retry_delay <= 0.0:
                raise ValueError(f"'gtnet-skt-tcp-retry-delay' value must be a positive float, not {self.gtnet_skt_tcp_retry_delay}")
            if self.gtnet_skt_udp_write_rate <= 0:
                raise ValueError(f"'gtnet-skt-udp-write-rate' must be a positive integer, not {self.gtnet_skt_udp_write_rate}")
            if len(self.gtnet_skt_tag_names) > 30:  # max of 30 data points per channel, 10 channels total for GTNET card
                raise ValueError(f"maximum of 30 points allowed per GTNET-SKT channel, {len(self.gtnet_skt_tag_names)} are defined in config")

        # Configure logging to stdout (includes debug messages from pypmu)
        logging.basicConfig(
            level=logging.DEBUG if self.debug else logging.INFO,
            format="%(asctime)s.%(msecs)03d [%(levelname)s] (%(name)s) %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
            stream=sys.stdout
        )
        self.log = logging.getLogger(self.__class__.__name__)
        self.log.setLevel(logging.DEBUG if self.debug else logging.INFO)

        self.log.info(f"Debug: {self.debug}")
        if not self.csv_enabled:
            self.log.warning("CSV output is DISABLED (since the 'csv-enabled' option is False)")
        elif not self.csv_max_files:
            self.log.warning("No limit set in 'csv-max-files', CSV files won't be cleaned up and could fill all available disk space")

        # Get bennu version
        self.bennu_version = "unknown"
        try:
            apt_result = check_output("apt show bennu", shell=True).decode()
            self.bennu_version = re.search(r"Version: (\w+)\s", apt_result).groups()[0]
        except Exception as ex:
            self.log.warning(f"Failed to get bennu version: {ex}")

        # Elasticsearch setup
        self._es_index_cache = set()
        self.es_queue = queue.Queue()  # queue for data to push to elastic
        if self.elastic_enabled:
            logging.getLogger("elastic_transport").setLevel(logging.WARNING)
            logging.getLogger("elasticsearch").setLevel(logging.WARNING)
            logging.getLogger("urllib3.connectionpool").setLevel(logging.WARNING)
        else:
            self.log.warning("Elasticsearch output is DISABLED (since the 'elastic-enabled' option is False)")

        # Create PMU instances: IP, Port, Name, Label, PDC ID
        self.pmus = []
        pmu_info = zip(self.pmu_ips, self.pmu_names, self.pmu_ports, self.pmu_labels, self.pdc_ids, self.pmu_protocols)
        for ip, name, port, label, pdc_id, protocol in pmu_info:
            pmu = PMU(ip=ip, port=port, pdc_id=pdc_id, name=name, label=label, protocol=protocol)
            self._rebuild_pmu_connection(pmu)  # build PMU connection, with automatic retry
            self.pmus.append(pmu)
        self.log.info(f"Instantiated and started {len(self.pmus)} PMUs")

        # Current values, keyed by tag name string
        self.current_values = {}  # type: Dict[str, Any]

        # Socket to be used for writes. This avoids opening/closing a TCP connection on every write
        self.__gtnet_socket = None  # type: Optional[socket.socket]

        self.gtnet_skt_tags = {name: typ for name, typ in zip(self.gtnet_skt_tag_names, self.gtnet_skt_tag_types)}  # type: Dict[str, str]
        self.log.debug(f"gtnet_skt_tags: {self.gtnet_skt_tags}")

        # Tracks current state of values for all GTNET-SKT points
        # The state is initialized using 'gtnet-skt-initial-values' from provider config
        # Refer to docstring for RTDS.write() for details
        # Also, pre-generate the struct format, since it won't change
        self.struct_format_string = "!"  # '!' => network byte order
        self.gtnet_skt_state = {}  # type: Dict[str, int | float]

        for name, typ, val in zip(self.gtnet_skt_tag_names, self.gtnet_skt_tag_types, self.gtnet_skt_initial_values):
            if typ == "int":
                self.gtnet_skt_state[name] = int(val)
                self.struct_format_string += "i"
            elif typ == "float":
                self.gtnet_skt_state[name] = float(val)
                self.struct_format_string += "f"
            else:
                raise ValueError(f"invalid type {typ} for GTNET-SKT tag {name} with initial value {val}")

        # Allow gtnet-skt fields to be read
        self.current_values.update(self.gtnet_skt_state)

        # Start GTNET-SKT writer thread if using UDP protocol
        if self.gtnet_skt_protocol == "udp":
            self.__gtnet_thread = threading.Thread(target=self._gtnet_continuous_write)
            self.__gtnet_thread.start()

        # Start data writer thread
        self.frame_queue = queue.Queue()
        self.__data_thread = threading.Thread(target=self._data_writer)
        self.__data_thread.start()

        # Begin polling PMUs
        self.__pmu_thread = threading.Thread(target=self._start_poll_pmus)
        self.__pmu_thread.start()

        # Start Elasticsearch pusher thread
        if self.elastic_enabled:
            self.__es_executor = ThreadPoolExecutor()
            for _ in range(3):
                self.__es_executor.submit(self._elastic_pusher)

        self.log.info("RTDS initialization is finished")

    def _has_conf(self, key: str) -> bool:
        return self.config.has_option("power-solver-service", key)

    def _conf(self, key: str, is_list: bool = False, convert=None) -> Any:
        """
        Read a value out of the configuration file section for the service.
        """
        val = self.config.get("power-solver-service", key)

        if isinstance(val, str):
            val = val.strip()

        if is_list:
            if not isinstance(val, str):
                raise ValueError(f"expected comma-separated list for '{key}'")

            val = [x.strip() for x in val.split(",") if x.strip()]

            # Type convert each value in the list
            if convert:
                val = [convert(x) for x in val]

        return val

    def _start_poll_pmus(self) -> None:
        """
        Query for data from the PMUs via C37.118 as fast as possible.
        """
        self.log.info(f"Starting polling threads for {len(self.pmus)} PMUs...")
        threads = []
        for pmu in self.pmus:
            pmu_thread = threading.Thread(target=self._poll_pmu, args=(pmu,))
            pmu_thread.start()
            threads.append(pmu_thread)

        # Save PMU metadata (configs and/or headers) to file
        metadata = {}
        for pmu in self.pmus:
            # Wait until configs have been pulled to save them
            while not self.pmus[0].pdc_config:
                sleep(1)

            metadata[str(pmu)] = {"config": pmu.pdc_config.__dict__}
            if pmu.pdc_header:
                metadata[str(pmu)]["header"] = pmu.pdc_header.__dict__

        timestamp = utc_now_formatted()

        # Save PMU metadata to JSON file
        meta_path = Path(self.csv_path, f"pmu_metadata_{timestamp}.json")
        if not meta_path.parent.exists():
            meta_path.parent.mkdir(exist_ok=True, parents=True)
        self.log.info(f"Writing metadata from {len(metadata)} PMUs to {meta_path}")
        meta_path.write_text(json.dumps(metadata, indent=4), encoding="utf-8")

        # Wait for values to be read from PMUs
        while not self.current_values or self.current_values == self.gtnet_skt_state:
            sleep(1)

        # Save the tag names to a file after there are values from the PMUs
        tags_path = Path(self.csv_path, f"tags_{timestamp}.txt")
        tags = list(self.current_values.keys())
        self.log.info(f"Writing {len(tags)} tag names to {tags_path}")
        tags_path.write_text("\n".join(tags), encoding="utf-8")

        # Block on PMU threads
        for thread in threads:
            thread.join()

    def _rebuild_pmu_connection(self, pmu: PMU) -> None:
        """
        Rebuild TCP connection to PMU if connection fails.

        For example, if RTDS simulation is stopped, the PMUs no longer exist.
        When the simulation restarts, this provider should automatically reconnect
        to the PMUs and start getting data again.
        """
        if pmu.pdc.pmu_socket:
            pmu.pdc.quit()

        while True:
            try:
                # Create TCP connection and get header(s)
                pmu.run()

                # Send start frame to kick off PMU data
                # NOTE (cegoes, 6/1/2023): this fixed a long-standing bug where
                # rebuilding connections to PMUs, where connections would be
                # rebuilt but data wouldn't be updating. This resulted in the
                # provider having to be manually restarted whenever the
                # RSCAD case was restarted.
                pmu.start()

                break
            except Exception as ex:
                self.log.error(f"Failed to rebuild PMU connection to {str(pmu)} due to error '{ex}', sleeping for {self.retry_delay} seconds before attempting again...")
                if pmu.pdc.pmu_socket:
                    pmu.pdc.quit()
                sleep(self.retry_delay)

        self.log.debug(f"(re)initialized PMU {str(pmu)}")

    def _poll_pmu(self, pmu: PMU) -> None:
        """
        Continually polls for data from a PMU and updates self.current_values.

        NOTE: This method is intended to be run in a thread,
        since it's loop that runs forever until killed.
        """
        self.log.info(f"Started polling thread for {str(pmu)}")
        retry_count = 0

        while True:
            try:
                data_frame = pmu.get_data_frame()
            except Exception as ex:
                self.log.error(f"Failed to get data frame from {str(pmu)} due to an exception '{ex}', attempting to rebuild connection...")
                if self.debug:  # only log traceback if debugging
                    self.log.exception(f"traceback for {str(pmu)}")
                self._rebuild_pmu_connection(pmu)
                continue

            if not data_frame:
                if retry_count >= 3:
                    self.log.error(f"Failed to request data {retry_count} times from {str(pmu)}, attempting to rebuild connection...")
                    self._rebuild_pmu_connection(pmu)
                    retry_count = 0
                else:
                    retry_count += 1
                    self.log.error(f"No data in frame from {str(pmu)}, sleeping for {self.retry_delay} seconds before retrying (retry count: {retry_count})")
                    sleep(self.retry_delay)
                continue

            self.frame_queue.put((pmu, data_frame))

    def _data_writer(self) -> None:
        """
        Handles processing of PMU data to avoid blocking the PMU polling thread.
        """
        self.log.info("Starting data writer thread")

        while True:
            # print(f"frame queue: {self.frame_queue.qsize()}")

            # This will block until there is an item to process
            pmu, frame = self.frame_queue.get()
            sceptre_ts = utc_now()

            for mm in frame["measurements"]:
                if mm["stat"] != "ok":
                    pmu.log.error(f"Bad/unknown PMU status: {mm['stat']}")
                if mm["stream_id"] != pmu.pdc_id:
                    pmu.log.warning(f"Unknown PMU stream ID: {mm['stream_id']} (expected {pmu.pdc_id})")

                # TODO: support PMU "digital" fields (mm["digital"])
                line = {
                    "stream_id": mm["stream_id"],  # int
                    "time": frame["time"],  # float
                    "freq": mm["frequency"],  # float
                    "dfreq": mm["rocof"],  # float
                    "phasors": {  # dict of dict, keyed by phasor ID (int)
                        # tuple of floats, (real, angle)
                        i: {"real": ph[0], "angle": ph[1]}
                        for i, ph in enumerate(mm["phasors"])
                    },
                }

                # Save data to Elasticsearch
                if self.elastic_enabled:
                    rtds_datetime = datetime.fromtimestamp(frame["time"], timezone.utc)
                    es_bodies = []

                    for ph_id, phasor in line["phasors"].items():
                        es_body = {
                            "@timestamp": sceptre_ts,
                            "rtds_time": rtds_datetime,
                            "sceptre_time": sceptre_ts,
                            "network": {
                                "protocol": "c37.118",
                                "transport": pmu.protocol,
                            },
                            "pmu": {
                                "name": pmu.name,
                                "label": pmu.label,
                                "ip": pmu.ip,
                                "port": pmu.port,
                                "id": pmu.pdc_id,
                            },
                            "measurement": {
                                "stream": mm["stream_id"],  # int
                                "status": mm["stat"],  # str
                                "time": frame["time"],  # float
                                "frequency": mm["frequency"],  # float
                                "dfreq": mm["rocof"],  # float
                                "channel": pmu.channel_names[ph_id],  # str
                                "phasor": {
                                    "id": ph_id,  # int
                                    "real": phasor["real"],  # float
                                    "angle": phasor["angle"],  # float
                                },
                            },
                        }
                        es_bodies.append(es_body)
                    self.es_queue.put(es_bodies)

                # Update global data structure with measurements
                #
                # NOTE: since there are usually multiple threads querying from multiple PMUs
                # simultaneously, a lock mutex is used to ensure self.current_values doesn't
                # result in a race condition or corrupted data.
                with self.__lock:
                    for ph_id, ph in line["phasors"].items():
                        # two types: "real", "angle"
                        for ph_type, val in ph.items():
                            # Example: BUS6_VA.real
                            tag = f"{pmu.label}_{pmu.channel_names[ph_id]}.{ph_type}"
                            self.current_values[tag] = val

                    # TODO: better handling of analog values, this is a hack for the HARMONIE LDRD
                    if mm["analog"]:
                        for i, analog_value in enumerate(mm["analog"]):
                            self.current_values[f"{pmu.name}_ANALOG_{i+1}.real"] = analog_value
                            self.current_values[f"{pmu.name}_ANALOG_{i+1}.angle"] = analog_value

                # Create CSV writer if it doesn't exist
                if self.csv_enabled and pmu.csv_writer is None:
                    # Build the CSV header
                    header = ["rtds_time", "sceptre_time", "freq", "dfreq"]
                    for i, ph in line["phasors"].items():
                        for k in ph.keys():
                            # Example: PHASOR CH 1:VA_real
                            header.append(f"{pmu.channel_names[i]}_{k}")

                    pmu.csv_writer = RotatingCSVWriter(
                        name=str(pmu),
                        csv_dir=self.csv_path / str(pmu),
                        header=header,
                        filename_base=f"{str(pmu)}",
                        rows_per_file=self.csv_rows_per_file,
                        max_files=self.csv_max_files
                    )

                # Write data to CSV file
                if self.csv_enabled:
                    csv_row = [line["time"], sceptre_ts.timestamp(), line["freq"], line["dfreq"]]
                    for ph in line["phasors"].values():
                        for v in ph.values():
                            csv_row.append(v)
                    pmu.csv_writer.write(csv_row)

    def _elastic_pusher(self) -> None:
        """
        Thread that sends PMU data to Elasticsearch using the Elasticsearch Bulk API.
        """
        self.log.info("Starting Elasticsearch pusher thread")

        # Only need to create these fields once, cache and copy into messages
        es_additions = {
            "ecs": {
                "version": "8.8.0"
            },
            "agent": {
                "type": "rtds-sceptre-provider",
                "version": self.bennu_version,
            },
            "observer": {
                "hostname": platform.node(),
                "geo": {
                    "timezone": str(datetime.now(timezone.utc).astimezone().tzinfo)
                }
            },
        }

        # Field type mapping for Elasticsearch.
        # NOTE: the table of fields is in the docstring at the top of this file
        # https://www.elastic.co/guide/en/elasticsearch/reference/7.9/mapping-types.html
        # https://www.elastic.co/guide/en/elasticsearch/reference/7.9/number.html
        es_type_mapping = {
            "@timestamp": {"type": "date"},
            "rtds_time": {"type": "date"},
            "sceptre_time": {"type": "date"},
            "event": {"properties": {"ingested": {"type": "date"}}},
            "ecs": {"properties": {"version": {"type": "keyword"}}},
            "agent": {
                "properties": {
                    "type": {"type": "keyword"},
                    "version": {"type": "keyword"},
                }
            },
            "observer": {
                "properties": {
                    "hostname": {"type": "keyword"},
                    "geo": {"properties": {"timezone": {"type": "keyword"}}},
                }
            },
            "network": {
                "properties": {
                    "protocol": {"type": "keyword"},
                    "transport": {"type": "keyword"},
                }
            },
            "pmu": {
                "properties": {
                    "name": {"type": "keyword"},
                    "label": {"type": "keyword"},
                    "ip": {"type": "ip"},
                    "port": {"type": "integer"},
                    "id": {"type": "long"},
                }
            },
            "measurement": {
                "properties": {
                    "stream": {"type": "byte"},
                    "status": {"type": "keyword"},
                    "time": {"type": "double"},
                    "frequency": {"type": "double"},
                    "dfreq": {"type": "double"},
                    "channel": {"type": "keyword"},
                    "phasor": {
                        "properties": {
                            "id": {"type": "byte"},
                            "real": {"type": "double"},
                            "angle": {"type": "double"},
                        }
                    },
                }
            }
        }

        self.log.info(f"Connecting to Elasticsearch host {self.elastic_host}")
        es = Elasticsearch(self.elastic_host)
        if not es.ping():  # cause connection to be created
            raise RuntimeError("failed to connect to Elasticsearch")

        while True:
            # print(f"elastic queue: {self.es_queue.qsize()}")
            # Batch up messages before sending them all in a single bulk request
            messages = []
            while len(messages) < 48:  # 8 PMUs * 6 channels
                # This blocks until there are messages
                messages.extend(self.es_queue.get())

            # TODO: there is potential for docs to be pushed to the wrong
            # date index at the start or end of a day.
            ts_now = utc_now()
            index = f"{self.elastic_index_basename}-{ts_now.strftime('%Y.%m.%d')}"

            if index not in self._es_index_cache:
                # check with elastic, could exist and not be in the cache
                # if the provider was restarted part way through a day.
                if not es.indices.exists(index=index):
                    self.log.info(f"Creating Elasticsearch index '{index}'...")
                    es.indices.create(index=index, body={
                        "mappings": {
                            "properties": es_type_mapping
                        }
                    })
                    self.log.info(f"Created Elasticsearch index '{index}'")
                self._es_index_cache.add(index)

            # Create list of docs to send using Elasticsearch's bulk API
            actions = [
                {
                    "_index": index,
                    "_source": {
                        **es_additions,
                        **message,
                        "event": {"ingested": ts_now},
                    }
                }
                for message in messages
            ]

            try:
                result = helpers.bulk(es, actions, request_timeout=10)
                if not result:
                    self.log.error(f"Empty Elasticsearch bulk result: {result}")
            except helpers.BulkIndexError as ex:
                self.log.exception("Failed Elasticsearch bulk ingest")
                self.log.error(f"** Errors **\n{pformat(ex.errors)}")
                raise RuntimeError("errors happened during elastic bulk ingest")
            except Exception:
                self.log.exception("Unknown error during Elasticsearch bulk ingest")

    def _serialize_value(self, tag: str, value: Any) -> str:
        """
        Convert a value to to a valid string for sending to subscribers (field devices).
        """
        if isinstance(value, bool):
            if value is True:
                return "true"
            else:
                return "false"

        if tag in self.gtnet_skt_tags and self.gtnet_skt_tags[tag] == "int":
            if int(value) >= 1:
                return "true"
            else:
                return "false"

        return str(value)

    def query(self) -> str:
        """
        Return all current tag names.

        Tag format for PMU data:
            {pmu-label}_{channel}.real
            {pmu-label}_{channel}.angle

        Tag examples for PMU data:
            BUS6_VA.real
            BUS6_VA.angle
        """
        self.log.debug("Processing query request")

        if not self.current_values:
            msg = "ERR=No data points have been read yet from the RTDS"
        else:
            msg = f"ACK={','.join(self.current_values.keys())}"

        self.log.log(  # Log at DEBUG level, unless there's an error
            logging.ERROR if "ERR" in msg else logging.DEBUG,
            f"Query response: {msg}"
        )

        return msg

    def read(self, tag: str) -> str:
        self.log.debug(f"Processing read request for tag '{tag}'")

        if not self.current_values:
            msg = "ERR=Data points have not been initialized yet from the RTDS"
        elif tag not in self.current_values:
            msg = "ERR=Tag not found in current values from RTDS"
        else:
            msg = f"ACK={self._serialize_value(tag, self.current_values[tag])}"

        # Log at DEBUG level, unless there's an error
        self.log.log(
            logging.ERROR if "ERR" in msg else logging.DEBUG,
            f"Read response for tag '{tag}': {msg}"
        )

        return msg

    def write(self, tags: dict) -> str:
        self.log.debug(f"Processing write request for tags: {tags}")

        if not tags:
            msg = "ERR=No tags provided for write to RTDS"
            self.log.error(msg)
            return msg

        # In OPC, there will be 3 tags for a point named "G1CB1":
        # - G1CB1_binary_output_closed
        # - G1CB1_binary_output_closed_opset
        # - G1CB1_binary_output_closed_optype
        #
        # To write to a point: set "G1CB1_binary_output_closed" to "1" in Quick Client in TOPServer in OPC
        # This will write "G1CB1.closed" to SCEPTRE.
        # To read status of a point from OPC, read "G1CB1_binary_output_closed_opset".
        # This is because DNP3 can't have values that are written and read, apparently.
        # So, G1CB1_binary_output_closed is "write", G1CB1_binary_output_closed_opset is "read".
        # The quality of "G1CB1_binary_output_closed" will show up as "Bad" in QuickClient, since it has no value.

        # Validate all incoming tag names match what's in config
        for tag, val in tags.items():
            if tag not in self.gtnet_skt_tags:
                msg = f"ERR=Invalid tag name '{tag}' for write to RTDS"
                self.log.error(f"{msg} (tags being written: {tags})")
                return msg

        # Update the state tracker with the values from the tags being written
        for tag, val in tags.items():
            # Validate types match what's in config, if they don't, then warn and typecast
            # NOTE: these will be strings if coming from pybennu-probe
            if not isinstance(val, str) and type(val).__name__ != self.gtnet_skt_tags[tag]:
                self.log.warning(
                    f"Data type of tag '{tag}' is '{type(val).__name__}', not "
                    f"'{self.gtnet_skt_tags[tag]}', forcing a typecast. If "
                    f"you're using 'pybennu-probe', this is normal."
                )

            if val is False or (isinstance(val, str) and val.lower() == "false"):
                val = "0"
            elif val is True or (isinstance(val, str) and val.lower() == "true"):
                val = "1"

            if self.gtnet_skt_tags[tag] == "int":
                typecasted_value = int(val)
            else:
                typecasted_value = float(val)

            # NOTE: don't need to sort incoming values, since they're updating the
            # dict (gtnet_skt_state), which is already in the proper order.
            self.log.info(f"Updating GTNET-SKT tag '{tag}' to {typecasted_value} (previous value: {self.gtnet_skt_state[tag]})")
            with self.__gtnet_lock:
                self.gtnet_skt_state[tag] = typecasted_value

        # If TCP, build and send, and retry connection if it fails
        # For UDP, the writer thread will handle sending the updated values
        if self.gtnet_skt_protocol == "tcp":
            payload = self._gtnet_build_payload()

            if self.debug:
                self.log.debug(f"Raw payload for {len(tags)} tags: {payload}")

            if not self.__gtnet_socket:
                self._gtnet_init_socket()

            sent = False
            while not sent:
                try:
                    self.__gtnet_socket.send(payload)
                    sent = True
                except Exception:
                    self.log.exception("GTNET-SKT send failed, resetting connection...")
                    self._gtnet_init_socket()

        # Update current values so GTNET-SKT points can be read from in addition to written
        # This is done AFTER sending value to ensure the right values are always available
        with self.__lock:
            self.current_values.update(self.gtnet_skt_state)

        msg = f"ACK=Wrote {len(tags)} tags to RTDS via GTNET-SKT"
        self.log.debug(msg)

        return msg

    def _gtnet_build_payload(self) -> bytes:
        """
        Generate the payload bytes to be sent across the socket.
        NOTE: see docstring at top of this file for details on GTNET-SKT payload structure.
        """
        # mutex to prevent threads from writing while we're reading
        with self.__gtnet_lock:
            values = list(self.gtnet_skt_state.values())

        return struct.pack(self.struct_format_string, *values)

    def _gtnet_continuous_write(self) -> None:
        """
        Continuously write values to RTDS via GTNET-SKT using UDP protocol.

        Rate is configured using 'gtnet-skt-udp-write-rate' in config.ini
        """
        sleep_for = 1.0 / float(self.gtnet_skt_udp_write_rate)
        target = (self.gtnet_skt_ip, self.gtnet_skt_port)

        self.log.info(f"Starting GTNET-SKT UDP writer thread [sleep_for={sleep_for}, target={target}]")
        self._gtnet_init_socket()

        while True:
            payload = self._gtnet_build_payload()
            self.__gtnet_socket.sendto(payload, target)
            sleep(sleep_for)

    def _gtnet_init_socket(self) -> None:
        """
        Initialize TCP or UDP socket, and if TCP connection fails, retry until it's successful.
        """
        self._gtnet_reset_socket()

        # TCP socket for synchronous connection.
        # Connection will fail if RSCAD case isn't running.
        if self.gtnet_skt_protocol == "tcp":
            target = (self.gtnet_skt_ip, self.gtnet_skt_port)
            connected = False
            while not connected:  # loop to handle connection failures
                try:
                    self.__gtnet_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    self.__gtnet_socket.connect(target)
                    connected = True
                except Exception:
                    self.log.error(f"Failed to connect to GTNET-SKT {target}, sleeping for {self.gtnet_skt_tcp_retry_delay} seconds before retrying")
                    if self.debug:  # only log traceback if debugging
                        self.log.exception(f"traceback for GTNET-SKT {target}")
                    self._gtnet_reset_socket()
                    sleep(self.gtnet_skt_tcp_retry_delay)

        # UDP socket for continually spamming data to the RTDS
        else:
            self.__gtnet_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def _gtnet_reset_socket(self) -> None:
        if self.__gtnet_socket:
            try:
                self.__gtnet_socket.close()
            except Exception:
                pass
            self.__gtnet_socket = None

    def periodic_publish(self) -> None:
        """
        Publish all tags periodically.

        Publish rate is configured by the 'publish-rate' configuration option.
        If publish rate is 0, then points will be published as fast as possible.

        Publisher message format:
            WRITE=<tag name>:<value>[,<tag name>:<value>,...]
            WRITE={tag name:value,tag name:value}
        """
        self.log.info(f"Beginning periodic publish (publish rate: {self.publish_rate})")
        while True:
            # mutex to prevent threads from writing while we're reading
            with self.__lock:
                tags = [
                    f"{tag}:{self._serialize_value(tag, value)}"
                    for tag, value in self.current_values.items()
                ]

            msg = "Write={" + ",".join(tags) + "}"
            self.publish(msg)

            # If publish_rate is not positive (0), don't sleep and go as fast as possible
            # Otherwise (if it's non-zero), log the points, and sleep like usual
            if self.publish_rate:
                # NOTE: to get the raw values, run 'pybennu-test-subscriber' on any RTU
                sleep(self.publish_rate)
