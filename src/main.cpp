#include <algorithm>
#include "main.hpp"

// HSteamNetConnection, Player
std::unordered_map<ENetPeer *, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int, std::vector<Player>> levelList;

unsigned int lastPlayerId = 0;

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

                    // no idea if this works
                    for (auto& player : levelList[senderPlayer.levelId.value()]) {
                        // TODO: Send PlayerData

                        Packet(X2X_LEAVE_LEVEL, 4, reinterpret_cast<uint8_t*>(&senderPlayer.playerId)).send(player.peer);
                    }

                    playerMap.erase(event.peer);
                    break;
                }

                case (ENET_EVENT_TYPE_RECEIVE): {
                    Player senderPlayer = playerMap[event.peer];
                    auto packet = Packet(event.packet);

                    /*fmt::print("Player {} -> Server\tPacket Length: {}\tPacket Type: {}\tPacket's Data Length: {}\n", senderPlayer.playerId, event.packet->dataLength,
                               packet.type, packet.length);
                    for (int x = 0; x < event.packet->dataLength; x++) {
                        fmt::print(" {:#04x}", event.packet->data[x]);
                    }
                    fmt::print("\n\n");*/

                    if (event.packet->dataLength < 5) {
                        fmt::print(stderr, "Received packet with invalid size.\n");
                        break;
                    }

                    switch (packet.type) {
                        case (X2X_JOIN_LEVEL): {
                            if (packet.length < 4) {
                                fmt::print(stderr, "Received invalid packet.\n");
                                break;
                            }

                            int levelId = *reinterpret_cast<int *>(packet.data);
                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);
                            playerMap[event.peer].levelId = levelId;

                            fmt::print("uwu\n");
                            levelList[levelId].push_back(senderPlayer);
                            fmt::print("owo\n");

                            //TODO: Get player data here

                            for (auto &player: levelList[levelId]) {
                                //if(player.playerId == senderPlayer.playerId) continue;
                                // TODO: Send PlayerData

                                fmt::print("hm\n");
                                fmt::print("peer null: {}", player.peer == nullptr);
                                if (player.peer) {
                                    //Packet(S2C_UPDATE_PLAYER_DATA, sizeof(ClientPlayerData), reinterpret_cast<uint8_t*>(&playerDataList[netID])).send(peer);
                                    Packet(X2X_JOIN_LEVEL, 4, reinterpret_cast<uint8_t*>(&senderPlayer.playerId)).send(player.peer);
                                }
                                fmt::print("hmmm\n");
                            }

                            break;
                        }

                        case (X2X_LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value()) break;

                            int levelId = *senderPlayer.levelId;

                            fmt::print("Player {} left level {}\n", senderPlayer.playerId, levelId);

                            for (auto &player: levelList[levelId]) {
                                Packet(X2X_LEAVE_LEVEL, sizeof(int),
                                       reinterpret_cast<uint8_t *>(&senderPlayer.playerId)).send(event.peer);
                            }

                            levelList[levelId].erase(
                                    std::remove_if(levelList[levelId].begin(), levelList[levelId].end(),
                                                   [senderPlayer](Player &player) {
                                                       return player.playerId == senderPlayer.playerId;
                                                   }), levelList[levelId].end());

                            senderPlayer.levelId = std::nullopt;
                            break;
                        }

                            /////////////////////////////////////////////////////
                        case (C2S_RENDER_DATA): {
                            if (!senderPlayer.levelId.has_value()) {
                                fmt::print("cringe\n");
                                break;
                            }

                            int levelId = *senderPlayer.levelId;

                            auto renderData = *reinterpret_cast<RenderData*>(packet.data);

                            fmt::print("Player {}: P1[{} {}]\t P2[{} {}]\n", senderPlayer.playerId,
                                       renderData.playerOne.posX, renderData.playerOne.posY,
                                       renderData.playerTwo.posX, renderData.playerTwo.posY);

                            for (auto &player: levelList[levelId]) {
                                if (player.playerId == senderPlayer.playerId) continue;

                                PlayerRenderData playerRenderData = {
                                        senderPlayer.playerId,
                                        renderData.playerOne,
                                        renderData.playerTwo,
                                        renderData.visible,
                                        renderData.dual
                                };

                                Packet(S2C_UPDATE_PLAYER_RENDER_DATA, sizeof(playerRenderData), reinterpret_cast<uint8_t*>(&playerRenderData)).send(player.peer);
                            }
                            break;
                        }
                    }

                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    // idk lol
                    break;
            }

            enet_packet_destroy(event.packet);
        }
    }
#pragma clang diagnostic pop

    // Clean up
    enet_host_destroy(server);
    return 0;
}