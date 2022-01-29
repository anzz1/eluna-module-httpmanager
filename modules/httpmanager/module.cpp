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

#include "HttpManager.h"

HttpManager* httpManager = NULL;

int HttpRequest(lua_State* L)
{
    if (httpManager) {
        std::string httpVerb = luaL_checkstring(L, 1);
        std::string url = luaL_checkstring(L, 2);
        std::string body;
        std::string bodyContentType;
        httplib::Headers headers;

        int headersIdx = 3;
        int callbackIdx = 3;

        if (!lua_istable(L, headersIdx) && lua_isstring(L, headersIdx) && lua_isstring(L, headersIdx + 1))
        {
            body = luaL_checkstring(L, 3);
            bodyContentType = luaL_checkstring(L, 4);
            headersIdx = 5;
            callbackIdx = 5;
        }

        if (lua_istable(L, headersIdx))
        {
            ++callbackIdx;

            lua_pushnil(L); // First key
            while (lua_next(L, headersIdx) != 0)
            {
                // Uses 'key' (at index -2) and 'value' (at index -1)
                if (lua_isstring(L, -2))
                {
                    std::string key(lua_tostring(L, -2));
                    std::string value(lua_tostring(L, -1));
                    headers.insert(std::pair<std::string, std::string>(key, value));
                }
                // Removes 'value'; keeps 'key' for next iteration
                lua_pop(L, 1);
            }
        }

        lua_pushvalue(L, callbackIdx);
        int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
        if (funcRef >= 0)
        {
            httpManager->PushRequest(new HttpWorkItem(funcRef, httpVerb, url, body, bodyContentType, headers));
        }
        else
        {
            luaL_argerror(L, callbackIdx, "HttpManager unable to make a ref to function");
        }
    }
    else
    {
        luaL_error(L, "HttpManager is not initialized");
    }

    return 0;
}

int HandleResponses(lua_State* L)
{
    if (httpManager) {
        httpManager->HandleHttpResponses(L);
    }
    return 0;
}

int _Unload(lua_State* L)
{
    delete httpManager;
    httpManager = NULL;
    return 0;
}

static const struct luaL_Reg functions [] = {
    {"HttpRequest", HttpRequest},
    {"HandleResponses", HandleResponses},
    {"_Unload", _Unload},
    {NULL, NULL}
};

MODULEAPI_CPP luaopen_httpmanager(lua_State *L) {
    httpManager = new HttpManager();
    luaL_register(L, "httpmanager", functions);
    return 1;
}
