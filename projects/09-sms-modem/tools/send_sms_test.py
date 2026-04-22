"""One-shot: wait for modem READY on COM7, then send an SMS command line.

Opens the port BEFORE reset so we catch the startup banner, triggers a hardware
reset via STM32_Programmer_CLI, then streams UART7 trace until we see
'modem READY' (or a timeout). Sends the sendsms command and prints everything
until either RESULT: SUCCESS / RESULT: FAIL or a final timeout.
"""
import serial, subprocess, sys, time

PORT = "COM7"
BAUD = 115200
PROG = r"C:\Users\arikg\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe"

PHONE = "+972544801489"
MSG   = "hello"

READY_TIMEOUT_S = 120.0   # modem init can include 60s network wait
SEND_TIMEOUT_S  = 60.0

def main() -> int:
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    try:
        # Drain anything already buffered
        ser.reset_input_buffer()

        # Hardware reset so we see the full init trace
        subprocess.run([PROG, "-c", "port=SWD", "freq=4000", "-hardRst"],
                       capture_output=True, check=False)

        print(f"[test] reset issued, waiting for READY on {PORT}", flush=True)
        t0 = time.time()
        buf = bytearray()
        ready = False
        while time.time() - t0 < READY_TIMEOUT_S:
            chunk = ser.read(256)
            if not chunk:
                continue
            buf.extend(chunk)
            try:
                text = chunk.decode("utf-8", errors="replace")
            except Exception:
                text = ""
            sys.stdout.write(text)
            sys.stdout.flush()
            if b"modem READY" in buf or b"init: PASS" in buf:
                ready = True
                break
            if b"init: FAIL" in buf:
                print("\n[test] modem init FAILED", flush=True)
                return 2

        if not ready:
            print("\n[test] timeout waiting for modem READY", flush=True)
            return 3

        # Small settle pause so the help block finishes printing
        time.sleep(1.0)

        cmd = f"sendsms {PHONE} , {MSG}\r\n"
        print(f"\n[test] sending command: {cmd.strip()}", flush=True)
        ser.write(cmd.encode("ascii"))
        ser.flush()

        t0 = time.time()
        sendbuf = bytearray()
        while time.time() - t0 < SEND_TIMEOUT_S:
            chunk = ser.read(256)
            if not chunk:
                continue
            sendbuf.extend(chunk)
            sys.stdout.write(chunk.decode("utf-8", errors="replace"))
            sys.stdout.flush()
            if b"RESULT: SUCCESS" in sendbuf:
                print("\n[test] SMS send SUCCESS", flush=True)
                return 0
            if b"RESULT: FAIL" in sendbuf:
                print("\n[test] SMS send FAILED", flush=True)
                return 4

        print("\n[test] timeout after command", flush=True)
        return 5
    finally:
        ser.close()

if __name__ == "__main__":
    sys.exit(main())
