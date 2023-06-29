Siren
======
With V4, Siren can be launched within an experiment instead of on the physical machine. 
Siren must be launched on the same compute where the AMS devices are connected. 
To launch it with the experiment, include the Siren VM inside the topology including the
ttyUSB files needed with an example shown below.
```bash
      "advanced": {
        "qemu-append": "-serial /dev/ttyUSB0 -serial /dev/ttyUSB1 -serial /dev/ttyUSB2 -serial /dev/ttyUSB3"
      }
```

A single Siren VM can have up to 4 AMS connections.
It may be possible to run multiple Siren VMs if more connections are needed, but this has not been tested.

If the qemu-append command includes a non-existent file, the Siren VM will not launch.

In case you want to still install Siren on a physical machine, look below for instructions.

## Installation (Ubuntu)
- Install Labjack Dependencies if using Labjack
```bash
pip3 install labjack labjack-ljm
wget https://labjack.com/sites/default/files/software/labjack_ljm_software_2019_07_16_x86_64.tar.gz
tar -xvf labjack_ljm_software_2019_07_16_x86_64.tar.gz
cd labjack_ljm_software_2019_07_16_x86_64
sudo ./labjack_ljm_installer.run
cd ../ && rm -rf labjack_ljm_software_2019_07_16_x86_64
```

- Configure
```bash
cd src/pybennu/
sudo make install-dev
```
If this doesn't work because of dependency issues, try the following command instead.
```bash
cd src/pybennu/
sudo pip3 install -e .
```

## Usage Notes
- If you want to test a labjack without the physical hardware, you can put siren in a labjack demo mode.
To do so, you need to go to `hil_device/labjack_t7.py` and change the line 
`self.handle = ljm.openS("T7", "ANY", id)` to 
`self.handle = ljm.openS("T7", "ANY", "LJM_DEMO_MODE")`
- In case running siren is putting a HIL device in a weird state, you might need to increase the sleep
in the `_to_provider` section of siren.py