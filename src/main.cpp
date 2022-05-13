#include "main.hpp"

// PeerId, ENetPeer*
std::unordered_map<unsigned int, ENetPeer*> peerReference;
// PeerId, PlayerData
std::unordered_map<unsigned int, ServerPlayerSkinData> playerDataList;
// Level ID, PeerId
std::unordered_map<int, std::vector<unsigned int>> playerLevelList;

unsigned int lastNetID = 0;

std::unordered_map<int, std::vector<unsigned int, std::allocator<unsigned int>>> getLevelMapWithNetId(unsigned int netId) {
    auto clonedLevelList = std::unordered_map(playerLevelList);

    auto it = clonedLevelList.begin();
    while (it != clonedLevelList.end())
    {
        if (std::find(clonedLevelList.begin(), clonedLevelList.end(), netId) != clonedLevelList.end()) {
            clonedLevelList.erase(it++);
        }
        else {
            ++it;
        }
    }

    return clonedLevelList;
}

void updateRenderData(unsigned int netID, RenderData renderData) {
    auto levelList = getLevelMapWithNetId(netID);

    PlayerRenderData renderData = {
        netID,
        renderData.gamemode,
        renderData.posX,
        renderData.posY,
        renderData.rotation,
        renderData.flipped,
        renderData.dual
    };

    for (const auto& entry : levelList) {
        auto playerList = playerLevelList[entry.first];

        for (const auto peerID : playerList) {
            if (peerID == netID) continue;
            ENetPeer* peer = peerReference[peerID];
            if (peer) Packet(UPDATE_PLAYER_RENDER_DATA, sizeof(renderData), reinterpret_cast<uint8_t*>(&renderData)).send(peer);
        }
    }
}

void updateSkinData(unsigned int netID, ServerPlayerSkinData newSkinData) {
    auto levelList = getLevelMapWithNetId(netID);

    ClientPlayerSkinData skinData = {
        netID,
        newSkinData.username,
		newSkinData.ship,
		newSkinData.ball,
		newSkinData.bird,
		newSkinData.dart,
		newSkinData.robot,
		newSkinData.spider,
		newSkinData.glow,
		newSkinData.color,
		newSkinData.color2
    };

    const std::string jsonData = json(skinData).dump();

    for (const auto& entry : levelList) {
        auto playerList = playerLevelList[entry.first];

        for (const auto peerID : playerList) {
            if (peerID == netID) continue;
            ENetPeer* peer = peerReference[peerID];
            if (peer) Packet(UPDATE_PLAYER_RENDER_DATA, jsonData.length()+1, reinterpret_cast<uint8_t*>((char *) jsonData.c_str())).send(peer);
        }
    }
}

void playerLeaveLevel(unsigned int netID) {
    auto levelList = getLevelMapWithNetId(netID);

    for (const auto& entry : levelList) {
        auto playerList = playerLevelList[entry.first];
        playerList.erase(std::remove(playerList.begin(), playerList.end(), netID), playerList.end());

        for (const auto peerID : playerList) {
            ENetPeer* peer = peerReference[peerID];
            if (peer) Packet(PLAYER_LEAVE_LEVEL, 4, reinterpret_cast<uint8_t*>(&netID)).send(peer);
        }
    }
}

int main()
{
    if (enet_initialize() != 0)
    {
        fmt::print("An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    ENetAddress address;
    ENetHost* server;

    address.host = ENET_HOST_ANY;
    address.port = 23973;

    server = enet_host_create(&address,
        1024,
        1,
        0,
        0);

    if (server == NULL)
    {
        fmt::print("An error occurred while trying to create an ENet server host.\n");
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        ENetEvent event;

        while (enet_host_service(server, &event, 0) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
            {
                event.peer->data = new unsigned char[sizeof(unsigned int)];

                memcpy(event.peer->data, &lastNetID, sizeof(unsigned int));
                peerReference[lastNetID++] = event.peer;

                Packet packet = Packet(PLAYER_DATA);
                packet.send(event.peer);
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE:
            {
                unsigned int netID = *reinterpret_cast<unsigned int*>(event.peer->data);
                auto packet = Packet(event.packet);

                fmt::print("Peer {} -> Host\nPacket Length: {}\nPacket Type: {}\nPacket's Data Length: {}\nHex:", netID, event.packet->dataLength, packet.type, packet.length);
                for (int x = 0; x < event.packet->dataLength; x++) {
                    fmt::print(" {:#04x}", packet[x]);
                }
                fmt::print("\n\n");

                switch (packet.type) {
					
                case PLAYER_DATA: {
                    playerDataList[netID] = json((char*)packet.data).get<ServerPlayerSkinData>();
                    break;
                }

                case JOIN_LEVEL: {
                    if (playerDataList.find(netID) == playerDataList.end()) break;

                    uint32_t levelId = Util::uint8_t_to_uint32_t(packet.data);
                    if (playerLevelList.find(levelId) == playerLevelList.end()) playerLevelList[levelId].push_back(netID);

                    for (auto peerId : playerLevelList[levelId]) {
                        ENetPeer* peer = peerReference[peerId];
                        if (peer) {
							updateSkinData(netID, playerDataList[netID]);
                            Packet(PLAYER_JOIN_LEVEL, 4, reinterpret_cast<uint8_t*>(&netID)).send(peer);
                        }
                    }

                    break;
                }

                case RENDER_DATA: {
                    if (playerDataList.find(netID) == playerDataList.end()) break;

                    RenderData renderData = *reinterpret_cast<RenderData*>(packet.data);
                    updateRenderData(netID, renderData);
                    break;
                };
							
                case LEAVE_LEVEL: {
                    playerLeaveLevel(netID);
                    break;
                }
								
                }

                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                unsigned int netID = *reinterpret_cast<unsigned int*>(event.peer->data);

                peerReference.erase(netID);
                playerDataList.erase(netID);
                playerLeaveLevel(netID);
                break;
            }
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}