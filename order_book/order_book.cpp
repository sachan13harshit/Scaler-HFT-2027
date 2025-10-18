#include <bits/stdc++.h>
using namespace std;

constexpr size_t POOL_SIZE = 1024 * 1024; 

struct MemoryPool {
    char buffer[POOL_SIZE];
    uint64_t offset{0};

    void* getMemory(uint64_t bytesNeeded) {
        if (offset + bytesNeeded > POOL_SIZE) {
            cerr << "MemoryPool exhausted!" << endl;
            return nullptr;
        }
        void* ptr = reinterpret_cast<void*>(&buffer[offset]);
        offset += bytesNeeded;
        return ptr;
    }

    void reset() { offset = 0; }
};

struct Order {
    uint64_t order_id;
    bool is_buy;
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;
};

struct PriceLevel {
    double price;
    uint64_t total_quantity{0};
    vector<Order*> orders;
};

class OrderBook {
public:
    virtual ~OrderBook() = default;

    virtual void add_order(const Order& order) = 0;
    virtual bool cancel_order(uint64_t order_id) = 0;
    virtual bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) = 0;
    virtual void get_snapshot(size_t depth, vector<PriceLevel>& bids, vector<PriceLevel>& asks) const = 0;
    virtual void print_book(size_t depth = 10) const = 0;
};

class OrderBookImpl : public OrderBook {
public:
    void add_order(const Order& order) override {
        Order* newOrder = new (pool.getMemory(sizeof(Order))) Order(order);
        order_lookup[newOrder->order_id] = newOrder;

        if (newOrder->is_buy) {
            auto it = bids.find(newOrder->price);
            if (it == bids.end()) {
                PriceLevel* pl = new (pool.getMemory(sizeof(PriceLevel))) PriceLevel();
                pl->price = newOrder->price;
                pl->orders.push_back(newOrder);
                pl->total_quantity = newOrder->quantity;
                bids[newOrder->price] = pl;
            } else {
                it->second->orders.push_back(newOrder);
                it->second->total_quantity += newOrder->quantity;
            }
        } else {
            auto it = asks.find(newOrder->price);
            if (it == asks.end()) {
                PriceLevel* pl = new (pool.getMemory(sizeof(PriceLevel))) PriceLevel();
                pl->price = newOrder->price;
                pl->orders.push_back(newOrder);
                pl->total_quantity = newOrder->quantity;
                asks[newOrder->price] = pl;
            } else {
                it->second->orders.push_back(newOrder);
                it->second->total_quantity += newOrder->quantity;
            }
        }
    }

    bool cancel_order(uint64_t order_id) override {
        auto it = order_lookup.find(order_id);
        if (it == order_lookup.end()) return false;
        Order* o = it->second;

        if (o->is_buy) {
            auto levelIt = bids.find(o->price);
            if (levelIt == bids.end()) return false;
            auto* pl = levelIt->second;
            pl->orders.erase(remove(pl->orders.begin(), pl->orders.end(), o), pl->orders.end());
            recalc_total(pl);
            if (pl->orders.empty()) bids.erase(levelIt);
        } else {
            auto levelIt = asks.find(o->price);
            if (levelIt == asks.end()) return false;
            auto* pl = levelIt->second;
            pl->orders.erase(remove(pl->orders.begin(), pl->orders.end(), o), pl->orders.end());
            recalc_total(pl);
            if (pl->orders.empty()) asks.erase(levelIt);
        }

        order_lookup.erase(it);
        return true;
    }

    bool amend_order(uint64_t order_id, double new_price, uint64_t new_qty) override {
        auto it = order_lookup.find(order_id);
        if (it == order_lookup.end()) return false;
        Order* o = it->second;

        bool price_changed = (o->price != new_price);
        if (price_changed) {
            bool is_buy = o->is_buy;
            cancel_order(order_id);
            o->price = new_price;
            o->quantity = new_qty;
            o->is_buy = is_buy;
            add_order(*o);
        } else {
            o->quantity = new_qty;
            if (o->is_buy) {
                auto levelIt = bids.find(o->price);
                if (levelIt != bids.end()) recalc_total(levelIt->second);
            } else {
                auto levelIt = asks.find(o->price);
                if (levelIt != asks.end()) recalc_total(levelIt->second);
            }
        }
        return true;
    }

    void get_snapshot(size_t depth, vector<PriceLevel>& bid_snap, vector<PriceLevel>& ask_snap) const override {
        bid_snap.clear();
        ask_snap.clear();

        size_t i = 0;
        for (auto& [price, pl] : bids) {
            if (i++ >= depth) break;
            bid_snap.push_back(*pl);
        }

        i = 0;
        for (auto& [price, pl] : asks) {
            if (i++ >= depth) break;
            ask_snap.push_back(*pl);
        }
    }

    void print_book(size_t depth = 10) const override {
        cout << "\n=== BIDS (Top " << depth << ") ===\n";
        size_t i = 0;
        for (auto& [price, pl] : bids) {
            if (i++ >= depth) break;
            cout << "Price: " << price << ", Qty: " << pl->total_quantity
                 << ", Orders: " << pl->orders.size() << "\n";
        }

        cout << "\n=== ASKS (Top " << depth << ") ===\n";
        i = 0;
        for (auto& [price, pl] : asks) {
            if (i++ >= depth) break;
            cout << "Price: " << price << ", Qty: " << pl->total_quantity
                 << ", Orders: " << pl->orders.size() << "\n";
        }
    }

private:
    void recalc_total(PriceLevel* pl) {
        pl->total_quantity = 0;
        for (auto* o : pl->orders) pl->total_quantity += o->quantity;
    }

    mutable MemoryPool pool;
    map<double, PriceLevel*, greater<double>> bids; 
    map<double, PriceLevel*> asks;                
    unordered_map<uint64_t, Order*> order_lookup;
};

uint64_t now_ns() {
    return chrono::duration_cast<chrono::nanoseconds>(
               chrono::steady_clock::now().time_since_epoch())
        .count();
}

int main() {
    OrderBook* ob = new OrderBookImpl();

    ob->add_order({1, true, 101.0, 10, now_ns()});
    ob->add_order({2, true, 100.0, 5, now_ns()});
    ob->add_order({3, false, 102.0, 20, now_ns()});
    ob->add_order({4, false, 103.0, 8, now_ns()});

    cout << "Initial Order Book:";
    ob->print_book();

    ob->amend_order(1, 101.0, 15);
    ob->cancel_order(2);

    cout << "\nAfter Amend & Cancel:";
    ob->print_book();

    vector<PriceLevel> bids, asks;
    ob->get_snapshot(2, bids, asks);

    cout << "\nSnapshot:\n";
    for (auto& b : bids)
        cout << "Bid -> Price: " << b.price << ", Qty: " << b.total_quantity << "\n";
    for (auto& a : asks)
        cout << "Ask -> Price: " << a.price << ", Qty: " << a.total_quantity << "\n";

    delete ob;
    return 0;
}
