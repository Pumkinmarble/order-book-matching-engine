#include "order_book.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <thread>
#include <vector>

void run_demo() {
    std::cout << "****************************************\n";
    std::cout << "  order book matching engine demo\n";
    std::cout << "****************************************\n\n";

    
    
    OrderBook book("AAPL");
    
    std::cout << "demo 1: initial market \n";
    book.add_order(Side::BUY, OrderType::LIMIT, 150.00, 100);
    book.add_order(Side::BUY, OrderType::LIMIT, 149.50, 200);
    book.add_order(Side::SELL, OrderType::LIMIT, 151.00, 150);
    book.add_order(Side::SELL, OrderType::LIMIT, 151.50, 100);
    book.print_book();
    
    std::cout << "demo 2: market order execution\n";
    book.add_order(Side::BUY, OrderType::MARKET, 0.0, 120);
    book.print_book();
    
    std::cout << "demo 3: limit order crossing the spread (agressive order) \n";
    book.add_order(Side::SELL, OrderType::LIMIT, 149.00, 150);
    book.print_book();
    
    std::cout << "\n********** statistics **********\n";
    std::cout << "total orders processed: " << book.get_total_orders() << "\n";
    std::cout << "total trades executed: " << book.get_total_trades() << "\n";
    std::cout << "average latency: " << std::fixed << std::setprecision(2) 
              << book.get_avg_latency_ns() << " ns ("
              << (book.get_avg_latency_ns() / 1000.0) << " μs)\n";
    std::cout << "min latency: " << book.get_min_latency_ns() << " ns\n";
    std::cout << "max latency: " << book.get_max_latency_ns() << " ns\n";
    std::cout << "best bid: $" << book.get_best_bid() << "\n";
    std::cout << "best ask: $" << book.get_best_ask() << "\n";
    std::cout << "spread: $" << book.get_spread() << "\n";
    
    std::cout << "\n********** trade history **********\n";
    const auto& trades = book.get_trades();
    for (const auto& trade : trades) {
        std::cout << "Trade: Buy #" << trade.buy_order_id 
                  << " x Sell #" << trade.sell_order_id
                  << " | Qty: " << trade.quantity 
                  << " @ $" << std::fixed << std::setprecision(2) << trade.price << "\n";
    }
}

void run_benchmark_small() {
    std::cout << "\n****************************************\n";
    std::cout << "             benchmark 1 (10k orders) \n";
    std::cout << "****************************************\n\n";
    
    OrderBook book("ONE");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_dist(99.0, 101.0);
    std::uniform_int_distribution<> qty_dist(10, 100);
    std::uniform_int_distribution<> side_dist(0, 1);
    
    const int NUM_ORDERS = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (side_dist(gen) == 0) ? Side::BUY : Side::SELL;
        double price = std::round(price_dist(gen) * 100.0) / 100.0;
        uint64_t quantity = qty_dist(gen);
        book.add_order(side, OrderType::LIMIT, price, quantity);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "total orders: " << book.get_total_orders() << "\n";
    std::cout << "total trades: " << book.get_total_trades() << "\n";
    std::cout << "total time: " << duration.count() << " ms\n";
    std::cout << "throughput: " << std::fixed << std::setprecision(0) 
              << (NUM_ORDERS * 1000.0 / duration.count()) << " orders/sec\n";
    std::cout << "avg latency: " << std::fixed << std::setprecision(3)
              << (book.get_avg_latency_ns() / 1000.0) << " μs\n";
}

void run_benchmark_large() {
    std::cout << "\n****************************************\n";
    std::cout << "          benchmark 2 (1M orders)\n";
    std::cout << "****************************************\n\n";
    
    OrderBook book("TWO");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_dist(99.0, 101.0);
    std::uniform_int_distribution<> qty_dist(10, 1000);
    std::uniform_int_distribution<> side_dist(0, 1);
    
    const int NUM_ORDERS = 1000000;
    
    std::cout << "processing " << NUM_ORDERS << " orders...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (side_dist(gen) == 0) ? Side::BUY : Side::SELL;
        double price = std::round(price_dist(gen) * 100.0) / 100.0;
        uint64_t quantity = qty_dist(gen);
        book.add_order(side, OrderType::LIMIT, price, quantity);
        
        if ((i + 1) % 100000 == 0) {
            std::cout << "progress: " << (i + 1) / 1000 << "K orders processed\r" << std::flush;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "\n\n********** benchmark results **********\n";
    std::cout << "total orders: " << book.get_total_orders() << "\n";
    std::cout << "total trades: " << book.get_total_trades() << "\n";
    std::cout << "total time: " << duration.count() << " ms\n";
    std::cout << "throughput: " << std::fixed << std::setprecision(0) 
              << (NUM_ORDERS * 1000.0 / duration.count()) << " orders/sec\n";
    std::cout << "\n*** latency statistics ***\n";
    std::cout << "average latency: " << std::fixed << std::setprecision(3)
              << (book.get_avg_latency_ns() / 1000.0) << " μs ("
              << book.get_avg_latency_ns() << " ns)\n";
    std::cout << "min Latency: " << book.get_min_latency_ns() << " ns\n";
    std::cout << "max Latency: " << book.get_max_latency_ns() << " ns\n";
}

int main() {
    std::cout << "\n* ORDER BOOK MATCHING ENGINE * :)\n\n";
    
    run_demo();
    run_benchmark_small();
    run_benchmark_large();
    
    std::cout << "\n****************************************\n";
    std::cout << "  benchmark complete!\n";
    std::cout << "****************************************\n";

    return 0;
}
