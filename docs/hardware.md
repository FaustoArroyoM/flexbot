# FlexBot Hardware Reference

> On-demand reference. Load with `@docs/hardware.md` when you need the bill
> of materials, wiring, or vendor datasheets — none of which are committed
> to this repo (see [Repo hygiene](#why-no-pdfs-in-this-repo) below).

## Honest scope

This runs on **one specific lab rig**: a 2-DOF flexible-link arm built on a
Quanser "2 DOF Serial Flexible Link" mechanical platform, re-instrumented
with a custom ESP32 PCB that replaces Quanser's original QUARC/DAQ
electronics entirely — the ESP32 reads the encoders and strain gauges and
drives the motor amplifiers directly, with no Quanser software or DAQ card
in the signal path on this rig.

You don't need the arm to build or run this project. Clone both repos,
build the PC host (see the main [README](../README.md)), flash the
firmware to a bare ESP32 DevKitC, and run an experiment: the PC will
connect over serial, exchange the start/stop/control protocol, and log a
CSV — with all-zero (or floating/noisy) sensor readings and no motor
attached to feel the output. That's enough to evaluate the protocol,
control-mode dispatch, and CSV pipeline without the physical hardware.

## Bill of materials

Datasheets and product pages are **linked, not committed** — see
[below](#why-no-pdfs-in-this-repo) for why. Retrieved 2026-07; vendor
pages move over time, so if a link is dead, search the vendor's site for
the part number in the left column.

| Component | Vendor | Link |
|---|---|---|
| ESP32-D0WD SoC (dual-core, 240 MHz) | Espressif | [ESP32 Series Datasheet](https://documentation.espressif.com/esp32_datasheet_en.html) |
| ESP32-WROOM-32 module | Espressif | [ESP32-WROOM-32 Datasheet](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html) |
| ESP32 DevKitC V4 dev board | AZ-Delivery | [Product page](https://www.az-delivery.de/en/products/esp-32-dev-kit-c-v4) |
| Shoulder motor — maxon RE 25, Ø25 mm, 20 W | maxon motor AG | [Product page](https://www.maxongroup.com/maxon/view/product/118755) |
| Elbow motor — maxon RE 35, Ø35 mm, 90 W | maxon motor AG | [Product page](https://www.maxongroup.com/maxon/view/product/273753) |
| Motor current amplifiers (×2) — Quanser AMPAQ-L2 | Quanser Inc. | [Product page](https://www.quanser.com/products/ampaq-l2-amplifier/) |
| Joint encoders (×2) — US Digital E5 optical kit encoder | US Digital | [Product page](https://www.usdigital.com/products/e5) |
| Strain gauge signal conditioning | custom (on the interface PCB below) | — |
| Base mechanical platform — Quanser 2 DOF Serial Flexible Link | Quanser Inc. | [Product page](https://www.quanser.com/products/2-dof-serial-flexible-link/) |
| Custom ESP32 interface PCB, Nov 2024 | lab-built | [`docs/FlexiLinkRobot_Platine_11-2024.pdf`](FlexiLinkRobot_Platine_11-2024.pdf) — kept in-repo, not vendor material |

**USB-serial driver** (Windows only — needed to see the ESP32 as a COM
port; which one depends on the USB-UART chip on your specific dev board):
[Silicon Labs CP210x VCP driver](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers)
or [WCH CH340/CH341 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html).
Linux and macOS ship these drivers in-kernel — nothing to install.

**Not verified — inferred from the code, not from opening the enclosure:**
the AMPAQ-L2 amplifiers and the Quanser DAQ manuals that shipped with the
platform are both present in `docs/Robot_Hardware/` for historical
reasons. `analog.cpp` (`dacWrite(DAC1, ...)` / `dacWrite(DAC2, ...)`)
confirms the ESP32's own two DAC channels drive the motor path directly —
consistent with the AMPAQ-L2s still being used as standalone command-voltage
amplifiers, but the Quanser Q8-USB DAQ card and QUARC toolchain the
platform originally shipped with are **not** part of this project's data
path. If you're extending this project and that's wrong, fix this note.

## Wiring / pin table

From `flexbot_firmware/include/config.h` and `analog.cpp`. Pin numbers are
ESP32 GPIO numbers unless noted.

| Signal | Pin | Notes |
|---|---|---|
| Encoder 1 (shoulder) — Channel A | GPIO 32 | |
| Encoder 1 (shoulder) — Channel B | GPIO 33 | |
| Encoder 1 (shoulder) — Index (Z) | GPIO 27 | resets hardware PCNT counter |
| Encoder 2 (elbow) — Channel A | GPIO 22 | |
| Encoder 2 (elbow) — Channel B | GPIO 17 | |
| Encoder 2 (elbow) — Index (Z) | GPIO 16 | resets hardware PCNT counter |
| Strain gauge 1 | GPIO 13 (`A14`) | 12-bit ADC, ±10 V range via signal conditioning |
| Strain gauge 2 | GPIO 4 (`A10`) | 12-bit ADC, ±10 V range via signal conditioning |
| Motor DAC out 1 (shoulder) | `DAC1` (GPIO 25) | ESP32 built-in 8-bit DAC, full range [0, 255] |
| Motor DAC out 2 (elbow) | `DAC2` (GPIO 26) | ESP32 built-in 8-bit DAC, restricted to [78, 171] for mechanical safety |
| Hall / limit — Elbow A | GPIO 34 | operational |
| Hall / limit — Elbow B | GPIO 35 | operational |
| Hall / limit — Shoulder A | GPIO 36 | wired but **not enabled** in firmware (`hall.cpp`) — see `docs/open-issues.md` §5 |
| Hall / limit — Shoulder B | GPIO 39 | wired but **not enabled** in firmware (`hall.cpp`) |

## Why no PDFs in this repo

Every PDF/driver `.zip` previously under `docs/ESP32/` and
`docs/Robot_Hardware/` was vendor-copyrighted material (Espressif, AZ-
Delivery, Quanser, US Digital, maxon, Silicon Labs, WCH) with no
redistribution rights granted — the Quanser manuals, for example,
explicitly state "no part may be reproduced, stored in a retrieval
system or transmitted in any form or by any means ... without the prior
written permission of Quanser Inc." Shipping copyrighted vendor manuals
and Windows driver installers from a personal GitHub repo is both a
rights problem and a trust anti-pattern — nobody should fetch a signed
driver from a stranger's repo. Linking to the vendor's own page is more
correct and ages better than a frozen local copy.

The one PDF still committed, `docs/FlexiLinkRobot_Platine_11-2024.pdf`,
documents the lab's own custom interface PCB — not vendor material — so
it isn't subject to the same constraint.
