#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u



OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
NUM_CORES=$(nproc)
SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)

echo "Zane printing sysroot"
echo ${SYSROOT}
exit

# SYSROOT=/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu
# export PATH="/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/:$PATH"echo "ROOT=${ROOT-<unset>}"
# Docker sysroot V
# /usr/local/arm-cross-compiler/install/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/../aarch64-none-linux-gnu/libc

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
if [ ! -d "${OUTDIR}" ]; then
    echo OUTDIR could not be created.

fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [  ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    # TODO: Add your kernel build steps here
    echo "Attempting make"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "Done cleaning"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "Done making defconfig"
    time make -j"$NUM_CORES" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "Done making all"
    time make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    echo "Done making modules"
    time make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/arm64/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
pwd
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
ls -a


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    

else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
echo "Done with distclean"
make defconfig
echo "Done with defconfig"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
echo "Done making defconfig"
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
echo "Done with busybox install"

echo "Library dependencies"
pwd

echo "Verifying busybox and /bin/sh"
ls -l ${OUTDIR}/rootfs/bin/busybox ${OUTDIR}/rootfs/bin/sh

INTERP="$(${CROSS_COMPILE}readelf -l ${OUTDIR}/rootfs/bin/busybox | awk -F': ' '/Requesting program interpreter/ {print $2}')"
echo "Busybox interpreter is: $INTERP"


${CROSS_COMPILE}readelf -a busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
if [ -e "${SYSROOT}/lib64/libm.so.6" ]; then
cp ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
echo "Moved libm.so.6" file zane!""
else
echo "sucks to be you"
exit
fi

if [ -e "${SYSROOT}/lib64/libresolv.so.2" ]; then
cp ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
echo "Moved libresolv.so.2 zane!"
else
echo "sucks to be you"
exit
fi

if [ -e "${SYSROOT}/lib64/libc.so.6" ]; then
cp ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
echo "Moved libc.so.6 zane!"
else
echo "sucks to be you"
exit
fi
# /arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib
if [ -e "${SYSROOT}/lib/ld-linux-aarch64.so.1" ]; then
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib

if [ ! -e "${SYSROOT}/lib/ld-linux-aarch64.so.1" ]; then
echo "Lied about moving linker"
exit
fi

echo "Moved ld-linux-aarch64.so.1!"
else
echo "sucks to be you"
exit
fi

# TODO: Make device nodes
cd ${OUTDIR}/rootfs
pwd
sudo mknod -m 666 dev/null c 1 3
echo "made dev/null"
sudo mknod -m 666 dev/console c 5 1
echo "made dev/console"

# TODO: Clean and build the writer utility
cd /home/zane/Documents/AESD/AESD-assignment3/finder-app
make clean
make TARGET=writer CROSS_COMPILE=-aarch64-none-linux-gnu-

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

mv writer ${OUTDIR}/rootfs/home 
cp finder.sh ${OUTDIR}/rootfs/home
mkdir ${OUTDIR}/rootfs/home/conf/
cp ./conf/assignment.txt ${OUTDIR}/rootfs/home/conf/
cp ./conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root .
echo "Chowned."

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}
rm -rf initramfs.cpio.gz

cd ${OUTDIR}/rootfs
pwd
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f "${OUTDIR}/initramfs.cpio"

echo "Done building"

