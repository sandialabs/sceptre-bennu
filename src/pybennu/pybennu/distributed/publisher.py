"""pybennu publisher interface.

This module defines the Publisher base class.
"""

import zmq
from loguru import logger

import pybennu.distributed.swig._Endpoint as E
from pybennu.distributed.utils import (
    chunk_message,
    publish_body_limit,
    publish_link_mtu,
)


class Publisher:
    """Publisher base class.

    This class implements functionality to connect to a ZMQ RADIO socket and
    publish data.
    """

    def __init__(self, endpoint):
        """Initialize connection environment."""
        if isinstance(endpoint, str):
            # if using a DynamicSimulatorSettings class, endpoint needs to be converted to an actual endpoint
            self.__endpoint = E.new_Endpoint()
            E.Endpoint_str_set(self.__endpoint, endpoint)
        else:
            self.__endpoint = endpoint

        self.__group = E.Endpoint_hash(self.__endpoint)
        # pybennu sends no NUL, so the whole body is message bytes
        self.max_body = publish_body_limit(publish_link_mtu(), len(self.__group))
        self.connect()

    def connect(self):
        """Connect to publish socket

        This function creates a ZMQ RADIO socket and "connects" to it.
        """
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.RADIO)
        self.__socket.connect(E.Endpoint_str_get(self.__endpoint))

    def publish(self, msg):
        """Publish a message to the socket."""
        try:
            if len(msg.encode()) > self.max_body:
                # chunk_message keeps each chunk strictly under its limit
                for chunk in chunk_message(msg, self.max_body + 1):
                    self.__socket.send_string(chunk, group=self.__group)
            else:
                self.__socket.send_string(msg, group=self.__group)
        except Exception as e:
            logger.exception("Publish error: {}", e)
