"""SCEPTRE provider for Speedgoat Simulink models.

The provider drives a Speedgoat real-time target through a persistent MATLAB
session on a Windows host. It talks to that session with file-based IPC over
SSH: the scripts in ``ipc_files/`` are uploaded to ``C:\\MatlabIPC`` and MATLAB
polls a command file for READ/WRITE/READMULTI requests.

Security / startup
------------------
SSH credentials are entered interactively ON PURPOSE. The username and the
Windows password are typed by an operator at startup and are never written to
config.ini, environment variables or any other file. Do not "fix" this by
moving the credentials into configuration.

The consequence is that the automated SCEPTRE startup
(``pybennu-power-solver start ... -d``, launched by sceptre-start.sh) is
*expected to fail* for this solver: as a daemon it has no terminal to prompt
on. It logs an error explaining this and exits. To run the provider, an
operator logs in to the provider VM and starts it in the foreground::

    pybennu-power-solver start -c /etc/sceptre/config.ini -e pybennu

(no ``-d``), then types the username and password when prompted.

What *is* persisted: an SSH key pair at ``~/.ssh/speedgoat_id_rsa(.pub)`` on
the provider VM, and its public key appended to the Windows user's
``authorized_keys``. The password is only used to deploy that key and is asked
for again on every start.
"""

import getpass
import logging
import os
import platform
import shlex
import stat
import sys
import time

import paramiko
from loguru import logger

from pybennu.distributed.provider import Provider

# paramiko logs through stdlib logging; its transport chatter is too noisy.
logging.getLogger("paramiko.transport").setLevel(logging.WARNING)

# How the operator must start this solver; shown when startup is not interactive.
MANUAL_START = "pybennu-power-solver start -c /etc/sceptre/config.ini -e pybennu"


class SpeedgoatProvider(Provider):
    def __init__(
        self,
        server_endpoint,
        publish_endpoint,
        matlab_host,
        simulink_file,
        publish_points,
        debug=False,
    ):
        """
        SCEPTRE Provider to control Speedgoat Simulink model via persistent MATLAB session
        on a Windows machine using file-based IPC over SSH.

        The SSH username and password are prompted for interactively; see the
        module docstring for why, and for how to start the provider.

        Args:
            server_endpoint (str): SCEPTRE server endpoint.
            publish_endpoint (str): SCEPTRE publish endpoint.
            matlab_host (str): IP or hostname of the Windows machine running MATLAB.
            simulink_file (str): Path to the Simulink model (.slx) to upload.
            publish_points (str): Path to publishPoints.txt, one tag per line.
            debug (bool, optional): Enable debug behaviour.
        """
        super().__init__(server_endpoint, publish_endpoint)

        self.matlab_host = matlab_host
        self.simulink_file = simulink_file
        self.publish_points = []
        self.load_publish_points(publish_points)
        self.debug = debug

        self.current_values = {}
        self.running = False
        self.client = None
        self.sftp = None

        # Credentials are typed by an operator, never read from config (see
        # module docstring). Fail clearly instead of with a bare EOFError when
        # started without a terminal, e.g. by the automated SCEPTRE startup.
        self._require_interactive_terminal()

        # Prompt for username before connecting
        self.username = self._prompt_for_username()

        # Remote IPC folder and script paths on Windows machine
        self.ipc_folder = "C:\\MatlabIPC"
        self.listener_script = f"{self.ipc_folder}\\speedgoat_listener.m"
        self.powershell_script = f"{self.ipc_folder}\\send_matlab_command.ps1"

        # Local IPC scripts shipped alongside this module
        script_dir = os.path.dirname(os.path.abspath(__file__))
        self.local_ipc_folder = os.path.join(script_dir, "ipc_files")

        # Connect to Matlab host and setup keys
        self._connect_ssh()

        # Setup IPC files
        self._remove_folder(self.ipc_folder)
        self._ensure_ipc_folder()
        self.upload_ipc_files()
        self.upload_simulink_file()

    def run(self):
        self.running = True
        logger.info("Starting SpeedgoatProvider run loop")

        # Start the MATLAB listener before processing commands
        self.start_matlab_listener()

        super().run()

    def load_publish_points(self, filepath):
        """
        Load the list of points (tags) to publish from a text file.

        Each line in the file should be a tag name.

        Args:
            filepath (str): Path to the publishPoints.txt file.
        """
        if not filepath or not os.path.isfile(filepath):
            logger.error(f"publishPoints file not found: {filepath}")
            self.publish_points = []
            return

        with open(filepath) as f:
            points = [
                line.strip() for line in f if line.strip() and not line.startswith("#")
            ]

        self.publish_points = points
        logger.info(f"Loaded {len(points)} publish points from {filepath}")

    def _generate_ssh_key(self, key_path):
        """
        Generate an RSA SSH key pair at key_path if it doesn't exist.
        """

        ssh_dir = os.path.dirname(key_path)
        if not os.path.exists(ssh_dir):
            logger.info(f"Creating local SSH directory at {ssh_dir}")
            os.makedirs(ssh_dir, mode=0o700, exist_ok=True)

        if os.path.exists(key_path) and os.path.exists(key_path + ".pub"):
            logger.info(f"SSH key pair already exists at {key_path}")
            return

        logger.info(f"Generating new RSA SSH key pair at {key_path}")
        key = paramiko.RSAKey.generate(2048)
        key.write_private_key_file(key_path)
        os.chmod(key_path, stat.S_IRUSR | stat.S_IWUSR)  # 600 permissions

        pub_key = f"{key.get_name()} {key.get_base64()}\n"
        with open(key_path + ".pub", "w") as f:
            f.write(pub_key)
        os.chmod(
            key_path + ".pub", stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH
        )  # 644 permissions

    def _require_interactive_terminal(self):
        """Refuse to start without a terminal to prompt for credentials on.

        This is the intended outcome of the automated SCEPTRE startup, which
        runs the solver as a daemon with stdin on /dev/null. Credentials are
        deliberately never read from files or the environment.
        """
        if sys.stdin is not None and sys.stdin.isatty():
            return

        msg = (
            "Speedgoat requires the SSH username and password to be typed in "
            "by an operator (by design, credentials are never stored in config "
            "files or the environment), so it cannot be started automatically. "
            f"Log in to {platform.node()} and start it manually in the "
            f"foreground: {MANUAL_START}"
        )
        logger.error(msg)
        raise RuntimeError(msg)

    def _prompt_for_username(self):
        """Prompt user for SSH username.

        Intentionally interactive: do not replace with a config/env value
        (see module docstring).
        """
        while True:
            username = input(f"Enter SSH username for {self.matlab_host}: ").strip()
            if username:
                return username
            print("Username cannot be empty. Please try again.")

    def _prompt_for_password(self):
        """Prompt user for the Windows machine password.

        Intentionally interactive and never stored: the password is held only
        in memory, used once to deploy the SSH public key, and asked for again
        on every start (see module docstring).
        """
        logger.info("Please enter the Windows machine password...")

        # Try up to 3 times
        for _attempt in range(3):
            try:
                password = getpass.getpass(
                    f"Enter password for {self.username}@{self.matlab_host}: "
                )
                if password:
                    return password
                print("Password cannot be empty. Please try again.")
            except Exception as e:
                print(f"Error reading password: {e}")

        raise RuntimeError(
            f"Failed to obtain the password for {self.username}@{self.matlab_host} "
            "after 3 attempts"
        )

    def _deploy_public_key(self, public_key_path):
        """
        Copy the public key to the remote Windows machine's authorized_keys.
        Requires password or other authentication for initial connection.
        """
        with open(public_key_path) as f:
            pub_key = f.read().strip()

        # Connect using password or existing method to deploy the key
        temp_client = paramiko.SSHClient()
        temp_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        try:
            temp_client.connect(
                self.matlab_host,
                username=self.username,
                password=self.windows_pass,
                timeout=30,
            )

            sftp = temp_client.open_sftp()

            # Ensure .ssh directory exists
            ssh_dir = f"C:\\Users\\{self.username}\\.ssh"
            try:
                sftp.stat(ssh_dir)
            except OSError:
                logger.info(f"Creating remote .ssh directory at {ssh_dir}")
                sftp.mkdir(ssh_dir)

            authorized_keys_path = ssh_dir + "\\authorized_keys"

            # Read existing authorized_keys if any
            try:
                with sftp.open(authorized_keys_path, "r") as f_remote:
                    existing_keys = f_remote.read().decode("utf-8")
            except OSError:
                existing_keys = ""

            if pub_key not in existing_keys:
                logger.info("Appending public key to authorized_keys")
                with sftp.open(authorized_keys_path, "a") as f_remote:
                    f_remote.write(pub_key + "\n")

            # Set permissions on authorized_keys (if needed)
            # Windows SSH server may ignore chmod, but no harm in trying
            try:
                sftp.chmod(authorized_keys_path, 0o600)
            except Exception:
                pass

            sftp.close()
            temp_client.close()
        except Exception as e:
            logger.error(f"Failed to deploy public key: {e}")
            raise

    def _connect_ssh(self):
        """
        Generate a key, deploy it if needed, then connect using the key.
        """
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # Default key path in user's home directory
        home = os.path.expanduser("~")
        self.ssh_key_path = os.path.join(home, ".ssh", "speedgoat_id_rsa")

        # Generate key if needed
        self._generate_ssh_key(self.ssh_key_path)

        # Prompt for password (interactive by design, never stored)
        self.windows_pass = self._prompt_for_password()

        # Deploy the key using the password
        self._deploy_public_key(self.ssh_key_path + ".pub")

        # Connect using the private key and SSH agent if available
        try:
            key = paramiko.RSAKey.from_private_key_file(self.ssh_key_path)
            self.client.connect(
                self.matlab_host,
                username=self.username,
                pkey=key,
                allow_agent=True,
                look_for_keys=True,
            )
            logger.info(
                f"SSH connected to {self.matlab_host} using key {self.ssh_key_path}"
            )
            self.sftp = self.client.open_sftp()
        except Exception as e:
            logger.error(f"SSH connection failed: {e}")
            raise

    def _remove_folder(self, remote_path):
        try:
            self.sftp.stat(self.ipc_folder)
            logger.debug(f"Deleting remote_path {remote_path}")
            for entry in self.sftp.listdir_attr(remote_path):
                entry_path = remote_path + "/" + entry.filename
                if stat.S_ISDIR(entry.st_mode):
                    # It's a directory, recurse into it
                    self._remove_folder(entry_path)
                else:
                    # It's a file, remove it
                    logger.debug(f"Deleting entry_path {entry_path}")
                    self.sftp.remove(entry_path)
            # Now the directory is empty, remove it
            self.sftp.rmdir(remote_path)
            logger.info(f"Removed directory: {remote_path}")
        except OSError as e:
            logger.warning(f"Failed to remove {remote_path}: {e}")

    def _ensure_ipc_folder(self):
        try:
            self.sftp.stat(self.ipc_folder)
        except OSError:
            logger.info(f"IPC folder {self.ipc_folder} does not exist. Creating it.")
            self.sftp.mkdir(self.ipc_folder)

        # Remove read-only attribute
        cmd_clear_readonly = f"powershell -Command \"attrib -R '{self.ipc_folder}'\""
        self._exec_command(cmd_clear_readonly)

    def upload_file(self, local_path, remote_path):
        if not os.path.isfile(local_path):
            raise FileNotFoundError(f"Local file not found: {local_path}")
        logger.info(f"Uploading {local_path} to {remote_path}")
        self.sftp.put(local_path, remote_path)

    def upload_ipc_files(self):
        """
        Upload all necessary IPC files from local_ipc_folder to remote IPC folder.
        """
        required_files = [
            "speedgoat_listener.m",
            "send_matlab_command.ps1",
            "start_matlab.ps1",
        ]

        for filename in required_files:
            local_path = os.path.join(self.local_ipc_folder, filename)
            remote_path = f"{self.ipc_folder}\\{filename}"
            self.upload_file(local_path, remote_path)
        logger.info("All IPC files uploaded successfully.")

    def upload_simulink_file(self):
        """
        Upload the Simulink model file to the MatlabIPC folder on Windows.
        """
        if not os.path.isfile(self.simulink_file):
            raise FileNotFoundError(
                f"Local Simulink model file not found: {self.simulink_file}"
            )

        filename = os.path.basename(self.simulink_file)
        remote_path = f"{self.ipc_folder}\\{filename}"

        logger.info(f"Uploading Simulink model {self.simulink_file} to {remote_path}")
        self.sftp.put(self.simulink_file, remote_path)

    def _exec_command(self, command, timeout=30):
        logger.debug(f"Executing SSH command: {command}")
        _stdin, stdout, stderr = self.client.exec_command(command, timeout=timeout)
        exit_status = stdout.channel.recv_exit_status()
        out = stdout.read().decode("utf-8").strip()
        err = stderr.read().decode("utf-8").strip()

        logger.debug(f"Command exit status: {exit_status}")
        logger.debug(f"Command stdout: {out}")
        logger.debug(f"Command stderr: {err}")

        if exit_status != 0:
            logger.error(f"Command failed: {err}")
        return out, err, exit_status

    def start_matlab_listener(self):
        """
        Start the persistent MATLAB listener session on the Windows machine.

        Runs the listener script in MATLAB non-blocking mode using PowerShell.
        """

        # Path to the PowerShell script on Windows
        start_script = f"{self.ipc_folder}\\start_matlab.ps1"

        # Compose command to run the script with powershell -File
        # Using -File avoids quoting issues with -Command
        ps_command = f'powershell -File "{start_script}"'

        logger.info(f"Starting MATLAB listener with command: {ps_command}")

        out, err, exit_status = self._exec_command(ps_command)
        logger.info(f"MATLAB start command exit status: {exit_status}")
        logger.info(f"MATLAB start command stdout: {out}")
        logger.info(f"MATLAB start command stderr: {err}")

    def stop_matlab_listener(self):
        """
        Gracefully stop the MATLAB listener by sending the 'exit' command.
        """
        response = self.send_matlab_command("exit")
        logger.info(f"MATLAB listener shutdown response: {response}")

    def send_matlab_command(self, matlab_cmd, timeout=10):
        """
        Send a MATLAB command to the listener via the PowerShell IPC script.

        Args:
            matlab_cmd (str): MATLAB command string to execute.
            timeout (int): Timeout in seconds for waiting for response.

        Returns:
            str: The output from MATLAB.
        """
        safe_cmd = shlex.quote(matlab_cmd)
        ps_command = (
            f"powershell -Command \"& '{self.powershell_script}' -command {safe_cmd}\""
        )
        logger.debug(f"Sending MATLAB command via SSH: {ps_command}")

        _stdin, stdout, stderr = self.client.exec_command(ps_command, timeout=timeout)
        exit_status = stdout.channel.recv_exit_status()
        out = stdout.read().decode("utf-8").strip()
        err = stderr.read().decode("utf-8").strip()

        if exit_status != 0:
            logger.error(f"Error sending MATLAB command: {err}")
            raise RuntimeError(f"MATLAB command failed: {err}")

        logger.debug(f"MATLAB command output: {out}")
        return out

    def query(self):
        if not self.publish_points:
            return "ERR=No publish points loaded"
        return f"ACK={','.join(self.publish_points)}"

    def read(self, tag):
        try:
            cmd = f"READ {tag}"
            response = self.send_matlab_command(cmd)
            response = response.strip()

            if response.startswith(("ACK=", "ERR=")):
                # Listener already sent ACK/ERR, just return it
                return response
            # Listener sent raw value, wrap it in ACK=
            return f"ACK={response}"
        except Exception as e:
            logger.error(f"Read failed for tag {tag}: {e}")
            return f"ERR=Read failed for tag {tag}"

    def write(self, tags):
        success_count = 0
        for tag, value in tags.items():
            logger.debug(f"in write: {tag} {value}")
            try:
                cmd = f"WRITE {tag} {value}"
                response = self.send_matlab_command(cmd).strip()

                if response.startswith("ACK="):
                    success_count += 1
                else:
                    logger.error(f"Write failed for tag {tag}: {response}")
                    return f"ERR=Write failed for tag {tag}: {response}"
            except Exception as e:
                logger.error(f"Write exception for tag {tag}: {e}")
                return f"ERR=Write exception for tag {tag}"

        return f"ACK=Wrote {success_count} tags"

    def periodic_publish(self):
        while True:
            try:
                if not self.publish_points:
                    time.sleep(1)
                    continue
                tags_csv = ",".join(self.publish_points)
                response = self.send_matlab_command(f"READMULTI {tags_csv}")
                # response format: ACK=val1,val2,val3,...
                if response.startswith("ACK="):
                    values = response[4:].split(",")
                    msg = ""
                    for tag, val in zip(self.publish_points, values):
                        self.current_values[tag] = val
                        msg += f"{tag}:{val.lower()},"
                    self.publish(msg)
                    logger.debug(f"Published: {msg}")
                else:
                    logger.error(f"Batch read failed: {response}")
            except Exception as e:
                logger.error(f"Error during periodic publish: {e}")
            time.sleep(0.01)

    def close(self):
        if self.client:
            self.client.close()
            logger.info(f"SSH connection to {self.matlab_host} closed")
