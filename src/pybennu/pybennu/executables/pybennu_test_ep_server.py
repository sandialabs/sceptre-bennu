"""Electric Power test pybennu provider.
"""

import argparse
import sys
import threading
import time

from pybennu.distributed.provider import Provider
import pybennu.distributed.swig._Endpoint as E


class ElectricPowerService(Provider):
    """ Test EP Service Class """

    def __init__(self, server_endpoint, publish_endpoint, debug):
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock = threading.Lock()
        self.__debug = debug
        self.mPS = {}

        self.mPS['bus-1.active'] = "true"
        self.mPS['bus-1.voltage'] = "0.93"
        self.mPS['bus-1.gen_mw'] = "10.2"
        self.mPS['bus-1.number'] = "1"

        self.mPS['bus-2.active'] = "true"
        self.mPS['bus-2.voltage'] = "1.45"
        self.mPS['bus-2.gen_mw'] = "10.2"
        self.mPS['bus-2.number'] = "2"

        self.mPS['bus-3.active'] = "true"
        self.mPS['bus-3.voltage'] = "1.45"
        self.mPS['bus-3.gen_mw'] = "10.2"
        self.mPS['bus-3.number'] = "3"

        self.mPS['branch-1-2_1.active'] = "true"
        self.mPS['branch-1-2_1.source'] = "1"
        self.mPS['branch-1-2_1.target'] = "2"
        self.mPS['branch-1-2_1.current'] = "20.0"

        self.mPS['branch-1-3_1.active'] = "true"
        self.mPS['branch-1-3_1.source'] = "1"
        self.mPS['branch-1-3_1.target'] = "3"

        self.mPS['branch-3-2_1.active'] = "true"
        self.mPS['branch-3-2_1.source'] = "3"
        self.mPS['branch-3-2_1.target'] = "2"

        self.mPS['load-1_bus-1.active'] = "true"
        self.mPS['load-1_bus-1.mw'] = "4.55"
        self.mPS['load-1_bus-1.mvar'] = "15.34"
        self.mPS['load-1_bus-1.bid'] = "5.6"

        self.mPS['load-1_bus-2.active'] = "true"
        self.mPS['load-1_bus-2.mw'] = "10.0"
        self.mPS['load-1_bus-2.mvar'] = "1.22"

        self.mPS['load-1_bus-3.active'] = "true"
        self.mPS['load-1_bus-3.mw'] = "10.0"
        self.mPS['load-1_bus-3.mvar'] = "4.3"

        self.mPS['generator-1_bus-1.active'] = "true"
        self.mPS['generator-1_bus-1.mw'] = "10.0"

    # Must return "ACK=tag1,tag2,..." or "ERR=<error message>"
    def query(self):
        if self.__debug:
            print("ElectricPowerService::query ---- received query request")
        result = "ACK="
        for k in self.mPS:
            result += k + ','
        return result

    # Must return "ACK=<value>" or "ERR=<error message>
    def read(self, tag):
        if self.__debug:
            print("ElectricPowerService::read ---- received read for tag %s" % tag)
        result = ""
        with self.__lock:
            if tag in self.mPS:
                result += "ACK="
                result += self.mPS[tag]
            else:
                result += "ERR=Tag not found"
            return result

    # Must return "ACK=<success message>" or "ERR=<error message>"
    def write(self, tag_dict):
        tag, value = list(tag_dict.items())[0]
        if self.__debug:
            print("ElectricPowerService::write ---- received write for tag %s -- %s" % (tag, value))
        result = ""
        with self.__lock:
            if tag in self.mPS:
                self.mPS[tag] = value
                result += "ACK=Updated tag " + tag + " -- " + value
            else:
                result += "ERR=Tag not found"
            return result

    def periodic_publish(self):
        while True:
            self.__publish_data()
            time.sleep(1)

    def __publish_data(self):
        with self.__lock:
            msg = ""
            for k, v in self.mPS.items():
                msg += k + ':' + v + ','
            self.publish(msg)


def main():
    program = 'Electric power test work service (python):'

    parser = argparse.ArgumentParser(description=program)
    parser.add_argument('-d', '--debug', action='store_true',
                        help='print debugging information')
    parser.add_argument('-s', '--server-endpoint', default='tcp://127.0.0.1:5555',
                        help='server listening endpoint')
    parser.add_argument('-p', '--publish-endpoint', default='udp://239.0.0.1:40000',
                        help='publishing endpoint')

    args = parser.parse_args()
    debug = args.debug
    server_endpoint = E.new_Endpoint()
    publish_endpoint = E.new_Endpoint()
    E.Endpoint_str_set(server_endpoint, args.server_endpoint)
    E.Endpoint_str_set(publish_endpoint, args.publish_endpoint)
    ep = ElectricPowerService(server_endpoint, publish_endpoint, args.debug)
    ep.run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        sys.exit()




