echo "Configure for Pi2"

RPI_ROOT=/home/jc/rpi
RPI_ROOTFS=$RPI_ROOT/opt/pi2_rootfs
RPI_OPT_VC=$RPI_ROOTFS/opt/vc
RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_ROOTFS/usr/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
#RPI_DEFS="-D__VCCOREVER__=0x04000000 -DRPI=1"
RPI_DEFS="-D__VCCOREVER__=0x04000000"
RPI_LIBDIRS="-L$RPI_ROOTFS/lib -L$RPI_ROOTFS/usr/lib -L$RPI_OPT_VC/lib"
#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

./configure --enable-cross-compile\
 --arch=armv71\
 --cpu=cortex-a7\
 --target-os=linux\
 --disable-thumb\
 --extra-cflags="$RPI_KEEPS $RPI_DEFS $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_DEFS $RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS -Wl,-rpath=/opt/vc/lib,-rpath-link=$RPI_OPT_VC/lib,-rpath=/lib,-rpath=/usr/lib,-rpath-link=$RPI_ROOTFS/lib,-rpath-link=$RPI_ROOTFS/usr/lib"\
 --extra-libs="-lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm"\
 --cross-prefix=$RPI_ROOT/opt/toolchains/gcc-linaro-5.1-2015.08-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

# --enable-shared\

# gcc option for getting asm listing
# -Wa,-ahls
