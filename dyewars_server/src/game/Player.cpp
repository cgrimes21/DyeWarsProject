#include "Player.h"
#include "PlayerRegistry.h"

Player::Player(uint64_t id, int start_x, int start_y)
        : id_(id), x_(start_x), y_(start_y), facing_(2) {}

void Player::SetFacing(uint8_t direction) {
    if (direction <= 3 && direction >= 0) {  // Validate: 0-3 only
        facing_ = direction;
    }
}

void Player::SetPosition(int16_t x, int16_t y) {
    // TODO Bounds check / Density check
    x_ = x;
    y_ = y;
}

bool Player::AttemptMove(uint8_t direction, uint8_t sent_facing, const TileMap &map) {
    auto now = std::chrono::steady_clock::now();

    // Cooldown check (client is 350ms, we allow 330ms for network grace)
    if (now - last_move_time_ < PlayerRegistry::MOVE_COOLDOWN) {
        return false;
    }

    // Must be facing the direction you want to move
    if (direction != facing_ || sent_facing != facing_) {
        return false;
    }

    // Validate direction
    if (direction > 3) {
        return false;
    }

    int16_t new_x = x_;
    int16_t new_y = y_;

    // 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT
    switch (direction) {
        case 0:
            new_y++;
            break;
        case 1:
            new_x++;
            break;
        case 2:
            new_y--;
            break;
        case 3:
            new_x--;
            break;
        default:
            return false;
    }

    // Ask the Map: "Is this safe?"
    if (!map.IsWalkable(new_x, new_y)) {
        return false;
    }
    // Success - update state
    last_move_time_ = now;
    x_ = new_x;
    y_ = new_y;
    return true;
}

bool Player::AttemptTurn(uint8_t new_facing) {
    if (new_facing > 3 || new_facing == facing_) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    if (now - last_turn_time_ < PlayerRegistry::TURN_COOLDOWN) {
        return false;
    }

    last_turn_time_ = now;
    facing_ = new_facing;
    return true;
}
