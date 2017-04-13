TARGET_DIR=../src/eupton_vc4dev_2012a/software/vc4/DEV/applications/tutorials/user_shader_example_tex
QASM=python\ pi-util/qasm.py
SRC_FILE=libavcodec/rpi_shader.qasm
DST_BASE=shader

$QASM -mc_c:$DST_BASE,$DST_BASE,$DST_BASE $SRC_FILE > $TARGET_DIR/$DST_BASE.c
$QASM -mc_h:$DST_BASE,$DST_BASE,$DST_BASE $SRC_FILE > $TARGET_DIR/$DST_BASE.h

