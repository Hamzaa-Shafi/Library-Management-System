// =====================================================================
//  LibraryManagementSystem.cpp
//  Console-based C++ Library Management System
// =====================================================================

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <limits>

using namespace std;

// ---------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------
const int    MAX_BOOKS = 500;   // fixed capacity - no vectors
const int    MAX_RECORDS = 2000;  // fixed capacity - no vectors
const int    BORROW_PERIOD_DAYS = 14;    // how many days a book can be kept
const double FINE_PER_DAY = 10.0;  // late fine, per day, per book
const double TORN_FINE_FRACTION = 0.5;   // torn book -> 50% of book price
const double LOST_FINE_FRACTION = 1.0;   // lost/misplaced -> full book price

const string BOOKS_FILE = "books.txt";
const string RECORDS_FILE = "records.txt";

// ---------------------------------------------------------------------
//  Data structures
// ---------------------------------------------------------------------
struct Date {
    int day = 1, month = 1, year = 2026;

    string toString() const {
        ostringstream oss;
        oss << setw(2) << setfill('0') << day << "-"
            << setw(2) << setfill('0') << month << "-"
            << year;
        return oss.str();
    }
};

enum ReturnCondition { GOOD = 0, TORN = 1, LOST = 2 };

struct Book {
    int    id = 0;
    string title;
    string author;
    int    totalCopies = 0;
    int    availableCopies = 0;
    double price = 0.0;
};

struct IssueRecord {
    int    id = 0;
    int    bookId = 0;
    string borrowerName;
    string borrowerId;
    Date   issueDate;
    Date   dueDate;
    bool   returned = false;
    Date   returnDate;
    double fine = 0.0;
    ReturnCondition condition = GOOD;
};

// ---------------------------------------------------------------------
//  Use time date according to system
// ---------------------------------------------------------------------
tm safeLocalTime(time_t t) {
    tm result{};
#if defined(_WIN32)
    localtime_s(&result, &t);
#else
    localtime_r(&t, &result);
#endif
    return result;
}

// ---------------------------------------------------------------------
//  Date helpers
// ---------------------------------------------------------------------
Date getCurrentDate() {
    time_t now = time(nullptr);
    tm lt = safeLocalTime(now);
    Date d;
    d.day = lt.tm_mday;
    d.month = lt.tm_mon + 1;
    d.year = lt.tm_year + 1900;
    return d;
}

Date addDays(Date d, int days) {
    tm t{};
    t.tm_mday = d.day;
    t.tm_mon = d.month - 1;
    t.tm_year = d.year - 1900;
    t.tm_hour = 12; // noon, to dodge DST edge cases
    time_t asTime = mktime(&t);
    asTime += static_cast<time_t>(days) * 24 * 60 * 60;
    tm result = safeLocalTime(asTime);

    Date out;
    out.day = result.tm_mday;
    out.month = result.tm_mon + 1;
    out.year = result.tm_year + 1900;
    return out;
}

// returns (b - a) in whole days
long daysBetween(const Date& a, const Date& b) {
    tm ta{}; ta.tm_mday = a.day; ta.tm_mon = a.month - 1; ta.tm_year = a.year - 1900; ta.tm_hour = 12;
    tm tb{}; tb.tm_mday = b.day; tb.tm_mon = b.month - 1; tb.tm_year = b.year - 1900; tb.tm_hour = 12;
    time_t ta_t = mktime(&ta);
    time_t tb_t = mktime(&tb);
    double diffSeconds = difftime(tb_t, ta_t);
    return static_cast<long>(diffSeconds / (60 * 60 * 24) + 0.5);
}

// Parses "DD-MM-YYYY" into a Date.
Date parseDate(const string& s) {
    Date d;
    size_t p1 = s.find('-');
    size_t p2 = (p1 == string::npos) ? string::npos : s.find('-', p1 + 1);
    if (p1 == string::npos || p2 == string::npos) return d; // malformed -> default

    d.day = stoi(s.substr(0, p1));
    d.month = stoi(s.substr(p1 + 1, p2 - p1 - 1));
    d.year = stoi(s.substr(p2 + 1));
    return d;
}

// ---------------------------------------------------------------------
//  Library class - fixed-size arrays, no STL containers
// ---------------------------------------------------------------------
class Library {
private:
    Book books[MAX_BOOKS];
    int  bookCount = 0;

    IssueRecord records[MAX_RECORDS];
    int  recordCount = 0;

    int nextBookId = 1;
    int nextRecordId = 1;

public:
    void load() {
        loadBooks();
        loadRecords();
    }

    void saveAll() {
        saveBooks();
        saveRecords();
    }

    // -------------------- File I/O --------------------
    void loadBooks() {
        ifstream in(BOOKS_FILE);
        if (!in.is_open()) return;
        string line;
        while (getline(in, line) && bookCount < MAX_BOOKS) {
            if (line.empty()) continue;
            stringstream ss(line);
            string field;
            Book b;

            getline(ss, field, '|');
            b.id = stoi(field);
            getline(ss, field, '|');
            b.title = field;
            getline(ss, field, '|'); 
            b.author = field;
            getline(ss, field, '|'); 
            b.totalCopies = stoi(field);
            getline(ss, field, '|');
            b.availableCopies = stoi(field);
            getline(ss, field, '|'); 
            b.price = stod(field);

            books[bookCount++] = b;
            if (b.id >= nextBookId) nextBookId = b.id + 1;
        }
    }

    void saveBooks() {
        ofstream out(BOOKS_FILE, ios::trunc);
        for (int i = 0; i < bookCount; i++) {
            const Book& b = books[i];
            out << b.id << '|' << b.title << '|' << b.author << '|'
                << b.totalCopies << '|' << b.availableCopies << '|'
                << b.price << '\n';
        }
    }

    void loadRecords() {
        ifstream in(RECORDS_FILE);
        if (!in.is_open()) return;
        string line;
        while (getline(in, line) && recordCount < MAX_RECORDS) {
            if (line.empty()) continue;
            stringstream ss(line);
            string field;
            IssueRecord r;

            getline(ss, field, '|'); r.id = stoi(field);
            getline(ss, field, '|'); r.bookId = stoi(field);
            getline(ss, field, '|'); r.borrowerName = field;
            getline(ss, field, '|'); r.borrowerId = field;
            getline(ss, field, '|'); r.issueDate = parseDate(field);
            getline(ss, field, '|'); r.dueDate = parseDate(field);
            getline(ss, field, '|'); r.returned = (field == "1");
            getline(ss, field, '|'); r.returnDate = parseDate(field);
            getline(ss, field, '|'); r.fine = stod(field);
            getline(ss, field, '|'); r.condition = static_cast<ReturnCondition>(stoi(field));

            records[recordCount++] = r;
            if (r.id >= nextRecordId) nextRecordId = r.id + 1;
        }
    }

    void saveRecords() {
        ofstream out(RECORDS_FILE, ios::trunc);
        for (int i = 0; i < recordCount; i++) {
            const IssueRecord& r = records[i];
            out << r.id << '|' << r.bookId << '|' << r.borrowerName << '|' << r.borrowerId << '|'
                << r.issueDate.toString() << '|' << r.dueDate.toString() << '|'
                << (r.returned ? "1" : "0") << '|' << r.returnDate.toString() << '|'
                << r.fine << '|' << static_cast<int>(r.condition) << '\n';
        }
    }

    // -------------------- Book management --------------------
    void addBook() {
        if (bookCount >= MAX_BOOKS) { cout << "\n[ERROR] Book capacity reached.\n\n"; return; }

        Book b;
        b.id = nextBookId++;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << "Title: ";
        getline(cin, b.title);
        cout << "Author: ";
        getline(cin, b.author);
        cout << "Number of copies: ";
        cin >> b.totalCopies;
        cout << "Price per copy: ";
        cin >> b.price;

        b.availableCopies = b.totalCopies;
        books[bookCount++] = b;
        saveBooks();
        cout << "\n[OK] Book added with ID " << b.id << "\n\n";
    }

    void displayBooks() const {
        if (bookCount == 0) { cout << "\nNo books in the library yet.\n\n"; return; }

        cout << "\n" << left
            << setw(5) << "ID" << setw(30) << "Title" << setw(20) << "Author"
            << setw(8) << "Total" << setw(10) << "Available" << setw(10) << "Price" << "\n";
        cout << string(83, '-') << "\n";
        for (int i = 0; i < bookCount; i++) {
            const Book& b = books[i];
            cout << left
                << setw(5) << b.id << setw(30) << b.title << setw(20) << b.author
                << setw(8) << b.totalCopies << setw(10) << b.availableCopies
                << setw(10) << b.price << "\n";
        }
        cout << "\n";
    }

    void searchBook() const {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Enter title or author keyword: ";
        string keyword;
        getline(cin, keyword);

        bool found = false;
        for (int i = 0; i < bookCount; i++) {
            const Book& b = books[i];
            if (b.title.find(keyword) != string::npos || b.author.find(keyword) != string::npos) {
                if (!found) {
                    cout << "\n" << left << setw(5) << "ID" << setw(30) << "Title"
                        << setw(20) << "Author" << setw(10) << "Available" << "\n";
                    cout << string(65, '-') << "\n";
                    found = true;
                }
                cout << left << setw(5) << b.id << setw(30) << b.title
                    << setw(20) << b.author << setw(10) << b.availableCopies << "\n";
            }
        }
        if (!found)
            cout << "\nNo matching books found.\n";
        cout << "\n";
    }

    Book* findBookById(int id) {
        for (int i = 0; i < bookCount; i++)
            if (books[i].id == id)
                return &books[i];
        return nullptr;
    }

    // -------------------- Issue / return --------------------
    void issueBook() {
        displayBooks();
        if (bookCount == 0) return;
        if (recordCount >= MAX_RECORDS) { 
            cout << "\n[ERROR] Record capacity reached.\n\n"; return;
        }

        int bookId;
        cout << "Enter Book ID to issue: ";
        cin >> bookId;

        Book* b = findBookById(bookId);
        if (!b) {
            cout << "\n[ERROR] No book with that ID.\n\n"; return;
        }
        if (b->availableCopies <= 0) { 
            cout << "\n[ERROR] No copies currently available.\n\n"; return; 
        }

        IssueRecord r;
        r.id = nextRecordId++;
        r.bookId = bookId;

        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Borrower name: ";
        getline(cin, r.borrowerName);
        cout << "Borrower ID / roll number: ";
        getline(cin, r.borrowerId);

        r.issueDate = getCurrentDate();
        r.dueDate = addDays(r.issueDate, BORROW_PERIOD_DAYS);
        r.returned = false;

        b->availableCopies--;
        records[recordCount++] = r;
        saveBooks();
        saveRecords();

        cout << "\n[OK] Issued \"" << b->title << "\" to " << r.borrowerName
            << ". Record ID: " << r.id
            << ". Due back by " << r.dueDate.toString() << ".\n\n";
    }

    void viewIssuedBooks() const {
        bool any = false;
        cout << "\n" << left
            << setw(6) << "Rec#" << setw(20) << "Borrower" << setw(20) << "Book"
            << setw(14) << "Issue Date" << setw(14) << "Due Date" << setw(10) << "Status" << "\n";
        cout << string(84, '-') << "\n";

        Date today = getCurrentDate();
        for (int i = 0; i < recordCount; i++) {
            const IssueRecord& r = records[i];
            if (r.returned) continue;
            any = true;
            string bookTitle = "(unknown)";
            for (int j = 0; j < bookCount; j++)
                if (books[j].id == r.bookId) bookTitle = books[j].title;

            bool overdue = daysBetween(r.dueDate, today) > 0;
            cout << left
                << setw(6) << r.id << setw(20) << r.borrowerName << setw(20) << bookTitle
                << setw(14) << r.issueDate.toString() << setw(14) << r.dueDate.toString()
                << setw(10) << (overdue ? "OVERDUE" : "OK") << "\n";
        }
        if (!any) cout << "No books are currently issued.\n";
        cout << "\n";
    }

    void returnBook() {
        viewIssuedBooks();

        int recId;
        cout << "Enter Record # to return: ";
        while (!(cin >> recId)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Please enter a numeric record #: ";
        }

        IssueRecord* rec = nullptr;
        for (int i = 0; i < recordCount; i++) {
            if (records[i].id == recId && !records[i].returned) { rec = &records[i]; break; }
        }

        if (!rec) { cout << "\n[ERROR] No matching active issue record.\n\n"; return; }

        Book* b = findBookById(rec->bookId);

        cout << "Book condition on return:\n";
        cout << "  1. Good\n  2. Torn / damaged\n  3. Lost / misplaced\n";

        int choice = 0;
        while (true) {
            cout << "Choice: ";
            if (!(cin >> choice)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "[ERROR] Please enter a number.\n";
                continue;
            }
            if (choice < 1 || choice > 3) {
                cout << "[ERROR] Invalid option - please enter 1, 2, or 3.\n";
                continue;
            }
            break; // valid choice, stop asking
        }

        rec->returnDate = getCurrentDate();
        rec->returned = true;

        // --- late fine ---
        long lateDays = daysBetween(rec->dueDate, rec->returnDate);
        double lateFine = (lateDays > 0) ? lateDays * FINE_PER_DAY : 0.0;

        // --- damage / loss fine ---
        double damageFine = 0.0;
        double bookPrice = b ? b->price : 0.0;

        if (choice == 2) {
            rec->condition = TORN;
            damageFine = bookPrice * TORN_FINE_FRACTION;
        }
        else if (choice == 3) {
            rec->condition = LOST;
            damageFine = bookPrice * LOST_FINE_FRACTION;
        }
        else {
            rec->condition = GOOD;
        }

        rec->fine = lateFine + damageFine;

        // A lost book does not physically come back, so don't restock it.
        // A returned (good/torn) book goes back into the available pool.
        if (b && rec->condition != LOST) {
            b->availableCopies++;
        }

        saveBooks();
        saveRecords();

        cout << "\n----------- Return Summary -----------\n";
        cout << "Book:            " << (b ? b->title : "(unknown)") << "\n";
        cout << "Days late:       " << (lateDays > 0 ? lateDays : 0) << "\n";
        cout << "Late fine:       " << fixed << setprecision(2) << lateFine << "\n";
        cout << "Condition:       " << (choice == 2 ? "Torn" : choice == 3 ? "Lost" : "Good") << "\n";
        cout << "Damage/loss fine:" << fixed << setprecision(2) << damageFine << "\n";
        cout << "TOTAL FINE:      " << fixed << setprecision(2) << rec->fine << "\n";
        cout << "---------------------------------------\n\n";
    }
};

// ---------------------------------------------------------------------
//  Menu / main
// ---------------------------------------------------------------------
void printMenu() {
    cout << "=========================================\n";
    cout << "        LIBRARY MANAGEMENT SYSTEM\n";
    cout << "=========================================\n";
    cout << "1. Add New Book\n";
    cout << "2. Display All Books\n";
    cout << "3. Search Book\n";
    cout << "4. Issue Book\n";
    cout << "5. Return Book (with fine calculation)\n";
    cout << "6. View Currently Issued Books\n";
    cout << "7. Exit\n";
    cout << "-----------------------------------------\n";
    cout << "Choice: ";
}

int main() {
    static Library library;
    library.load();

    int choice;
    while (true) {
        printMenu();
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nInvalid input, please enter a number.\n\n";
            continue;
        }

        switch (choice) {
        case 1: library.addBook(); break;
        case 2: library.displayBooks(); break;
        case 3: library.searchBook(); break;
        case 4: library.issueBook(); break;
        case 5: library.returnBook(); break;
        case 6: library.viewIssuedBooks(); break;
        case 7:
            library.saveAll();
            cout << "\nData saved. Goodbye!\n";
            return 0;
        default:
            cout << "\nInvalid choice, try again.\n\n";
        }
    }
}