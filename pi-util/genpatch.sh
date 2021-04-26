set -e

NOPATCH=
if [ "$1" == "--notag" ]; then
  shift
  NOPATCH=1
fi

if [ "$1" == "" ]; then
  echo Usage: $0 [--notag] \<patch_tag\>
  echo e.g.: $0 mmal_4
  exit 1
fi

VERSION=`cat RELEASE`
if [ "$VERSION" == "" ]; then
  echo Can\'t find version RELEASE
  exit 1
fi

PATCHFILE=../ffmpeg-$VERSION-$1.patch

if [ $NOPATCH ]; then
  echo Not tagged
else
  # Only continue if we are all comitted
  git diff --name-status --exit-code

  PATCHTAG=pi/$VERSION/$1
  echo Tagging: $PATCHTAG

  git tag $PATCHTAG
fi
echo Generating patch: $PATCHFILE
git diff n$VERSION -- > $PATCHFILE
