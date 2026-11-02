cmake_minimum_required(VERSION 3.16)
project(AtracDEnc_AAL VERSION 0.3.0 LANGUAGES C CXX)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/core/atracdenc-master/src)
set(CORE_SOURCES
    atrac3denc.cpp atrac/at3/atrac3.cpp atrac/at3/atrac3_bitstream.cpp
    atrac/at1/atrac1.cpp atrac/at3p/at3p_tables.cpp atrac/at3p/at3p_bitstream.cpp
    atrac/at3p/at3p_mdct.cpp atrac/atrac_scale.cpp
    atrac/atrac_enc_cache.cpp atrac/atrac_psy_common.cpp lib/mdct/mdct.cpp
    lib/bitstream/bitstream.cpp lib/bs_encode/encode.cpp qmf/qmf.cpp env.cpp
    transient_detector.cpp transient_spectral_upsampler.cpp)
list(TRANSFORM CORE_SOURCES PREPEND "${VENDOR}/")
set(FFT_SOURCES ${VENDOR}/lib/fft/kissfft_impl/kiss_fft.c
                ${VENDOR}/lib/fft/kissfft_impl/tools/kiss_fftr.c
                ${VENDOR}/atrac/atrac3plus_pqf/atrac3plus_pqf.c)
set_source_files_properties(${FFT_SOURCES} PROPERTIES COMPILE_DEFINITIONS "kiss_fft_scalar=float")
add_library(independent_aal STATIC ${CORE_SOURCES} ${FFT_SOURCES}
    core/core_adapter.cpp core/independent_fixed_transform.cpp
    core/independent_fixed_qmf.cpp compression/analysis.cpp
    compression/independent_lpc.cpp compression/decorrelation.cpp
    core/plus/plus_core_adapter.cpp core/plus/parser/plus_core_parser.cpp
    core/plus/fixed_plus_predictor.cpp core/plus/dequant/independent_plus_dequant.cpp
    core/plus/imdct/independent_plus_transform.cpp core/plus/ipqf/independent_plus_ipqf.cpp
    src/encoder.cpp src/c_api.cpp
    core/decode/aal3/aal3_integer_gain.cpp
    core/decode/aalp/core_reader.cpp
    core/decode/aalp/fixed_predictor.cpp
    core/decode/aalp/aalp_integer_gain.cpp
    src/decoder.cpp
    src/decoder_c_api.cpp)
add_library(AtracDEncAAL::encoder ALIAS independent_aal)
add_library(AtracDEncAAL::decoder ALIAS independent_aal)
set_target_properties(independent_aal PROPERTIES OUTPUT_NAME atracdenc_aal)
target_include_directories(independent_aal PUBLIC include PRIVATE
    ${VENDOR} ${VENDOR}/lib ${VENDOR}/lib/fft/kissfft_impl core core/plus)
target_compile_definitions(independent_aal PRIVATE CORE_INDEPENDENT)
if(MSVC)
    target_compile_options(independent_aal PRIVATE /utf-8 /fp:strict)
else()
    target_compile_options(independent_aal PRIVATE -ffp-contract=off)
endif()
add_executable(atracdenc-aal src/cli.cpp)
target_link_libraries(atracdenc-aal PRIVATE independent_aal)
option(AAL_BUILD_GUI "Build the optional Windows test interface" OFF)
if(WIN32 AND AAL_BUILD_GUI)
    add_executable(AtracDEnc-AAL-GUI WIN32 src/native_gui.cpp)
    target_link_libraries(AtracDEnc-AAL-GUI PRIVATE independent_aal comctl32 comdlg32 shell32 ole32 gdi32 user32)
    if(MSVC)
        target_compile_options(AtracDEnc-AAL-GUI PRIVATE /utf-8 /UNDEBUG)
    elseif(MINGW)
        target_compile_options(AtracDEnc-AAL-GUI PRIVATE -UNDEBUG)
        target_link_options(AtracDEnc-AAL-GUI PRIVATE -municode -Wl,--subsystem,windows)
    endif()
endif()
if(MINGW)
    target_link_options(atracdenc-aal PRIVATE -municode)
endif()
install(TARGETS atracdenc-aal independent_aal RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(FILES include/independent_aal.h DESTINATION include)
include(CTest)
if(BUILD_TESTING)
    add_executable(aal-conformance tests/check_conformance.cpp)
    target_link_libraries(aal-conformance PRIVATE independent_aal)
    if(MINGW)
        target_link_options(aal-conformance PRIVATE -municode)
    endif()
    add_test(NAME official_enhancement_conformance COMMAND aal-conformance
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/enhancement-vectors.bin)
    add_test(NAME official_plus_enhancement_conformance COMMAND aal-conformance
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/plus-enhancement-vectors.bin)
    add_executable(aal-c-api-test tests/check_c_api.c)
    target_link_libraries(aal-c-api-test PRIVATE independent_aal)
    add_test(NAME c_api_profiles COMMAND aal-c-api-test)
    add_executable(aal-gui-i18n-test tests/check_gui_i18n.cpp)
    if(MSVC)
        target_compile_options(aal-gui-i18n-test PRIVATE /utf-8)
    endif()
    add_test(NAME gui_language_selection COMMAND aal-gui-i18n-test)
    add_test(NAME encoder_round_trip_all_profiles COMMAND atracdenc-aal --self-test-all)
    add_executable(aal-decoder-test tests/check_decoder.cpp tests/check_decode_c_api.c)
    target_link_libraries(aal-decoder-test PRIVATE independent_aal)
    add_test(NAME official_aal_decode COMMAND aal-decoder-test ${CMAKE_CURRENT_SOURCE_DIR}/tests/decoder-vectors.bin)
endif()
