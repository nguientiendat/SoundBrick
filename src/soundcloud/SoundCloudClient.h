#pragma once
#include <string>
#include <vector>
#include "Track.h"
#include "../network/HttpClient.h"

namespace soundcloud {

class SoundCloudClient {
public:
    SoundCloudClient();
    ~SoundCloudClient();

    // Load configuration (e.g. client_id or oauth_token) from a JSON file.
    bool loadConfig(const std::string& configPath);
    bool saveConfig(const std::string& configPath);
    
    std::string getClientId() const { return clientId; }
    void setClientId(const std::string& id) { clientId = id; }

    // Tự động cào dữ liệu từ soundcloud.com để lấy client_id mới nhất
    std::string autoFetchClientId();

    // Search for tracks
    std::vector<Track> searchTracks(const std::string& query, int limit = 20);

    // Get stream URL for a given track urn or ID
    std::string resolveStreamUrl(const std::string& trackId);

private:
    HttpClient httpClient;
    std::string clientId;
    std::string oauthToken;
};

} // namespace soundcloud
