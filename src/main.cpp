#include "main.hpp"

// PlayerId, Player
std::unordered_map<int, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int, std::vector<Player>> levelList;

int lastPlayerId = 0;

std::string parseIpAddress(int address) {
    unsigned char bytes[4];
    bytes[0] = address & 0xFF;
    bytes[1] = (address >> 8) & 0xFF;
    bytes[2] = (address >> 16) & 0xFF;
    bytes[3] = (address >> 24) & 0xFF;
    return fmt::format("{}.{}.{}.{}", bytes[3], bytes[2], bytes[1], bytes[0]);
}

int main() {
    int port = 23973; // Make this a cmd option

    fmt::print("Starting server...\n");

    if (enet_initialize() != 0) {
        fmt::print(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    ENetAddress address;
    ENetHost *server;

    address.host = ENET_HOST_ANY;
    address.port = port;

    server = enet_host_create(&address,
                              2048,
                              1,
                              0,
                              0);

    if (server == nullptr) {
        fmt::print(stderr, "An error occurred while trying to create an ENet server host.\n");
        exit(EXIT_FAILURE);
    }

    fmt::print("Server listening on port {}\n", port);

    // Event Loop

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {

            switch (event.type) {
                case (ENET_EVENT_TYPE_CONNECT): {
                    fmt::print("Client connected from {}:{}\n", parseIpAddress(event.peer->address.host),
                               event.peer->address.port);
                    int playerId = lastPlayerId++;
                    event.peer->data = new char[sizeof(int)];
                    memcpy(event.peer->data, &playerId, sizeof(int));

                    Player player{event.peer, playerId};
                    playerMap[playerId] = player;
                    break;
                }

                case (ENET_EVENT_TYPE_DISCONNECT): {
                    fmt::print("Client disconnected from {}:{}\n", parseIpAddress(event.peer->address.host),
                               event.peer->address.port);

                    Player senderPlayer = playerMap[*reinterpret_cast<int *>(event.peer->data)];

                    if (senderPlayer.levelId.has_value()) {

                        for (auto &player: levelList[senderPlayer.levelId.value()]) {
                            if (player.playerId == senderPlayer.playerId)
                                continue;

                            IncomingLeaveLevel incomingLeaveLevel;
                            incomingLeaveLevel.set_playerid(senderPlayer.playerId);

                            Packet packet;
                            packet.set_type(LEAVE_LEVEL);
                            packet.set_data(incomingLeaveLevel.SerializeAsString());

                            PacketUtility::sendPacket(packet, player.peer);
                        }
                    }

                    playerMap.erase(senderPlayer.playerId);
                    break;
                }

                case (ENET_EVENT_TYPE_RECEIVE): {
                    Player senderPlayer = playerMap[*reinterpret_cast<int *>(event.peer->data)];
                    Packet packet;
                    packet.ParseFromArray(event.packet->data, event.packet->dataLength);

                    fmt::print("Player {} -> Server\tPacket Length: {}\tPacket Type: {}\n",
                               senderPlayer.playerId, event.packet->dataLength,
                               packet.type());
                    for (int x = 0; x < event.packet->dataLength; x++) {
                        fmt::print(" {:#04x}", event.packet->data[x]);
                    }
                    fmt::print("\n\n");

                    switch (packet.type()) {
                        case (ICON_DATA): {
                            IconData iconData;
                            iconData.ParseFromString(packet.data());

                            senderPlayer.iconData = iconData;

                            if (!senderPlayer.levelId.has_value()) break;
                            IncomingIconData incomingIconData;
                            incomingIconData.set_playerid(senderPlayer.playerId);
                            *incomingIconData.mutable_icondata() = iconData;

                            Packet incomingIconDataPacket;
                            incomingIconDataPacket.set_type(ICON_DATA);
                            incomingIconDataPacket.set_data(incomingIconData.SerializeAsString());


                            for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(incomingIconDataPacket, levelPlayer.peer);
                            }

                            break;
                        }

                        case (COLOR_DATA): {
                            ColorData colorData;
                            colorData.ParseFromString(packet.data());

                            senderPlayer.colorData = colorData;

                            if (senderPlayer.levelId.has_value()) {
                                IncomingColorData incomingColorData;
                                incomingColorData.set_playerid(senderPlayer.playerId);
                                *incomingColorData.mutable_colordata() = colorData;

                                Packet incomingColorDataPacket;
                                incomingColorDataPacket.set_type(COLOR_DATA);
                                incomingColorDataPacket.set_data(incomingColorData.SerializeAsString());

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;

                                    PacketUtility::sendPacket(incomingColorDataPacket, levelPlayer.peer);
                                }
                            }

                            break;
                        }

                        case (JOIN_LEVEL): {
                            JoinLevel joinLevel;
                            joinLevel.ParseFromString(packet.data());

                            int levelId = joinLevel.levelid();

                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);
                            playerMap[senderPlayer.playerId].levelId = levelId;

                            levelList[levelId].push_back(senderPlayer);

                            IncomingJoinLevel incomingJoinLevel;
                            incomingJoinLevel.set_playerid(senderPlayer.playerId);

                            IncomingIconData incomingIconData;
                            incomingIconData.set_playerid(senderPlayer.playerId);
                            *incomingIconData.mutable_icondata() = senderPlayer.iconData;

                            IncomingColorData incomingColorData;
                            incomingColorData.set_playerid(senderPlayer.playerId);
                            *incomingColorData.mutable_colordata() = senderPlayer.colorData;

                            Packet incomingIconDataPacket;
                            incomingIconDataPacket.set_type(ICON_DATA);
                            incomingIconDataPacket.set_data(incomingIconData.SerializeAsString());

                            Packet incomingColorDataPacket;
                            incomingColorDataPacket.set_type(COLOR_DATA);
                            incomingColorDataPacket.set_data(incomingColorData.SerializeAsString());

                            Packet incomingJoinLevelPacket;
                            incomingJoinLevelPacket.set_type(JOIN_LEVEL);
                            incomingJoinLevelPacket.set_data(incomingJoinLevel.SerializeAsString());


                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                if (levelPlayer.peer) {
                                    IncomingJoinLevel incomingLevelPlayerJoinLevel;
                                    incomingLevelPlayerJoinLevel.set_playerid(levelPlayer.playerId);

                                    IncomingColorData incomingLevelPlayerColorData;
                                    incomingLevelPlayerColorData.set_playerid(levelPlayer.playerId);
                                    *incomingLevelPlayerColorData.mutable_colordata() = levelPlayer.colorData;

                                    IncomingIconData incomingLevelPlayerIconData;
                                    incomingLevelPlayerIconData.set_playerid(levelPlayer.playerId);
                                    *incomingLevelPlayerIconData.mutable_icondata() = levelPlayer.iconData;

                                    IncomingRenderData incomingPlayerRenderData;
                                    incomingPlayerRenderData.set_playerid(levelPlayer.playerId);
                                    *incomingPlayerRenderData.mutable_renderdata() = levelPlayer.renderData;

                                    Packet incomingLevelPlayerJoinPacket;
                                    incomingLevelPlayerJoinPacket.set_type(JOIN_LEVEL);
                                    incomingLevelPlayerJoinPacket.set_data(
                                            incomingLevelPlayerJoinLevel.SerializeAsString());

                                    Packet incomingLevelPlayerColorDataPacket;
                                    incomingLevelPlayerColorDataPacket.set_type(COLOR_DATA);
                                    incomingLevelPlayerColorDataPacket.set_data(
                                            incomingLevelPlayerColorData.SerializeAsString());

                                    Packet incomingLevelPlayerIconDataPacket;
                                    incomingLevelPlayerIconDataPacket.set_type(ICON_DATA);
                                    incomingLevelPlayerIconDataPacket.set_data(
                                            incomingLevelPlayerIconData.SerializeAsString());

                                    Packet incomingLevelPlayerRenderDataPacket;
                                    incomingLevelPlayerRenderDataPacket.set_type(RENDER_DATA);
                                    incomingLevelPlayerRenderDataPacket.set_data(
                                            incomingPlayerRenderData.SerializeAsString());

                                    PacketUtility::sendPacket(incomingJoinLevelPacket, levelPlayer.peer);
                                    PacketUtility::sendPacket(incomingLevelPlayerJoinPacket, senderPlayer.peer);

                                    PacketUtility::sendPacket(incomingIconDataPacket, levelPlayer.peer);
                                    PacketUtility::sendPacket(incomingColorDataPacket, levelPlayer.peer);

                                    PacketUtility::sendPacket(incomingLevelPlayerIconDataPacket, senderPlayer.peer);
                                    PacketUtility::sendPacket(incomingLevelPlayerColorDataPacket, senderPlayer.peer);

                                    PacketUtility::sendPacket(incomingLevelPlayerRenderDataPacket, levelPlayer.peer);
                                }
                            }

                            break;
                        }

                        case (LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value())
                                break;

                            int levelId = *senderPlayer.levelId;

                            fmt::print("Player {} left level {}\n", senderPlayer.playerId, levelId);

                            IncomingLeaveLevel incomingLeaveLevel;
                            incomingLeaveLevel.set_playerid(senderPlayer.playerId);

                            Packet incomingLeaveLevelPacket;
                            incomingLeaveLevelPacket.set_type(LEAVE_LEVEL);
                            incomingLeaveLevelPacket.set_data(incomingLeaveLevel.SerializeAsString());

                            for (auto &player: levelList[levelId]) {
                                if (player.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(incomingLeaveLevelPacket, player.peer);
                            }

                            levelList[levelId].erase(
                                    std::remove_if(levelList[levelId].begin(), levelList[levelId].end(),
                                                   [senderPlayer](Player &player) {
                                                       return player.playerId == senderPlayer.playerId;
                                                   }),
                                    levelList[levelId].end());

                            senderPlayer.levelId = std::nullopt;
                            break;
                        }

                            /////////////////////////////////////////////////////
                        case (RENDER_DATA): {
                            if (!senderPlayer.levelId.has_value()) {
                                fmt::print("cringe\n");
                                break;
                            }

                            int levelId = *senderPlayer.levelId;

                            RenderData renderData;
                            renderData.ParseFromString(packet.data());

                            senderPlayer.renderData = renderData;

                            IncomingRenderData incomingRenderData;
                            incomingRenderData.set_playerid(senderPlayer.playerId);
                            *incomingRenderData.mutable_renderdata() = renderData;

                            Packet incomingRenderDataPacket;
                            incomingRenderDataPacket.set_type(RENDER_DATA);
                            incomingRenderDataPacket.set_data(incomingRenderData.SerializeAsString());

                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(incomingRenderDataPacket, levelPlayer.peer);
                            }
                            break;
                        }
                    }

                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    break;
            }

            enet_packet_destroy(event.packet);
        }
    }

    enet_host_destroy(server);
    return 0;
}