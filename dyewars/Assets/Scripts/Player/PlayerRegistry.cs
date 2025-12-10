// PlayerRegistry.cs
// Centralized storage for all player data (local and remote).
// This is the single source of truth for player state.
//
// Other systems query this registry to get player information.
// Subscribes to EventBus events to update player data.

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

        private void OnEnable()
        {
            EventBus.Subscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPositionCorrected);
            EventBus.Subscribe<LocalPlayerFacingChangedEvent>(OnLocalFacingChanged);
            EventBus.Subscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Subscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Subscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Unsubscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPositionCorrected);
            EventBus.Unsubscribe<LocalPlayerFacingChangedEvent>(OnLocalFacingChanged);
            EventBus.Unsubscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Unsubscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Unsubscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
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
        // EVENT HANDLERS - Subscribed to EventBus events from PacketHandler
        // ====================================================================

        private void OnWelcomeReceived(WelcomeReceivedEvent evt)
        {
            if (localPlayer != null)
            {
                Debug.LogWarning("PlayerRegistry: Local player already exists!");
                return;
            }

            Debug.Log($"PlayerRegistry: Creating local player with ID {evt.PlayerId} at {evt.Position} facing {evt.Facing}");
            localPlayer = new PlayerData(evt.PlayerId, isLocalPlayer: true);
            localPlayer.SetPosition(evt.Position);
            localPlayer.SetFacing(evt.Facing);
            players[evt.PlayerId] = localPlayer;
            // Publish for other listeners (UI, etc.)
            EventBus.Publish(new LocalPlayerIdAssignedEvent { PlayerId = evt.PlayerId }, this);
        }

        private void OnLocalPositionCorrected(LocalPlayerPositionCorrectedEvent evt)
        {
            if (localPlayer == null) return;

            int dx = Mathf.Abs(localPlayer.Position.x - evt.Position.x);
            int dy = Mathf.Abs(localPlayer.Position.y - evt.Position.y);

            if (dx > 1 || dy > 1)
                Debug.Log("PlayerRegistry: Large correction, snapping to " + evt.Position);
            else if (dx > 0 || dy > 0)
                Debug.Log("PlayerRegistry: Small correction to " + evt.Position);

            localPlayer.SetPosition(evt.Position);
        }

        private void OnLocalFacingChanged(LocalPlayerFacingChangedEvent evt)
        {
            if (localPlayer == null) return;
            localPlayer.SetFacing(evt.Facing);
        }

        private void OnPlayerJoined(PlayerJoinedEvent evt)
        {
            if (players.ContainsKey(evt.PlayerId))
            {
                Debug.LogWarning("PlayerRegistry: Player " + evt.PlayerId + " already exists");
                return;
            }

            var player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
            player.SetPosition(evt.Position);
            player.SetFacing(evt.Facing);
            players[evt.PlayerId] = player;

            Debug.Log("PlayerRegistry: Remote player " + evt.PlayerId + " joined at " + evt.Position);
        }

        private void OnRemotePlayerUpdate(RemotePlayerUpdateEvent evt)
        {
            // Skip our own updates
            if (localPlayer != null && evt.PlayerId == localPlayer.PlayerId) return;

            if (!players.TryGetValue(evt.PlayerId, out var player))
            {
                // New remote player discovered via batch update
                player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
                players[evt.PlayerId] = player;
                Debug.Log("PlayerRegistry: New remote player " + evt.PlayerId + " discovered");
            }

            player.SetPosition(evt.Position);
            player.SetFacing(evt.Facing);
        }

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            if (players.Remove(evt.PlayerId))
                Debug.Log("PlayerRegistry: Player " + evt.PlayerId + " removed");
        }
    }
}
