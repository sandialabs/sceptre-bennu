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


"""
This fork of pyPMU has been substantially modified from the original pyPMU source.
These changes include this file (pdc.py), frame.py, splitter.py, and possibly a few others.

Notable changes (cegoes, 10/04/2024):
- Fixes/workarounds for quirks with the RTDS dynamic simulator's PMU functionality
- Fix bugs we've run into over the years. For example, quit() wasn't setting socket to none, and it'll try to close the socket even if it's None already.
- Type annotations and code modernization (e.g. f-strings)
- Code formatting and cleanup
- Various fixes from outstanding Pull Requests on the pyPMU GitHub
- UDP support, based on work done by Yuri Poledna in his "udp-support-v0.1" branch on the pyPMU GitLab repository.

At some point when I have time I'd like to merge these changes upstream. This is a fork of convenience.
"""

import logging
import socket
import struct
from typing import Literal, Optional
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


    def __init__(self, pdc_id=1, pmu_ip="127.0.0.1", pmu_port=4712, buffer_size=2048, method: Literal["tcp", "udp", "multicast"]="tcp", udp_bind_ip=""):

        self.pdc_id: int = pdc_id
        self.buffer_size: int = buffer_size
        self.method: Literal["tcp", "udp", "multicast"] = method

        self.pmu_ip: str = pmu_ip
        self.pmu_port: int = pmu_port
        self.pmu_address: tuple = (pmu_ip, pmu_port)
        self.pmu_socket: Optional[socket.socket] = None
        self.pmu_cfg1: Optional[ConfigFrame1] = None
        self.pmu_cfg2: Optional[ConfigFrame2] = None

        # for UDP and multicast, this is the local system
        self.bind_ip: str = udp_bind_ip


    def run(self) -> None:
        """
        Setup socket and connect to PMU (TCP) or bind to local socket (UDP).
        """

        if self.pmu_socket:
            self.logger.warning(
                "[%d] - PDC already connected to PMU (%s:%d)",
                self.pdc_id, self.pmu_ip, self.pmu_port
            )
            return

        try:
            # Connect to PMU
            if self.method == "tcp":
                self.pmu_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.pmu_socket.connect(self.pmu_address)
                self.logger.info(
                    "[%d] - PDC successfully connected to PMU (%s:%d) as TCP",
                    self.pdc_id, self.pmu_ip, self.pmu_port
                )
            elif self.method in ["udp", "multicast"]:
                self.pmu_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.pmu_socket.bind((self.bind_ip, self.pmu_port))

                if self.method == "multicast":
                    self.pmu_socket.setsockopt(
                        socket.IPPROTO_IP,
                        socket.IP_ADD_MEMBERSHIP,
                        struct.pack('4sL', socket.inet_aton(self.pmu_ip), socket.INADDR_ANY)
                    )
                    self.logger.info(
                        "[%d] - PDC UDP MULTICAST socket listener setup on %s:%d to %s",
                        self.pdc_id, self.bind_ip, self.pmu_port, self.pmu_ip
                    )
                else:
                    self.logger.info(
                        "[%d] - PDC UDP socket listener opened on %s:%d)",
                        self.pdc_id, self.bind_ip, self.pmu_port,
                    )
            else:
                raise ValueError(f"Invalid method '{self.method}'")
        except Exception as e:
            self.logger.error(
                "[%d] - Error setting up PMU socket (%s:%d) [udp_bind_ip: %s]: %s",
                self.pdc_id, self.pmu_ip, self.pmu_port, self.bind_ip, str(e)
            )
            raise e from None


    def start(self) -> None:
        """
        Request from PMU to start sending data
        :return: NoneType
        """
        self.logger.info(
            "[%d] - Requesting to start sending from PMU (%s:%d)",
            self.pdc_id, self.pmu_ip, self.pmu_port
        )

        start = CommandFrame(self.pdc_id, "start")
        start_bytes = start.convert2bytes()

        if(self.method == "tcp"):
            self.pmu_socket.sendall(start_bytes)
        else:
            self.pmu_socket.sendto(start_bytes, (self.pmu_ip, self.pmu_port))


    def stop(self) -> None:
        """
        Request from PMU to start sending data
        :return: NoneType
        """
        self.logger.info(
            "[%d] - Requesting to stop sending from PMU (%s:%d)",
            self.pdc_id, self.pmu_ip, self.pmu_port
        )

        stop = CommandFrame(self.pdc_id, "stop")
        stop_bytes = stop.convert2bytes()

        if self.method == "tcp":
            self.pmu_socket.sendall(stop_bytes)
        else:
            self.pmu_socket.sendto(stop_bytes, self.pmu_address)


    def get_header(self) -> HeaderFrame:
        """
        Request for PMU header message
        :return: HeaderFrame
        """
        self.logger.info(
            "[%d] - Requesting header message from PMU (%s:%d)",
            self.pdc_id, self.pmu_ip, self.pmu_port
        )

        get_header = CommandFrame(self.pdc_id, "header")
        get_header_bytes = get_header.convert2bytes()

        if self.method == "tcp":
            self.pmu_socket.sendall(get_header_bytes)
        else:
            self.pmu_socket.sendto(get_header_bytes, self.pmu_address)

        header = self.get()
        if isinstance(header, HeaderFrame):
            return header
        else:
            raise PdcError(f"Invalid Header message received: {header!r}")


    def get_config(
        self,
        version: Literal["cfg1", "cfg2", "cfg3"] = "cfg2",
    ) -> CommonFrame:
        """
        Request for Configuration frame.
        NOTE: ConfigFrame3 is not implemented yet by pypmu
        :param version: string Possible values "cfg1", "cfg2" and "cfg3"
        :return: ConfigFrame
        """
        get_config = CommandFrame(self.pdc_id, version)
        config_bytes = get_config.convert2bytes()

        # TODO: not sure if this will work with multicast
        if self.method == "tcp":
            self.pmu_socket.sendall(config_bytes)
        else:
            self.pmu_socket.sendto(config_bytes, self.pmu_address)

        config = self.get()
        if type(config) == ConfigFrame1:
            self.pmu_cfg1 = config
        elif type(config) == ConfigFrame2:
            self.pmu_cfg2 = config
        else:
            raise PdcError("Invalid Configuration message received")

        return config


    def get(self) -> Optional[CommonFrame]:
        """
        Decoding received messages from PMU
        :return: CommonFrame
        """

        received_data = b""
        received_message = None

        # NOTE: old code that I disabled for the RTDS work.
        # Keep receiving until SYNC + FRAMESIZE is received, 4 bytes in total.
        # Should get this in first iteration. FRAMESIZE is needed to determine when one complete message
        # has been received.
        # # while len(received_data) < 4:
        # #     received_data += self.pmu_socket.recv(self.buffer_size)

        if self.method == "tcp":
            received_data += self.pmu_socket.recv(4)
            bytes_received = len(received_data)
            total_frame_size = int.from_bytes(received_data[2:4], byteorder="big", signed=False)

            # Keep receiving until every byte of that message is received
            while bytes_received < total_frame_size:
                message_chunk = self.pmu_socket.recv(
                    min(total_frame_size - bytes_received, self.buffer_size)
                )

                if not message_chunk:
                    break

                received_data += message_chunk
                bytes_received += len(message_chunk)

        if self.method in ["udp", "multicast"]:
            received_data = self.pmu_socket.recv(1024)  # TODO: why 1024?
            total_frame_size = int.from_bytes(received_data[2:4], byteorder="big", signed=False)

        # If complete message is received try to decode it
        if len(received_data) == total_frame_size:
            try:
                # Try to decode received data
                received_message = CommonFrame.convert2frame(
                    received_data, self.pmu_cfg2
                )
                self.logger.debug(
                    "[%d] - Received %s from PMU (%s:%d)",
                    self.pdc_id, type(received_message).__name__, self.pmu_ip, self.pmu_port
                )
            except FrameError as ex:
                self.logger.warning(
                    "[%d] - Received unknown message from PMU (%s:%d), the FrameError exception is: %s",
                    self.pdc_id, self.pmu_ip, self.pmu_port, str(ex)
                )

        return received_message

    def quit(self) -> None:
        """
        Close connection to PMU
        :return: NoneType
        """
        if self.pmu_socket is None:
            return

        self.pmu_socket.close()
        self.logger.info(
            "[%d] - Connection to PMU closed (%s:%d)",
            self.pdc_id, self.pmu_ip, self.pmu_port
        )
        # Fix pypmu not closing right (cegoes, 06/14/2022)
        self.pmu_socket = None


class PdcError(BaseException):
    pass
