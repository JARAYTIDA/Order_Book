#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <future>

using namespace std;

// Mutexes for thread safety
mutex userMutex;
mutex marketMutex;

// Class to manage individual user profiles
class UserProfile {
public:
    string username;
    int balance;
    map<string, int> stocksOwned; // Company name -> Number of stocks owned
    map<string, vector<pair<int, int>>> buyOrders;  // Company name -> List of (price, quantity) buy orders
    map<string, vector<pair<int, int>>> sellOrders; // Company name -> List of (price, quantity) sell orders

    UserProfile() : username(""), balance(0) {}  // Default constructor

    UserProfile(string uname) : username(uname), balance(100000) {}  // Constructor with username

    // Function to display user information
    void displayProfile() {
        lock_guard<mutex> lock(userMutex);
        cout << "User: " << username << endl;
        cout << "Balance: " << balance << endl;
        cout << "Stocks Owned:" << endl;
        for (const auto& stock : stocksOwned) {
            cout << "  " << stock.first << ": " << stock.second << " shares" << endl;
        }
        cout << "Buy Orders:" << endl;
        for (const auto& order : buyOrders) {
            cout << "  " << order.first << ":" << endl;
            for (const auto& p : order.second) {
                cout << "    Price: " << p.first << ", Quantity: " << p.second << endl;
            }
        }
        cout << "Sell Orders:" << endl;
        for (const auto& order : sellOrders) {
            cout << "  " << order.first << ":" << endl;
            for (const auto& p : order.second) {
                cout << "    Price: " << p.first << ", Quantity: " << p.second << endl;
            }
        }
        cout << endl;
    }

    // Function to add buy order
    void addBuyOrder(const string& stockName, int price, int quantity) {
        lock_guard<mutex> lock(userMutex);
        buyOrders[stockName].push_back(make_pair(price, quantity));
    }

    // Function to add sell order
    void addSellOrder(const string& stockName, int price, int quantity) {
        lock_guard<mutex> lock(userMutex);
        sellOrders[stockName].push_back(make_pair(price, quantity));
    }

    // Function to update stocks owned after a trade
    void updateStocksOwned(const string& stockName, int quantity) {
        lock_guard<mutex> lock(userMutex);
        stocksOwned[stockName] += quantity;
        if (stocksOwned[stockName] == 0) {
            stocksOwned.erase(stockName);
        }
    }

    // Function to remove a completed order
    void removeCompletedOrder(map<string, vector<pair<int, int>>>& orders, const string& stockName, int price, int quantity) {
        lock_guard<mutex> lock(userMutex);
        auto& orderList = orders[stockName];
        for (auto it = orderList.begin(); it != orderList.end(); ++it) {
            if (it->first == price && it->second == quantity) {
                orderList.erase(it);
                break;
            }
        }
        if (orderList.empty()) {
            orders.erase(stockName);
        }
    }
};

// Class to manage the order book for a single stock
class OrderBook {
    priority_queue<pair<int, int>> buy;  // max-heap for buy orders (price -> quantity)
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> sell; // min-heap for sell orders
    int ltp; // last traded price

public:
    OrderBook() : ltp(0) {}

    // Function to place a buy order
    void buyOrder(int price, int quantity, UserProfile& user, const string& stockName) {
        user.addBuyOrder(stockName, price, quantity);
        lock_guard<mutex> lock(marketMutex);
        while (quantity > 0 && !sell.empty() && sell.top().first <= price) {
            auto bestSell = sell.top();
            int tradeQuantity = min(quantity, bestSell.second);
            ltp = bestSell.first;
            quantity -= tradeQuantity;
            user.balance -= ltp * tradeQuantity;
            user.updateStocksOwned(stockName, tradeQuantity);
            sell.pop();
            if (bestSell.second > tradeQuantity) {
                sell.push(make_pair(bestSell.first, bestSell.second - tradeQuantity));
            }
            user.removeCompletedOrder(user.sellOrders, stockName, ltp, tradeQuantity);
        }
        if (quantity > 0) {
            buy.push(make_pair(price, quantity));
        }
    }

    // Function to place a sell order
    void sellOrder(int price, int quantity, UserProfile& user, const string& stockName) {
        user.addSellOrder(stockName, price, quantity);
        lock_guard<mutex> lock(marketMutex);
        while (quantity > 0 && !buy.empty() && buy.top().first >= price) {
            auto bestBuy = buy.top();
            int tradeQuantity = min(quantity, bestBuy.second);
            ltp = bestBuy.first;
            quantity -= tradeQuantity;
            user.balance += ltp * tradeQuantity;
            user.updateStocksOwned(stockName, -tradeQuantity);
            buy.pop();
            if (bestBuy.second > tradeQuantity) {
                buy.push(make_pair(bestBuy.first, bestBuy.second - tradeQuantity));
            }
            user.removeCompletedOrder(user.buyOrders, stockName, ltp, tradeQuantity);
        }
        if (quantity > 0) {
            sell.push(make_pair(price, quantity));
        }
    }

    // Function to display the current state of the order book
    void printBook() {
        lock_guard<mutex> lock(marketMutex);
        cout << "************   Buy Orders  *************" << endl;
        cout << "----------------------------------------" << endl;
        cout << "|      Price      |      Quantity      |" << endl;
        cout << "----------------------------------------" << endl;
        priority_queue<pair<int, int>> buyCopy = buy;
        while (!buyCopy.empty()) {
            auto order = buyCopy.top();
            buyCopy.pop();
            cout << "|   " << setw(7) << order.first << "   |   " << setw(9) << order.second << "   |" << endl;
        }
        cout << "----------------------------------------" << endl;

        cout << "****** ******   Sell Orders  *************" << endl;
        cout << "----------------------------------------" << endl;
        cout << "|      Price      |      Quantity      |" << endl;
        cout << "----------------------------------------" << endl;
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> sellCopy = sell;
        while (!sellCopy.empty()) {
            auto order = sellCopy.top();
            sellCopy.pop();
            cout << "|      " << setw(7) << order.first << "      |      " << setw(9) << order.second << "      |" << endl;
        }
        cout << "----------------------------------------" << endl;

        cout << "****** Last Traded Price : " << ltp << " *******" << endl << endl;
    }

    int getLastTradedPrice() const {
        return ltp;
    }
};

// Class to manage all stocks and their respective order books
class StockMarket {
    map<string, OrderBook> stocks;

public:
    // Function to list a new stock in the market
    void listStock(const string& stockName) {
        lock_guard<mutex> lock(marketMutex);
        if (stocks.find(stockName) != stocks.end()) {
            cout << "Stock already listed in the market." << endl;
        } else {
            stocks[stockName] = OrderBook();
            cout << "Stock " << stockName << " listed successfully!" << endl;
        }
    }

    // Function to place a buy order for a specific stock
    void buyOrder(const string& stockName, int price, int quantity, UserProfile& user) {
        if (stocks.find(stockName) == stocks.end()) {
            cout << "Stock not found in the market." << endl;
            return;
        }
        (void)async(launch::async, &OrderBook::buyOrder, &stocks[stockName], price, quantity, ref(user), stockName);
    }

    // Function to place a sell order for a specific stock
    void sellOrder(const string& stockName, int price, int quantity, UserProfile& user) {
        if (stocks.find(stockName) == stocks.end()) {
            cout << "Stock not found in the market." << endl;
            return;
        }
        (void)async(launch::async, &OrderBook::sellOrder, &stocks[stockName], price, quantity, ref(user), stockName);
    }

    // Function to display the order book of a specific stock
    void displayOrderBook(const string& stockName) {
        if (stocks.find(stockName) == stocks.end()) {
            cout << "Stock not found in the market." << endl;
            return;
        }
        async(launch::async, &OrderBook::printBook, &stocks[stockName]).get();
    }

    // Function to display the last traded prices of all stocks
    void displayLastTradedPrices() {
        cout << "****** Last Traded Prices for All Stocks ******" << endl;
        for (const auto& stock : stocks) {
            cout << stock.first << " : " << stock.second.getLastTradedPrice() << endl;
        }
        cout << "************************************************" << endl << endl;
    }
};

// Class to manage user profiles and handle login/signup
class UserManager {
    map<string, UserProfile> users;

public:
    // Function to sign up a new user
    void signUp(string username) {
        lock_guard<mutex> lock(userMutex);
        if (users.find(username) != users.end()) {
            cout << "Username already taken. Please choose another one." << endl;
        } else {
            users[username] = UserProfile(username);
            cout << "User " << username << " created successfully!" << endl;
        }
    }

    // Function to log in an existing user
    UserProfile* login(string username) {
        lock_guard<mutex> lock(userMutex);
        if (users.find(username) != users.end()) {
            return &users[username];
        } else {
            cout << "Username not found. Please sign up first." << endl;
            return nullptr;
        }
    }
};

// Management class for listing stocks in the market
class Management {
public:
    bool login(string username, string password) {
        // In a real system, you would check the password securely
        return (username == "admin" && password == "admin123");
    }

    void listStock(StockMarket& market) {
        string stockName;
        cout << "Enter the name of the stock to list: ";
        cin >> stockName;
        market.listStock(stockName);
    }
};

int main() {
    UserManager userManager;
    StockMarket market;
    Management management;

    while (true) {
        cout << "Welcome to the Trading System!" << endl;
        cout << "1. Sign Up" << endl;
        cout << "2. User Login" << endl;
        cout << "3. Management Login" << endl;
        cout << "4. Exit" << endl;
        int choice;
        cin >> choice;

        if (choice == 1) {
            string username;
            cout << "Enter username: ";
            cin >> username;
            async(launch::async, &UserManager::signUp, &userManager, username).get();
        } else if (choice == 2) {
            string username;
            cout << "Enter username: ";
            cin >> username;
            UserProfile* user = userManager.login(username);
            if (user) {
                while (true) {
                    cout << "Welcome, " << user->username << "!" << endl;
                    async(launch::async, &UserProfile::displayProfile, user).get();
                    cout << "1. Buy" << endl;
                    cout << "2. Sell" << endl;
                    cout << "3. View Order Book" << endl;
                    cout << "4. View Last Traded Prices" << endl;
                    cout << "5. Logout" << endl;
                    int action;
                    cin >> action;

                    if (action == 1) {
                        string stockName;
                        int price, quantity;
                        cout << "Enter the stock name: ";
                        cin >> stockName;
                        cout << "Enter the price at which you want to buy: ";
                        cin >> price;
                        cout << "Enter the quantity you want to buy: ";
                        cin >> quantity;
                        if (user->balance >= price * quantity) {
                            market.buyOrder(stockName, price, quantity, *user);
                        } else {
                            cout << "Insufficient balance!" << endl;
                        }
                    } else if (action == 2) {
                        string stockName;
                        int price, quantity;
                        cout << "Enter the stock name: ";
                        cin >> stockName;
                        cout << "Enter the price at which you want to sell: ";
                        cin >> price;
                        cout << "Enter the quantity you want to sell: ";
                        cin >> quantity;
                        if (user->stocksOwned[stockName] >= quantity) {
                            market.sellOrder(stockName, price, quantity, *user);
                        } else {
                            cout << "Insufficient stocks owned!" << endl;
                        }
                    } else if (action == 3) {
                        string stockName;
                        cout << "Enter the stock name: ";
                        cin >> stockName;
                        market.displayOrderBook(stockName);
                    } else if (action == 4) {
                        async(launch::async, &StockMarket::displayLastTradedPrices, &market).get();
                    } else if (action == 5) {
                        cout << "Logging out..." << endl;
                        break;
                    } else {
                        cout << "Invalid choice! Please try again." << endl;
                    }
                }
            }
        } else if (choice == 3) {
            string username, password;
            cout << "Enter management username: ";
            cin >> username;
            cout << "Enter management password: ";
            cin >> password;
            if (management.login(username, password)) {
                cout << "Management login successful!" << endl;
                management.listStock(market);
            } else {
                cout << "Invalid credentials! Access denied." << endl;
            }
        } else if (choice == 4) {
            cout << "Exiting the system. Goodbye!" << endl;
            break;
        } else {
            cout << "Invalid choice! Please try again." << endl;
        }
    }

    return 0;
}
