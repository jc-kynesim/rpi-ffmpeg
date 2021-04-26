set -e
U=/usr/include/arm-linux-gnueabihf
rm -rf $U/libavcodec
rm -rf $U/libavdevice
rm -rf $U/libavfilter
rm -rf $U/libavformat
rm -rf $U/libavutil
rm -rf $U/libswresample
rm -rf $U/libswscale
U=/usr/include/aarch64-linux-gnu
rm -rf $U/libavcodec
rm -rf $U/libavdevice
rm -rf $U/libavfilter
rm -rf $U/libavformat
rm -rf $U/libavutil
rm -rf $U/libswresample
rm -rf $U/libswscale
U=/usr/lib/arm-linux-gnueabihf
rm -f $U/libavcodec.*
rm -f $U/libavdevice.*
rm -f $U/libavfilter.*
rm -f $U/libavformat.*
rm -f $U/libavutil.*
rm -f $U/libswresample.*
rm -f $U/libswscale.*
U=/usr/lib/arm-linux-gnueabihf/neon/vfp
rm -f $U/libavcodec.*
rm -f $U/libavdevice.*
rm -f $U/libavfilter.*
rm -f $U/libavformat.*
rm -f $U/libavutil.*
rm -f $U/libswresample.*
rm -f $U/libswscale.*
U=/usr/lib/aarch64-linux-gnu
rm -f $U/libavcodec.*
rm -f $U/libavdevice.*
rm -f $U/libavfilter.*
rm -f $U/libavformat.*
rm -f $U/libavutil.*
rm -f $U/libswresample.*
rm -f $U/libswscale.*

