# Build results

## Map Statistics 1.1.4 test candidate

- Change: added a dedicated `Volume Decals` counter for entities whose classname is exactly `misc_volume_decal`, including entities found through recursively expanded prefabs.
- Configuration: `Release | x64`
- Compiler/linker: MSVC 14.16 (Visual Studio 2017 toolchain)
- Windows SDK: 10.0.19041.0
- Warnings: none
- Errors: none
- PE architecture: AMD64 / PE32+
- Subsystem: Windows GUI DLL
- Export: exactly one named export, `ModToolsPluginEntry`
- SHA-256: `5443cb1cb86b5f9de4cf529a79bbcece22adc4f5a5768d27cf0c253d2880fa00`
- Deployment: copied to the BO3 `bin\plugins\MapStatistics` directory while Radiant was not running; the installed INI was preserved.
- Runtime status: the user confirmed that the deployed 1.1.4 test candidate loads and works correctly in Radiant, including the new Volume Decals count.

## Map Statistics 1.1.3

- Configuration: `Release | x64`
- Language: C++17
- Compiler/linker: MSVC 14.16 (Visual Studio 2017 toolchain)
- Windows SDK: 10.0.19041.0
- Runtime: static MSVC runtime (`/MT`)
- Warnings: none
- Errors: none
- PE architecture: AMD64 / PE32+
- Subsystem: Windows GUI DLL
- Export: exactly one named export, `ModToolsPluginEntry`
- Runtime DLL dependencies: `USER32.dll`, `KERNEL32.dll`

### Current local release hashes

```text
ff86bbd8a12b53b4916537d3d1fa39e36c1bb0a00ad2f18fd204c1b2dd4cfbe9  MapStatistics.dll
ce8dd363ce259c0f4cd70946c44f94e1254a62370af52ffb2f73a5a19b08d2c5  MapStatistics.ini
```

The previously built 1.1.3 candidate (`7535efc96ac866cb588172ba28330e1c7e76fcc548bb5e2f805380921b8b3ff1`) was installed into the BO3 plugin directory. `RadiantPlugins.log` confirms that it loaded at 04:36:21, registered the `Ctrl+Shift+F12` hotkey and produced a statistics scan without the removed Entities or Materials fields. The user manually confirmed that the plugin, overlay, hotkey and settings UI work correctly in Radiant.

The current local DLL removes the continuous tick/fallback scanner. Statistics are refreshed only through the **Refresh Statistics** menu command or the **Refresh Statistics Now** button in the settings window. Loading another map or entering/leaving a prefab clears the previous snapshot without starting a scan.

This DLL was deployed with its default INI to the BO3 `bin\plugins\MapStatistics` directory. The user confirmed in Radiant that the manual refresh behavior works very well.
