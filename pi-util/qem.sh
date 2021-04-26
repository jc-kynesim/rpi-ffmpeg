TARGET_DIR=../src/eupton_vc4dev_2012a/software/vc4/DEV/applications/tutorials/user_shader_example_tex
QASM=python\ ../local/bin/qasm.py
SRC_FILE=libavcodec/rpi_hevc_shader.qasm
DST_BASE=shader

cp libavcodec/rpi_hevc_shader_cmd.h $TARGET_DIR
$QASM -mc_c:$DST_BASE,$DST_BASE,$DST_BASE $SRC_FILE > $TARGET_DIR/$DST_BASE.c
$QASM -mc_h:$DST_BASE,$DST_BASE,$DST_BASE $SRC_FILE > $TARGET_DIR/$DST_BASE.h

