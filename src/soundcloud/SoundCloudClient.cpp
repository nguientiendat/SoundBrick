#include "SoundCloudClient.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace soundcloud {

SoundCloudClient::SoundCloudClient() {
}

SoundCloudClient::~SoundCloudClient() {
}

bool SoundCloudClient::loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << configPath << std::endl;
        return false;
    }

    try {
        json config = json::parse(file);
        if (config.contains("client_id")) {
            clientId = config["client_id"].get<std::string>();
        }
        if (config.contains("oauth_token")) {
            oauthToken = config["oauth_token"].get<std::string>();
        }
        
        std::cout << "Loaded config. Client ID present: " << !clientId.empty() 
                  << ", OAuth present: " << !oauthToken.empty() << std::endl;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error in config: " << e.what() << std::endl;
        return false;
    }
}

bool SoundCloudClient::saveConfig(const std::string& configPath) {
    json j;
    j["client_id"] = clientId;
    j["oauth_token"] = oauthToken;
    std::ofstream file(configPath);
    if (!file.is_open()) return false;
    file << j.dump(4);
    return true;
}

std::string SoundCloudClient::autoFetchClientId() {
    HttpResponse res = httpClient.get("https://soundcloud.com");
    if (res.statusCode != 200) return "";
    
    std::string prefix = "<script crossorigin src=\"";
    std::string assetPrefix = "https://a-v2.sndcdn.com/assets/";
    
    size_t pos = 0;
    while ((pos = res.body.find(prefix + assetPrefix, pos)) != std::string::npos) {
        size_t start = pos + prefix.length();
        size_t end = res.body.find("\"", start);
        if (end != std::string::npos) {
            std::string jsUrl = res.body.substr(start, end - start);
            
            HttpResponse jsRes = httpClient.get(jsUrl);
            if (jsRes.statusCode == 200) {
                size_t idPos = jsRes.body.find("client_id:\"");
                if (idPos != std::string::npos) {
                    std::string id = jsRes.body.substr(idPos + 11, 32);
                    if (id.length() == 32) return id;
                }
            }
        }
        pos += prefix.length();
    }
    return "";
}

std::vector<Track> SoundCloudClient::searchTracks(const std::string& query, int limit) {
    std::vector<Track> results;
    if (clientId.empty() && oauthToken.empty()) {
        std::cerr << "Error: No SoundCloud client_id or oauth_token configured." << std::endl;
        return results;
    }

    // SoundCloud v2 search API
    // Note: The API requires a valid client_id or an Authorization header
    std::string url = "https://api-v2.soundcloud.com/search/tracks?q=";
    
    // Simple URL encoding (should be improved for production)
    std::string encodedQuery = query;
    size_t pos = 0;
    while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
        encodedQuery.replace(pos, 1, "%20");
        pos += 3;
    }
    
    url += encodedQuery + "&limit=" + std::to_string(limit);
    if (!clientId.empty()) {
        url += "&client_id=" + clientId;
    }

    std::map<std::string, std::string> headers;
    if (!oauthToken.empty()) {
        headers["Authorization"] = "OAuth " + oauthToken;
    }

    HttpResponse res = httpClient.get(url, headers);
    
    if (res.statusCode != 200) {
        std::cerr << "Search failed with status " << res.statusCode << ": " << res.error << std::endl;
        return results;
    }

    try {
        json j = json::parse(res.body);
        if (j.contains("collection") && j["collection"].is_array()) {
            for (const auto& item : j["collection"]) {
                Track t;
                // Some results might be playlists or users, make sure it's a track
                if (item.contains("kind") && item["kind"] == "track") {
                    auto safeString = [](const json& j, const std::string& k, const std::string& def = "") {
                        return (j.contains(k) && j[k].is_string()) ? j[k].get<std::string>() : def;
                    };

                    t.urn = item.value("id", 0) != 0 ? std::to_string(item.value("id", 0)) : "";
                    t.title = safeString(item, "title", "Unknown Title");
                    
                    if (item.contains("user") && item["user"].is_object()) {
                        t.artist = safeString(item["user"], "username", "Unknown Artist");
                    }
                    
                    t.artworkUrl = safeString(item, "artwork_url");
                    t.permalink = safeString(item, "permalink_url");
                    
                    // Dùng contains để tránh type_error với duration/streamable
                    t.durationMs = (item.contains("duration") && item["duration"].is_number()) ? item["duration"].get<int>() : 0;
                    t.playable = (item.contains("streamable") && item["streamable"].is_boolean()) ? item["streamable"].get<bool>() : false;
                    
                    // Save transcodings url for stream resolution
                    if (item.contains("media") && item["media"].contains("transcodings") && item["media"]["transcodings"].is_array()) {
                        for (const auto& tc : item["media"]["transcodings"]) {
                            if (tc.contains("url") && tc.contains("format")) {
                                std::string protocol = safeString(tc["format"], "protocol");
                                // We prefer progressive over hls for simple players
                                if (protocol == "progressive") {
                                    t.permalink = safeString(tc, "url"); // We hijack permalink to store the transcoding URL for now
                                    break;
                                } else if (protocol == "hls" && t.permalink.empty()) {
                                    t.permalink = safeString(tc, "url");
                                }
                            }
                        }
                    }
                    
                    results.push_back(t);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse SoundCloud JSON response: " << e.what() << std::endl;
    }

    return results;
}

std::string SoundCloudClient::resolveStreamUrl(const std::string& transcodingUrl) {
    if (clientId.empty() || transcodingUrl.empty()) return "";

    std::string url = transcodingUrl + "?client_id=" + clientId;
    std::map<std::string, std::string> headers;
    if (!oauthToken.empty()) {
        headers["Authorization"] = "OAuth " + oauthToken;
    }

    HttpResponse res = httpClient.get(url, headers);
    if (res.statusCode != 200) return "";

    try {
        json j = json::parse(res.body);
        return (j.contains("url") && j["url"].is_string()) ? j["url"].get<std::string>() : "";
    } catch (...) {
        return "";
    }
}

} // namespace soundcloud
