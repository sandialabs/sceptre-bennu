"""Electric Power test pybennu HELICS provider.
"""
import argparse
import logging
import sys
import threading

from pybennu.providers.utils.helics_helper import HelicsFederate

logger = logging.getLogger('helics')
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.INFO)


class ElectricPowerService(HelicsFederate):
    """ Test EP Service Class """

    def __init__(self, config, debug=False):
        self.__lock = threading.Lock()
        if(debug):
            logger.setLevel(logging.DEBUG)
        self.state = {}

        # Initialize HelicsFederate
        HelicsFederate.__init__(self, config)

        # Initialize power system state
        self.state['bus-1.active'] = True
        self.state['bus-1.voltage'] = 0.93
        self.state['bus-1.gen_mw'] = 10.2
        self.state['bus-1.number'] = 1

        self.state['bus-2.active'] = True
        self.state['bus-2.voltage'] = 1.45
        self.state['bus-2.gen_mw'] = 10.2
        self.state['bus-2.number'] = 2

        self.state['bus-3.active'] = True
        self.state['bus-3.voltage'] = 1.45
        self.state['bus-3.gen_mw'] = 10.2
        self.state['bus-3.number'] = 3

        self.state['branch-1-2_1.active'] = True
        self.state['branch-1-2_1.source'] = 1
        self.state['branch-1-2_1.target'] = 2
        self.state['branch-1-2_1.current'] = 20.0

        self.state['branch-1-3_1.active'] = True
        self.state['branch-1-3_1.source'] = 1
        self.state['branch-1-3_1.target'] = 3

        self.state['branch-3-2_1.active'] = True
        self.state['branch-3-2_1.source'] = 3
        self.state['branch-3-2_1.target'] = 2

        self.state['load-1_bus-1.active'] = True
        self.state['load-1_bus-1.mw'] = 4.55
        self.state['load-1_bus-1.mvar'] = 15.34
        self.state['load-1_bus-1.bid'] = 5.6

        self.state['load-1_bus-2.active'] = True
        self.state['load-1_bus-2.mw'] = 10.0
        self.state['load-1_bus-2.mvar'] = 1.22

        self.state['load-1_bus-3.active'] = True
        self.state['load-1_bus-3.mw'] = 10.0
        self.state['load-1_bus-3.mvar'] = 4.3

        self.state['generator-1_bus-1.active'] = True
        self.state['generator-1_bus-1.mw'] = 10.0

    # Read tag (value=None)
    # Write tag (value=<bool or double>)
    def tag(self, tag, value=None):
        with self.__lock:
            if value is not None:
                self.state[tag] = value
            else:
                if tag in self.state:
                    return self.state[tag]
                else:
                    return False if self.get_type(tag) == 'bool' else 0

def main():
    program = 'Electric power test work service (python) - HELICS'

    parser = argparse.ArgumentParser(description=program)
    parser.add_argument('-d', '--debug', action='store_true',
                        help='print debugging information')
    parser.add_argument('-c', '--config', required=True,
                        default='helics.json',
                        help='path to helics config file')

    args = parser.parse_args()
    ep = ElectricPowerService(args.config, args.debug)
    ep.run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        sys.exit()
