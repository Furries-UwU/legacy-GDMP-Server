#include "main.hpp"

// HSteamNetConnection, Player
std::unordered_map<HSteamNetConnection, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int, std::vector<Player>> levelList;

ISteamNetworkingSockets *interface;
HSteamNetPollGroup pollGroup;
HSteamListenSocket socket;

int lastPlayerId = 0;

/////////////////////////

void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *statusInfo)
{
    switch (statusInfo->m_info.m_eState)
    {
    case k_ESteamNetworkingConnectionState_None:
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
    {
        if (statusInfo->m_eOldState != k_ESteamNetworkingConnectionState_Connected)
            break;

        auto player = playerMap.find(statusInfo->m_hConn);
        if (player == playerMap.end())
            break;

        if (player->second.levelId.has_value())
        {
            for (auto levelPlayer : levelList[player->second.levelId.value()])
            {
                Packet(LEAVE_LEVEL, sizeof(int), reinterpret_cast<uint8_t *>(player->second.playerId)).send(interface, levelPlayer.connection);
            };
        };

        playerMap.erase(player);

        interface->CloseConnection(statusInfo->m_hConn, 0, nullptr, false);
        break;
    }

    case k_ESteamNetworkingConnectionState_Connecting:
    {
        if (playerMap.find(statusInfo->m_hConn) != playerMap.end())
        {
            break;
        }

        if (interface->AcceptConnection(statusInfo->m_hConn) != k_EResultOK)
        {
            interface->CloseConnection(statusInfo->m_hConn, 0, nullptr, false);
            fmt::print("Can't accept connection. (It was already closed?)");
            break;
        }

        if (!interface->SetConnectionPollGroup(statusInfo->m_hConn, pollGroup))
        {
            interface->CloseConnection(statusInfo->m_hConn, 0, nullptr, false);
            fmt::print("Failed to set poll group?");
            break;
        }

        playerMap[statusInfo->m_hConn] = {
            statusInfo->m_hConn,
            lastPlayerId++};

        break;
    }

    case k_ESteamNetworkingConnectionState_Connected:
        break;

    default:
        // Silences -Wswitch
        break;
    }
}

/////////////////////////

void sendIconData(HSteamNetConnection connection, IncomingIconData incomingIconData)
{
    Packet(ICON_DATA, sizeof(incomingIconData), reinterpret_cast<uint8_t *>(&incomingIconData)).send(interface, connection);
}

void sendColorData(HSteamNetConnection connection, IncomingColorData incomingColorData)
{
    Packet(COLOR_DATA, sizeof(incomingColorData), reinterpret_cast<uint8_t *>(&incomingColorData)).send(interface, connection);
}

void sendUsername(HSteamNetConnection connection, IncomingUsername incomingUsername, int nameLength)
{
    Packet(USERNAME, sizeof(int) + nameLength, reinterpret_cast<uint8_t *>(&incomingUsername)).send(interface, connection);
}

int main()
{
    int port = 23973; // Make this a cmd option

    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
    {
        fmt::print("GameNetworkingSockets_Init failed. {}", errMsg);
        return 1;
    }

    fmt::print("Starting server...\n");

    interface = SteamNetworkingSockets();

    SteamNetworkingIPAddr serverLocalAddr;
    serverLocalAddr.Clear();
    serverLocalAddr.m_port = port;

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)OnSteamNetConnectionStatusChanged);

    socket = interface->CreateListenSocketIP(serverLocalAddr, 1, &opt);
    if (socket == k_HSteamListenSocket_Invalid)
    {
        fmt::print("Failed to listen on port {}\n", port);
        return 1;
    }

    pollGroup = interface->CreatePollGroup();
    if (pollGroup == k_HSteamNetPollGroup_Invalid)
    {
        fmt::print("Failed to listen on port {}\n", port);
        return 1;
    }

    fmt::print("Server listening on port {}\n", port);

    // Event Loop

    while (true)
    {
        ISteamNetworkingMessage *incomingMessage = nullptr;
        int numMsgs = interface->ReceiveMessagesOnPollGroup(pollGroup, &incomingMessage, 1);

        if (numMsgs == 0)
            continue;

        if (numMsgs < 0)
        {
            fmt::print("Error checking for messages");
            return 1;
        }

        if (incomingMessage->m_cbSize < 5)
        {
            fmt::print("Recieved invalid packet");
            continue;
        }

        Player player = playerMap[incomingMessage->m_conn];
        Packet *packet = reinterpret_cast<Packet *>(&incomingMessage->m_pData);

        fmt::print("PlayerId {} -> Host\nPacket Length: {}\nPacket Type: {}\nPacket's Data Length: {}\nHex:", player.playerId, incomingMessage->m_cbSize, packet->type, packet->length);
        for (int x = 0; x < incomingMessage->m_cbSize; x++)
        {
            fmt::print(" {:#04x}", packet[x]);
        }
        fmt::print("\n\n");

        switch (packet->type)
        {
        case (RENDER_DATA):
        {
            if (!player.levelId.has_value())
                break;

            IncomingRenderData incomingRenderData;
            incomingRenderData.playerId = player.playerId;
            incomingRenderData.renderData = *reinterpret_cast<RenderData *>(packet->data);

            for (auto levelPlayer : levelList[player.levelId.value()])
            {
                if (levelPlayer.playerId == player.playerId)
                    continue;
                Packet(RENDER_DATA, sizeof(incomingRenderData), reinterpret_cast<uint8_t *>(&incomingRenderData)).send(interface, levelPlayer.connection);
            }

            break;
        }

        case (USERNAME):
        {
            player.username = std::string(reinterpret_cast<char *>(packet->data));

            if (player.levelId.has_value())
            {
                for (auto levelPlayer : levelList[player.levelId.value()])
                {
                    if (levelPlayer.playerId == player.playerId)
                        continue;

                    IncomingUsername incomingUsername;
                    incomingUsername.playerId = player.playerId;
                    incomingUsername.username = (char *)player.username.c_str();

                    sendUsername(levelPlayer.connection, incomingUsername, player.username.length() + 1);
                }
            }

            break;
        }

        case (ICON_DATA):
        {
            player.iconData = *reinterpret_cast<IconData *>(packet->data);

            IncomingIconData incomingIconData;
            incomingIconData.playerId = player.playerId;
            incomingIconData.iconData = player.iconData;

            if (player.levelId.has_value())
            {
                for (auto levelPlayer : levelList[player.levelId.value()])
                {
                    if (levelPlayer.playerId == player.playerId)
                        continue;
                    sendIconData(levelPlayer.connection, incomingIconData);
                }
            }

            break;
        }

        case (COLOR_DATA):
        {
            player.colorData = *reinterpret_cast<ColorData *>(packet->data);

            IncomingColorData incomingColorData;
            incomingColorData.playerId = player.playerId;
            incomingColorData.colorData = player.colorData;

            if (player.levelId.has_value())
            {
                for (auto levelPlayer : levelList[player.levelId.value()])
                {
                    if (levelPlayer.playerId == player.playerId)
                        continue;
                    sendColorData(levelPlayer.connection, incomingColorData);
                }
            }

            break;
        }
        ////////////////////////////////
        case (JOIN_LEVEL):
        {
            if (player.levelId.has_value())
                break;
            player.levelId.value() = *reinterpret_cast<int *>(packet->data);

            IncomingIconData playerIncomingIconData;
            playerIncomingIconData.playerId = player.playerId;
            playerIncomingIconData.iconData = player.iconData;

            IncomingColorData playerIncomingColorData;
            playerIncomingColorData.playerId = player.playerId;
            playerIncomingColorData.colorData = player.colorData;

            IncomingUsername playerIncomingUsername;
            playerIncomingUsername.playerId = player.playerId;
            playerIncomingUsername.username = (char *)player.username.c_str();

            for (auto levelPlayer : levelList[player.levelId.value()])
            {
                IncomingIconData incomingIconData;
                incomingIconData.playerId = levelPlayer.playerId;
                incomingIconData.iconData = levelPlayer.iconData;

                IncomingColorData incomingColorData;
                incomingColorData.playerId = levelPlayer.playerId;
                incomingColorData.colorData = levelPlayer.colorData;

                IncomingUsername incomingUsername;
                incomingUsername.playerId = levelPlayer.playerId;
                incomingUsername.username = (char *)levelPlayer.username.c_str();

                // Send player data to the person who join level
                sendIconData(incomingMessage->m_conn, incomingIconData);
                sendColorData(incomingMessage->m_conn, incomingColorData);
                sendUsername(incomingMessage->m_conn, incomingUsername, levelPlayer.username.length() + 1);

                // Send the player data to the persno who's in the level
                sendIconData(levelPlayer.connection, playerIncomingIconData);
                sendColorData(levelPlayer.connection, playerIncomingColorData);
                sendUsername(levelPlayer.connection, playerIncomingUsername, player.username.length() + 1);

                // Finally, Send JOIN_LEVEL packet
                Packet(JOIN_LEVEL, sizeof(int), reinterpret_cast<uint8_t *>(player.playerId)).send(interface, levelPlayer.connection);
            }

            levelList[player.levelId.value()].push_back(player);
            break;
        }

        case (LEAVE_LEVEL):
        {
            if (!player.levelId.has_value())
                break;

            for (auto levelPlayer : levelList[player.levelId.value()])
            {
                Packet(LEAVE_LEVEL, sizeof(int), reinterpret_cast<uint8_t *>(player.playerId)).send(interface, levelPlayer.connection);
            }

            // Jesus, CoPilot, Calm down bro-
            levelList[player.levelId.value()].erase(std::remove_if(levelList[player.levelId.value()].begin(), levelList[player.levelId.value()].end(), [player](Player levelPlayer)
                                                                   { return levelPlayer.playerId == player.playerId; }),
                                                    levelList[player.levelId.value()].end());

            player.levelId.reset();
            break;
        }
        };

        incomingMessage->Release();
    }

    // Clean up

    interface->CloseListenSocket(socket);
    socket = k_HSteamListenSocket_Invalid;

    interface->DestroyPollGroup(pollGroup);
    pollGroup = k_HSteamNetPollGroup_Invalid;

    return 0;
}