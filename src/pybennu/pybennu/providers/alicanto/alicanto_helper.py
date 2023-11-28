"""alicanto helper class
"""
import json
import logging
import signal
import sys
import time
import threading
import math

from distutils.util import strtobool
from py_expression_eval import Parser

from pybennu.distributed.subscriber import Subscriber
from pybennu.distributed.client import Client
import pybennu.distributed.swig._Endpoint as E

logger = logging.getLogger('alicanto')
logger.setLevel(logging.INFO)

# TestSubscriber used for easy multiple subscribers
class TestSubscriber(Subscriber):
    def __init__(self, sub_source):
        new_publish_endpoint = E.new_Endpoint()
        E.Endpoint_str_set(new_publish_endpoint, 'udp://'+str(sub_source))
        Subscriber.__init__(self, new_publish_endpoint)

#TestClient used with special send function which returns the reply
class TestClient(Client):
    def __init__(self, end_dest):
        new_endpoint_dest = E.new_Endpoint()
        E.Endpoint_str_set(new_endpoint_dest, 'tcp://'+str(end_dest))
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

        if status == self._Client__kACK:
            print("I: ACK: "+data)
            #self.reply_handler(data)
        else:
            print("I: ERR -- %s" % msg)

        return reply

    def close(self):
        #self._Client__socket.close()
        self._Client__context.term()

class alicantoFederate():
    def __init__(self, config, exit_handler=None):
        exit = exit_handler if exit_handler else self.ctrl_exit_handler
        signal.signal(signal.SIGINT, exit) # ctlr + c
        # Tag=>destination map
        self.dests = {}
        # Tag=>type map
        self.types = {}
        # Tag=>logic map
        self.logic = {}
        # Expression parser for logic
        self.parser = Parser()
        # Set of all tags (for printing)
        self.tags = {}

        ##############  Get counts from json  ######################
        cfg = None
        self.end_count = 0
        self.sub_count = 0
        self.pub_count = 0

        with open(config, 'r') as file_:
            cfg = json.loads(file_.read())
            try:
                self.end_count = len(cfg["endpoints"])
                logger.info(f"\tNumber of endpoints: {self.end_count}")
            except:
                logger.info(f"\tNumber of endpoints: {self.end_count}")
            try:
                self.sub_count = len(cfg["subscriptions"])
                logger.info(f"\tNumber of subscriptions: {self.sub_count}")
            except:
                logger.info(f"\tNumber of subscriptions: {self.sub_count}")
            try:
                self.pub_count = len(cfg["publications"])
                logger.info(f"\tNumber of publications: {self.pub_count}")
            except:
                logger.info(f"\tNumber of publications: {self.pub_count}")

        # Diagnostics to confirm JSON config correctly added the required
        #   endpoints, publications, and subscriptions
        self.endid = {}
        self.end_dests = []
        for i in range(0, self.end_count):
            self.endid[i] = cfg["endpoints"][i]
            end_name = self.endid[i]["name"]
            end_destination = self.endid[i]["destination"]
            end_type = self.endid[i]["type"]
            logger.debug(f"\tRegistered endpoint ---> end_name: {end_name} ---> end_destination: {end_destination}")
            #end_name = end_name.split('/')[1] if '/' in end_name else end_name
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
            logger.debug(f"\tRegistered subscription ---> sub_name: {sub_name} ---> sub_type: {sub_type} ---> sub_info: {sub_info}")
            #sub_name = sub_name.split('/')[1] if '/' in sub_name else sub_name
            self.tags.update({sub_name : 0 })
            self.types[sub_name] = sub_type
            if sub_info:
                logger.debug(f"\t\t********** LOGIC **********")
                for exp in sub_info.split(';'):
                    lhs, rhs = exp.split('=')
                    self.logic[lhs.strip()] = rhs.strip()
                    logger.debug(f'\t\t{exp.strip()}')
        #make sub_sources elements unique
        self.sub_sources = list(set(self.sub_sources))
        
        self.__lock = threading.Lock()


        for sub_source in self.sub_sources:
            logger.debug(f"\tLaunching Subscriber Thread ---> subscription: udp://{sub_source}")
            subber = TestSubscriber(sub_source)
            #Subscriber.__init__(self, new_publish_endpoint)
            #subber = Subscriber(new_publish_endpoint)
            subber.subscription_handler = self._subscription_handler
            self.__sub_thread = threading.Thread(target=subber.run)
            self.__sub_thread.setName(sub_source)
            self.__sub_thread.daemon = True
            self.__sub_thread.start()
            #subber.run()            
                  
        #new_server_endpoint = E.new_Endpoint()
        #E.Endpoint_str_set(new_server_endpoint, 'tcp://127.0.0.1:5556')
        #self.client = Client(new_server_endpoint)
        #self.client = TestClient("127.0.0.1:5556")


        self.end_clients = {}
        for end_dest in self.end_dests:
            # Initialize bennu Client
            end_dest = end_dest.split('/')[0]
            #self.end_clients[end_dest] = TestClient(end_dest)
            self.end_clients[end_dest] = None
        logger.debug(f"\tEnd_clients: {self.end_clients}")

    def run(self):
        ##############  Entering Execution Mode  ##############################
        #h.alicantoFederateEnterExecutingMode(self.fed)
        logger.info("Entered alicanto execution mode")
        '''
        grantedtime = 0

        # Blocking call for a time request at simulation time 0
        time = h.alicanto_TIME_ZERO
        logger.debug(f"Requesting initial time {time}")
        grantedtime = h.alicantoFederateRequestTime(self.fed, time)
        logger.debug(f"Granted time {grantedtime}")
        '''
        
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
            #value = self.tag(end_name)
            self.end_clients[end_dest] = TestClient(end_dest)
            reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
            #Try to keep up with threads
            time.sleep(1)
            self.end_clients[end_dest].close()
            self.end_clients[end_dest] = None
            value = reply[1].rstrip('\x00')
            self.endid[i]["value"] = value
            self.tag(full_end_dest, value)
            logger.debug(f"Initial Endpoints {end_name} / {end_dest}:{value} ")
    



        ########## Main co-simulation loop ####################################
        # As long as granted time is in the time range to be simulated...
        while True:
            self.print_state()
            time.sleep(1)
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
                                self._tag(full_end_dest, value)
                            # Skip if value is unchanged
                            elif value == self.tag(full_end_dest):
                                continue

                        self.end_clients[end_dest] = TestClient(end_dest)
                        if self.logic[full_end_dest] is not None:
                            self.end_clients[end_dest].write_analog_point(end_dest_tag, self.tag(full_end_dest))
                        else:
                            self.end_clients[end_dest].write_analog_point(end_dest_tag, self.tag(full_end_name))
                        time.sleep(0.5)
                        reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
                        #Try to help thread craziness
                        time.sleep(1)
                        self.end_clients[end_dest].close()
                        self.end_clients[end_dest] = None
                        value = reply[1].rstrip('\x00')
                        self.tag(full_end_dest, value)
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
                                self._tag(full_end_dest, value)
                            # Skip if value is unchanged
                            elif value == self.tag(full_end_dest):
                                continue

                        self.end_clients[end_dest] = TestClient(end_dest)
                        if self.logic[full_end_dest] is not None:
                            self.end_clients[end_dest].write_digital_point(end_dest_tag, self.tag(full_end_dest))
                        else:
                            self.end_clients[end_dest].write_digital_point(end_dest_tag, self.tag(full_end_name))
                        time.sleep(0.5)
                        reply = self.end_clients[end_dest].send("READ="+end_dest_tag)
                        #Try to help thread craziness
                        time.sleep(1)
                        self.end_clients[end_dest].close()
                        self.end_clients[end_dest] = None
                        value = reply[1].rstrip('\x00')
                        self.tag(full_end_dest, value)



    def print_state(self):
        logger.debug("=================== DATA ===================")
        for tag in self.tags:
            logger.debug(f"{tag:<30} --- {self.tag(tag):}")
        logger.debug("============================================")

    def get_type(self, tag):
        return self.types[tag]

    def _tag(self, tag, val):
        if self.get_type(tag) == 'bool':
            # Make sure any val is 'true'|'false'
            val = bool(strtobool(str(val)))
            val = str(val).lower()
        else:
            val = float(val)
        return self.tag(tag, val)

    def tag(self, tag, val=None):
        raise NotImplementedError("Subclass and implement this method")

    def _client_handler(reply):
        split = reply.split(',')
        print("Reply:")
        if len(split) > 1:
            for tag in split:
                print("\t", tag)
        else:
            print("\t", split[0])
        return split[0]

    def _subscription_handler(self, message):
        """Receive Subscription message
        This method gets called by the Subscriber base class in its run()
        method when a message from a publisher is received.

        Ex message: "load-1_bus-101.mw:999.000,load-1_bus-101.active:true,"

        Args:
            message (str): published zmq message as a string
        """
        points = message.split(',')
        points = points[:-1] # remove last element since it might be empty
        #print(message)
        sub_source = threading.current_thread().name
        
        for point in points:
            if point == "":
                continue
            '''
            split = point.split('/')
            tag_source = split[0]
            tag = split[1].split(':')[0]
            value = split[1].split(':')[0]
            '''

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
                    with self.__lock:
                        self.tag(full_tag, value)
                        #update
                        print("UPDATE NOW: ",full_tag)
                        print("New value: ",value)
                else:
                    continue
            elif field == 'status':
                self.tag(full_tag, value)
                #update
                print("UPDATE NOW: ",full_tag)
                print("New value: ",value)
            else:
                continue
            
                


    def ctrl_exit_handler(self, signal, frame):
        print("SIGINT or CTRL-C detected. Exiting gracefully")
        sys.exit()
