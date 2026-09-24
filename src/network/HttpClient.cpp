#include "HttpClient.h"
#include <curl/curl.h>
#include <iostream>

HttpClient::HttpClient() : initialized(false) {
    if (curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK) {
        initialized = true;
    } else {
        std::cerr << "Failed to initialize libcurl globally" << std::endl;
    }
}

HttpClient::~HttpClient() {
    if (initialized) {
        curl_global_cleanup();
    }
}

size_t HttpClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realsize);
    return realsize;
}

HttpResponse HttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    response.statusCode = 0;

    if (!initialized) {
        response.error = "libcurl not initialized";
        return response;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize curl handle";
        return response;
    }

    struct curl_slist* chunk = nullptr;
    for (const auto& pair : headers) {
        std::string headerString = pair.first + ": " + pair.second;
        chunk = curl_slist_append(chunk, headerString.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // SSL Verification: In embedded systems, ca-certificates might be missing or outdated.
    // For MVP we might disable verification if needed, but let's try with it first.
    // Uncomment the following if HTTPS fails due to cert issues.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Timeouts
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.statusCode = static_cast<int>(http_code);
    }

    if (chunk) {
        curl_slist_free_all(chunk);
    }
    curl_easy_cleanup(curl);

    return response;
}
