echo "Configure for Pi1"

RPI_TOOLROOT=`pwd`/../tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf
RPI_OPT_VC=`pwd`/../firmware/hardfp/opt/vc

RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
RPI_LIBDIRS="-L$RPI_TOOLROOT/lib -L$RPI_OPT_VC/lib"
#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

./configure --enable-cross-compile\
 --cpu=arm1176jzf-s\
 --arch=arm\
 --disable-neon\
 --target-os=linux\
 --disable-stripping\
 --enable-mmal\
 --enable-shared\
 --extra-cflags="-g $RPI_KEEPS $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS -Wl,-rpath=/opt/vc/lib,-rpath-link=$RPI_OPT_VC/lib,-rpath=/lib,-rpath=/usr/lib,-rpath-link=$RPI_TOOLROOT/lib,-rpath-link=$RPI_TOOLROOT/lib"\
 --extra-libs="-Wl,--start-group -lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm -lvchostif -lvchiq_arm"\
 --cross-prefix=$RPI_TOOLROOT/bin/arm-linux-gnueabihf-


# --enable-extra-warnings\
# --arch=armv71\
# --enable-shared\

# gcc option for getting asm listing
# -Wa,-ahls
