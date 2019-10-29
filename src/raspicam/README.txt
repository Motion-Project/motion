The files in this directory are used in the MMAL/RaspberryPI camera
support code. The files were taken from the Raspberry PI userland git
repository:

https://github.com/raspberrypi/userland

The files are mostly a straight copy from the userland versions. Only
the "RaspiCamControl.c" file was altered to include the
"mmal_status_to_int" function defined in the helper module
"RaspiHelpers.h". The callout to the helper module was therefore
removed. The inserted "mmal_status_to_int" function is an exact copy
from the helper module.

Additional Revision:
Added more default values to raspicamcontrol_set_defaults

They are used to parse an options string and setup the camera
parameters appropriately. The format of the string is the same as
other raspberry pi camera tools.
