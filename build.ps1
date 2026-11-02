param([string]$Zig='zig',[string]$Target='x86_64-windows-gnu',[switch]$Gui,[string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$vendor=Join-Path $PSScriptRoot 'core\atracdenc-master\src'
$objectsDir=Join-Path $PSScriptRoot ('build\'+$Target+'\obj')
$bin=Join-Path $PSScriptRoot 'bin'
if($OutputDirectory){$bin=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $OutputDirectory))}
New-Item -ItemType Directory -Force -Path $objectsDir,$bin | Out-Null
$include=@(('-I'+$vendor),('-I'+(Join-Path $vendor 'lib')),('-I'+(Join-Path $vendor 'lib\fft\kissfft_impl')),('-I'+(Join-Path $PSScriptRoot 'core')),('-I'+(Join-Path $PSScriptRoot 'core\plus')))
$zigTarget=if($Target -match '^.+-macos$'){$Target+'.11.0'}else{$Target}
$common=@('-target',$zigTarget,'-O2','-DNDEBUG','-DCORE_INDEPENDENT','-ffp-contract=off')
if($Target -match 'macos'){$common+='-mmacosx-version-min=11.0'}
$objects=@()
$cfiles=@('lib\fft\kissfft_impl\kiss_fft.c','lib\fft\kissfft_impl\tools\kiss_fftr.c','atrac\atrac3plus_pqf\atrac3plus_pqf.c')
foreach($file in $cfiles) {
    $object=Join-Path $objectsDir (($file -replace '[\\/]','_')+'.o')
    & $Zig cc @common -Dkiss_fft_scalar=float @include -c (Join-Path $vendor $file) -o $object
    if($LASTEXITCODE -ne 0){throw ('C compilation failed: '+$file)}
    $objects+=$object
}
$cppfiles=@('atrac3denc.cpp','atrac\at3\atrac3.cpp','atrac\at3\atrac3_bitstream.cpp','atrac\at1\atrac1.cpp','atrac\at3p\at3p_tables.cpp','atrac\at3p\at3p_bitstream.cpp','atrac\at3p\at3p_mdct.cpp','atrac\atrac_scale.cpp','atrac\atrac_enc_cache.cpp','atrac\atrac_psy_common.cpp','lib\mdct\mdct.cpp','lib\bitstream\bitstream.cpp','lib\bs_encode\encode.cpp','qmf\qmf.cpp','env.cpp','transient_detector.cpp','transient_spectral_upsampler.cpp')
$sources=@($cppfiles | ForEach-Object {Join-Path $vendor $_})
$sources+=@('core\core_adapter.cpp','core\independent_fixed_transform.cpp','core\independent_fixed_qmf.cpp','compression\analysis.cpp','compression\independent_lpc.cpp','compression\decorrelation.cpp','src\encoder.cpp','src\c_api.cpp') | ForEach-Object {Join-Path $PSScriptRoot $_}
$sources+=@('core\plus\plus_core_adapter.cpp','core\plus\parser\plus_core_parser.cpp','core\plus\fixed_plus_predictor.cpp','core\plus\dequant\independent_plus_dequant.cpp','core\plus\imdct\independent_plus_transform.cpp','core\plus\ipqf\independent_plus_ipqf.cpp') | ForEach-Object {Join-Path $PSScriptRoot $_}
$sources+=@('core\decode\aal3\aal3_integer_gain.cpp','core\decode\aalp\core_reader.cpp','core\decode\aalp\fixed_predictor.cpp','core\decode\aalp\aalp_integer_gain.cpp','src\decoder.cpp','src\decoder_c_api.cpp') | ForEach-Object {Join-Path $PSScriptRoot $_}
foreach($source in $sources) {
    $object=Join-Path $objectsDir (([IO.Path]::GetFileName($source))+'.o')
    & $Zig c++ @common -std=c++17 @include -c $source -o $object
    if($LASTEXITCODE -ne 0){throw ('C++ compilation failed: '+$source)}
    $objects+=$object
}
$link=@()
$name='atracdenc-aal-'+$Target
if($Target -match 'windows'){$link+= '-municode';$name='atracdenc-aal.exe'}
& $Zig c++ @common -std=c++17 -s @include @link (Join-Path $PSScriptRoot 'src\cli.cpp') @objects -o (Join-Path $bin $name)
if($LASTEXITCODE -ne 0){throw 'CLI link failed.'}
$archive=Join-Path $bin ('libatracdenc_aal-'+$Target+'.a')
& $Zig ar rcs $archive @objects
if($LASTEXITCODE -ne 0){throw 'Static library archive failed.'}
$testName='aal-conformance-'+$Target
if($Target -match 'windows'){$testName='aal-conformance.exe'}
& $Zig c++ @common -std=c++17 -s @include @link (Join-Path $PSScriptRoot 'tests\check_conformance.cpp') @objects -o (Join-Path $bin $testName)
if($LASTEXITCODE -ne 0){throw 'Conformance test link failed.'}
$apiTestName='aal-c-api-test-'+$Target
if($Target -match 'windows'){$apiTestName='aal-c-api-test.exe'}
$apiTestObject=Join-Path $objectsDir 'aal-c-api-test.o'
& $Zig cc @common @include -c (Join-Path $PSScriptRoot 'tests\check_c_api.c') -o $apiTestObject
if($LASTEXITCODE -ne 0){throw 'C API test compilation failed.'}
& $Zig c++ @common -s $apiTestObject @objects -o (Join-Path $bin $apiTestName)
if($LASTEXITCODE -ne 0){throw 'C API test link failed.'}
$languageTestName='aal-gui-i18n-test-'+$Target
if($Target -match 'windows'){$languageTestName='aal-gui-i18n-test.exe'}
& $Zig c++ @common -std=c++17 -s (Join-Path $PSScriptRoot 'tests\check_gui_i18n.cpp') -o (Join-Path $bin $languageTestName)
if($LASTEXITCODE -ne 0){throw 'GUI language test build failed.'}
$decoderTestName='aal-decoder-test-'+$Target
if($Target -match 'windows'){$decoderTestName='aal-decoder-test.exe'}
$decoderCObject=Join-Path $objectsDir 'check_decode_c_api.o'
& $Zig cc @common -c (Join-Path $PSScriptRoot 'tests\check_decode_c_api.c') -o $decoderCObject
if($LASTEXITCODE -ne 0){throw 'Decoder C ABI test compilation failed.'}
& $Zig c++ @common -std=c++17 -s @include (Join-Path $PSScriptRoot 'tests\check_decoder.cpp') $decoderCObject @objects -o (Join-Path $bin $decoderTestName)
if($LASTEXITCODE -ne 0){throw 'Decoder test link failed.'}
if($Gui) {
    if($Target -notmatch 'windows'){throw 'The optional test GUI is Windows only.'}
    $guiCommon=@($common | Where-Object {$_ -ne '-DNDEBUG'})
    & $Zig c++ @guiCommon -std=c++17 -s @include -municode '-Wl,--subsystem,windows' (Join-Path $PSScriptRoot 'src\native_gui.cpp') @objects -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -luser32 -o (Join-Path $bin 'AtracDEnc-AAL-GUI.exe')
    if($LASTEXITCODE -ne 0){throw 'Test GUI link failed.'}
}
Write-Output $bin
