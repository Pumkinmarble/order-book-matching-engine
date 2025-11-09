# Low-Latency Order Matching Engine

A C++17 order book implementation with price-time priority matching, custom memory pooling, and sub-microsecond latency for high-frequency trading applications.

## Quick Start

### Build
```bash
make
```


### Run Demo & Benchmarks
```bash
make run
# or
./order_book
```

### Clean Build
```bash
make clean
```

## Requirements

- C++17 or later
- GCC/Clang with `-O3` optimization recommended

## What It Does

Simulates a real exchange order book:
1. Maintains separate bid/ask price levels
2. Matches incoming orders against resting liquidity
3. Executes trades at best available prices
4. Tracks order status (NEW → PARTIAL_FILL → FILLED)
5. Records complete trade history

## Example Output:
<img width="416" height="792" alt="image" src="https://github.com/user-attachments/assets/e1084302-f247-4b53-bb31-7b38a36599ed" />
<img width="446" height="441" alt="image" src="https://github.com/user-attachments/assets/1936eaef-14f4-4909-a3fe-e0f00fa866f2" />

