/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "Actions.h"
#include "core/Common.h"
#include "game/PlayerRegistry.h"

namespace Actions::Movement {

    std::shared_ptr<Player> MoveCommand::Execute(GameContext& ctx) const {
        auto player = ctx.players.GetByID(player_id);
        if (!player) return nullptr;

        // AttemptMove now handles cooldown + facing validation internally
        if (player->AttemptMove(direction, facing, ctx.map)) {
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }

    std::shared_ptr<Player> TurnCommand::Execute(GameContext& ctx) const {
        auto player = ctx.players.GetByID(player_id);
        if (!player) return nullptr;

        if (player->AttemptTurn(direction)) {
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }
}
