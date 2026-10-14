"""
Provider interface.

This module defines the pybennu Provider base class. It is the interface which
all providers will implement.
"""

import logging
import threading
from abc import ABC, abstractmethod

import pybennu.distributed.publisher as publisher
import pybennu.distributed.server as server

logger = logging.getLogger(__name__)


class Provider(ABC):
    """
    Provider base class.

    Inheriting providers must implement:

    - query()
    - read(tag)
    - write(tags)
    - periodic_publish()
    """

    def __init__(self, server_endpoint: str, publish_endpoint: str) -> None:
        """Initialize connection environment."""
        self.__publisher = publisher.Publisher(publish_endpoint)

        self.__publish_thread = threading.Thread(target=self.periodic_publish)
        self.__publish_thread.daemon = True

        self.__server = server.Server(server_endpoint)
        self.__server.request_handler = self.__message_handler

    @abstractmethod
    def query(self) -> str:
        """
        Return all current tag names.

        Must return 'ACK=tag1,tag2,...' or 'ERR=<error message>'.
        """
        pass

    @abstractmethod
    def read(self, tag: str) -> str:
        """
        Read the current value of a single tag.

        Must return 'ACK=<value>' or 'ERR=<error message>'.
        """
        pass

    @abstractmethod
    def write(self, tags: dict) -> str:
        """
        Write values to one or more tags.

        Must return 'ACK=<success message>' or 'ERR=<error message>'.
        """
        pass

    @abstractmethod
    def periodic_publish(self) -> None:
        """
        Publish all tags periodically.
        Inheriting providers must implement.
        """
        pass

    def publish(self, msg: str) -> None:
        """Publish message using publisher."""
        self.__publisher.publish(msg)

    def run(self) -> None:
        """Start periodic publish thread and server."""
        self.__publish_thread.start()
        self.__server.run()

    def __message_handler(self, message: str) -> str:
        """
        Message requests should be in the form:
            QUERY=
            READ=<tag name>
            WRITE=<tag name>:<value>[,<tag name>:<value>,...]
            WRITE={tag name:value, tag name:value}

        Note that the last example above uses a JSON blob as the payload
        instead of a plain ol' string.
        """
        if not message:
            return "ERR=Message empty"
        if "=" not in message:
            return "ERR=Invalid message request"

        op, _, payload = message.partition("=")
        logger.debug("Received %s request with payload %s", op, payload)

        reply = ""
        if op == "QUERY" or op == "query":
            reply += self.query()
        elif op == "READ" or op == "read":
            reply += self.read(payload)
        elif op == "WRITE" or op == "write":
            tags = {}
            for tag in payload.split(","):
                if tag:
                    name, sep, value = tag.partition(":")
                    if not sep:
                        return f"ERR=Malformed WRITE point '{tag}'"
                    tags[name] = value
            reply += self.write(tags)
        else:
            reply += f"ERR=Unknown command type '{op}'"

        logger.debug("Sending reply for payload %s -- %s", payload, reply)
        return reply
