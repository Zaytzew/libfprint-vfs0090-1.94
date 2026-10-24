# vfs0090 driver for libfprint 1.94.100 (Validity 138a:0090, ThinkPad P50/T460p class)

A port of the match-on-host `vfs0090` driver from
[rabbitambulance-ra/libfprint-tod-vfs0090](https://github.com/rabbitambulance-ra/libfprint-tod-vfs0090)
(commit `0c7427a`) into the **libfprint 1.94.100** tree, so it works with the
stock `fprintd` and `fprintd-pam` of current distributions (tested on Fedora 44).

Unofficial and not upstream. It works on one machine so far; reports from other
`138a:0090` owners are welcome.

## Tested on

- ThinkPad P50 (20FA0045MS), sensor `138a:0090`, ROM 6.7 build 164
- Fedora 44, libfprint 1.94.100, stock fprintd + fprintd-pam
- enroll, verify, GDM login and sudo work; the paired state survives a reboot

## Before you start: pairing

This driver expects a sensor that is already paired (TLS keys and firmware in the
sensor flash). Pairing was done with python-validity 0.15 plus
[python-validity PR #294](https://github.com/uunicorn/python-validity/pull/294)
(ROM 6.7 build 164 has no signed partition table). python-validity can pair
`0090` but cannot enroll it (`create_enrollment` → `0401`); this driver does the
enrollment instead. Mask `python3-validity.service` while pairing (udev starts it
when the sensor re-enumerates after a reset), and remove python-validity
afterwards — it and fprintd must not hold the device at the same time.

## What changed against the fork

- `fpi_ssm_next_state_delayed()`: dropped the `cancellable` argument (1.94 API)
- `fpi_device_class_auto_initialize_features()` in `class_init` (fprintd crashes without it)
- `#include "config.h"`: without it `HAVE_PIXMAN` was undefined and the x2 image
  upscale silently disappeared, which gave match scores of 0/12
- own corner-aligned bilinear x2 upscale instead of `fpi_image_resize`
  (the pixman path matched noticeably worse here)
- meson: driver registered with the `nss`, `openssl` and `pixman` helpers

## Build (Fedora)

```sh
dnf download --source libfprint          # 1.94.100
rpm -i libfprint-1.94.100-*.src.rpm
cp 0001-vfs0090-driver.patch ~/rpmbuild/SOURCES/
cp libfprint.spec ~/rpmbuild/SPECS/      # Fedora's spec + Patch0 + release suffix
sudo dnf builddep ~/rpmbuild/SPECS/libfprint.spec
rpmbuild -bb ~/rpmbuild/SPECS/libfprint.spec
sudo dnf install ~/rpmbuild/RPMS/x86_64/libfprint-1.94.100-1.vfs0090.*.rpm
fprintd-enroll
```

Other distributions: apply `0001-vfs0090-driver.patch` to the libfprint
v1.94.100 source (`patch -p1`) and build with meson as usual; the driver needs
NSS, OpenSSL and pixman development headers.

**A distribution update of libfprint replaces this build and the reader stops
working** — rebuild or pin the package (`dnf versionlock` / `exclude=libfprint`).

## Security note

This is a match-on-host driver: fingerprint images are processed and templates
are stored on the computer (NBIS/bozorth3), not inside the sensor.

## tools/nbis_matrix.c

Offline harness used while tuning: prints the bozorth3 score matrix for
captured PGM images (from libfprint's `examples/img-capture`). Build it inside
the libfprint tree against its private headers.

## License

LGPL-2.1-or-later, as libfprint and the original driver (copyright Nikita
Mikhailov, Marco Trevisan and contributors; see the file headers).
