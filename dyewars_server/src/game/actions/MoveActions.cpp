/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "Actions.h"
#include "core/Common.h"
#include "game/PlayerRegistry.h"

namespace Actions::Movement {
    std::shared_ptr<Player> MoveCommand::Execute(const GameContext &ctx) const {
        auto player = ctx.players.GetByClientID(client_id);
        if (!player) return nullptr;

        // AttemptMove now handles cooldown + facing validation internally
        if (player->AttemptMove(direction, facing, ctx.map)) {
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }

    std::shared_ptr<Player> TurnCommand::Execute(const GameContext &ctx) const {
        auto player = ctx.players.GetByClientID(player_id);
        if (!player) return nullptr;

        if (player->AttemptTurn(direction)) {
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }
}
