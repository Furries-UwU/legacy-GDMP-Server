#include "main.hpp"

// ENetPeer, Player
std::unordered_map<ENetPeer *, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int, std::vector<Player>> levelList;

int lastPlayerId = 0;

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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {

            switch (event.type) {
                case (ENET_EVENT_TYPE_CONNECT): {
                    fmt::print("Client connected from {}:{}\n", event.peer->address.host, event.peer->address.port);
                    playerMap[event.peer] = Player{event.peer, lastPlayerId++};
                    break;
                }

                case (ENET_EVENT_TYPE_DISCONNECT): {
                    fmt::print("Client disconnected from {}:{}\n", event.peer->address.host, event.peer->address.port);

                    Player senderPlayer = playerMap[event.peer];

                    if (!senderPlayer.levelId.has_value()) break;

                    for (auto &player: levelList[senderPlayer.levelId.value()]) {
                        if (player.playerId == senderPlayer.playerId)
                            continue;
                        Packet(LEAVE_LEVEL, 4, reinterpret_cast<uint8_t *>(&senderPlayer.playerId)).send(player.peer);
                    }

                    playerMap.erase(event.peer);
                    break;
                }

                case (ENET_EVENT_TYPE_RECEIVE): {
                    Player senderPlayer = playerMap[event.peer];
                    auto packet = Packet(event.packet);

                    fmt::print("Player {} -> Server\tPacket Length: {}\tPacket Type: {}\tPacket's Data Length: {}\n", senderPlayer.playerId, event.packet->dataLength,
                               packet.type, packet.length);
                    for (int x = 0; x < event.packet->dataLength; x++) {
                        fmt::print(" {:#04x}", event.packet->data[x]);
                    }
                    fmt::print("\n\n");

                    if (event.packet->dataLength < sizeof(Packet)) {
                        fmt::print(stderr, "Received packet with invalid size.\n");
                        break;
                    }

                    switch (packet.type) {
                        case (ICON_DATA): {
                            IconData iconData = *reinterpret_cast<IconData *>(packet.data);
                            senderPlayer.iconData = iconData;

                            if (senderPlayer.levelId.has_value()) {
                                IncomingIconData incomingIconData = {senderPlayer.playerId, iconData};

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;
                                    Packet(ICON_DATA, sizeof(incomingIconData),
                                           reinterpret_cast<uint8_t *>(&incomingIconData)).send(levelPlayer.peer);
                                }
                            }
                        }

                        case (COLOR_DATA): {
                            ColorData colorData = *reinterpret_cast<ColorData *>(packet.data);
                            senderPlayer.colorData = colorData;

                            if (senderPlayer.levelId.has_value()) {
                                IncomingColorData incomingColorData = {senderPlayer.playerId, colorData};

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;
                                    Packet(COLOR_DATA, sizeof(incomingColorData),
                                           reinterpret_cast<uint8_t *>(&incomingColorData)).send(levelPlayer.peer);
                                }
                            }
                        }

                        case (JOIN_LEVEL): {
                            int levelId = *reinterpret_cast<int *>(packet.data);
                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);
                            playerMap[event.peer].levelId = levelId;

                            levelList[levelId].push_back(senderPlayer);

                            IncomingIconData incomingIconData = {
                                    senderPlayer.playerId,
                                    senderPlayer.iconData
                            };

                            IncomingColorData incomingColorData = {
                                    senderPlayer.playerId,
                                    senderPlayer.colorData
                            };

                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                if (levelPlayer.peer) {
                                    IncomingColorData levelPlayerIncomingColorData = {
                                            levelPlayer.playerId,
                                            levelPlayer.colorData
                                    };

                                    IncomingIconData levelPlayerIncomingIconData = {
                                            levelPlayer.playerId,
                                            levelPlayer.iconData
                                    };

                                    IncomingRenderData levelPlayerIncomingRenderData = {
                                            levelPlayer.playerId,
                                            levelPlayer.renderData
                                    };

                                    Packet(JOIN_LEVEL, 4, reinterpret_cast<uint8_t *>(&senderPlayer.playerId)).send(
                                            levelPlayer.peer);
                                    Packet(JOIN_LEVEL, 4, reinterpret_cast<uint8_t *>(&levelPlayer.playerId)).send(
                                            senderPlayer.peer);

                                    Packet(ICON_DATA, sizeof(levelPlayerIncomingIconData),
                                           reinterpret_cast<uint8_t *>(&levelPlayerIncomingIconData)).send(
                                            senderPlayer.peer);
                                    Packet(ICON_DATA, sizeof(incomingIconData),
                                           reinterpret_cast<uint8_t *>(&incomingIconData)).send(levelPlayer.peer);

                                    Packet(COLOR_DATA, sizeof(levelPlayerIncomingColorData),
                                           reinterpret_cast<uint8_t *>(&levelPlayerIncomingColorData)).send(
                                            senderPlayer.peer);
                                    Packet(COLOR_DATA, sizeof(incomingColorData),
                                           reinterpret_cast<uint8_t *>(&incomingColorData)).send(levelPlayer.peer);

                                    Packet(RENDER_DATA, sizeof(levelPlayerIncomingRenderData),
                                           reinterpret_cast<uint8_t *>(&levelPlayerIncomingRenderData)).send(
                                            senderPlayer.peer);
                                }
                            }

                            break;
                        }

                        case (LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value())
                                break;

                            int levelId = *senderPlayer.levelId;

                            fmt::print("Player {} left level {}\n", senderPlayer.playerId, levelId);

                            for (auto &player: levelList[levelId]) {
                                if (player.playerId == senderPlayer.playerId)
                                    continue;

                                Packet(LEAVE_LEVEL, 4, reinterpret_cast<uint8_t *>(&senderPlayer.playerId)).send(
                                        player.peer);
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

                            auto renderData = *reinterpret_cast<RenderData *>(packet.data);

                            senderPlayer.renderData = renderData;

                            /*fmt::print("Player {}: P1[{} {}]\t P2[{} {}]\n", senderPlayer.playerId,
                                       renderData.playerOne.po`sition.x, renderData.playerOne.position.y,
                                       renderData.playerTwo.position.x, renderData.playerTwo.position.y);*/

                            IncomingRenderData incomingRenderData = {
                                    senderPlayer.playerId,
                                    renderData
                            };

                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                Packet(RENDER_DATA, sizeof(incomingRenderData),
                                       reinterpret_cast<uint8_t *>(&incomingRenderData))
                                        .send(levelPlayer.peer);
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
#pragma clang diagnostic pop
    enet_host_destroy(server);
    return 0;
}