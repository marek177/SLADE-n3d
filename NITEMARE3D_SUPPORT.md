# Nitemare 3-D support

This build adds archive/resource support for the original Nitemare 3-D files:

- `IMG.1`, `IMG.2`, `IMG.3`: converts every embedded wall, object and sprite to a real PNG entry on opening. If `GAME.PAL` is beside the IMG file, its PCX palette is applied automatically. Sprite backgrounds using colour index 31 are made transparent when that colour dominates the border.
- `MAP.1`, `MAP.2`, `MAP.3`: converts every 64x64 two-layer level to a 512x512 PNG overview. Wall IDs are colour-coded and object cells are marked in red/orange, so the level geometry is visible immediately.
- `UIF.DAT`: opens its entries and detects embedded PCX/MIDI resources.
- `SND.DAT`: keeps MIDI entries and wraps raw effects 34-110 as playable mono 8-bit WAV files at the original 10,989 Hz rate.
- `GAME.PAL`: is a standard PCX file and is already handled by SLADE.
- `OBJECTS.1-3` and `WALLS.1-3`: are text definitions and are already handled by SLADE's text editor.

## Editing safety

Nitemare archives are exposed as read-only extractor/viewer archives. The original container is preserved and cannot be accidentally overwritten with converted PNG/WAV preview data.

The current implementation is resource extraction with a 2D map overview. It does not yet add a dedicated semantic Nitemare map editor with named wall/object painting tools.

## Validation

Run:

```sh
python3 tools/test_nitemare_formats.py /path/to/IMG.1 /path/to/IMG.2 /path/to/IMG.3 /path/to/MAP.1 /path/to/MAP.2 /path/to/MAP.3 /path/to/UIF.DAT /path/to/SND.DAT
```

The validator parses every entry, rebuilds the container without edits, and requires a byte-identical result.
