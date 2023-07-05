""" A monitor that ingests messages published by a provider into elastic

This script will run on an ELK box as a daemon. It subscribes to a provider
and connects to elastic. It expects the data received to be strings in the
format "<tag>:<value,<tag>:<value>, ...". It converts these messages into
a dictionary and then pushes to elastic.

Examples:
    generate help string:
    $ groundtruth_monitor -h

    start as daemon, connecting to provider/elastic as specified by config.ini
    $ groundtruth_monitor  -d -c config.ini start

    stop daemon
    $ groundtruth_monitor -d stop

"""

import os
import json
import argparse
import signal
import sys
import threading
from datetime import datetime
from configparser import ConfigParser, NoOptionError

from elasticsearch import Elasticsearch
from elasticsearch import helpers

from pybennu.distributed.subscriber import Subscriber
from pybennu.providers.utils.daemon import Daemon
import pybennu.distributed.swig._Endpoint as E


class GroundTruthMonitor(Daemon, Subscriber):
    """ Class to ingest messages from a provider into elasticsearch

    Subscribes to a provider via Subscriber and ingests messages
    into elasticsearch. The index is "ground_truth" and the doc type depends on the
    name of the tag appearing in the received message. These will correspond to the
    type of points available for systems. See self.__type() for a list of types.
    """
    def __init__(self, args):
        """ Initialize ES, Subscriber and Daemon

        Reads config.ini file for publish and Elasticsearch endpoints.
        Defaults: publish=udp://239.0.0.1:40000, Elasticsearch=127.0.0.1

        Args:
            args (argparse.Namespace): object that has command line args as members
        """
        if args.operation in ('start', 'restart'): # only care about the config file when starting
            if args.config_file:
                config_file = args.config_file
            else:
                config_file = '/etc/sceptre/%s.ini' % (args.env)

            if os.path.exists(config_file):
                config_file = os.path.abspath(config_file)
            else:
                print('\nThe file %s does not exist - please provide a valid config file.\n'
                      % (config_file))
                sys.exit(-1)

        config = ConfigParser()
        config.read(config_file)

        self.publish_endpoint = E.new_Endpoint()
        E.Endpoint_str_set(self.publish_endpoint, 'udp://239.0.0.1:40000')
        elastic_ip = '127.0.0.1'
        self._filter = '/'
    
        if 'power-groundtruth-monitor' in config.sections():
            try:
                E.Endpoint_str_set(self.publish_endpoint, config.get('power-groundtruth-monitor',
                                                                     'publish-endpoint').strip())
            except NoOptionError:
                print("WARNING: publish-endpoint not found in the configuration file."
                      " Using default: {}".format(E.Endpoint_str_get(self.publish_endpoint)))
            try:
                elastic_ip = config.get('power-groundtruth-monitor', 'elasticsearch-ip').strip()
            except NoOptionError:
                print("WARNING: elasticsearch-ip not found in the configuration file."
                      " Using default: {}".format(elastic_ip))
            try: 
                self._filter = config.get('power-groundtruth-monitor', 'filter').strip()
            except NoOptionError:
                print("WARNING: filter not found in the configuration file."
                      " Using default: {}".format(self._filter))

        else:
            try:
                E.Endpoint_str_set(self.publish_endpoint, config.get('groundtruth-monitor',
                                                                     'publish-endpoint').strip())
            except NoOptionError:
                print("WARNING: publish-endpoint not found in the configuration file."
                      " Using default: {}".format(E.Endpoint_str_get(self.publish_endpoint)))
            try:
                elastic_ip = config.get('groundtruth-monitor', 'elasticsearch-ip').strip()
            except NoOptionError:
                print("WARNING: elasticsearch-ip not found in the configuration file."
                      " Using default: {}".format(elastic_ip))
            try: 
                self._filter = config.get('groundtruth-monitor', 'filter').strip()
            except NoOptionError:
                print("WARNING: filter not found in the configuration file."
                      " Using default: {}".format(self._filter))

        self.__es = Elasticsearch([{'host': elastic_ip, 'port':'9200'}])
        Subscriber.__init__(self, self.publish_endpoint)
        self.time_last_send = datetime.now()
        Daemon.__init__(self, '/var/run/bennu-ground-truth-mon-%s.pid' % (args.env))

        #setup thread to deal with pushes to elastic
        self.__lock = threading.Lock()
        self.new_data = [] #used to store messages until it gets pushed to ES
        self.__publish_thread = threading.Thread(target=self.push_updates_to_elastic)
        self.__publish_thread.daemon = True
        self.__publish_thread.start()

    def _run(self):
        """ Connect and listen to Provider and handle term signal

        This method gets called by the Daemon base class on start(). It calls the run()
        method of the Subscriber base class to start listening on the socket.
        """
        def handle_exit(signum, stack):
            """ Disconnect from provider when term signal received """
            print("\nStopping ground truth monitor...\n")
            self.running = False
            self.disconnect()

        signal.signal(signal.SIGINT, handle_exit)
        signal.signal(signal.SIGTERM, handle_exit)

        Subscriber.run(self)

    def _subscription_handler(self, message):
        """ Receive Subscription message and ingest into Elasticsearch

        This method gets called by the Subscriber base class in its run()
        method when a message from a publisher is received.

        Ex message: "load-1_bus-101.mw:999.000,load-1_bus-101.active:true,"

        Args:
            message (str): published zmq message as a string
        """
        now = datetime.now()
        time_diff = now - self.time_last_send
        self.__parse_filter()

        points = message.split(',')

        for point in points:
            if len(point)>1: #end of message may be empty
                message_dict = {}
                split = point.split(':')
                tag = split[0]
                value = split[1]
                if 'alert' in tag or 'batch' in tag:
                    if time_diff.seconds >= 30:
                        self.time_last_send = now
                    else:
                        return

                message_dict['@timestamp'] = datetime.now()
                message_dict['provider'] = E.Endpoint_str_get(self.publish_endpoint)
                tags = tag.split('.')
                if len(tags) == 2:
                    message_dict['device'] = tags[0]
                    message_dict['field'] = tags[1]
                else:
                    message_dict['device'] = tags[0]
                    message_dict['field'] = ''
                try:
                    message_dict['value'] = float(value)
                except:
                    message_dict['status'] = value

                #check that message is in filter
                if self.__filter_message(message_dict):
                    with self.__lock:
                        self.new_data.append(message_dict)

    def __parse_filter(self):
        filters = self._filter.split(' ')
        self.providers = []
        self.devices = []
        self.fields = []

        for f in filters:
            parsed = f.split('/')
            try:
                self.fields.append(parsed[3])
                self.devices.append(parsed[2])
                self.providers.append(parsed[1])
            except:
                self.fields.append('')
                self.devices.append('')
                self.providers.append('')
            
    def __filter_message(self, message_dict):
        in_filter = False
        for i in range(len(self.providers)):
            if (self.providers[i] in message_dict['provider']) & (self.devices[i] in message_dict['device']) & (self.fields[i] in message_dict['field']):
                in_filter = True
        return in_filter

    def push_updates_to_elastic(self):
        while True:
            if self.new_data:
                with self.__lock:
                    messages = list(self.new_data)
                    self.new_data = []
                index = 'bennu-'  + datetime.now().strftime('%Y.%m.%d')
                actions = [{'_index': index, 'type': 'doc', '_source': message} for message in messages] 
                try:
                    res = helpers.bulk(self.__es, actions, request_timeout=30)
                except:
                    print('something wrong with bulk push to es')

    def __generate_id(self, device):
        """ Generate an elasticsearch doc ID

        The generated doc ID is the device name concatenated with the datetime

        Args:
            tag (str): The tag name for which to generate an elastic ID

        Returns:
            A doc ID string (e.g. bus-1~20170202_120153)
        """
        return device + '~' + datetime.now().strftime("%Y%m%d_%H%M%S")


def main():
    """ Parse args and start GroundTruthMonitor as a daemon """

    parser = argparse.ArgumentParser(description="SCEPTRE Ground Truth Monitor")
    parser.add_argument('-c', '--config-file', dest='config_file',
                        help='Config file to load (.ini)')
    parser.add_argument('-d', '--daemonize', dest='daemonize',
                        action='store_true',
                        help='Run as a daemon')
    parser.add_argument('-e', '--env', dest='env',
                        default='bennu',
                        help='Environment for daemon to run in')
    parser.add_argument('operation', metavar='OPERATION',
                        type=str, choices=['start', 'stop', 'restart', 'status'],
                        help='Daemon operation - accepts any of these values: start, stop,\
                        restart, status')

    args = parser.parse_args()

    handle = GroundTruthMonitor(args)
    handle.handler = handle._subscription_handler
    if args.operation == 'start':
        if args.daemonize:
            handle.start()
            pid = handle.get_pid()
            if not pid:
                print('Unable to start as a daemon')
        else:
            handle._run()
    elif args.operation == 'stop':
        handle.stop()
    elif args.operation == 'restart':
        print('Restarting daemon')
        pid = handle.get_pid()
        if not pid:
            print('Daemon is not already running - cannot restart')
        else:
            handle.restart()
    elif args.operation == 'status':
        print('Viewing daemon status')
        pid = handle.get_pid()
        if not pid:
            print('Daemon is not running')
        else:
            print('Daemon is running as PID %s' % (pid))
    sys.exit(0)


if __name__ == '__main__':
    main()
