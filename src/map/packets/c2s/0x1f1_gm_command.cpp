/*
===========================================================================

  Copyright (c) 2025 LandSandBoat Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

===========================================================================
*/

#include "0x1f1_gm_command.h"

#include "command_handler.h"
#include "common/logging.h"
#include "common/lua.h"
#include "entities/char_entity.h"
#include "map_session.h"
#include "packets/c2s/validation.h"
#include "packets/s2c/0x1f1_gm_command_result.h"
#include "utils/jailutils.h"

auto GP_CLI_COMMAND_GM_COMMAND::validate(MapSession* PSession, const CCharEntity* PChar) const -> PacketValidationResult
{
    return PacketValidator(PChar)
        .range("Length", this->Length, 1, sizeof(this->Command));
}

void GP_CLI_COMMAND_GM_COMMAND::process(MapSession* PSession, CCharEntity* PChar) const
{
    using Result = GP_SERV_COMMAND_GM_COMMAND_RESULT::Result;

    // Same rule as '!' in chat: no commands from jail. The window is GM only, so players are refused too.
    if (PChar->m_GMlevel == 0 || jailutils::InPrison(PChar))
    {
        GP_SERV_COMMAND_GM_COMMAND_RESULT::send(PChar, this->RequestId, Result::Refused, {});
        return;
    }

    // Header (4) + RequestId (2) + Length (1) + padding (1) come before Command.
    const auto sent    = (header.size * 4U > 8U) ? (header.size * 4U) - 8U : 0U;
    const auto length  = std::min<std::size_t>({ this->Length, sent, sizeof(this->Command) });
    const auto command = asStringFromUntrustedSource(this->Command, length);
    if (command.empty())
    {
        GP_SERV_COMMAND_GM_COMMAND_RESULT::send(PChar, this->RequestId, Result::Failed, {});
        return;
    }

    // The same runner as chat '!' commands: it checks cmdprops permission and writes the audit log.
    // While it runs, what the command prints to this GM is collected for the reply instead of the chat log.
    // Lines printed later (from timers) still go to the chat log as usual.
    auto output              = std::vector<std::string>{};
    PChar->m_gmCommandOutput = &output;

    auto&      scheduler = *PSession->scheduler;
    const auto result    = CCommandHandler::call(scheduler, lua, PChar, command);

    PChar->m_gmCommandOutput = nullptr;
    GP_SERV_COMMAND_GM_COMMAND_RESULT::send(PChar, this->RequestId, result == CommandResult::Success ? Result::Ran : Result::Failed, output);
}
