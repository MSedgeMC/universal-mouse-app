#!/usr/bin/env python3
"""Read actual Input Remapper injector status; never emits input events."""
import dbus
import json
import re
import sys
if len(sys.argv)!=2 or not re.fullmatch(r'(?:INSTANT USB GAMING MOUSE|USB GAMING MOUSE)(?:\s+\d+)?\s*',sys.argv[1]):
    raise SystemExit('Invalid mouse group name')
bus=dbus.SystemBus()
proxy=bus.get_object('inputremapper.Control','/inputremapper/Control')
state=str(proxy.get_state(sys.argv[1],dbus_interface='inputremapper.Control'))
print(json.dumps({'state':state}))
