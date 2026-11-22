# Kernel modules and device tree for the Unihertz Jelly Star

MediaTek's `kernel_device_modules-6.12` for the **Unihertz Jelly Star** (MT6789 / Helio G99),
with the drivers, fixes and device tree this phone needs. Together with Google's unmodified
GKI kernel it is the complete kernel side of the
[LineageOS 23.2 port](https://github.com/osquera/lineageos_unihertz_jellystar): every kernel
module the build ships and the DTB/DTBO are built from this tree. No stock kernel binaries are
left.

Branch: **`jellystar`**.

## What is in the build

| Part | Source |
|---|---|
| Kernel image | Google's GKI build `6.12.69-android16-6-g86dc53cfe87f-ab15464463-4k`, unmodified (the one stock firmware ships) |
| GKI modules (`system_dlkm`) | Google's, unmodified |
| Vendor modules (`vendor_boot`, `vendor_dlkm`) | this tree |
| DTB and DTBO | this tree: MediaTek's `mt6789.dts` plus `arch/arm64/boot/dts/mediatek/mt6789-jellystar-overlay.dtso` |

Userspace is a different matter: like every LineageOS device, the vendor partition is built from
the stock firmware's binaries (camera HAL, audio, GPU driver, modem stack and so on). This tree
is about the kernel side only.

## What is Jelly Star specific

The base is MediaTek's ALPS tree (`android16`, commit `aab53d9`, via
[ramabondanp/alps-kernel_device_modules-6.12](https://github.com/ramabondanp/alps-kernel_device_modules-6.12)).
The build also needs `vendor/mediatek/kernel_modules` from OnePlus' MT6993 GPL release (see
[Building](#building)). On top of the base, in `git log aab53d9..jellystar`:

**Drivers imported from other GPL releases**, at the version stock ships where one was public:

| Hardware | Driver | From |
|---|---|---|
| Back camera, Samsung S5KGM2 | `imgsensor` sensor driver | LG K42 (MT6765) kernel source, via [rezesferi/lg-k42-kernel-source](https://github.com/rezesferi/lg-k42-kernel-source) |
| Front camera, Sony IMX219 | `imgsensor` sensor driver (2-lane) | Volla Phone MT6763 kernel, [HelloVolla/android_kernel_volla_mt6763](https://github.com/HelloVolla/android_kernel_volla_mt6763) |
| Fuel gauge, O2Micro/BMT OZ8806 | `fauge` | [LineageOS/android_kernel_motorola_sm6225](https://github.com/LineageOS/android_kernel_motorola_sm6225) (driver), Samsung Galaxy A05s via [cd-Crypton/android_kernel_samsung_bengal-5.15](https://github.com/cd-Crypton/android_kernel_samsung_bengal-5.15) (gauge algorithm) |
| Touch, Chipsemi CHSC5xxx | `chsc5xxx_touch` | [danascape/linux-daria-mt6877](https://github.com/danascape/linux-daria-mt6877) |
| Fingerprint, Sunwave | `sunwave_fp` | [Realme C51/C53 kernel source](https://github.com/realme-kernel-opensource/realme_C51_C53_Note50_C60_C51_N53-AndroidU-kernel-source) |
| NFC, TMS THN31 | `tms_thn31` | [OnePlusOSS/android_kernel_oneplus_mt6835](https://github.com/OnePlusOSS/android_kernel_oneplus_mt6835) |
| TEE, TrustKernel | `tkcore`, `tkcore_drv` | LAVA Agni 4 (MT6897), [Mashopy/android_kernel_device_modules_lava_mt6897](https://github.com/Mashopy/android_kernel_device_modules_lava_mt6897) |
| Front RGB LED, Awinic AW2033 | `leds-aw2033` | [LineageOS/android_kernel_motorola_sm6225](https://github.com/LineageOS/android_kernel_motorola_sm6225) |
| Speaker amp, FourSemi FS19xx | `snd-soc-fs19xx` | [xiaomi-klee-devs/android_kernel_device_modules-6.6](https://github.com/xiaomi-klee-devs/android_kernel_device_modules-6.6) |
| Speaker amp, FourSemi FS16xx | `snd-soc-fsm` | [radxa/allwinner-bsp](https://github.com/radxa/allwinner-bsp) |
| Charger, Richtek RT9471 | `rt9471` | MediaTek MT6789 5.10 kernel, ported to 6.12 |
| Video codec | `mtk-vcodec`, `mtk-vcu` | [OnePlus MT6993 GPL release](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_mt6993) (the revision stock's codec daemon expects) |

Each import is a separate commit with the exact source commit, followed by a commit that adapts
it to this phone and kernel 6.12.

**Written for this phone:**

- `drivers/gpu/drm/panel/panel-unihertz-jellystar-st7701s.c`: the ST7701S display panel (480x854,
  single-lane DSI).
- `drivers/leds/lampbelt-leds.c`: the LED belt on the back (a PWM channel in MediaTek's memory
  mode plus GPIOs; same sysfs interface as stock).
- `mt6789-jellystar-overlay.dtso`: the board description (the stock DTBO and stock's additions to
  the base DTB, as source).
- Jelly Star battery data for the OZ8806 gauge.
- Changes to MediaTek code: charger and battery glue for the RT9471 and OZ8806, the camera buffer
  ioctl layout stock's camera HAL uses, GPU clock dividers, the video encoder's message layout,
  and the camera sensor drivers' answers to the camera HAL (timing, gain ranges, white balance,
  frame delays), matched to what the stock driver reports.

**How the stock-matching parts were done:** where no public source existed, behaviour was taken
from the device tree, the stock firmware's data (named data tables such as battery and sensor
tables) and what the stock modules do at runtime (register writes and driver answers, recorded
with tracefs and kprobes). Stock module code was not decompiled.

## Building

The build scripts live in the device tree,
[lineageos_unihertz_jellystar](https://github.com/osquera/lineageos_unihertz_jellystar) under
`kernel/tools/`, and are described in its `kernel/README.md`:

```sh
kernel/tools/setup-workspace.sh ~/jellystar-kernel        # once; needs repo, git, ~60 GB
JOBS=6 kernel/tools/build-modules.sh ~/jellystar-kernel/ws
kernel/tools/stage-modules.py ~/jellystar-kernel/ws \
    --stock-vendor-boot <dir> --stock-vendor-dlkm <dir> --gki-symvers <file>
```

`setup-workspace.sh` uses public sources only: Google's common kernel pinned to the GKI build
above, OnePlus' MT6993 GPL release for MediaTek's Kleaf rules and `vendor/mediatek/kernel_modules`,
and this tree. `stage-modules.py` copies the modules and the DTB/DTBO into the device tree and
checks every module's imported symbols against what loads before it.

The tree also has the entry point LineageOS's `build_kernel` expects, as on the official
MediaTek GKI devices: `build_mgk_64_k612.sh` with `build.config.jellystar`.

## Status

Working with this tree on the Jelly Star (LineageOS 23.2): display, touch, both cameras (photo,
video, flash), audio (speaker, earpiece, mic, Bluetooth), charging (also when powered off),
battery level, fingerprint, NFC, Wi-Fi, Bluetooth, mobile data and calls, GPS, sensors, the LED
belt, the front LED, the side key, video recording and playback.

Built but not tested: the FS16xx amplifier path (`snd-soc-fsm`; the test phone has the FS19xx
amp) and the colour variant of the LED belt, if one exists.

Bugs and questions: [the LineageOS port's issues](https://github.com/osquera/lineageos_unihertz_jellystar/issues).

## License

GPL-2.0, see `COPYING` and `LICENSES/`. Imported drivers keep their own copyright headers.
