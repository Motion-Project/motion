#! /usr/bin/python

# written by folkert@vanheusden.com

# invocation:
#   ./send.py "hello, this is my text"

import json
import socket
import sys

host = '127.0.0.1'
udp_port = 1888
text = sys.argv[1]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(text, (host, udp_port))
