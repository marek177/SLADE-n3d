# Nitemare 3-D support

This build adds archive/resource support for the original Nitemare 3-D files:

- `IMG.1`, `IMG.2`, `IMG.3`: opens the embedded 8-bit column-major graphics as individual entries and previews them in the graphics editor.
- `MAP.1`, `MAP.2`, `MAP.3`: opens each 64x64 two-layer map as an individual entry. The preview uses the wall byte as the palette index; object cells are highlighted with indices 128-255.
- `UIF.DAT`, `SND.DAT`: opens the 16-bit-length/32-bit-offset directory and detects embedded PCX and MIDI resources.
- `GAME.PAL`: is a standard PCX file and is already handled by SLADE.
- `OBJECTS.1-3` and `WALLS.1-3`: are text definitions and are already handled by SLADE's text editor.

## Editing safety

MAP entries can be changed and saved when each map remains exactly 8192 bytes. DAT contents can be resized up to 65535 bytes per entry; the directory is rebuilt on save. IMG entries can be changed only without changing record sizes because their header contains undocumented internal lookup tables. SLADE refuses structural IMG changes instead of creating a corrupt game file.

The current implementation is resource/archive support with a 2D map preview. It does not yet add a dedicated semantic Nitemare map-mode with named wall/object painting tools.

## Validation

Run:

```sh
python3 tools/test_nitemare_formats.py /path/to/IMG.1 /path/to/IMG.2 /path/to/IMG.3 /path/to/MAP.1 /path/to/MAP.2 /path/to/MAP.3 /path/to/UIF.DAT /path/to/SND.DAT
```

The validator parses every entry, rebuilds the container without edits, and requires a byte-identical result.
