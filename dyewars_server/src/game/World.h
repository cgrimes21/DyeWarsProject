/// =======================================
/// DyeWarsServer - World
///
/// Owns all world data:
/// - TileMap: static tile data (terrain, walls)
/// - SpatialHash: dynamic entity positions (players, NPCs)
///
/// Single point of access for all spatial queries.
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>
#include "TileMap.h"
#include "game/SpatialHash.h"
#include "game/Player.h"

class Player;   // Forward declare

/// ============================================================================
/// WORLD
///
/// The authoritative source for:
/// - "Can I walk here?" (TileMap)
/// - "Who's near here?" (SpatialHash)
/// - "Can player A see player B?" (Combines both)
///
/// Design:
/// - TileMap is pure data, doesn't know about players
/// - SpatialHash tracks player positions, doesn't know about tiles
/// - World coordinates between them
/// ============================================================================
class World {
public:
    /// ========================================================================
    /// CONFIGURATION
    /// ========================================================================

    /// How far players can see (in tiles)
    /// Used for view-based broadcasting and visibility checks
    static constexpr int16_t VIEW_RANGE = 10;

    /// ========================================================================
    /// CONSTRUCTION
    /// ========================================================================

    /// Create a world with a new tilemap
    explicit World(int16_t width, int16_t height)
            : tilemap_(std::make_unique<TileMap>(width, height)) {}

    explicit World(std::unique_ptr<TileMap> tileMap)
            : tilemap_(std::move(tileMap)) {
    }

    /// ========================================================================
    /// TILEMAP ACCESS - Static World Data
    /// ========================================================================

    /// Get the tilemap (for direct tile queries)
    TileMap &GetMap() { return *tilemap_; }

    const TileMap &GetMap() const { return *tilemap_; }

    /// ========================================================================
    /// PLAYER MANAGEMENT - Dynamic Entity Tracking
    /// ========================================================================

    /// Add a player to the world
    /// Call when player spawns or enters this world/zone
    void AddPlayer(
            uint64_t player_id,
            int16_t x,
            int16_t y,
            std::shared_ptr<Player> player = nullptr) {
        spatial_hash_.Add(player_id, x, y, player);
    }

    /// Remove a player from the world
    /// Call when player despawns, disconnects, or changes zones
    void RemovePlayer(uint64_t player_id) {
        spatial_hash_.Remove(player_id);
    }


    /// Update a player's position
    /// Call when player moves. Returns true if player changed spatial cells.
    bool UpdatePlayerPosition(uint64_t player_id, int16_t new_x, int16_t new_y) {
        return spatial_hash_.Update(player_id, new_x, new_y);
    }

    /// Get a player by ID
    std::shared_ptr<Player> GetPlayer(uint64_t player_id) const {
        return spatial_hash_.GetEntity(player_id);
    }

    /// Check if player exists in this world
    bool HasPlayer(uint64_t player_id) const {
        return spatial_hash_.Contains(player_id);
    }

    /// Get total player count in this world
    size_t PlayerCount() const {
        return spatial_hash_.Count();
    }

    /// ========================================================================
    /// SPATIAL QUERIES - Range and Visibility
    /// ========================================================================

    /// Get all players within VIEW_RANGE of a position
    /// Uses spatial hash for O(K) lookup, then exact distance filter
    std::vector<std::shared_ptr<Player>> GetPlayersInRange(int16_t x, int16_t y) const {
        return GetPlayersInRange(x, y, VIEW_RANGE);
    }

    /// Get all players within a custom range of a position
    std::vector<std::shared_ptr<Player>> GetPlayersInRange(int16_t x, int16_t y, int16_t range) const {
        // Coarse filter: get candidates from spatial hash
        auto candidates = spatial_hash_.GetNearbyEntities(x, y, range);

        // Fine filter: exact distance check
        std::vector<std::shared_ptr<Player>> result;
        result.reserve(candidates.size());

        for (const auto &player: candidates) {
            if (IsInRange(x, y, player->GetX(), player->GetY(), range)) {
                result.push_back(player);
            }
        }
        return result;
    }

    /// Get all player IDs within range (when you just need IDs)
    std::vector<uint64_t> GetPlayerIDsInRange(int16_t x, int16_t y) const {
        return GetPlayerIDsInRange(x, y, VIEW_RANGE);
    }

    /// Get all player IDs within a custom range
    std::vector<uint64_t> GetPlayerIDsInRange(int16_t x, int16_t y, int16_t range) const {
        auto candidates = spatial_hash_.GetNearbyIDs(x, y, range);

        // Fine filter with distance check
        std::vector<uint64_t> result;
        result.reserve(candidates.size());

        for (uint64_t id: candidates) {
            auto player = spatial_hash_.GetEntity(id);
            if (player && IsInRange(x, y, player->GetX(), player->GetY(), range)) {
                result.push_back(id);
            }
        }
        return result;
    }

    /// Get all players that can see a specific position
    /// (Same as GetPlayersInRange - if A can see B, B can see A)
    std::vector<std::shared_ptr<Player>> GetViewersOf(int16_t x, int16_t y) const {
        return GetPlayersInRange(x, y, VIEW_RANGE);
    }

    /// ========================================================================
    /// VISIBILITY CHECKS
    /// ========================================================================

    /// Check if two positions are within view range of each other
    /// Uses rectangular distance (faster than circular)
    bool IsInView(int16_t x1, int16_t y1, int16_t x2, int16_t y2) const {
        return IsInRange(x1, y1, x2, y2, VIEW_RANGE);
    }

    /// Check if two positions are within a custom range
    static bool IsInRange(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t range) {
        int16_t dx = std::abs(x1 - x2);
        int16_t dy = std::abs(y1 - y2);
        return (dx <= range && dy <= range);
    }

    /// Check if a player can see a position
    bool CanPlayerSee(uint64_t player_id, int16_t x, int16_t y) const {
        auto player = GetPlayer(player_id);
        if (!player) return false;
        return IsInView(player->GetX(), player->GetY(), x, y);
    }

    /// Check if player A can see player B
    bool CanSee(uint64_t viewer_id, uint64_t target_id) const {
        auto viewer = GetPlayer(viewer_id);
        auto target = GetPlayer(target_id);
        if (!viewer || !target) return false;
        return IsInView(viewer->GetX(), viewer->GetY(), target->GetX(), target->GetY());
    }

    /// ========================================================================
    /// ITERATION
    /// ========================================================================

    /// Iterate over all players in this world
    void ForEachPlayer(const std::function<void(uint64_t, const std::shared_ptr<Player> &)> &func) const {
        spatial_hash_.ForEach(func);
    }

    /// Get all players (copy of shared_ptrs)
    std::vector<std::shared_ptr<Player>> GetAllPlayers() const {
        std::vector<std::shared_ptr<Player>> result;
        result.reserve(PlayerCount());
        ForEachPlayer([&result](uint64_t, const std::shared_ptr<Player> &p) {
            result.push_back(p);
        });
        return result;
    }

    /// ========================================================================
    /// DEBUG / STATS
    /// ========================================================================

    /// Get number of active spatial hash cells
    size_t ActiveCellCount() const {
        return spatial_hash_.CellCount();
    }


private:
    /// ========================================================================
    /// DATA
    /// ========================================================================

    /// Static world data: tiles, terrain, walls
    std::unique_ptr<TileMap> tilemap_;

    /// Dynamic entity tracking: player positions
    SpatialHash spatial_hash_;
};
