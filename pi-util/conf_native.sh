echo "Configure for native build"

FFSRC=`pwd`
MC=`dpkg --print-architecture`
BUILDBASE=$FFSRC/out

#RPI_KEEPS="-save-temps=obj"
RPI_KEEPS=""

NOSHARED=
MMAL=

while [ "$1" != "" ] ; do
    case $1 in
	--noshared)
	    NOSHARED=1
	    ;;
	--mmal)
	    MMAL=1
	    ;;
	*)
	    echo "Usage $0: [--noshared] [--mmal]"
	    exit 1
	    ;;
    esac
    shift
done


MCOPTS=
RPI_INCLUDES=
RPI_LIBDIRS=
RPI_DEFINES=
RPI_EXTRALIBS=

if [ "$MC" == "arm64" ]; then
  echo "M/C aarch64"
  A=aarch64-linux-gnu
  B=arm64
elif [ "$MC" == "armhf" ]; then
  echo "M/C armv7"
  A=arm-linux-gnueabihf
  B=armv7
  MCOPTS="--arch=armv6t2 --cpu=cortex-a7"
  RPI_DEFINES=-mfpu=neon-vfpv4
else
  echo Unexpected architecture $MC
  exit 1
fi

if [ $MMAL ]; then
  RPI_OPT_VC=/opt/vc
  RPI_INCLUDES="-I$RPI_OPT_VC/include -I$RPI_OPT_VC/include/interface/vcos/pthreads -I$RPI_OPT_VC/include/interface/vmcs_host/linux"
  RPI_LIBDIRS="-L$RPI_OPT_VC/lib"
  RPI_DEFINES="$RPI_DEFINES -D__VCCOREVER__=0x4000000"
  RPI_EXTRALIBS="-Wl,--start-group -lbcm_host -lmmal -lmmal_util -lmmal_core -lvcos -lvcsm -lvchostif -lvchiq_arm -Wl,--end-group"
  RPIOPTS="--enable-mmal"
else
  RPIOPTS="--disable-mmal"
fi

C=`lsb_release -sc`
V=`cat RELEASE`

SHARED_LIBS="--enable-shared"
if [ $NOSHARED ]; then
  SHARED_LIBS="--disable-shared"
  OUT=$BUILDBASE/$B-$C-$V-static-rel
  echo Static libs
else
  echo Shared libs
  OUT=$BUILDBASE/$B-$C-$V-shared-rel
fi

USR_PREFIX=$OUT/install
LIB_PREFIX=$USR_PREFIX/lib/$A
INC_PREFIX=$USR_PREFIX/include/$A

echo Destination directory: $OUT
mkdir -p $OUT
# Nothing under here need worry git - including this .gitignore!
echo "**" > $BUILDBASE/.gitignore
cd $OUT

$FFSRC/configure \
 --prefix=$USR_PREFIX\
 --libdir=$LIB_PREFIX\
 --incdir=$INC_PREFIX\
 $MCOPTS\
 --disable-stripping\
 --disable-thumb\
 --enable-sand\
 --enable-v4l2-request\
 --enable-libdrm\
 --enable-vout-egl\
 --enable-vout-drm\
 --enable-gpl\
 $SHARED_LIBS\
 $RPIOPTS\
 --extra-cflags="-ggdb $RPI_KEEPS $RPI_DEFINES $RPI_INCLUDES"\
 --extra-cxxflags="$RPI_DEFINES $RPI_INCLUDES"\
 --extra-ldflags="$RPI_LIBDIRS"\
 --extra-libs="$RPI_EXTRALIBS"\
 --extra-version="rpi"


# gcc option for getting asm listing
# -Wa,-ahls
