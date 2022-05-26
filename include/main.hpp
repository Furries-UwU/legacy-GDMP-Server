#pragma once
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <map>

#include "packetUtility.hpp"
#include "packet.pb.h"
#include <enet/enet.h>
#include <fmt/core.h>

struct Player {
    ENetPeer* peer;
    ///////////////////////////////////////
    int playerId;
    std::optional<int> levelId;
    ///////////////////////////////////////
    std::string username;
    RenderData renderData;
    IconData iconData;
    ColorData colorData;
};

int main(int, char**);