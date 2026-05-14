---
name: wifi-ble/fc41d
description: Quectel FC41D Wi-Fi+BLE module skill — WiFi scan/connect, TCP socket, BLE scan/advertise AT commands for TFL_CONNECT_2 board.
task_types: [code, arch, test]
keywords: [wifi, ble, bluetooth, fc41d, quectel, wireless, wlan, scan, advertise, advertising, beacon, socket, tcp, udp, http, mqtt, ssl, qwscan, qstaapinfo, qbleinit, qbleadvdata, qblescan, qiopen, qisend, wifi_ble, wifible, station, hotspot, ap, gatt, central, peripheral]
priority: tier4
---

# Quectel FC41D Wi-Fi + BLE Module Skill

Wi-Fi 802.11b/g/n + Bluetooth 5.2 combo module. AT command interface over UART9.
Reference: `Quectel_FC41D&FCM100D&FCM740D&FLMx40D_AT_Commands_Manual_V2.0.pdf`

## Hardware (TFL_CONNECT_2 board)

| Signal           | MCU Pin | Direction | Notes                                  |
|------------------|---------|-----------|----------------------------------------|
| WIFI_UART9_TX    | PD15    | MCU→FC41D | AF11 — FC41D UART RX                  |
| WIFI_UART9_RX    | PD14    | FC41D→MCU | AF11 — FC41D UART TX                  |
| WIFI_SPI4_SCK    | PE2     | MCU→FC41D | AF5 — SPI4 clock (12 MHz)             |
| WIFI_SPI4_NSS    | PE4     | MCU→FC41D | AF5 — SPI4 chip select (HW)           |
| WIFI_SPI4_MISO   | PE5     | FC41D→MCU | AF5 — SPI4 data in                    |
| WIFI_SPI4_MOSI   | PE6     | MCU→FC41D | AF5 — SPI4 data out                   |
| WIFI_BLE_PWR_EN  | PD5     | MCU       | OUT — HIGH to enable module power      |
| WIFI_BLE_RESETN  | PD6     | MCU       | OUT (open drain) — LOW to reset module |
| BLE_STS          | PD11    | FC41D→MCU | IN — BLE status indicator              |

**DMA:** GPDMA2 Channel 5 assigned to UART9 TX.
**Defined in main.h:** `MAIN_WIFI_BLE_UART_INDEX 9`, `MAIN_WIFI_BLE_UART (huart9)`.

## Power-On / Reset Sequence

```c
/* 1. Enable module power */
HAL_GPIO_WritePin(GPIOD, WIFI_BLE_PWR_EN_Pin, GPIO_PIN_SET);
HAL_Delay(100);

/* 2. Assert RESETN LOW for ≥ 100 ms, then release */
HAL_GPIO_WritePin(GPIOD, WIFI_BLE_RESETN_Pin, GPIO_PIN_RESET);
HAL_Delay(200);
HAL_GPIO_WritePin(GPIOD, WIFI_BLE_RESETN_Pin, GPIO_PIN_SET);

/* 3. Wait for module boot — ~2–3 s for "ready" indication */
/* Send AT until OK response (poll every 500 ms, up to 10 s) */
```

## AT Initialization Sequence

```
ATE0                        ← disable echo (essential)
AT+QVERSION                 ← read firmware version (trace log)
```

Module is ready when `AT` → `OK`.

---

## 1. WiFi — Scan Available Networks

```
AT+QWSCAN
```

**Response (one line per AP found):**
```
+QWSCAN: <signal>,<security>,<ssid>,<bssid>

OK
```

| Field      | Type    | Notes                                               |
|------------|---------|-----------------------------------------------------|
| signal     | integer | 0–63; convert to dBm: `actual_dBm = value - 100`   |
| security   | integer | 0=OPEN, 1=WEP, 2=WPA, 3=WPA2, 4=WPA/WPA2 mixed    |
| ssid       | string  | AP name                                              |
| bssid      | string  | MAC address of AP                                    |

**Example:**
```
→ AT+QWSCAN
← +QWSCAN: 55,3,MyHomeWiFi,AA:BB:CC:DD:EE:FF
← +QWSCAN: 42,3,Office_5G,11:22:33:44:55:66
← +QWSCAN: 30,0,FreeWiFi,77:88:99:AA:BB:CC
←
← OK
```

## 2. WiFi — Connect to Selected Network

**Connect (one-time):**
```
AT+QSTAAPINFO=<ssid>,<password>
```

**Connect and save (auto-reconnect on reboot):**
```
AT+QSTAAPINFODEF=<ssid>,<password>
```

**Response:** `OK` on success, `ERROR` on failure.

**Check connection status:**
```
AT+QGETWIFISTATE
```

**Response when connected:**
```
+QGETWIFISTATE: <ssid>,<bssid>,<channel>,<rssi>

OK
```

**Response when disconnected:**
```
+QGETWIFISTATE: DISCONNECT

OK
```

**Disconnect from current AP:**
```
AT+QSTADISCONN
```

**Full WiFi connect sequence:**
```
→ ATE0
← OK
→ AT+QSTAAPINFODEF=MyHomeWiFi,MyPassword123
← OK
(wait ~3–5 s for DHCP)
→ AT+QGETWIFISTATE
← +QGETWIFISTATE: MyHomeWiFi,AA:BB:CC:DD:EE:FF,6,55
← OK
```

## 3. WiFi — Open TCP Socket

The FC41D uses the Quectel TCP/IP AT command set (same family as EG915N).

### Configure (optional)
```
AT+QICFG="recv/mode",<mode>      ← 0=buffer, 1=push (URC)
```

### Open TCP Connection
```
AT+QIOPEN=<connectID>,"TCP","<host>",<port>,<local_port>,<access_mode>
```

| Param       | Type    | Notes                                            |
|-------------|---------|--------------------------------------------------|
| connectID   | 0–4     | Socket index                                     |
| type        | string  | "TCP" or "UDP"                                   |
| host        | string  | IP address or hostname                           |
| port        | integer | Remote port                                      |
| local_port  | integer | Local port (0 = auto)                            |
| access_mode | integer | 0=buffer, 1=push, 2=transparent                 |

**Response (asynchronous URC):**
```
+QIOPEN: <connectID>,<err>
```
err=0 means success; non-zero is error code.

### Send Data
```
AT+QISEND=<connectID>,<length>
```
Wait for `>` prompt, then send raw bytes. Response: `SEND OK`.

### Receive Data (buffer mode, access_mode=0)
```
+QIURC: "recv",<connectID>          ← URC: data available
AT+QIRD=<connectID>,<max_length>    ← read
← +QIRD: <actual_length>
← <data>
← OK
```

### Receive Data (push mode, access_mode=1)
```
+QIURC: "recv",<connectID>,<length>
<data>
```

### Query Socket State
```
AT+QISTATE
← +QISTATE: <connectID>,<type>,<remote_ip>,<remote_port>,<local_port>,<state>
← OK
```

### Close Socket
```
AT+QICLOSE=<connectID>
← OK
```

### Get Error Details
```
AT+QIGETERROR
← +QIGETERROR: <err_code>,"<err_string>"
```

**Full TCP session example:**
```
→ AT+QIOPEN=0,"TCP","192.168.1.100",8080,0,1
← OK
← +QIOPEN: 0,0
→ AT+QISEND=0,12
← >
→ Hello World!
← SEND OK
(incoming data)
← +QIURC: "recv",0,5
← REPLY
→ AT+QICLOSE=0
← OK
```

---

## 4. BLE — Scan Available Devices

### Initialize BLE as Central (scanner)
```
AT+QBLEINIT=1                   ← 1=central/client, 2=peripheral/server
← OK
```

### Start BLE Scan
```
AT+QBLESCAN=1                   ← 1=start, 0=stop
```

**Response (one URC per discovered device):**
```
+QBLESCAN: <addr>,<addr_type>,<rssi>,<adv_data>
```

| Field     | Type    | Notes                                         |
|-----------|---------|-----------------------------------------------|
| addr      | string  | BLE MAC address (e.g. "AA:BB:CC:DD:EE:FF")   |
| addr_type | integer | 0=public, 1=random                            |
| rssi      | integer | Signal strength in dBm (negative value)       |
| adv_data  | hex     | Raw advertising payload (hex-encoded)         |

### Stop BLE Scan
```
AT+QBLESCAN=0
← OK
```

**Full BLE scan sequence:**
```
→ AT+QBLEINIT=1
← OK
→ AT+QBLESCAN=1
← OK
← +QBLESCAN: AA:BB:CC:DD:EE:FF,0,-55,0201060709536D617274
← +QBLESCAN: 11:22:33:44:55:66,1,-72,020106030302180A09
...
→ AT+QBLESCAN=0
← OK
```

## 5. BLE — Send Message via Advertising

BLE advertising broadcasts data to all nearby devices without a connection.
The module must be initialized as **peripheral** (server).

### Initialize BLE as Peripheral
```
AT+QBLEINIT=2                   ← 2=peripheral/server
← OK
```

### Set BLE Device Name
```
AT+QBLENAME=<name>
← OK
```

### Create GATT Service and Characteristics
```
AT+QBLEGATTSSRV=<service_uuid>
← OK
AT+QBLEGATTSCHAR=<char_uuid>
← OK
```
UUIDs are 4-hex-digit (16-bit) or 32-hex-digit (128-bit). Service can only be set once after init.

### Set Advertising Parameters
```
AT+QBLEADVPARAM=<adv_int_min>,<adv_int_max>
← OK
```

| Param        | Range           | Notes                                     |
|--------------|-----------------|-------------------------------------------|
| adv_int_min  | 0x0020–0x4000   | Min interval in units of 0.625 ms         |
| adv_int_max  | 0x0020–0x4000   | Max interval in units of 0.625 ms         |

Common values: `160,160` = 100 ms interval; `1600,1600` = 1 s interval.

### Set Advertising Data
```
AT+QBLEADVDATA=<hex_data>
← OK
```

The `<hex_data>` is a raw BLE advertising payload (up to 31 bytes = 62 hex chars).
Format follows the BLE AD Structure: `<length><type><data>` repeated.

**Common AD Types:**
| Type byte | Meaning                  |
|-----------|--------------------------|
| 0x01      | Flags                    |
| 0x02/0x03 | 16-bit service UUIDs     |
| 0x08/0x09 | Shortened/Complete name  |
| 0xFF      | Manufacturer specific    |

**Example — broadcast name "TFL" + custom data:**
```
Flags:   02 01 06           (3 bytes: len=2, type=Flags, value=0x06=LE General+BR/EDR Not Supported)
Name:    04 09 54 46 4C     (5 bytes: len=4, type=Complete Name, "TFL")
Custom:  05 FF 01 02 AB CD  (6 bytes: len=5, type=Manufacturer Specific, company=0x0201, data=ABCD)
```
→ `AT+QBLEADVDATA=020106040954464C05FF0102ABCD`

### Start / Stop Advertising
```
AT+QBLEADVSTART
← OK

AT+QBLEADVSTOP
← OK
```

**Full advertising sequence (broadcast a message):**
```
→ AT+QBLEINIT=2
← OK
→ AT+QBLENAME=TFL_SENSOR
← OK
→ AT+QBLEADVPARAM=160,160
← OK
→ AT+QBLEADVDATA=0201060B0954464C5F53454E534F5205FF0102AABB
← OK
→ AT+QBLEADVSTART
← OK
```

To change the broadcast message, stop advertising, update data, and restart:
```
→ AT+QBLEADVSTOP
← OK
→ AT+QBLEADVDATA=<new_hex_payload>
← OK
→ AT+QBLEADVSTART
← OK
```

## 6. BLE — Read Message from Device via Advertising

When scanning (central mode), the advertising data from other devices is received
in the `+QBLESCAN` URC. The `adv_data` field contains the raw advertising payload.

### Parsing Advertising Data in C

```c
/* Parse BLE AD structures from raw advertising bytes.
 * advdata: raw bytes (decoded from hex), advdata_len: byte count.
 * Returns pointer to data of requested AD type, sets *out_len. */
const uint8_t *ble_adv_find_type(const uint8_t *advdata, uint8_t advdata_len,
                                  uint8_t ad_type, uint8_t *out_len)
{
    uint8_t i = 0;
    while (i < advdata_len) {
        uint8_t field_len = advdata[i];
        if (field_len == 0 || i + field_len >= advdata_len) break;
        if (advdata[i + 1] == ad_type) {
            *out_len = field_len - 1;
            return &advdata[i + 2];
        }
        i += field_len + 1;
    }
    return NULL;
}
```

**Hex string to bytes conversion:**
```c
static uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

uint8_t hex_to_bytes(const char *hex, uint8_t *out, uint8_t max_len)
{
    uint8_t len = 0;
    while (*hex && *(hex + 1) && len < max_len) {
        out[len++] = (uint8_t)((hex_nibble(*hex) << 4) | hex_nibble(*(hex + 1)));
        hex += 2;
    }
    return len;
}
```

**Reading a specific device's advertising message:**
```
→ AT+QBLEINIT=1
← OK
→ AT+QBLESCAN=1
← OK
← +QBLESCAN: AA:BB:CC:DD:EE:FF,0,-55,0201060B0954464C5F53454E534F5205FF0102AABB
```

Parse `+QBLESCAN` URC:
1. Extract `addr` field → match against target device MAC
2. Decode `adv_data` hex → bytes
3. Walk AD structures with `ble_adv_find_type()`
4. Extract `0xFF` (Manufacturer Specific) for custom payload, or `0x09` for device name

---

## BLE GATT Connected Mode (alternative to advertising)

For bidirectional data exchange via a BLE connection (not broadcast):

### As Central — Connect and Read/Write
```
AT+QBLEINIT=1
← OK
AT+QBLESCAN=1
(find target device)
AT+QBLESCAN=0
AT+QBLECONN=<addr>
← +QBLECONN: <conn_id>,<addr>
AT+QBLEGATTCWR=<data>              ← write to characteristic
```

### As Peripheral — Notify Connected Central
```
AT+QBLEINIT=2
← OK
AT+QBLEGATTSSRV=FFF1               ← create service
AT+QBLEGATTSCHAR=FFF2              ← RX characteristic
AT+QBLEGATTSCHAR=FFF3              ← TX characteristic (notify)
AT+QBLEADVPARAM=160,160
AT+QBLEADVSTART
(wait for central to connect — URC: +QBLECONN: ...)
AT+QBLEGATTSNTFY=FFF3,<data>       ← send notification to connected central
```

### Transparent Mode (serial ↔ BLE bridge)
```
AT+QBLECFGMTU=512                  ← set MTU (requires firmware ≥ FC41DAAR03A07)
AT+QBLETRANMODE=FFF2               ← enable transparent mode on characteristic
(all UART data now forwarded to/from BLE characteristic FFF2)
+++                                 ← exit transparent mode (send as raw text)
```

---

## HTTP Commands (for REST APIs over WiFi)

```
AT+QHTTPCFG="url","https://<host>/<path>"
AT+QHTTPCFG="header","Content-Type","application/json"
AT+QHTTPCFG="header","Authorization","Bearer <token>"
AT+QHTTPCFG="sslctxid",0                   ← bind SSL context for HTTPS
AT+QHTTPPOST=<body_length>,<input_timeout>,<response_timeout>
← CONNECT
(send JSON body)
← OK
← +QHTTPPOST: <status_code>,<content_type>,<content_length>
AT+QHTTPREAD=<timeout>
← CONNECT
← <response_body>
← OK
← +QHTTPREAD: 0
```

## SSL/TLS Configuration

```
AT+QSSLCERT="CA",<ctx_id>,<cert_length>    ← load CA certificate (then paste PEM)
AT+QSSLCFG="version",<ctx_id>,3            ← TLS 1.2
AT+QSSLCFG="ciphersuite",<ctx_id>,0xFFFF   ← all cipher suites
AT+QSSLCFG="verify",<ctx_id>,0             ← 0=no verify, 1=verify server cert
```

## Key URCs Summary

| URC                              | Meaning                                | When active           |
|----------------------------------|----------------------------------------|-----------------------|
| `+QWSCAN: ...`                  | WiFi scan result (one per AP)          | After AT+QWSCAN       |
| `+QGETWIFISTATE: ...`           | WiFi connection status                 | After AT+QGETWIFISTATE|
| `+QIOPEN: <cid>,<err>`          | TCP/UDP socket opened                  | After AT+QIOPEN       |
| `+QIURC: "recv",<cid>`          | TCP data available (buffer mode)       | Socket open, mode 0   |
| `+QIURC: "recv",<cid>,<len>`    | TCP data arrived (push mode)           | Socket open, mode 1   |
| `+QIURC: "closed",<cid>`        | Remote closed socket                   | Socket was open       |
| `+QBLESCAN: <addr>,...`          | BLE device discovered                  | After AT+QBLESCAN=1   |
| `+QBLECONN: <cid>,<addr>`       | BLE device connected                   | After AT+QBLECONN     |
| `+QBLEDISCONN: <cid>,<addr>`    | BLE device disconnected                | Connection was active  |
| `+QHTTPPOST: <code>,...`        | HTTP POST response received            | After AT+QHTTPPOST    |
| `+QHTTPREAD: 0`                 | HTTP read complete                     | After AT+QHTTPREAD    |

## Common Gotchas

- **Echo must be off** (`ATE0`) before parsing responses — same as EG915N.
- **RESETN is active LOW, open drain** — drive LOW to reset, release to HIGH (or HiZ). Do not drive HIGH if open-drain configured.
- **WiFi connect takes 3–5 s** for DHCP to complete. Poll `AT+QGETWIFISTATE` until non-DISCONNECT status.
- **RSSI from AT+QWSCAN is offset by +100** — a value of 55 means −45 dBm.
- **AT+QBLEINIT must be called before any BLE command.** Role 1=central, 2=peripheral. Changing role requires `AT+QBLEINIT=0` (deinit) first.
- **GATT services can only be set once after AT+QBLEINIT=2.** To change services, deinit and reinit.
- **BLE advertising payload max 31 bytes.** Encode carefully — each AD structure has 1 byte length + 1 byte type + N bytes data.
- **AT+QBLETRANMODE and AT+QBLECFGMTU require firmware ≥ FC41DAAR03A07.** Check with `AT+QVERSION`.
- **Socket `access_mode=2` (transparent)** means UART becomes a raw TCP pipe — `+++` to escape back to AT mode.
- **WiFi and BLE can run simultaneously** — the FC41D supports concurrent WiFi STA + BLE peripheral/central.
- **SPI4 interface** is available for high-throughput data (12 MHz) but AT commands always go through UART9.
- **Module power** — both WiFi and BLE share the same power enable pin (PD5). Resetting affects both.
