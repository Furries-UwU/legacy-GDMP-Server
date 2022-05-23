#pragma once
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <map>

#include <enet/enet.h>
#include <fmt/core.h>

#include "struct.hpp"
#include "packet.hpp"
#include "enum.hpp"

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

int main();