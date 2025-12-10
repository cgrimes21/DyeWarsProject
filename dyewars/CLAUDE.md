# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DyeWars is a 2D multiplayer game built with Unity (C#). The client connects to a game server over TCP using a custom binary protocol with big-endian encoding.

## Build and Run

This is a Unity project. Open in Unity Editor (2022.3 LTS or later):
- **Build**: File > Build Settings > Build
- **Play**: Press Play in Editor
- **Reload Scene**: Ctrl+R (in-game)

No command-line build scripts exist. Use Unity Editor for all build operations.

## Architecture

### Core Systems

The codebase uses **EventBus** for decoupled communication between systems. PacketHandler publishes events, and systems like PlayerRegistry and PlayerViewFactory subscribe to handle them.

**ServiceLocator** (`Assets/Scripts/Core/ServiceLocator.cs`): Central service registry. Services register themselves in Awake() and are retrieved via `ServiceLocator.Get<T>()`.

**EventBus** (`Assets/Scripts/Core/EventBus.cs`): Pub/sub event system. Used for:
- All packet handling (WelcomeReceivedEvent, PlayerJoinedEvent, etc.)
- Input events (DirectionInputEvent)
- Server corrections (LocalPlayerPositionCorrectedEvent, LocalPlayerFacingChangedEvent)
- Connection events (ConnectedToServerEvent, DisconnectedFromServerEvent)

### Network Layer

**NetworkService** (`Assets/Scripts/Network/NetworkService.cs`): Main orchestrator. Creates PacketHandler (no dependencies). Queues packets from background thread and processes them on main thread.

**PacketHandler** (`Assets/Scripts/Network/Incoming/PacketHandler.cs`): Parses incoming packets and publishes events via EventBus. No direct dependencies on other systems.

**PacketSender** (`Assets/Scripts/Network/Outgoing/PacketSender.cs`): Constructs and sends outgoing packets.

### Player System

**PlayerRegistry** (`Assets/Scripts/Player/PlayerRegistry.cs`): Single source of truth for player state. Subscribes to EventBus events:
- `WelcomeReceivedEvent` -> creates local player data, publishes `LocalPlayerIdAssignedEvent`
- `LocalPlayerPositionCorrectedEvent` -> updates local player position
- `LocalPlayerFacingChangedEvent` -> updates local player facing
- `PlayerJoinedEvent` -> creates remote player data
- `RemotePlayerUpdateEvent` -> updates remote player data
- `PlayerLeftEvent` -> removes player data

**PlayerViewFactory** (`Assets/Scripts/Player/PlayerViewFactory.cs`): Creates/destroys player views. Subscribes to EventBus events:
- `WelcomeReceivedEvent` -> creates local player view
- `PlayerJoinedEvent` -> creates remote player view
- `RemotePlayerUpdateEvent` -> updates or creates remote player view
- `PlayerLeftEvent` -> destroys player view

**LocalPlayerController** (`Assets/Scripts/Player/LocalPlayerController.cs`): Handles input and movement. Subscribes to:
- `DirectionInputEvent` (from InputService)
- `LocalPlayerPositionCorrectedEvent` (from PacketHandler)
- `LocalPlayerFacingChangedEvent` (from PacketHandler)

**PlayerView** (`Assets/Scripts/Player/PlayerView.cs`): Visual representation.

## Key Patterns

1. **EventBus for all packet handling**: PacketHandler publishes events, systems subscribe
2. **Multiple subscribers**: Same event can trigger multiple handlers (e.g., WelcomeReceivedEvent triggers both PlayerRegistry and PlayerViewFactory)
3. **Threading**: Network receives on background thread, packets processed in Update()
4. **Client-Side Prediction**: Local player moves immediately, server corrections via events

## Adding New Packet Types

1. Add opcode constant to `PacketOpcodes.cs`
2. Add event struct to `GameEvents.cs`
3. Add handler method in `PacketHandler.ProcessPacket()` that publishes the event
4. Subscribe to the event in systems that need to handle it (PlayerRegistry, PlayerViewFactory, etc.)

## Opcode Implementation Status

### Server -> Client (EventBus)

| Opcode | Handler | Publishes | Subscribers |
|--------|---------|-----------|-------------|
| 0x10 S_Welcome | HandleWelcome | WelcomeReceivedEvent | PlayerRegistry, PlayerViewFactory |
| 0x11 S_Position_Correction | HandleLocalPositionCorrection | LocalPlayerPositionCorrectedEvent, LocalPlayerFacingChangedEvent | PlayerRegistry, LocalPlayerController |
| 0x12 S_Facing_Correction | HandleLocalFacingCorrection | LocalPlayerFacingChangedEvent | PlayerRegistry, LocalPlayerController |
| 0x25 S_Joined_Game | HandleRemotePlayerSpatial | PlayerJoinedEvent | PlayerRegistry, PlayerViewFactory |
| 0x26 S_Left_Game | HandlePlayerLeft | PlayerLeftEvent | PlayerRegistry, PlayerViewFactory |
| 0x27 S_RemotePlayer_Update | HandleBatchUpdate | RemotePlayerUpdateEvent (per player) | PlayerRegistry, PlayerViewFactory |
| 0xF2 S_Kick_Notification | HandleKick | DisconnectedFromServerEvent | UI |
| 0xF7 S_Pong_Response | HandlePong | PongReceivedEvent | (logs latency) |

### Client -> Server

| Opcode | Method |
|--------|--------|
| 0x00 C_Handshake_Request | SendHandshake() |
| 0x01 C_Move_Request | SendMove() |
| 0x02 C_Turn_Request | SendTurn() |
| 0x04 C_Interact_Request | SendInteract() |
| 0x40 C_Attack_Request | SendAttack() |
| 0x41 C_Skill_Use_Request | SendUseSkill() |
| 0x50 C_Message_Send | SendChatMessage() |
| 0x51 C_Emote_Send | SendEmote() |
| 0xF6 C_Ping_Request | SendPing() |
| 0xFE C_Disconnect_Request | SendDisconnect() |

## Data Flow

```
Server sends packet
    ↓
NetworkService.ProcessQueues() (main thread)
    ↓
PacketHandler.ProcessPacket()
    ↓
EventBus.Publish(Event)
    ↓
Multiple subscribers handle the event:
    ├── PlayerRegistry - updates player data
    ├── PlayerViewFactory - creates/updates/destroys views
    └── LocalPlayerController - handles corrections (for position/facing events)
```
