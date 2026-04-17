"""Capture COM7 trace from ThreadX blink project.

Opens COM7 before reset, hard-resets the MCU via STM32_Programmer_CLI,
reads for 10 s, prints every line. Used to verify:
  - boot banner appears once
  - 1 Hz heartbeat lines with tick/red/green counters tracking as expected
"""
import serial
import subprocess
import time

PROG = (r"C:\Program Files (x86)\STMicroelectronics\STM32Cube"
        r"\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe")

ser = serial.Serial("COM7", 115200, timeout=2)
time.sleep(0.3)

subprocess.run([PROG, "-c", "port=SWD", "freq=4000", "-hardRst"],
               capture_output=True)

deadline = time.time() + 10
while time.time() < deadline:
    line = ser.readline().decode("utf-8", errors="replace").strip()
    if line:
        print(line)
ser.close()
