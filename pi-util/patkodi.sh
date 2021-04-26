set -e
KODIBASE=/home/jc/rpi/kodi/xbmc
JOBS=-j20
make $JOBS
git diff xbmc/release/4.3-kodi > $KODIBASE/tools/depends/target/ffmpeg/pfcd_hevc_optimisations.patch
make -C $KODIBASE/tools/depends/target/ffmpeg $JOBS
make -C $KODIBASE/build install


