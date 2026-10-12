# trdQueue

Low-latency market-data pipeline and order-book engine. This repository is a portfolio project: the C++ core is a pure hot path, and everything that talks to the network, disk, or a browser lives outside it.

## Live book

`trdqueue_live` subscribes to the public Coinbase Exchange feed (no API key). It prints the best bid and ask when they change, and serves a page on this machine with that quote and recent trades.

```bash
cmake -S . -B build
cmake --build build
./build/trdqueue_live
./build/trdqueue_live ETH-USD
```

Open http://127.0.0.1:8080. `GET /api/book` returns the Coinbase quote as JSON. The same page accepts your own limit orders and matches them against each other when the prices cross. Ctrl-C stops the feed.