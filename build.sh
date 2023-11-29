#!/bin/bash
#
# Copyright (C) 2023 ZHANtech™
#

WORK_DIR="${PWD}"
KERNEL_DIR="$(basename $PWD)"
DISTRO=$(source /etc/os-release && echo ${NAME})
KERVER=$(make kernelversion)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
COMMIT_HEAD=$(git log --oneline -1)
ANYKERNEL3_DIR="${HOME}"/kernel/anykernel
COMPILER_STRING=clang-r498229b
TC_DIR=prebuilts/clang/host/linux-x86/clang-r498229b
OUT_DIR=out/android13-5.15/dist

# Repo URL
ANYKERNEL_REPO="https://github.com/zhantech/Anykernel3.git"
ANYKERNEL_BRANCH="topaz"

# Costumize
KERNEL="Pringgodani"
RELEASE_VERSION="2.2"
DEVICE="Topaz-Tapas-Xun"
BENGAL_DEVICE="Bengal"
KERNELNAME="${KERNEL}-${RELEASE_VERSION}-${BRANCH}-${DEVICE}-$(TZ=Asia/Jakarta date +%y%m%d)"
BENGAL_KERNELNAME="${KERNEL}-${RELEASE_VERSION}-${BRANCH}-${BENGAL_DEVICE}-$(TZ=Asia/Jakarta date +%y%m%d)"
FINAL_KERNEL_ZIP="${KERNELNAME}.zip"
FINAL_KERNEL_IMG="${BENGAL_KERNELNAME}.img"

function clean() {
rm -rf "$HOME"/kernel
cd ..
rm -rf out
}

function cloning() {
if ! [ -d "${TC_DIR}" ]; then
echo "Clang not found! Cloning to ${TC_DIR}..."
if ! git clone --depth=1 https://gitlab.com/prebuilts_clang_host_linux-x86/clang-r498229b.git ${TC_DIR}; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

# Telegram
CHATIDQ="-1001308839345"
CHATID="-1001308839345" # Group/channel chatid (use rose/userbot to get it)
TELEGRAM_TOKEN="5988732593:AAEn7SJOoh5x8VWtevuPGO25-TRnygaLsoM" # Get from botfather

# Export Telegram.sh
TELEGRAM_DIR="${HOME}"/kernel/telegram
if ! [ -d "${TELEGRAM_DIR}" ]; then
    git clone https://github.com/zhantech/telegram.sh/ "${TELEGRAM_DIR}"
fi

}

function compile_kernel() {
TELEGRAM="${TELEGRAM_DIR}"/telegram

tg_cast() {
    "${TELEGRAM}" -t "${TELEGRAM_TOKEN}" -c "${CHATID}" -H \
    "$(
        for POST in "${@}"; do
            echo "${POST}"
        done
    )"
}

# Starting
tg_cast "<b>STARTING KERNEL BUILD</b>" \
    "Docker OS: ${DISTRO}" \
    "Device: ${DEVICE}" \
    "Kernel Version : ${KERVER}" \
    "Kernel Name: <code>${KERNEL}</code>" \
    "Release Version: ${RELEASE_VERSION}" \
    "Toolchain: ${COMPILER_STRING}" \
    "Branch : <code>$BRANCH</code>" \
    "Last Commit : <code>$COMMIT_HEAD</code>"
START=$(TZ=Asia/Jakarta date +"%s")

LTO=thin BUILD_CONFIG=$KERNEL_DIR/build.config.gki.aarch64 build/build.sh

# Check If compilation is success
    if ! [ -f "${OUT_DIR}"/Image ]; then
        END=$(TZ=Asia/Jakarta date +"%s")
        DIFF=$(( END - START ))
        echo -e "Kernel compilation failed, See buildlog to fix errors"
        tg_cast "Build for ${DEVICE} <b>failed</b> in $((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)! Check Instance for errors @zh4ntech"
        exit 1
    fi

}

function ziping() {
cd "${WORK_DIR}"

    git clone "$ANYKERNEL_REPO" -b "$ANYKERNEL_BRANCH" "$ANYKERNEL3_DIR"

echo "**** Copying Image ****"
cp ../$OUT_DIR/Image $ANYKERNEL3_DIR/Image
cp ../$OUT_DIR/boot.img "$HOME"/kernel/$FINAL_KERNEL_IMG

echo "**** Time to zip up! ****"
cd $ANYKERNEL3_DIR/
zip -r9 "$HOME"/kernel/"$FINAL_KERNEL_ZIP" * -x README $FINAL_KERNEL_ZIP
echo "**** Done, here is your sha1 ****"
sha1sum "$HOME"/kernel/$FINAL_KERNEL_ZIP
sha1sum "$HOME"/kernel/$FINAL_KERNEL_IMG
}

function upload() {
    "${TELEGRAM}" -f "$HOME"/kernel/"$FINAL_KERNEL_ZIP" -t "${TELEGRAM_TOKEN}" -c "${CHATIDQ}" 
echo "Kernel uploaded to telegram..."

END=$(TZ=Asia/Jakarta date +"%s")
DIFF=$(( END - START ))
tg_cast "Build for ${DEVICE} with ${COMPILER_STRING} <b>succeed</b> took $((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)! by @zh4ntech"

scp "$HOME"/kernel/$FINAL_KERNEL_IMG zhantech@frs.sourceforge.net:/home/frs/project/zhantech/Pringgodani/bengal
scp "$HOME"/kernel/$FINAL_KERNEL_ZIP zhantech@frs.sourceforge.net:/home/frs/project/zhantech/Pringgodani/topaz-xun

echo "Kernel uploaded to sourceforge..."

rm -rf "$HOME"/kernel
}

# eksekusi
    echo ".........................."
    echo ".     Clean Directory    ."
    echo ".........................."
clean
    echo ".........................."
    echo ".     Cloning            ."
    echo ".........................."
cloning
    echo ".........................."
    echo ".     Building Kernel    ."
    echo ".........................."
compile_kernel
    echo ".........................."
    echo ".     Ziping Kernel      ."
    echo ".........................."
ziping
    echo ".........................."
    echo ".     Uploading Kernel   ."
    echo ".........................."
upload
    echo ".........................."
    echo ".     Build Finished     ."
    echo ".........................."
