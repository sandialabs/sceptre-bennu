import atexit
import logging
import re
import time
from typing import Any, Dict, List, Optional, Literal

from pybennu.pypmu.synchrophasor.frame import CommonFrame, DataFrame, HeaderFrame
from pybennu.pypmu.synchrophasor.pdc import Pdc
from pybennu.utils import RotatingCSVWriter


class PMU:
    """
    Wrapper class for a Phasor Measurement Unit (PMU).

    Polls for data using C37.118 protocol, utilizing the pypmu library under-the-hood.

    Technically, this makes us a Phasor Data Concentrator (PDC), hence the
    usage of pypmu's "Pdc" class, and not the "Pmu" class.
    This does not act as a PMU, in other words we aggregate data *from* PMUs,
    we aren't *providing* data to a PDC.
    """

    def __init__(
        self,
        ip: str,
        port: int,
        pdc_id: int,
        name: str = "",
        label: str = "",
        protocol: Literal["tcp", "udp", "multicast"] = "tcp",
        bind_ip: str = "",
        retry_delay: float = 5.0,
    ) -> None:
        self.ip: str = ip
        self.port: int = port
        self.pdc_id: int = pdc_id
        self.name: str = name
        self.label: str = label
        self.protocol: Literal["tcp", "udp", "multicast"] = protocol
        self.bind_ip: str = bind_ip
        self.retry_delay: float = retry_delay

        if self.protocol not in ["tcp", "udp", "multicast"]:
            raise ValueError(f"PMU protocol must be 'tcp', 'udp' or 'multicast', got {self.protocol}")

        # Configure PDC instance (pypmu.synchrophasor.pdc.Pdc)
        self.pdc = Pdc(
            pdc_id=self.pdc_id,
            pmu_ip=self.ip,
            pmu_port=self.port,
            method=self.protocol,
            udp_bind_ip=self.bind_ip,
        )

        self.pdc_header: Optional[HeaderFrame] = None
        self.pdc_config: Optional[CommonFrame] = None
        self.channel_names: List[str] = []
        self.csv_writer: Optional[RotatingCSVWriter] = None
        self.station_name: str = ""
        self.started: bool = False  # if start command has been issued

        # Configure logging
        self.log: logging.Logger = logging.getLogger(f"PMU [{str(self)}]")
        self.log.setLevel(logging.DEBUG)
        self.pdc.logger = logging.getLogger(f"Pdc [{str(self)}]")
        # NOTE: pypmu logs a LOT of stuff at DEBUG, leave at INFO
        # unless you're doing deep debugging of the pypmu code.
        self.pdc.logger.setLevel(logging.INFO)

        # cleanup on exit
        atexit.register(self.quit)

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

    @property
    def connected(self) -> bool:
        """
        If connected to PMU (TCP) or socket bound (UDP).
        """
        return self.pdc.pmu_socket is not None

    def run(self) -> None:
        """
        Connect to PMU (TCP) or bind local socket (UDP).
        """
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
        """
        Send start frame to begin streaming data.
        WARNING: This must only be called AFTER run() is called!
        """
        if self.started:
            self.log.warning("start() called after PMU already started!")
        self.pdc.start()
        self.started = True

    def quit(self) -> None:
        """
        Close and cleanup connection with PMU.
        If there's no connection or connection failed, this does nothing.
        Therefore, it's safe to run at any point.
        """
        # If start command has been issued, then send a stop command
        if self.started and self.connected:
            try:
                self.pdc.stop()
            except Exception:
                pass  # don't care about exceptions here really...maybe, TBD

        self.started = False
        self.pdc.quit()

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

    def rebuild_tcp_connection(self) -> None:
        """
        Rebuild TCP connection to PMU if connection fails.

        WARNING: this will loop forever and block until the
        TCP connection is re-established.

        For example, if RTDS simulation is stopped, the PMUs no longer exist.
        When the simulation restarts, this provider should automatically reconnect
        to the PMUs and start getting data again.
        """
        self.quit()

        while True:
            try:
                # Create TCP connection and get header(s)
                self.run()

                # Send start frame to kick off PMU data
                # NOTE (cegoes, 6/1/2023): this fixed a long-standing bug where
                # rebuilding connections to PMUs, where connections would be
                # rebuilt but data wouldn't be updating. This resulted in the
                # provider having to be manually restarted whenever the
                # RSCAD case was restarted.
                self.start()
                break
            except Exception as ex:
                self.log.error(
                    f"Failed to rebuild TCP connection due to error '{ex}', sleeping for {self.retry_delay} seconds before attempting again..."
                )
                self.quit()
                time.sleep(self.retry_delay)

        self.log.debug("(re)initialized TCP connection")
