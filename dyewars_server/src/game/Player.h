/// =======================================
/// DyeWarsServer - Player
///
/// Player entity with movement validation.
/// Owns its own state, asks TileMap for collision.
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>
#include "game/TileMap.h"

/// Forward declare MoveResult (defined in PlayerRegistry.h)
/// Or define it here if you prefer Player to be standalone
enum class MoveResult : uint8_t {
    Success,           // Move succeeded
    OnCooldown,        // Too soon since last move
    WrongFacing,       // Player not facing movement direction
    InvalidDirection,  // Direction value out of range (0-3)
    Blocked            // Tile is not walkable
};

/// ============================================================================
/// PLAYER
///
/// Represents a player entity in the game world.
///
/// Responsibilities:
/// - Own position, facing, identity
/// - Validate movement (cooldown, facing, direction)
/// - Ask TileMap for collision (doesn't own map data)
///
/// Does NOT:
/// - Know about other players (that's World/SpatialHash)
/// - Know about networking (that's ClientConnection)
/// - Manage its own lifecycle (that's PlayerRegistry)
/// ============================================================================
class Player {
public:
    explicit Player(
            uint64_t player_id,
            int start_x,
            int start_y,
            uint8_t facing = 0)
            : id_(player_id),
              x_(start_x),
              y_(start_y),
              facing_(facing),
              client_id_(0),
              last_move_time_(std::chrono::steady_clock::now() - std::chrono::seconds(1)) {

    }

    /// ========================================================================
    /// IDENTITY
    /// ========================================================================

    uint64_t GetID() const { return id_; }

    void SetClientID(uint64_t client_id) { client_id_ = client_id; }

    // Client link (which network connection owns this player)
    uint64_t GetClientID() const { return client_id_; }

    void SetName(const std::string &name) { name_ = name; }

    const std::string &GetName() const { return name_; }

    /// ========================================================================
    /// POSITION
    /// ========================================================================

    int16_t GetX() const { return x_; }

    int16_t GetY() const { return y_; }

    /// Direct position set (for teleport, spawn, admin commands) \n
    /// Does NOT validate - caller is responsible for checking walkability
    void SetPosition(int16_t x, int16_t y) {
        x_ = x;
        y_ = y;
    }

    /// ========================================================================
    /// FACING
    /// ========================================================================

    uint8_t GetFacing() const { return facing_; }

    /// Set facing direction (0=North, 1=East, 2=South, 3=West)
    void SetFacing(uint8_t facing) {
        if (facing <= 3) {
            facing_ = facing;
        }
    }

    MoveResult AttemptMove(uint8_t direction, uint8_t sent_facing, const TileMap &map) {
        const auto now = std::chrono::steady_clock::now();
        // ====================================================================
        // COOLDOWN CHECK
        // Prevent speed hacking by enforcing minimum time between moves
        // Client is 350ms, we allow 330ms for network grace
        // ====================================================================
        if (now - last_move_time_ < MOVE_COOLDOWN) {
            return MoveResult::OnCooldown;
        }
        // ====================================================================
        // FACING CHECK
        // Player must be facing the direction they want to move
        // This prevents "moonwalking" and ensures animation sync
        // ====================================================================
        if (direction != facing_ || sent_facing != facing_) {
            return MoveResult::WrongFacing;
        }

        // ====================================================================
        // DIRECTION VALIDATION
        // 0=North, 1=East, 2=South, 3=West
        // ====================================================================
        if (direction > 3) {
            return MoveResult::InvalidDirection;
        }

        // ====================================================================
        // CALCULATE NEW POSITION
        // ====================================================================
        int16_t new_x = x_;
        int16_t new_y = y_;

        switch (direction) {
            case 0:
                new_y++;
                break;  // North (Y+ is up in our coord system)
            case 1:
                new_x++;
                break;  // East
            case 2:
                new_y--;
                break;  // South
            case 3:
                new_x--;
                break;  // West
            default:
                return MoveResult::InvalidDirection;
        }

        // ====================================================================
        // COLLISION CHECK
        // Ask the map if destination is walkable
        // This is the only external dependency - we don't own map data
        // ====================================================================
        if (!map.IsTileBlocked(new_x, new_y)) {
            return MoveResult::Blocked;
        }

        // ====================================================================
        // SUCCESS - Update state
        // ====================================================================
        last_move_time_ = now;
        x_ = new_x;
        y_ = new_y;

        return MoveResult::Success;
    }

    /// Check if player can move (not on cooldown)
    bool CheckMoveCooldown() const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_move_time_) >= MOVE_COOLDOWN;
    }

    /// Get time until next move is allowed (for client prediction)
    std::chrono::milliseconds TimeUntilCanMove() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_move_time_);
        if (elapsed >= MOVE_COOLDOWN) {
            return std::chrono::milliseconds(0);
        }
        return MOVE_COOLDOWN - elapsed;
    }

    bool AttemptTurn(uint8_t new_facing) {
        if (new_facing > 3 || new_facing == facing_) {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_turn_time_ < TURN_COOLDOWN) {
            return false;
        }

        last_turn_time_ = now;
        facing_ = new_facing;
        return true;
    }


private:
    /// ========================================================================
    /// CONFIGURATION
    /// ========================================================================

    /// Movement cooldowns (client timing minus network grace)
    static constexpr auto MOVE_COOLDOWN = std::chrono::milliseconds(330);  // Client: 350ms
    static constexpr auto TURN_COOLDOWN = std::chrono::milliseconds(200);  // Client: 220ms
    static constexpr auto NETWORK_GRACE = std::chrono::milliseconds(20);

    // Identity
    uint64_t id_;
    uint64_t client_id_ = 0;
    std::string name_;

    // Position
    int16_t x_;
    int16_t y_;
    uint8_t facing_ = 2; // Default: facing down (0=up, 1=right, 2=down, 3=left)

    // Move Time Limits
    std::chrono::steady_clock::time_point last_move_time_{};
    std::chrono::steady_clock::time_point last_turn_time_{};
};