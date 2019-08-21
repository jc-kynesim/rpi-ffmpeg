set -e

make install

cp -r install/* ../vlc2/sysroot/raspian_stretch_pi1-sysroot/usr
