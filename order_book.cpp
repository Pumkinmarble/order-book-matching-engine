#include "order_book.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

OrderPool::OrderPool() : next_chunk_idx_(0), next_order_in_chunk_(0) {
    chunks_.reserve(POOL_SIZE / CHUNK_SIZE);
    free_list_.reserve(POOL_SIZE);
    
    chunks_.push_back(std::make_unique<std::array<Order, CHUNK_SIZE>>());
}

Order* OrderPool::allocate() {
    if (!free_list_.empty()) {
        Order* order = free_list_.back();
        free_list_.pop_back();
        return order;
    }
    
    if (next_order_in_chunk_ >= CHUNK_SIZE) {
        next_chunk_idx_++;
        next_order_in_chunk_ = 0;
        
        if (next_chunk_idx_ >= chunks_.size()) {
            chunks_.push_back(std::make_unique<std::array<Order, CHUNK_SIZE>>());
        }
    }
    
    return &(*chunks_[next_chunk_idx_])[next_order_in_chunk_++];
}

void OrderPool::deallocate(Order* order) {
    free_list_.push_back(order);
}

void OrderPool::clear() {
    free_list_.clear();
    next_chunk_idx_ = 0;
    next_order_in_chunk_ = 0;
}

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol), total_orders_processed_(0), total_trades_(0), 
      total_latency_ns_(0), min_latency_ns_(UINT64_MAX), 
      max_latency_ns_(0), order_id_counter_(1) {
    trades_.reserve(1000000);
}

OrderBook::~OrderBook() {
    for (auto& [id, order] : orders_) {
        pool_.deallocate(order);
    }
}

uint64_t OrderBook::add_order(Side side, OrderType type, double price, uint64_t quantity) {
    auto start = std::chrono::high_resolution_clock::now();
    
    uint64_t order_id = order_id_counter_.fetch_add(1);
    
    Order* order = pool_.allocate();
    new (order) Order(order_id, symbol_, side, type, price, quantity);
    
    orders_[order_id] = order;
    
    match_order(order);
    
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    total_latency_ns_.fetch_add(latency_ns);
    total_orders_processed_.fetch_add(1);
    
    if (latency_ns < min_latency_ns_) min_latency_ns_ = latency_ns;
    if (latency_ns > max_latency_ns_) max_latency_ns_ = latency_ns;
    
    return order_id;
}

void OrderBook::match_order(Order* order) {
    if (order->type == OrderType::MARKET) {
        match_market_order(order);
    } else {
        match_limit_order(order);
    }
}

void OrderBook::match_market_order(Order* order) {
    if (order->side == Side::BUY) {
        while (!asks_.empty() && order->filled_quantity < order->quantity) {
            auto it = asks_.begin();
            double price = it->first;
            PriceLevel* level = it->second.get();
            
            while (!level->is_empty() && order->filled_quantity < order->quantity) {
                Order* opposing_order = level->get_front();
                uint64_t trade_qty = std::min(
                    order->quantity - order->filled_quantity,
                    opposing_order->quantity - opposing_order->filled_quantity
                );
                
                execute_trade(order, opposing_order, price, trade_qty);
                
                level->update_volume_after_fill(trade_qty);
                
                if (opposing_order->filled_quantity == opposing_order->quantity) {
                    level->remove_front();
                    orders_.erase(opposing_order->id);
                    pool_.deallocate(opposing_order);
                }
            }
            
            if (level->is_empty()) {
                asks_.erase(it);
            }
        }
    } else {
        while (!bids_.empty() && order->filled_quantity < order->quantity) {
            auto it = bids_.begin();
            double price = it->first;
            PriceLevel* level = it->second.get();
            
            while (!level->is_empty() && order->filled_quantity < order->quantity) {
                Order* opposing_order = level->get_front();
                uint64_t trade_qty = std::min(
                    order->quantity - order->filled_quantity,
                    opposing_order->quantity - opposing_order->filled_quantity
                );
                
                execute_trade(opposing_order, order, price, trade_qty);
                
                level->update_volume_after_fill(trade_qty);
                
                if (opposing_order->filled_quantity == opposing_order->quantity) {
                    level->remove_front();
                    orders_.erase(opposing_order->id);
                    pool_.deallocate(opposing_order);
                }
            }
            
            if (level->is_empty()) {
                bids_.erase(it);
            }
        }
    }
    
    if (order->filled_quantity == order->quantity) {
        order->status = OrderStatus::FILLED;
        orders_.erase(order->id);
        pool_.deallocate(order);
    } else if (order->filled_quantity > 0) {
        order->status = OrderStatus::PARTIAL_FILL;
    }
}

void OrderBook::match_limit_order(Order* order) {
    if (order->side == Side::BUY) {
        while (!asks_.empty() && order->filled_quantity < order->quantity) {
            auto it = asks_.begin();
            double price = it->first;
            PriceLevel* level = it->second.get();
            
            if (order->price < price) break;
            
            while (!level->is_empty() && order->filled_quantity < order->quantity) {
                Order* opposing_order = level->get_front();
                uint64_t trade_qty = std::min(
                    order->quantity - order->filled_quantity,
                    opposing_order->quantity - opposing_order->filled_quantity
                );
                
                execute_trade(order, opposing_order, price, trade_qty);
                
                level->update_volume_after_fill(trade_qty);
                
                if (opposing_order->filled_quantity == opposing_order->quantity) {
                    level->remove_front();
                    orders_.erase(opposing_order->id);
                    pool_.deallocate(opposing_order);
                }
            }
            
            if (level->is_empty()) {
                asks_.erase(it);
            }
        }
    } else {
        while (!bids_.empty() && order->filled_quantity < order->quantity) {
            auto it = bids_.begin();
            double price = it->first;
            PriceLevel* level = it->second.get();
            
            if (order->price > price) break;
            
            while (!level->is_empty() && order->filled_quantity < order->quantity) {
                Order* opposing_order = level->get_front();
                uint64_t trade_qty = std::min(
                    order->quantity - order->filled_quantity,
                    opposing_order->quantity - opposing_order->filled_quantity
                );
                
                execute_trade(opposing_order, order, price, trade_qty);
                
                level->update_volume_after_fill(trade_qty);
                
                if (opposing_order->filled_quantity == opposing_order->quantity) {
                    level->remove_front();
                    orders_.erase(opposing_order->id);
                    pool_.deallocate(opposing_order);
                }
            }
            
            if (level->is_empty()) {
                bids_.erase(it);
            }
        }
    }
    
    if (order->filled_quantity < order->quantity) {
        if (order->side == Side::BUY) {
            auto it = bids_.find(order->price);
            if (it == bids_.end()) {
                bids_[order->price] = std::make_unique<PriceLevel>(order->price);
                it = bids_.find(order->price);
            }
            it->second->add_order(order);
        } else {
            auto it = asks_.find(order->price);
            if (it == asks_.end()) {
                asks_[order->price] = std::make_unique<PriceLevel>(order->price);
                it = asks_.find(order->price);
            }
            it->second->add_order(order);
        }
        
        if (order->filled_quantity > 0) {
            order->status = OrderStatus::PARTIAL_FILL;
        }
    } else {
        order->status = OrderStatus::FILLED;
        orders_.erase(order->id);
        pool_.deallocate(order);
    }
}

void OrderBook::execute_trade(Order* buy_order, Order* sell_order,
                              double price, uint64_t quantity) {
    buy_order->filled_quantity += quantity;
    sell_order->filled_quantity += quantity;
    
    trades_.push_back(Trade{
        buy_order->id,
        sell_order->id,
        price,
        quantity,
        std::chrono::high_resolution_clock::now().time_since_epoch()
    });
    
    total_trades_.fetch_add(1);
    
    if (buy_order->filled_quantity == buy_order->quantity) {
        buy_order->status = OrderStatus::FILLED;
    } else {
        buy_order->status = OrderStatus::PARTIAL_FILL;
    }
    
    if (sell_order->filled_quantity == sell_order->quantity) {
        sell_order->status = OrderStatus::FILLED;
    } else {
        sell_order->status = OrderStatus::PARTIAL_FILL;
    }
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return false;
    
    Order* order = it->second;
    if (order->status == OrderStatus::FILLED) return false;
    
    order->status = OrderStatus::CANCELLED;
    return true;
}

Order* OrderBook::get_order(uint64_t order_id) {
    auto it = orders_.find(order_id);
    return (it != orders_.end()) ? it->second : nullptr;
}

double OrderBook::get_best_bid() const {
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::get_best_ask() const {
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

double OrderBook::get_spread() const {
    if (bids_.empty() || asks_.empty()) return 0.0;
    return get_best_ask() - get_best_bid();
}

uint64_t OrderBook::get_bid_volume(double price) const {
    auto it = bids_.find(price);
    return (it != bids_.end()) ? it->second->total_volume : 0;
}

uint64_t OrderBook::get_ask_volume(double price) const {
    auto it = asks_.find(price);
    return (it != asks_.end()) ? it->second->total_volume : 0;
}

double OrderBook::get_avg_latency_ns() const {
    uint64_t orders = total_orders_processed_.load();
    if (orders == 0) return 0.0;
    return static_cast<double>(total_latency_ns_.load()) / orders;
}

void OrderBook::print_book(int depth) const {
    std::cout << "\n********** order book: " << symbol_ << " **********\n";

    std::cout << std::fixed << std::setprecision(2);
    
    std::vector<std::pair<double, uint64_t>> ask_levels;
    for (const auto& [price, level] : asks_) {
        ask_levels.push_back({price, level->total_volume});
        if (ask_levels.size() >= static_cast<size_t>(depth)) break;
    }
    
    std::reverse(ask_levels.begin(), ask_levels.end());
    for (const auto& [price, volume] : ask_levels) {
        std::cout << "                    " << std::setw(10) << volume 
                  << " @ " << std::setw(8) << price << " (ASK)\n";
    }
    
    std::cout << "                    ----------------\n";
    std::cout << "                    spread: " << get_spread() << "\n";
    std::cout << "                    ----------------\n";
    
    int count = 0;
    for (const auto& [price, level] : bids_) {
        std::cout << "(BID) " << std::setw(8) << price << " @ " 
                  << std::setw(10) << level->total_volume << "\n";
        if (++count >= depth) break;
    }
    
    std::cout << "******************************************\n\n";
}
