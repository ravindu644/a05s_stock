#!/bin/bash
export RDIR="$(pwd)"

if [ ! -d "${RDIR}/kernel_platform/prebuilts" ]; then
    echo -e "[+] Downloading Toolchain...\n"
    curl -LO --progress-bar https://github.com/ravindu644/a05s_stock/releases/download/toolchain/toolchain.zip
    curl -LO --progress-bar https://github.com/ravindu644/a05s_stock/releases/download/toolchain/toolchain.z01
    zip -s- toolchain.zip -O combined.zip && unzip combined.zip && rm combined.zip
    tar -xvf toolchain.tar.gz
    mv prebuilts "${RDIR}/kernel_platform" && chmod +x -R "${RDIR}/kernel_platform/prebuilts"
    rm toolchain*
    sudo apt install rsync > /dev/null 2>&1
else
    echo -e "[+] Toolchain already installed...\n"    
fi

cd "${RDIR}/kernel_platform"

#kernel_platform/out/msm-kernel-m269-consolidate/gki_kernel/common/.config is the path for final config
#HERMETIC_TOOLCHAIN=0 is required for menuconfig to load external libraries
HERMETIC_TOOLCHAIN=0 BUILD_CONFIG=msm-kernel/build.config.msm.m269.sec LTO=thin build/config.sh
HERMETIC_TOOLCHAIN=0 BUILD_CONFIG=msm-kernel/build.config.msm.m269.sec LTO=thin build/build.sh