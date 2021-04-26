set -e

make install

cp -r install/* ../vlc/sysroot/raspian_stretch_pi1-sysroot/usr
