#pragma once
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>
#include <map>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <fmt/core.h>

#include "struct.hpp"
#include "packet.hpp"
#include "enum.hpp"
#include "util.hpp"

struct Player {
    HSteamNetConnection connection;
    ///////////////////////////////////////
    int playerId;
    std::optional<int> levelId;
    ///////////////////////////////////////
    std::string username; // Unused for now
    IconData iconData;
    ColorData colorData;
};

int main();