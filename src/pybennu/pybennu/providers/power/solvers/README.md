## Siren

The Siren provider was created in order to allow SCEPTRE field devices to receive/send analog
signals from/to HIL devices without another provider mediating. Thus, the Siren provider itself
is responsible for getting values from hardware while publishing these values at the same time.
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

## Speedgoat

The Speedgoat provider drives a Simulink model running on a Speedgoat real-time target. It does this
through a persistent MATLAB session on a Windows host. The provider connects to that host over SSH,
uploads the IPC scripts in `speedgoat/ipc_files/` and the Simulink model to `C:\MatlabIPC`, and starts
the MATLAB listener. After that, reads and writes go to MATLAB through a file-based command/result
queue. The provider polls every publish point and publishes the values continuously (about every 10 ms
plus one SSH round trip).

### Usage

- `type`: `provider`
- `simulator`: `Speedgoat`
- `matlab_host`: (str) IP or hostname of the Windows machine running MATLAB.
- `simulink_file`: (str) Path to the Simulink model (`.slx`). Injected to `/etc/sceptre/speedgoat.slx`.
- `publish_points`: (str) Path to a text file with one tag per line (lines starting with `#` are ignored).
  Injected to `/etc/sceptre/publishPoints.txt`.
- `debug`: (bool) If set to true, print out debug information. Default: `false`

### Starting the provider (manual, by design)

The SSH username and Windows password are **typed in by an operator at startup**. This is
deliberate, for security: credentials are never written to `config.ini`, environment variables or
any other file. Please don't add a config option for them.

As a result, the automated SCEPTRE startup (`pybennu-power-solver start ... -d`) **fails on purpose**
for this solver. As a daemon it has no terminal to prompt on, so it logs an error saying it must be
started manually, then exits. To run it, log in to the provider VM and start it in the foreground
(no `-d`):

```text
pybennu-power-solver start -c /etc/sceptre/config.ini -e pybennu
```

then enter the username and password when prompted. The password is held in memory only and is
asked for again on every start.

What *is* persisted:

- an SSH key pair at `~/.ssh/speedgoat_id_rsa` and `~/.ssh/speedgoat_id_rsa.pub` on the provider VM
- that public key, appended to `C:\Users\<username>\.ssh\authorized_keys` on the Windows host

### Windows host prerequisites

- OpenSSH server enabled, with PowerShell available to the SSH user
- MATLAB with Simulink Real-Time at `F:\MATLAB\bin\matlab.exe` (hard-coded in `ipc_files/start_matlab.ps1`)
- A Speedgoat target registered in MATLAB as `TargetPC1` (hard-coded in `ipc_files/speedgoat_listener.m`)
- Write access to `C:\MatlabIPC`, which the provider deletes and recreates on every start
