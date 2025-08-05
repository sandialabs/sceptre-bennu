## Siren

The Siren provider was created in order to allow SCEPTRE field devices to receive/send analog
signals from/to HIL devices without another provider mediating. Thus, the Siren provider itself
is responsible for getting values from hardware  while publishing these values at the same time. 
Likewise, clients can send write requests to the provider for tags mapped to hardware, and the
Siren provider will write these values to the hardware.


### Usage

- `type`: `provider`
- `simulator`: `Siren`
- `server_endpoint`: (str) TCP address on which the provider listens for client requests.
- `publish_endpoint`: (str) Endpoint address for the UDP multicast traffic from the provider. Default: `udp://*;239.0.0.1:40000`
- `debug`: (bool) If set to true, print out debug information. Default: `false`
- `siren`: (dict)
  - `siren_json`: (str) Path to the `siren.json` configuration file **on the field device**. Default: `/etc/sceptre/siren.json`
  - `publish_rate`: (float) The rate in seconds at which the provider publishes its values. Default: `1.0`
  - `rms_buffer_size`: (int) The size of the CircularBuffer from which the RMS is calculated (if set). Default: `100`
  - `tags`: (list of dicts)
    - `name`: (str) Name of a tag meant to be published by the provider. Must match the name of a tag in `siren.json` if HIL operations are desired. But if not, can be an arbitrary name with arbitrary values.
    - `type`: (str) Datatype of the tag. Must be `int`, `float`, or `bool`.
    - `initial_value`: The initial value that provider holds for the tag. Either an integer, float, or boolean.
    - `publish_rms`: (bool) Boolean dictating whether the provider publishes the RMS value of a tag (true)
       or the raw signal from the hardware (false). Should only be set to true if receiving an AC
       signal from the hardware. Default: `false`

