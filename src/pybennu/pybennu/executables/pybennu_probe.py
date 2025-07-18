"""Command line probe.

Used for querying/reading/writing values to/from a bennu FEP or provider.
"""

import argparse
import sys

import pybennu.distributed.client as client
import pybennu.distributed.swig._Endpoint as E


def handler(reply: str) -> None:
    split = reply.split(',')
    print("Reply:")
    if len(split) > 1:
        for tag in split:
            print("\t", tag)
    else:
        print("\t", split[0])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-e', '--endpoint',
        default='tcp://127.0.0.1:1330',
        help="FEP (:1330) or Provider (:5555) endpoint, e.g. 'tcp://172.16.1.2:5555' or 'tcp://127.0.0.1:1330'"
    )
    parser.add_argument(
        '-c', '--command',
        default='query',
        choices=['query', 'read', 'write', 'raw'],
        help="Command to issue: query | read | write | raw"
    )
    parser.add_argument(
        '-t', '--tag',
        help="Full tag name, e.g. 'bus101.active', 'br1.closed' or 'pv_power.value'"
    )
    value_group = parser.add_mutually_exclusive_group()
    value_group.add_argument(
        '-v', '--value',
        help='Value to write (for analog tags)'
    )
    value_group.add_argument(
        '-s', '--status',
        choices=['true', 'false', '1', '0'],
        help="Status (for boolean tags), one of: true | 1 | false | 0"
    )
    value_group.add_argument(
        '-r', '--raw', type=str,
        help='Raw command to issue, passed as-is. '
             'Example: --raw "WRITE=G1CB1_binary_input_closed:1,G2CB2_binary_input_closed:1,G3CB3_binary_input_closed:1"'
    )

    args = parser.parse_args()

    endpoint = E.new_Endpoint()
    E.Endpoint_str_set(endpoint, args.endpoint)

    # print(E.Endpoint_str_get(endpoint))
    # tcp://172.16.1.2:5555

    # print(E.Endpoint_hash(endpoint))
    # 15fd2f305feef8d

    probe = client.Client(endpoint)
    probe.reply_handler = handler

    msg = ""
    if args.command == 'query':
        msg += "QUERY="
    elif args.command == 'read':
        msg += "READ=" + args.tag
    elif args.command == 'write':
        if args.status:
            val = 'true' if args.status in ['true', '1'] else 'false'
        else:
            val = args.value
        msg += f"WRITE={args.tag}:{val}"
    elif args.command == 'raw':
        msg += args.raw
    else:
        print(f"UNKNOWN COMMAND: {args.command}")
        sys.exit(1)

    print(f"Sending message: {msg}")
    probe.send(msg)


if __name__ == "__main__":
    main()
