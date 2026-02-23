# Claudium3DS

A Claude AI chat client for the Nintendo 3DS, built with devkitARM and libctru.

![Claudium3DS Icon](resources/icon.png)

---

## Features

- Chat with Claude AI directly on your Nintendo 3DS
- Works on **Old 3DS** (64 MB RAM) and **New 3DS**
- System software keyboard via `swkbdInit` (no external keyboard needed)
- Scrollable conversation history (D-pad / Circle Pad)
- Touch-screen buttons: **Send**, **Keyboard**, **Settings**, **New Chat**, **Scroll Up/Down**
- API key stored securely on SD card (`sdmc:/3ds/Claudium3DS/config.ini`)
- Configurable model, system prompt, and max tokens
- Save/load conversations to SD card (`sdmc:/3ds/Claudium3DS/chats/`)
- Builds as both `.3dsx` (Homebrew Launcher) and `.cia` (installable title)

---

## Screenshots

_(Screenshots will be added after hardware testing.)_

---

## Requirements

### Hardware
- Nintendo 3DS / 3DS XL / 2DS / New 3DS / New 3DS XL / New 2DS XL
- SD card with at least a few MB free
- Wi-Fi connection

### Build Tools
- [devkitPro](https://devkitpro.org/) with the following pacman packages:
  ```
  3ds-dev
  3ds-curl
  3ds-mbedtls
  ```
- `bannertool` (for CIA builds)
- `makerom` / `3dsxtool` (for CIA builds)

Install with:
```bash
dkp-pacman -S 3ds-dev 3ds-curl 3ds-mbedtls
```

---

## Building

### .3dsx (Homebrew Launcher)

```bash
make
```

Output: `Claudium3DS.3dsx` and `Claudium3DS.smdh`

### .cia (Installable Title)

```bash
make cia
```

Output: `Claudium3DS.cia`

Requires `bannertool` and `makerom` to be in your `PATH`.

### Clean

```bash
make clean
```

---

## Usage

1. Copy `Claudium3DS.3dsx` to `/3ds/Claudium3DS/Claudium3DS.3dsx` on your SD card, or install the `.cia`.
2. Launch from the Homebrew Launcher (or Home Menu for CIA).
3. Go to **Settings** and enter your Anthropic API key.
4. Return to chat, tap **Keyboard**, type your message, and tap **Send**.

### Controls

| Control | Action |
|---------|--------|
| Touch **Keyboard** | Open system keyboard |
| Touch **Send** | Send the current message |
| Touch **Settings** | Open settings screen |
| Touch **New Chat** | Clear conversation |
| Touch **Scrl^** / **ScrlV** | Scroll chat up/down |
| D-pad Up / Down | Scroll chat history |
| A | Open keyboard (chat screen) / Confirm (settings) |
| B | Send (if text entered) / Back (settings) |
| Start | Exit app |

---

## Configuration

Settings are saved to `sdmc:/3ds/Claudium3DS/config.ini`:

```ini
[Claudium3DS]
api_key=sk-ant-...
model=claude-sonnet-4-20250514
system_prompt=You are a helpful AI assistant running on a Nintendo 3DS.
max_tokens=1024
theme=0
session_token=
```

### Available Models

| Model | Description |
|-------|-------------|
| `claude-sonnet-4-20250514` | Default — balanced performance |
| `claude-haiku-4-20250414` | Faster, lower cost |

---

## Project Structure

```
Claudium3DS/
├── Makefile
├── README.md
├── LICENSE
├── source/
│   ├── main.c          — Entry point & main loop
│   ├── ui.c / ui.h     — Screen rendering (console-based)
│   ├── api.c / api.h   — Anthropic API via libcurl
│   ├── config.c / .h   — Settings load/save (SD card INI)
│   ├── chat.c / .h     — Conversation history management
│   ├── input.c / .h    — Keyboard & touch input
│   └── cJSON.c / .h    — Lightweight JSON parser
├── resources/
│   ├── icon.png        — 48×48 app icon
│   ├── banner.png      — 256×128 CIA banner
│   └── audio.wav       — CIA jingle
└── meta/
    └── app.rsf         — CIA metadata
```

---

## Technical Notes

- **Networking**: Uses `socInit` + libcurl with mbedTLS for HTTPS
- **JSON**: Bundled cJSON (no external dependency)
- **Memory**: Limits API response buffer to 64 KB to protect Old 3DS RAM
- **Sliding window**: Automatically trims conversation when it exceeds `MAX_MESSAGES` (64)
- **Voice input**: Not implemented — the 3DS microphone cannot perform on-device STT

---

## Credits

- [cJSON](https://github.com/DaveGamble/cJSON) — Dave Gamble et al. (MIT)
- [libctru](https://github.com/devkitPro/libctru) — devkitPro team
- [devkitARM](https://devkitpro.org/) — devkitPro

---

## License

MIT — see [LICENSE](LICENSE).
