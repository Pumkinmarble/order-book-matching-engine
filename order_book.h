#pragma once

#include <memory>
#include <unordered_map>
#include <map>
#include <deque>
#include <string>
#include <chrono>
#include <vector>
#include <array>
#include <atomic>

enum class OrderType {
    MARKET,
    LIMIT
};

enum class Side {
    BUY,
    SELL
};

enum class OrderStatus {
    NEW,
    PARTIAL_FILL,
    FILLED,
    CANCELLED
};

struct Order {
    uint64_t id;
    std::string symbol;
    Side side;
    OrderType type;
    double price;
    uint64_t quantity;
    uint64_t filled_quantity;
    OrderStatus status;
    std::chrono::nanoseconds timestamp;
    
    Order() = default;
    Order(uint64_t id_, const std::string& symbol_, Side side_, 
          OrderType type_, double price_, uint64_t quantity_)
        : id(id_), symbol(symbol_), side(side_), type(type_), 
          price(price_), quantity(quantity_), filled_quantity(0),
          status(OrderStatus::NEW),
          timestamp(std::chrono::high_resolution_clock::now().time_since_epoch()) {}
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    double price;
    uint64_t quantity;
    std::chrono::nanoseconds timestamp;
};

class OrderPool {
private:
    static constexpr size_t POOL_SIZE = 100000;
    static constexpr size_t CHUNK_SIZE = 1000;
    
    std::vector<std::unique_ptr<std::array<Order, CHUNK_SIZE>>> chunks_;
    std::vector<Order*> free_list_;
    size_t next_chunk_idx_;
    size_t next_order_in_chunk_;
    
public:
    OrderPool();
    Order* allocate();
    void deallocate(Order* order);
    void clear();
};

class PriceLevel {
public:
    double price;
    uint64_t total_volume;
    std::deque<Order*> orders;
    
    PriceLevel(double p) : price(p), total_volume(0) {}
    
    void add_order(Order* order) {
        orders.push_back(order);
        total_volume += (order->quantity - order->filled_quantity);
    }
    
    Order* get_front() {
        return orders.empty() ? nullptr : orders.front();
    }
    
    void update_volume_after_fill(uint64_t filled_qty) {
        if (total_volume >= filled_qty) {
            total_volume -= filled_qty;
        } else {
            total_volume = 0;
        }
    }
    
    void remove_front() {
        if (!orders.empty()) {
            orders.pop_front();
        }
    }
    
    bool is_empty() const {
        return orders.empty();
    }
};

class OrderBook {
private:
    std::string symbol_;
    
    std::map<double, std::unique_ptr<PriceLevel>, std::greater<double>> bids_;
    std::map<double, std::unique_ptr<PriceLevel>> asks_;
    
    std::unordered_map<uint64_t, Order*> orders_;
    
    OrderPool pool_;
    
    std::vector<Trade> trades_;
    
    std::atomic<uint64_t> total_orders_processed_;
    std::atomic<uint64_t> total_trades_;
    std::atomic<uint64_t> total_latency_ns_;
    uint64_t min_latency_ns_;
    uint64_t max_latency_ns_;
    
    std::atomic<uint64_t> order_id_counter_;
    
    void match_order(Order* order);
    void match_market_order(Order* order);
    void match_limit_order(Order* order);
    void execute_trade(Order* buy_order, Order* sell_order, 
                      double price, uint64_t quantity);
    
public:
    OrderBook(const std::string& symbol);
    ~OrderBook();
    
    uint64_t add_order(Side side, OrderType type, double price, uint64_t quantity);
    bool cancel_order(uint64_t order_id);
    Order* get_order(uint64_t order_id);
    
    double get_best_bid() const;
    double get_best_ask() const;
    double get_spread() const;
    uint64_t get_bid_volume(double price) const;
    uint64_t get_ask_volume(double price) const;
    
    const std::vector<Trade>& get_trades() const { return trades_; }
    uint64_t get_total_orders() const { return total_orders_processed_.load(); }
    uint64_t get_total_trades() const { return total_trades_.load(); }
    double get_avg_latency_ns() const;
    uint64_t get_min_latency_ns() const { return min_latency_ns_; }
    uint64_t get_max_latency_ns() const { return max_latency_ns_; }
    
    void print_book(int depth = 5) const;
};
