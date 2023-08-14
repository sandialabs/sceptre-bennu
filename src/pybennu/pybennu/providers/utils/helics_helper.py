"""HELICS helper class
"""
import json
import logging
import signal
import sys

from distutils.util import strtobool
from py_expression_eval import Parser

import helics as h

logger = logging.getLogger('helics')
logger.setLevel(logging.INFO)
logger.info(f"Initializing helics version: {h.helicsGetVersion()}")


class HelicsFederate(object):
    def __init__(self, config, exit_handler=None):
        exit = exit_handler if exit_handler else self.ctrl_exit_handler
        signal.signal(signal.SIGINT, exit) # ctlr + c
        # Tag=>type map
        self.types = {}
        # Tag=>logic map
        self.logic = {}
        # Expression parser for logic
        self.parser = Parser()
        # Set of all tags (for printing)
        self.tags = set()

        # Default max simulation time is 1 week (in seconds)
        hours = 24*7 # one week
        total_interval = int(60 * 60 * hours)

        # Get federate values at runtime
        with open(config, 'r') as file_:
            cfg = json.loads(file_.read())
            end_time = cfg.get('end_time', None)
            self.end_time = end_time if end_time else total_interval
            req_time = cfg.get('request_time', None)
            self.request_time = 1
            if req_time:
                self.request_time = (h.HELICS_TIME_MAXTIME
                                     if req_time == 'max'
                                     else int(req_time))

        ##############  Registering  federate from json  ######################
        self.fed = h.helicsCreateCombinationFederateFromConfig(config)
        federate_name = h.helicsFederateGetName(self.fed)
        logger.info(f"Created federate {federate_name}")
        self.end_count = h.helicsFederateGetEndpointCount(self.fed)
        logger.info(f"\tNumber of endpoints: {self.end_count}")
        self.sub_count = h.helicsFederateGetInputCount(self.fed)
        logger.info(f"\tNumber of subscriptions: {self.sub_count}")
        self.pub_count = h.helicsFederateGetPublicationCount(self.fed)
        logger.info(f"\tNumber of publications: {self.pub_count}")

        # Diagnostics to confirm JSON config correctly added the required
        #   endpoints, publications, and subscriptions
        self.endid = {}
        for i in range(0, self.end_count):
            self.endid[i] = h.helicsFederateGetEndpointByIndex(self.fed, i)
            end_name = self.endid[i].name
            end_type = self.endid[i].type
            logger.debug(f"\tRegistered endpoint ---> {end_name}")
            end_name = end_name.split('/')[1] if '/' in end_name else end_name
            self.tags.add(end_name)
            self.types[end_name] = end_type
        self.subid = {}
        for i in range(0, self.sub_count):
            self.subid[i] = h.helicsFederateGetInputByIndex(self.fed, i)
            sub_name = self.subid[i].target
            sub_type = self.subid[i].type
            sub_info = self.subid[i].info # stores logic for interdependencies
            logger.debug(f"\tRegistered subscription ---> {sub_name}")
            sub_name = sub_name.split('/')[1] if '/' in sub_name else sub_name
            self.tags.add(sub_name)
            self.types[sub_name] = sub_type
            if sub_info:
                logger.debug(f"\t\t********** LOGIC **********")
                for exp in sub_info.split(';'):
                    lhs, rhs = exp.split('=')
                    self.logic[lhs.strip()] = rhs.strip()
                    logger.debug(f'\t\t{exp.strip()}')
        self.pubid = {}
        for i in range(0, self.pub_count):
            self.pubid[i] = h.helicsFederateGetPublicationByIndex(self.fed, i)
            pub_name = self.pubid[i].name
            pub_type = self.pubid[i].type
            logger.debug(f"\tRegistered publication ---> {pub_name}")
            pub_name = pub_name.split('/')[1] if '/' in pub_name else pub_name
            self.tags.add(pub_name)
            self.types[pub_name] = pub_type

    def run(self):
        ##############  Entering Execution Mode  ##############################
        h.helicsFederateEnterExecutingMode(self.fed)
        logger.info("Entered HELICS execution mode")

        grantedtime = 0

        # Blocking call for a time request at simulation time 0
        time = h.HELICS_TIME_ZERO
        logger.debug(f"Requesting initial time {time}")
        grantedtime = h.helicsFederateRequestTime(self.fed, time)
        logger.debug(f"Granted time {grantedtime}")

        # Publish initial values to helics
        for i in range(self.pub_count):
            pub_name = self.pubid[i].name
            pub_name = (pub_name.split('/')[1]
                        if '/' in pub_name
                        else pub_name)
            value = self.tag(pub_name)
            self.pubid[i].publish(value)
            logger.debug(f"\tPublishing {pub_name}:{value} "
                         f"at time {grantedtime}")

        self.action_post_request_time()

        ########## Main co-simulation loop ####################################
        # As long as granted time is in the time range to be simulated...
        while grantedtime < self.end_time:
            self.print_state()

            # Time request for the next physical interval to be simulated
            requested_time = grantedtime + self.request_time
            requested_time = (self.request_time
                              if self.request_time == h.HELICS_TIME_MAXTIME
                              else requested_time)
            logger.debug(f"Requesting time {requested_time}")
            grantedtime = h.helicsFederateRequestTime(self.fed, requested_time)
            logger.debug(f"Granted time {grantedtime}")

            # Process subscriptions
            for i in range(self.sub_count):
                if h.helicsInputIsUpdated(self.subid[i]):
                    sub_name = self.subid[i].target
                    sub_name = (sub_name.split('/')[1]
                                if '/' in sub_name
                                else sub_name)
                    value = self.subid[i].string
                    self._tag(sub_name, value)
                    logger.debug(f"\tUpdated {sub_name}:{value} "
                                 f"at time {grantedtime}")

            # Process logic
            for tag, exp in self.logic.items():
                expr = self.parser.parse(exp)
                # Assign variables
                vars = {}
                for var in expr.variables():
                    vars[var] = self.tag(var)
                # Evaluate expression
                value = expr.evaluate(vars)
                value = str(value).lower()
                if value != self.tag(tag):
                    logger.debug(f"\tLOGIC: {tag.strip()}={exp} ----> {value}")
                    # Assign new tag value
                    self._tag(tag.strip(), value)

            # Process endpoints
            for i in range(self.end_count):
                while self.endid[i].has_message():
                    end_name = self.endid[i].name
                    end_name = (end_name.split('/')[1]
                                if '/' in end_name
                                else end_name)
                    msg = self.endid[i].get_message()
                    value = msg.data
                    logger.debug(f"\tReceived message from endpoint "
                                 f"{msg.source} at time {grantedtime}"
                                 f" with data {value}")
                    self._tag(end_name, value)
                    logger.debug(f"\tUpdated {end_name}:{value} "
                                 f"at time {grantedtime}")

            # Process publications
            for i in range(self.pub_count):
                pub_name = self.pubid[i].name
                pub_name = (pub_name.split('/')[1]
                            if '/' in pub_name
                            else pub_name)
                value = self.tag(pub_name)
                self.pubid[i].publish(value)
                logger.debug(f"\tPublishing {pub_name}:{value} "
                             f"at time {grantedtime}")

            self.action_post_request_time()

        # Clean up HELICS
        self.destroy_federate()

    def print_state(self):
        logger.debug("=================== DATA ===================")
        for tag in self.tags:
            logger.debug(f"{tag:<30} --- {self.tag(tag):}")
        logger.debug("============================================")

    def action_post_request_time(self):
        pass

    def send_msg_to_endpoint(self, name, value):
        endpoint = self.fed.get_endpoint_by_name(name)
        endpoint.send_data(value.encode(), endpoint.default_destination)

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

    def destroy_federate(self):
        h.helicsFederateFinalize(self.fed)
        h.helicsFederateFree(self.fed)
        h.helicsCloseLibrary()
        sys.exit()

    def ctrl_exit_handler(self, signal, frame):
        print("SIGINT or CTRL-C detected. Exiting gracefully")
        self.destroy_federate()
