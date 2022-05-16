#include "main.hpp"

// HSteamNetConnection, Player
std::unordered_map<ENetPeer *, Player> playerMap;
// LevelId, std::vector<Player>
std::unordered_map<int, std::vector<Player>> levelList;

ENetPeer *peer;

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

    if (server == NULL) {
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
                    fmt::print("Client connected from {}:{}\n", event.peer->address.host, event.peer->address.port);
                    playerMap[event.peer] = Player{event.peer, lastPlayerId++};
                    break;
                }

                case (ENET_EVENT_TYPE_DISCONNECT): {
                    fmt::print("Client disconnected from {}:{}\n", event.peer->address.host, event.peer->address.port);

                    Player senderPlayer = playerMap[event.peer];

                    // TODO: Send player left message to all players in the same level

                    playerMap.erase(event.peer);
                    break;
                }

                case (ENET_EVENT_TYPE_RECEIVE): {
                    Player senderPlayer = playerMap[event.peer];
                    auto packet = Packet::serialize(event.packet);

                    fmt::print("Me -> Player {}\nPacket Length: {}\nPacket Type: {}\nPacket's Data Length: {}\nHex:", senderPlayer.playerId, event.packet->dataLength,
                               packet.type, packet.length);
                    for (int x = 0; x < event.packet->dataLength; x++) {
                        fmt::print(" {:#04x}", event.packet->data[x]);
                    }
                    fmt::print("\n\n");

                    if (event.packet->dataLength < 5) {
                        fmt::print(stderr, "Received packet with invalid size.\n");
                        break;
                    }

                    switch (packet.type) {
                        case (JOIN_LEVEL): {
                            if (packet.length < 4) {
                                fmt::print(stderr, "Received invalid packet.\n");
                                break;
                            }

                            int levelId = *reinterpret_cast<int *>(packet.data);
                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);

                            //TODO: Get player data here

                            for (auto &player: levelList[levelId]) {
                                // TODO: Send PlayerData

                                Packet(JOIN_LEVEL, sizeof(int), reinterpret_cast<uint8_t*>(senderPlayer.playerId)).sendPacket(player.peer);
                            }

                            break;
                        }

                        case (LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value()) break;

                            int levelId = *senderPlayer.levelId;

                            for (auto &player: levelList[levelId]) {
                                Packet(LEAVE_LEVEL, sizeof(int),
                                       reinterpret_cast<uint8_t *>(&senderPlayer.playerId)).sendPacket(event.peer);
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
                        case (RENDER_DATA): {
                            if (!senderPlayer.levelId.has_value())
                                break;

                            int levelId = *senderPlayer.levelId;

                            auto renderData = json(*reinterpret_cast<uint8_t *>(packet.data),
                                                   packet.length).get<RenderData>();

                            auto incomingRenderData = json::to_bson(
                                    json(IncomingRenderData{senderPlayer.playerId, renderData}));

                            for (auto &player: levelList[levelId]) {
                                if (player.playerId == senderPlayer.playerId) continue;

                                Packet(RENDER_DATA, incomingRenderData.size(),
                                       reinterpret_cast<uint8_t *>(incomingRenderData.data()))
                                        .sendPacket(player.peer);
                            }
                            break;
                        }
                    }

                    break;
                }
            }

            enet_packet_destroy(event.packet);
        }
    }

    // Clean up
    enet_host_destroy(server);
    return 0;
}