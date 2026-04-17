#!/usr/bin/env python3
import socketio
import sys
import time
import os

try:
    import keyboard
except ImportError:
    print('Missing dependency: keyboard')
    print('Install with: pip install keyboard')
    sys.exit(1)
try:
    import jwt
except ImportError:
    print('Missing dependency: pyjwt')
    print('Install with: pip install pyjwt')
    sys.exit(1)



#find .cncrcjs in home directory and read token from it
home = os.path.expanduser('~')
cncrcjs_path = os.path.join(home, '.cncrc')
if os.path.exists(cncrcjs_path):
    #read SECRET from .cncrc file. json secret: "secret"
    import json
    with open(cncrcjs_path, 'r') as f:
        config = json.load(f)
        if 'secret' in config:
            CNCJS_SECRET = config['secret']
            print('Loaded CNCJS token from .cncrc file.')
        else:
            print('No "secret" key found in .cncrc file')
CNCJS_TOKEN = jwt.encode({'id': 'pendant', 'name': 'pendant'}, CNCJS_SECRET, algorithm='HS256')


CNCJS_URL   = 'http://localhost:8000'
#CNCJS_TOKEN = 'YOUR_CNCJS_API_TOKEN_HERE'  # automatic token generation from .cncrc file or hardcoded value
SERIAL_PORT = '/dev/ttyACM0'

CURRENT_AXIS = 'X'  # Default axis for movement commands
STEP_SIZE = 0.5     # Default step size for jogging (in mm)
PROBE_DIAMETER = 4.0  # Diameter of the probe tip (in mm)
# Edit this mapping to add/remove key->gcode actions.
# Use string names: 'f20', 'f19', etc. Use single chars for regular keys: 'm', 'n', etc.
KEY_TO_GCODE = {
    'm': '!',
    'n': '~',
    'f20': 'G0 X10',
}
ENABLED = False
sio = socketio.Client()
#f20 - jog forward
#f19 - jog backward
#f18 - z axis
#f17 - y axis
#f16 - x axis
#f15 - probe current axis
def send_cmd(cmd):
    """Send one command to cncjs on the selected serial port."""
    sio.emit('command', (SERIAL_PORT, 'gcode', cmd))
    print(f'Sent: {cmd}')


def handle_key(key_event):
    global CURRENT_AXIS
    key = key_event.name
    
    # Check if key is directly in mapping
    if key in KEY_TO_GCODE:
        send_cmd(KEY_TO_GCODE[key])
    
    # Handle special function keys
    match key:
        case 'f16':
            CURRENT_AXIS = 'X'
        case 'f17':
            CURRENT_AXIS = 'Y'
        case 'f18':
            CURRENT_AXIS = 'Z'
        case 'f19':
            if(ENABLED):
                send_cmd(f'G0 {CURRENT_AXIS}-{STEP_SIZE}')
        case 'f20':
            if(ENABLED):
                send_cmd(f'G0 {CURRENT_AXIS}+{STEP_SIZE}')
        case 'f15':
            if(ENABLED):
                match CURRENT_AXIS:
                    case 'X' | 'Y':
                        send_cmd(f'G38.2 {CURRENT_AXIS}20 F75') # fast find probe
                        send_cmd(f'G0 {CURRENT_AXIS}-2') # back off a bit
                        send_cmd(f'G38.2 {CURRENT_AXIS}20 F25') # slow find probe for accuracy
                        send_cmd(f'G4 P0.1') # dwell for a moment to stabilize
                        send_cmd(f'G10 L20 P1 {CURRENT_AXIS}{-PROBE_DIAMETER/2}') # set current axis to 0 with probe offset
                        send_cmd(f'G4 P0.1') # dwell again to ensure command is processed
                        send_cmd(f'G0 {CURRENT_AXIS}-5')  # Move away after probing
                    case 'Z':
                        send_cmd(f'G38.2 Z-20 F75') # fast find probe
                        send_cmd(f'G0 Z2') # back off a bit
                        send_cmd(f'G38.2 Z-20 F25') # slow find probe for accuracy
                        send_cmd(f'G4 P0.1') # dwell for a moment to stabilize
                        send_cmd(f'G10 L20 P1 Z0') # set Z to 0 at probe contact
                        send_cmd(f'G0 Z5')  # Move up after probing
        case 'f14':
            ENABLED = True
        case 'f13':
            ENABLED = False

    

def on_key_press(key_event):
    try:
        print(f'Key pressed: {key_event.name}')
    except Exception as e:
        print(f'Error reading key: {e}')
    
    # Stop listener on ESC
    if key_event.name == 'esc':
        return False
    
    handle_key(key_event)

@sio.event
def connect():
    print('Connected')
sio.emit('open', (SERIAL_PORT, {'baudrate': 115200, 'controllerType': 'Grbl'}))

@sio.event
def disconnect():
    print('Disconnected')

@sio.on('serialport:read')
def on_read(data):
    print('From machine:', data)

def main():
    time.sleep(4)
    sio.connect(f'{CNCJS_URL}?token={CNCJS_TOKEN}', transports=['websocket'])
    print('Namespaces:', sio.namespaces)
    print('Connected:', sio.connected)
    print('Active key mapping:')
    for key, cmd in KEY_TO_GCODE.items():
        print(f'  {key} -> {cmd}')
    print('Press ESC to exit.\n')

    keyboard.on_press(on_key_press)
    print('Keyboard listener started...\n')

    try:
        keyboard.wait('esc')
    except KeyboardInterrupt:
        pass
    finally:
        sio.disconnect()
        print('Exited.')


if __name__ == '__main__':
    main()
