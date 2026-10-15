"""Generic test pybennu subscriber"""

import sys

import pybennu.distributed.swig._Endpoint as E
from pybennu.distributed.subscriber import Subscriber
from pybennu.log import setup_logging


class TestSubscriber(Subscriber):
    def __init__(self, endpoint):
        Subscriber.__init__(self, endpoint)
        self.subscription_handler = self.print_update

    def print_update(self, message):
        print(f"Received subscription update: {message}")


def main():
    setup_logging("pybennu-test-subscriber", file_by_default=False)
    endpoint = E.new_Endpoint()
    E.Endpoint_str_set(endpoint, "udp://239.0.0.1:40000")
    sub = TestSubscriber(endpoint)
    sub.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit()
