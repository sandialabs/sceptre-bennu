"""
SCEPTRE Provider for OPAL-RT.

OPAL-RT publishes PMU data to SCEPTRE endpoint using C37.118 protocol.

Code for this provider was based on work done on RTDS provider.


## Note on boolean tags in OPC

In OPC, there will be 3 tags for a boolean point named "G1CB1":
- G1CB1_binary_output_closed
- G1CB1_binary_output_closed_opset
- G1CB1_binary_output_closed_optype

To write to a point: set "G1CB1_binary_output_closed" to "1" in Quick Client in TOPServer in OPC

This will write "G1CB1.closed" to SCEPTRE.

To read status of a point from OPC, read "G1CB1_binary_output_closed_opset".
This is because DNP3 can't have values that are written and read, apparently.
So, G1CB1_binary_output_closed is "write", G1CB1_binary_output_closed_opset is "read".
The quality of "G1CB1_binary_output_closed" will show up as "Bad" in QuickClient, since it has no value.
"""

from __future__ import annotations

import json
import logging
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from threading import Lock, Thread
from queue import Queue

from pymodbus.exceptions import ModbusException

from pybennu.settings import PybennuSettings, ModbusRegister
from pybennu.elastic import ElasticPusher
from pybennu.pmu_wrapper import PMU
from pybennu.modbus import ModbusWrapper
from pybennu.utils import RotatingCSVWriter, utc_now, utc_now_formatted
from pybennu.distributed.provider import Provider


# TODOs
# - [ ] Add the SCEPTRE tag and other metadata to Elastic docs
# - [ ] Log most messages to a file with Rotating handler to avoid filling up log (since bennu isn't very smart and won't rotate the file). Separate log files for things like CSV handler?


MBResultType = Tuple[str, Any, ModbusRegister, datetime]


class OPALRT(Provider):
    """
    SCEPTRE Provider for the OPAL-RT simulator.
    """

    def __init__(self, server_endpoint, publish_endpoint, settings: PybennuSettings) -> None:
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.conf: PybennuSettings = settings

        if "default" in self.conf.data_dir:
            self.conf.data_dir = "/root/opalrt_data/"
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

        # Configure logging to stdout (includes debug messages from pypmu)
        logging.basicConfig(
            level=logging.DEBUG if self.conf.debug else logging.INFO,
            format="%(asctime)s.%(msecs)03d [%(levelname)s] (%(name)s) %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
            stream=sys.stdout,
        )

        self.log: logging.Logger = logging.getLogger(self.__class__.__name__)
        self.log.setLevel(logging.DEBUG if self.conf.debug else logging.INFO)

        # Elasticsearch setup
        self.es: Optional[ElasticPusher] = None
        if self.conf.elastic.enabled:
            self.es = ElasticPusher(conf=self.conf)
        else:
            self.log.warning("NOTE: Elasticsearch output is DISABLED ('elastic.enabled' is false or unset)")

        # Modbus setup
        if self.conf.modbus.enabled:
            if not self.conf.modbus.registers:
                raise ValueError("No Modbus registers defined and modbus enabled!")

            self.mb_registers = {
                reg.sceptre_tag: reg
                for reg in self.conf.modbus.registers
            }
            self.log.info(f"{len(self.mb_registers)} TOTAL Modbus registers configured")

            # Determine which registers are readable (have "r" or "rw" access)
            self.mb_readable_regs = {}
            for tag, reg in self.mb_registers.items():
                if "r" in reg.access:
                    self.mb_readable_regs[tag] = reg
            self.log.info(f"{len(self.mb_registers)} READABLE Modbus registers configured")

            self.mb_data_queue: Queue[Tuple[Dict[str, Any], List[MBResultType]]] = Queue()

            self.mb_client = ModbusWrapper(
                ip=str(self.conf.modbus.ip),
                port=self.conf.modbus.port,
                timeout=self.conf.modbus.timeout,
                debug=self.conf.debug,
            )

            self.__modbus_data_thread = Thread(target=self._modbus_data_writer, daemon=True)
            self.__modbus_polling_thread = Thread(target=self._modbus_poll, daemon=True)
        else:
            self.log.warning("NOTE: Modbus is DISABLED ('modbus.enabled' is false or unset)")

        # PMU setup
        if self.conf.pmu.enabled:
            self.log.info(f"Configuring PMU streaming for {len(self.conf.pmu.pmus)} PMUs")

            if not self.conf.pmu.pmus:
                raise ValueError("No PMUs defined in configuration (pmu.pmus or pmu.tags)!")

            self.pmus: List[PMU] = []
            self.pmu_frame_queue: Queue[Tuple[PMU, dict]] = Queue()

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

            self.__pmu_data_thread = Thread(target=self._pmu_data_writer, daemon=True)
            self.__pmu_polling_thread = Thread(target=self._start_poll_pmus, daemon=True)
        else:
            self.log.warning("NOTE: PMU is DISABLED ('pmu.enabled' is false or unset)")

        self.log.info(f"Initialized {repr(self)}")

    def run(self) -> None:
        """Start threads and the provider."""
        if self.conf.modbus.enabled:
            if not self.mb_client.connect():
                raise ConnectionError(f"Failed to connect to Modbus server {self.mb_client!s} (timeout: {self.conf.modbus.timeout})")

            self.__modbus_data_thread.start()
            self.__modbus_polling_thread.start()

        if self.conf.pmu.enabled:
            self.__pmu_data_thread.start()
            self.__pmu_polling_thread.start()

        if self.es:
            self.es.start()  # start threads

        super().run()  # start publisher and main message loop

    @staticmethod
    def _serialize_value(value: Any) -> str:
        """
        Convert a value to to a valid string for sending to subscribers (field devices).
        """
        if isinstance(value, bool):
            if value is True:
                return "true"
            else:
                return "false"

        return str(value)

    def _modbus_poll(self) -> None:
        self.log.info(
            f"Starting Modbus polling for {len(self.mb_readable_regs)} readable registers (polling rate: {self.conf.modbus.polling_rate}, total registers: {len(self.mb_registers)})"
        )
        retry_count = 0

        # Loop forever and poll modbus registers
        while True:
            mb_vals: Dict[str, Any] = {}
            mb_results: List[MBResultType] = []
            timestamp = utc_now()  # use same timestamp for all reads

            # Instead of pushing one register at a time, we read
            # all of the registers, then put the batch read into
            # the queue. This enables current_values to be updated
            # in a single call, instead of piecemeal.
            try:
                # TODO: we can't just do a single call to read_holding_registers, because
                # there may be write-only registers between readable registers. Modbus
                # is simple, and for a read multiple, it's a start address and number of
                # registers/coils to query, that's it. Also, the register types will vary.
                # Therefore, for now we just read all of the values in a loop.
                for tag, reg in self.mb_readable_regs.items():
                    val = self.mb_client.read_register(reg)

                    # tuple of tag name, value, Register object, and timestamp
                    mb_results.append((tag, val, reg, timestamp))
                    mb_vals[tag] = val
            except (ValueError, TypeError, NotImplementedError) as ex:
                self.log.exception(f"Critical error reading Modbus register '{reg.name}' for tag '{tag}'")
                raise ex
            except ModbusException as ex:
                self.log.error(f"Failed to read Modbus register for tag '{tag}' due to Modbus error '{ex!s}'")

                if self.conf.modbus.retry_attempts and retry_count < self.conf.modbus.retry_attempts:
                    self.log.error(f"Sleeping for {self.conf.modbus.retry_delay} seconds before retrying read...")
                    time.sleep(self.conf.modbus.retry_delay)
                    continue
                elif self.conf.modbus.retry_attempts:
                    # Wait to rebuild Modbus connection in a loop
                    self.mb_client.retry_until_connected(self.conf.modbus.retry_delay)
                    continue
                else:
                    # Retries are disabled, error out
                    raise ex
            except Exception as ex:
                # Anything not handled above is unexpected and should be handled as a bug
                self.log.exception(f"Unknown error attempting to read Modbus register for tag '{tag}'")
                raise ex

            # Push the results in the data processor's queue
            self.mb_data_queue.put((mb_vals, mb_results))

            # Only log if polling rate is a reasonable value
            if self.conf.debug and self.conf.modbus.polling_rate >= 0.5:
                self.log.debug(
                    f"Polled {len(self.mb_readable_regs)} registers from {self.mb_client.ip}, sleeping for {self.conf.modbus.polling_rate} seconds..."
                )

            # Wait before re-polling
            time.sleep(self.conf.modbus.polling_rate)

    def _modbus_data_writer(self) -> None:
        """
        Handles processing of Modbus data to avoid blocking the Modbus polling thread.
        """
        self.log.info("Starting Modbus data writer thread")

        while True:
            mb_vals, mb_results = self.mb_data_queue.get()

            # TODO: this lock will also affect PMU streaming and polling...may want to re-evaluate
            #
            # Lock prevents values being read while we write.
            # While Python is thread-safe, it's possible for a periodic publish to read
            # a set of values that are from a previous poll and others from a ongoing poll,
            # basically a split-brain/partial-read. Using a lock prevents that from happening.
            with self.__current_values_lock:
                self.current_values.update(mb_vals)

            # Save data to Elasticsearch
            if self.es:
                es_bodies = []

                for tag, value, reg, ts in mb_results:
                    es_body = {
                        "@timestamp": ts,
                        "network": {
                            "protocol": "modbus",
                            "transport": "tcp",
                        },
                        "modbus": {
                            "register": reg.num,
                            "name": reg.name,
                            "register_type": reg.reg_type,
                            "data_type": reg.data_type,
                            "unit_type": reg.unit_type,
                            "description": reg.description,
                            "sceptre_tag": reg.sceptre_tag,
                            "raw_value": str(value),
                            "human_value": reg.humanize(value),
                            "ip": str(self.conf.modbus.ip),
                            "port": self.conf.modbus.port,
                        },
                        "groundtruth": {
                            "description": reg.description,
                            "tag": tag,
                            "type": reg.data_type,
                            "value": value,
                        },
                    }

                    # Kibana doesn't allow dynamic typing without using runtime fields
                    # This is a minor hack to store the value as both keyword type in .value,
                    # and as it's actual type in a separate field, e.g. "float" field will have
                    # the value for floating point values.
                    if isinstance(value, float):
                        es_body["groundtruth"]["float"] = value
                    elif isinstance(value, bool):
                        es_body["groundtruth"]["bool"] = value
                    elif isinstance(value, int):
                        es_body["groundtruth"]["int"] = value
                    else:
                        self.log.warning(f"Unknown data type '{type(value)}' for tag '{tag}' (description: {reg.description})")

                    es_bodies.append(es_body)

                self.es.enqueue(es_bodies)

    def _start_poll_pmus(self) -> None:
        """
        Query for data from the PMUs via C37.118 as fast as possible.
        """
        self.log.info(f"Starting polling threads for {len(self.pmus)} PMUs...")
        threads = []
        for pmu in self.pmus:
            pmu_thread = Thread(target=self._poll_pmu, args=(pmu,), daemon=True)
            pmu_thread.start()
            threads.append(pmu_thread)

        # Save PMU metadata (configs and/or headers) to file
        metadata = {}
        for pmu in self.pmus:
            # Wait until configs have been pulled to save them
            while not self.pmus[0].pdc_config:
                time.sleep(1)

            metadata[str(pmu)] = {"config": pmu.pdc_config.__dict__}
            if pmu.pdc_header:
                metadata[str(pmu)]["header"] = pmu.pdc_header.__dict__

        timestamp = utc_now_formatted()

        # Save PMU metadata to JSON file
        meta_path = Path(self.data_dir, f"pmu_metadata_{timestamp}.json")
        if not meta_path.parent.exists():
            meta_path.parent.mkdir(exist_ok=True, parents=True)
        self.log.info(f"Writing metadata from {len(metadata)} PMUs to {meta_path}")
        meta_path.write_text(json.dumps(metadata, indent=4), encoding="utf-8")

        # Wait for values to be read from PMUs
        while not self.current_values:
            self.log.info("Waiting for current values to be updated from PMUs...")
            time.sleep(1)

        # Save the tag names to a file after there are values from the PMUs
        tags_path = Path(self.data_dir, f"tags_{timestamp}.txt")
        tags = list(self.current_values.keys())
        self.log.info(f"Writing {len(tags)} PMU tag names to {tags_path}")
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

        pmu.quit()  # Make sure there is not already a running PMU data stream (such as when restarting the provider but not the OPALRT sim)

        # Setup the PMU connection (UDP or TCP)
        pmu.run()

        # Send start frame to kick off PMU data
        pmu.start()

        while True:
            try:
                data_frame = pmu.get_data_frame()
            except Exception as ex:
                self.log.error(f"Failed to get data frame from {str(pmu)} due to an exception: {ex}")
                if self.conf.debug:  # only log traceback if debugging
                    self.log.exception(f"traceback for {str(pmu)}")

                if pmu.protocol == "tcp" and retry_count < self.conf.pmu.retry_attempts:
                    self.log.error(f"attempting to rebuild TCP connection... (retry count: {retry_count}, retry max: {self.conf.pmu.retry_attempts})")
                    pmu.rebuild_tcp_connection()
                    continue
                else:
                    raise ex from None

            if not data_frame:
                if retry_count < self.conf.pmu.retry_attempts:
                    self.log.error(f"No data in frame from {str(pmu)}, sleeping for {self.conf.pmu.retry_delay} seconds before retrying (retry count: {retry_count}, retry max: {self.conf.pmu.retry_attempts})")
                    retry_count += 1
                    time.sleep(self.conf.pmu.retry_delay)
                elif pmu.protocol == "tcp":
                    self.log.error(f"Failed to request data {retry_count} times from {str(pmu)}, attempting to rebuild connection...")
                    pmu.rebuild_tcp_connection()
                    retry_count = 0
                else:
                    raise ConnectionError(f"Failed to request data {retry_count} times from {str(pmu)}, terminating")

                continue

            self.pmu_frame_queue.put((pmu, data_frame))

    def _pmu_data_writer(self) -> None:
        """
        Handles processing of PMU data to avoid blocking the PMU polling thread.
        """
        self.log.info("Starting PMU data writer thread")

        while True:
            # This will block until there is an item to process
            pmu, frame = self.pmu_frame_queue.get()

            # This is an epic hack to workaround a nasty time synchronization issue.
            # Extensive documentation about this is on the wiki.
            # tl;dr: due the 8 PMU connections being handled in independent threads,
            # the sceptre_time will differ between the 8 PMUs for the same rtds_time.
            with self.__time_map_lock:
                if frame["time"] not in self.time_map:
                    self.time_map[frame["time"]] = utc_now()
                sceptre_ts = self.time_map[frame["time"]]

            pmu_datetime = datetime.fromtimestamp(frame["time"], timezone.utc)
            drift = abs((sceptre_ts - pmu_datetime).total_seconds()) * 1000.0  # float

            for mm in frame["measurements"]:
                if mm["stat"] != "ok":
                    pmu.log.error(f"Bad/unknown PMU status: {mm['stat']}")
                if mm["stream_id"] != pmu.pdc_id:
                    pmu.log.warning(f"Unknown PMU stream ID: {mm['stream_id']} (expected {pmu.pdc_id})")

                # TODO: support PMU "digital" fields (mm["digital"])
                #   digital is needed for OpalRT (done, but need to test with live experiment)
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
                    "digital": {  # digital is a list of breaker values (0,1) representing closed/open
                        i: {"Breaker Status": cb}
                        for i, cb in enumerate(mm["digital"])
                    },
                }
                pmu.log.info(f"Measurements from frame: {line}")

                # Save data to Elasticsearch
                if self.es:
                    es_bodies = []

                    # TODO: "digital" fields
                    # TODO: "analog" fields (not being used for griDNA currently)
                    # TODO: add field for breaker value

                    for ph_id, phasor in line["phasors"].items():
                        es_body = {
                            "@timestamp": sceptre_ts,
                            "simulator_time": pmu_datetime,
                            "sceptre_time": sceptre_ts,
                            "time_drift": drift,
                            "network": {
                                "protocol": "c37.118",
                                "transport": "udp" if pmu.protocol != "tcp" else "tcp",
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
                                # TODO: test
                                # "breaker_status": line["digital"][0]["Breaker Status"],
                            },
                            # # TODO: groundtruth
                            # # Need to write two separate docs, with "real" and "angle"
                            # "groundtruth": {
                            #     "tag": f"{pmu.label}_{pmu.channel_names[ph_id]}.{ph_type}",
                            #     "type": "float",
                            #     "value": phasor["real"],
                            #     "description": "",
                            # },
                        }
                        pmu.log.info(f"Generated new ES document {es_body}")
                        es_bodies.append(es_body)

                    self.es.enqueue(es_bodies)

                # Update global data structure with measurements.
                # These are the values that get published to SCEPTRE.
                #
                # NOTE: since there are usually multiple threads querying from multiple PMUs
                # simultaneously, a lock mutex is used to ensure self.current_values doesn't
                # result in a race condition or corrupted data.
                # TODO: this is where SCEPTRE tag is created dynamically...put this in elastic
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

                    # TODO: "digital" fields

                # Create CSV writer if it doesn't exist
                if self.conf.csv.enabled and pmu.csv_writer is None:
                    # Build the CSV header
                    header = ["simulator_time", "sceptre_time", "freq", "dfreq"]
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
        if self.conf.debug:
            self.log.debug("Processing query request")

        if not self.current_values:
            msg = "ERR=No data points have been read yet from the OPALRT"
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
            msg = "ERR=Data points have not been initialized yet from the OPALRT"
            self.log.error(msg)
            return msg

        if tag not in self.current_values:
            msg = "ERR=Tag not found in current values from OPALRT"
            self.log.critical(f"{msg} (tag: {tag})")
            return msg

        if tag in self.mb_registers:
            if not self.conf.modbus.enabled:
                msg = "ERR=Modbus not enabled but read from a configured modbus register from OPALRT"
                self.log.error(f"{msg} (tag: {tag})")
                return msg

            # For Modbus registers, do a read, instead of just grabbing current state
            val = self.mb_client.read_register(self.mb_registers[tag])
            msg = f"ACK={self._serialize_value(val)}"
        else:
            # This is reading a single value, so no need to lock (Python is thread-safe)
            msg = f"ACK={self._serialize_value(self.current_values[tag])}"

        if self.conf.debug:
            self.log.debug(f"Read response for tag '{tag}': {msg}")

        return msg

    def write(self, tags: dict) -> str:
        if self.conf.debug:
            self.log.debug(f"Processing write request for tags: {tags}")

        if not tags:
            msg = "ERR=No tags provided for write to OPALRT"
            self.log.error(msg)
            return msg

        if not self.conf.modbus.enabled:
            msg = "ERR=Modbus is not enabled which is required for write to OPALRT"
            self.log.error(f"{msg} (tags: {tags})")
            return msg

        for tag, val in tags.items():
            # get register matching the tag
            reg = self.mb_registers.get(tag)

            # Writes are only supported for Modbus registers, and only
            # for modbus registers that have access of "rw" or "w"
            # (read/write or write).
            if not reg:
                msg = f"ERR=Tag '{tag}' is not a configured Modbus register for write to OPALRT"
                self.log.error(f"{msg} (tags being written: {tags})\n=> configured Modbus registers: {list(self.mb_registers.keys())}")
                return msg
            elif "w" not in reg.access:
                msg = f"ERR=Modbus Register does not have write permissions for tag '{tag}' for write to OPALRT"
                self.log.error(f"{msg} (reg.access: {reg.access}, tags being written: {tags})")
                return msg

            # write the value
            self.mb_client.write_register(reg, val)
            # TODO: save Modbus write values to Elasticsearch

        msg = f"ACK=Wrote {len(tags)} tags to OPALRT via Modbus"

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
                    f"{tag}:{self._serialize_value(value)}"
                    for tag, value in self.current_values.items()
                ]

            msg = "Write={" + ",".join(tags) + "}"
            self.publish(msg)

            # If publish_rate is not positive (0), don't sleep and go as fast as possible
            # Otherwise (if it's non-zero), log the points, and sleep like usual
            if self.conf.publish_rate:
                # NOTE: to get the raw values, run 'pybennu-test-subscriber' on any RTU
                time.sleep(self.conf.publish_rate)
