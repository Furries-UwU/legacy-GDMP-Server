#include "main.hpp"

// PlayerId, Player
std::unordered_map<uint16_t, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int32_t, std::vector<Player>> levelList;

uint16_t lastPlayerId = 0;

std::string parseIpAddress(int address) {
    unsigned char bytes[4];
    bytes[0] = address & 0xFF;
    bytes[1] = (address >> 8) & 0xFF;
    bytes[2] = (address >> 16) & 0xFF;
    bytes[3] = (address >> 24) & 0xFF;
    return fmt::format("{}.{}.{}.{}", bytes[0], bytes[1], bytes[2], bytes[3]);
}

int main(int argc, char **argv) {
    int port = 23973;

#ifndef WIN32
    for(;;) {
        switch (getopt(argc, argv, "p:h")) {

            case 'p': {
                port = std::stoi(optarg);
                continue;
            }

            case '?':
            case 'h':
            default :
                fmt::print("Usage: {} [-p port]\n", argv[0]);
                return 0;

            case -1:
                break;
        }

        break;
    }
#endif

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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {

            switch (event.type) {
                case (ENET_EVENT_TYPE_CONNECT): {
                    fmt::print("Client connected from {}:{}\n", parseIpAddress(event.peer->address.host),
                               event.peer->address.port);
                    uint16_t playerId = lastPlayerId++;
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

                            Packet packet{LEAVE_LEVEL, sizeof(senderPlayer.playerId), reinterpret_cast<uint8_t *>(senderPlayer.playerId)};
                            packet.send(player.peer);
                        }
                    }

                    playerMap.erase(senderPlayer.playerId);
                    break;
                }

                case (ENET_EVENT_TYPE_RECEIVE): {
                    Player senderPlayer = playerMap[*reinterpret_cast<int *>(event.peer->data)];

                    Packet packet{event.packet};

                    fmt::print("Player {} -> Server\tPacket Length: {}\tPacket Type: {}\n",
                               senderPlayer.playerId,
                               event.packet->dataLength,
                               packet.type);
                    for (int x = 0; x < event.packet->dataLength; x++) {
                        fmt::print(" {:#04x}", event.packet->data[x]);
                    }
                    fmt::print("\n\n");

                    switch (packet.type) {
                        case (JOIN_LEVEL): {
                            int32_t levelId = *reinterpret_cast<int32_t *>(packet.data);

                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);
                            fmt::print("0\n");playerMap[senderPlayer.playerId].levelId = levelId;

                            fmt::print("1\n");levelList[levelId].push_back(senderPlayer);

                            //if (1 >= levelList[levelId].size()) break; // commented out for debugging

                            fmt::print("2\n");IncomingIconData senderIconData{senderPlayer.playerId, senderPlayer.iconData};
                            fmt::print("3\n");Packet senderIconDataPacket{ICON_DATA, sizeof(senderIconData), reinterpret_cast<uint8_t *>(&senderIconData)};

                            fmt::print("4\n");IncomingColorData senderColorData{senderPlayer.playerId, senderPlayer.colorData};
                            fmt::print("5\n");Packet senderColorDataPacket{COLOR_DATA, sizeof(senderColorData), reinterpret_cast<uint8_t *>(&senderColorData)};

                            fmt::print("6\n");Packet senderJoinLevelPacket{JOIN_LEVEL, sizeof(senderPlayer.playerId), reinterpret_cast<uint8_t *>(&senderPlayer.playerId)};

                            for (auto &levelPlayer: levelList[levelId]) {
                                /*if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;*/

                                if (levelPlayer.peer) {
                                    IncomingIconData levelPlayerIconData{levelPlayer.playerId, levelPlayer.iconData};
                                    Packet levelPlayerIconDataPacket{ICON_DATA, sizeof(levelPlayerIconData), reinterpret_cast<uint8_t *>(&levelPlayerIconData)};

                                    IncomingColorData levelPlayerColorData{levelPlayer.playerId, levelPlayer.colorData};
                                    Packet levelPlayerColorDataPacket{COLOR_DATA, sizeof(levelPlayerColorData), reinterpret_cast<uint8_t *>(&levelPlayerColorData)};

                                    IncomingRenderData levelPlayerRenderData{levelPlayer.playerId, levelPlayer.renderData};
                                    Packet levelPlayerRenderDataPacket{COLOR_DATA, sizeof(levelPlayerRenderData), reinterpret_cast<uint8_t *>(&levelPlayerRenderData)};

                                    Packet levelPlayerJoinLevelPacket{JOIN_LEVEL, sizeof(senderPlayer.playerId), reinterpret_cast<uint8_t *>(&senderPlayer.playerId)};

                                    // send packets to player that just joined
                                    levelPlayerJoinLevelPacket.send(senderPlayer.peer);
                                    levelPlayerIconDataPacket.send(senderPlayer.peer);
                                    levelPlayerColorDataPacket.send(senderPlayer.peer);
                                    levelPlayerRenderDataPacket.send(senderPlayer.peer);

                                    // send packets from the player that joined to peers
                                    senderJoinLevelPacket.send(levelPlayer.peer);
                                    senderIconDataPacket.send(levelPlayer.peer);
                                    senderColorDataPacket.send(levelPlayer.peer);
                                    // (do I send a RenderData packet too?)
                                }
                            }

                            break;
                        }

                        case (LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value())
                                break;

                            int32_t levelId = *senderPlayer.levelId;

                            fmt::print("Player {} left level {}\n", senderPlayer.playerId, levelId);

                            Packet senderLeaveLevelPacket{LEAVE_LEVEL, sizeof(senderPlayer.playerId), reinterpret_cast<uint8_t *>(&senderPlayer.playerId)};

                            for (auto &player: levelList[levelId]) {
                                /*if (player.playerId == senderPlayer.playerId)
                                    continue;*/

                                senderLeaveLevelPacket.send(player.peer);
                            }

                            levelList[levelId].erase(
                                    std::remove_if(levelList[levelId].begin(), levelList[levelId].end(),
                                                   [senderPlayer](Player &player) {
                                                       return player.playerId == senderPlayer.playerId;
                                                   }),
                                    levelList[levelId].end());

                            if (levelList[levelId].empty()) levelList.erase(levelId);

                            senderPlayer.levelId = std::nullopt;
                            break;
                        }

                        case (RENDER_DATA): {
                            if (!senderPlayer.levelId.has_value()) {
                                fmt::print("cringe\n");
                                break;
                            }

                            int32_t levelId = *senderPlayer.levelId;
                            senderPlayer.renderData = *reinterpret_cast<RenderData*>(packet.data);

                            IncomingRenderData senderRenderData{senderPlayer.playerId, senderPlayer.renderData};
                            Packet senderRenderDataPacket{RENDER_DATA, sizeof(senderRenderData), reinterpret_cast<uint8_t *>(&senderRenderData)};

                            for (auto &levelPlayer: levelList[levelId]) {
                                /*if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;*/

                                senderRenderDataPacket.send(levelPlayer.peer);
                            }
                            break;
                        }

                        case (COLOR_DATA): {
                            senderPlayer.colorData = *reinterpret_cast<ColorData*>(packet.data);

                            if (senderPlayer.levelId.has_value()) {
                                IncomingColorData senderColorData{senderPlayer.playerId, senderPlayer.colorData};
                                Packet senderColorDataPacket{COLOR_DATA, sizeof(senderColorData), reinterpret_cast<uint8_t *>(&senderColorData)};

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    /*if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;*/

                                    senderColorDataPacket.send(levelPlayer.peer);
                                }
                            }

                            break;
                        }

                        case (ICON_DATA): {
                            senderPlayer.iconData = *reinterpret_cast<IconData*>(packet.data);

                            if (senderPlayer.levelId.has_value()) {
                                IncomingIconData senderIconData{senderPlayer.playerId, senderPlayer.iconData};
                                Packet senderIconDataPacket{ICON_DATA, sizeof(senderIconData), reinterpret_cast<uint8_t *>(&senderIconData)};

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    /*if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;*/

                                    senderIconDataPacket.send(levelPlayer.peer);
                                }
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