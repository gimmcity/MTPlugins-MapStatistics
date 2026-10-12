# Map Statistics for BO3 Radiant

Map Statistics is a native x64 [MTPlugins](https://github.com/Aidan601/MTPlugins) plugin for Black Ops III Radiant. It adds a configurable on-demand statistics overlay to the 3D camera viewport.

## Features

- On-demand brush, patch and serialized vertex counts.
- Counts for models, script models, script brushmodels, script origins, script structs, FX, triggers, lights and reflection probes.
- Recursively expands placed `misc_prefab` files while preventing recursion cycles.
- Parses placed FX and nested FX runners for supported light elements.
- Configurable overlay corner and text color.
- Persistent per-counter visibility settings.
- `Ctrl+Shift+F12` toggles the overlay.
- Scans run only when **Refresh Statistics Now** is clicked; the render callback only displays the last snapshot.

Version 1.1.3 removes the former **Entities** and **Materials** counters, including their settings and aggregation work.

## Counting definitions

- **Models:** placed `misc_model` and `dyn_model` entities.
- **Script Models:** entities whose classname is exactly `script_model`.
- **Script Brushmodels:** entities whose classname is exactly `script_brushmodel`.
- **Script Origins:** entities whose classname is exactly `script_origin`.
- **Script Structs:** entities whose classname is exactly `script_struct`.
- **FX:** placed entities whose classname is exactly `fx`.
- **Triggers:** entities whose classname starts with `trigger_`.
- **Lights:** `light` and `light_*` entities plus supported light elements in placed FX and recursively referenced FX runners.
- **Reflection Probes:** entities whose classname is exactly `reflection_probe`.
- **Brushes:** classic plane-defined brush primitives in the current map and recursively referenced prefabs.
- **Patches:** `mesh` and `curve` primitives in the current map and recursively referenced prefabs.
- **Vertices:** distinct serialized plane-definition points per classic brush plus serialized `v` control vertices in mesh and curve primitives.

## Requirements

- Black Ops III Mod Tools
- MTPlugins with Radiant plugin support
- Windows x64

Map Statistics runs only inside the BO3 Radiant editor process. It does not load into the game or multiplayer.

## Installation for local testing

Create this directory under the Black Ops III installation:

```text
<BO3>\bin\plugins\MapStatistics\
```

Copy these files into it:

```text
MapStatistics.dll
MapStatistics.ini
```

Start Radiant and confirm that **Map Statistics 1.1.3 [Radiant]** is enabled in MTPlugins. Use `Ctrl+Shift+F12` to toggle the overlay. When published through MTPlugins-Index, users should install it through **Browse plugins** instead of copying files manually.

Open **Plugins > Map Statistics > Refresh Statistics** or click **Refresh Statistics Now** in the plugin settings whenever you want to rescan the current map. Loading a different map or entering/leaving a prefab clears the old snapshot until the next manual refresh.

## Configuration

`MapStatistics.ini` controls the initial overlay state, corner, RGB color and the visibility of each counter. The native settings dialog writes the same values while Radiant is running.

The index marks this file as configuration, so MTPlugins updates preserve user settings.

## Building

Install Visual Studio with the **Desktop development with C++** workload and a Windows 10 or Windows 11 SDK. Open `MapStatistics.sln`, select `Release | x64`, and build the solution.

Alternatively run:

```text
build.bat
```

Output:

```text
build\Release\MapStatistics.dll
```

The project uses C++17, warning level 4, SDL checks and the static MSVC runtime (`/MT`).

## Publishing through MTPlugins-Index

Push the release commit to this repository, fork [MTPlugins-Index](https://github.com/Aidan601/MTPlugins-Index), then run its official submission helper from the fork clone:

```text
python tools/submit.py C:\path\to\MTPlugins-MapStatistics --test-build
```

The helper pins the exact source commit and writes `sources/MapStatistics.json`. A submission pull request changes only that file. After review and merge, MTPlugins-Index builds and publishes the official DLL from the reviewed source. `MapStatistics.ini` is marked as configuration so updates preserve existing user settings.

GitHub releases in this repository are optional convenience downloads and are not used by the source-build submission.

## Source layout

- `src/` contains the plugin lifecycle, settings UI, map/FX scanner and overlay renderer.
- `sdk/` contains the MTPlugins API headers used to build the plugin.
- `MapStatistics.ini` is the default configuration included in releases.

## Author

GimmCity

## License

GPL-3.0. See `LICENSE` and `NOTICE.md`.
