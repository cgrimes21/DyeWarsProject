#include "Actions.h"
#include "server/GameServer.h"
#include "network/packets/outgoing/PacketSender.h"
#include "core/Log.h"

namespace Actions::Movement {
    void Move(GameServer *server, uint64_t client_id, uint8_t direction, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            // Get client ping for cooldown adjustment
            uint32_t ping_ms = 0;
            auto conn = server->Clients().GetClientCopy(client_id);
            if (conn) {
                ping_ms = conn->GetPing();
            }

            auto result = player->AttemptMove(direction, facing, server->GetWorld().GetMap(), ping_ms);

            if (result == MoveResult::Success) {
                server->GetWorld().UpdatePlayerPosition(
                        player->GetID(),
                        player->GetX(),
                        player->GetY());
                server->Players().MarkDirty(player);
            } else {
                // Move failed (collision, cooldown, etc.) - rubber band client back
                Log::Trace("Player {} move attempt failed: dir={}, facing={}, result={}",
                           player->GetID(), direction, facing, static_cast<int>(result));
                auto conn = server->Clients().GetClientCopy(client_id);
                if (conn) {
                    Packets::PacketSender::PositionCorrection(
                            conn,
                            player->GetX(),
                            player->GetY(),
                            player->GetFacing());
                }
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