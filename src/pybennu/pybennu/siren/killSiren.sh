#!/bin/bash

# Made to kill siren when it's running in the background
# without messing up connections to the hardware by sending sigint.
# We need to send the signal twice to fully close siren.

kill -INT "$(pgrep -o -f "python3 siren")"
sleep .5
kill -INT "$(pgrep -o -f "python3 siren")"
