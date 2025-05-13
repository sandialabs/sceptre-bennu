"""pybennu publisher interface.

This module defines the Publisher base class.
"""

import zmq

import pybennu.distributed.swig._Endpoint as E


class Publisher:
    """ Publisher base class.

    This class implements functionality to connect to a ZMQ RADIO socket and
    publish data.
    """
    def __init__(self, endpoint):
        """ Initialize connection environment.
        """
        self.MTU = 1465

        if isinstance(endpoint, str):
            # if using a DynamicSimulatorSettings class, endpoint needs to be converted to an actual endpoint
            self.__endpoint = E.new_Endpoint()
            E.Endpoint_str_set(self.__endpoint, endpoint)
        else:
            self.__endpoint = endpoint

        self.__group = E.Endpoint_hash(self.__endpoint)
        self.connect()

    def connect(self):
        """ Connect to publish socket

        This function creates a ZMQ RADIO socket and "connects" to it.
        """
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.RADIO)
        self.__socket.connect(E.Endpoint_str_get(self.__endpoint))

    def publish(self, msg):
        """ Publish a message to the socket.
        """
        try:
            size = len(msg)
            if size > self.MTU:
                parts = msg.split(',')
                b_count = 0
                message = ""
                for p in parts:
                    part = p + ','
                    b_count += len(part)
                    temp = message + part
                    # If adding the part is smaller than MTU and we haven't consumed all the msg...
                    if len(temp) < self.MTU and b_count <= size:
                        message = temp
                    # Otherwise, send the message without 'part' and reset message
                    else:
                        self.__socket.send_string(message, group=self.__group)
                        message = part
            else:
                self.__socket.send_string(msg, group=self.__group)
        except Exception as e:
            print("Publish error: {}".format(e))
