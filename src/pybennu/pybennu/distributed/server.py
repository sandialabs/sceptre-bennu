"""pybennu server interface.
"""

import zmq

import pybennu.distributed.swig._Endpoint as E


class Server:
    """ Server base class.

    This class implements functionality to bind to a ZMQ REP socket,
    wait for requests from clients, and respond. Inheriting classes can
    implement their own request handler by setting self.request_handler =
    <custom handler>.
    """
    __kACK = "ACK"
    __kERROR = "ERR"

    def __init__(self, endpoint):
        """ Initialize connection environment.
        """

        if isinstance(endpoint, str):
            # if using a DynamicSimulatorSettings class, endpoint needs to be converted to an actual endpoint
            self.__endpoint = E.new_Endpoint()
            E.Endpoint_str_set(self.__endpoint, endpoint)
        else:
            self.__endpoint = endpoint

        self.request_handler = self.__defaultHandler
        self.bind()

    def bind(self):
        """ Bind to listening socket.
        """
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.REP)
        self.__socket.bind(E.Endpoint_str_get(self.__endpoint))

    def __defaultHandler(self, request):
        return "ACK="

    def run(self):
        """ Listen for requests and reply to them.
        """
        while True:
            request = self.__socket.recv_string()
            print("Server ---- Received request: {}".format(request))
            reply = self.request_handler(request.strip('\x00'))
            self.__socket.send_string(reply+'\x00') # must include null byte
            print("Server ---- Sent reply: {}".format(reply))
