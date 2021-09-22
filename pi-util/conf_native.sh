echo "Configure for native build"

FFSRC=`pwd`
MC=`uname -m`

#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

if [ "$MC" == "aarch64" ]; then
  echo "M/C aarch64"
  A=aarch64-linux-gnu
  B=arm64
  MCOPTS=
  RPI_INCLUDES=
  RPI_LIBDIRS=
  RPI_DEFINES=
  RPI_EXTRALIBS=
  RPIOPTS="--disable-mmal --enable-sand"
else
  echo "M/C armv7"
  A=arm-linux-gnueabihf
  B=armv7
  MCOPTS="--arch=armv6t2 --cpu=cortex-a7"
  RPI_OPT_VC=/opt/vc
  RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
  RPI_LIBDIRS="-L$RPI_OPT_VC/lib"
  RPI_DEFINES="-D__VCCOREVER__=0x4000000 -mfpu=neon-vfpv4"
  RPI_EXTRALIBS="-Wl,--start-group -lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm -lvchostif -lvchiq_arm -Wl,--end-group"
  RPIOPTS="--enable-mmal --enable-rpi"
fi
C=`lsb_release -sc`
V=`cat RELEASE`

SHARED_LIBS="--enable-shared"
if [ "$1" == "--noshared" ]; then
  SHARED_LIBS="--disable-shared"
  OUT=out/$B-$C-$V-static-rel
  echo Static libs
else
  echo Shared libs
  OUT=out/$B-$C-$V-shared-rel
fi

USR_PREFIX=$FFSRC/$OUT/install
LIB_PREFIX=$USR_PREFIX/lib/$A
INC_PREFIX=$USR_PREFIX/include/$A

echo Destination directory: $OUT
mkdir -p $FFSRC/$OUT
cd $FFSRC/$OUT

$FFSRC/configure \
 --prefix=$USR_PREFIX\
 --libdir=$LIB_PREFIX\
 --incdir=$INC_PREFIX\
 $MCOPTS\
 --disable-stripping\
 --disable-thumb\
 --enable-v4l2-request\
 --enable-libdrm\
 --enable-epoxy\
 --enable-libudev\
 --enable-vout-drm\
 --enable-vout-egl\
 $SHARED_LIBS\
 $RPIOPTS\
 --extra-cflags="-ggdb $RPI_KEEPS $RPI_DEFINES $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_DEFINES $RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS"\
 --extra-libs="$RPI_EXTRALIBS"\
 --extra-version="rpi"

# --enable-decoder=hevc_rpi\
# --enable-extra-warnings\
# --arch=armv71\

# gcc option for getting asm listing
# -Wa,-ahls
