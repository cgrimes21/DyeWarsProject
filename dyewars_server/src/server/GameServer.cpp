/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#include "GameServer.h"
#include "ClientConnection.h"
#include "core/Log.h"
#include "lua/LuaEngine.h"
#include "network/BandwidthMonitor.h"
#include "network/packets/outgoing/PacketSender.h"


GameServer::GameServer(asio::io_context &io_context)
        : io_context_(io_context),
          acceptor_(
                  io_context,
                  asio::ip::tcp::endpoint(
                          asio::ip::address::from_string(Protocol::ADDRESS),
                          Protocol::PORT)),
          world_(256, 256),
          lua_engine_(std::make_shared<LuaGameEngine>()) {
    Log::Info("Server starting on port {}...", Protocol::PORT);
    StartAccept();
    game_loop_thread_ = std::thread(&GameServer::GameLogicThread, this);
}

GameServer::~GameServer() {
    Shutdown();
}

/// ============================================================================
/// SERVER CONTROL
/// ============================================================================

void GameServer::Shutdown() {
    if (shutdown_requested_.exchange(true)) return;

    Log::Info("Shutting down server...");
    server_running_ = false;

    // Stop accepting connections
    std::error_code ec;
    acceptor_.close(ec);

    // Close all client connections
    clients_.CloseAll();

    // Wait for game loop to finish
    if (game_loop_thread_.joinable()) {
        game_loop_thread_.join();
    }

    io_context_.stop();

    Log::Info("Server shutdown complete");
}

void GameServer::ReloadScripts() const {
    if (lua_engine_) {
        lua_engine_->ReloadScripts();
    }
}

/// ============================================================================
/// NETWORKING
/// ============================================================================

void GameServer::StartAccept() {
    acceptor_.async_accept([this](
            const std::error_code ec,
            asio::ip::tcp::socket socket) {
        if (!ec && server_running_) {
            // If we fail getting ip, close socket and listen for next connection
            std::string ip;
            try {
                ip = socket.remote_endpoint().address().to_string();
            } catch (...) {
                socket.close();
                if (server_running_) StartAccept();
                return;
            }
            Log::Info("IP: {} trying to connect.", ip);

            if (limiter_.IsBanned(ip)) {
                Log::Trace("Rejected banned IP: {}", ip);
                socket.close();
            } else if (!limiter_.CheckRateLimit(ip)) {
                Log::Trace("Rate limited IP: {}", ip);
                socket.close();
            } else if (!limiter_.CanConnect(ip)) {
                Log::Trace("Connection limit reached for IP: {}", ip);
                socket.close();
            } else {
                limiter_.AddConnection(ip);

                uint64_t client_id = next_client_id_++;

                const auto client = std::make_shared<ClientConnection>(
                        std::move(socket),
                        this,
                        client_id);

                // Start the session's async read chain and handshake timer.
                // The session keeps itself alive via shared_from_this() in its async callbacks.
                // If the handshake fails or times out, the session will clean itself up.
                client->Start();
            }
        } else if (ec && server_running_) {
            // Only log if it's not a shutdown
            Log::Error("Accept failed: {}", ec.message());
        }
        // Only restart if still running
        if (server_running_) {
            StartAccept();
        }
    });
}

/// ============================================================================
/// ACTION QUEUE
/// ============================================================================

void GameServer::QueueAction(std::function<void()> action) {
    {
        std::lock_guard<std::mutex> lock(action_mutex_);
        action_queue_.push(std::move(action));
    }
}

void GameServer::ProcessActionQueue() {
    std::queue<std::function<void()> > to_process;
    {
        std::lock_guard<std::mutex> lock(action_mutex_);
        std::swap(to_process, action_queue_);
    }
    while (!to_process.empty()) {
        to_process.front()();
        to_process.pop();
    }
}

/// ============================================================================
/// GAME LOOP
/// ============================================================================

void GameServer::GameLogicThread() {
    constexpr int TICKS_PER_SECOND = 20;
    constexpr std::chrono::milliseconds TICK_RATE(1000 / TICKS_PER_SECOND); // 50ms

    Log::Info("Game loop started ({} ticks/sec)", TICKS_PER_SECOND);

    // Measuring Tick Lag
    double total_ms = 0;
    int tick_count = 0;

    while (server_running_) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Process queued actions from network thread
        ProcessActionQueue();

        // 2. Process game tick (movement, broadcasting, etc.)
        ProcessTick();

        // 3. Send pings periodically
        if (++ping_tick_counter_ >= PING_INTERVAL_TICKS) {
            ping_tick_counter_ = 0;
            SendPingToAllClients();
        }

        // 4. Update bandwidth monitor
        BandwidthMonitor::Instance().Tick();

        // 4. Track performance
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;

        total_ms += ms;
        tick_count++;

        if (tick_count >= 100) {
            // every 5 sec at 20 TPS
            Log::Trace("Avg tick: {:.3f}ms", total_ms / tick_count);
            total_ms = 0;
            tick_count = 0;
        }

        if (ms > 40.0) {
            Log::Warn("Slow tick: {:.3f}ms / {}", ms, TICK_RATE);
        }

        // 5. Sleep until next tick
        if (elapsed < TICK_RATE) {
            std::this_thread::sleep_for(TICK_RATE - elapsed);
        }
    }
    Log::Info("Game Loop Ended.");
}

/// ============================================================================
/// PROCESS TICK - View-Based Broadcasting
/// ============================================================================

void GameServer::ProcessTick() {
    // Get players that changed this tick
    auto dirty_players = players_.ConsumeDirtyPlayers();
    if (dirty_players.empty()) return;

    BroadcastDirtyPlayers(dirty_players);

    // Call Lua hooks
    if (lua_engine_) {
        for (const auto &player: dirty_players) {
            lua_engine_->OnPlayerMoved(
                    player->GetID(),
                    player->GetX(),
                    player->GetY(),
                    player->GetFacing());
        }
    }
}

/*
    // Broadcast updates to all clients
    for (const auto &player: dirty_players) {
        uint64_t player_id = player->GetID();
        int16_t x = player->GetX();
        int16_t y = player->GetY();
        uint8_t facing = player->GetFacing();

        * I want to send a batch update here similar to the commented out code below
         * thing is, anything about the player could have changed. do I send one large batch or
         * movement batch
         * status batch
         * appearance batch
         *
         * and how would I know what batch to send? switch player->is_movement_dirty?
         *
        clients_.BroadcastToAll([=](const auto& conn) {
            Packets::Send::PlayerUpdate(conn, player_id, x, y, facing);
        });
    }



    //.size() returns an int. going from 4 bytes to 1 byte, so we need a static cast
    //const auto count = static_cast<uint8_t>(dirty_players.size());
    Protocol::Packet batch;
    Protocol::PacketWriter::WriteByte(batch.payload,
                                      Protocol::Opcode::Batch::S_RemotePlayer_Update); // should be 20 for client
    // We can fit about 200 player updates in a standard 1400 byte MTU packet.
    // If you have more, you'd loop this, but for now let's assume < 200 moves/tick.
    //Protocol::PacketWriter::WriteByte(batch.payload, count);

    uint8_t processed = 0;
    for (const auto &player: dirty_players) {
        Protocol::PacketWriter::WriteUInt64(batch.payload, player->GetID());
        Protocol::PacketWriter::WriteShort(batch.payload, static_cast<uint16_t>(player->GetX()));
        Protocol::PacketWriter::WriteShort(batch.payload, static_cast<uint16_t>(player->GetY()));
        Protocol::PacketWriter::WriteByte(batch.payload, player->GetFacing());
        processed++;

        // TODO Split into multiple packets
        if (processed == 255) { break; }
    }
    batch.payload[1] = processed; // Set the number of players in the batch

    //13 bytes per player * 255 = 3315, what is the size of uint16?
    // Set the size of the payload
    batch.size = static_cast<uint16_t>(batch.payload.size());
    auto data = std::make_shared<std::vector<uint8_t>>(batch.ToBytes());

    // Let's implement a playerregistry.GetAllPlayersInRange( X ) to grab a
    // List of players our player can see to send them updates. is this how it works?





    * Packet Batch Example:

    // TODO: this takes every moving players data, builds one packet, and sends it to every connected player
    // Extremely unnecessary, just a good example of packet coalescing.
    // Will change it to send to view/etc later

    // --- OPTIMIZATION: PACKET COALESCING ---
    // Instead of sending N small packets, we build ONE big packet.

    uint8_t count = static_cast<uint8_t>(moving_players.size());
    Protocol::Packet batchPacket;
    Protocol::PacketWriter::WriteByte(batchPacket.payload, 0x20);

    // We can fit about 200 player updates in a standard 1400 byte MTU packet.
    // If you have more, you'd loop this, but for now let's assume < 200 moves/tick.
    Protocol::PacketWriter::WriteByte(batchPacket.payload, count);

    for (const auto& p : moving_players) {
        // Append Player ID (4 bytes)
        Protocol::PacketWriter::WriteUInt(batchPacket.payload, p.player_id);
        // Append X, Y (2 bytes each)
        Protocol::PacketWriter::WriteShort(batchPacket.payload, p.x);
        Protocol::PacketWriter::WriteShort(batchPacket.payload, p.y);
        // Append Facing
        Protocol::PacketWriter::WriteByte(batchPacket.payload, p.facing);
    }

    batchPacket.size = static_cast<uint16_t>(batchPacket.payload.size());

    // Serialize ONCE, send to ALL.
    // This is incredibly fast because we don't rebuild the vector for every client.
    auto encoded_bytes = make_shared<std::vector<uint8_t>>(batchPacket.ToBytes());

    for (auto& session : all_receivers) {
        // In a perfect world, you check 'session->GetPlayerID()'
        // to avoid sending a player their own movement, but sending it
        // is actually good for sync correction (anti-cheat).
        session->RawSend(encoded_bytes);
    }
     *
}*/

/// ============================================================================
/// VIEW-BASED BROADCASTING
///
/// For each dirty player, find viewers who can see them using spatial hash.
/// Build one batched packet per viewer containing all visible updates.
/// ============================================================================

void GameServer::BroadcastDirtyPlayers(const std::vector<std::shared_ptr<Player> > &dirty_players) {
    // Map: viewer_id -> list of dirty players they can see
    std::unordered_map<uint64_t, std::vector<std::shared_ptr<Player> > > viewer_updates;

    // For each dirty player, find viewers using spatial hash
    for (const auto &dirty_player: dirty_players) {
        int16_t px = dirty_player->GetX();
        int16_t py = dirty_player->GetY();
        uint64_t dirty_id = dirty_player->GetID();

        // Use World's spatial hash to find nearby players
        // This is O(K) where K = players in nearby cells, NOT O(N)!
        auto nearby_viewers = world_.GetPlayersInRange(px, py);

        for (const auto &viewer: nearby_viewers) {
            // Include self - client uses this for server-authoritative position sync
            viewer_updates[viewer->GetID()].push_back(dirty_player);
        }
    }

    // Send batched packets to each viewer
    for (auto &[viewer_id, updates]: viewer_updates) {
        if (updates.empty()) continue;

        // Get viewer's player and connection
        auto viewer = players_.GetByID(viewer_id);
        if (!viewer) continue;

        auto conn = clients_.GetClientCopy(viewer->GetClientID());
        if (!conn) continue;

        // Build batch packet
        Protocol::Packet batch;
        Protocol::PacketWriter::WriteByte(batch.payload,
                                          Protocol::Opcode::Batch::S_Player_Spatial);
        Protocol::PacketWriter::WriteByte(batch.payload, 0); // Placeholder for count

        uint8_t count = 0;
        for (const auto &player: updates) {
            // Player update: ID (8) + X (2) + Y (2) + Facing (1) = 13 bytes
            Protocol::PacketWriter::WriteUInt64(batch.payload, player->GetID());
            Protocol::PacketWriter::WriteShort(batch.payload, static_cast<uint16_t>(player->GetX()));
            Protocol::PacketWriter::WriteShort(batch.payload, static_cast<uint16_t>(player->GetY()));
            Protocol::PacketWriter::WriteByte(batch.payload, player->GetFacing());
            count++;

            // Safety: max 255 per packet
            if (count == 255) break;
        }

        // Patch in count
        batch.payload[1] = count;
        batch.size = static_cast<uint16_t>(batch.payload.size());

        // Send
        auto data = std::make_shared<std::vector<uint8_t> >(batch.ToBytes());
        conn->RawSend(data);
    }
}

void GameServer::OnClientLogin(const std::shared_ptr<ClientConnection> &client) {
    QueueAction([this, client] {
        // Everything below now runs on game thread


        // Register with client manager
        clients_.AddClient(client);
        const uint64_t client_id = client->GetClientID();

        // Create player in registry
        auto player = players_.CreatePlayer(client_id, 0, 0);

        // Add to world's spatial hash
        world_.AddPlayer(
                player->GetID(),
                player->GetX(),
                player->GetY(),
                player);

        Log::Info("Client {} logged in as player {}", client_id, player->GetID());

        // Send welcome packet to this client
        Packets::PacketSender::Welcome(client, player);

        // Get nearby players for this client (includes self)
        auto nearby_players = world_.GetPlayersInRange(
                player->GetX(),
                player->GetY()
        );

        // Send all nearby players (including self) to new client
        // This syncs client with server's authoritative position
        Packets::PacketSender::BatchPlayerSpatial(client, nearby_players);

        // Broadcast new player to all nearby viewers (single player per viewer)
        for (const auto &viewer : nearby_players) {
            if (viewer->GetID() == player->GetID()) continue;

            auto viewer_conn = clients_.GetClientCopy(viewer->GetClientID());
            if (!viewer_conn) continue;

            Packets::PacketSender::PlayerSpatial(
                    viewer_conn,
                    player->GetID(),
                    player->GetX(),
                    player->GetY(),
                    player->GetFacing()
            );
        }
    });
}

void GameServer::SendPingToAllClients() {
    clients_.BroadcastToAll([](const std::shared_ptr<ClientConnection> &client) {
        client->SendPing();
    });
}

void GameServer::OnClientDisconnect(uint64_t client_id, const std::string &ip) {
    // TODO by the time this runs on main thread, client could of d/c. that's
    // why we get a client id here. check when run on main thread
    QueueAction([this, client_id, ip] {
        auto player = players_.GetByClientID(client_id);
        if (player) {
            // Get player ID before removal
            uint64_t player_id = players_.GetPlayerIDForClient(client_id);
            int16_t px = player->GetX();
            int16_t py = player->GetY();

            // Get nearby viewers BEFORE removing from spatial hash
            auto nearby_viewers = world_.GetPlayersInRange(px, py);

            // Remove from World's spatial hash
            world_.RemovePlayer(player_id);

            // Remove from registry
            players_.RemoveByClientID(client_id);

            // Notify only nearby players
            for (const auto &viewer: nearby_viewers) {
                {
                    if (viewer->GetID() == player_id) continue;

                    auto viewer_conn = clients_.GetClientCopy(viewer->GetClientID());
                    if (!viewer_conn) continue;

                    Packets::PacketSender::PlayerLeft(viewer_conn, player_id);
                }
                Log::Info("Player {} disconnected", player_id);
            }
        }

        // Remove from client manager
        clients_.RemoveClient(client_id);

        // Update rate limiter
        limiter_.RemoveConnection(ip);

        Log::Info("Client {} disconnected", client_id);
    });
}

