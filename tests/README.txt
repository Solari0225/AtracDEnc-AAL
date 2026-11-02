Run the standalone conformance target against each of the two fixtures.
enhancement-vectors.bin: AAL3, 3017 packets / 5855736 payload bytes, version1.
plus-enhancement-vectors.bin: AALP, 1566 packets / 4071400 payload bytes, version2.
Each comparison requires the complete official payload and padding field to
agree byte-for-byte, and verifies independent residual decoding as well.
The data contain project-generated synthetic audio residuals and original
Sony encoded data, including true first/last alignment and flush samples.
No Sony program or machine-code capture is included or needed to run tests.
See fixtures.json and plus-fixtures.json for layouts, case order and hashes.
Version1: AALV, LE32 version=1,count; records LE32 payloadBytes,padding;
         planar left/right signed16LE1024; official payload.
Version2: AALV, LE32 version=2,count; records LE32 frameSamples,payloadBytes,
         padding; planar left/right signed16LE[frameSamples]; official payload.
The v2 data cover all nine AALP profiles, LPC, fixed prediction, all three
decorrelation choices, Rice coding and raw fallback (18 raw /1548 coded).

macOS: bash build.sh
       ./bin/aal-conformance tests/enhancement-vectors.bin
       ./bin/aal-conformance tests/plus-enhancement-vectors.bin
       ./bin/aal-c-api-test
       ./bin/atracdenc-aal --self-test-all
Windows: powershell -File build.ps1 -Zig C:\tools\zig\zig.exe
         .\bin\aal-conformance.exe tests\enhancement-vectors.bin
         .\bin\aal-conformance.exe tests\plus-enhancement-vectors.bin
         .\bin\aal-c-api-test.exe
         .\bin\atracdenc-aal.exe --self-test-all
Both builds also produce the encoder CLI and static library.

decoder-vectors.bin: complete independently decoded original AAL and varied
synthetic core syntax, with exact reference signed16LE stereo PCM.
Format: AALDV001, LE32 count. Each record: LE32 nameBytes,omaBytes,pcmBytes;
UTF-8 name; complete unencrypted OMA (metadata removed); reference PCM.
See decoder-vectors.json for cases, lengths, hashes and reference scope.
Run aal-decoder-test tests/decoder-vectors.bin (add .exe on Windows).
The decoder tests also cover ownership, C ABI, cancellation and malformed data.
