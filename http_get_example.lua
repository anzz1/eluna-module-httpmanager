
require("httpmanager")
require("_httpmanager")

-- command .ip to retrieve and display server public ip

RegisterPlayerEvent(42, function(event, player, cmd)
    if cmd:lower() == "ip" then
        local guid
        if (player) then
            guid = player:GetGUID()
        else
            guid = -1
        end

        httpmanager.HttpRequest("GET", "http://icanhazip.com", function(status, body, headers)
            if (guid == -1) then
                print("Server IP is: "..string.gsub(body, "[\n\r]", ""))
            else
                local player = GetPlayerByGUID(guid)
                if (player) then
                    player:SendBroadcastMessage("Server IP is: "..string.gsub(body, "[\n\r]", ""))
                end
            end
        end)
        return false
    end
end)
