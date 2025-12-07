#pragma once

#include "ClientManager.h"
#include "game/PlayerRegistry.h"
#include "game/World.h"
#include "lua/LuaEngine.h"
#include "network/ConnectionLimiter.h"
#include <asio.hpp>
#include <atomic>
#include <thread>

class GameServer {
public:
    explicit GameServer(asio::io_context &io_context);

    ~GameServer();//Destructor
    void Shutdown();

    // Logic
    void OnClientLogin(const std::shared_ptr<ClientConnection> &client);

    //Accessors
    ClientManager &Clients() { return clients_; }

    PlayerRegistry &Players() { return players_; }

    ConnectionLimiter &Limiter() { return limiter_; }

    World &GetWorld() { return world_; }

    asio::io_context &GetIOContext() { return io_context_; }

    bool IsRunning() const { return server_running_.load(); }

    void ReloadScripts() const;

    // Queue an action from any thread (IO thread calls this)
    void QueueAction(std::function<void()> action);


private:
    void StartAccept();

    void GameLogicThread();

    void ProcessTick();

    void ProcessActionQueue();

    // Network
    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    // State
    std::atomic<bool> server_running_{true};
    std::atomic<bool> shutdown_requested_{false};

    // Systems (owned by GameServer)
    ClientManager clients_;
    PlayerRegistry players_;
    World world_;
    ConnectionLimiter limiter_;

    //Main Action Queue
    std::queue<std::function<void()>> action_queue_;
    std::mutex action_mutex_;

    // Threads
    std::thread game_loop_thread_;

    // Lua
    std::shared_ptr<LuaGameEngine> lua_engine_;

    // ID Generation
    /// <summary>
    /// For a game server, uint64_t is the pragmatic choice.
    /// Even at 1 million connections per second, it takes 584,000 years to wrap.
    /// </summary>
    std::atomic<uint64_t> next_client_id_ = 1;
};