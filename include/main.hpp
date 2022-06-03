#pragma once
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <vector>
#include <string>
#include <map>

#include "struct.hpp"
#include "packet.hpp"
#include <enet/enet.h>
#include <fmt/core.h>

struct Player {
    ENetPeer* peer{};
    ///////////////////////////////////////
    uint16_t playerId{};
    std::optional<int32_t> levelId;
    ///////////////////////////////////////
    RenderData renderData{};
    //IconData iconData{};
    //ColorData colorData{};
};

int main(int, char**);