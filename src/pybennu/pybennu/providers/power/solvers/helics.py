"""bennu provider / HELICS federate

Provides an interface between bennu field devices and a helics federation.
"""
import logging
import threading

from distutils.util import strtobool

from pybennu.distributed.provider import Provider
from pybennu.providers.utils.helics_helper import HelicsFederate

logger = logging.getLogger('helics')
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.INFO)


class Helics(Provider, HelicsFederate):
    """ bennu provider / Helics federate """

    def __init__(self, server_endpoint, publish_endpoint, config, debug=False):
        self.__lock = threading.Lock()
        if debug:
            logger.setLevel(logging.DEBUG)

        # Initialize bennu Provider
        Provider.__init__(self, server_endpoint, publish_endpoint)

        # Initialize HelicsFederate and start run() method in a separate thread
        HelicsFederate.__init__(self, config)
        self.__publish_thread = threading.Thread(target=HelicsFederate.run,
                                                 args=(self,))
        self.__publish_thread.daemon = True
        self.__publish_thread.start()

        # Initialize system state
        self.state = {}
        for tag in self.tags:
            self.state[tag] = False if self.get_type(tag) == 'bool' else 0

    def action_post_request_time(self):
        self.publish_data()

    def tag(self, tag, value=None):
        with self.__lock:
            if value is not None:
                self.state[tag] = value
            else:
                if tag in self.state:
                    return self.state[tag]
                else:
                    return False if self.get_type(tag) == 'bool' else 0

    ##############################################################
    ##############  bennu provider methods  ######################
    ##############################################################
    def query(self):
        return f'ACK={",".join([tag for tag in self.tags])}'

    def read(self, tag):
        msg = ''
        with self.__lock:
            if tag in self.state:
                msg = f'ACK={str(self.state[tag]).lower()}'
            else:
                msg = f'ERR=Tag not found: {tag}'
        return msg

    # 'tags' will be a dictionary of key/value pairs
    def write(self, tags):
        with self.__lock:
            print("### PROCESSING UPDATE ###")
            try:
                for tag in tags:
                    print(f"### TAG: {tag} ###")
                    if tag not in self.state:
                        raise NameError(f"{tag} is not a known tag name")
                    value = tags[tag]
                    self.state[tag] = value
                    self.send_msg_to_endpoint(tag, value)
                    print(f"### UPDATED: {tag}:{value} ###")
            except Exception as err:
                print(f"ERROR: failed to process update: {err}")
                return f'ERR={err}'
            return 'ACK=Success processing Helics write command'

    def periodic_publish(self):
        pass
    ##############################################################
    ##############################################################
    ##############################################################

    def publish_data(self):
        msg = []

        with self.__lock:
            for tag, value in self.state.items():
                if self.get_type(tag) == 'bool':
                    # Make sure any bool val is 'true'|'false'
                    val = bool(strtobool(str(value)))
                    val = str(val).lower()
                    msg.append(f'{tag}:{val}')
                else:
                    val = float(value)
                    msg.append(f'{tag}:{val}')
        self.publish(','.join(msg))

    # Override `run` function from both the Provider and HelicsFederate parent
    # classes to ensure just the parent Provider.run function is called.  The
    # HelicsFederate.run function is called as a separate thread in the
    # constructor.
    def run(self):
        Provider.run(self)
