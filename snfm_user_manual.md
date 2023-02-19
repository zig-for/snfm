# Super Nintendo File Manager
## A manager for your super nintendo ROMs. 

### General info
Compatible with SNI, not compatible with QUSB2SNES.

Launch SNI before using. Future versions will launch SNI on its own, if it's not already running.

Both programs may hang if SNI is frozen or in an invalid state.

### Binaries:

#### send_file 
Used to automagically send and launch ROMs. I recommend associating .sfc and .smc files with it. Otherwise, you can either drag-drop ROMs on top of it, or running `send_file your_rom_file.sfc`.

Warning: there's currently no checks for ROM validity before launching. If you try to launch something that's not a ROM, it may hang your device, forcing a restart.

See `snfm_config_example.yaml` for examples of how to configure send_file

#### manage_files

Used for a convenient file-tree-like interface for your FXPAKPRO

It's not recommended to run two instances of the app at once, or other applications that may touch the filesystem - this may result in device hangs, if two incompatible operations are performed.

Features:
* Drag and drop, both within the device, and in/out
* Right click context menu
* Double click to run
* Opening/Closing a directory also refreshes it
* The Device menu can reset your game or kick you back to the main menu