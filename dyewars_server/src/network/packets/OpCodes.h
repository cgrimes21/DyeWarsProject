// PacketOpcodes.hpp
// Centralized definition of all network packet opcodes.
// This is the single source of truth for packet types used by client and server.
//
// Usage:
//   Protocol::Opcode::LocalPlayer::S_Died
//   Protocol::Opcode::RemotePlayer::S_Entered_Range
//   Protocol::Opcode::Batch::S_RemotePlayer_Update
//   Protocol::Opcode::Map::S_Tile_Data
//
// Naming Convention:
//   C_ = Client -> Server
//   S_ = Server -> Client

#pragma once

#include <cstdint>
#include <string>

struct OpCodeInfo {
    uint8_t op;
    const char *desc;
    const char *name;
    uint8_t payloadSize;

    /// Payload size constant for variable-length packets
    static constexpr uint8_t VARIABLE_SIZE = 0;
};

namespace Protocol::Opcode {
    namespace Server::Connection {
        // ========================================================================
        // CONNECTION & HANDSHAKE - 0xF0 - 0xFF
        // ========================================================================

        // Server accepts handshake.
        // Payload: [serverVersion:2][serverMagic:4]
        constexpr OpCodeInfo S_HandshakeAccepted = {
            0xF0,
            "Server accepts client handshake",
            "S_HandshakeAccepted",
            7  // opcode(1) + version(2) + magic(4)
        };

        // Server rejects handshake.
        // Payload: [reasonCode:1][reasonLength:1][reason:variable]
        constexpr OpCodeInfo S_Handshake_Rejected = {
            0xF1,
            "Server rejects client handshake",
            "S_Handshake_Rejected",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Server cleanly disconnects client due to shutdown.
        // Payload: [reason:1]
        constexpr OpCodeInfo S_ServerShutdown = {
            0xF2,
            "Server shutting down",
            "S_ServerShutdown",
            2  // opcode(1) + reason(1)
        };

        // Server acknowledges disconnect.
        // Payload: (none)
        constexpr OpCodeInfo S_Disconnect_Acknowledged = {
            0xFF,
            "Server acknowledges client disconnect",
            "S_Disconnect_Acknowledged",
            1  // opcode only
        };

        // Server ping request.
        // Payload: [timestamp:4]
        constexpr OpCodeInfo S_Ping_Request = {
            0xF8,
            "Server requests ping from client",
            "S_Ping_Request",
            5  // opcode(1) + timestamp(4)
        };

        // Server heartbeat acknowledgment.
        // Payload: (none)
        constexpr OpCodeInfo S_Heartbeat_Response = {
            0xFB,
            "Server acknowledges client heartbeat",
            "S_Heartbeat_Response",
            1  // opcode only
        };
    }

    namespace Client::Connection {
        // ========================================================================
        // CLIENT CONNECTION & HANDSHAKE
        // ========================================================================

        // Initial connection handshake from client.
        // Payload: [version:2][clientMagic:4]
        constexpr OpCodeInfo C_Handshake_Request = {
            0x00,
            "Client sends handshake to server",
            "C_Handshake_Request",
            7  // opcode(1) + version(2) + magic(4)
        };

        // Client requests graceful disconnect.
        // Payload: (none)
        constexpr OpCodeInfo C_Disconnect_Request = {
            0xFE,
            "Client requests disconnect",
            "C_Disconnect_Request",
            1  // opcode only
        };

        // Client ping request.
        // Payload: [timestamp:4]
        constexpr OpCodeInfo C_Ping_Request = {
            0xF6,
            "Client requests ping from server",
            "C_Ping_Request",
            5  // opcode(1) + timestamp(4)
        };

        // Client pong response.
        // Payload: [timestamp:4]
        constexpr OpCodeInfo C_Pong_Response = {
            0xF9,
            "Client responds to server ping",
            "C_Pong_Response",
            5  // opcode(1) + timestamp(4)
        };

        // Client heartbeat.
        // Payload: (none)
        constexpr OpCodeInfo C_Heartbeat_Request = {
            0xFA,
            "Client sends heartbeat",
            "C_Heartbeat_Request",
            1  // opcode only
        };
    }

    // ========================================================================
    // CLIENT MOVEMENT & ACTIONS - 0x01-0x04
    // ========================================================================
    namespace Movement {
        // Player requests to move.
        // Payload: [direction:1][facing:1]
        constexpr OpCodeInfo C_Move_Request = {
            0x01,
            "Client requests movement",
            "C_Move_Request",
            3  // opcode(1) + direction(1) + facing(1)
        };

        // Player requests to turn.
        // Payload: [direction:1]
        constexpr OpCodeInfo C_Turn_Request = {
            0x02,
            "Client requests turn",
            "C_Turn_Request",
            2  // opcode(1) + direction(1)
        };

        // Player requests to warp/teleport.
        // Payload: [targetMapId:2][targetX:2][targetY:2]
        constexpr OpCodeInfo C_Warp_Request = {
            0x03,
            "Client requests warp",
            "C_Warp_Request",
            7  // opcode(1) + mapId(2) + x(2) + y(2)
        };

        // Player interacts with something.
        // Payload: (none)
        constexpr OpCodeInfo C_Interact_Request = {
            0x04,
            "Client requests interaction",
            "C_Interact_Request",
            1  // opcode only
        };
    }

    // ========================================================================
    // LOCAL PLAYER (Server -> Client) - 0x10-0x12
    // ========================================================================
    namespace LocalPlayer {
        // Welcome packet with player ID and initial state.
        // Payload: [playerId:8][x:2][y:2][facing:1]
        constexpr OpCodeInfo S_Welcome = {
            0x10,
            "Server sends welcome with player state",
            "S_Welcome",
            14  // opcode(1) + playerId(8) + x(2) + y(2) + facing(1)
        };

        // Position correction (rubber-banding).
        // Payload: [x:2][y:2][facing:1]
        constexpr OpCodeInfo S_Position_Correction = {
            0x11,
            "Server corrects client position",
            "S_Position_Correction",
            6  // opcode(1) + x(2) + y(2) + facing(1)
        };

        // Facing correction.
        // Payload: [facing:1]
        constexpr OpCodeInfo S_Facing_Correction = {
            0x12,
            "Server corrects client facing",
            "S_Facing_Correction",
            2  // opcode(1) + facing(1)
        };
    }

    // ========================================================================
    // REMOTE PLAYERS (Server -> Client) - 0x26
    // ========================================================================
    namespace RemotePlayer {
        // Player left the game.
        // Payload: [playerId:8]
        constexpr OpCodeInfo S_Left_Game = {
            0x26,
            "Remote player left the game",
            "S_Left_Game",
            9  // opcode(1) + playerId(8)
        };
    }

    // ========================================================================
    // BATCH UPDATES (Server -> Client) - 0x25
    // ========================================================================
    namespace Batch {
        // Batch player spatial update (position + facing).
        // Creates player on client if doesn't exist, otherwise updates.
        // Payload: [count:1][[playerId:8][x:2][y:2][facing:1]]... (13 bytes per player)
        constexpr OpCodeInfo S_Player_Spatial = {
            0x25,
            "Batch player position/facing update",
            "S_Player_Spatial",
            OpCodeInfo::VARIABLE_SIZE  // 2 + (13 * count)
        };
    }

    // ========================================================================
    // COMBAT (Client -> Server) - 0x40
    // ========================================================================
    namespace Combat {
        // Player attacks.
        // Payload: (none, uses facing)
        constexpr OpCodeInfo C_Attack_Request = {
            0x40,
            "Client requests attack",
            "C_Attack_Request",
            1  // opcode only
        };
    }

} // namespace Protocol::Opcode

// See UnusedOpCodes.h for planned but not yet implemented opcodes
#include "UnusedOpCodes.h"

// ============================================================================
// OPCODE UTILITIES
// ============================================================================
namespace Protocol::OpcodeUtil {
    inline std::string GetName(uint8_t opcode) {
        switch (opcode) {
            // Server Connection
            case Opcode::Server::Connection::S_HandshakeAccepted.op:
                return Opcode::Server::Connection::S_HandshakeAccepted.name;
            case Opcode::Server::Connection::S_Handshake_Rejected.op:
                return Opcode::Server::Connection::S_Handshake_Rejected.name;
            case Opcode::Server::Connection::S_ServerShutdown.op:
                return Opcode::Server::Connection::S_ServerShutdown.name;
            case Opcode::Server::Connection::S_Disconnect_Acknowledged.op:
                return Opcode::Server::Connection::S_Disconnect_Acknowledged.name;
            case Opcode::Server::Connection::S_Ping_Request.op:
                return Opcode::Server::Connection::S_Ping_Request.name;
            case Opcode::Server::Connection::S_Heartbeat_Response.op:
                return Opcode::Server::Connection::S_Heartbeat_Response.name;

            // Client Connection
            case Opcode::Client::Connection::C_Handshake_Request.op:
                return Opcode::Client::Connection::C_Handshake_Request.name;
            case Opcode::Client::Connection::C_Disconnect_Request.op:
                return Opcode::Client::Connection::C_Disconnect_Request.name;
            case Opcode::Client::Connection::C_Ping_Request.op:
                return Opcode::Client::Connection::C_Ping_Request.name;
            case Opcode::Client::Connection::C_Pong_Response.op:
                return Opcode::Client::Connection::C_Pong_Response.name;
            case Opcode::Client::Connection::C_Heartbeat_Request.op:
                return Opcode::Client::Connection::C_Heartbeat_Request.name;

            // Movement
            case Opcode::Movement::C_Move_Request.op:
                return Opcode::Movement::C_Move_Request.name;
            case Opcode::Movement::C_Turn_Request.op:
                return Opcode::Movement::C_Turn_Request.name;
            case Opcode::Movement::C_Warp_Request.op:
                return Opcode::Movement::C_Warp_Request.name;
            case Opcode::Movement::C_Interact_Request.op:
                return Opcode::Movement::C_Interact_Request.name;

            // LocalPlayer
            case Opcode::LocalPlayer::S_Welcome.op:
                return Opcode::LocalPlayer::S_Welcome.name;
            case Opcode::LocalPlayer::S_Position_Correction.op:
                return Opcode::LocalPlayer::S_Position_Correction.name;
            case Opcode::LocalPlayer::S_Facing_Correction.op:
                return Opcode::LocalPlayer::S_Facing_Correction.name;

            // Unused LocalPlayer
            case Opcode::Unused::LocalPlayer::S_Stats_Update.op:
                return Opcode::Unused::LocalPlayer::S_Stats_Update.name;
            case Opcode::Unused::LocalPlayer::S_Exp_Gained.op:
                return Opcode::Unused::LocalPlayer::S_Exp_Gained.name;
            case Opcode::Unused::LocalPlayer::S_Level_Up.op:
                return Opcode::Unused::LocalPlayer::S_Level_Up.name;
            case Opcode::Unused::LocalPlayer::S_Died.op:
                return Opcode::Unused::LocalPlayer::S_Died.name;
            case Opcode::Unused::LocalPlayer::S_Respawned.op:
                return Opcode::Unused::LocalPlayer::S_Respawned.name;
            case Opcode::Unused::LocalPlayer::S_Appearance_Changed.op:
                return Opcode::Unused::LocalPlayer::S_Appearance_Changed.name;
            case Opcode::Unused::LocalPlayer::S_Warped.op:
                return Opcode::Unused::LocalPlayer::S_Warped.name;

            // Unused Map
            case Opcode::Unused::Map::S_Tile_Data.op:
                return Opcode::Unused::Map::S_Tile_Data.name;
            case Opcode::Unused::Map::S_Tile_Update.op:
                return Opcode::Unused::Map::S_Tile_Update.name;
            case Opcode::Unused::Map::S_Map_Info.op:
                return Opcode::Unused::Map::S_Map_Info.name;
            case Opcode::Unused::Map::S_Object_Data.op:
                return Opcode::Unused::Map::S_Object_Data.name;
            case Opcode::Unused::Map::S_Collision_Data.op:
                return Opcode::Unused::Map::S_Collision_Data.name;

            // RemotePlayer
            case Opcode::RemotePlayer::S_Left_Game.op:
                return Opcode::RemotePlayer::S_Left_Game.name;

            // Unused RemotePlayer
            case Opcode::Unused::RemotePlayer::S_Entered_Range.op:
                return Opcode::Unused::RemotePlayer::S_Entered_Range.name;
            case Opcode::Unused::RemotePlayer::S_Left_Range.op:
                return Opcode::Unused::RemotePlayer::S_Left_Range.name;
            case Opcode::Unused::RemotePlayer::S_Appearance_Changed.op:
                return Opcode::Unused::RemotePlayer::S_Appearance_Changed.name;
            case Opcode::Unused::RemotePlayer::S_Died.op:
                return Opcode::Unused::RemotePlayer::S_Died.name;
            case Opcode::Unused::RemotePlayer::S_Respawned.op:
                return Opcode::Unused::RemotePlayer::S_Respawned.name;

            // Unused Entity
            case Opcode::Unused::Entity::S_Entered_Range.op:
                return Opcode::Unused::Entity::S_Entered_Range.name;
            case Opcode::Unused::Entity::S_Left_Range.op:
                return Opcode::Unused::Entity::S_Left_Range.name;
            case Opcode::Unused::Entity::S_Position_Update.op:
                return Opcode::Unused::Entity::S_Position_Update.name;
            case Opcode::Unused::Entity::S_State_Changed.op:
                return Opcode::Unused::Entity::S_State_Changed.name;
            case Opcode::Unused::Entity::S_Target_Changed.op:
                return Opcode::Unused::Entity::S_Target_Changed.name;
            case Opcode::Unused::Entity::S_Died.op:
                return Opcode::Unused::Entity::S_Died.name;
            case Opcode::Unused::Entity::S_Respawned.op:
                return Opcode::Unused::Entity::S_Respawned.name;

            // Batch
            case Opcode::Batch::S_Player_Spatial.op:
                return Opcode::Batch::S_Player_Spatial.name;
            case Opcode::Unused::Batch::S_Entity_Update.op:
                return Opcode::Unused::Batch::S_Entity_Update.name;

            // Combat
            case Opcode::Combat::C_Attack_Request.op:
                return Opcode::Combat::C_Attack_Request.name;

            // Unused Combat
            case Opcode::Unused::Combat::S_Effect_Play.op:
                return Opcode::Unused::Combat::S_Effect_Play.name;
            case Opcode::Unused::Combat::S_Damage.op:
                return Opcode::Unused::Combat::S_Damage.name;
            case Opcode::Unused::Combat::S_Heal.op:
                return Opcode::Unused::Combat::S_Heal.name;
            case Opcode::Unused::Combat::S_Skill_Cast.op:
                return Opcode::Unused::Combat::S_Skill_Cast.name;
            case Opcode::Unused::Combat::S_Buff_Applied.op:
                return Opcode::Unused::Combat::S_Buff_Applied.name;
            case Opcode::Unused::Combat::S_Buff_Removed.op:
                return Opcode::Unused::Combat::S_Buff_Removed.name;
            case Opcode::Unused::Combat::S_Miss.op:
                return Opcode::Unused::Combat::S_Miss.name;
            case Opcode::Unused::Combat::S_Critical.op:
                return Opcode::Unused::Combat::S_Critical.name;
            case Opcode::Unused::Combat::C_Skill_Use_Request.op:
                return Opcode::Unused::Combat::C_Skill_Use_Request.name;
            case Opcode::Unused::Combat::C_Target_Request.op:
                return Opcode::Unused::Combat::C_Target_Request.name;
            case Opcode::Unused::Combat::C_Target_Clear_Request.op:
                return Opcode::Unused::Combat::C_Target_Clear_Request.name;

            // Unused Chat
            case Opcode::Unused::Chat::C_Message_Send.op:
                return Opcode::Unused::Chat::C_Message_Send.name;
            case Opcode::Unused::Chat::C_Emote_Send.op:
                return Opcode::Unused::Chat::C_Emote_Send.name;
            case Opcode::Unused::Chat::C_Whisper_Send.op:
                return Opcode::Unused::Chat::C_Whisper_Send.name;
            case Opcode::Unused::Chat::S_Message_Broadcast.op:
                return Opcode::Unused::Chat::S_Message_Broadcast.name;
            case Opcode::Unused::Chat::S_Emote_Broadcast.op:
                return Opcode::Unused::Chat::S_Emote_Broadcast.name;
            case Opcode::Unused::Chat::S_Whisper_Received.op:
                return Opcode::Unused::Chat::S_Whisper_Received.name;
            case Opcode::Unused::Chat::S_Whisper_Sent_Confirm.op:
                return Opcode::Unused::Chat::S_Whisper_Sent_Confirm.name;
            case Opcode::Unused::Chat::S_System_Message.op:
                return Opcode::Unused::Chat::S_System_Message.name;

            // Unused Inventory
            case Opcode::Unused::Inventory::C_Item_Use_Request.op:
                return Opcode::Unused::Inventory::C_Item_Use_Request.name;
            case Opcode::Unused::Inventory::C_Item_Drop_Request.op:
                return Opcode::Unused::Inventory::C_Item_Drop_Request.name;
            case Opcode::Unused::Inventory::C_Item_Pickup_Request.op:
                return Opcode::Unused::Inventory::C_Item_Pickup_Request.name;
            case Opcode::Unused::Inventory::C_Item_Move_Request.op:
                return Opcode::Unused::Inventory::C_Item_Move_Request.name;
            case Opcode::Unused::Inventory::S_Full_Update.op:
                return Opcode::Unused::Inventory::S_Full_Update.name;
            case Opcode::Unused::Inventory::S_Slot_Update.op:
                return Opcode::Unused::Inventory::S_Slot_Update.name;
            case Opcode::Unused::Inventory::S_GroundItem_Spawned.op:
                return Opcode::Unused::Inventory::S_GroundItem_Spawned.name;
            case Opcode::Unused::Inventory::S_GroundItem_Removed.op:
                return Opcode::Unused::Inventory::S_GroundItem_Removed.name;

            // Unused Debug
            case Opcode::Unused::Debug::S_Custom_Response.op:
                return Opcode::Unused::Debug::S_Custom_Response.name;
            case Opcode::Unused::Debug::C_Request_State.op:
                return Opcode::Unused::Debug::C_Request_State.name;
            case Opcode::Unused::Debug::S_State_Response.op:
                return Opcode::Unused::Debug::S_State_Response.name;

            // Unused System (0xF3-0xF5)
            case Opcode::Unused::System::S_Kick_Notification.op:  // 0xF3
                return Opcode::Unused::System::S_Kick_Notification.name;
            case Opcode::Unused::System::S_Shutdown_Warning.op:   // 0xF4
                return Opcode::Unused::System::S_Shutdown_Warning.name;
            case Opcode::Unused::System::S_Time_Sync.op:          // 0xF5
                return Opcode::Unused::System::S_Time_Sync.name;

            default:
                return "Unknown(0x" + std::to_string(opcode) + ")";
        }
    }

    inline bool IsClientToServer(uint8_t opcode) {
        return (opcode >= 0x00 && opcode <= 0x0F) ||
               (opcode >= 0x40 && opcode <= 0x4F) ||
               (opcode >= 0x50 && opcode <= 0x57) ||
               (opcode >= 0x60 && opcode <= 0x6F) ||
               (opcode >= 0x80 && opcode <= 0x8F) ||
               (opcode >= 0xA0 && opcode <= 0xAF) ||
               opcode == Opcode::Client::Connection::C_Ping_Request.op ||
               opcode == Opcode::Client::Connection::C_Pong_Response.op ||
               opcode == Opcode::Client::Connection::C_Heartbeat_Request.op ||
               opcode == Opcode::Client::Connection::C_Disconnect_Request.op ||
               opcode == Opcode::Unused::Debug::C_Request_State.op;
    }

    inline bool IsServerToClient(uint8_t opcode) {
        return !IsClientToServer(opcode);
    }

    inline std::string GetCategory(uint8_t opcode) {
        if (opcode == 0x00) return "Connection";
        if (opcode >= 0x01 && opcode <= 0x0F) return "Movement";
        if (opcode >= 0x10 && opcode <= 0x19) return "LocalPlayer";
        if (opcode >= 0x1A && opcode <= 0x1F) return "Map";
        if (opcode >= 0x20 && opcode <= 0x24) return "RemotePlayer";
        if (opcode == 0x25) return "Batch";
        if (opcode == 0x26) return "RemotePlayer";
        if (opcode >= 0x28 && opcode <= 0x2E) return "Entity";
        if (opcode == 0x2F) return "Batch";
        if (opcode >= 0x30 && opcode <= 0x4F) return "Combat";
        if (opcode >= 0x50 && opcode <= 0x5F) return "Chat";
        if (opcode >= 0x60 && opcode <= 0x7F) return "Inventory";
        if (opcode >= 0xE0 && opcode <= 0xEF) return "Debug";
        if (opcode >= 0xF0 && opcode <= 0xFF) return "System/Connection";
        return "Unknown";
    }
}
