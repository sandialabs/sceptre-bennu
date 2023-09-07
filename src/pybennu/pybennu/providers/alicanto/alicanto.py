###################### UNCLASSIFIED // OFFICIAL USE ONLY ######################
#
# Copyright 2018 National Technology & Engineering Solutions of Sandia,
# LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,
# there is a non-exclusive license for use of this work by or on behalf
# of the U.S. Government. Export of this data may require a license from
# the United States Government.
#
###############################################################################
"""bennu provider / alicanto handler

Provides an interface between bennu field devices and a alicanto federation.
"""
import logging
import threading

from distutils.util import strtobool

from pybennu.distributed.client import Client
import pybennu.distributed.swig._Endpoint as E
from pybennu.providers.alicanto.alicanto_helper import alicantoFederate

logger = logging.getLogger('alicanto')
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.INFO)


class Alicanto(alicantoFederate):
    """ bennu provider / alicanto federate """

    def __init__(self, server_endpoint, publish_endpoint, config, debug=False):
        self.__lock = threading.Lock()
        if debug:
            logger.setLevel(logging.DEBUG)


        
        # Initialize alicantoFederate and start run() method in a separate thread
        ## Need to write a way for alicanto to publish updates
        alicantoFederate.__init__(self, config)
        self.__publish_thread = threading.Thread(target=alicantoFederate.run,
                                                 args=(self,))
        self.__publish_thread.daemon = True
        self.__publish_thread.start()
        
        # Initialize system state
        self.state = {}
        for tag in self.tags:
            self.state[tag] = False if self.get_type(tag) == 'bool' else 0


    def tag(self, tag, value=None):
        with self.__lock:
            if value is not None:
                self.state[tag] = value
            else:
                if tag in self.state:
                    return self.state[tag]
                else:
                    return False if self.get_type(tag) == 'bool' else 0


    # CLIENT write helper
    def write(self, tag, value):
        with self.__lock:
            print("### PROCESSING UPDATE ###")
            try:
                for tag in self.tags:
                    print(f"### TAG: {tag} ###")
                    if tag not in self.state:
                        raise NameError(f"{tag} is not a known tag name")
                    value = self.tags[tag]
                    self.state[tag] = value
                    self.send_msg_to_endpoint(tag, value)
                    print(f"### UPDATED: {tag}:{value} ###")
            except Exception as err:
                print(f"ERROR: failed to process update: {err}")
                return f'ERR={err}'
            return 'ACK=Success processing alicanto write command'
