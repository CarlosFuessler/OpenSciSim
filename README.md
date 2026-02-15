# OpenSciSim

Interactive science simulator built with [Raylib](https://www.raylib.com/) in C.
Explore mathematics, physics and chemistry through hands-on visualizations.

![C](https://img.shields.io/badge/C11-00599C?style=flat&logo=c&logoColor=white)
![Raylib](https://img.shields.io/badge/Raylib-black?style=flat&logo=raylib&logoColor=white)

## Features

### Mathematics
- **CAS Plotter** — GeoGebra-style algebra view with 2D/3D function plotting, template buttons, implicit multiplication, graph coloring
- **Calculator** — Casio-style scientific calculator with expression history
- **Math Sims** — Parametric curves and polar graphs with presets

### Physics
- **Atom Models** — Interactive 3D visualization of 5 atomic models (Dalton → Quantum Mechanical) for 10 elements with detailed explanations
- **Mechanics Lab** — Pendulum and projectile motion simulations with adjustable parameters

### Chemistry
- **Periodic Table** — Color-coded table (118 elements) with element details
- **Molecule Viewer** — 3D ball-and-stick models of common molecules (H₂O, CO₂, CH₄, NH₃, NaCl, Ethanol)
- **Chemistry Lab** — Reaction visualizer and pH (acid/base) simulator

## Prerequisites

### Using Nix (recommended)

```bash
# Everything is handled by the flake — just enter the dev shell
nix develop
```

### Manual

- A C11 compiler (gcc, clang)
- [Raylib](https://www.raylib.com/) (≥ 5.0) installed and available via `pkg-config`
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

## Build for Web (WASM)

OpenSciSim can be compiled to WebAssembly using [Emscripten](https://emscripten.org/) and run in any modern browser.

### 1. Install Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 2. Build Raylib for WASM

Clone raylib (if you haven't already) and build it for WASM:

```bash
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_WEB
```

This creates `libraylib.a` in the same directory.

### 3. Build OpenSciSim for Web

```bash
# Point to your raylib directory
make web RAYLIB_PATH=~/path/to/raylib
```

This generates:
- `build/web/index.html`
- `build/web/index.js`
- `build/web/index.wasm`

Custom HTML shell (no Emscripten branding): `web/shell.html`

### 4. Serve & Open

WASM files require HTTP (not `file://` URLs):

```bash
make web-serve
# Then open http://localhost:8080
```

Or manually:

```bash
cd build/web
python3 -m http.server 8080
```

**Troubleshooting:**
- If `libraylib.a` not found: confirm you ran `make PLATFORM=PLATFORM_WEB` in `raylib/src/`
- If emcc not found: source `emsdk_env.sh` from your emsdk directory
- Slow initial load: WASM bundles include all assets; first load ~20-50MB depending on browser cache

## GitHub Pages (Automatic Deployment)

This repo includes a GitHub Actions workflow that builds the WASM bundle and deploys it to **GitHub Pages** on every push to `main` or `master`.

**Enable Pages once:**
1. Go to **Settings → Pages**
2. Set **Source** to **GitHub Actions**

After that, every push will publish the site at:
`https://<your-username>.github.io/<your-repo>/`

## Project Structure

```
OpenSciSim/
├── flake.nix                    # Nix flake (deps + dev shell)
├── Makefile                     # Native + WASM build targets
├── assets/
│   └── fonts/
│       └── JetBrainsMono-Regular.ttf
├── src/
│   ├── main.c                   # Entry point, topic registration
│   ├── ui/
│   │   ├── theme.h              # Colors, sizes, constants
│   │   ├── ui.h                 # AppUI, Topic, ScreenState
│   │   └── ui.c                 # Start screen, tab bar, widgets
│   ├── utils/
│   │   ├── arena.h              # Arena allocator
│   │   └── arena.c
│   └── modules/
│       ├── module.h             # Module interface
│       ├── cas/                  # CAS plotter (2D + 3D)
│       │   ├── cas.c / cas.h
│       │   ├── parser.c / parser.h
│       │   ├── eval.c / eval.h
│       │   ├── plotter.c / plotter.h
│       │   └── plotter3d.c / plotter3d.h
│       ├── calc/                 # Scientific calculator
│       │   └── calc.c / calc.h
│       ├── physics/              # Atom model simulator
│       │   └── physics.c / physics.h
│       └── chemistry/            # Periodic table + molecules
│           └── chemistry.c / chemistry.h
└── build/
    └── web/                     # WASM output (generated)
```

## License

MIT
