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
from distutils.util import strtobool
from py_expression_eval import Parser

from pybennu.distributed.subscriber import Subscriber
from pybennu.distributed.client import Client
import pybennu.distributed.swig._Endpoint as E

#Adding a timeout helper to cause client objects not to feeze program
import signal
from contextlib import contextmanager


@contextmanager
def timeout(time):
    # Register a function to raise a TimeoutError on the signal.
    signal.signal(signal.SIGALRM, raise_timeout)
    # Schedule the signal to be sent after ``time``
    signal.alarm(time)

    try:
        yield
    except TimeoutError:
        pass
    finally:
        # Unregister the signal so it won't be triggered
        # if the timeout is not reached.
        signal.signal(signal.SIGALRM, signal.SIG_IGN)


def raise_timeout(signum, frame):
    raise TimeoutError

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
        Client.__init__(self, new_endpoint_dest)
    
    def send(self, message):
        """ Send message to Provider
        """
        with timeout(10):
            self.connect()
            # send update
            self._Client__socket.send_string(message+'\0') # must include null byte
            # get response
            msg = self._Client__socket.recv_string()
            reply = msg.split('=')
            status = reply[0]
            data = reply[1]

            if status == self._Client__kACK:
                print("I: ACK: "+data)
                #self.reply_handler(data)
            else:
                print("I: ERR -- %s" % msg)

            self._Client__socket.close()
            self._Client__context.term()

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
        self.logic = {}
        # Expression parser for logic
        self.parser = Parser()
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
            logger.info(f"Registered endpoint ---> end_name: {end_name} ---> end_destination: {end_destination}")
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
            logger.info(f"Registered subscription ---> sub_name: {sub_name} ---> sub_type: {sub_type} ---> sub_info: {sub_info}")
            self.tags.update({sub_name : 0 })
            self.types[sub_name] = sub_type
            if sub_info:
                logger.info(f"********** LOGIC **********")
                for exp in sub_info.split(';'):
                    lhs, rhs = exp.split('=')
                    self.logic[lhs.strip()] = rhs.strip()
                    logger.info(f'{exp.strip()}')
        #make sub_sources elements unique
        self.sub_sources = list(set(self.sub_sources))

        for tag in self.tags:
            self.state[tag] = False if self.get_type(tag) == 'bool' else 0
        
        for sub_source in self.sub_sources:
            logger.info(f"Launching Subscriber Thread ---> subscription: udp://{sub_source}")
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
            try:
                self.end_clients[end_dest] = alicantoClient(end_dest)
            except:
                logger.error(f"\tError Initializing Client: {self.end_clients}")
        for key in list(self.end_clients.keys()):
            logger.info(f"End_client: {key}")

    def run(self):
        ##############  Entering Execution Mode  ##############################
        logger.info("Entered alicanto execution mode")
        # Endpoint initial values to alicanto
        for i in range(self.end_count):
            full_end_name = self.endid[i]["name"]
            end_name = (full_end_name.split('/')[1]
                        if '/' in full_end_name
                        else full_end_name)
            full_end_dest = self.endid[i]["destination"]
            end_dest = (full_end_dest.split('/')[0]
                        if '/' in full_end_dest
                        else full_end_dest)
            end_dest_tag = (full_end_dest.split('/')[1]
                        if '/' in full_end_dest
                        else full_end_dest)
            try:
                self.end_clients[end_dest] = alicantoClient(end_dest)
                reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
                value = reply[1].rstrip('\x00')
                self.endid[i]["value"] = value
                self.tag(full_end_dest, value)
                logger.debug(f"Initial Endpoints {end_name} / {end_dest}:{value} ")
        
            except:
                logger.error(f"\tError Initializing Client: {self.end_clients}")
                continue
    
        ########## Main co-simulation loop ####################################
        while True:
            self.publish_state()
            time.sleep(0.1)
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

                if self.types[full_end_name] == 'float' or self.types[full_end_name] == 'double':
                    if not math.isclose(float(self.tag(full_end_name)), float(self.tag(full_end_dest))):
                        #Handle Logic
                        if self.logic[full_end_dest] is not None:
                            expr = self.parser.parse(self.logic[full_end_dest])
                            '''
                            # Assign variables
                            vars = {}
                            for var in expr.variables():
                                vars[var] = self.tag(var)
                            '''
                            i = 0
                            # Assign vars not working, so assign token manually
                            for token in expr.tokens:
                                for search_tag in self.tags:
                                    if token.toString() == search_tag:
                                         expr.tokens[i].number_ = self.tag(token.toString())
                                i += 1
                            # Evaluate expression
                            value = expr.evaluate(vars)
                            value = str(value).lower()
                            if value != self.tag(full_end_dest):
                                logger.debug(f"\tLOGIC: {full_end_dest.strip()}={self.logic[full_end_dest]} ----> {value}")
                                # Assign new tag value
                                self.tag(full_end_dest, value)
                            # Skip if value is unchanged
                            elif value == self.tag(full_end_dest):
                                continue
                            
                        try:
                            self.end_clients[end_dest] = alicantoClient(end_dest)
                            if self.logic[full_end_dest] is not None:
                                self.end_clients[end_dest].write_analog_point(end_dest_tag, self.tag(full_end_dest))
                            else:
                                self.end_clients[end_dest].write_analog_point(end_dest_tag, self.tag(full_end_name))
                            time.sleep(0.5)
                            reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
                            value = reply[1].rstrip('\x00')
                            self.tag(full_end_dest, value)
                        except:
                            logger.error(f"\tError Initializing Client: {self.end_clients}")
                            continue
                elif self.types[full_end_name] == 'bool':
                    if str(self.tag(full_end_name)).lower() != str(self.tag(full_end_dest)).lower():
                        #Handle Logic
                        if self.logic[full_end_dest] is not None:
                            expr = self.parser.parse(self.logic[full_end_dest])
                            '''
                            # Assign variables
                            vars = {}
                            for var in expr.variables():
                                vars[var] = self.tag(var)
                            '''
                            i = 0
                            # Assign vars not working, so assign token manually
                            for token in expr.tokens:
                                for search_tag in self.tags:
                                    if token.toString() == search_tag:
                                         expr.tokens[i].number_ = bool(self.tag(token.toString()))
                                i += 1
                            # Evaluate expression
                            value = expr.evaluate(vars)
                            value = str(value)
                            if value != self.tag(full_end_dest):
                                logger.debug(f"\tLOGIC: {full_end_dest.strip()}={self.logic[full_end_dest]} ----> {value}")
                                # Assign new tag value
                                self.tag(full_end_dest, value)
                            # Skip if value is unchanged
                            elif value == self.tag(full_end_dest):
                                continue
                        try:
                            self.end_clients[end_dest] = alicantoClient(end_dest)
                            if self.logic[full_end_dest] is not None:
                                self.end_clients[end_dest].write_digital_point(end_dest_tag, eval(self.tag(full_end_dest)))
                            else:
                                self.end_clients[end_dest].write_digital_point(end_dest_tag, eval(self.tag(full_end_name)))
                            time.sleep(0.5)
                            reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
                            value = reply[1].rstrip('\x00')
                            self.tag(full_end_dest, value)
                        except:
                            logger.error(f"\tError Initializing Client: {self.end_clients}")
                            continue

    def publish_state(self):
        logger.info("=================== DATA ===================")
        for tag in self.tags:
            logger.info(f"{tag:<30} --- {self.tag(tag):}")
        logger.info("============================================")

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
        points = points[:-1] # remove last element since it might be empty
        sub_source = threading.current_thread().name
        
        for point in points:
            if not point:
                continue 
            if point == "":
                continue

            try:
                tag = point.split(':')[0]
                full_tag = sub_source + '/' + tag
                value = point.split(':')[1]
            except:
                continue
            
            if full_tag not in self.tags:
                continue

            if self.types[full_tag] == 'bool':
                if value.lower() == 'false' or value == '0':
                    value = False
                    field = 'status'
                elif value.lower() == 'true' or value == '1':
                    value = True
                    field = 'status'
            else:
                value = float(value)
                field = 'value'

            if field == 'value':
                if not math.isclose(float(self.tag(full_tag)), value):  
                    self.tag(full_tag, value)
                    logger.debug("UPDATE NOW: "+full_tag)
                    logger.debug("New value: "+str(value))
                else:
                    continue
            elif field == 'status':
                if self.tag(full_tag) != value:  
                    self.tag(full_tag, value)
                    logger.debug("UPDATE NOW: "+full_tag)
                    logger.debug("New value: "+str(value))
                else:
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