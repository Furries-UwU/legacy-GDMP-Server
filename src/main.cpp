#include "main.hpp"

// UUID, PEER
std::unordered_map<unsigned int, ENetPeer *> peerReference;
// Level ID, UUID
std::unordered_map<int, std::vector<unsigned int>> playerLevelList;

unsigned int lastNetID = 0;

void playerLeaveLevel(unsigned int netID) {
    for (auto& pair : playerLevelList)
    {
        std::vector<unsigned int>& vec = pair.second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [netID](unsigned int id)
            { return id == netID; }),
            vec.end());
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
    ENetHost *server;

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

                fmt::print("Packet Length: {}\n", event.packet->dataLength);

                fmt::print("Hex:");
                for (int x = 0; x < event.packet->dataLength; x++) {
                    fmt::print(" {:#04x}", packet[x]);
                }

                fmt::print("\n");

                fmt::print("Packet Type: {}\nPacket Data Length: {}\n", packet.type, packet.length);

                if (packet.length > 0) {
                    fmt::print("ASCII: {}\n\n", packet.data);
                }

                switch (packet.type) {
                    case PLAYER_DATA: {
                        break;
                    }
                    case JOIN_LEVEL: {
                        int levelId = Util::uint8_t_to_int(packet.data);
                        fmt::print("Level ID: {}", levelId);
                        playerLevelList[levelId].push_back(netID);
                        break;
                    }
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
                unsigned int netID = *reinterpret_cast<unsigned int *>(event.peer->data);

                peerReference.erase(netID);
                playerLeaveLevel(netID);
                break;
            }
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}