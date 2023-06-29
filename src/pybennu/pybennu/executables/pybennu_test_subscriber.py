"""Generic test pybennu subscriber
"""

import sys

from pybennu.distributed.subscriber import Subscriber
import pybennu.distributed.swig._Endpoint as E


class TestSubscriber(Subscriber):
    def __init__(self, endpoint):
        Subscriber.__init__(self, endpoint)


def main():
    endpoint = E.new_Endpoint()
    E.Endpoint_str_set(endpoint, 'udp://239.0.0.1:40000')
    sub = TestSubscriber(endpoint)
    sub.run()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit()

