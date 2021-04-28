echo "Configure for native build"

FFSRC=`pwd`

RPI_OPT_VC=/opt/vc
RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
RPI_LIBDIRS="-L$RPI_OPT_VC/lib"
RPI_DEFINES="-D__VCCOREVER__=0x4000000 -mfpu=neon-vfpv4"
#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

SHARED_LIBS="--enable-shared"
if [ "$1" == "--noshared" ]; then
  SHARED_LIBS="--disable-shared"
  OUT=out/armv7-static-rel
  echo Static libs
else
  echo Shared libs
  OUT=out/armv7-shared-rel
fi

USR_PREFIX=$FFSRC/$OUT/install
LIB_PREFIX=$USR_PREFIX/lib/arm-linux-gnueabihf
INC_PREFIX=$USR_PREFIX/include/arm-linux-gnueabihf

mkdir -p $FFSRC/$OUT
cd $FFSRC/$OUT

$FFSRC/configure \
 --prefix=$USR_PREFIX\
 --libdir=$LIB_PREFIX\
 --incdir=$INC_PREFIX\
 --arch=armv6t2\
 --cpu=cortex-a7\
 --disable-stripping\
 --disable-thumb\
 --enable-mmal\
 --enable-rpi\
 --enable-v4l2-request\
 --enable-libdrm\
 --enable-libudev\
 --enable-vout-drm\
 $SHARED_LIBS\
 --extra-cflags="-ggdb $RPI_KEEPS $RPI_DEFINES $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_DEFINES $RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS"\
 --extra-libs="-Wl,--start-group -lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm -lvchostif -lvchiq_arm"\
 --extra-version="rpi"

# --enable-decoder=hevc_rpi\
# --enable-extra-warnings\
# --arch=armv71\

# gcc option for getting asm listing
# -Wa,-ahls
