<div align="center">

<img src="assets/SocketLoader Logo.png" alt="CustomStickerImporter Logo"  width="560" />  </br>

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/PCBSPartCreator/PCBS2.SocketLoader/releases)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/PCBSPartCreator/PCBS2.SocketLoader/blob/main/LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)](https://github.com/PCBSPartCreator/PCBS2.SocketLoader/blob/main)
[![Game](https://img.shields.io/badge/game-PC%20Building%20Simulator%202-orange.svg)](https://www.pcbuildingsim.com/)
[![Loader](https://img.shields.io/badge/loader-PCBS2.XPL-8A2BE2.svg)](https://github.com/PCBSPartCreator/PCBS2.XPL)
[![C++](https://img.shields.io/badge/C%2B%2B-14-00599C.svg?logo=cplusplus)](https://github.com/PCBSPartCreator/PCBS2.SocketLoader/blob/main)

A [PCBS2.XPL](https://github.com/PCBSPartCreator/PCBS2.XPL) addon that registers custom CPU / mainboard sockets in PC Building Simulator 2.

Define a socket with an ID and a Display name in a small `.ini` file, and the game treats it as a first-class socket: it has a proper name, custom CPUs and motherboards can use it, and it shows up correctly in the parts-shop **socket filter** and matches there. This makes it possible to ship fictional or next-gen platforms (a new AMD/Intel socket) as fully buildable parts.

[✨ Features](#-features) • [🔧 Requirements](#-requirements) • [📦 Installation](#-installation) • [⚙️ Configuration](#️-configuration) • [🛠 Building from Source](#-building-from-source) • [🔬 How It Works](#-how-it-works)  [📖 Usage Notes](#-usage-notes) • [🐛 Troubleshooting](#-troubleshooting) • [© Credits](#-credits)

</div>

## ✨ Features

✅ **Custom CPU / mainboard sockets**:  
Register new sockets by ID and name. The game shows the name everywhere a socket name appears.

✅ **Shop filter fix**:  
Custom sockets appear in the parts-shop socket filter dropdown *and* match against it.

✅ **Simple config**:  
One line per socket in `sockets.ini` (`<Id> = <DisplayName>`); comment a line out to disable it.

✅ **No loader of its own**  
Runs as a native PCBS2.XPL addon and shares the loader's hook engine.

✅ **Version-safe hooking**:  
Game methods are resolved by name at runtime, not by fixed offsets, so it survives most game updates.

✅ **Light on frame time**:  
Setup runs once at startup, then the addon goes idle - no per-frame work.

✅ **Detailed logging**:  
Resolve and patch status written to `PCBS2.SocketLoader.log` in the addon folder.

## 🔧 Requirements

- **Game**: [PC Building Simulator 2](https://store.epicgames.com/p/pc-building-simulator-2)
- **Loader**: [PCBS2.XPL](https://github.com/PCBSPartCreator/PCBS2.XPL) installed and working
- **OS**: Windows 10 / 11 (x64)
- **Runtime**: [Visual C++ Redistributable 2015-2026](https://visualstudio.microsoft.com/de/downloads/) (usually already installed)

## 📦 Installation

### Step 1: Install PCBS2.XPL

SocketLoader is a PCBS2.XPL addon, so the loader has to be installed first. Follow the [PCBS2.XPL installation steps](https://github.com/PCBSPartCreator/PCBS2.XPL#-installation).  
In short, place its `version.dll` next to `PCBS2.exe` and launch the game once so it creates the `addons/` folder.

### Step 2: Install the addon

1. Download the latest release.
2. Copy the DLL so it sits at:
   ```
   ...\PCBuildingSimulator2\addons\PCBS2.SocketLoader\PCBS2.SocketLoader.dll
   ```
   The folder name matters - the addon reads its config from its own `addons/PCBS2.SocketLoader/` folder.
3. Launch the game once. The addon creates `PCBS2.SocketLoader.ini`, `sockets.ini` and `PCBS2.SocketLoader.log` in that folder.

### Step 3: Define your sockets

Open `addons\PCBS2.SocketLoader\sockets.ini` and add one line per socket under `[Sockets]` (see [Configuration](#️-configuration)). Restart the game to apply changes.

### Verification

The addon writes `PCBS2.SocketLoader.log` inside its folder. A healthy run looks like this:

```
==================================================
 PCBS2.SocketLoader v1.0.0
==================================================
[i] Initialize
[i] Addon directory: ...\addons\PCBS2.SocketLoader\
[i] Config: EnableSockets=true PatchEnabled=true
[i] Resolve: class CpuSocketExt ok
[i] Patch: s_names[100] = "AM6"
[i] Patch: s_used[100] = true
[i] Patch: custom sockets applied successfully
```

## ⚙️ Configuration

Two files live in `addons\PCBS2.SocketLoader\`, both created automatically on first launch. Existing files are never overwritten, so your edits are safe across updates.

### sockets.ini

This is where you define sockets. One line per socket under the `[Sockets]` section:

```ini
[Sockets]
100 = AM6
101 = LGA 2027
```

- **ID**: A number from **100 to 999**. IDs below 100 are reserved for the game's built-in sockets - stay at 100+ to avoid colliding with them. Give every custom socket a unique ID.
- **DisplayName**: The name shown in the game (shop, tooltips, filter).
- **Disable a socket**: Put a `;` in front of its line to comment it out.

The ID you choose here is what a custom CPU / motherboard part references as its socket - see [Note 2](#note-2-making-parts-that-use-a-custom-socket).

### PCBS2.SocketLoader.ini

Behaviour switches. The defaults are what you want for normal use; the rest are mainly for troubleshooting.

```ini
[General]
EnableSockets=true      ; master switch for the whole addon
LogConfig=false         ; log the parsed config back out

[Runtime]
ResolveEnabled=true     ; resolve the game's socket classes (required for anything to work)
PatchEnabled=true       ; register the custom sockets (names + usable flags)
HookFilter=true         ; fix the parts-shop socket filter for custom sockets
LogResolveSteps=false   ; verbose resolve logging
DebugFilter=false       ; verbose filter-hook diagnostics
```

## 🛠 Building from Source

### Requirements

- **[Visual Studio](https://visualstudio.microsoft.com/de/downloads/)** with the *Desktop development with C++* workload
- **[Windows SDK 10](https://learn.microsoft.com/de-de/windows/apps/windows-sdk/downloads)**
- **C++ 14 or newer**

### Steps

1. Clone the repository.
2. Open the solution in Visual Studio.
3. Set the configuration to **Release / x64**.
4. Build. The output is `PCBS2.SocketLoader.dll`.

No third-party libraries are vendored and there is **no MinHook dependency**. All hooking goes through PCBS2.XPL's exported `XPL_CreateHook`, resolved at runtime with `GetProcAddress` on `version.dll` - so there is no import library to link against PCBS2.XPL. The project targets x64 only, matching the game's 64-bit IL2CPP build.

### Project Layout

| File               | Purpose                                                                     |
| ------------------ | --------------------------------------------------------------------------- |
| `dllmain.cpp`      | XPL exports (`XPL_GetName` / `GetVersion` / `Initialize` / `Tick` / `Shutdown`) |
| `Addon.*`          | Addon lifecycle and the one-time setup driven from `Tick`                   |
| `Config.*` / `ini.*` | `sockets.ini` / `PCBS2.SocketLoader.ini` parsing and default generation   |
| `Paths.*`          | Addon folder / file path resolution                                        |
| `Runtime.*` / `IL2cppAPI.*` | IL2CPP runtime access and class/field lookup                       |
| `SocketResolver.*` | Resolves the game's socket classes and fields by name                      |
| `SocketPatcher.*`  | Extends the socket name / usable arrays with the custom sockets            |
| `FilterHook.*`     | The parts-shop socket-filter hook (installed via `XPL_CreateHook`)         |
| `HookManager.*`    | Thin wrapper over `XPL_CreateHook`                                          |
| `Log.h`            | Win32-backed logger                                                        |

## 🔬 How It Works

PC Building Simulator 2 is a 64-bit Unity IL2CPP game whose sockets live in fixed-size internal tables. SocketLoader loads as a PCBS2.XPL addon and, at startup, extends those tables with your custom entries and fixes the one place the game would otherwise ignore them.

1. **Load** - PCBS2.XPL calls `XPL_Initialize`. The addon resolves its paths, creates the config files if missing, and reads `sockets.ini`.
2. **Resolve** - On the first ticks it locates the game's socket classes and the arrays that hold socket names and usable flags, all by name so a game update rarely breaks it.
3. **Register** - It rebuilds those arrays to include the custom sockets: each socket's ID gets its display name, and its "usable" flag is set so CPUs and motherboards using that ID can be installed.
4. **Fix the filter** - It installs a single hook (through `XPL_CreateHook`) on the parts-shop socket filter. For parts that use a custom socket, the hook returns the custom socket name, so the socket appears in the filter dropdown and parts match against it correctly.
5. **Go idle** - Once registration and the hook are in place, there is nothing left to do, so the addon settles and stops doing per-tick work.

```
XPL_Initialize
     │  read sockets.ini + config
     ▼
Resolve (first ticks)
     │  find socket classes + name/usable arrays by name
     ▼
Register
     │  extend arrays: ID -> name, mark usable
     ▼
Filter hook (via XPL_CreateHook)
     │  return custom name for custom-socket parts
     ▼
Idle
```

## 📖 Usage Notes

### Note 1: Defining a socket

Add a line under `[Sockets]` in `sockets.ini`: `<Id> = <DisplayName>`, with the ID between 100 and 999. Restart the game. The socket now exists in-game with that name. Comment the line out with `;` to remove it again.

### Note 2: Making parts that use a custom socket

SocketLoader registers the *socket* - it does not create CPUs or motherboards. To get buildable parts on a custom socket, create custom CPU / motherboard parts whose socket value is the ID you defined here, using [PCBS2 Part Creator](https://www.nexusmods.com/pcbuildingsimulator2/mods/102) and [PCBS2.XPL](https://github.com/PCBSPartCreator/PCBS2.XPL) to load them. A CPU and a motherboard that share the same custom socket ID will fit together, just like a stock platform.

### Note 3: Cooler compatibility

A custom socket is its own platform, so it needs its own compatible coolers - it does not inherit another socket's cooler list. Provide coolers that list the custom socket if you want cooling options for it.

### Note 4: Removing the addon and save safety

Saved builds can reference custom parts and sockets. If you remove SocketLoader (or the custom parts) later, use [PCBS2.XPL](https://github.com/PCBSPartCreator/PCBS2.XPL)'s SaveFix - or the dedicated PCBS2 Save Fixer - so saves that referenced the now-missing content still load.

### Note 5: Debug switches

`LogResolveSteps`, `DebugFilter` and `LogConfig` are off by default and only add log noise; leave them off unless you are diagnosing a problem.

## 🐛 Troubleshooting

Most issues can be read straight from `PCBS2.SocketLoader.log` in the addon folder. Check it first.

### The addon doesn't load

**Symptoms**: No `PCBS2.SocketLoader.log` appears, or PCBS2.XPL's own log doesn't list the addon.

**Solutions**:

- Confirm the DLL is at `addons\PCBS2.SocketLoader\PCBS2.SocketLoader.dll` (exact folder name).
- Confirm PCBS2.XPL itself is installed and working, and that its log lists `PCBS2.SocketLoader` under loaded addons.
- If the log mentions the XPL hook service is missing, update PCBS2.XPL - this addon needs a loader version that exports `XPL_CreateHook`.

### A socket doesn't show up

**Symptoms**: The addon loads but a socket isn't in-game.

**Solutions**:

- Check `sockets.ini`: the line must be under `[Sockets]`, use an ID between 100 and 999, and not be commented out with `;`.
- Look for `Patch: s_names[<ID>] = "..."` and `Patch: custom sockets applied successfully` in the log. `Patch: no enabled custom sockets, nothing to patch` means no valid, enabled entries were found.
- Make sure `EnableSockets` and `PatchEnabled` are `true` in `PCBS2.SocketLoader.ini`.

### The socket is missing from the shop filter

**Symptoms**: The socket works when building, but doesn't appear in / match the parts-shop socket filter.

**Solutions**:

- Confirm `HookFilter=true` in `PCBS2.SocketLoader.ini`.
- Check the log for the filter hook being installed. If it isn't, set `DebugFilter=true` temporarily and re-check the log for details, then report it.

### "Resolve failed" / class or field not found

**Symptoms**: Log lines about a socket class or field not being resolved, and no patching happens.

**Solutions**:

- A game update likely changed the internal layout. Watch the repository for an updated release, or open an [issue](https://github.com/PCBSPartCreator/PCBS2.SocketLoader/issues) with the log (set `LogResolveSteps=true` first for more detail).
- Confirm you're running the official Epic Games build of the game.

## © Credits

- **[PCBS2.XPL](https://github.com/PCBSPartCreator/PCBS2.XPL)** - the mod loader that hosts this addon and provides the shared `XPL_CreateHook` hooking service
- **[Jelly's Socket Creator](https://github.com/ZeOs360/JellysSocketCreator)** - the original custom-socket mod that inspired this one

---

<div align="center">

**Made with ❤️ by anonymus637**  
<sub>Parts of this project and its documentation were created with the help of AI.</sub>
</div>
