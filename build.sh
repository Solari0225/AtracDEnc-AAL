#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
cc="${CC:-cc}"
cxx="${CXX:-c++}"
vendor="$PWD/core/atracdenc-master/src"
mkdir -p build/native/obj bin
include=("-I$vendor" "-I$vendor/lib" "-I$vendor/lib/fft/kissfft_impl" "-I$PWD/core" "-I$PWD/core/plus")
common=(-O2 -DNDEBUG -DCORE_INDEPENDENT -ffp-contract=off)
if [[ "$(uname -s)" == Darwin ]]; then common+=(-mmacosx-version-min=11.0); fi
objects=()
for source in lib/fft/kissfft_impl/kiss_fft.c lib/fft/kissfft_impl/tools/kiss_fftr.c atrac/atrac3plus_pqf/atrac3plus_pqf.c; do
    object="build/native/obj/$(basename "$source").o"
    "$cc" "${common[@]}" -Dkiss_fft_scalar=float "${include[@]}" "$@" -c "$vendor/$source" -o "$object"
    objects+=("$object")
done
sources=(
    "$vendor/atrac3denc.cpp" "$vendor/atrac/at3/atrac3.cpp"
    "$vendor/atrac/at3/atrac3_bitstream.cpp" "$vendor/atrac/at1/atrac1.cpp"
    "$vendor/atrac/at3p/at3p_tables.cpp" "$vendor/atrac/atrac_scale.cpp"
    "$vendor/atrac/at3p/at3p_bitstream.cpp" "$vendor/atrac/at3p/at3p_mdct.cpp"
    "$vendor/atrac/atrac_enc_cache.cpp" "$vendor/atrac/atrac_psy_common.cpp"
    "$vendor/lib/mdct/mdct.cpp" "$vendor/lib/bitstream/bitstream.cpp"
    "$vendor/lib/bs_encode/encode.cpp" "$vendor/qmf/qmf.cpp" "$vendor/env.cpp"
    "$vendor/transient_detector.cpp" "$vendor/transient_spectral_upsampler.cpp"
    core/core_adapter.cpp core/independent_fixed_transform.cpp
    core/independent_fixed_qmf.cpp compression/analysis.cpp
    compression/independent_lpc.cpp compression/decorrelation.cpp
    core/plus/plus_core_adapter.cpp core/plus/parser/plus_core_parser.cpp
    core/plus/fixed_plus_predictor.cpp core/plus/dequant/independent_plus_dequant.cpp
    core/plus/imdct/independent_plus_transform.cpp core/plus/ipqf/independent_plus_ipqf.cpp
    src/encoder.cpp src/c_api.cpp
    core/decode/aal3/aal3_integer_gain.cpp core/decode/aalp/core_reader.cpp core/decode/aalp/fixed_predictor.cpp core/decode/aalp/aalp_integer_gain.cpp src/decoder.cpp src/decoder_c_api.cpp)
for source in "${sources[@]}"; do
    object="build/native/obj/$(basename "$source").o"
    "$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" -c "$source" -o "$object"
    objects+=("$object")
done
ar rcs bin/libatracdenc_aal.a "${objects[@]}"
"$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" src/cli.cpp "${objects[@]}" -o bin/atracdenc-aal
"$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" tests/check_conformance.cpp "${objects[@]}" -o bin/aal-conformance
"$cc" "${common[@]}" "${include[@]}" "$@" -c tests/check_c_api.c -o build/native/obj/aal-c-api-test.o
"$cxx" "${common[@]}" "$@" build/native/obj/aal-c-api-test.o "${objects[@]}" -o bin/aal-c-api-test
"$cxx" "${common[@]}" -std=c++17 "$@" tests/check_gui_i18n.cpp -o bin/aal-gui-i18n-test
"$cc" "${common[@]}" "$@" -c tests/check_decode_c_api.c -o build/native/obj/check_decode_c_api.o
"$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" tests/check_decoder.cpp build/native/obj/check_decode_c_api.o "${objects[@]}" -o bin/aal-decoder-test
"$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" tests/check_converter.cpp "${objects[@]}" -o bin/aal-converter-test
"$cc" "${common[@]}" "$@" -c tests/check_stream_c_api.c -o build/native/obj/check_stream_c_api.o
"$cxx" "${common[@]}" -std=c++17 "${include[@]}" "$@" tests/check_streaming.cpp build/native/obj/check_stream_c_api.o "${objects[@]}" -o bin/aal-streaming-test
if [[ "$(uname -s)" == Darwin && "${AAL_BUILD_GUI:-1}" != 0 ]]; then
    app="bin/AtracDEnc-AAL-GUI.app"
    mkdir -p "$app/Contents/MacOS"
    version="$(cat VERSION)"
    sed "s/@PROJECT_VERSION@/$version/g" tools/macos/Info.plist.in > "$app/Contents/Info.plist"
    "$cxx" -O2 -std=c++17 -fobjc-arc -fblocks -mmacosx-version-min=11.0 -ffp-contract=off "${include[@]}" "$@" src/macos_gui.mm "${objects[@]}" -framework AppKit -Wl,-dead_strip -o "$app/Contents/MacOS/AtracDEnc-AAL-GUI"
    codesign --force --sign - "$app"
fi
echo "Built the CLI, static library, conformance, C API and GUI language tests in bin/"
