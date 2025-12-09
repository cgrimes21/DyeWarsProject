// PlayerViewFactory.cs
// Factory for creating player GameObjects (local and remote).
// PacketHandler calls methods directly to create/destroy views.
//
// Single responsibility: instantiate and destroy player visuals.
// The actual visual logic lives in PlayerView.

using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Game;

namespace DyeWars.Player
{
    public class PlayerViewFactory : MonoBehaviour
    {
        [Header("Prefabs")]
        [SerializeField] private GameObject localPlayerPrefab;
        [SerializeField] private GameObject remotePlayerPrefab;

        // Track created views for cleanup
        private GameObject localPlayerView;
        private readonly Dictionary<ulong, GameObject> remotePlayerViews = new Dictionary<ulong, GameObject>();

        // Service references
        private PlayerRegistry playerRegistry;
        private GridService gridService;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<PlayerViewFactory>(this);
        }

        private void Start()
        {
            playerRegistry = ServiceLocator.Get<PlayerRegistry>();
            gridService = ServiceLocator.Get<GridService>();

            // Register callback for remote player updates
            playerRegistry.OnRemotePlayerUpdated += OnRemotePlayerUpdated;
        }

        private void OnDestroy()
        {
            if (playerRegistry != null)
                playerRegistry.OnRemotePlayerUpdated -= OnRemotePlayerUpdated;

            ServiceLocator.Unregister<PlayerViewFactory>();
            DestroyAllViews();
        }

        // ====================================================================
        // PUBLIC API - Called by PacketHandler
        // ====================================================================

        public void CreateLocalPlayerView(ulong playerId)
        {
            if (localPlayerView != null)
            {
                Debug.LogWarning("PlayerViewFactory: Local player already exists!");
                return;
            }

            if (localPlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Local player prefab not assigned!");
                return;
            }

            Vector3 worldPos = Vector3.zero;
            if (playerRegistry?.LocalPlayer != null && gridService != null)
                worldPos = gridService.GridToWorld(playerRegistry.LocalPlayer.Position);

            localPlayerView = Instantiate(localPlayerPrefab, worldPos, Quaternion.identity);
            localPlayerView.name = "LocalPlayer_" + playerId;

            var view = localPlayerView.GetComponent<PlayerView>();
            if (view != null)
                view.InitializeAsLocalPlayer();

            Debug.Log("PlayerViewFactory: Created local player view for " + playerId);
        }

        public void CreateRemotePlayerView(ulong playerId, Vector2Int position, int facing)
        {
            if (remotePlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Remote player prefab not assigned!");
                return;
            }

            if (remotePlayerViews.ContainsKey(playerId))
            {
                Debug.LogWarning("PlayerViewFactory: Remote player " + playerId + " already exists!");
                return;
            }

            Vector3 worldPos = gridService != null ? gridService.GridToWorld(position) : Vector3.zero;

            var playerObj = Instantiate(remotePlayerPrefab, worldPos, Quaternion.identity);
            playerObj.name = "RemotePlayer_" + playerId;

            var view = playerObj.GetComponent<PlayerView>();
            if (view != null)
            {
                view.InitializeAsRemotePlayer(playerId, this);
                view.SetFacing(facing);
            }

            remotePlayerViews[playerId] = playerObj;
            Debug.Log("PlayerViewFactory: Created remote player view for " + playerId + " at " + position);
        }

        public void DestroyPlayerView(ulong playerId)
        {
            if (remotePlayerViews.TryGetValue(playerId, out var playerObj))
            {
                Destroy(playerObj);
                remotePlayerViews.Remove(playerId);
                Debug.Log("PlayerViewFactory: Destroyed remote player view for " + playerId);
            }
        }

        // ====================================================================
        // CALLBACK - Called by PlayerRegistry for batch updates
        // ====================================================================

        private void OnRemotePlayerUpdated(ulong playerId, Vector2Int position, int facing)
        {
            // Skip local player
            if (playerRegistry?.LocalPlayer != null && playerId == playerRegistry.LocalPlayer.PlayerId)
                return;

            // Create view if it does not exist
            if (!remotePlayerViews.ContainsKey(playerId))
            {
                CreateRemotePlayerView(playerId, position, facing);
                return;
            }

            // Update existing view
            if (remotePlayerViews.TryGetValue(playerId, out var playerObj))
            {
                var view = playerObj.GetComponent<PlayerView>();
                if (view != null)
                {
                    view.UpdateFromServer(position, facing);
                }
            }
        }

        // ====================================================================
        // INTERNAL
        // ====================================================================

        private void DestroyAllViews()
        {
            if (localPlayerView != null)
            {
                Destroy(localPlayerView);
                localPlayerView = null;
            }

            foreach (var kvp in remotePlayerViews)
            {
                if (kvp.Value != null)
                    Destroy(kvp.Value);
            }
            remotePlayerViews.Clear();
        }

        public GameObject GetLocalPlayerObject() => localPlayerView;
        public GameObject GetRemotePlayerObject(ulong playerId) => 
            remotePlayerViews.TryGetValue(playerId, out var obj) ? obj : null;
    }
}
