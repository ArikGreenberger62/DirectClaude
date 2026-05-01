# 10-modem-socket — STATE (compact session cache)

## Status
Build clean. Flashed and trace-verified 2026-05-01.
TCP socket connected to test.traffilog.co.il:30111, state = MODEM_STATE_READY.

## Key facts
- UART7 (PE7/PE8) = trace + command interface, 115200 8N1, COM7
- USART2 (PA2/PA3/PD3/PD4) = modem AT/data, HW flow control
- APN = "internet", access mode 0 (buffer mode), connectID = 0
- PWRKEY circuit: MCU HIGH → modem on (direct drive, 1000 ms pulse)
- SIM ready, CSQ ~11, network registration stat=1 confirmed at last boot
- IP assigned: 10.117.116.85 (DHCP, may change)
- huart7.hdmatx = NULL and huart2.hdmatx = NULL (DMA TX detached — blocking TX only)
- `send <message>` command via UART7 terminal → Modem_SendTCP()
- Incoming TCP data printed as `[TCP] recv: <data>`

## Bug fixes locked in
- None beyond standard project patterns

## Next step
Project complete. Optional extensions:
- Add a periodic heartbeat send from the main loop
- Implement TCP reconnect on MODEM_STATE_ERROR (re-call mdm_tcp_connect)
- Add `AT+CMGD=1,4` SMS cleanup and SMS receive (+CNMI) if needed
