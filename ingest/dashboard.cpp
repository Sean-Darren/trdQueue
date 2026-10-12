#include "dashboard.hpp"

#include <iostream>
#include <utility>

namespace {

constexpr int kPort = 8080;
constexpr const char* kHost = "127.0.0.1";

const char* kPage = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>trdQueue</title>
<style>
  :root { color-scheme: light; }
  body {
    margin: 0;
    font-family: system-ui, sans-serif;
    background: #f3f0e8;
    color: #1c1915;
  }
  main { max-width: 40rem; margin: 0 auto; padding: 1.5rem 1rem 2rem; }
  h1 { font-size: 1.25rem; font-weight: 650; margin: 0; }
  #status { margin: 0.35rem 0 1rem; color: #5c564c; }
  .quotes { display: grid; grid-template-columns: 1fr 1fr; gap: 0.75rem; }
  .quote {
    background: #fffdf8;
    border: 1px solid #ddd6c8;
    border-radius: 0.5rem;
    padding: 0.85rem 1rem;
  }
  .quote span { display: block; font-size: 0.8rem; letter-spacing: 0.04em; text-transform: uppercase; color: #5c564c; }
  .quote strong { display: block; margin-top: 0.2rem; font-size: 1.6rem; font-variant-numeric: tabular-nums; }
  .bid strong { color: #0b6b3a; }
  .ask strong { color: #9c2f2f; }
  .quote em { font-style: normal; color: #5c564c; font-variant-numeric: tabular-nums; }
  h2 { font-size: 1rem; margin: 1.4rem 0 0.4rem; }
  form { display: flex; flex-wrap: wrap; gap: 0.6rem; align-items: end; margin: 0.2rem 0 0.8rem; }
  label { display: flex; flex-direction: column; gap: 0.2rem; font-size: 0.8rem; color: #5c564c; }
  input, select, button { font: inherit; color: inherit; background: #fffdf8; border: 1px solid #ddd6c8; border-radius: 0.35rem; padding: 0.4rem 0.55rem; }
  button { cursor: pointer; }
  #local-status { min-height: 1.2rem; margin: 0 0 0.6rem; }
  table { width: 100%; border-collapse: collapse; background: #fffdf8; border: 1px solid #ddd6c8; border-radius: 0.5rem; }
  th, td { text-align: left; padding: 0.45rem 0.7rem; font-variant-numeric: tabular-nums; }
  th { font-size: 0.75rem; letter-spacing: 0.04em; text-transform: uppercase; color: #5c564c; font-weight: 600; }
  tr + tr td { border-top: 1px solid #eee7da; }
  .empty { color: #5c564c; }
  caption { caption-side: bottom; text-align: left; padding: 0.45rem 0.15rem 0; color: #5c564c; font-size: 0.8rem; }
</style>
</head>
<body>
<main>
  <h1 id="product">trdQueue</h1>
  <p id="status">Waiting for snapshot</p>
  <div class="quotes">
    <div class="quote bid"><span>Bid</span><strong id="bid-price">—</strong><em id="bid-size"></em></div>
    <div class="quote ask"><span>Ask</span><strong id="ask-price">—</strong><em id="ask-size"></em></div>
  </div>
  <h2>Recent trades</h2>
  <table>
    <thead><tr><th>Maker</th><th>Price</th><th>Size</th><th>Trade</th></tr></thead>
    <tbody id="trades"></tbody>
    <caption>Side is the resting order on Coinbase.</caption>
  </table>
  <h2>Your orders</h2>
  <p id="local-status">Place a limit order. It matches your other orders on this page.</p>
  <form id="order-form">
    <label>Side
      <select id="order-side">
        <option value="buy">Buy</option>
        <option value="sell">Sell</option>
      </select>
    </label>
    <label>Price <input id="order-price" inputmode="decimal" required></label>
    <label>Size <input id="order-size" inputmode="decimal" required></label>
    <button type="submit">Place limit order</button>
  </form>
  <div class="quotes">
    <div>
      <h2>Resting bids</h2>
      <table>
        <thead><tr><th>Price</th><th>Size</th><th>Id</th><th></th></tr></thead>
        <tbody id="local-bids"></tbody>
      </table>
    </div>
    <div>
      <h2>Resting asks</h2>
      <table>
        <thead><tr><th>Price</th><th>Size</th><th>Id</th><th></th></tr></thead>
        <tbody id="local-asks"></tbody>
      </table>
    </div>
  </div>
  <h2>Your fills</h2>
  <table>
    <thead><tr><th>Maker</th><th>Price</th><th>Size</th><th>Trade</th></tr></thead>
    <tbody id="local-trades"></tbody>
    <caption>A trade happens when your buy and sell prices cross. The price is the resting order's price.</caption>
  </table>
</main>
<script>
function text(id, value) { document.getElementById(id).textContent = value; }
function render(view) {
  text("product", view.product_id || "trdQueue");
  text("status", view.book_live ? "Live" : "Waiting for snapshot");
  text("bid-price", view.bid ? view.bid.price : "—");
  text("bid-size", view.bid ? view.bid.size : "");
  text("ask-price", view.ask ? view.ask.price : "—");
  text("ask-size", view.ask ? view.ask.size : "");
  const body = document.getElementById("trades");
  body.replaceChildren();
  const trades = view.trades || [];
  if (trades.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 4;
    cell.className = "empty";
    cell.textContent = "No trades yet";
    row.appendChild(cell);
    body.appendChild(row);
    return;
  }
  for (const trade of trades) {
    const row = document.createElement("tr");
    for (const value of [trade.side, trade.price, trade.size, String(trade.trade_id)]) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.appendChild(cell);
    }
    body.appendChild(row);
  }
}
function fillOrders(id, rows) {
  const body = document.getElementById(id);
  body.replaceChildren();
  if (!rows || rows.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 4;
    cell.className = "empty";
    cell.textContent = "None";
    row.appendChild(cell);
    body.appendChild(row);
    return;
  }
  for (const order of rows) {
    const row = document.createElement("tr");
    for (const value of [order.price, order.size, String(order.id)]) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.appendChild(cell);
    }
    const action = document.createElement("td");
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = "Cancel";
    button.addEventListener("click", () => postJson("/api/cancel", { order_id: order.id }));
    action.appendChild(button);
    row.appendChild(action);
    body.appendChild(row);
  }
}
function renderLocal(view) {
  if (typeof view.message === "string" && view.message.length > 0) text("local-status", view.message);
  fillOrders("local-bids", view.bids);
  fillOrders("local-asks", view.asks);
  const body = document.getElementById("local-trades");
  body.replaceChildren();
  const trades = view.trades || [];
  if (trades.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 4;
    cell.className = "empty";
    cell.textContent = "No fills yet";
    row.appendChild(cell);
    body.appendChild(row);
    return;
  }
  for (const trade of trades) {
    const row = document.createElement("tr");
    for (const value of [trade.side, trade.price, trade.size, String(trade.trade_id)]) {
      const cell = document.createElement("td");
      cell.textContent = value;
      row.appendChild(cell);
    }
    body.appendChild(row);
  }
}
function postJson(path, payload) {
  return fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  }).then((res) => res.json()).then(renderLocal);
}
document.getElementById("order-form").addEventListener("submit", (event) => {
  event.preventDefault();
  postJson("/api/orders", {
    side: document.getElementById("order-side").value,
    price: document.getElementById("order-price").value.trim(),
    size: document.getElementById("order-size").value.trim()
  }).catch(() => text("local-status", "Could not place the order"));
});
function connect() {
  const ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/");
  ws.onmessage = (event) => {
    try {
      const view = JSON.parse(event.data);
      if (view.channel === "local") renderLocal(view);
      else render(view);
    } catch (err) { text("status", "Bad update"); }
  };
  ws.onclose = () => {
    text("status", "Reconnecting");
    setTimeout(connect, 1000);
  };
}
fetch("/api/book").then((res) => res.json()).then(render).catch(() => {});
fetch("/api/local").then((res) => res.json()).then(renderLocal).catch(() => {});
connect();
</script>
</body>
</html>
)html";

ix::HttpResponsePtr json_response(int status, const std::string& body) {
    ix::WebSocketHttpHeaders headers;
    headers["Content-Type"] = "application/json";
    headers["Cache-Control"] = "no-store";
    const char* description = status == 200 ? "OK" : "Not Found";
    return std::make_shared<ix::HttpResponse>(status, description, ix::HttpErrorCode::Ok, headers,
                                              body);
}

ix::HttpResponsePtr html_response() {
    ix::WebSocketHttpHeaders headers;
    headers["Content-Type"] = "text/html; charset=utf-8";
    headers["Cache-Control"] = "no-store";
    return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, kPage);
}

std::string path_of(const std::string& uri) {
    const auto query = uri.find('?');
    if (query == std::string::npos) {
        return uri;
    }
    return uri.substr(0, query);
}

}  // namespace

Dashboard::Dashboard() : server_(kPort, kHost) {}

Dashboard::~Dashboard() {
    stop();
}

bool Dashboard::start() {
    server_.disablePerMessageDeflate();
    server_.setOnConnectionCallback(
        [this](ix::HttpRequestPtr request,
               std::shared_ptr<ix::ConnectionState>) { return handle_http(std::move(request)); });
    server_.setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& web_socket,
               const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                web_socket.send(snapshot_json());
                if (local_snapshot_) {
                    web_socket.send(local_snapshot_());
                }
            }
        });

    const std::pair<bool, std::string> listened = server_.listen();
    if (!listened.first) {
        std::cerr << "error: dashboard listen failed: " << listened.second << '\n';
        return false;
    }
    server_.start();
    started_ = true;
    std::cerr << "dashboard http://" << kHost << ':' << kPort << '\n';
    return true;
}

void Dashboard::stop() {
    if (!started_) {
        return;
    }
    server_.stop();
    started_ = false;
}

void Dashboard::publish(const trdqueue::BookView& view) {
    std::string payload;
    {
        std::lock_guard<std::mutex> lock(mu_);
        view_ = view;
        payload = trdqueue::book_view_json(view_);
    }
    broadcast(payload);
}

void Dashboard::set_order_handler(std::function<std::string(const std::string&)> handler) {
    order_handler_ = std::move(handler);
}

void Dashboard::set_cancel_handler(std::function<std::string(const std::string&)> handler) {
    cancel_handler_ = std::move(handler);
}

void Dashboard::set_local_snapshot(std::function<std::string()> snapshot) {
    local_snapshot_ = std::move(snapshot);
}

ix::HttpResponsePtr Dashboard::handle_http(ix::HttpRequestPtr request) {
    if (!request) {
        return json_response(404, "{}");
    }
    const std::string path = path_of(request->uri);
    if (request->method == "GET") {
        if (path.empty() || path == "/") {
            return html_response();
        }
        if (path == "/api/book") {
            return json_response(200, snapshot_json());
        }
        if (path == "/api/local" && local_snapshot_) {
            return json_response(200, local_snapshot_());
        }
    }
    if (request->method == "POST" && path == "/api/orders" && order_handler_) {
        const std::string body = order_handler_(request->body);
        if (local_snapshot_) {
            broadcast(local_snapshot_());
        }
        return json_response(200, body);
    }
    if (request->method == "POST" && path == "/api/cancel" && cancel_handler_) {
        const std::string body = cancel_handler_(request->body);
        if (local_snapshot_) {
            broadcast(local_snapshot_());
        }
        return json_response(200, body);
    }
    return json_response(404, "{}");
}

std::string Dashboard::snapshot_json() const {
    std::lock_guard<std::mutex> lock(mu_);
    return trdqueue::book_view_json(view_);
}

void Dashboard::broadcast(const std::string& payload) {
    for (const std::shared_ptr<ix::WebSocket>& client : server_.getClients()) {
        if (client) {
            client->send(payload);
        }
    }
}
