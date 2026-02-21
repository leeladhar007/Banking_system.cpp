#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <iomanip>
#include <ctime>
#include <sstream>

using namespace std;

// Database connection details
class DatabaseConfig {
public:
    static const string HOST;
    static const string USER;
    static const string PASSWORD;
    static const string DATABASE;
};

const string DatabaseConfig::HOST = "tcp://127.0.0.1:3306";
const string DatabaseConfig::USER = "root";
const string DatabaseConfig::PASSWORD = "your_password"; // Change this
const string DatabaseConfig::DATABASE = "banking_system";

// Base Account class
class Account {
protected:
    int accountNumber;
    string accountHolder;
    string accountType;
    double balance;
    string phoneNumber;
    string email;
    string address;
    string status;

public:
    Account() : accountNumber(0), balance(0.0) {}
    
    virtual ~Account() {}
    
    // Getters
    int getAccountNumber() const { return accountNumber; }
    string getAccountHolder() const { return accountHolder; }
    double getBalance() const { return balance; }
    
    // Virtual functions for polymorphism
    virtual void displayAccountInfo() {
        cout << "\n=== Account Information ===" << endl;
        cout << "Account Number: " << accountNumber << endl;
        cout << "Account Holder: " << accountHolder << endl;
        cout << "Account Type: " << accountType << endl;
        cout << "Balance: $" << fixed << setprecision(2) << balance << endl;
        cout << "Phone: " << phoneNumber << endl;
        cout << "Email: " << email << endl;
        cout << "Status: " << status << endl;
    }
    
    virtual double calculateInterest() { return 0.0; }
    
    friend class BankingSystem;
};

// Savings Account class (inheritance)
class SavingsAccount : public Account {
private:
    static const double INTEREST_RATE; // 4% annual interest

public:
    SavingsAccount() {
        accountType = "Savings";
    }
    
    double calculateInterest() override {
        return balance * INTEREST_RATE;
    }
    
    void displayAccountInfo() override {
        Account::displayAccountInfo();
        cout << "Annual Interest Rate: " << (INTEREST_RATE * 100) << "%" << endl;
        cout << "Yearly Interest: $" << fixed << setprecision(2) << calculateInterest() << endl;
    }
};

const double SavingsAccount::INTEREST_RATE = 0.04;

// Current Account class (inheritance)
class CurrentAccount : public Account {
private:
    static const double MINIMUM_BALANCE;
    static const double PENALTY_FEE;

public:
    CurrentAccount() {
        accountType = "Current";
    }
    
    double calculateInterest() override {
        return 0.0; // No interest for current accounts
    }
    
    bool checkMinimumBalance() {
        return balance >= MINIMUM_BALANCE;
    }
    
    void displayAccountInfo() override {
        Account::displayAccountInfo();
        cout << "Minimum Balance Required: $" << MINIMUM_BALANCE << endl;
        cout << "Penalty Fee: $" << PENALTY_FEE << endl;
        cout << "Minimum Balance Status: " << (checkMinimumBalance() ? "Maintained" : "Below Minimum") << endl;
    }
};

const double CurrentAccount::MINIMUM_BALANCE = 1000.00;
const double CurrentAccount::PENALTY_FEE = 25.00;

// Transaction class
class Transaction {
private:
    int transactionId;
    int accountNumber;
    string transactionType;
    double amount;
    double balanceAfter;
    string transactionDate;
    string description;

public:
    Transaction(int accNo, string type, double amt, double balAfter, string desc = "") 
        : accountNumber(accNo), transactionType(type), amount(amt), 
          balanceAfter(balAfter), description(desc) {
        
        // Get current timestamp
        time_t now = time(0);
        char* dt = ctime(&now);
        transactionDate = dt;
        transactionDate.pop_back(); // Remove newline
    }
    
    void displayTransaction() {
        cout << "ID: " << transactionId 
             << " | Date: " << transactionDate
             << " | Type: " << transactionType
             << " | Amount: $" << fixed << setprecision(2) << amount
             << " | Balance After: $" << balanceAfter << endl;
        if (!description.empty()) {
            cout << "  Description: " << description << endl;
        }
    }
    
    friend class BankingSystem;
};

// Main Banking System class
class BankingSystem {
private:
    sql::mysql::MySQL_Driver *driver;
    unique_ptr<sql::Connection> conn;
    
    // Database connection
    bool connectToDatabase() {
        try {
            driver = sql::mysql::get_mysql_driver_instance();
            conn = unique_ptr<sql::Connection>(driver->connect(DatabaseConfig::HOST, 
                                                               DatabaseConfig::USER, 
                                                               DatabaseConfig::PASSWORD));
            conn->setSchema(DatabaseConfig::DATABASE);
            return true;
        } catch (sql::SQLException &e) {
            cerr << "Database connection failed: " << e.what() << endl;
            return false;
        }
    }

public:
    BankingSystem() {
        if (!connectToDatabase()) {
            throw runtime_error("Failed to connect to database");
        }
        cout << "Connected to database successfully!" << endl;
    }
    
    ~BankingSystem() {
        if (conn) {
            conn->close();
        }
    }
    
    // Create new account
    bool createAccount() {
        try {
            string name, phone, email, address;
            int type;
            double initialDeposit;
            
            cout << "\n=== Create New Account ===" << endl;
            cout << "Enter Account Holder Name: ";
            cin.ignore();
            getline(cin, name);
            
            cout << "Enter Phone Number: ";
            getline(cin, phone);
            
            cout << "Enter Email: ";
            getline(cin, email);
            
            cout << "Enter Address: ";
            getline(cin, address);
            
            cout << "Select Account Type (1. Savings, 2. Current): ";
            cin >> type;
            
            cout << "Enter Initial Deposit: $";
            cin >> initialDeposit;
            
            if (initialDeposit < 0) {
                cout << "Initial deposit cannot be negative!" << endl;
                return false;
            }
            
            string accType = (type == 1) ? "Savings" : "Current";
            
            // Check minimum balance for current account
            if (type == 2 && initialDeposit < 1000) {
                cout << "Current account requires minimum balance of $1000!" << endl;
                return false;
            }
            
            // Prepare SQL statement
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("INSERT INTO accounts (account_holder, account_type, balance, phone_number, email, address) VALUES (?, ?, ?, ?, ?, ?)")
            );
            
            pstmt->setString(1, name);
            pstmt->setString(2, accType);
            pstmt->setDouble(3, initialDeposit);
            pstmt->setString(4, phone);
            pstmt->setString(5, email);
            pstmt->setString(6, address);
            
            if (pstmt->executeUpdate()) {
                cout << "Account created successfully!" << endl;
                
                // Get the generated account number
                unique_ptr<sql::Statement> stmt(conn->createStatement());
                unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() as id"));
                if (res->next()) {
                    int accNo = res->getInt("id");
                    cout << "Your Account Number is: " << accNo << endl;
                    
                    // Record initial deposit transaction
                    recordTransaction(accNo, "Deposit", initialDeposit, initialDeposit, 
                                    "Initial Deposit");
                }
                return true;
            }
        } catch (sql::SQLException &e) {
            cerr << "Error creating account: " << e.what() << endl;
        }
        return false;
    }
    
    // Deposit money
    bool deposit(int accountNumber, double amount) {
        try {
            if (amount <= 0) {
                cout << "Invalid amount!" << endl;
                return false;
            }
            
            // Check if account exists and get current balance
            double currentBalance = getBalance(accountNumber);
            if (currentBalance == -1) return false;
            
            double newBalance = currentBalance + amount;
            
            // Update balance
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("UPDATE accounts SET balance = ? WHERE account_number = ?")
            );
            pstmt->setDouble(1, newBalance);
            pstmt->setInt(2, accountNumber);
            
            if (pstmt->executeUpdate()) {
                // Record transaction
                recordTransaction(accountNumber, "Deposit", amount, newBalance, 
                                "Cash Deposit");
                cout << "Deposit successful! New balance: $" << fixed << setprecision(2) 
                     << newBalance << endl;
                return true;
            }
        } catch (sql::SQLException &e) {
            cerr << "Error depositing: " << e.what() << endl;
        }
        return false;
    }
    
    // Withdraw money
    bool withdraw(int accountNumber, double amount) {
        try {
            if (amount <= 0) {
                cout << "Invalid amount!" << endl;
                return false;
            }
            
            double currentBalance = getBalance(accountNumber);
            if (currentBalance == -1) return false;
            
            // Check account type for minimum balance
            string accType = getAccountType(accountNumber);
            if (accType == "Current") {
                if (currentBalance - amount < 1000) {
                    cout << "Withdrawal would violate minimum balance requirement!" << endl;
                    return false;
                }
            }
            
            if (currentBalance < amount) {
                cout << "Insufficient balance!" << endl;
                return false;
            }
            
            double newBalance = currentBalance - amount;
            
            // Update balance
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("UPDATE accounts SET balance = ? WHERE account_number = ?")
            );
            pstmt->setDouble(1, newBalance);
            pstmt->setInt(2, accountNumber);
            
            if (pstmt->executeUpdate()) {
                // Record transaction
                recordTransaction(accountNumber, "Withdrawal", amount, newBalance, 
                                "Cash Withdrawal");
                cout << "Withdrawal successful! New balance: $" << fixed << setprecision(2) 
                     << newBalance << endl;
                return true;
            }
        } catch (sql::SQLException &e) {
            cerr << "Error withdrawing: " << e.what() << endl;
        }
        return false;
    }
    
    // Transfer money between accounts
    bool transfer(int fromAccount, int toAccount, double amount) {
        try {
            if (amount <= 0) {
                cout << "Invalid amount!" << endl;
                return false;
            }
            
            if (fromAccount == toAccount) {
                cout << "Cannot transfer to same account!" << endl;
                return false;
            }
            
            // Check if both accounts exist
            double fromBalance = getBalance(fromAccount);
            double toBalance = getBalance(toAccount);
            
            if (fromBalance == -1 || toBalance == -1) {
                cout << "One or both accounts not found!" << endl;
                return false;
            }
            
            // Check minimum balance for source account if it's current
            string fromAccType = getAccountType(fromAccount);
            if (fromAccType == "Current") {
                if (fromBalance - amount < 1000) {
                    cout << "Transfer would violate minimum balance requirement!" << endl;
                    return false;
                }
            }
            
            if (fromBalance < amount) {
                cout << "Insufficient balance in source account!" << endl;
                return false;
            }
            
            // Perform transfer
            double newFromBalance = fromBalance - amount;
            double newToBalance = toBalance + amount;
            
            // Start transaction
            conn->setAutoCommit(false);
            
            try {
                // Update source account
                unique_ptr<sql::PreparedStatement> pstmt1(
                    conn->prepareStatement("UPDATE accounts SET balance = ? WHERE account_number = ?")
                );
                pstmt1->setDouble(1, newFromBalance);
                pstmt1->setInt(2, fromAccount);
                pstmt1->executeUpdate();
                
                // Update destination account
                unique_ptr<sql::PreparedStatement> pstmt2(
                    conn->prepareStatement("UPDATE accounts SET balance = ? WHERE account_number = ?")
                );
                pstmt2->setDouble(1, newToBalance);
                pstmt2->setInt(2, toAccount);
                pstmt2->executeUpdate();
                
                // Record transactions
                string desc = "Transfer to account " + to_string(toAccount);
                recordTransaction(fromAccount, "Transfer", amount, newFromBalance, desc);
                
                desc = "Transfer from account " + to_string(fromAccount);
                recordTransaction(toAccount, "Transfer", amount, newToBalance, desc);
                
                // Commit transaction
                conn->commit();
                conn->setAutoCommit(true);
                
                cout << "Transfer successful!" << endl;
                cout << "New balance in account " << fromAccount << ": $" 
                     << fixed << setprecision(2) << newFromBalance << endl;
                return true;
                
            } catch (sql::SQLException &e) {
                conn->rollback();
                conn->setAutoCommit(true);
                throw;
            }
            
        } catch (sql::SQLException &e) {
            cerr << "Error transferring: " << e.what() << endl;
        }
        return false;
    }
    
    // Display account information
    void displayAccountInfo(int accountNumber) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT * FROM accounts WHERE account_number = ?")
            );
            pstmt->setInt(1, accountNumber);
            
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            if (res->next()) {
                Account* acc = nullptr;
                
                // Create appropriate account type object
                string accType = res->getString("account_type");
                if (accType == "Savings") {
                    acc = new SavingsAccount();
                } else {
                    acc = new CurrentAccount();
                }
                
                // Set account details
                acc->accountNumber = res->getInt("account_number");
                acc->accountHolder = res->getString("account_holder");
                acc->balance = res->getDouble("balance");
                acc->phoneNumber = res->getString("phone_number");
                acc->email = res->getString("email");
                acc->address = res->getString("address");
                acc->status = res->getString("status");
                
                // Polymorphic call
                acc->displayAccountInfo();
                
                delete acc;
            } else {
                cout << "Account not found!" << endl;
            }
        } catch (sql::SQLException &e) {
            cerr << "Error displaying account info: " << e.what() << endl;
        }
    }
    
    // Display transaction history
    void displayTransactionHistory(int accountNumber) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT * FROM transactions WHERE account_number = ? ORDER BY transaction_date DESC LIMIT 10")
            );
            pstmt->setInt(1, accountNumber);
            
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            cout << "\n=== Last 10 Transactions ===" << endl;
            bool found = false;
            
            while (res->next()) {
                found = true;
                Transaction txn(
                    res->getInt("account_number"),
                    res->getString("transaction_type"),
                    res->getDouble("amount"),
                    res->getDouble("balance_after"),
                    res->getString("description")
                );
                txn.transactionId = res->getInt("transaction_id");
                txn.transactionDate = res->getString("transaction_date");
                txn.displayTransaction();
            }
            
            if (!found) {
                cout << "No transactions found for this account." << endl;
            }
            
        } catch (sql::SQLException &e) {
            cerr << "Error displaying transaction history: " << e.what() << endl;
        }
    }
    
    // Get all accounts
    void listAllAccounts() {
        try {
            unique_ptr<sql::Statement> stmt(conn->createStatement());
            unique_ptr<sql::ResultSet> res(stmt->executeQuery(
                "SELECT account_number, account_holder, account_type, balance, status FROM accounts"
            ));
            
            cout << "\n=== All Accounts ===" << endl;
            cout << left << setw(15) << "Account No" 
                 << setw(25) << "Holder Name"
                 << setw(15) << "Type"
                 << setw(15) << "Balance"
                 << "Status" << endl;
            cout << string(70, '-') << endl;
            
            while (res->next()) {
                cout << left << setw(15) << res->getInt("account_number")
                     << setw(25) << res->getString("account_holder")
                     << setw(15) << res->getString("account_type")
                     << "$" << setw(14) << fixed << setprecision(2) << res->getDouble("balance")
                     << res->getString("status") << endl;
            }
            
        } catch (sql::SQLException &e) {
            cerr << "Error listing accounts: " << e.what() << endl;
        }
    }
    
    // Calculate and display interest for all savings accounts
    void calculateInterest() {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT * FROM accounts WHERE account_type = 'Savings'")
            );
            
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            cout << "\n=== Interest Calculation for Savings Accounts ===" << endl;
            
            while (res->next()) {
                SavingsAccount sa;
                sa.accountNumber = res->getInt("account_number");
                sa.accountHolder = res->getString("account_holder");
                sa.balance = res->getDouble("balance");
                
                double interest = sa.calculateInterest();
                cout << "Account: " << sa.getAccountNumber() 
                     << " | Holder: " << sa.getAccountHolder()
                     << " | Balance: $" << fixed << setprecision(2) << sa.getBalance()
                     << " | Annual Interest: $" << interest << endl;
            }
            
        } catch (sql::SQLException &e) {
            cerr << "Error calculating interest: " << e.what() << endl;
        }
    }
    
    // Close account
    bool closeAccount(int accountNumber) {
        try {
            // Check if account exists and has zero balance
            double balance = getBalance(accountNumber);
            if (balance == -1) return false;
            
            if (balance > 0) {
                cout << "Cannot close account with positive balance. Please withdraw all funds first." << endl;
                return false;
            }
            
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("UPDATE accounts SET status = 'Closed' WHERE account_number = ?")
            );
            pstmt->setInt(1, accountNumber);
            
            if (pstmt->executeUpdate()) {
                cout << "Account closed successfully!" << endl;
                return true;
            }
            
        } catch (sql::SQLException &e) {
            cerr << "Error closing account: " << e.what() << endl;
        }
        return false;
    }

private:
    // Helper function to get account balance
    double getBalance(int accountNumber) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT balance FROM accounts WHERE account_number = ?")
            );
            pstmt->setInt(1, accountNumber);
            
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            if (res->next()) {
                return res->getDouble("balance");
            }
        } catch (sql::SQLException &e) {
            cerr << "Error getting balance: " << e.what() << endl;
        }
        return -1;
    }
    
    // Helper function to get account type
    string getAccountType(int accountNumber) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT account_type FROM accounts WHERE account_number = ?")
            );
            pstmt->setInt(1, accountNumber);
            
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            if (res->next()) {
                return res->getString("account_type");
            }
        } catch (sql::SQLException &e) {
            cerr << "Error getting account type: " << e.what() << endl;
        }
        return "";
    }
    
    // Record transaction
    void recordTransaction(int accountNumber, string type, double amount, 
                          double balanceAfter, string description) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("INSERT INTO transactions (account_number, transaction_type, amount, balance_after, description) VALUES (?, ?, ?, ?, ?)")
            );
            
            pstmt->setInt(1, accountNumber);
            pstmt->setString(2, type);
            pstmt->setDouble(3, amount);
            pstmt->setDouble(4, balanceAfter);
            pstmt->setString(5, description);
            
            pstmt->executeUpdate();
            
        } catch (sql::SQLException &e) {
            cerr << "Error recording transaction: " << e.what() << endl;
        }
    }
};

// Main function
int main() {
    try {
        BankingSystem bank;
        int choice, accountNo, toAccount;
        double amount;
        
        do {
            cout << "\n=================================" << endl;
            cout << "     BANKING SYSTEM MENU" << endl;
            cout << "=================================" << endl;
            cout << "1. Create New Account" << endl;
            cout << "2. Deposit Money" << endl;
            cout << "3. Withdraw Money" << endl;
            cout << "4. Transfer Money" << endl;
            cout << "5. Check Account Details" << endl;
            cout << "6. View Transaction History" << endl;
            cout << "7. List All Accounts" << endl;
            cout << "8. Calculate Interest (Savings)" << endl;
            cout << "9. Close Account" << endl;
            cout << "0. Exit" << endl;
            cout << "=================================" << endl;
            cout << "Enter your choice: ";
            cin >> choice;
            
            switch(choice) {
                case 1:
                    bank.createAccount();
                    break;
                    
                case 2:
                    cout << "Enter Account Number: ";
                    cin >> accountNo;
                    cout << "Enter Amount to Deposit: $";
                    cin >> amount;
                    bank.deposit(accountNo, amount);
                    break;
                    
                case 3:
                    cout << "Enter Account Number: ";
                    cin >> accountNo;
                    cout << "Enter Amount to Withdraw: $";
                    cin >> amount;
                    bank.withdraw(accountNo, amount);
                    break;
                    
                case 4:
                    cout << "Enter Source Account Number: ";
                    cin >> accountNo;
                    cout << "Enter Destination Account Number: ";
                    cin >> toAccount;
                    cout << "Enter Amount to Transfer: $";
                    cin >> amount;
                    bank.transfer(accountNo, toAccount, amount);
                    break;
                    
                case 5:
                    cout << "Enter Account Number: ";
                    cin >> accountNo;
                    bank.displayAccountInfo(accountNo);
                    break;
                    
                case 6:
                    cout << "Enter Account Number: ";
                    cin >> accountNo;
                    bank.displayTransactionHistory(accountNo);
                    break;
                    
                case 7:
                    bank.listAllAccounts();
                    break;
                    
                case 8:
                    bank.calculateInterest();
                    break;
                    
                case 9:
                    cout << "Enter Account Number to Close: ";
                    cin >> accountNo;
                    bank.closeAccount(accountNo);
                    break;
                    
                case 0:
                    cout << "Thank you for using our Banking System!" << endl;
                    break;
                    
                default:
                    cout << "Invalid choice! Please try again." << endl;
            }
            
        } while(choice != 0);
        
    } catch (exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
