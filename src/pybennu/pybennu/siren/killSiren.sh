#!/bin/bash

# Made to kill siren when it's running in the background 
# without messing up connections to the hardware by sending sigint.
# We need to send the signal twice to fully close siren.

kill -INT $(ps -aux | grep "python3 siren" | awk '{print $2}' | head -1)
sleep .5
kill -INT $(ps -aux | grep "python3 siren" | awk '{print $2}' | head -1)
