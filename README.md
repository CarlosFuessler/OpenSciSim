# OpenSciSim

Interactive science simulator in C using [Raylib](https://www.raylib.com/).
Explore math, physics, and chemistry with visual demos.

![C](https://img.shields.io/badge/C11-00599C?style=flat&logo=c&logoColor=white)
![Raylib](https://img.shields.io/badge/Raylib-black?style=flat&logo=raylib&logoColor=white)

## Features

- **Math:** CAS plotter (2D/3D), calculator, parametric/polar sims
- **Physics:** atomic models, pendulum + projectile mechanics
- **Chemistry:** periodic table, molecule viewer, reaction and pH lab

## Prerequisites

### Using Nix (recommended)

```bash
nix develop
```

### Manual

- A C11 compiler (gcc/clang)
- [Raylib](https://www.raylib.com/) (≥ 5.0) via `pkg-config`
- GNU Make

## Build & Run

```bash
# Native (inside nix develop or with raylib installed)
make
./openscisim

# Or in one step
make run

# Clean build artifacts
make clean
```

## Controls

| Action | Key / Mouse |
|---|---|
| Navigate topics | Click topic cards on start screen |
| Switch tabs | Click tab bar within a topic |
| Back to start | `ESC` or click `< Home` |
| 2D Plot zoom | Scroll wheel |
| 2D Plot pan | Click & drag |
| 3D Orbit | Click & drag |
| 3D Zoom | Scroll wheel |
| 3D Reset view | `Home` key |

## Web (WASM)

Build with [Emscripten](https://emscripten.org/) and run in any modern browser.

### 1. Install Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 2. Build Raylib for Web

```bash
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_WEB
```

This creates `libraylib.web.a` (or `libraylib.a` on older raylib versions) in the same directory.

### 3. Build OpenSciSim for Web

```bash
make web RAYLIB_PATH=~/path/to/raylib
```

### 4. Serve & Open

```bash
make web-serve
# Then open http://localhost:8080
```

Or manually: `cd build/web && python3 -m http.server 8080`

**Troubleshooting:**
- If the raylib library is missing, run `make PLATFORM=PLATFORM_WEB` in `raylib/src/`
- If `emcc` is missing, source `emsdk_env.sh`
- First load can be large because assets are bundled

## GitHub Pages (Automatic Deployment)

This repo builds and deploys the WASM bundle to **GitHub Pages** on push to `main` or `master`.
Enable once in **Settings → Pages** by setting **Source** to **GitHub Actions**.
Then the site is published at `https://<your-username>.github.io/<your-repo>/`.

## Project Structure

```
OpenSciSim/
├── src/        # app + modules
├── assets/
├── web/        # HTML shell
├── build/web/  # WASM output (generated)
├── Makefile
└── flake.nix
```

## License

MIT
