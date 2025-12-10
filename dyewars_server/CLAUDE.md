# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build
cmake --build build

# Run server
./build/DyeWarsServer.exe   # Windows
./build/DyeWarsServer       # Linux/Mac
```

## Dependencies (via vcpkg or system packages)

- asio (networking)
- nlohmann_json (JSON)
- spdlog (logging)
- sol2 + lua (Lua scripting)
- SQLite3 (database)

Requires C++20.

## File Structure

```
src/
├── main.cpp                     # Entry point, console command loop
├── core/
│   ├── Common.h                 # Common includes and typedefs
│   └── Log.h                    # spdlog wrapper
├── server/
│   ├── GameServer.h/cpp         # Central server, game loop, action queue
│   ├── ClientConnection.h/cpp   # Per-client TCP connection handler
│   └── ClientManager.h/cpp      # Tracks all connected clients
├── game/
│   ├── World.h                  # Owns TileMap + SpatialHash + VisibilityTracker
│   ├── TileMap.h                # Static tile/collision data
│   ├── SpatialHash.h            # O(1) player position lookups
│   ├── VisibilityTracker.h      # Tracks who each player can see (enter/leave view)
│   ├── Player.h                 # Player entity, movement validation
│   ├── PlayerRegistry.h         # Tracks all logged-in players
│   └── actions/
│       ├── Actions.h/cpp        # Action dispatcher
│       └── MoveActions.cpp      # Movement action handlers
├── network/
│   ├── BandwidthMonitor.h       # Traffic statistics
│   ├── ConnectionLimiter.h/cpp  # Rate limiting, IP bans
│   └── packets/
│       ├── Protocol.h           # Packet framing, reader/writer utilities
│       ├── OpCodes.h            # All opcodes with payload docs
│       ├── incoming/
│       │   └── PacketHandler.h/cpp  # Routes incoming packets
│       └── outgoing/
│           └── PacketSender.h   # Builds outgoing packets
├── database/
│   └── DatabaseManager.h/cpp    # SQLite persistence
└── lua/
    └── LuaEngine.h/cpp          # Lua scripting bridge
```

## Architecture

### Threading Model

The server uses two main threads:
1. **IO Thread** - ASIO event loop handles all network I/O
2. **Game Loop Thread** - Runs at fixed tick rate, processes game logic

Communication between threads uses a thread-safe action queue (`GameServer::QueueAction`). Network handlers queue lambdas that execute on the game thread.

### Core Components

**GameServer** (`src/server/GameServer.h`) - Central coordinator:
- Accepts TCP connections on port 8080
- Owns `ClientManager`, `PlayerRegistry`, `World`, `ConnectionLimiter`
- Runs the game loop and broadcasts updates

**ClientConnection** (`src/server/ClientConnection.h`) - Per-connection handler:
- Manages TCP socket lifecycle
- Handles handshake with magic bytes validation
- Parses packets and routes to `PacketHandler`

**World** (`src/game/World.h`) - Spatial authority:
- Contains `TileMap` (static collision data)
- Contains `SpatialHash` (dynamic player positions)
- Contains `VisibilityTracker` (who each player can see)
- Provides view-range queries for broadcasting

**VisibilityTracker** (`src/game/VisibilityTracker.h`) - Enter/leave view events:
- Tracks `known_players_`: player_id → set of player IDs they know about
- Tracks `known_by_`: reverse index for O(K) removal instead of O(N)
- `Update()` returns diff of who entered/left view after movement
- Uses scratch buffers to avoid per-call allocations

**Player** (`src/game/Player.h`) - Player entity:
- Owns position, facing, identity
- Validates movement (cooldowns, collision, facing, tile blocking, player blocking)
- Does NOT know about networking or other players

### Network Protocol

Custom binary protocol defined in `src/network/packets/`:
- **Protocol.h** - Packet framing (magic bytes 0x11 0x68, big-endian)
- **OpCodes.h** - All opcodes with documented payloads
- **PacketHandler** - Incoming packet routing (incoming/)
- **PacketSender** - Outgoing packet building (outgoing/)

Opcode naming: `C_` = Client->Server, `S_` = Server->Client

### Opcode Flow

**Incoming (Client -> Server)** - Handled in `PacketHandler.cpp`:

| Opcode | Constant | Payload | Action | Status |
|--------|----------|---------|--------|--------|
| `0x01` | `Movement::C_Move_Request` | `[dir:1][facing:1]` (3 bytes) | `Actions::Movement::Move()` | Implemented |
| `0x02` | `Movement::C_Turn_Request` | `[dir:1]` (2 bytes) | `Actions::Movement::Turn()` | Implemented |
| `0x04` | `Movement::C_Interact_Request` | (none) (1 byte) | - | TODO |
| `0x40` | `Combat::C_Attack_Request` | (none) (1 byte) | - | TODO |

**Outgoing (Server -> Client)** - Sent via `PacketSender.h`:

| Opcode | Constant | Payload | Sender Function |
|--------|----------|---------|-----------------|
| `0x10` | `LocalPlayer::S_Welcome` | `[id:8][x:2][y:2][facing:1]` (14 bytes) | `Welcome()` |
| `0x11` | `LocalPlayer::S_Position_Correction` | `[x:2][y:2][facing:1]` (6 bytes) | `PositionCorrection()` |
| `0x12` | `LocalPlayer::S_Facing_Correction` | `[facing:1]` (2 bytes) | `FacingCorrection()` |
| `0x25` | `Batch::S_Player_Spatial` | `[count:1][[id:8][x:2][y:2][facing:1]]...` (2+ bytes, 13 per player) | `BatchPlayerSpatial()` / `PlayerSpatial()` |
| `0x26` | `RemotePlayer::S_Left_Game` | `[id:8]` (9 bytes) | `PlayerLeft()` |
| `0xF0` | `Server::Connection::S_HandshakeAccepted` | (none) (1 byte) | `GivePlayerID()` |
| `0xF2` | `Server::Connection::S_ServerShutdown` | `[reason:1]` (2 bytes) | `ServerShutdown()` |

**Batch Player Spatial (0x25)** - Unified packet for player position/facing:
- Client creates the player if it doesn't exist, otherwise updates position/facing
- Used for: initial player list on login, ongoing position broadcasts
- Replaces the old separate `S_Joined_Game` and `S_RemotePlayer_Update` packets

**Request -> Action -> Response Flow:**
```
Client sends C_Move_Request (0x01)
  -> PacketHandler::Handle() parses opcode
  -> Actions::Movement::Move() queues lambda via QueueAction()
  -> Game loop executes: Player::AttemptMove() validates
  -> Success: World::UpdatePlayerPosition(), PlayerRegistry::MarkDirty()
  -> VisibilityTracker::Update() calculates who entered/left view
  -> Send S_Player_Spatial (0x25) for players entering view
  -> Send S_Left_Game (0x26) for players leaving view
  -> BroadcastDirtyPlayers() sends S_Player_Spatial (0x25) to nearby players
```

### Movement Validation

`Player::AttemptMove()` validates in order:
1. **Cooldown** - Prevents speed hacking (adjusted for client ping)
2. **Facing** - Player must face movement direction
3. **Direction** - Must be 0-3 (N/E/S/W)
4. **Tile blocking** - `TileMap::IsTileBlocked()` checks walls + out of bounds
5. **Player blocking** - Callback checks if another player occupies target tile

```cpp
MoveResult AttemptMove(direction, facing, map, ping_ms, is_occupied_callback);
// Returns: Success, OnCooldown, WrongFacing, InvalidDirection, Blocked, OccupiedByPlayer
```

### Visibility Tracking

Two-way visibility problem:
- When A moves toward stationary B, the "dirty" system tells B about A
- But A never learns about B (B didn't move, so B isn't "dirty")

Solution: `VisibilityTracker` tracks who each player "knows about":
```
A moves → Update(A, visible_players)
        → Diff against A's known set
        → entered: players A just walked toward (send S_Player_Spatial)
        → left: players A walked away from (send S_Left_Game)
```

Data structures (bidirectional for O(K) removal):
```
known_players_: A → {B, C}     // A knows about B and C
known_by_:      B → {A, D}     // B is known by A and D
```

When B disconnects, only update A and D (who knew about B), not all N players.

### Lua Scripting

`LuaGameEngine` (`src/lua/LuaEngine.h`) exposes game events to Lua. Scripts in `../scripts/main.lua` can modify behavior. Hot-reload with console command `r`.

**Note:** Player IDs are passed to Lua as strings (not numbers) because Lua's `double` can't accurately represent `uint64_t` values larger than 2^53.

## Console Commands

When running the server:
- `start/stop/restart` - Server control
- `r` - Reload Lua scripts
- `stats` - Bandwidth and player counts
- `debug` - Enable trace logging
- `exit` - Shutdown

## Key Patterns

- Use `QueueAction()` to safely pass work from IO thread to game thread
- Player movement must pass `Player::AttemptMove()` validation
- Broadcast to nearby players via `World::GetPlayersInRange()`
- Packet sizes are explicitly defined in `Protocol::PayloadSize`
