#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Builds the Jelly Star kernel (GKI image and modules) with MediaTek's Kleaf rules and puts the
# results in out/mgk_64_k612/dist, for LineageOS's build_kernel (vendor/lineage/build/envsetup.sh).
# Run from the root of the kernel workspace, where the manifest links build.config to
# build.config.jellystar.

set -e

TARGET_DEVICE="mgk_64_k612"

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${ROOT_DIR}"

KERNEL_MODULES_DIR="$(find "${ROOT_DIR}" -maxdepth 1 -name 'kernel_device_modules-*' | sort -V | tail -n1)"
if [ -z "${KERNEL_MODULES_DIR}" ] || [ ! -f "${KERNEL_MODULES_DIR}/build.sh" ]; then
    echo "error: no kernel_device_modules-*/build.sh found" >&2
    exit 1
fi
KERNEL_MODULES_NAME="$(basename "${KERNEL_MODULES_DIR}")"

export OUT_DIR="${ROOT_DIR}/out"
mkdir -p "${OUT_DIR}"

INTERMEDIATE_DIST="${OUT_DIR}/dist"
FINAL_DIST="${OUT_DIR}/${TARGET_DEVICE}/dist"

rm -rf "${INTERMEDIATE_DIST}" "${FINAL_DIST}"

# build.sh takes the workspace-relative directory name from $0.
(cd "${ROOT_DIR}" && "${KERNEL_MODULES_NAME}/build.sh")

SRC="${INTERMEDIATE_DIST}"
[ -d "${SRC}/${KERNEL_MODULES_NAME}" ] && SRC="${SRC}/${KERNEL_MODULES_NAME}"

mkdir -p "${FINAL_DIST}"

# Vendor modules, then the GKI kernel's own modules (system_dlkm) and image.
find "${SRC}" -path "*${TARGET_DEVICE}_customer_modules_install.user*" -name '*.ko' \
    -exec cp -t "${FINAL_DIST}/" {} +
find "${SRC}" -path "*${TARGET_DEVICE}_kernel_aarch64.user*" -name '*.ko' \
    -exec cp -n -t "${FINAL_DIST}/" {} +
find "${SRC}" -path "*${TARGET_DEVICE}_kernel_aarch64.user*" -name 'Image*' \
    -exec cp -t "${FINAL_DIST}/" {} +
find "${SRC}" -maxdepth 3 -name 'kernel-uapi-headers.tar.gz' -exec cp -t "${FINAL_DIST}/" {} + || true
# Modules the GKI build adds for this target (cfg80211, mac80211, reboot-mode, ...)
find "${SRC}/${TARGET_DEVICE}.user/" -name '*.ko' -exec cp -n -t "${FINAL_DIST}/" {} +

# Device tree: the MT6789 base DTB and the board overlay image
find "${SRC}/${TARGET_DEVICE}.user/" -maxdepth 1 -name '*.dtb' -exec cp -t "${FINAL_DIST}/" {} +
dtbo_img="${SRC}/${TARGET_DEVICE}_kernel_images.user_dtbo/dtbo.img"
if [ -f "${dtbo_img}" ]; then
    cp -a "${dtbo_img}" "${FINAL_DIST}/"
fi

echo "Kernel artifacts in ${FINAL_DIST}"
ls "${FINAL_DIST}" | sed 's/\.ko$//' | wc -l
