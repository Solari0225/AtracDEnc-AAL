#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace independent_aal {
enum class CoreCodec { Atrac3, Atrac3plus };
struct EncodingProfile {
    const char* id;
    const char* container_name;
    CoreCodec codec;
    uint32_t nominal_bitrate;
    uint16_t core_bytes;
    uint16_t frame_samples;
    uint16_t pcm_trim;
    uint8_t oma_codec_id;
    uint8_t oma_frame_parameters;
    uint32_t core_parameters;
    constexpr double actual_core_bitrate() const {return double(core_bytes)*8*44100/frame_samples;}
};
// Only configurations individually accepted and PCM-round-tripped by Sony
// SMLE 7.1.00.11181. Ordinary lossy ATRAC3 rates are not AAL3 profiles.
inline constexpr std::array<EncodingProfile,10> kOfficialProfiles={{
    {"aal3-132","AAL3",CoreCodec::Atrac3,132000,384,1024,138,0x22,0x52,0x002030},
    {"aalp-48","AALP",CoreCodec::Atrac3plus,48000,280,2048,368,0x21,0x91,0x002822},
    {"aalp-64","AALP",CoreCodec::Atrac3plus,64000,376,2048,368,0x21,0x91,0x00282e},
    {"aalp-96","AALP",CoreCodec::Atrac3plus,96000,560,2048,368,0x21,0x91,0x002845},
    {"aalp-128","AALP",CoreCodec::Atrac3plus,128000,744,2048,368,0x21,0x91,0x00285c},
    {"aalp-160","AALP",CoreCodec::Atrac3plus,160000,936,2048,368,0x21,0x91,0x002874},
    {"aalp-192","AALP",CoreCodec::Atrac3plus,192000,1120,2048,368,0x21,0x91,0x00288b},
    {"aalp-256","AALP",CoreCodec::Atrac3plus,256000,1488,2048,368,0x21,0x91,0x0028b9},
    {"aalp-320","AALP",CoreCodec::Atrac3plus,320000,1864,2048,368,0x21,0x91,0x0028e8},
    {"aalp-352","AALP",CoreCodec::Atrac3plus,352000,2048,2048,368,0x21,0x91,0x0028ff}
}};
inline const EncodingProfile& profile_by_id(std::string_view id) {
    for(const auto& profile:kOfficialProfiles)if(id==profile.id)return profile;
    throw std::invalid_argument("Unknown AAL profile. Use --list-profiles; unsupported rates are never substituted.");
}
inline const EncodingProfile& profile_by_parameters(uint8_t codec,uint32_t parameters) {
    for(const auto& profile:kOfficialProfiles)if(profile.oma_codec_id==codec&&profile.core_parameters==parameters)return profile;
    throw std::invalid_argument("Unsupported AAL codec or core configuration.");
}
inline uint32_t profile_block_count(uint32_t samples,const EncodingProfile& profile) {
    return uint32_t((uint64_t(samples)+profile.pcm_trim+profile.frame_samples-1)/profile.frame_samples+1);
}
inline uint16_t profile_tail(uint32_t samples,const EncodingProfile& profile) {
    return uint16_t(samples%profile.frame_samples?samples%profile.frame_samples:profile.frame_samples);
}
}
