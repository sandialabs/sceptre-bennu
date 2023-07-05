#!/usr/bin/python3
import abc
import enum
import logging
import traceback

LEVELS = {'debug': logging.DEBUG,       # 10
          'info': logging.INFO,         # 20
          'warning': logging.WARNING,   # 30
          'filter': 31,
          'error': logging.ERROR,       # 40
          'critical': logging.CRITICAL  # 50
         }

class HIL_Device(abc.ABC):
    @abc.abstractmethod
    def __init__(self, device_config, device_name, level_name, device_id=''):
        raise NotImplementedError

    @abc.abstractmethod
    def cleanup(self):
        raise NotImplementedError

    def setup_logger(self, level_name):
        self.logger = logging.getLogger(self.device_name)
        level = LEVELS.get(level_name, logging.NOTSET)
        self.logger.setLevel(level)

    @abc.abstractmethod
    def config_to_analog(self, *args):
        raise NotImplementedError

    @abc.abstractmethod
    def config_to_digital(self, *args):
        raise NotImplementedError

    @abc.abstractmethod
    def read_analog(self, io_point):
        raise NotImplementedError

    @abc.abstractmethod
    def write_analog(self, io_point, volts):
        raise NotImplementedError

    @abc.abstractmethod
    def read_digital(self, io_point):
        raise NotImplementedError

    @abc.abstractmethod
    def write_digital(self, io_point, volts):
        raise NotImplementedError

    @abc.abstractmethod
    def write_waveform(self, io_point, freq):
        raise NotImplementedError

    @abc.abstractmethod
    def __del__(self):
        raise NotImplementedError

    def validate_label_mapping(self):
        """Ensures that all required values are present in a device's config."""
        for label, mapping in self.label_mapping.items():
            if 'analog' in label:
                keys = mapping.keys()
                if ("pin" not in keys or
                    "min_data" not in keys or "max_data" not in keys or
                    "min_volt" not in keys or "max_volt" not in keys):
                    raise NotImplementedError
            elif 'digital' in label or 'waveform' in label or 'flip_flop' in label:
                if not mapping:
                    raise NotImplementedError
            else:
                raise NotImplementedError

    def data_to_voltage(self, io_point, data):
        """Wrapper for linear_scale.

        Args:
            io_point: The IO point whose mins/maxes should be considered.
            data: The datum to be converted.

        Returns:
            A float that represents voltage.
        """
        r_min = self.label_mapping[io_point]["min_data"]
        r_max = self.label_mapping[io_point]["max_data"]
        t_min = self.label_mapping[io_point]["min_volt"]
        t_max = self.label_mapping[io_point]["max_volt"]
        return self.linear_scale(data, r_min, r_max, t_min, t_max)

    def voltage_to_data(self, io_point, voltage):
        """Wrapper for linear_scale.

        Args:
            io_point: The IO point whose mins/maxes should be considered.
            voltage: The voltage to be converted.

        Returns:
            A float that represents data.
        """
        r_min = self.label_mapping[io_point]["min_volt"]
        r_max = self.label_mapping[io_point]["max_volt"]
        t_min = self.label_mapping[io_point]["min_data"]
        t_max = self.label_mapping[io_point]["max_data"]
        return self.linear_scale(voltage, r_min, r_max, t_min, t_max)

    def linear_scale(self, m, r_min, r_max, t_min, t_max):
        '''Linearly scales m in [r_min, r_max] to a value in [t_min, t_max].

        For an explanation, see:
        https://stats.stackexchange.com/questions/281162/scale-a-number-between-a-range
        '''
        return min((m - r_min) / (r_max - r_min) * (t_max - t_min) + t_min, t_max)


class MsgType(enum.Enum):
    INFO = 1
    DEBUG = 2
    ERROR = 3
