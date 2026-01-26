import atexit
import logging
import platform
import copy
import queue
from threading import Thread
from datetime import datetime, timezone
from pprint import pformat
from typing import List, Set

from elasticsearch import Elasticsearch, helpers

from pybennu.utils import utc_now, get_bennu_version
from pybennu.settings import PybennuSettings

# Schema docs: https://sandialabs.github.io/sceptre-docs/13-providers.html

# Field type mapping for Elasticsearch.
# NOTE: the table of fields is in the docstring at the top of this file
# https://www.elastic.co/guide/en/elasticsearch/reference/current/mapping-types.html
# https://www.elastic.co/guide/en/elasticsearch/reference/current/number.html
ES_BASE_TYPE_MAPPING = {
    "@timestamp": {"type": "date"},
    "rtds_time": {"type": "date"},
    "sceptre_time": {"type": "date"},
    "simulator_time": {"type": "date"},
    "time_drift": {"type": "double"},
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
            "ip": {"type": "ip"},
            "name": {"type": "keyword"},
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
    # NOTE: this is for PMU measurements, leaving it at root for backwards compat
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
    },
    "modbus": {
        "properties": {
            "register": {"type": "integer"},
            "name": {"type": "keyword"},
            "register_type": {"type": "keyword"},  # holding, coil, discrete, input
            "data_type": {"type": "keyword"},
            "unit_type": {"type": "keyword"},
            "description": {"type": "keyword"},
            "sceptre_tag": {"type": "keyword"},
            "raw_value": {"type": "keyword"},
            "human_value": {"type": "keyword"},
            "ip": {"type": "ip"},
            "port": {"type": "integer"},
        }
    },
    "groundtruth": {
        "properties": {
            "description": {"type": "keyword"},
            "tag": {"type": "keyword"},
            "type": {"type": "keyword"},
            # Kibana doesn't allow dynamic typing without using runtime fields
            # This is a minor hack to store the value as both keyword type in .value,
            # and as it's actual type in a separate field, e.g. "float" field will have
            # the value for floating point values.
            "value": {"type": "keyword"},
            "float": {"type": "double"},
            "bool": {"type": "boolean"},
            "int": {"type": "integer"},
        }
    },
    "sceptre": {
        "properties": {
            "experiment": {"type": "keyword"},
            "topology": {"type": "keyword"},
            "scenario": {"type": "keyword"},
            "provider": {"type": "keyword"},
            "server_endpoint": {"type": "keyword"},
            "publish_endpoint": {"type": "keyword"},
        }
    },
    "node": {"type": "keyword"},
}


class ElasticPusher:
    """
    Handles export of data from providers to Elasticsearch.
    """
    TYPE_MAPPING: dict = copy.deepcopy(ES_BASE_TYPE_MAPPING)

    def __init__(self, conf: PybennuSettings) -> None:
        self.conf: PybennuSettings = conf
        self.index_cache: Set[str] = set()  # index names that are known to exist
        self.data_queue: queue.Queue = queue.Queue()  # queue of data values to push

        # Reduce verbosity of Elasticsearch's loggers
        logging.getLogger("elastic_transport").setLevel(logging.WARNING)
        logging.getLogger("elasticsearch").setLevel(logging.WARNING)
        logging.getLogger("urllib3.connectionpool").setLevel(logging.WARNING)

        # Configure logging
        self.log: logging.Logger = logging.getLogger(
            f"Elastic [{self.conf.elastic.host}]"
        )
        if self.conf.debug:
            self.log.setLevel(logging.DEBUG)
        else:
            self.log.setLevel(logging.INFO)

        # Only need to create these fields once, cache and copy into messages
        self.msg_additions: dict = {
            "ecs": {
                "version": "8.11.0",
            },
            "agent": {
                "type": "sceptre-provider",
                "version": get_bennu_version(),
            },
            "observer": {
                "hostname": platform.node(),
                "ip": str(self.conf.provider_ip),
                "name": self.conf.provider_hostname,
                "geo": {
                    "timezone": str(datetime.now(timezone.utc).astimezone().tzinfo),
                },
            },
            "sceptre": {
                "experiment": self.conf.sceptre_experiment,
                "topology": self.conf.sceptre_topology,
                "scenario": self.conf.sceptre_scenario,
                "provider": self.conf.simulator,
                "server_endpoint": self.conf.server_endpoint,
                "publish_endpoint": self.conf.publish_endpoint,
            },
        }

        self.threads = []  # use normal threads since these are long-running

    def __str__(self) -> str:
        return self.conf.elastic.host

    def start(self) -> None:
        for _ in range(self.conf.elastic.num_threads):
            t = Thread(target=self._run_push, daemon=True)
            self.threads.append(t)

        for t in self.threads:
            t.start()

    def enqueue(self, messages: List[dict]) -> None:
        """
        Add list of data bodies to push to the internal queue.
        """
        self.data_queue.put(messages)

    def _run_push(self) -> None:
        """
        Intended to be called as a thread. Runs forever, grabbing data from
        self.data_queue and pushing it to Elasticsearch.
        """
        self.log.info("Connecting to Elasticsearch...")
        es = Elasticsearch(self.conf.elastic.host)
        if not es.ping():  # cause connection to be created
            raise RuntimeError(f"failed to connect to Elasticsearch server '{self.conf.elastic.host}'")
        self.log.info("Successfully connected to Elasticsearch!")
        atexit.register(es.close)

        while True:
            # Batch up messages before sending them all in a single bulk request
            messages = []
            while len(messages) < 48:  # 8 PMUs * 6 channels
                # This blocks until there are messages
                messages.extend(self.data_queue.get())

            self.send_messages(es, messages)

    def send_messages(self, es: Elasticsearch, messages: list) -> bool:
        """
        Sends data to Elasticsearch using the Elasticsearch Bulk API.
        """
        # NOTE: this results in insane amount of log utput when running normally,
        # uncomment at your own risk (or if doing serious debugging).
        # self.log.debug(f"Pushing {len(messages)} docs to ES server at {self.conf.elastic.host}")

        ts_now = utc_now()
        actions = []

        # Create list of docs to send ("actions") using Elasticsearch's bulk API
        for message in messages:
            # Use the message's timestamp to generate the index name.
            # This ensures docs at start/end of a day get into the correct index.
            index = f"{self.conf.elastic.index_basename}-{message['@timestamp'].strftime('%Y.%m.%d')}"

            if index not in self.index_cache:
                # check with elastic, could exist and not be in the cache
                # if the provider was restarted part way through a day.
                if not es.indices.exists(index=index):
                    self.log.info(f"Creating Elasticsearch index '{index}'...")
                    es.indices.create(index=index, body={
                        "mappings": {
                            "properties": self.TYPE_MAPPING
                        }
                    })
                    self.log.info(f"Created Elasticsearch index '{index}'")
                self.index_cache.add(index)

            action = {
                "_index": index,
                "_source": {
                    **self.msg_additions,
                    **message,
                    "event": {"ingested": ts_now},
                }
            }
            actions.append(action)

        # Index the data using Elasticsearch's Bulk API.
        # 10 second timeout for request to fail.
        try:
            result = helpers.bulk(es, actions, request_timeout=10)
            if result:
                # NOTE: this results in insane amount of log utput when running normally,
                # uncomment at your own risk (or if doing serious debugging).
                # self.log.debug(f"Finished pushing {len(actions)} docs to ES")
                return True
            self.log.error(f"Empty Elasticsearch bulk result: {result}")
        except helpers.BulkIndexError as ex:
            self.log.exception("Failed Elasticsearch bulk ingest")
            self.log.error(f"** Errors **\n{pformat(ex.errors)}")
            raise RuntimeError("errors happened during elastic bulk ingest")
        except Exception:
            self.log.exception("Unknown error during Elasticsearch bulk ingest")

        return False
