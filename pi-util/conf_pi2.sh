echo "Configure for Pi2/3"

RPI_TOOLROOT=`pwd`/../tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf
RPI_OPT_VC=`pwd`/../firmware/hardfp/opt/vc

RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
RPI_LIBDIRS="-L$RPI_TOOLROOT/lib -L$RPI_OPT_VC/lib"
RPI_DEFINES="-D__VCCOREVER__=0x4000000 -mfpu=neon-vfpv4"
#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

USR_PREFIX=`pwd`/install
LIB_PREFIX=$USR_PREFIX/lib/arm-linux-gnueabihf
INC_PREFIX=$USR_PREFIX/include/arm-linux-gnueabihf

./configure --enable-cross-compile\
 --prefix=$USR_PREFIX\
 --libdir=$LIB_PREFIX\
 --incdir=$INC_PREFIX\
 --arch=armv6t2\
 --cpu=cortex-a7\
 --target-os=linux\
 --disable-stripping\
 --disable-thumb\
 --enable-mmal\
 --enable-rpi\
 --extra-cflags="-ggdb $RPI_KEEPS $RPI_DEFINES $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_DEFINES $RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS -Wl,-rpath=/opt/vc/lib,-rpath-link=$RPI_OPT_VC/lib,-rpath=/lib,-rpath=/usr/lib,-rpath-link=$RPI_TOOLROOT/lib,-rpath-link=$RPI_TOOLROOT/lib"\
 --extra-libs="-Wl,--start-group -lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm -lvchostif -lvchiq_arm"\
 --cross-prefix=$RPI_TOOLROOT/bin/arm-linux-gnueabihf-

# --enable-shared\

# --enable-decoder=hevc_rpi\
# --enable-extra-warnings\
# --arch=armv71\
# --enable-shared\

# gcc option for getting asm listing
# -Wa,-ahls
