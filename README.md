# FerroFetchFirmware
Firmware for the Applied Procrastination "Fetch" ferrofluid display

Currently almost everything in here is under development and no hardware project should be based on this firmware as it is in it's current state. Things will change.

## Dependencies
- [SdFat library by Greiman](https://github.com/greiman/SdFat)
- [RTCLib library by Adafruit](https://github.com/adafruit/RTClib)

New with Fetch V2.0:
- [AprocAnimation library](https://github.com/appliedprocrastination/AprocAnimation) (this library was previously part of FerroFetchFirmware, but has now been broken out as its own library. The library has  been redesigned to work better with the MagnetControllerV2 PCBs, meaning it is no longer compatible with the first version of PCBs in Fetch)
- [MagnetControllerV2 library](https://github.com/appliedprocrastination/MagnetControllerV2-library)
- [PWM Servo Driver library by Adafruit](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)

## PlatformIO
The code has been developed and used in the PlatformIO environment, which is recommended to be used with VSCode (Atom also works).
For more information: https://platformio.org/install

## Electronics
The electronics in this project is also open source, and can (hopefully) be found on your favorite project hub:

- [Instructables](https://www.instructables.com/id/Mesmerizing-Ferrofluid-Display-Silently-Controlled/)
- [Hackaday.io](https://hackaday.io/project/167056-fetch-a-ferrofluid-display)
- [Hackster.io](https://www.hackster.io/AppliedProc/fetch-a-ferrofluid-display-ca8557)
- [Arduino Project hub](https://create.arduino.cc/projecthub/AppliedProc/fetch-a-ferrofluid-display-ca8557)
- [Designspark](https://www.rs-online.com/designspark/student-innovation-ferrofluid-display-silently-controlled-by-electromagnets)

## License
GPL3+
