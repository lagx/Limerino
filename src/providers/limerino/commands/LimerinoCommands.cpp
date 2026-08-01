// SPDX-License-Identifier: MIT

#include "providers/limerino/commands/LimerinoCommands.hpp"

#include "controllers/commands/CommandController.hpp"
#include "providers/limerino/commands/Follows.hpp"
#include "providers/limerino/commands/Identity.hpp"

namespace chatterino::LimerinoCommands {

void initialize(CommandController &commands)
{
    // Batch 1 - Identity & account
    commands.registerExternalCommand("/uid", &uid);
    commands.registerExternalCommand("/namehistory", &nameHistory);
    commands.registerExternalCommand("/modlist", &modList);
    commands.registerExternalCommand("/ml", &modList);

    // Batch 2 - Follows (replaces upstream's deprecated /follow builtin)
    commands.registerExternalCommand("/follow", &follow);
}

}  // namespace chatterino::LimerinoCommands
