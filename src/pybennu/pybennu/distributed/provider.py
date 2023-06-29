"""Provider interface.

This module defines the pybennu Provider base class. It is the interface which
all providers will implement.
"""
import threading
from abc import ABC, abstractmethod

import pybennu.distributed.publisher as publisher
import pybennu.distributed.server as server


class Provider(ABC):
    """Provider base class."""

    def __init__(self, server_endpoint, publish_endpoint):
        """Initialize connection environment."""
        self.__publisher = publisher.Publisher(publish_endpoint)
        self.__publish_thread = threading.Thread(target=self.periodic_publish)
        self.__publish_thread.daemon = True
        self.__server = server.Server(server_endpoint)
        self.__server.request_handler = self.__message_handler

    def run(self):
        """Start periodic publish thread and server."""
        self.__publish_thread.start()
        self.__server.run()

    def __message_handler(self, message):
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
        elif '=' not in message:
            return "ERR=Invalid message request"
        split = message.split('=')
        op = split[0]
        payload = split[1]
        print("Received %s request with payload %s" % (op, payload))
        reply = ""
        if op == "QUERY" or op == "query":
            reply += self.query()
        elif op == "READ" or op == "read":
            reply += self.read(payload)
        elif op == "WRITE" or op == "write":
            tags = {}
            for tag in payload.split(','):
                if tag:
                    split = tag.split(':')
                    tags[split[0]] = split[1]
            reply += self.write(tags)
        else:
            reply += "ERR=Unknown command type '%s'", op
        print("Sending reply for payload %s -- %s" % (payload, reply))
        return reply

    def publish(self, msg):
        """Publish message using publisher."""
        self.__publisher.publish(msg)

    # Methods to process incoming message.
    # Inheriting providers must implement these.

    @abstractmethod
    def query(self):
        """Must return 'ACK=tag1,tag2,...' or 'ERR=<error message>'."""
        pass

    @abstractmethod
    def read(self, tag):
        """Must return 'ACK=<value>' or 'ERR=<error message>'."""
        pass

    @abstractmethod
    def write(self, tags):
        """Must return 'ACK=<success message>' or 'ERR=<error message>'."""
        pass

    @abstractmethod
    def periodic_publish(self):
        """Method to periodically publish data. Inheriting providers must implement."""
        pass
