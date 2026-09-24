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
    
    // Asynchronous versions would use a thread pool or run in a worker thread.
    // For Phase 2, we implement synchronous basic operations.
    // We will call this from a worker thread in the application.

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    bool initialized;
};
