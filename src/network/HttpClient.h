#pragma once
#include <string>
#include <map>
#include <functional>

struct HttpResponse {
    int statusCode;
    std::string body;
    std::string error;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Perform a synchronous GET request
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    
    // Download a file directly to disk
    bool downloadFile(const std::string& url, const std::string& filepath, const std::map<std::string, std::string>& headers = {});

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t WriteFileCallback(void* contents, size_t size, size_t nmemb, void* userp);
    bool initialized;
};
