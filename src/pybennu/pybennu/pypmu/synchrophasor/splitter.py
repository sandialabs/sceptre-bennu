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

from .frame import *
from .pdc import Pdc
from .pmu import Pmu


__author__ = "Stevan Sandi"
__copyright__ = "Copyright (c) 2016, Tomo Popovic, Stevan Sandi, Bozo Krstajic"
__credits__ = []
__license__ = "BSD-3"
__version__ = "1.0.0-aplha"


class StreamSplitter(object):

    def __init__(self, source_ip, source_port, listen_ip, listen_port, pdc_id=1, method="tcp", buffer_size=2048):

        self.pdc = Pdc(pdc_id, source_ip, source_port, buffer_size, method)
        self.pmu = Pmu(ip=listen_ip, port=listen_port, method=method, buffer_size=buffer_size, set_timestamp=False)

        self.source_cfg1 = None
        self.source_cfg2 = None
        self.source_cfg3 = None
        self.source_header = None


    def run(self):

        self.pdc.run()
        self.source_header = self.pdc.get_header()
        self.source_cfg2 = self.pdc.get_config()
        self.pdc.start()

        self.pmu.run()
        self.pmu.set_header(self.source_header)
        self.pmu.set_configuration(self.source_cfg2)

        while True:

            message = self.pdc.get()

            if self.pmu.clients and message:

                self.pmu.send(message)

                if isinstance(message, HeaderFrame):
                    self.pmu.set_header(message)
                elif isinstance(message, ConfigFrame2):
                    self.pmu.set_configuration(message)


class StreamSplitterError(BaseException):
    pass
