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

                            IncomingPacket incomingLeaveLevelPacket;
                            incomingLeaveLevelPacket.set_playerid(senderPlayer.playerId);

                            PacketUtility::sendPacket(player.peer, incomingLeaveLevelPacket);
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
                        case (USERNAME): {
                            senderPlayer.username = packet.bytedata();

                            if (!senderPlayer.levelId.has_value()) break;

                            IncomingPacket incomingUsernamePacket;
                            incomingUsernamePacket.set_playerid(senderPlayer.playerId);
                            incomingUsernamePacket.set_bytedata(packet.bytedata());

                            for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(levelPlayer.peer, incomingUsernamePacket);
                            }

                            break;
                        }
                        case (ICON_DATA): {
                            senderPlayer.iconData = packet.icondata().SerializeAsString();

                            if (!senderPlayer.levelId.has_value()) break;
                            IncomingPacket incomingIconDataPacket;
                            incomingIconDataPacket.set_type(ICON_DATA);
                            incomingIconDataPacket.set_playerid(senderPlayer.playerId);
                            incomingIconDataPacket.mutable_icondata()->CopyFrom(packet.icondata());

                            for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(levelPlayer.peer, incomingIconDataPacket);
                            }

                            break;
                        }

                        case (COLOR_DATA): {
                            senderPlayer.colorData = packet.colordata().SerializeAsString();

                            if (senderPlayer.levelId.has_value()) {
                                IncomingPacket incomingColorDataPacket;
                                incomingColorDataPacket.set_type(COLOR_DATA);
                                incomingColorDataPacket.set_playerid(senderPlayer.playerId);
                                incomingColorDataPacket.mutable_colordata()->CopyFrom(packet.colordata());

                                for (auto &levelPlayer: levelList[senderPlayer.levelId.value()]) {
                                    if (levelPlayer.playerId == senderPlayer.playerId)
                                        continue;

                                    PacketUtility::sendPacket(levelPlayer.peer, incomingColorDataPacket);
                                }
                            }

                            break;
                        }

                        case (JOIN_LEVEL): {
                            int levelId = *reinterpret_cast<int *>(packet.bytedata());

                            fmt::print("Player {} joined level {}\n", senderPlayer.playerId, levelId);
                            playerMap[senderPlayer.playerId].levelId = levelId;

                            levelList[levelId].push_back(senderPlayer);

                            if (1 >= levelList[levelId].size()) break;

                            auto SHIT1 = IconData();
                            SHIT1.ParseFromString(senderPlayer.iconData);
                            IncomingPacket incomingIconDataPacket;
                            incomingIconDataPacket.set_type(ICON_DATA);
                            incomingIconDataPacket.set_playerid(senderPlayer.playerId);
                            incomingIconDataPacket.mutable_icondata()->CopyFrom(SHIT1);

                            auto SHIT2 = ColorData();
                            SHIT2.ParseFromString(senderPlayer.colorData);
                            IncomingPacket incomingColorDataPacket;
                            incomingColorDataPacket.set_type(COLOR_DATA);
                            incomingColorDataPacket.set_playerid(senderPlayer.playerId);
                            incomingColorDataPacket.mutable_colordata()->CopyFrom(SHIT2);

                            IncomingPacket incomingJoinLevelPacket;
                            incomingJoinLevelPacket.set_type(JOIN_LEVEL);
                            incomingJoinLevelPacket.set_playerid(senderPlayer.playerId);

                            IncomingPacket incomingUsernamePacket;
                            incomingUsernamePacket.set_type(USERNAME);
                            incomingUsernamePacket.set_playerid(senderPlayer.playerId);
                            incomingUsernamePacket.set_bytedata(senderPlayer.username.c_str());


                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                if (levelPlayer.peer) {

                                    auto FUCK = IconData();
                                    FUCK.ParseFromString(levelPlayer.iconData);
                                    IncomingPacket incomingLevelPlayerIconDataPacket;
                                    incomingLevelPlayerIconDataPacket.set_type(ICON_DATA);
                                    incomingLevelPlayerIconDataPacket.set_playerid(levelPlayer.playerId);
                                    incomingLevelPlayerIconDataPacket.mutable_icondata()->CopyFrom(FUCK);

                                    auto FUCK2 = ColorData();
                                    FUCK2.ParseFromString(levelPlayer.colorData);
                                    IncomingPacket incomingLevelPlayerColorDataPacket;
                                    incomingLevelPlayerColorDataPacket.set_type(COLOR_DATA);
                                    incomingLevelPlayerColorDataPacket.set_playerid(levelPlayer.playerId);
                                    incomingLevelPlayerColorDataPacket.mutable_colordata()->CopyFrom(FUCK2);

                                    IncomingPacket incomingLevelPlayerJoinPacket;
                                    incomingLevelPlayerJoinPacket.set_type(JOIN_LEVEL);
                                    incomingLevelPlayerJoinPacket.set_playerid(levelPlayer.playerId);

                                    IncomingPacket incomingLevelPlayerUsernamePacket;
                                    incomingLevelPlayerUsernamePacket.set_type(USERNAME);
                                    incomingLevelPlayerUsernamePacket.set_playerid(levelPlayer.playerId);
                                    incomingLevelPlayerUsernamePacket.set_bytedata(levelPlayer.username.c_str());

                                    auto FUCK3 = RenderData();
                                    FUCK3.ParseFromString(levelPlayer.renderData);
                                    IncomingPacket incomingLevelPlayerRenderDataPacket;
                                    incomingLevelPlayerRenderDataPacket.set_type(USERNAME);
                                    incomingLevelPlayerRenderDataPacket.set_playerid(levelPlayer.playerId);
                                    incomingLevelPlayerRenderDataPacket.mutable_renderdata()->CopyFrom(FUCK3);

                                    PacketUtility::sendPacket(levelPlayer.peer, incomingJoinLevelPacket);
                                    PacketUtility::sendPacket(senderPlayer.peer, incomingLevelPlayerJoinPacket);

                                    PacketUtility::sendPacket(levelPlayer.peer, incomingIconDataPacket);
                                    PacketUtility::sendPacket(senderPlayer.peer, incomingLevelPlayerIconDataPacket);

                                    PacketUtility::sendPacket(levelPlayer.peer, incomingColorDataPacket);
                                    PacketUtility::sendPacket(senderPlayer.peer, incomingLevelPlayerColorDataPacket);

                                    PacketUtility::sendPacket(levelPlayer.peer, incomingUsernamePacket);
                                    PacketUtility::sendPacket(senderPlayer.peer, incomingLevelPlayerUsernamePacket);

                                    PacketUtility::sendPacket(senderPlayer.peer, incomingLevelPlayerRenderDataPacket);
                                }
                            }

                            break;
                        }

                        case (LEAVE_LEVEL): {
                            if (!senderPlayer.levelId.has_value())
                                break;

                            int levelId = *senderPlayer.levelId;

                            fmt::print("Player {} left level {}\n", senderPlayer.playerId, levelId);

                            IncomingPacket incomingLeaveLevelPacket;
                            incomingLeaveLevelPacket.set_type(LEAVE_LEVEL);
                            incomingLeaveLevelPacket.set_playerid(senderPlayer.playerId);

                            for (auto &player: levelList[levelId]) {
                                if (player.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(player.peer, incomingLeaveLevelPacket);
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

                            /////////////////////////////////////////////////////
                        case (RENDER_DATA): {
                            if (!senderPlayer.levelId.has_value()) {
                                fmt::print("cringe\n");
                                break;
                            }

                            int levelId = *senderPlayer.levelId;

                            senderPlayer.renderData = packet.renderdata().SerializeAsString();

                            IncomingPacket incomingRenderDataPacket;
                            incomingRenderDataPacket.set_type(RENDER_DATA);
                            incomingRenderDataPacket.set_playerid(senderPlayer.playerId);
                            incomingRenderDataPacket.mutable_renderdata()->CopyFrom(packet.renderdata());

                            for (auto &levelPlayer: levelList[levelId]) {
                                if (levelPlayer.playerId == senderPlayer.playerId)
                                    continue;

                                PacketUtility::sendPacket(levelPlayer.peer, incomingRenderDataPacket);
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