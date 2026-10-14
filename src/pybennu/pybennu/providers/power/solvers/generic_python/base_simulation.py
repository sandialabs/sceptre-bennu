import time


class BaseSimulation:
    """Base class for simulations."""

    def __init__(self, dt=0.1):
        """Initialize the simulation."""

        # Define the structure for state. All derived classed must use this same structure.
        self.state = {
            "input": {"analog": {}, "binary": {}},
            "output": {"analog": {}, "binary": {}},
        }

        # Set the timestep for the discrete even simulation
        self.dt = dt  # Set the time step
        self.check_state()
        self.running = False

    def check_state(self):
        """Ensure values provided for state are the correct types"""
        for tag, value in self.state["input"]["analog"].items():
            if type(value) is not float:
                print(f"{tag} must be a float")

        for tag, value in self.state["input"]["binary"].items():
            if type(value) is not bool:
                print(f"{tag} must be a bool")

        for tag, value in self.state["output"]["analog"].items():
            if type(value) is not float:
                print(f"{tag} must be a float")

        for tag, value in self.state["output"]["binary"].items():
            if type(value) is not bool:
                print(f"{tag} must be a bool")

    def get_current_state(self):
        """Get the current state of the simulation"""
        return self.state

    def get_parameter(self, tag, io, tag_type):
        """Get the value of parameter with name tag, io either 'input' or 'output', and tag_type either 'analog' or 'binary'"""
        # Check if tag is in the sim state with the right types requested
        if tag in self.state[io][tag_type]:
            return self.state[io][tag_type][tag]
        raise KeyError(f"Error: Tag {tag} not found in {tag_type} {io}.")

    def set_parameter(self, tag, io, tag_type, value):
        """Set the value of parameter with name tag, io either 'input' or 'output', and tag_type either 'analog' or 'binary'"""
        # Convert value appropriately
        if tag_type == "analog":
            value = float(value)
        if tag_type == "binary":
            value = bool(value)

        # Check if the tag is the sim state with the right types requested
        if tag in self.state[io][tag_type]:
            self.state[io][tag_type][tag] = value
        # If not, throw error
        else:
            raise KeyError(f"Error: Tag {tag} not found in {tag_type} {io}")

    def run(self):
        """Run the simulation loop."""
        self.running = True
        t0 = time.time()
        while self.running:
            self.update_state()  # Call the method to update the state
            t1 = time.time()
            elapsed_time = t1 - t0
            remaining_time = self.dt - elapsed_time
            if remaining_time > 0:
                time.sleep(remaining_time)  # Simulate real-time updates
            else:
                print("Error: simulation running slower than real time.")
            t0 = time.time()  # Set the new initial time for the next loop

    def stop(self):
        """Stop the simulation."""
        self.running = False

    def update_state(self):
        """Update the state of the simulation. To be overridden by subclasses."""
        raise NotImplementedError("Subclasses must implement this method.")
