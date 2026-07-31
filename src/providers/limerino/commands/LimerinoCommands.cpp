// SPDX-License-Identifier: MIT

#include "providers/limerino/commands/LimerinoCommands.hpp"

#include "controllers/commands/CommandController.hpp"
#include "providers/limerino/commands/Identity.hpp"

namespace chatterino::LimerinoCommands {

void initialize(CommandController &commands)
{
    // Batch 1 - Identity & account
    commands.registerExternalCommand("/uid", &uid);
    commands.registerExternalCommand("/namehistory", &nameHistory);
    commands.registerExternalCommand("/modlist", &modList);
    commands.registerExternalCommand("/ml", &modList);
}

}  // namespace chatterino::LimerinoCommands
