"""pybennu client interface.
"""

import zmq

import pybennu.distributed.swig._Endpoint as E


class Client:
    """ Client base class.

    This class implements functionality to connect to a Provider via ZMQ
    sockets and send request messages. Inheriting classes can implement
    their own reply handler by setting self.reply_handler = <custom handler>.
    """
    __kACK = "ACK"
    __kERROR = "ERR"

    def __init__(self, endpoint):
        """ Initialize connection environment.
        """
        self.__endpoint = endpoint
        self.reply_handler = self.__defaultHandler
        self.connect()

    def connect(self):
        """ Connect to Provider listening socket

        This function creates a ZMQ REQ socket and "connects" to it.
        """
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.REQ)
        self.__socket.setsockopt(zmq.LINGER, 0)
        self.__socket.connect(E.Endpoint_str_get(self.__endpoint))

    def __defaultHandler(self, reply):
        """ Default reply handler.
        """
        print("Client received reply: %s" % reply)

    def send(self, message):
        """ Send message to Provider
        """
        # send update
        self.__socket.send_string(message+'\0') # must include null byte
        # get response
        msg = self.__socket.recv_string()
        reply = msg.split('=')
        status = reply[0]
        data = reply[1]

        if status == self.__kACK:
            print("I: ACK")
            self.reply_handler(data)
        else:
            print("I: ERR -- %s" % msg)

    def write_analog_point(self, tag, value):
        """Updates the analog value of a tag.

        Args:
            tag: String name of tag to update.
            value: Value that the tag will be updated to.
        """

        update = "WRITE=" + tag + ":" + str(value)
        self.send(update)

    def write_digital_point(self, tag, value):
        """Updates the boolean value of a tag.

        Args:
            tag: String name of tag to update.
            value: Value that the tag will be updated to.
        """
        val = "true" if value else "false"
        update = "WRITE=" + tag + ":" + val
        self.send(update)
