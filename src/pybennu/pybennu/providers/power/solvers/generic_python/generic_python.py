"""Generic python solver provider.

This module instantiates a Python-based solver for use in SCEPTRE
environments.  The Python class inherits from the Provider
class which abstracts away the communication interface code that all
providers require.
"""

import copy
import importlib
import os
import sys
import threading
import time

from pybennu.distributed.provider import Provider
from pybennu.providers.power.solvers.generic_python.base_simulation import (
    BaseSimulation,
)


class GenericPython(Provider):
    """A genearic python implementation of a discrete event simulation."""

    def __init__(
        self,
        server_endpoint: str,
        publish_endpoint: str,
        python_sim: str,
        debug: bool = False,
    ) -> None:
        Provider.__init__(self, server_endpoint, publish_endpoint)
        self.__lock = threading.Lock()

        self.debug = debug

        self.simulation = self.load_simulation(python_sim)
        self.valid_tag_format = True

        self.elements = {
            "inputs_to_fd": {"analog": {}, "binary": {}},
            "outputs_from_fd": {"analog": {}, "binary": {}},
        }

        self.create()

    def load_simulation(self, simulation_script: str) -> BaseSimulation:
        """Load the specific sumulation that you want to run. Must inherit from BaseSimulation"""
        spec = importlib.util.spec_from_file_location("simulation", simulation_script)
        simulation_dir = os.path.dirname(simulation_script)
        sys.path.append(simulation_dir)
        module = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(module)
        except Exeption as e:
            print(f"Failed to load module {simulation_dir}: {e}")

        if not hasattr(module, "Simulation"):
            print(f'The module {simulation_dir} does not have a "Simulation" class.')

        return module.Simulation()  # Assuming the simulation class is named Simulation

    def create(self) -> None:
        """Initialize and start the simulation thread
        Note outputs from the sim are input to the field device and inputs to the sim are outputs from the field device"""

        self.simulation_thread = threading.Thread(target=self.simulation.run)
        self.simulation_thread.start()
        self._check_sim_tag_format()
        self._update_elements()

    def query(self) -> str:
        """Implements the query command interface for receiving the
        available system tags when requested by field devices or probes.
        When a query command is received, all available points of the
        simulation will be collected and returned."""

        if self.debug:
            print("Processing query")
        tags = ""
        with self.__lock:
            # Iterate through both 'inputs_to_fd' and 'outputs_from_fd'
            for io in self.elements:
                # Iterate through both 'analog' and 'binary'
                for tag_type in self.elements[io]:
                    # Iterate through each tag
                    for tag in self.elements[io][tag_type]:
                        # Add tag to list of tags
                        tags += f"{tag},"
        msg = f"ACK={tags}"
        return msg

    def read(self, tag: str) -> str:
        """Implements the read command interface for receiving the Py
        model when commanded by field devices or probes. When a read command
        is received, the internal representation of the specific element of
        the system under test will be collected and returned."""

        if self.debug:
            print("Processing read request")
        try:
            with self.__lock:
                # Iterate through both 'inputs_to_fd' and 'outputs_from_fd'
                for io in list(self.elements.keys()):
                    # Iterate through both 'analog' and 'binary'
                    for tag_type in list(self.elements[io].keys()):
                        # If we found the tag, get the value
                        if tag in self.elements[io][tag_type]:
                            value = self.elements[io][tag_type][tag]
            # Return the tag and value pair
            reply = tag + ":" + str(value).lower()
            msg = f"ACK={reply}" if str(value) else "ERR=No data found."
        except Exception as err:
            msg = f"ERR={err}"
        return msg

    def write(self, tags: dict) -> str:
        """Implements the write command interface for updating the simulation
        when commanded by field devices or probes. When a write command
        is received, the internal representation of the system under test will
        be updated and published after the solve completes."""

        if self.debug:
            print("Processing write command")

        try:
            msg = ""
            for tag, value in tags.items():
                if self.debug:
                    print(
                        f"Python.write ---- received write command for: {tag} with value {value}  ->",
                        end="",
                    )
                val = None
                # Use only 'outputs_from_fd' since we can only write to simulation inputs
                # Iterate through both 'analog' and 'binary'
                for tag_type in self.elements["outputs_from_fd"]:
                    # If tag was found, make sure value is converted to appropriate type
                    if tag in self.elements["outputs_from_fd"][tag_type]:
                        if tag_type == "binary":
                            if value in ["True", "False"]:
                                val = True if value == "True" else False
                            elif value in ["true", "false"]:
                                val = True if value == "true" else False
                            elif value in ["on", "off"]:
                                val = True if value == "on" else False
                            elif value in ["yes", "no"]:
                                val = True if value == "yes" else False
                            elif value in ["1", "0"]:
                                val = True if value == "1" else False
                            else:
                                return f"ERR=Invalid value for tag: {tag}"
                        elif tag_type == "analog":
                            val = float(value)
                        break
                # If tag/val was not found, return error
                if val is None:
                    return f"ERR=Provider failed to find writable tag {tag}"
                # Otherwise, write the tag/val to inputs of the simulation
                with self.__lock:
                    self.elements["outputs_from_fd"][tag_type][tag] = val
                    try:
                        self.simulation.set_parameter(tag, "input", tag_type, val)
                    except:
                        # If tag wasn't found, it might be because tags in simulation were not formatted
                        tag = tag.split(".")[0]
                        self.simulation.set_parameter(tag, "input", tag_type, val)
            msg = "ACK=Success processing Python write command"
        except Exception as err:
            print(
                f"ERROR: Provider failed to process write message with exception: {err}"
            )
            msg = f"ERR=Provider failed to process write message with exception: {err} on line {sys.exc_info()[-1].tb_lineno}"

        return msg

    def periodic_publish(self) -> None:
        """Loop that calls `self.publish` every second."""

        while True:
            msg = self._pack_data()
            try:
                self.publish(msg)
            except Exception as e:
                print(f"provider periodic_publish error: {e}")
            time.sleep(1)

    def _pack_data(self) -> str:
        """Pack system data into a string"""
        self._update_elements()

        msg = ""
        # Iterate through both 'inputs_to_fd' and 'outputs_from_fd'
        for io in list(self.elements.keys()):
            # Iterate through tag type 'analog' and 'binary'
            for tag_type in self.elements[io]:
                # Iterate through each tag
                for tag in self.elements[io][tag_type]:
                    # Get the tag value
                    value = self.elements[io][tag_type][tag]
                    # Add tag:value to broadcast message
                    msg += f"{tag}:{str(value).lower()},"
        return msg

    def _update_elements(self) -> None:
        """Update the provider elements with the current state of the simulation"""
        with self.__lock:
            # Get current state from simulation
            sim_state = self.simulation.get_current_state()
            if not self.valid_tag_format:
                # Need to check tags from simulation to ensure they are in device.field format. If not, force them to be
                sim_state = self._convert_tags(sim_state)

            # Outputs from the simulation are inputs to the field devices
            self.elements["inputs_to_fd"]["analog"] = sim_state["output"]["analog"]
            self.elements["inputs_to_fd"]["binary"] = sim_state["output"]["binary"]
            # Inputs to the simulation are outputs from the field devices
            # Need to check tags from simulation to ensure they are in device.field format. If not, force them to be
            self.elements["outputs_from_fd"]["analog"] = sim_state["input"]["analog"]
            self.elements["outputs_from_fd"]["binary"] = sim_state["input"]["binary"]

    def _check_sim_tag_format(self) -> None:
        """Check that tags in the similation state are of right form."""
        sim_state = self.simulation.get_current_state()
        # Iterate through all the tags in sim_state
        for io in sim_state:
            for tag_type in sim_state[io]:
                for tag, value in sim_state[io][tag_type].items():
                    if not (len(tag.split(".")) == 2):
                        self.valid_tag_format = False

    def _convert_tags(self, sim_state: dict) -> dict:
        """Convert tags in the similation state to the right form."""
        # Iterate through all the tags in sim_state
        new_sim_state = copy.deepcopy(sim_state)
        for io in new_sim_state:
            for tag_type in new_sim_state[io]:
                new_tag_dict = {}
                for tag, value in new_sim_state[io][tag_type].items():
                    # Check if formatting is in device.field format. If not, add a field based on type
                    formatted_tag = self._format_tag(tag, tag_type)
                    # If tag format was updated then replace the tag:val pair in sim_state
                    if not formatted_tag == tag:
                        new_tag_dict[formatted_tag] = value
                    else:
                        new_tag_dict[tag] = value
                new_sim_state[io][tag_type] = new_tag_dict
        return new_sim_state

    def _format_tag(self, tag: str, tag_type: str) -> str:
        """Check/modify formatting of tag to conform to 'device.field' format."""
        parts = tag.split(".")
        tag_device = parts[0]
        if len(parts) < 2:
            if "analog" in tag_type:
                tag_field = "value"
            elif "binary" in tag_type:
                tag_field = "status"
        else:
            tag_field = parts[1]
        return f"{tag_device}.{tag_field}"
