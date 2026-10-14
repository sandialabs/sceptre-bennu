"""pybennu server interface."""

import logging

import zmq

import pybennu.distributed.swig._Endpoint as E

logger = logging.getLogger(__name__)


class Server:
    """Server base class.

    This class implements functionality to bind to a ZMQ REP socket,
    wait for requests from clients, and respond. Inheriting classes can
    implement their own request handler by setting self.request_handler =
    <custom handler>.
    """

    __kACK = "ACK"
    __kERROR = "ERR"

    def __init__(self, endpoint):
        """Initialize connection environment."""

        if isinstance(endpoint, str):
            # if using a DynamicSimulatorSettings class, endpoint needs to be converted to an actual endpoint
            self.__endpoint = E.new_Endpoint()
            E.Endpoint_str_set(self.__endpoint, endpoint)
        else:
            self.__endpoint = endpoint

        self.request_handler = self.__defaultHandler
        self.bind()

    def bind(self):
        """Bind to listening socket."""
        self.__context = zmq.Context()
        self.__socket = self.__context.socket(zmq.REP)
        self.__socket.bind(E.Endpoint_str_get(self.__endpoint))

    def __defaultHandler(self, request):
        return "ACK="

    def run(self):
        """Listen for requests and reply to them."""
        while True:
            try:
                request = self.__socket.recv_string()
                logger.debug("Server ---- Received request: %s", request)
                reply = self.request_handler(request.strip("\x00"))
            except zmq.ZMQError:
                raise
            except Exception:
                # REP must reply after a recv (even a failed decode), or the socket wedges
                logger.exception("Server ---- request handler failed")
                reply = "ERR=Internal error"
            self.__socket.send_string(reply + "\x00")  # must include null byte
            logger.debug("Server ---- Sent reply: %s", reply)
