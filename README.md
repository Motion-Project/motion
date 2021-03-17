MotionPlus
=============

## Description

MotionPlus is a break at version 4.2.2 from the Motion application.  MotionPlus removes
some of the outdated processes and features of the Motion application and introduces new
functionalities.

The following is a initial list of revisions from the Motion application.
- Compiled in C++ (Not all code has been converted from C)
- FFmpeg is required.
- Removed
    - Brooktree devices
    - Round-Robin
    - Netcam options not using FFmpeg
    - Assembler code
    - Web items not using libmicrohttpd
    - Embedded tracking (Is now all external scripts)
    - autobrightness
    - mpeg4, msmpeg4, swf, ffv1 and mov movie formats.
    - separate ports for camera streams
    - text web interfaces
    - Web control GET interface to parameters
- New functionality
    - Secondary detection method via OpenCV
    - New primary detection parameters
    - Sound from pass through sources
    - JSON configuration parameters
    - User provided web page
    - Add/delete camera via web interface
    - Additional control parameters
    - Edits on user configuration parameters
    - POST web control processing
    - ROI picture output

## Documentation

The documentation for MotionPlus is currently in the process of being updated from the old
Motion application.

## License

MotionPlus is distributed under the GNU GENERAL PUBLIC LICENSE (GPL) version 3 or later.

## Contributing

Issues and pull requests will considered at the developers discretion.  It is best to open issues for discussion prior to coding a PR since not all will be accepted.


