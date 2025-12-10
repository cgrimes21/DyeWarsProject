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
│   ├── World.h                  # Owns TileMap + SpatialHash, spatial queries
│   ├── TileMap.h                # Static tile/collision data
│   ├── SpatialHash.h            # O(1) player position lookups
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
- Provides view-range queries for broadcasting

**Player** (`src/game/Player.h`) - Player entity:
- Owns position, facing, identity
- Validates movement (cooldowns, collision, facing)
- Does NOT know about networking

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
  -> BroadcastDirtyPlayers() sends S_Player_Spatial (0x25) to nearby players
```

### Lua Scripting

`LuaGameEngine` (`src/lua/LuaEngine.h`) exposes game events to Lua. Scripts in `../scripts/main.lua` can modify behavior. Hot-reload with console command `r`.

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
