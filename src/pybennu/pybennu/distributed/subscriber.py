"""pybennu subscriber interface.

This module defines the Subscriber base class. It implements an interface to
subscribe to data published by a Provider.
"""

import zmq

import pybennu.distributed.swig._Endpoint as E


class Subscriber:
    """ Subscriber base class.

    This class implements functionality to bind to a ZMQ DISH socket
    and subscribe to data being published by a Provider RADIO socket.
    Inheriting classes can implement their own subscription handler by
    setting self.susbcription_handler = <custom handler>.
    """
    def __init__(self, endpoint):
        """ Initialize connection environment.
        """
        self.__endpoint = endpoint
        self.subscription_handler = self.__defaultHandler
        self.bind()

    def bind(self):
        """ Bind to DISH socket and join a group.
        """
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.DISH)
        self.__socket.bind(E.Endpoint_str_get(self.__endpoint))
        self.__socket.join(E.Endpoint_hash(self.__endpoint))

    def __defaultHandler(self, message):
        print("Received subscription update: %s" % message)

    def run(self):
        """ Listen for messages from a Publisher.
        """
        while True:
            try:
                message = self.__socket.recv_string()
            except zmq.Again:
                print("E: Subscriber missed a message")
                continue
            self.subscription_handler(message)
