// PlayerRegistry.cs
// Centralized storage for all player data (local and remote).
// This is the single source of truth for player state.
//
// Other systems query this registry to get player information.
// PacketHandler calls methods directly to update player data.

using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;

namespace DyeWars.Player
{
    public class PlayerRegistry : MonoBehaviour
    {
        // All players indexed by their ID
        private readonly Dictionary<ulong, PlayerData> players = new Dictionary<ulong, PlayerData>();

        // Quick reference to local player
        private PlayerData localPlayer;

        // Callback for view updates (set by PlayerViewFactory)
        public System.Action<ulong, Vector2Int, int> OnRemotePlayerUpdated;

        // Public access
        public PlayerData LocalPlayer => localPlayer;
        public IReadOnlyDictionary<ulong, PlayerData> AllPlayers => players;
        public int PlayerCount => players.Count;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<PlayerRegistry>(this);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<PlayerRegistry>();
        }

        // ====================================================================
        // PUBLIC API - Queries
        // ====================================================================

        public PlayerData GetPlayer(ulong playerId)
        {
            return players.TryGetValue(playerId, out var player) ? player : null;
        }

        public bool TryGetLocalPlayerID(out ulong playerId)
        {
            if (localPlayer != null)
            {
                playerId = localPlayer.PlayerId;
                return true;
            }
            playerId = default;
            return false;
        }

        public bool IsPositionOccupied(Vector2Int position)
        {
            foreach (var player in players.Values)
            {
                if (player.Position == position)
                    return true;
            }
            return false;
        }

        public bool HasPlayer(ulong playerId)
        {
            return players.ContainsKey(playerId);
        }

        public IEnumerable<PlayerData> GetRemotePlayers()
        {
            foreach (var player in players.Values)
            {
                if (!player.IsLocalPlayer)
                    yield return player;
            }
        }

        // ====================================================================
        // PUBLIC API - Local Player Actions (called by LocalPlayerController)
        // ====================================================================

        public void PredictLocalPosition(Vector2Int newPosition)
        {
            if (localPlayer == null) return;
            localPlayer.SetPosition(newPosition);
        }

        public void SetLocalFacing(int facing)
        {
            if (localPlayer == null) return;
            localPlayer.SetFacing(facing);
        }

        // ====================================================================
        // PACKET HANDLERS - Called directly by PacketHandler
        // ====================================================================

        public void HandleLocalPlayerAssigned(ulong playerId)
        {
            Debug.Log("PlayerRegistry: Creating local player with ID " + playerId);
            localPlayer = new PlayerData(playerId, isLocalPlayer: true);
            players[playerId] = localPlayer;
        }

        public void HandleLocalPositionCorrection(Vector2Int position, int facing)
        {
            if (localPlayer == null) return;

            int dx = Mathf.Abs(localPlayer.Position.x - position.x);
            int dy = Mathf.Abs(localPlayer.Position.y - position.y);

            if (dx > 1 || dy > 1)
                Debug.Log("PlayerRegistry: Large correction, snapping to " + position);
            else if (dx > 0 || dy > 0)
                Debug.Log("PlayerRegistry: Small correction to " + position);

            localPlayer.SetPosition(position);
            localPlayer.SetFacing(facing);

            // Notify LocalPlayerController via event (it needs to update its internal state)
            EventBus.Publish(new LocalPlayerPositionCorrectedEvent { Position = position });
            EventBus.Publish(new LocalPlayerFacingChangedEvent { Facing = facing });
        }

        public void HandleLocalFacingCorrection(int facing)
        {
            if (localPlayer == null) return;
            localPlayer.SetFacing(facing);
            EventBus.Publish(new LocalPlayerFacingChangedEvent { Facing = facing });
        }

        public void HandleRemotePlayerJoined(ulong playerId, Vector2Int position, int facing)
        {
            if (players.ContainsKey(playerId))
            {
                Debug.LogWarning("PlayerRegistry: Player " + playerId + " already exists");
                return;
            }

            var player = new PlayerData(playerId, isLocalPlayer: false);
            player.SetPosition(position);
            player.SetFacing(facing);
            players[playerId] = player;

            Debug.Log("PlayerRegistry: Remote player " + playerId + " joined at " + position);
        }

        public void HandleRemotePlayerUpdate(ulong playerId, Vector2Int position, int facing)
        {
            // Skip our own updates
            if (localPlayer != null && playerId == localPlayer.PlayerId) return;

            if (!players.TryGetValue(playerId, out var player))
            {
                // New remote player discovered via batch update
                player = new PlayerData(playerId, isLocalPlayer: false);
                players[playerId] = player;
                Debug.Log("PlayerRegistry: New remote player " + playerId + " discovered");
                
                // Notify view factory to create view
                OnRemotePlayerUpdated?.Invoke(playerId, position, facing);
            }

            player.SetPosition(position);
            player.SetFacing(facing);

            // Notify view to animate
            OnRemotePlayerUpdated?.Invoke(playerId, position, facing);
        }

        public void HandlePlayerLeft(ulong playerId)
        {
            if (players.Remove(playerId))
                Debug.Log("PlayerRegistry: Player " + playerId + " removed");
        }
    }
}
