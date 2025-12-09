// NetworkService.cs
// The main network orchestrator. This MonoBehaviour ties together:
//   - NetworkConnection (raw TCP, runs receive on background thread)
//   - PacketSender (constructs and sends outgoing packets)
//   - PacketHandler (parses incoming packets, calls handlers directly)
//
// Key responsibility: Bridge between background thread and main thread.
// NetworkConnection receives packets on a background thread, but Unity
// and our game systems need to run on the main thread. NetworkService
// queues received packets and processes them in Update().

using System;
using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Network.Connection;
using DyeWars.Network.Outbound;
using DyeWars.Network.Inbound;
using DyeWars.Network.Protocol;
using DyeWars.Player;

namespace DyeWars.Network
{
    public class NetworkService : MonoBehaviour, INetworkService
    {
        [Header("Connection Settings")]
        [SerializeField] private string serverHost = "127.0.0.1";
        [SerializeField] private int serverPort = 8080;
        [SerializeField] private bool connectOnStart = true;
        private volatile bool connectedToServer = false;

        // Sub-components (composition over inheritance)
        private NetworkConnection connection;
        private PacketSender sender;
        public PacketSender Sender => sender;
        private PacketHandler handler;

        // Main thread dispatch queue
        private readonly Queue<byte[]> packetQueue = new Queue<byte[]>();
        private readonly object queueLock = new object();

        // INetworkService implementation
        public bool IsConnected => connection?.IsConnected ?? false;
        public byte[] LastSentPacket => connection?.LastSentPacket;
        public volatile byte[] lastReceivedPacket;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            // Create connection and sender immediately
            connection = new NetworkConnection();
            sender = new PacketSender(connection);

            // Subscribe to connection events (these fire on background thread!)
            connection.OnPacketReceived += OnPacketReceivedFromBackground;
            connection.OnConnected += OnConnectedFromBackground;
            connection.OnDisconnected += OnDisconnectedFromBackground;

            // Register this service so other systems can find it
            ServiceLocator.Register<INetworkService>(this);
        }

        private void Start()
        {
            // Now that all services are registered, create PacketHandler with dependencies
            var playerRegistry = ServiceLocator.Get<PlayerRegistry>();
            var playerViewFactory = ServiceLocator.Get<PlayerViewFactory>();
            handler = new PacketHandler(playerRegistry, playerViewFactory);

            if (connectOnStart)
            {
                Connect(serverHost, serverPort);
            }
        }

        private void Update()
        {
            if (connectedToServer)
            {
                EventBus.Publish(new ConnectedToServerEvent());
                connectedToServer = false;
            }
            ProcessQueues();
        }

        private void OnDestroy()
        {
            if (connection != null)
            {
                connection.OnPacketReceived -= OnPacketReceivedFromBackground;
                connection.OnConnected -= OnConnectedFromBackground;
                connection.OnDisconnected -= OnDisconnectedFromBackground;
            }

            Disconnect();
            ServiceLocator.Unregister<INetworkService>();
        }

        // ====================================================================
        // MAIN THREAD PACKET PROCESSING
        // ====================================================================

        private void ProcessQueues()
        {
            List<byte[]> packetsToProcess = null;

            lock (queueLock)
            {
                if (packetQueue.Count > 0)
                {
                    packetsToProcess = new List<byte[]>(packetQueue);
                    packetQueue.Clear();
                }
            }

            if (packetsToProcess != null)
            {
                foreach (var packet in packetsToProcess)
                {
                    lastReceivedPacket = packet;
                    handler.ProcessPacket(packet);
                }
            }
        }

        // ====================================================================
        // BACKGROUND THREAD CALLBACKS
        // ====================================================================

        private void OnPacketReceivedFromBackground(byte[] payload)
        {
            lock (queueLock)
            {
                packetQueue.Enqueue(payload);
            }
        }

        private void OnConnectedFromBackground()
        {
            sender.SendHandshake();
            connectedToServer = true;
            Debug.Log("NetworkService: Connected (from background thread)");
        }

        private void OnDisconnectedFromBackground(string reason)
        {
            Debug.Log("NetworkService: Disconnected - " + reason + " (from background thread)");
        }

        // ====================================================================
        // INetworkService IMPLEMENTATION
        // ====================================================================

        public void Connect(string host, int port)
        {
            connection.Connect(host, port);
        }

        public void Disconnect()
        {
            connection?.Disconnect();
            EventBus.Publish(new DisconnectedFromServerEvent { Reason = "Disconnected" });
        }

        public PacketSender GetSender() => sender;
        public void SendPing() => sender.SendPing();
        public void SendHeartbeatResponse() { }
        public void SendSecurityResponse() { }
    }
}
