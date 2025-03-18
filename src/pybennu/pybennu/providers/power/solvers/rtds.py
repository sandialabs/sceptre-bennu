"""
SCEPTRE Provider for the Real-Time Dynamic Simulator (RTDS).

## Overview

Data is read via C37.118 protocol from Phasor Measurement Unit (PMU) interface on the RTDS GTNET card.

Data is written via GTNET-SKT protocol to the RTDS GTNET card.
See docstring in pybennu/gtnet_skt.py for details on the GTNET-SKT protocol.


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

See pybennu/elastic.py for details on the index mapping.


## Notes

NOTE: in the bennu VM, rtds.py is located in dist-packages.
Just in case you need to modify it on the fly ;)
/usr/lib/python3/dist-packages/pybennu/providers/power/solvers/rtds.py

If observing C37.118 traffic in Wireshark, configure manual decode for
each PMU port and set the protocol to "SYNCHROPHASOR" (synphasor).
Wireshark -> "Analyze..." -> "Decode As..."
"""

import json
import logging
import sys
from datetime import datetime, timezone
from logging.handlers import QueueHandler, QueueListener
from pathlib import Path
from time import sleep
from typing import Any, Dict, List, Optional
from threading import Thread, Lock
from queue import Queue

from pybennu.distributed.provider import Provider
from pybennu.elastic import ElasticPusher
from pybennu.settings import PybennuSettings
from pybennu.pmu_wrapper import PMU
from pybennu.gtnet_skt import GtnetSkt
from pybennu.utils import RotatingCSVWriter, utc_now, utc_now_formatted

# TODO: push state of GTNET-SKT values to Elasticsearch
# TODO: log most messages to a file with Rotating handler to avoid filling up log (since bennu isn't very smart and won't rotate the file)
# TODO: move stuff out of module docstring into a README file


class RTDS(Provider):
    """
    SCEPTRE Provider for the Real-Time Dynamic Simulator (RTDS).
    """

    def __init__(
        self,
        server_endpoint,
        publish_endpoint,
        settings: PybennuSettings,
    ) -> None:
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.conf: PybennuSettings = settings

        if "default" in self.conf.data_dir:
            self.conf.data_dir = "/root/rtds_data/"
        self.data_dir = Path(self.conf.data_dir).resolve()
        self.data_dir.mkdir(exist_ok=True, parents=True)

        # Current simulation values, keyed by tag name string
        #
        # Lock prevents values being read while values are being updated.
        # While Python is thread-safe, it's possible for a periodic publish to read
        # a set of values that are from a previous poll and others from a ongoing poll,
        # basically a split-brain/partial-read. Using a lock prevents that from happening.
        self.__current_values_lock: Lock = Lock()
        self.current_values: Dict[str, Any] = {}

        # Hack
        # TODO: rotate out values to keep this from eating up memory if running for a long time
        self.__time_map_lock: Lock = Lock()
        self.time_map: Dict[int, datetime] = {}

        # asynchronous logging. Log messages are placed into a queue,
        # which is serviced by a thread that runs each handler with
        # messages from the queue. This isn't needed as much for stdout,
        # but if we enable logging to file or elastic then it will be.
        self.log_queue: Queue = Queue()
        queue_handler = QueueHandler(self.log_queue)

        formatter = logging.Formatter(
            fmt="%(asctime)s.%(msecs)03d [%(levelname)s] (%(name)s) %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S"
        )
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(formatter)
        console_handler.setLevel(logging.DEBUG if self.conf.debug else logging.INFO)
        queue_listener = QueueListener(self.log_queue, console_handler, respect_handler_level=True)
        queue_listener.start()

        # Configure logging to stdout (includes debug messages from pypmu)
        logging.basicConfig(
            level=logging.DEBUG if self.conf.debug else logging.INFO,
            handlers=[queue_handler],
            # format="%(asctime)s.%(msecs)03d [%(levelname)s] (%(name)s) %(message)s",
            # datefmt="%Y-%m-%d %H:%M:%S",
            # stream=sys.stdout
        )

        self.log: logging.Logger = logging.getLogger(self.__class__.__name__)
        self.log.setLevel(logging.DEBUG if self.conf.debug else logging.INFO)

        # Elasticsearch setup
        self.es: Optional[ElasticPusher] = None
        if self.conf.elastic.enabled:
            self.es = ElasticPusher(conf=self.conf)
        else:
            self.log.warning("NOTE: Elasticsearch output is DISABLED ('elastic.enabled' is false or unset)")

        # PMU setup
        if self.conf.pmu.enabled:
            self.log.info("Configuring PMU streaming")

            self.pmus: List[PMU] = []

            for p in self.conf.pmu.pmus:
                pmu = PMU(
                    ip=str(p.ip),
                    port=p.port,
                    pdc_id=p.pdc_id,
                    name=p.name,
                    label=p.label,
                    protocol=p.protocol,
                    bind_ip=str(self.conf.pmu.bind_ip) if p.protocol != "tcp" else "",
                    retry_delay=self.conf.pmu.retry_delay,
                )
                self.pmus.append(pmu)

            self.__pmu_polling_thread: Thread = Thread(target=self._start_poll_pmus, daemon=True)
        else:
            self.log.warning("NOTE: PMU is DISABLED ('pmu.enabled' is false or unset)")

        self.gtnet: Optional[GtnetSkt] = None
        if self.conf.gtnet_skt.enabled:
            self.gtnet = GtnetSkt(conf=self.conf)
            self.current_values.update(self.gtnet.state)

        self.frame_queue: Queue = Queue()
        self.__data_thread: Thread = Thread(target=self._data_writer, daemon=True)

        self.log.info("RTDS initialization is finished")

    def run(self) -> None:
        """Start threads and the provider."""
        self.__data_thread.start()

        if self.conf.pmu.enabled:
            self.__pmu_polling_thread.start()

        if self.gtnet:
            self.gtnet.start()

        if self.es:
            self.es.start()  # start threads

        super().run()  # start publisher and main message loop

    def _serialize_value(self, tag: str, value: Any) -> str:
        """
        Convert a value to to a valid string for sending to subscribers (field devices).
        """
        if isinstance(value, bool):
            if value is True:
                return "true"
            else:
                return "false"

        if self.gtnet:
            if tag in self.gtnet.tags and self.gtnet.tags[tag].type == "int":
                if int(value) >= 1:
                    return "true"
                else:
                    return "false"

        return str(value)

    def _start_poll_pmus(self) -> None:
        """
        Query for data from the PMUs via C37.118 as fast as possible.
        """
        self.log.info(f"Starting polling threads for {len(self.pmus)} PMUs...")
        threads = []
        for pmu in self.pmus:
            pmu_thread = Thread(target=self._poll_pmu, args=(pmu,))
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
        # TODO: why is this relying on csv-path variable?!?!?
        meta_path = Path(self.data_dir, f"pmu_metadata_{timestamp}.json")
        if not meta_path.parent.exists():
            meta_path.parent.mkdir(exist_ok=True, parents=True)
        self.log.info(f"Writing metadata from {len(metadata)} PMUs to {meta_path}")
        meta_path.write_text(json.dumps(metadata, indent=4), encoding="utf-8")

        # Wait for values to be read from PMUs
        while not self.current_values or self.current_values == self.gtnet.state:
            self.log.info("Waiting for current values to be updated from PMUs...")
            sleep(1)

        # Save the tag names to a file after there are values from the PMUs
        tags_path = Path(self.data_dir, f"tags_{timestamp}.txt")
        tags = list(self.current_values.keys())
        self.log.info(f"Writing {len(tags)} tag names to {tags_path}")
        tags_path.write_text("\n".join(tags), encoding="utf-8")

        # Block on PMU threads
        for thread in threads:
            thread.join()

    def _poll_pmu(self, pmu: PMU) -> None:
        """
        Continually polls for data from a PMU and updates self.current_values.

        NOTE: This method is intended to be run in a thread,
        since it's loop that runs forever until killed.
        """
        self.log.info(f"Started polling thread for {str(pmu)}")
        retry_count = 0

        # Setup the PMU connection (UDP or TCP)
        pmu.run()

        # Send start frame to kick off PMU data
        pmu.start()

        while True:
            try:
                data_frame = pmu.get_data_frame()
            except Exception as ex:
                self.log.error(f"Failed to get data frame from {str(pmu)} due to an exception '{ex}', attempting to rebuild connection...")
                if self.conf.debug:  # only log traceback if debugging
                    self.log.exception(f"traceback for {str(pmu)}")
                pmu.rebuild_tcp_connection()
                continue

            if not data_frame:
                if retry_count >= 3:
                    self.log.error(f"Failed to request data {retry_count} times from {str(pmu)}, attempting to rebuild connection...")
                    pmu.rebuild_tcp_connection()
                    retry_count = 0
                else:
                    retry_count += 1
                    self.log.error(f"No data in frame from {str(pmu)}, sleeping for {self.conf.pmu.retry_delay} seconds before retrying (retry count: {retry_count})")
                    sleep(self.conf.pmu.retry_delay)
                continue

            self.frame_queue.put((pmu, data_frame))

    def _data_writer(self) -> None:
        """
        Handles processing of PMU data to avoid blocking the PMU polling thread.
        """
        self.log.info("Starting data writer thread")

        while True:
            # This will block until there is an item to process
            pmu, frame = self.frame_queue.get()

            # This is an epic hack to workaround a nasty time syncronization issue.
            # Extensive documentation about this is on the wiki.
            # tl;dr: due the 8 PMU connections being handled in independant threads,
            # the sceptre_time will differ between the 8 PMUs for the same rtds_time.
            with self.__time_map_lock:
                if frame["time"] not in self.time_map:
                    self.time_map[frame["time"]] = utc_now()
                sceptre_ts = self.time_map[frame["time"]]

            rtds_datetime = datetime.fromtimestamp(frame["time"], timezone.utc)
            drift = abs((sceptre_ts - rtds_datetime).total_seconds()) * 1000.0  # float

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
                if self.es:
                    es_bodies = []

                    for ph_id, phasor in line["phasors"].items():
                        es_body = {
                            "@timestamp": sceptre_ts,
                            "rtds_time": rtds_datetime,
                            "sceptre_time": sceptre_ts,
                            "time_drift": drift,
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

                    self.es.enqueue(es_bodies)

                # Update global data structure with measurements.
                # These are the values that get published to SCEPTRE.
                #
                # NOTE: since there are usually multiple threads querying from multiple PMUs
                # simultaneously, a lock mutex is used to ensure self.current_values doesn't
                # result in a race condition or corrupted data.
                with self.__current_values_lock:
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
                if self.conf.csv.enabled and pmu.csv_writer is None:
                    # Build the CSV header
                    header = ["rtds_time", "sceptre_time", "freq", "dfreq"]
                    for i, ph in line["phasors"].items():
                        for k in ph.keys():
                            # Example: PHASOR CH 1:VA_real
                            header.append(f"{pmu.channel_names[i]}_{k}")

                    pmu.csv_writer = RotatingCSVWriter(
                        name=str(pmu),
                        csv_dir=Path(self.conf.csv.file_path) / str(pmu),
                        header=header,
                        filename_base=f"{str(pmu)}",
                        rows_per_file=self.conf.csv.rows_per_file,
                        max_files=self.conf.csv.max_files,
                    )

                # Write data to CSV file
                if self.conf.csv.enabled:
                    csv_row = [line["time"], sceptre_ts.timestamp(), line["freq"], line["dfreq"]]
                    for ph in line["phasors"].values():
                        for v in ph.values():
                            csv_row.append(v)
                    pmu.csv_writer.write(csv_row)


    # ===== Provider API =====

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
        if self.conf.debug:
            self.log.debug("Processing query request")

        if not self.current_values:
            msg = "ERR=No data points have been read yet from the RTDS"
            self.log.error(msg)
            return msg

        # NOTE: no need to lock here because these are just the tag names,
        # which shouldn't change at runtime.
        msg = f"ACK={','.join(self.current_values.keys())}"

        if self.conf.debug:
            self.log.debug(f"Query response: {msg}")

        return msg

    def read(self, tag: str) -> str:
        if self.conf.debug:
            self.log.debug(f"Processing read request for tag '{tag}'")

        if not self.current_values:
            msg = "ERR=Data points have not been initialized yet from the RTDS"
            self.log.error(msg)
            return msg

        if tag not in self.current_values:
            msg = "ERR=Tag not found in current values from RTDS"
            self.log.critical(f"{msg} (tag: {tag})")
            return msg

        # This is reading a single value, so no need to lock (Python is thread-safe)
        msg = f"ACK={self._serialize_value(tag, self.current_values[tag])}"

        if self.conf.debug:
            self.log.debug(f"Read response for tag '{tag}': {msg}")

        return msg

    def write(self, tags: dict) -> str:
        if self.conf.debug:
            self.log.debug(f"Processing write request for tags: {tags}")

        if not tags:
            msg = "ERR=No tags provided for write to RTDS"
            self.log.error(msg)
            return msg

        if not self.gtnet:
            msg = "ERR=GTNETSKT is not enabled or configured which is required for write to RTDS"
            self.log.error(f"{msg} (tags: {tags})")
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
        for tag in tags.keys():
            if tag not in self.gtnet.tags:
                msg = f"ERR=Invalid tag name '{tag}' for write to RTDS"
                self.log.error(f"{msg} (tags being written: {tags})")
                return msg

        # Typecast values (e.g. bool to 0/1)
        typecasted = self.gtnet.typecast_values(tags)

        if self.conf.debug:
            self.log.debug(f"Updating GTNET-SKT tags to: {typecasted}")

        # Update the state tracker with the values from the tags being written.
        # NOTE: don't need to sort incoming values, since they're updating the
        # GtnetSkt.state dict, which is already in the proper order.
        with self.gtnet.lock:
            self.gtnet.state.update(typecasted)

        # If TCP, build and send the updated state, and retry connection if it fails
        # For UDP, the writer thread will handle sending the updated values
        if self.conf.gtnet_skt.protocol == "tcp":
            self.gtnet.tcp_write_state()

        # Update current values so GTNET-SKT points can be read from in addition to written
        # This is done AFTER sending value to ensure the right values are always available
        with self.__current_values_lock:
            self.current_values.update(self.gtnet.state)

        msg = f"ACK=Wrote {len(tags)} tags to RTDS via GTNET-SKT"

        if self.conf.debug:
            self.log.debug(msg)

        return msg

    def periodic_publish(self) -> None:
        """
        Publish all tags periodically.

        Publish rate is configured by the 'publish-rate' configuration option.
        If publish rate is 0, then points will be published as fast as possible.

        Publisher message format:
            WRITE=<tag name>:<value>[,<tag name>:<value>,...]
            WRITE={tag name:value,tag name:value}
        """
        self.log.info(f"Beginning periodic publish (publish rate: {self.conf.publish_rate})")
        while True:
            # mutex to prevent threads from writing while we're reading
            with self.__current_values_lock:
                tags = [
                    f"{tag}:{self._serialize_value(tag, value)}"
                    for tag, value in self.current_values.items()
                ]

            msg = "Write={" + ",".join(tags) + "}"
            self.publish(msg)

            # If publish_rate is not positive (0), don't sleep and go as fast as possible
            # Otherwise (if it's non-zero), log the points, and sleep like usual
            if self.conf.publish_rate:
                # NOTE: to get the raw values, run 'pybennu-test-subscriber' on any RTU
                sleep(self.conf.publish_rate)
