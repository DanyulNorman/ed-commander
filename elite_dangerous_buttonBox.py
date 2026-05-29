import json
import time
import os
import pygame
import serial
# ----------------------------
# CONFIG
# ----------------------------
STATUS_FILE = os.path.expanduser(
    r"C:\Users\rever\Saved Games\Frontier Developments\Elite Dangerous\Status.json"
)

COM_PORT = "COM7"
BAUD_RATE = 115200

# ----------------------------
# All Elite Dangerous Flags (Flags + Flags2)
# ----------------------------
ELITE_FLAGS = {
    # Flags (0–31)
    "Docked": 0,
    "Landed": 1,
    "LandingGearDown": 2,
    "ShieldsUp": 3,
    "Supercruise": 4,
    "FlightAssistOff": 5,
    "HardpointsDeployed": 6,
    "InWing": 7,
    "LightsOn": 8,
    "CargoScoopDeployed": 9,
    "SilentRunning": 10,
    "ScoopingFuel": 11,
    "SrvHandbrake": 12,
    "SrvUsingTurretView": 13,
    "SrvTurretRetracted": 14,
    "SrvDriveAssist": 15,
    "FsdMassLocked": 16,
    "FsdCharging": 17,
    "FsdCooldown": 18,
    "LowFuel": 19,
    "OverHeating": 20,
    "HasLatLong": 21,
    "IsInDanger": 22,
    "BeingInterdicted": 23,
    "InMainShip": 24,
    "InFighter": 25,
    "InSRV": 26,
    "HudInAnalysisMode": 27,
    "NightVision": 28,
    "AltitudeFromAvgRadius": 29,
    "FsdJump": 30,
    "SrvHighBeam": 31,
}

ELITE_FLAGS2 = {
    "OnFoot": 0,
    "InTaxi": 1,
    "InMulticrew": 2,
    "OnFootInStation": 3,
    "OnFootOnPlanet": 4,
    "AimDownSight": 5,
    "LowOxygen": 6,
    "LowHealth": 7,
    "Cold": 8,
    "Hot": 9,
    "VeryCold": 10,
    "VeryHot": 11,
    "GlideMode": 12,
    "OnFootInHangar": 13,
    "OnFootSocialSpace": 14,
    "OnFootExterior": 15,
    "BreathableAtmosphere": 16,
    "TelepresenceMulticrew": 17,
    "PhysicalMulticrew": 18,
    "FsdHyperdriveCharging": 19,
}

# ----------------------------
# LED MAPPING
# ----------------------------
LED_TO_FLAGS = {
    0: ["FsdCooldown", "FsdMassLocked","FsdHyperdriveCharging"],  # blink
    1: ["LandingGearDown"],
    2: ["HudInAnalysisMode"],
    3: [],  # NEGATIVE_FLAGS handles FlightAssist
    4: [],  
    5: ["LowFuel", "OverHeating", "IsInDanger", "BeingInterdicted", "FsdJump","SilentRunning"],  # blink
    6: ["FsdCharging", "ScoopingFuel", "FsdJump"],  # blink
    7: [],  # Button 16 modifier indicator (special handling)
}

NEGATIVE_FLAGS = {
    3: ["FlightAssistOff"],  # LED 3 ON when FlightAssistOff is NOT set
    4: ["HudInAnalysisMode"]  
    
}

BLINK_LEDS = [0, 5, 6]


# Map your buttons to their DirectInput codes (example, you may need to adjust)
MODIFIER_BUTTONS = [5, 10, 20]


# Track button states from Arduino
buttons_state = {}

# ----------------------------
# SERIAL SETUP
# ----------------------------
ser = serial.Serial(COM_PORT, BAUD_RATE)
time.sleep(2)  # wait for Arduino reset

# ----------------------------
# STATE TRACKING
# ----------------------------
prev_led_states = [False] * 8  # steady LED state
prev_blink_state = [False] * 8  # blinking LED state

# ----------------------------
# Pygame setup
# ----------------------------
pygame.init()
pygame.joystick.init()

# Select Arduino joystick
joystick = None
for i in range(pygame.joystick.get_count()):
    j = pygame.joystick.Joystick(i)
    j.init()
    name = j.get_name()
    if "Arduino" in name:
        joystick = j
        print(f"Using joystick: {name}")
        break

if not joystick:
    raise RuntimeError("Arduino joystick not found!")
    

# ----------------------------
# HELPER FUNCTIONS
# ----------------------------
def send_led_command(led_num, state):
    cmd = f"ON:{led_num}" if state else f"OFF:{led_num}"
    ser.write(f"{cmd}\n".encode())

def send_blink(led_num):
    ser.write(f"BLINK:{led_num}\n".encode())

def check_modifier_led():
    """
    LED 7 ON if any of the MODIFIER_BUTTONS are pressed.
    """
    pygame.event.pump()
    for btn in MODIFIER_BUTTONS:
        # Pygame uses 0-based button numbers
        if joystick.get_button(btn - 1):
            return True
    return False

# ----------------------------
# MAIN LOOP
# ----------------------------
print("Starting Elite Dangerous LED watcher...")
while True:
    try:
        # Read ED status
        try:
            with open(STATUS_FILE, "r") as f:
                status = json.load(f)
        except json.JSONDecodeError:
            time.sleep(0.1)
            continue

        flags = status.get("Flags", 0)
        flags2 = status.get("Flags2", 0)

        for led_num in range(8):
            is_blink_led = led_num in BLINK_LEDS

            if led_num == 7:
                active = check_modifier_led()
            else:
                controlling_flags = LED_TO_FLAGS.get(led_num, [])
                active = any(
                    (flags & (1 << ELITE_FLAGS[f])) if f in ELITE_FLAGS else
                    (flags2 & (1 << ELITE_FLAGS2[f])) for f in controlling_flags
                )

                neg_flags = NEGATIVE_FLAGS.get(led_num, [])
                active |= any(not (flags & (1 << ELITE_FLAGS[f])) for f in neg_flags)

            # Handle LED output
            if is_blink_led:
                if active and not prev_blink_state[led_num]:
                    send_blink(led_num)
                elif not active and prev_blink_state[led_num]:
                    send_led_command(led_num, False)
                prev_blink_state[led_num] = active
            else:
                if active != prev_led_states[led_num]:
                    send_led_command(led_num, active)
                prev_led_states[led_num] = active

        time.sleep(0.1)

    except Exception as e:
        print("Error:", e)
        time.sleep(1)