#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
namespace atracdenc_aal_gui {
inline constexpr uint64_t max_input_bytes = 1024ULL * 1024 * 1024;
inline std::string extension(const std::filesystem::path& input) {
    auto value = input.extension().u8string();
    for (char& c : value) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    return value;
}
inline bool is_decode_input(const std::filesystem::path& input) {
    const auto value = extension(input);
    return value == ".oma" || value == ".aa3";
}
inline const wchar_t* output_extension(const std::filesystem::path& input) {
    return is_decode_input(input) ? L".wav" : L".oma";
}
inline uint16_t read_le16(const unsigned char* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
inline uint32_t read_le32(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
// Bounded, header-only queue validation shared by both native interfaces.
// The codec repeats authoritative validation before publishing any output.
inline const wchar_t* input_problem(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) return L"不是可读取的音频文件";
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) return L"不是可读取的音频文件";
    if (bytes > max_input_bytes) return L"输入文件超过 1 GiB 上限";
    std::ifstream input(path, std::ios::binary);
    if (!input) return L"不是可读取的音频文件";
    if (is_decode_input(path)) {
        std::array<unsigned char, 96> header{};
        if (!input.read(reinterpret_cast<char*>(header.data()), 10)) return L"AAL 文件头不完整";
        uint64_t offset = 0;
        if (!std::memcmp(header.data(), "ea3", 3) || !std::memcmp(header.data(), "ID3", 3)) {
            for (unsigned i=6; i<10; ++i) {
                if (header[i] & 0x80) return L"AAL 文件头不完整";
                offset = (offset << 7) | header[i];
            }
            offset += 10;
        }
        if (offset > bytes || bytes - offset < header.size()) return L"AAL 文件头不完整";
        input.seekg(std::streamoff(offset));
        if (!input.read(reinterpret_cast<char*>(header.data()), header.size()) || std::memcmp(header.data(), "EA3", 3))
            return L"仅支持未加密的 AAL3 / AALP OMA/AA3";
        if (header[32] != 0x21 && header[32] != 0x22) return L"仅支持未加密的 AAL3 / AALP OMA/AA3";
        return nullptr;
    }
    if (extension(path) != ".wav") return L"请选择 PCM WAV 或 AAL OMA/AA3 文件";
    std::array<unsigned char, 12> riff{};
    if (!input.read(reinterpret_cast<char*>(riff.data()), riff.size())) return L"WAV 文件头不完整";
    if (std::memcmp(riff.data(), "RIFF", 4) || std::memcmp(riff.data()+8, "WAVE", 4))
        return L"需要标准 RIFF / PCM WAV 文件";
    uint64_t offset=12;
    for (unsigned chunks=0; chunks<4096 && offset+8<=bytes; ++chunks) {
        std::array<unsigned char, 8> chunk{};
        input.seekg(std::streamoff(offset));
        if (!input.read(reinterpret_cast<char*>(chunk.data()), chunk.size())) break;
        const uint32_t size=read_le32(chunk.data()+4);
        if (size > bytes-offset-8) return L"WAV 格式信息不完整";
        if (!std::memcmp(chunk.data(), "fmt ", 4)) {
            std::array<unsigned char, 16> format{};
            if (size<16 || !input.read(reinterpret_cast<char*>(format.data()), format.size())) return L"WAV 格式信息不完整";
            if (read_le16(format.data())!=1 || read_le16(format.data()+2)!=2 ||
                read_le32(format.data()+4)!=44100 || read_le16(format.data()+14)!=16 ||
                read_le16(format.data()+12)!=4 || read_le32(format.data()+8)!=176400)
                return L"仅支持 44.1 kHz、16 位、双声道 PCM WAV";
            return nullptr;
        }
        offset+=8ULL+size+(size&1u);
    }
    return L"未找到有效的 PCM 格式信息";
}
inline std::filesystem::path vacant_output(const std::filesystem::path& folder, const std::filesystem::path& input) {
    auto stem=input.stem();
    if (stem.empty()) stem="audio";
    for (unsigned suffix=1; suffix<1000000; ++suffix) {
        auto name=stem;
        if (suffix!=1) name+=" ("+std::to_string(suffix)+")";
        name+=is_decode_input(input)?".wav":".oma";
        auto output=folder/name, report=output;
        report+=".json";
        if (!std::filesystem::exists(output) && !std::filesystem::exists(report)) return output;
    }
    throw std::runtime_error("No unused output filename is available.");
}
}
