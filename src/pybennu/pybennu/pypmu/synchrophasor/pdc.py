# Copyright (c) 2016, Tomo Popovic, Stevan Sandi & Bozo Krstajic
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of pyPMU nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import socket
from sys import stdout
from .frame import *


__author__ = "Stevan Sandi"
__copyright__ = "Copyright (c) 2016, Tomo Popovic, Stevan Sandi, Bozo Krstajic"
__credits__ = []
__license__ = "BSD-3"
__version__ = "1.0.0-alpha"


class Pdc(object):

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    handler = logging.StreamHandler(stdout)
    formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
    handler.setFormatter(formatter)
    logger.addHandler(handler)


    def __init__(self, pdc_id=1, pmu_ip="127.0.0.1", pmu_port=4712, buffer_size=2048, method="tcp"):

        self.pdc_id = pdc_id
        self.buffer_size = buffer_size
        self.method = method

        self.pmu_ip = pmu_ip
        self.pmu_port = pmu_port
        self.pmu_address = (pmu_ip, pmu_port)
        self.pmu_socket = None
        self.pmu_cfg1 = None
        self.pmu_cfg2 = None
        self.pmu_header = None


    def run(self):

        if self.pmu_socket:
            self.logger.info("[%d] - PDC already connected to PMU (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)
        else:
            try:
                # Connect to PMU
                self.pmu_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.pmu_socket.connect(self.pmu_address)
                self.logger.info("[%d] - PDC successfully connected to PMU (%s:%d)",
                                 self.pdc_id, self.pmu_ip, self.pmu_port)
            except Exception as e:
                self.logger.error("[%d] - Error while connecting to (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)
                self.logger.error(str(e))


    def start(self):
        """
        Request from PMU to start sending data
        :return: NoneType
        """
        start = CommandFrame(self.pdc_id, "start")
        self.pmu_socket.sendall(start.convert2bytes())
        self.logger.info("[%d] - Requesting to start sending from PMU (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)


    def stop(self):
        """
        Request from PMU to start sending data
        :return: NoneType
        """
        start = CommandFrame(self.pdc_id, "stop")
        self.pmu_socket.sendall(start.convert2bytes())
        self.logger.info("[%d] - Requesting to stop sending from PMU (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)


    def get_header(self):
        """
        Request for PMU header message
        :return: HeaderFrame
        """
        get_header = CommandFrame(self.pdc_id, "header")
        self.pmu_socket.sendall(get_header.convert2bytes())

        self.logger.info("[%d] - Requesting header message from PMU (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)

        header = self.get()
        if isinstance(header, HeaderFrame):
            return header
        else:
            raise PdcError("Invalid Header message received")


    def get_config(self, version="cfg2"):
        """
        Request for Configuration frame.
        :param version: string Possible values "cfg1", "cfg2" and "cfg3"
        :return: ConfigFrame
        """
        get_config = CommandFrame(self.pdc_id, version)
        self.pmu_socket.sendall(get_config.convert2bytes())

        config = self.get()
        if type(config) == ConfigFrame1:
            self.pmu_cfg1 = config
        elif type(config) == ConfigFrame2:
            self.pmu_cfg2 = config
        else:
            raise PdcError("Invalid Configuration message received")

        return config


    def get(self):
        """
        Decoding received messages from PMU
        :return: CommonFrame
        """

        received_data = b""
        received_message = None

        """
        Keep receiving until SYNC + FRAMESIZE is received, 4 bytes in total.
        Should get this in first iteration. FRAMESIZE is needed to determine when one complete message
        has been received.
        """

        # while len(received_data) < 4:
        #     received_data += self.pmu_socket.recv(self.buffer_size)
        received_data += self.pmu_socket.recv(4)

        bytes_received = len(received_data)
        total_frame_size = int.from_bytes(received_data[2:4], byteorder="big", signed=False)

        # Keep receiving until every byte of that message is received
        while bytes_received < total_frame_size:
            message_chunk = self.pmu_socket.recv(min(total_frame_size - bytes_received, self.buffer_size))
            if not message_chunk:
                break
            received_data += message_chunk
            bytes_received += len(message_chunk)

        # If complete message is received try to decode it
        if len(received_data) == total_frame_size:
            try:
                received_message = CommonFrame.convert2frame(received_data, self.pmu_cfg2)  # Try to decode received data
                self.logger.debug("[%d] - Received %s from PMU (%s:%d)", self.pdc_id, type(received_message).__name__,
                                  self.pmu_ip, self.pmu_port)
            except FrameError as ex:
                self.logger.exception("[%d] - Failed to process message from PMU (%s:%d): %s",
                                      self.pdc_id, self.pmu_ip, self.pmu_port, str(ex))

        return received_message


    def quit(self):
        """
        Close connection to PMU
        :return: NoneType
        """
        self.pmu_socket.close()
        self.logger.info("[%d] - Connection to PMU closed (%s:%d)", self.pdc_id, self.pmu_ip, self.pmu_port)
        # Fix pypmu not closing right (cegoes, 06/14/2022)
        self.pmu_socket = None


class PdcError(BaseException):
    pass
