set -e

if [ "$1" == "" ]; then
  echo Usage: $0 \<src_dir\> [\<rootname\>]
  echo src_dir is a source for rsync so may contain m/c name.
  echo rootname will be set to \"raspian_jessie_pi1\" if missing
  echo e.g.: pi-util/syncroot.sh my-pi: raspian_jessie_pi1
  exit 1
fi

SYSROOT_NAME=$2
if [ "$SYSROOT_NAME" == "" ]; then
  SYSROOT_NAME=raspian_jessie_pi1
fi

DST_ROOT=`pwd`
DST=$DST_ROOT/build/linux/$SYSROOT_NAME-sysroot
SRC=$1

echo Sync src:  $SRC
echo Sync dest: $DST

mkdir -p $DST/lib
mkdir -p $DST/opt/vc/include
mkdir -p $DST/usr/lib/pkgconfig
mkdir -p $DST/usr/bin
mkdir -p $DST/usr/share

#### MUST NOT include /opt/vc/include/*GL*
# Creates conflicts with GL includes inside Chrome

rsync -rl $SRC/lib/arm-linux-gnueabihf $DST/lib
rsync -rl $SRC/opt/vc/lib $DST/opt/vc
rsync -l  $SRC/opt/vc/include/bcm_host.h $DST/opt/vc/include
rsync -rl $SRC/opt/vc/include/interface $DST/opt/vc/include
rsync -rl $SRC/opt/vc/include/vcinclude $DST/opt/vc/include
rsync -rl $SRC/usr/lib/arm-linux-gnueabihf $DST/usr/lib
rsync -rl $SRC/usr/lib/gcc $DST/usr/lib
rsync -rl $SRC/usr/include $DST/usr

pi-util/rebase_liblinks.py $DST


