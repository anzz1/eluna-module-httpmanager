
-- URL to your players_online.php
local MY_SERVER_URL = "http://localhost:8000/players_online.php"

-- Set same secret as in players_online.php
local MY_SECRET = "5ecret"

--------

require("httpmanager")
require("_httpmanager")

local function UpdatePlayersOnline()
    local body = "s="..MY_SECRET.."&c="..GetPlayerCount()
    httpmanager.HttpRequest("POST", MY_SERVER_URL, body, "application/x-www-form-urlencoded", (function(status, body, headers) end))
end

UpdatePlayersOnline()

RegisterPlayerEvent(3, UpdatePlayersOnline) -- PLAYER_EVENT_ON_LOGIN
RegisterPlayerEvent(4, UpdatePlayersOnline) -- PLAYER_EVENT_ON_LOGOUT
