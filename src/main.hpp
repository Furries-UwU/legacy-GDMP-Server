#pragma once
#include <unordered_map>
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
    unsigned int playerId;
    std::optional<int> levelId;
    ///////////////////////////////////////
    std::string username;
    ServerPlayerData renderData;
};

int main();