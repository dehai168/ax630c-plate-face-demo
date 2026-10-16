#!/bin/bash
# build_one.sh <name> <onnx>
export COLUMNS=220; N=$1; ONNX=$2; B=/workspace/cn_val_su/improve/build
rm -rf $B/$N; /workspace/dl/pulsar2/ax_pulsar2_8.0_lite_package/bin/pulsar2_fixed build --target_hardware AX620E --input $ONNX --output_dir $B/$N --config $B/$N.json > $B/$N.log 2>&1; echo "$N exit $?"; cp $B/$N/compiled.axmodel $B/$N.axmodel 2>/dev/null; ls -la $B/$N.axmodel
