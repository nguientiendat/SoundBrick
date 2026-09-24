#pragma once
#include <string>
#include <cstdint>

namespace soundcloud {

struct Track {
    std::string urn;           // e.g., "soundcloud:tracks:12345678" or just "12345678"
    std::string title;
    std::string artist;
    std::string artworkUrl;
    std::string permalink;
    uint64_t durationMs;
    bool playable;
};

} // namespace soundcloud
