One Node is a simple localized file transfer system fixed between two devices. More progress is coming, and contributors are always welcome.

<div align="center">

<img src="resources/icon.png" alt="One Node Logo" width="120" height="120" />

# One Node

**Instant peer-to-peer file transfer between your Linux desktop and Android phone.**

No cloud. No cables. No accounts. Just your local network.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Desktop-Linux-orange.svg)]()
[![Platform](https://img.shields.io/badge/Mobile-Android-green.svg)]()
[![Built with](https://img.shields.io/badge/Built%20with-Qt6%20%2B%20Kotlin-purple.svg)]()
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)]()

[Getting Started](#-installation) · [How It Works](#-how-it-works) · [Contributing](#-contributing) · [Roadmap](#-roadmap)

</div>

---

## What is One Node?

One Node is a lightweight, open-source file transfer app that lets you instantly send files between your Linux machine and Android phone over local Wi-Fi — with no internet, no cloud storage, and no third-party servers involved.

Inspired by AirDrop, built for Linux.

Drop a file onto the app. It's on your phone in seconds.

---

## Features

- **Zero configuration** — devices discover each other automatically via mDNS. No IP addresses to type.
- **Simple pairing** — link devices once using a 6-digit code. Stays linked until you manually unlink.
- **Drag and drop** — drag any file onto the One Node window to send it instantly.
- **Bidirectional** — send files from desktop to phone and phone to desktop.
- **No internet required** — everything happens directly between your devices over LAN.
- **Real-time notifications** — get notified the moment a file arrives on either side.
- **Lightweight** — runs quietly in your system tray, using minimal resources.

---

## How It Works

### Discovery
The Linux app advertises itself on the local network using **Avahi (mDNS)**. The Android app scans for this advertisement using Android's **NSD (Network Service Discovery)** and automatically resolves the desktop's IP — no manual entry needed.

### Pairing
The Android app connects to the desktop on **port 45678** and sends the 6-digit code. If it matches, the desktop generates a unique pairing token and sends it back. Both sides save the token and IP to persistent storage. Pairing happens once.

### File Transfer
Files are streamed directly between devices over **TCP** using a simple binary protocol:

```
[ 4 bytes ] filename length  (big-endian int32)
[ N bytes ] filename         (UTF-8)
[ 8 bytes ] file size        (big-endian int64)
[ M bytes ] raw file data
```

Desktop → Phone uses port **45679**.
Phone → Desktop uses port **45680**.

No compression, no overhead, no middlemen. Just raw bytes over your local network.

### Architecture

```
┌─────────────────────────────────────────┐
│           Linux Desktop (Qt6 / C++)     │
│                                         │
│  mainwindow        ← UI + orchestration │
│  pairingserver     ← TCP server :45678  │
│  filetransfer      ← TCP client :45679  │
│  mdnsadvertiser    ← Avahi mDNS beacon  │
│  [desktopReceiver] ← TCP server :45680  │
└──────────────┬──────────────────────────┘
               │
               │  Local Wi-Fi (LAN)
               │  No internet required
               │
┌──────────────▼──────────────────────────┐
│           Android App (Kotlin)          │
│                                         │
│  MainActivity      ← UI + NSD discovery │
│  ReceiverService   ← TCP server :45679  │
│  [fileSender]      ← TCP client :45680  │
└─────────────────────────────────────────┘
```

---

## Installation

### Android
Download the latest APK from the [Releases](https://github.com/dwarageshc7203/One-Node/releases) page and install it on your phone.

> Enable "Install from unknown sources" in your Android settings if prompted.

### Linux — AppImage (Recommended)
No dependencies required. Download and run:

```bash
chmod +x One-Node-x86_64.AppImage
./One-Node-x86_64.AppImage
```

### Linux — Build from Source

**Requirements:**
- Qt 6 (Core, Widgets, Network, Gui)
- CMake 3.16+
- GCC / G++ with C++17 support
- Avahi client library

**Install dependencies (Ubuntu / Debian):**
```bash
sudo apt update
sudo apt install qt6-base-dev cmake g++ ninja-build \
                 avahi-daemon libavahi-client-dev pkg-config
```

**Clone and build:**
```bash
git clone https://github.com/dwarageshc7203/One-Node.git
cd One-Node

mkdir build && cd build
cmake .. -G Ninja
ninja
```

**Run:**
```bash
./onenode
```

---

## Project Structure

```
One-Node/
├── main.cpp                 ← App entry point
├── mainwindow.h/.cpp        ← Main window, UI, drag & drop, tray icon
├── pairingserver.h/.cpp     ← TCP pairing server (port 45678)
├── filetransfer.h/.cpp      ← Outbound file transfer engine
├── mdnsadvertiser.h/.cpp    ← Avahi mDNS advertisement
├── resources.qrc            ← Embedded assets
├── icon.png                 ← App icon (512×512)
├── CMakeLists.txt           ← Build configuration
├── onenode.desktop          ← Linux desktop launcher entry
└── android/
    └── app/src/main/
        ├── java/com/onenode/dropin/
        │   ├── MainActivity.kt       ← Pairing UI + NSD discovery
        │   └── ReceiverService.kt    ← Background file receiver
        ├── res/layout/
        │   └── activity_main.xml     ← Android UI layout
        └── AndroidManifest.xml       ← Permissions + service declarations
```

---

## Ports Reference

| Port  | Direction          | Purpose                  |
|-------|--------------------|--------------------------|
| 45678 | Android → Linux    | Pairing handshake (TCP)  |
| 45679 | Linux → Android    | File transfer (TCP)      |
| 45680 | Android → Linux    | File transfer (TCP)      |

If you have a firewall enabled on Linux, allow these ports:
```bash
sudo ufw allow 45678/tcp
sudo ufw allow 45679/tcp
sudo ufw allow 45680/tcp
```

---

## Contributing

One Node is actively developed and contributions of all kinds are welcome —
bug fixes, new features, documentation improvements, UI polish, or ports to
new platforms. If you use the app and want to make it better, this is your repo.

### Getting Started

```bash
# Fork the repo on GitHub, then:
git clone https://github.com/YOUR_USERNAME/One-Node.git
cd One-Node
git checkout -b feature/your-feature-name
```

Build and run the Linux app:
```bash
mkdir build && cd build && cmake .. -G Ninja && ninja
./onenode
```

Open the `android/` folder in Android Studio for the Android app.

### Good First Issues

If you're new to the codebase, these are well-scoped starting points:

| Task | File to touch | Difficulty |
|------|--------------|------------|
| Transfer progress bar on Linux | `mainwindow.cpp` — connect existing `transferProgress(int)` signal to a `QProgressBar` | Easy |
| Tap notification to open file on Android | `ReceiverService.kt` — add `PendingIntent` with `FileProvider` | Easy |
| "Open received folder" button on Linux | `mainwindow.cpp` — use `QDesktopServices::openUrl()` | Easy |
| Dark mode support on Linux | `mainwindow.cpp` — replace hardcoded hex colors with `QPalette` | Medium |
| File integrity check (SHA256) | `filetransfer.cpp` + `ReceiverService.kt` — compute and verify checksum after transfer | Medium |
| Multi-file queue | `dropEvent()` in `mainwindow.cpp` — send files sequentially | Medium |
| Windows port | Replace `mdnsadvertiser.cpp` with Bonjour SDK equivalent | Hard |
| End-to-end encryption | Wrap TCP connections with TLS using Qt's `QSslSocket` | Hard |

### Submitting a Pull Request

1. Make your changes on a feature branch
2. Test the full flow — pair, send desktop→phone, send phone→desktop
3. Keep commits focused and descriptive
4. Open a PR with a clear description of what changed and why

### Reporting Bugs

Open a [GitHub Issue](https://github.com/dwarageshc7203/One-Node/issues) and include:
- What you were doing
- What you expected
- What actually happened
- Your Linux distro + version or Android version
- Any error messages or Logcat output

### Coding Conventions

**C++ (Linux):**
- 4-space indentation
- Prefer Qt types (`QString`, `QByteArray`) over STL equivalents
- All networking must be async — never block the main thread
- Handle all socket errors explicitly

**Kotlin (Android):**
- All IO on `Dispatchers.IO`
- All UI updates on `Dispatchers.Main` or `runOnUiThread`
- Log with `Log.d("One-Node", "...")` for traceability

---

## Roadmap

- [ ] Always-on-top drop zone overlay (Linux)
- [ ] Windows and macOS desktop support
- [ ] Transfer resume on connection failure
- [ ] End-to-end encryption (TLS)
- [ ] File integrity verification (SHA256)
- [ ] Send from Android share sheet
- [ ] Transfer history log
- [ ] Auto-reconnect when IP changes (without re-pairing)
- [ ] Multiple simultaneous transfers

Have an idea not on this list? [Open an issue](https://github.com/dwarageshc7203/One-Node/issues) and let's discuss it.

---

## Limitations

- Works on the same local network only — no internet, no cross-network transfer
- No encryption on transfer data yet (planned)
- No transfer resume — if connection drops mid-file, resend required
- Linux only on desktop — Windows/macOS port is on the roadmap

---

## License

MIT License — see [LICENSE](LICENSE) for details.

You're free to use, modify, and distribute this project. Attribution appreciated but not required.

---

## Author

Built by [Dwaragesh](https://github.com/dwarageshc7203)

Inspired by AirDrop. Built for Linux.

---

<div align="center">

If One Node is useful to you, consider giving it a ⭐ on GitHub.
It helps more people find the project.

</div>