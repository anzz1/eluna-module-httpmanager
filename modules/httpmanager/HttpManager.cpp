#include <thread>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
};

#if defined(WIN32)
#define MODULEAPI_CPP extern "C" __declspec(dllexport) int
#else
#define MODULEAPI_CPP extern "C" int
#endif

// Define this if you want OpenSSL (https://) support
//#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "httplib.h"
#include "HttpManager.h"

HttpWorkItem::HttpWorkItem(int funcRef, const std::string& httpVerb, const std::string& url, const std::string& body, const std::string& contentType, const httplib::Headers& headers)
    : funcRef(funcRef),
    httpVerb(httpVerb),
    url(url),
    body(body),
    contentType(contentType),
    headers(headers)
{ }

HttpResponse::HttpResponse(int funcRef, int statusCode, const std::string& body, const httplib::Headers& headers)
    : funcRef(funcRef),
    statusCode(statusCode),
    body(body),
    headers(headers)
{ }

HttpManager::HttpManager()
    : workQueue(16),
    responseQueue(16),
    startedWorkerThread(false),
    cancelationToken(false),
    condVar(),
    condVarMutex(),
    parseUrlRegex("^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?")
{
    StartHttpWorker();
}

HttpManager::~HttpManager()
{
    StopHttpWorker();
}

void HttpManager::PushRequest(HttpWorkItem* item)
{
    std::unique_lock<std::mutex> lock(condVarMutex);
    workQueue.push(item);
    condVar.notify_one();
}

void HttpManager::StartHttpWorker()
{
    ClearQueues();

    if (!startedWorkerThread)
    {
        cancelationToken.store(false);
        workerThread = std::thread(&HttpManager::HttpWorkerThread, this);
        startedWorkerThread = true;
    }
}

void HttpManager::ClearQueues()
{
    while (workQueue.front())
    {
        HttpWorkItem* item = *workQueue.front();
        if (item != nullptr)
        {
            delete item;
        }
        workQueue.pop();
    }

    while (responseQueue.front())
    {
        HttpResponse* item = *responseQueue.front();
        if (item != nullptr)
        {
            delete item;
        }
        responseQueue.pop();
    }
}

void HttpManager::StopHttpWorker()
{
    if (!startedWorkerThread)
    {
        return;
    }

    cancelationToken.store(true);
    condVar.notify_one();
    workerThread.join();
    ClearQueues();
    startedWorkerThread = false;
}

void HttpManager::HttpWorkerThread()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(condVarMutex);
            condVar.wait(lock, [&] { return workQueue.front() != nullptr || cancelationToken.load(); });
        }

        if (cancelationToken.load())
        {
            break;
        }
        if (!workQueue.front())
        {
            continue;
        }

        HttpWorkItem* req = *workQueue.front();
        workQueue.pop();
        if (!req)
        {
            continue;
        }

        try
        {
            std::string host;
            std::string path;

            if (!ParseUrl(req->url, host, path)) {
                //ELUNA_LOG_ERROR("[Eluna]: Could not parse URL %s", req->url.c_str());
                printf("[Eluna][httpmanager]: Could not parse URL %s\n", req->url.c_str());
                continue;
            }

            httplib::Client cli(host);
            cli.set_connection_timeout(0, 3000000); // 3 seconds
            cli.set_read_timeout(5, 0); // 5 seconds
            cli.set_write_timeout(5, 0); // 5 seconds

            httplib::Result res = DoRequest(cli, req, path);
            httplib::Error err = res.error();
            if (err != httplib::Error::Success)
            {
                //ELUNA_LOG_ERROR("[Eluna]: HTTP request error: %s", httplib::to_string(err).c_str());
                printf("[Eluna][httpmanager]: HTTP request error: %s\n", httplib::to_string(err).c_str());
                continue;
            }

            if (res->status == 301)
            {
                std::string location = res->get_header_value("Location");
                std::string host;
                std::string path;

                if (!ParseUrl(location, host, path))
                {
                    //ELUNA_LOG_ERROR("[Eluna]: Could not parse URL after redirect: %s", location.c_str());
                    printf("[Eluna][httpmanager]: Could not parse URL after redirect: %s\n", location.c_str());
                    continue;
                }
                httplib::Client cli2(host);
                cli2.set_connection_timeout(0, 3000000); // 3 seconds
                cli2.set_read_timeout(5, 0); // 5 seconds
                cli2.set_write_timeout(5, 0); // 5 seconds
                res = DoRequest(cli2, req, path);
            }

            responseQueue.push(new HttpResponse(req->funcRef, res->status, res->body, res->headers));
        }
        catch (const std::exception& ex)
        {
            //ELUNA_LOG_ERROR("[Eluna]: HTTP request error: %s", ex.what());
            printf("[Eluna][httpmanager]: HTTP request error: %s\n", ex.what());
        }

        delete req;
    }
}

httplib::Result HttpManager::DoRequest(httplib::Client& client, HttpWorkItem* req, const std::string& urlPath)
{
    const char* path = urlPath.c_str();
    if (req->httpVerb == "GET")
    {
        return client.Get(path, req->headers);
    }
    if (req->httpVerb == "HEAD")
    {
        return client.Head(path, req->headers);
    }
    if (req->httpVerb == "POST")
    {
        return client.Post(path, req->headers, req->body, req->contentType.c_str());
    }
    if (req->httpVerb == "PUT")
    {
        return client.Put(path, req->headers, req->body, req->contentType.c_str());
    }
    if (req->httpVerb == "PATCH")
    {
        return client.Patch(path, req->headers, req->body, req->contentType.c_str());
    }
    if (req->httpVerb == "DELETE")
    {
        return client.Delete(path, req->headers);
    }
    if (req->httpVerb == "OPTIONS")
    {
        return client.Options(path, req->headers);
    }

    //ELUNA_LOG_ERROR("[Eluna]: HTTP request error: invalid HTTP verb %s", req->httpVerb.c_str());
    printf("[Eluna][httpmanager]: HTTP request error: invalid HTTP verb %s\n", req->httpVerb.c_str());
    return client.Get(path, req->headers);
}

bool HttpManager::ParseUrl(const std::string& url, std::string& host, std::string& path)
{
    std::smatch matches;

    if (!std::regex_search(url, matches, parseUrlRegex))
    {
        return false;
    }

    std::string scheme = matches[2];
    std::string authority = matches[4];
    std::string query = matches[7];
    host = scheme + "://" + authority;
    path = matches[5];
    if (path.empty())
    {
        path = "/";
    }
    path += (query.empty() ? "" : "?") + query;

    return true;
}

void HttpManager::HandleHttpResponses(lua_State* L)
{
    while (!responseQueue.empty())
    {
        HttpResponse* res = *responseQueue.front();
        responseQueue.pop();

        if (res == nullptr)
        {
            continue;
        }

        //LOCK_ELUNA;

        // Get function
        lua_rawgeti(L, LUA_REGISTRYINDEX, res->funcRef);

        // Push parameters
        lua_pushinteger(L, res->statusCode);
        lua_pushstring(L, res->body.c_str());
        lua_newtable(L);
        for (const auto& item : res->headers) {
            lua_pushstring(L, item.first.c_str());
            lua_pushstring(L, item.second.c_str());
            lua_settable(L, -3);
        }

        // Call function
        ExecuteCall(L, 3, 0);

        luaL_unref(L, LUA_REGISTRYINDEX, res->funcRef);

        delete res;
    }
}

void HttpManager::PrintError(lua_State* L, std::string error)
{
    std::ostringstream err;
    err << "printError('[Eluna][httpmanager]: " << error << "')";
    luaL_dostring(L, err.str().c_str());
}

void HttpManager::ExecuteCall(lua_State* L, int params, int res)
{
    int top = lua_gettop(L);
    int base = top - params;

    // Expected: function, [parameters]
    assert(base > 0);

    // Check function type
    if (!lua_isfunction(L, base))
    {
        //ELUNA_LOG_ERROR("[Eluna]: Cannot execute call: registered value is %s, not a function.", luaL_tolstring(L, base, NULL));
        std::ostringstream err;
        err << "Cannot execute call: registered value is " << luaL_tolstring(L, base, NULL) << ", not a function.";
        PrintError(L, err.str());
        
        assert(false); // stack probably corrupt
    }

    int result = lua_pcall(L, params, res, 0);

    // lua_pcall returns 0 on success.
    // On error print the error and push nils for expected amount of returned values
    if (result)
    {
        // Stack: errmsg
        const char* msg = lua_tostring(L, -1);
        //ELUNA_LOG_ERROR("%s", msg);
        PrintError(L, msg);
        lua_pop(L, 1);

        // Force garbage collect
        lua_gc(L, LUA_GCCOLLECT, 0);

        // Push nils for expected amount of results
        for (int i = 0; i < res; ++i)
            lua_pushnil(L);
        // Stack: [nils]
    }
    // Stack: [results]
}
