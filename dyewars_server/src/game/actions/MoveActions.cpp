/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "Actions.h"
#include "core/Common.h"
#include "game/PlayerRegistry.h"
#include "server/GameServer.h"

namespace Actions::Movement {
    void Move(GameServer *server, uint64_t client_id, uint8_t direction, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            if (player->AttemptMove(direction, facing, server->GetWorld().GetMap())) {
                //player->SetDirty(true);
                // Instead of marking the player itself, we tell the system responsible for movements
                // that this player is dirty. that way we know what packets we need to broadcast
                // (batch player position update)
                server->Players().MarkDirty(player);
            }
        });
    }

    void Turn(GameServer *server, uint64_t client_id, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            player->SetFacing(facing);
            player->SetDirty(true);
        });
    }
}
