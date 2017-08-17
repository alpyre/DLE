# DLE (A DoomLike 2.5D Engine for Amiga Computers by Alpyre)
An educational and experimental hobby project to create a pseudo 3D (2.5D)
graphics engine on Amiga computers.

Features:
* Opens a 4 color native screen (NO RTG)
* Parses map data from an ASCII map file
* Loads 48x48 ILBM files as textures (from progdir)
* Does NOT use Floating Point operations during render
* Uses affine texture mapping (for maximum fps)
* Can reach a minimum 25 FPS on mildly accelerated Amiga's
* A small test map for testing purposes

### How to compile
Use SAS/C v6.58 (with the provided makefile)

### Demo visual
![demo](http://i68.tinypic.com/2gwvsj6.jpg)

## Controls
| Key           | Function                                                     |
| ------------- | ------------------------------------------------------------ |
| `wasd`        | Movement and look around                                     |
| `F1`          | Toggle FPS counter                                           |
| `F2`          | Toggle sectorID display                                      |
| `f`           | Toggle full screen                                           |
| `ESC`         | Quit                                                         |
