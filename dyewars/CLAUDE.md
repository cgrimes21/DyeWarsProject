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

The codebase uses **direct dependency injection** for core game flow (easy to trace with "Find All References") and **events only for optional listeners** (UI, audio, input).

**ServiceLocator** (`Assets/Scripts/Core/ServiceLocator.cs`): Central service registry. Services register themselves in Awake() and are retrieved via `ServiceLocator.Get<T>()`.

**EventBus** (`Assets/Scripts/Core/EventBus.cs`): Pub/sub event system. Used for:
- Input events (DirectionInputEvent)
- Server corrections to LocalPlayerController
- Disconnection events for UI
- NOT used for core packet handling flow

### Network Layer

**NetworkService** (`Assets/Scripts/Network/NetworkService.cs`): Main orchestrator. Creates PacketHandler with injected dependencies (PlayerRegistry, PlayerViewFactory).

**PacketHandler** (`Assets/Scripts/Network/Incoming/PacketHandler.cs`): Parses incoming packets and calls methods directly on injected dependencies. Easy to trace flow with "Find All References".

**PacketSender** (`Assets/Scripts/Network/Outgoing/PacketSender.cs`): Constructs and sends outgoing packets.

### Player System

**PlayerRegistry** (`Assets/Scripts/Player/PlayerRegistry.cs`): Single source of truth for player state. Has public handler methods called directly by PacketHandler:
- `HandleLocalPlayerAssigned(playerId)`
- `HandleLocalPositionCorrection(position, facing)`
- `HandleLocalFacingCorrection(facing)`
- `HandleRemotePlayerJoined(playerId, position, facing)`
- `HandleRemotePlayerUpdate(playerId, position, facing)`
- `HandlePlayerLeft(playerId)`

**PlayerViewFactory** (`Assets/Scripts/Player/PlayerViewFactory.cs`): Creates/destroys player views. Called directly by PacketHandler:
- `CreateLocalPlayerView(playerId)`
- `CreateRemotePlayerView(playerId, position, facing)`
- `DestroyPlayerView(playerId)`

**LocalPlayerController** (`Assets/Scripts/Player/LocalPlayerController.cs`): Handles input and movement. Subscribes to:
- `DirectionInputEvent` (from InputService)
- `LocalPlayerPositionCorrectedEvent` (from PlayerRegistry)
- `LocalPlayerFacingChangedEvent` (from PlayerRegistry)

**PlayerView** (`Assets/Scripts/Player/PlayerView.cs`): Visual representation. Methods called directly by PlayerViewFactory or LocalPlayerController.

## Key Patterns

1. **Direct DI for core flow**: PacketHandler -> PlayerRegistry/PlayerViewFactory (traceable)
2. **Events for optional listeners**: Input, corrections, UI events
3. **Threading**: Network receives on background thread, packets processed in Update()
4. **Client-Side Prediction**: Local player moves immediately, server corrections via events

## Adding New Packet Types

1. Add opcode constant to `PacketOpcodes.cs`
2. Add handler method in `PacketHandler.ProcessPacket()`
3. Add public method on target service (PlayerRegistry, etc.)
4. Call the method directly from handler

## Opcode Implementation Status

### Server -> Client (Direct DI)

| Opcode | Handler | Calls |
|--------|---------|-------|
| 0x10 S_Welcome | HandleWelcome | PlayerRegistry.HandleLocalPlayerAssigned, PlayerViewFactory.CreateLocalPlayerView |
| 0x11 S_Position_Correction | HandlePositionCorrection | PlayerRegistry.HandleLocalPositionCorrection |
| 0x12 S_Facing_Correction | HandleFacingCorrection | PlayerRegistry.HandleLocalFacingCorrection |
| 0x25 S_Joined_Game | HandlePlayerJoined | PlayerRegistry.HandleRemotePlayerJoined, PlayerViewFactory.CreateRemotePlayerView |
| 0x26 S_Left_Game | HandlePlayerLeft | PlayerRegistry.HandlePlayerLeft, PlayerViewFactory.DestroyPlayerView |
| 0x27 S_RemotePlayer_Update | HandleBatchUpdate | PlayerRegistry.HandleRemotePlayerUpdate (per player) |
| 0xF2 S_Kick_Notification | HandleKick | EventBus.Publish(DisconnectedFromServerEvent) |
| 0xF7 S_Pong_Response | HandlePong | (logs latency) |

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
Direct method calls:
    ├── PlayerRegistry.Handle*() - updates player data
    └── PlayerViewFactory.Create*/Destroy*() - creates/destroys views
            ↓
        PlayerRegistry publishes correction events (if needed)
            ↓
        LocalPlayerController subscribes - handles corrections
```
