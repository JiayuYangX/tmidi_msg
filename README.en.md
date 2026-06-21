# TMIDI Messages PLUGIN

SSP PLUGIN/2.0 plugin that bridges TMIDI Player to desktop ghost characters via DDE.

When TMIDI Player starts playing a MIDI file, the plugin detects it and injects Sakura Script into SSP, making the ghost character announce track information in real time.

## Background

TMIDI Player gained a built-in SSTP client in **ver.3.8.1** (2001), which could send song titles to Ukagaka/SSP ghosts on playback start. However, the implementation uses synchronous (blocking) Winsock — if the connection times out, playback hangs. As SSP and ghost protocols have evolved over 20+ years, TMIDI's built-in SSTP no longer works in modern environments.

This plugin replaces the broken TCP/SSTP pathway with a **DDE + PLUGIN/2.0** approach:
- **DDE** communicates with TMIDI to query playback status and track information (TMIDI still exposes a full DDE service to this day)
- **PLUGIN/2.0** injects scripts directly into SSP — no network connection, no timeout blocking

## Features

- **Zero-config DDE detection** — automatically finds all running TMIDI Player instances
- **Multi-instance support** — tracks all running TMIDI Player instances simultaneously
- **No false triggers on resume** — triggers only on initial play or after explicit stop
- **Auto-reconnect** — silently reconnects if TMIDI is restarted
- **Customizable template** — edit `sstp_sample.txt` to change what the ghost says

## Requirements

- **TMIDI Player** (DDE service name: `TMIDI`)
- **SSP** (Sakura Script Player) with a ghost character loaded
- **Windows** (DDE runs on the system ANSI codepage)

## Installation

Download the latest NAR file from Releases.

- **Method 1**: Open SSP, drag the NAR file in, click confirm to overwrite

- **Method 2**: Set SSP as the default program for `.nar` files, double-click to run, click confirm to overwrite

- **Method 3**: Rename the `.nar` extension to `.zip`, extract and overwrite into `SSP/plugin/tmidi_msg`

```
SSP/
  plugin/
    tmidi_msg/
      descript.txt
      install.txt
      message.chinese.txt
      message.english.txt
      message.japanese.txt
      sstp_sample.txt
      tmidi_msg.dll
```

## Template Configuration

Edit `sstp_sample.txt` to customize the message. The format is similar to TMIDI Player's original `sstp_sample.txt` (**note: `$target` and `$module` are not supported**).

The template supports WRD type detection, auto-selected based on the playing file:

| Section      | Trigger                               |
|--------------|---------------------------------------|
| `#MIMPIWRD`  | Companion `.wrd` or `.dv` file exists |
| `#SherryWRD` | Companion `.sry` file exists          |
| `#NeoWRD`    | Playing file has `.neo` extension     |
| `#NoWRD`     | No WRD detected (fallback)            |

### Template variables

| Variable  | Replaced with |
|-----------|---------------|
| `$title`  | Song title    |
| `$format` | File Format   |

## How It Works

```
TMIDI Player ──DDE──▶ tmidi_msg.dll ──PLUGIN/2.0──▶ SSP ──Sakura Script──▶ Ghost
```

- Uses **DDEML** (`APPCLASS_STANDARD`) to establish persistent conversations
- Polls `getstatus` every second via `OnSecondChange`
- On status `"play"` with changed filename, fetches title via `gettitle` and injects script
- DDE strings handled in `CP_WINUNICODE`, data in `CF_TEXT` via system ANSI codepage

Known issue: If the same file is stopped and restarted within the plugin's timer interval (minimum 1 second), or reopened without stopping, the filename change won't be detected, so no dialogue will be triggered. (I don't suppose anyone would actually do this though.)

## Build

```bash
cl /LD /O2 /MT /Fe:tmidi_msg.dll tmidi_msg.cpp user32.lib shell32.lib
```

Requires MSVC (`cl.exe`). SSP is a 32-bit application, so use the x86 toolchain (e.g. run `vcvars32.bat`). Produces a single `tmidi_msg.dll`.

## License

See `readme.txt` for the original TMIDI Player license. Plugin code is free to use and modify.
