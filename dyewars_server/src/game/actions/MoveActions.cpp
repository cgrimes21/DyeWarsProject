#include "Actions.h"
#include "server/GameServer.h"

namespace Actions::Movement {
    void Move(GameServer *server, uint64_t client_id, uint8_t direction, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            auto result = player->AttemptMove(direction, facing, server->GetWorld().GetMap());

            if (result == MoveResult::Success) {
                server->GetWorld().UpdatePlayerPosition(
                        player->GetID(),
                        player->GetX(),
                        player->GetY());
                server->Players().MarkDirty(player);
            }
        });
    }

    void Turn(GameServer *server, uint64_t client_id, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            player->SetFacing(facing);
            server->Players().MarkDirty(player);
        });
    }
}