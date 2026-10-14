from pybennu.providers.power.solvers.generic_python.base_simulation import (
    BaseSimulation,
)


class Simulation(BaseSimulation):
    """Simulation class for demonstration purposes."""

    def __init__(self):
        dt = 1.0
        super().__init__(dt)

        # In the simulator, inputs are inputs to the simulation (outputs from a fd) and vice versa
        self.state = {
            "input": {
                "analog": {"temperature_setpoint.value": 100.0},
                "binary": {"on.status": True},
            },
            "output": {
                "analog": {"temperature.value": 25.0, "temperature_diff.value": 75.0},
                "binary": {"at_temp.status": False},
            },
        }
        self.check_state()

    def update_state(self):
        # Simulate changes in outputs based on inputs
        # if on, change heat to setpoint, otherwise, do nothing
        if self.state["input"]["binary"]["on.status"]:
            if self.state["output"]["analog"]["temperature_diff.value"] > 0:
                self.state["output"]["analog"]["temperature.value"] -= 1
            elif self.state["output"]["analog"]["temperature_diff.value"] < 0:
                self.state["output"]["analog"]["temperature.value"] += 1
        self.state["output"]["analog"]["temperature_diff.value"] = (
            self.state["output"]["analog"]["temperature.value"]
            - self.state["input"]["analog"]["temperature_setpoint.value"]
        )
        self.state["output"]["binary"]["at_temp.status"] = (
            self.state["output"]["analog"]["temperature_diff.value"] == 0
        )
