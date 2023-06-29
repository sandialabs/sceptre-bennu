#!/usr/bin/python3
import sys
from time import time, sleep
from statistics import mean
from random import random, randrange
from labjack_t7 import LabJackT7
import argparse

class LJ_Tester(object):
    def __init__(self, addr, isDebug, isDigital, count):
        """Initialize test.
        """
        self.lj = LabJackT7('configs/labjack_t7.json', isDebug)
        self.isDigital = isDigital
        self.count = count
        self.addr = addr
        self.errors = []
        self.delays = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Cleans up LJ interface on exit.
        """
        self.lj.cleanup()

    def error(self, tx, rx):
        """
        Returns absolute error as percentage of range.
        tx (int): value sent
        rx (int): value received
        rtn (float): abs((rx-tx)/58)
        """
        return abs((rx-tx)/60)

    def LJ_TX_RX(self, addr_i, addr_o, tx):
        low = high = tx
        if not self.isDigital:
            low = tx-.06*5
            high = tx+.06*5
        
        if self.isDigital:
            self.lj.write_digital(addr_o,tx)
        else:
            self.lj.write_analog(addr_o,tx)
        
        count = 0
        while True:
            if self.isDigital:
                rx = self.lj.read_digital(addr_i)
            else:
                rx = self.lj.read_analog(addr_i)
            
            count += 1
            if rx is None:
                continue
            if rx <= high and rx >= low:
                break
        return rx

    def testDigital(self):
        tx = False

        for i in range(self.count):
            print("Test Iteration #{}".format(i))
            tx = not tx
            t1 = time()
            for i in self.addr:
                rx = self.LJ_TX_RX(i[0], i[1],tx)
            t2 = time()
            self.delays.append(t2-t1)
        print("Mean Delay: {}    Max Delay: {}    Min Delay: {}".format(mean(self.delays), max(self.delays), min(self.delays)))
    
    def testAnalog(self):
        for i in range(self.count):
            print("Test Iteration #{}".format(i))
            tx = 58*random()
            t1 = time()
            for i in self.addr:
                rx = self.LJ_TX_RX(i[0], i[1],tx)
                self.errors.append(self.error(tx, rx))
            t2 = time()
            self.delays.append(t2-t1)
        print("Mean Delay: {}    Max Delay: {}    Min Delay: {}    Mean Error: {}    Max Error: {}    Min Error {}".format(mean(self.delays), max(self.delays), min(self.delays), mean(self.errors), max(self.errors), min(self.errors)))

    def run(self):
        print("Testing {}".format(self.addr))
        if self.isDigital:
            self.testDigital()
        else:
            self.testAnalog()
        '''
        tx = False
#        for i in self.addr:
#            self.lj.write_digital(i[1],tx)
#             self.lj.write_analog(i[1],tx)
        for i in range(self.count):
            print("Test Iteration #{}".format(i))
#            tx = 58*random()
            tx = not tx
            t1 = time()
            #print("Test:", i, tx, t1)
            for i in self.addr:
#                 self.lj.write_digital(i[1],tx)
#                 self.lj.write_analog(i[1],tx)
                rx = self.LJ_TX_RX(i[0], i[1],tx)
                self.errors.append(self.error(tx, rx))
            t2 = time()
            self.delays.append(t2-t1)
        print("Mean Delay: {}    Max Delay: {}    Min Delay: {}    Mean Error: {}    Max Error: {}    Min Error {}".format(mean(self.delays), max(self.delays), min(self.delays), mean(self.errors), max(self.errors), min(self.errors)))
        '''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help='print LabJack debugging information')
    parser.add_argument('--digital', action='store_true', help='Perform a test on the digital pins')
    parser.add_argument('--analog', action='store_true', help='Perform a test on the analog pins')
    parser.add_argument('-c', '--count', help='Iteration Count')

    args = parser.parse_args()

    if args.digital and args.analog:
        print("Can only test digital or analog, not both at the same time")
        return
    
    count = 100
    if args.count and args.count > 0:
        count = args.count
    
    isDigital = True
    addr = [("digital_input_1", "digital_output_1"),
      ("digital_input_2", "digital_output_2"),
      ("digital_input_3", "digital_output_3"),
      ("digital_input_4", "digital_output_4"),
      ("digital_input_5", "digital_output_5"),
      ("digital_input_6", "digital_output_6"),
      ("digital_input_7", "digital_output_7"),
      ("digital_input_8", "digital_output_8")
      ]
    if args.analog:
        isDigital = False
        addr = [("analog_input_1", "analog_output_1")]
#    addr = [("digital_input_1", "digital_output_1")]

    with LJ_Tester(addr, args.debug, isDigital, count) as t:
        t.run()

if __name__ == "__main__":
    main()

