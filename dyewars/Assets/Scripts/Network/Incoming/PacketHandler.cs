// PacketHandler.cs
// All incoming packet processing in one place.
// When you add a new server->client packet, add it here.
//
// Single responsibility: parse incoming packets and call handlers directly.
//
// This class does NOT contain game logic. It:
//   1. Parses the packet bytes
//   2. Calls methods directly on injected dependencies
//
// This keeps all "what did the server send us?" logic in one file.
// Direct DI makes the flow easy to trace with "Find All References".

using UnityEngine;
using DyeWars.Network.Protocol;
using DyeWars.Player;

namespace DyeWars.Network.Inbound
{
    public class PacketHandler
    {
        // Direct dependencies - easy to trace with "Find All References"
        private readonly PlayerRegistry playerRegistry;
        private readonly PlayerViewFactory playerViewFactory;

        public PacketHandler(PlayerRegistry playerRegistry, PlayerViewFactory playerViewFactory)
        {
            this.playerRegistry = playerRegistry;
            this.playerViewFactory = playerViewFactory;
        }

        public void ProcessPacket(byte[] payload)
        {
            if (payload == null || payload.Length < 1) return;

            byte opcode = payload[0];
            int offset = 1;

            switch (opcode)
            {
                case Opcode.LocalPlayer.S_Position_Correction:
                    HandlePositionCorrection(payload, offset);
                    break;

                case Opcode.LocalPlayer.S_Welcome:
                    HandleWelcome(payload, offset);
                    break;

                case Opcode.LocalPlayer.S_Facing_Correction:
                    HandleFacingCorrection(payload, offset);
                    break;

                case Opcode.RemotePlayer.S_Left_Game:
                    HandlePlayerLeft(payload, offset);
                    break;

                case Opcode.Batch.S_RemotePlayer_Update:
                    HandleBatchUpdate(payload, offset);
                    break;

                case Opcode.RemotePlayer.S_Joined_Game:
                    HandlePlayerJoined(payload, offset);
                    break;

                case Opcode.Connection.S_Pong_Response:
                    HandlePong(payload, offset);
                    break;

                case Opcode.System.S_Kick_Notification:
                    HandleKick(payload, offset);
                    break;

                default:
                    Debug.LogWarning("PacketHandler: Unknown opcode 0x" + opcode.ToString("X2"));
                    break;
            }
        }

        private void HandleWelcome(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 8)) return;
            ulong playerId = PacketReader.ReadU64(payload, ref offset);
            Debug.Log("PacketHandler: Welcome received, player ID " + playerId);
            playerRegistry.HandleLocalPlayerAssigned(playerId);
            playerViewFactory.CreateLocalPlayerView(playerId);
        }

        private void HandlePositionCorrection(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Position_Correction - 1)) return;
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);
            playerRegistry.HandleLocalPositionCorrection(new Vector2Int(x, y), facing);
        }

        private void HandleFacingCorrection(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 1)) return;
            int facing = PacketReader.ReadU8(payload, ref offset);
            playerRegistry.HandleLocalFacingCorrection(facing);
        }

        private void HandlePlayerJoined(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 13)) return;
            ulong playerId = PacketReader.ReadU64(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);
            var position = new Vector2Int(x, y);
            playerRegistry.HandleRemotePlayerJoined(playerId, position, facing);
            playerViewFactory.CreateRemotePlayerView(playerId, position, facing);
        }

        private void HandlePlayerLeft(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 8)) return;
            ulong playerId = PacketReader.ReadU64(payload, ref offset);
            playerRegistry.HandlePlayerLeft(playerId);
            playerViewFactory.DestroyPlayerView(playerId);
        }

        private void HandleBatchUpdate(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 1)) return;
            int count = PacketReader.ReadU8(payload, ref offset);
            for (int i = 0; i < count; i++)
            {
                if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_Batch_RemotePlayer_Update_PerPlayer)) break;
                ulong playerId = PacketReader.ReadU64(payload, ref offset);
                int x = PacketReader.ReadU16(payload, ref offset);
                int y = PacketReader.ReadU16(payload, ref offset);
                int facing = PacketReader.ReadU8(payload, ref offset);
                playerRegistry.HandleRemotePlayerUpdate(playerId, new Vector2Int(x, y), facing);
            }
        }

        private void HandlePong(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;
            uint timestamp = PacketReader.ReadU32(payload, ref offset);
            uint now = (uint)(Time.realtimeSinceStartup * 1000);
            Debug.Log("PacketHandler: Ping latency = " + (now - timestamp) + "ms");
        }

        private void HandleKick(byte[] payload, int offset)
        {
            string reason = "Unknown";
            if (PacketReader.HasBytes(payload, offset, 1))
            {
                byte reasonLength = PacketReader.ReadU8(payload, ref offset);
                if (PacketReader.HasBytes(payload, offset, reasonLength))
                {
                    byte[] reasonBytes = new byte[reasonLength];
                    for (int i = 0; i < reasonLength; i++)
                        reasonBytes[i] = PacketReader.ReadU8(payload, ref offset);
                    reason = System.Text.Encoding.UTF8.GetString(reasonBytes);
                }
            }
            Debug.LogWarning("PacketHandler: Kicked - " + reason);
            Core.EventBus.Publish(new Core.DisconnectedFromServerEvent { Reason = reason });
        }
    }
}
