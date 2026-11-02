#pragma once
#include <filesystem>
#include <string>
namespace atracdenc_aal_gui {
inline bool is_decode_input(const std::filesystem::path& input) {
    auto extension = input.extension().u8string();
    for (char& c : extension) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    return extension == ".oma" || extension == ".aa3";
}
inline const wchar_t* output_extension(const std::filesystem::path& input) {
    return is_decode_input(input) ? L".wav" : L".oma";
}
}
