"""Alicanto is a new feature made to be a more simple co-simulation tool than HELICS.

The code is similar to the bennu HELICS code but stripped down.
Alicanto runs as a Subscriber and Client object. It takes in a configuration file (which points to a json) which defines which points Alicanto cares about
JSON format
 - Subscriptions tell Alicanto which publish point (udp) to subscrie to and which point to keep track of
 - Endpoints tell Alicanto where to corelate that subscribed point to a server-endpoint
"""
import logging
import threading
import argparse
import json
import signal
import sys
import time
import math

from pybennu.distributed.subscriber import Subscriber
from pybennu.distributed.client import Client
import pybennu.distributed.swig._Endpoint as E

logging.basicConfig(level=logging.DEBUG,format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger('alicanto')
#logger.addHandler(logging.StreamHandler())


class alicantoSubscriber(Subscriber):
    def __init__(self, sub_source):
        new_publish_endpoint = E.new_Endpoint()
        E.Endpoint_str_set(new_publish_endpoint, 'udp://'+str(sub_source))
        Subscriber.__init__(self, new_publish_endpoint)

class alicantoClient(Client):
    def __init__(self, end_dest):
        new_endpoint_dest = E.new_Endpoint()
        E.Endpoint_str_set(new_endpoint_dest, 'tcp://'+str(end_dest))
        self.endpointName = 'tcp://'+str(end_dest)
        Client.__init__(self, new_endpoint_dest)
        
    def send(self, message):
        """ Send message to Provider
        """
        # send update
        self._Client__socket.send_string(message+'\0') # must include null byte
        # get response
        msg = self._Client__socket.recv_string()
        reply = msg.split('=')
        status = reply[0]
        data = reply[1]

        if status != self._Client__kACK:
            logger.error(msg)

        return reply

class alicanto():
    def __init__(self, config, debug=False, exit_handler=None):

        if debug:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.INFO)

        exit = exit_handler if exit_handler else self.ctrl_exit_handler
        signal.signal(signal.SIGINT, exit) # ctlr + c

        self.lock = threading.Lock()
        
        # Initialize system state
        self.state = {}
        # Tag=>destination map
        self.dests = {}
        # Tag=>type map
        self.types = {}
        # Set of all tags 
        self.tags = {}

        ##############  Get counts from json  ######################
        cfg = None
        self.end_count = 0
        self.sub_count = 0 
        self.pub_count = 0

        with open(config, 'r') as file:
            cfg = json.loads(file.read())
            try:
                self.end_count = len(cfg["endpoints"])
                logger.info(f"Number of endpoints: {self.end_count}")
            except:
                logger.exception("No endpoints found or endpoints incorrectly formated, check configuration file")
            try:
                self.sub_count = len(cfg["subscriptions"])
                logger.info(f"Number of subscriptions: {self.sub_count}")
            except:
                logger.exception("No subscriptions found or subscriptions incorrectly formated, check configuration file")

        # Diagnostics to confirm JSON config correctly added the required
        #   endpoints and subscriptions
        self.endid = {}
        self.end_dests = []
        for i, endpoint in enumerate(cfg["endpoints"]):
            self.endid[i] = endpoint
            end_name = endpoint["name"]
            end_destination = endpoint["destination"]
            end_type = endpoint["type"]
            logger.debug(f"Registered endpoint ---> end_name: {end_name} ---> end_destination: {end_destination}")
            self.tags.update({end_destination : 0})
            self.end_dests.append(end_destination)
            self.dests[end_name] = end_destination
            self.types[end_name] = end_type
            self.types[end_destination] = end_type
        # make end_dests elements unique
        self.end_dests = list(set(self.end_dests))

        self.subid = {}
        self.sub_sources = []
        for i in range(0, self.sub_count):
            self.subid[i] = cfg["subscriptions"][i]
            sub_name = self.subid[i]["key"]
            sub_type = self.subid[i]["type"]
            sub_source = sub_name.split('/')[0]
            self.sub_sources.append(sub_source)
            try:
                sub_info = self.subid[i]["info"] # stores logic for interdependencies
            except:
                sub_info = None
            logger.debug(f"Registered subscription ---> sub_name: {sub_name} ---> sub_type: {sub_type} ---> sub_info: {sub_info}")
            #sub_name = sub_name.split('/')[1] if '/' in sub_name else sub_name
            self.tags.update({sub_name : 0 })
            self.types[sub_name] = sub_type
            if sub_info:
                logger.debug(f"********** LOGIC **********")
                for exp in sub_info.split(';'):
                    lhs, rhs = exp.split('=')
                    self.logic[lhs.strip()] = rhs.strip()
                    logger.debug(f'{exp.strip()}')
        #make sub_sources elements unique
        self.sub_sources = list(set(self.sub_sources))

        for tag in self.tags:
            self.state[tag] = False if self.get_type(tag) == 'bool' else 0
        
        for sub_source in self.sub_sources:
            logger.debug(f"Launching Subscriber Thread ---> subscription: udp://{sub_source}")
            subber = alicantoSubscriber(sub_source)
            subber.subscription_handler = self._subscription_handler
            self.__sub_thread = threading.Thread(target=subber.run)
            self.__sub_thread.setName(sub_source)
            self.__sub_thread.daemon = True
            self.__sub_thread.start()
         
        self.end_clients = {}
        for end_dest in self.end_dests:
            # Initialize bennu Client
            end_dest = end_dest.split('/')[0]
            self.end_clients[end_dest] = alicantoClient(end_dest)
        for key in list(self.end_clients.keys()):
            logger.debug(f"End_client: {key}")

    def run(self):
        ##############  Entering Execution Mode  ##############################
        logger.info("Entered alicanto execution mode")
    
        ########## Main co-simulation loop ####################################
        while True:
            self.publish_state()
            for key, value in self.endid.items():
                full_end_name = value["name"]
                end_name = (full_end_name.split('/')[1]
                            if '/' in full_end_name
                            else full_end_name)
                full_end_dest = value["destination"]
                end_dest = (full_end_dest.split('/')[0]
                            if '/' in full_end_dest
                            else full_end_dest)
                end_dest_tag = (full_end_dest.split('/')[1]
                            if '/' in full_end_dest
                            else full_end_dest)

                # !!need to add something to handle binary points
                if self.types[full_end_name] == 'float' or self.types[full_end_name] == 'double':
                    if not math.isclose(float(self.tag(full_end_name)), float(self.tag(full_end_dest))):
                        self.end_clients[end_dest].write_analog_point(end_dest_tag, self.tag(full_end_name))
                        reply = self.end_clients[end_dest].send("READ="+end_name)
                        value = reply[1].rstrip('\x00')
                        self.tag(full_end_dest, value)
                elif self.types[full_end_name] == 'bool':
                    if str(self.tag(full_end_name)).lower() != str(self.tag(full_end_dest)).lower():
                        self.end_clients[end_dest].write_digital_point(end_dest_tag, self.tag(full_end_name))
                        reply = self.end_clients[end_dest].send("READ="+end_name)
                        value = reply[1].rstrip('\x00')
                        self.tag(full_end_dest, value)

    def publish_state(self):
        logger.debug("=================== DATA ===================")
        for tag in self.tags:
            logger.debug(f"{tag:<30} --- {self.tag(tag):}")
        logger.debug("============================================")

    def get_type(self, tag):
        return self.types[tag]

    def tag(self, tag, value=None):
        with self.lock:
            if value is not None:
                self.state[tag] = value
            else:
                if tag in self.state:
                    return self.state[tag]
                else:
                    return False if self.get_type(tag) == 'bool' else 0

    def _subscription_handler(self, message):
        """Receive Subscription message
        This method gets called by the Subscriber base class in its run()
        method when a message from a publisher is received.

        Ex message: "point1.value:10.50,point2.value:1.00,"

        Args:
            message (str): published zmq message as a string
        """
        points = message.split(',')
        sub_source = threading.current_thread().name
        
        for point in points:
            if not point:
                continue 
            if point == "":
                continue

            tag = point.split(':')[0]
            full_tag = sub_source + '/' + tag
            value = point.split(':')[1]
            
            if full_tag not in self.tags:
                continue

            if value.lower() == 'false':
                value = False
                field = 'status'
            elif value.lower() == 'true':
                value = True
                field = 'status'
            else:
                value = float(value)
                field = 'value'

            if field == 'value':
                if not math.isclose(float(self.tag(full_tag)), value):  
                    self.tag(full_tag, value)
                    logger.info("UPDATE NOW: "+full_tag)
                    logger.info("New value: "+str(value))
                else:
                    continue
            elif field == 'status':
                logger.info("Cannot handle binary points")
                continue
            else:
                continue
            
    def ctrl_exit_handler(self, signal, frame):
        logger.info("SIGINT or CTRL-C detected. Exiting gracefully")
        sys.exit()

def main():

    parser = argparse.ArgumentParser(description="SCEPTRE Power Solver Service")
    parser.add_argument('-c', '--config-file', dest='config_file',default="/root/repos/alicanto.json",
                        help='Config file to load (.json)')
    parser.add_argument('-d', '--debug', dest='debug',default=True,
                        help='')       
    
    args = parser.parse_args()

    solver = alicanto(args.config_file,debug=args.debug)
    solver.run()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit()