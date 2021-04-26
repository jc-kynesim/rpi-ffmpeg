echo "Configure for ARM64 native build"

#RPI_KEEPS="-save-temps=obj"

SHARED_LIBS="--enable-shared"
if [ "$1" == "--noshared" ]; then
  SHARED_LIBS="--disable-shared"
  echo Static libs
  OUT=out/arm64-static-rel
else
  echo Shared libs
  OUT=out/arm64-shared-rel
fi

mkdir -p $OUT
cd $OUT

A=aarch64-linux-gnu
USR_PREFIX=`pwd`/install
LIB_PREFIX=$USR_PREFIX/lib/$A
INC_PREFIX=$USR_PREFIX/include/$A

../../configure \
 --prefix=$USR_PREFIX\
 --libdir=$LIB_PREFIX\
 --incdir=$INC_PREFIX\
 --disable-stripping\
 --disable-thumb\
 --disable-mmal\
 --enable-sand\
 --enable-v4l2-request\
 --enable-libdrm\
 --enable-epoxy\
 --enable-libudev\
 --enable-vout-drm\
 --enable-vout-egl\
 $SHARED_LIBS\
 --extra-cflags="-ggdb"

# --enable-decoder=hevc_rpi\
# --enable-extra-warnings\
# --arch=armv71\

# gcc option for getting asm listing
# -Wa,-ahls
