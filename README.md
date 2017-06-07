# cam_cap

This program is based on uvccapture to do camera capture and save as multiple formats.

Usage is: uvccapture [options]
Options:
-v              Verbose (add more v's to be more verbose)
-o<filename>    Output filename prefix(default: cam_cap_snap_xxx.jpg).
-d<device>      V4L2 Device (default: /dev/video1)
-x<width>       Image Width (must be supported by device), default 1920x1080
-y<height>      Image Height (must be supported by device), default 1920x1080
-j<integer>     Skip <integer> frames before first capture
-t<integer>     Take continuous shots with <integer> microseconds between them (0 for single shot), default is single shot
-T              Test capture speed, -n must be set with this option
-n<integer>     Take <integer> shots then exit. If delay is defined, it will do capture with delay interval, Or, it will do capture continuously
-q<percentage>  JPEG Quality Compression Level (activates YUYV capture), default 95
-r              Use read instead of mmap for image capture
-w              Wait for capture command to finish before starting next capture
-m              Toggles capture mode to YUYV capture
-f<format>      Change output format, 0-MJPEG, 1-YUYV, 2-BMP, default is BMP
Camera Settings:
-B<integer>     Brightness
-C<integer>     Contrast
-S<integer>     Saturation
-G<integer>     Gain
