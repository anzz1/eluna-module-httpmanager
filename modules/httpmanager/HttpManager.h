#ifndef ELUNA_HTTP_MANAGER_H
#define ELUNA_HTTP_MANAGER_H

#include <regex>

#include "httplib.h"
#include "SPSCQueue.h"

struct HttpWorkItem
{
public:
    HttpWorkItem(int funcRef, const std::string& httpVerb, const std::string& url, const std::string& body, const std::string &contentType, const httplib::Headers& headers);

    int funcRef;
    std::string httpVerb;
    std::string url;
    std::string body;
    std::string contentType;
    httplib::Headers headers;
};

struct HttpResponse
{
public:
    HttpResponse(int funcRef, int statusCode, const std::string& body, const httplib::Headers& headers);

    int funcRef;
    int statusCode;
    std::string body;
    httplib::Headers headers;
};


class HttpManager
{
public:
    HttpManager();
    ~HttpManager();

    void StartHttpWorker();
    void StopHttpWorker();
    void PushRequest(HttpWorkItem* item);
    void HandleHttpResponses(lua_State* L);
    void ExecuteCall(lua_State* L, int params, int res);
    void PrintError(lua_State* L, std::string error);

private:
    void ClearQueues();
    void HttpWorkerThread();
    bool ParseUrl(const std::string& url, std::string& host, std::string& path);
    httplib::Result DoRequest(httplib::Client& client, HttpWorkItem* req, const std::string& path);

    rigtorp::SPSCQueue<HttpWorkItem*> workQueue;
    rigtorp::SPSCQueue<HttpResponse*> responseQueue;
    std::thread workerThread;
    bool startedWorkerThread;
    std::atomic_bool cancelationToken;
    std::condition_variable condVar;
    std::mutex condVarMutex;
    std::regex parseUrlRegex;
};

#endif // #ifndef ELUNA_HTTP_MANAGER_H
