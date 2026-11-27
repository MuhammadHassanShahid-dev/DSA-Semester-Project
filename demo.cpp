// Hotel Management with Room Booking - DEMO
// DSA Lab semester project (P21), C++17 / g++
//
// Structures (built from scratch, no STL containers):
//   Interval tree : booked dates of each room (AVL-balanced, maxEnd)
//   Linked list   : rooms, guest stays, guest bill lines
//   Min-heap      : lowest free room number of each room type
//   AVL tree      : guests ordered by guest ID
//   Hash table    : guest lookup by CNIC
//
// Dates: check-in and check-out are both at 12:00 PM.
// A date is a day number (days since 1970-01-01) and a stay is the
// range [checkIn, checkOut). Bookings are accepted from today (live
// clock) up to one year ahead.
//
// Not in the demo (main project): housekeeping queue, extend, cancel,
// check-out, delete operations, rehashing, discounts, file save/load,
// data generator, benchmarks.

#include <iostream>
#include <string>
#include <cmath>
#include <ctime>
#include <cstdlib>

using namespace std;

// ---------- Hotel settings ----------
const int NUM_TYPES = 4;
const string TYPE_NAMES[NUM_TYPES] = {"Basic", "Business Class",
                                      "Premium Class", "Premium Suite"};
const double TYPE_RATES[NUM_TYPES] = {5000, 8000, 12000, 20000};

const int NUM_SERVICES = 5;
const string SERVICE_NAMES[NUM_SERVICES] = {"Food", "Laundry", "Minibar",
                                            "Room service", "Miscellaneous"};
const double SERVICE_PRICES[NUM_SERVICES] = {1500, 800, 1000, 500, 500};

const int WINDOW_DAYS = 365;               // booking window from today
const int MIN_YEAR = 2000, MAX_YEAR = 2100;
const string CHECK_TIME = "12:00 PM";
const string MONTH_NAMES[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// ---------- Text helpers ----------
string money(double amount) {
    bool negative = amount < 0;
    if (negative) amount = -amount;
    long long cents = llround(amount * 100.0);
    int frac = (int)(cents % 100);
    string s = (negative ? "-" : "") + to_string(cents / 100) + ".";
    if (frac < 10) s += "0";
    return s + to_string(frac);
}

string pad(const string& text, size_t width) {
    string t = text;
    while (t.length() < width) t += " ";
    return t;
}

// ---------- Calendar helpers ----------
bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeapYear(y)) ? 29 : d[m - 1];
}

// Calendar date -> day number
int dateToSerial(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Day number -> calendar date
void serialToDate(int serial, int& y, int& m, int& d) {
    int z = serial + 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    if (m <= 2) y++;
}

string dateString(int serial) {            // e.g. 25 Oct 2026
    int y, m, d;
    serialToDate(serial, y, m, d);
    return (d < 10 ? "0" : "") + to_string(d) + " " + MONTH_NAMES[m - 1] +
           " " + to_string(y);
}

string stamp(int serial) { return dateString(serial) + ", " + CHECK_TIME; }

int today() {                              // live date from the clock
    time_t now = time(nullptr);
    tm* t = localtime(&now);
    return dateToSerial(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

int windowEnd() { return today() + WINDOW_DAYS; }

// Reads DD-MM-YYYY (separators: - / . or space)
bool parseDate(const string& raw, int& serial, string& error) {
    const string format = "Use the format DD-MM-YYYY, e.g. 25-10-2026.";
    int parts[3] = {0, 0, 0};
    int np = 0;
    bool inNumber = false;
    for (size_t i = 0; i < raw.length(); i++) {
        char c = raw[i];
        if (c >= '0' && c <= '9') {
            if (!inNumber) {
                if (np == 3) { error = format; return false; }
                np++;
                inNumber = true;
            }
            parts[np - 1] = parts[np - 1] * 10 + (c - '0');
            if (parts[np - 1] > 100000) { error = format; return false; }
        } else if (c == '-' || c == '/' || c == '.' || c == ' ') {
            inNumber = false;
        } else {
            error = format;
            return false;
        }
    }
    if (np != 3) { error = format; return false; }
    int d = parts[0], m = parts[1], y = parts[2];
    if (y < MIN_YEAR || y > MAX_YEAR) {
        error = "Year must be between " + to_string(MIN_YEAR) + " and " +
                to_string(MAX_YEAR) + ".";
        return false;
    }
    if (m < 1 || m > 12) { error = "Month must be from 1 to 12."; return false; }
    if (d < 1 || d > daysInMonth(y, m)) {
        error = MONTH_NAMES[m - 1] + " " + to_string(y) + " has only " +
                to_string(daysInMonth(y, m)) + " days.";
        return false;
    }
    serial = dateToSerial(y, m, d);
    return true;
}

// "    14 Nov 2026 -> 25 Nov 2026" line used by the summary
string periodText(int from, int to) {
    return "    " + dateString(from) + " -> " + dateString(to) + "\n";
}

// ---------- Input validation ----------
bool isUnicodeDash(const string& s, size_t i) {   // en dash, em dash, minus
    if (i + 2 >= s.length()) return false;
    unsigned char a = s[i], b = s[i + 1], c = s[i + 2];
    return a == 0xE2 && ((b == 0x80 && (c == 0x93 || c == 0x94)) ||
                         (b == 0x88 && c == 0x92));
}

// Keeps digits, skips dashes and spaces, rejects anything else
bool extractDigits(const string& raw, string& digits, const string& label,
                   string& error) {
    digits = "";
    for (size_t i = 0; i < raw.length(); i++) {
        char c = raw[i];
        if (c >= '0' && c <= '9') digits += c;
        else if (c == '-' || c == ' ') continue;
        else if (isUnicodeDash(raw, i)) i += 2;
        else {
            error = label + " can only contain digits and dashes.";
            return false;
        }
    }
    return true;
}

bool normalizeCnic(const string& raw, string& out, string& error) {
    if (!extractDigits(raw, out, "CNIC", error)) return false;
    if (out.length() != 13) {
        error = "CNIC must have exactly 13 digits (you entered " +
                to_string(out.length()) + ").";
        return false;
    }
    return true;
}

// Mobile number: 11 digits starting with 03
bool normalizePhone(const string& raw, string& out, string& error) {
    if (!extractDigits(raw, out, "Phone number", error)) return false;
    if (out.length() != 11) {
        error = "Phone number must have exactly 11 digits (you entered " +
                to_string(out.length()) + ").";
        return false;
    }
    if (out[0] != '0' || out[1] != '3') {
        error = "Phone number must start with 03, e.g. 0300-1234567.";
        return false;
    }
    return true;
}

// ---------- Console input ----------
string readLine(const string& prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) {
        cout << "\nInput closed. Exiting.\n";
        exit(0);
    }
    return s;
}

string readNonEmpty(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (!s.empty()) return s;
        cout << "  Value cannot be empty.\n";
    }
}

int readInt(const string& prompt, int minV, int maxV) {
    while (true) {
        string s = readLine(prompt);
        bool ok = !s.empty() && s.length() <= 9;
        for (size_t i = 0; i < s.length() && ok; i++)
            if (s[i] < '0' || s[i] > '9') ok = false;
        if (ok) {
            int v = atoi(s.c_str());
            if (v >= minV && v <= maxV) return v;
        }
        cout << "  Enter a whole number from " << minV << " to " << maxV << ".\n";
    }
}

bool readYesNo(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        if (s == "y" || s == "Y") return true;
        if (s == "n" || s == "N") return false;
        cout << "  Please type y or n.\n";
    }
}

// ask... functions repeat until valid; an empty line cancels
bool askCnic(const string& prompt, string& cnic) {
    while (true) {
        string raw = readLine(prompt);
        if (raw.empty()) { cout << "  Cancelled.\n"; return false; }
        string error;
        if (normalizeCnic(raw, cnic, error)) return true;
        cout << "  " << error << "\n";
    }
}

bool askPhone(const string& prompt, string& phone) {
    while (true) {
        string raw = readLine(prompt);
        if (raw.empty()) { cout << "  Cancelled.\n"; return false; }
        string error;
        if (normalizePhone(raw, phone, error)) return true;
        cout << "  " << error << "\n";
    }
}

bool askDate(const string& prompt, int& serial) {
    while (true) {
        string raw = readLine(prompt);
        if (raw.empty()) { cout << "  Cancelled.\n"; return false; }
        string error;
        if (parseDate(raw, serial, error)) return true;
        cout << "  " << error << "\n";
    }
}

// Asks check-in date and nights; keeps the stay inside the booking window
bool askStay(int& checkIn, int& nights) {
    int last = windowEnd();
    while (true) {
        if (!askDate("Check-in date (DD-MM-YYYY, " + CHECK_TIME +
                     ", Enter to cancel): ", checkIn)) return false;
        if (checkIn < today()) {
            cout << "  Check-in cannot be before today ("
                 << dateString(today()) << ").\n";
        } else if (checkIn >= last) {
            cout << "  Check-in must be before " << dateString(last) << ".\n";
        } else {
            break;
        }
    }
    int maxNights = last - checkIn;
    nights = readInt("Number of nights (1-" + to_string(maxNights) + "): ",
                     1, maxNights);
    return true;
}

// =====================================================================
// INTERVAL TREE: AVL-balanced BST keyed by checkIn, each node keeps
// maxEnd (largest checkOut in its subtree). Stays are [checkIn, checkOut).
// =====================================================================
class IntervalNode {
public:
    int checkIn, checkOut, maxEnd, bookingID, height;
    IntervalNode *left, *right;
    IntervalNode(int in, int out, int id)
        : checkIn(in), checkOut(out), maxEnd(out), bookingID(id),
          height(1), left(nullptr), right(nullptr) {}
};

class IntervalTree {
private:
    IntervalNode* root;
    int count;

    static int h(IntervalNode* n) { return n ? n->height : 0; }
    static int bigger(int a, int b) { return a > b ? a : b; }

    // Recompute height and maxEnd from the children
    void updateNode(IntervalNode* n) {
        n->height = 1 + bigger(h(n->left), h(n->right));
        int m = n->checkOut;
        if (n->left && n->left->maxEnd > m) m = n->left->maxEnd;
        if (n->right && n->right->maxEnd > m) m = n->right->maxEnd;
        n->maxEnd = m;
    }

    IntervalNode* rotateRight(IntervalNode* y) {
        IntervalNode* x = y->left;
        y->left = x->right;
        x->right = y;
        updateNode(y);                     // lower node first
        updateNode(x);
        return x;
    }

    IntervalNode* rotateLeft(IntervalNode* x) {
        IntervalNode* y = x->right;
        x->right = y->left;
        y->left = x;
        updateNode(x);
        updateNode(y);
        return y;
    }

    IntervalNode* insertNode(IntervalNode* n, int in, int out, int id) {
        if (!n) {
            count++;
            return new IntervalNode(in, out, id);
        }
        if (in < n->checkIn) n->left = insertNode(n->left, in, out, id);
        else                 n->right = insertNode(n->right, in, out, id);

        updateNode(n);
        int balance = h(n->left) - h(n->right);
        if (balance > 1) {
            if (in >= n->left->checkIn) n->left = rotateLeft(n->left);    // LR
            return rotateRight(n);                                        // LL
        }
        if (balance < -1) {
            if (in < n->right->checkIn) n->right = rotateRight(n->right); // RL
            return rotateLeft(n);                                         // RR
        }
        return n;
    }

    void destroy(IntervalNode* n) {        // post-order: children first
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

    void bookedNode(IntervalNode* n, string& text) const {   // in date order
        if (!n) return;
        bookedNode(n->left, text);
        int nights = n->checkOut - n->checkIn;
        text += "    #" + to_string(n->bookingID) + "  " +
                dateString(n->checkIn) + " -> " + dateString(n->checkOut) +
                " (" + to_string(nights) + " night" + (nights == 1 ? "" : "s") +
                ")\n";
        bookedNode(n->right, text);
    }

    // Walk bookings in date order; each gap before a booking is free time
    void gapsNode(IntervalNode* n, int& cursor, int to, string& text) const {
        if (!n) return;
        gapsNode(n->left, cursor, to, text);
        int end = n->checkIn < to ? n->checkIn : to;
        if (end > cursor) text += periodText(cursor, end);
        if (n->checkOut > cursor) cursor = n->checkOut;
        gapsNode(n->right, cursor, to, text);
    }

public:
    IntervalTree() : root(nullptr), count(0) {}
    ~IntervalTree() { destroy(root); }
    IntervalTree(const IntervalTree&) = delete;             // single owner
    IntervalTree& operator=(const IntervalTree&) = delete;

    void insert(int in, int out, int id) { root = insertNode(root, in, out, id); }

    // Any booking overlapping [in, out), or nullptr. One path: O(log n)
    IntervalNode* findOverlap(int in, int out) const {
        IntervalNode* cur = root;
        while (cur) {
            if (in < cur->checkOut && cur->checkIn < out) return cur;
            if (cur->left && cur->left->maxEnd > in) cur = cur->left;
            else cur = cur->right;
        }
        return nullptr;
    }

    bool isFree(int in, int out) const { return findOverlap(in, out) == nullptr; }
    int getCount() const { return count; }

    string bookedText() const {
        string text;
        bookedNode(root, text);
        return text;
    }

    // Free periods between from and to (the gaps between bookings)
    string freePeriods(int from, int to) const {
        string text;
        int cursor = from;
        gapsNode(root, cursor, to, text);
        if (cursor < to) text += periodText(cursor, to);
        return text;
    }
};

// =====================================================================
// ROOMS: singly linked list, every room owns an interval tree
// =====================================================================
class RoomNode {
public:
    int roomNumber;
    string roomType;
    double ratePerNight;
    IntervalTree stays;
    RoomNode* next;
    RoomNode(int num, const string& type, double rate)
        : roomNumber(num), roomType(type), ratePerNight(rate), next(nullptr) {}
};

class RoomList {
private:
    RoomNode* head;
    int count;

public:
    RoomList() : head(nullptr), count(0) {}
    ~RoomList() {
        while (head) {
            RoomNode* temp = head;
            head = head->next;
            delete temp;
        }
    }
    RoomList(const RoomList&) = delete;
    RoomList& operator=(const RoomList&) = delete;

    void addRoom(int num, const string& type, double rate) {
        RoomNode* node = new RoomNode(num, type, rate);
        if (!head) head = node;
        else {
            RoomNode* cur = head;
            while (cur->next) cur = cur->next;
            cur->next = node;
        }
        count++;
    }

    RoomNode* findRoom(int num) const {
        for (RoomNode* cur = head; cur; cur = cur->next)
            if (cur->roomNumber == num) return cur;
        return nullptr;
    }

    int getCount() const { return count; }

    int bookedRoomCount() const {          // rooms with at least 1 booking
        int n = 0;
        for (RoomNode* c = head; c; c = c->next)
            if (c->stays.getCount() > 0) n++;
        return n;
    }

    // Every room: its bookings and its free periods between from and to
    string summaryText(int from, int to) const {
        string text;
        for (RoomNode* c = head; c; c = c->next) {
            text += "\nRoom " + to_string(c->roomNumber) + " | " + c->roomType +
                    " | " + money(c->ratePerNight) + " per night\n";
            string booked = c->stays.bookedText();
            text += "  Booked:\n" +
                    (booked.empty() ? string("    none\n") : booked);
            string free = c->stays.freePeriods(from, to);
            text += "  Free:\n" + (free.empty() ? string("    none\n") : free);
        }
        return text;
    }

    // One line per room: FREE, or the booking it clashes with
    string availabilityText(int in, int out) const {
        string text;
        for (RoomNode* c = head; c; c = c->next) {
            text += "Room " + to_string(c->roomNumber) + " | " +
                    pad(c->roomType, 15) + "| ";
            IntervalNode* hit = c->stays.findOverlap(in, out);
            if (!hit) text += "FREE\n";
            else text += "booked (clashes with booking #" +
                         to_string(hit->bookingID) + ")\n";
        }
        return text;
    }
};

// =====================================================================
// MIN-HEAP: array based, one per room type, stores room numbers only
// =====================================================================
class RoomHeap {
private:
    int* data;
    int size, capacity;

    void grow() {                          // double the array
        int* bigger = new int[capacity * 2];
        for (int i = 0; i < size; i++) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        capacity *= 2;
    }

    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (data[p] <= data[i]) break;
            int t = data[p]; data[p] = data[i]; data[i] = t;
            i = p;
        }
    }

    void siftDown(int i) {
        while (true) {
            int l = 2 * i + 1, r = 2 * i + 2, smallest = i;
            if (l < size && data[l] < data[smallest]) smallest = l;
            if (r < size && data[r] < data[smallest]) smallest = r;
            if (smallest == i) break;
            int t = data[i]; data[i] = data[smallest]; data[smallest] = t;
            i = smallest;
        }
    }

public:
    RoomHeap() : data(new int[8]), size(0), capacity(8) {}
    ~RoomHeap() { delete[] data; }
    RoomHeap(const RoomHeap&) = delete;
    RoomHeap& operator=(const RoomHeap&) = delete;

    bool isEmpty() const { return size == 0; }
    int getSize() const { return size; }
    int peekMin() const { return size ? data[0] : -1; }

    void push(int roomNumber) {
        if (size == capacity) grow();
        data[size] = roomNumber;
        siftUp(size);
        size++;
    }

    int pop() {
        if (size == 0) return -1;
        int top = data[0];
        data[0] = data[--size];
        siftDown(0);
        return top;
    }
};

// =====================================================================
// GUEST: owns a linked list of stays and a linked list of bill lines
// =====================================================================
class ChargeNode {
public:
    string description;
    double amount;
    int day;                               // day number of the charge
    ChargeNode* next;
    ChargeNode(const string& d, double a, int dy)
        : description(d), amount(a), day(dy), next(nullptr) {}
};

class StayNode {
public:
    int bookingID, roomNumber, checkIn, checkOut;
    string status;
    StayNode* next;
    StayNode(int id, int room, int in, int out)
        : bookingID(id), roomNumber(room), checkIn(in), checkOut(out),
          status("Booked"), next(nullptr) {}
};

class Guest {
private:
    int guestID;
    string name, cnic, phone;
    StayNode* stays;
    ChargeNode *billHead, *billTail;

public:
    Guest(int id, const string& n, const string& c, const string& p)
        : guestID(id), name(n), cnic(c), phone(p),
          stays(nullptr), billHead(nullptr), billTail(nullptr) {}

    ~Guest() {
        while (stays) {
            StayNode* t = stays;
            stays = stays->next;
            delete t;
        }
        while (billHead) {
            ChargeNode* t = billHead;
            billHead = billHead->next;
            delete t;
        }
    }
    Guest(const Guest&) = delete;
    Guest& operator=(const Guest&) = delete;

    int getID() const { return guestID; }
    const string& getName() const { return name; }
    bool hasStays() const { return stays != nullptr; }

    // True if the day falls inside one of the guest's booked stays
    bool hasStayOn(int day) const {
        for (StayNode* s = stays; s; s = s->next)
            if (s->status == "Booked" && day >= s->checkIn && day <= s->checkOut)
                return true;
        return false;
    }

    void addStay(int bookingID, int room, int in, int out) {
        StayNode* node = new StayNode(bookingID, room, in, out);
        if (!stays) { stays = node; return; }
        StayNode* cur = stays;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }

    // O(1) append using the tail pointer
    void addCharge(const string& description, double amount, int day) {
        ChargeNode* node = new ChargeNode(description, amount, day);
        if (!billHead) billHead = billTail = node;
        else { billTail->next = node; billTail = node; }
    }

    double getTotalBill() const {
        double total = 0;
        for (ChargeNode* c = billHead; c; c = c->next) total += c->amount;
        return total;
    }

    string info() const {
        return "ID " + to_string(guestID) + " | " + name + " | CNIC " + cnic +
               " | Phone " + phone;
    }

    string staysText() const {
        string text;
        for (StayNode* s = stays; s; s = s->next)
            text += "  Booking #" + to_string(s->bookingID) + ": Room " +
                    to_string(s->roomNumber) + ", " + stamp(s->checkIn) +
                    "  ->  " + stamp(s->checkOut) + ", " + s->status + "\n";
        return text.empty() ? "  (no stays)\n" : text;
    }

    string billStatement() const {
        string text = "Bill for " + name + " (Guest ID " +
                      to_string(guestID) + ")\n";
        for (ChargeNode* c = billHead; c; c = c->next)
            text += "  " + dateString(c->day) + " | " + c->description +
                    " | " + money(c->amount) + "\n";
        return text + "  TOTAL: " + money(getTotalBill()) + "\n";
    }
};

// =====================================================================
// AVL TREE OF GUESTS: keyed by guest ID, owns the Guest objects
// =====================================================================
class AVLNode {
public:
    Guest* data;
    AVLNode *left, *right;
    int height;
    AVLNode(Guest* g) : data(g), left(nullptr), right(nullptr), height(1) {}
};

class GuestAVL {
private:
    AVLNode* root;
    int count;

    static int h(AVLNode* n) { return n ? n->height : 0; }
    static int bigger(int a, int b) { return a > b ? a : b; }
    static void update(AVLNode* n) {
        n->height = 1 + bigger(h(n->left), h(n->right));
    }

    AVLNode* rotateRight(AVLNode* y) {
        AVLNode* x = y->left;
        y->left = x->right;
        x->right = y;
        update(y);
        update(x);
        return x;
    }

    AVLNode* rotateLeft(AVLNode* x) {
        AVLNode* y = x->right;
        x->right = y->left;
        y->left = x;
        update(x);
        update(y);
        return y;
    }

    AVLNode* insertNode(AVLNode* n, Guest* g, bool& ok) {
        if (!n) { count++; return new AVLNode(g); }
        int id = g->getID();
        if (id < n->data->getID()) n->left = insertNode(n->left, g, ok);
        else if (id > n->data->getID()) n->right = insertNode(n->right, g, ok);
        else { ok = false; return n; }

        update(n);
        int balance = h(n->left) - h(n->right);
        if (balance > 1) {                 // left heavy
            if (id > n->left->data->getID())
                n->left = rotateLeft(n->left);        // LR case
            return rotateRight(n);                    // LL case
        }
        if (balance < -1) {                // right heavy
            if (id < n->right->data->getID())
                n->right = rotateRight(n->right);     // RL case
            return rotateLeft(n);                     // RR case
        }
        return n;
    }

    void destroy(AVLNode* n) {             // post-order, also deletes guests
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n->data;
        delete n;
    }

    void inorderNode(AVLNode* n, string& text) const {
        if (!n) return;
        inorderNode(n->left, text);
        text += n->data->info() + "\n";
        inorderNode(n->right, text);
    }

public:
    GuestAVL() : root(nullptr), count(0) {}
    ~GuestAVL() { destroy(root); }
    GuestAVL(const GuestAVL&) = delete;
    GuestAVL& operator=(const GuestAVL&) = delete;

    bool insert(Guest* g) {
        bool ok = true;
        root = insertNode(root, g, ok);
        return ok;
    }

    Guest* search(int id) const {
        AVLNode* cur = root;
        while (cur) {
            if (id == cur->data->getID()) return cur->data;
            cur = (id < cur->data->getID()) ? cur->left : cur->right;
        }
        return nullptr;
    }

    string inorder() const {               // sorted by guest ID
        string text;
        inorderNode(root, text);
        return text;
    }

    int getHeight() const { return h(root); }
    int getCount() const { return count; }
};

// =====================================================================
// HASH TABLE: separate chaining, keyed by 13-digit CNIC
// Points to guests only; the AVL tree owns them.
// =====================================================================
class HashNode {
public:
    string cnic;
    Guest* guest;
    HashNode* next;
    HashNode(const string& c, Guest* g) : cnic(c), guest(g), next(nullptr) {}
};

class CnicHashTable {
private:
    HashNode** buckets;
    int tableSize;

    int hashOf(const string& cnic) const {
        long long hv = 0;
        for (size_t i = 0; i < cnic.length(); i++)
            hv = (hv * 31 + (cnic[i] - '0')) % tableSize;
        return (int)hv;
    }

public:
    CnicHashTable() : tableSize(1009) {
        buckets = new HashNode*[tableSize];
        for (int i = 0; i < tableSize; i++) buckets[i] = nullptr;
    }

    ~CnicHashTable() {                     // frees chain nodes only
        for (int i = 0; i < tableSize; i++) {
            HashNode* cur = buckets[i];
            while (cur) {
                HashNode* t = cur;
                cur = cur->next;
                delete t;
            }
        }
        delete[] buckets;
    }
    CnicHashTable(const CnicHashTable&) = delete;
    CnicHashTable& operator=(const CnicHashTable&) = delete;

    bool insert(const string& cnic, Guest* g) {
        if (find(cnic)) return false;      // duplicate CNIC
        int idx = hashOf(cnic);
        HashNode* node = new HashNode(cnic, g);
        node->next = buckets[idx];
        buckets[idx] = node;
        return true;
    }

    Guest* find(const string& cnic) const {
        for (HashNode* cur = buckets[hashOf(cnic)]; cur; cur = cur->next)
            if (cur->cnic == cnic) return cur->guest;
        return nullptr;
    }
};

// =====================================================================
// HOTEL MANAGER: owns every structure, only class that talks to the user
// =====================================================================
class HotelManager {
private:
    GuestAVL guests;
    CnicHashTable cnicIndex;
    RoomList rooms;
    RoomHeap heaps[NUM_TYPES];             // one heap per room type
    int nextGuestID, nextBookingID;
    bool sampleLoaded;

    // Room price always comes from its type
    void addRoom(int number, int typeIdx) {
        rooms.addRoom(number, TYPE_NAMES[typeIdx], TYPE_RATES[typeIdx]);
        heaps[typeIdx].push(number);
    }

    Guest* registerGuest(const string& name, const string& cnic,
                         const string& phone) {
        Guest* g = new Guest(nextGuestID, name, cnic, phone);
        if (!cnicIndex.insert(cnic, g)) {  // CNIC already used
            delete g;
            return nullptr;
        }
        guests.insert(g);
        nextGuestID++;
        return g;
    }

    // Lowest-numbered free room of the type: pop room numbers from the
    // heap (smallest first) and test each one with its interval tree.
    // Returns the room number, or -1 if rejected.
    int bookRoom(Guest* g, int typeIdx, int checkInDay, int nights) {
        int checkOutDay = checkInDay + nights;
        if (nights < 1 || checkInDay < today() || checkOutDay > windowEnd()) {
            cout << "  Stay must be between " << dateString(today()) << " and "
                 << dateString(windowEnd()) << ".\n";
            return -1;
        }

        RoomHeap& heap = heaps[typeIdx];
        int* parked = new int[heap.getSize()];   // numbers popped so far
        int parkedCount = 0, chosen = -1;
        while (!heap.isEmpty()) {
            int r = heap.pop();
            parked[parkedCount++] = r;
            RoomNode* room = rooms.findRoom(r);
            if (room && room->stays.isFree(checkInDay, checkOutDay)) {
                chosen = r;
                break;
            }
        }
        for (int i = 0; i < parkedCount; i++) heap.push(parked[i]);  // restore
        delete[] parked;

        if (chosen == -1) {
            cout << "  No " << TYPE_NAMES[typeIdx] << " room is free from "
                 << stamp(checkInDay) << " to " << stamp(checkOutDay)
                 << ". Booking rejected.\n";
            return -1;
        }

        RoomNode* room = rooms.findRoom(chosen);
        int id = nextBookingID++;
        double charge = nights * room->ratePerNight;

        room->stays.insert(checkInDay, checkOutDay, id);
        g->addStay(id, chosen, checkInDay, checkOutDay);
        g->addCharge("Room " + to_string(chosen) + ": " + to_string(nights) +
                     " night(s) x " + money(room->ratePerNight),
                     charge, checkInDay);

        cout << "  Booked " << TYPE_NAMES[typeIdx] << " room " << chosen
             << " for " << g->getName() << " (booking #" << id << ")\n"
             << "    Check-in : " << stamp(checkInDay) << "\n"
             << "    Check-out: " << stamp(checkOutDay) << "  (" << nights
             << " night" << (nights == 1 ? "" : "s") << ")\n"
             << "    Charge   : " << money(charge) << "\n";
        return chosen;
    }

    // ---------- menu actions ----------
    void showSummary() const {
        int from = today(), to = windowEnd();
        int total = rooms.getCount(), booked = rooms.bookedRoomCount();
        cout << "\n===== ROOM SUMMARY =====\n"
             << "Window: " << dateString(from) << " -> " << dateString(to)
             << " (all times " << CHECK_TIME << ")\n"
             << "Total rooms: " << total << " | Rooms with bookings: " << booked
             << " | Rooms with no bookings: " << total - booked << "\n"
             << rooms.summaryText(from, to);
    }

    void showGuests() const {
        cout << "\n--- Guests sorted by ID (AVL inorder) ---\n";
        string s = guests.inorder();
        cout << (s.empty() ? "  (no guests yet)\n" : s);
        cout << "Guests: " << guests.getCount() << " | AVL height: "
             << guests.getHeight() << "\n";
    }

    void menuRegister() {
        string name = readNonEmpty("Guest name: ");
        string cnic, phone;
        if (!askCnic("CNIC (13 digits, dashes allowed, Enter to cancel): ", cnic))
            return;
        if (!askPhone("Phone (11 digits, e.g. 0300-1234567, Enter to cancel): ",
                      phone)) return;
        Guest* g = registerGuest(name, cnic, phone);
        if (!g) cout << "  A guest with this CNIC already exists.\n";
        else cout << "  Registered: " << g->info() << "\n";
    }

    void menuBook() {
        string cnic;
        if (!askCnic("Guest CNIC (Enter to cancel): ", cnic)) return;
        Guest* g = cnicIndex.find(cnic);
        if (g) {
            cout << "  Found guest: " << g->getName() << "\n";
        } else {                           // new guest: register first
            cout << "  New guest - please enter details.\n";
            string name = readNonEmpty("Guest name: ");
            string phone;
            if (!askPhone("Phone (11 digits, e.g. 0300-1234567, Enter to cancel): ",
                          phone)) return;
            g = registerGuest(name, cnic, phone);
            cout << "  Registered: " << g->info() << "\n";
        }

        cout << "  Room types:\n";
        for (int i = 0; i < NUM_TYPES; i++)
            cout << "    " << i + 1 << ". " << pad(TYPE_NAMES[i], 15)
                 << money(TYPE_RATES[i]) << " per night\n";
        int t = readInt("Choose room type (1-" + to_string(NUM_TYPES) + "): ",
                        1, NUM_TYPES) - 1;

        int checkIn, nights;
        if (!askStay(checkIn, nights)) return;
        cout << "  Check-out will be: " << stamp(checkIn + nights) << "\n";
        bookRoom(g, t, checkIn, nights);
    }

    void menuCheckRoom() const {
        int num = readInt("Room number: ", 1, 999999);
        RoomNode* room = rooms.findRoom(num);
        if (!room) { cout << "  Room " << num << " does not exist.\n"; return; }
        int checkIn, nights;
        if (!askStay(checkIn, nights)) return;
        int checkOut = checkIn + nights;
        cout << "  Checking " << stamp(checkIn) << "  ->  "
             << stamp(checkOut) << "\n";
        IntervalNode* hit = room->stays.findOverlap(checkIn, checkOut);
        if (!hit) cout << "  Room " << num << " is FREE for this stay.\n";
        else cout << "  Room " << num << " is NOT free. It clashes with booking #"
                  << hit->bookingID << " (" << stamp(hit->checkIn) << "  ->  "
                  << stamp(hit->checkOut) << ").\n";
    }

    void menuFreeRooms() const {
        int checkIn, nights;
        if (!askStay(checkIn, nights)) return;
        int checkOut = checkIn + nights;
        cout << "\n--- Availability: " << stamp(checkIn) << "  ->  "
             << stamp(checkOut) << " ---\n"
             << rooms.availabilityText(checkIn, checkOut)
             << "A booking takes the lowest-numbered FREE room of its type.\n";
    }

    void menuFindByCnic() const {
        string cnic;
        if (!askCnic("CNIC to search (Enter to cancel): ", cnic)) return;
        Guest* g = cnicIndex.find(cnic);
        if (!g) { cout << "  No guest found with that CNIC.\n"; return; }
        cout << "  " << g->info() << "\n" << g->staysText();
    }

    void menuFindByID() const {
        int id = readInt("Guest ID to search: ", 1, 999999999);
        Guest* g = guests.search(id);
        if (!g) { cout << "  No guest with ID " << id << ".\n"; return; }
        cout << "  " << g->info() << "\n" << g->staysText();
    }

    // Preset services only; the date must fall inside one of the stays
    void menuAddCharge() {
        string cnic;
        if (!askCnic("Guest CNIC (Enter to cancel): ", cnic)) return;
        Guest* g = cnicIndex.find(cnic);
        if (!g) { cout << "  No guest found with that CNIC.\n"; return; }
        if (!g->hasStays()) {
            cout << "  " << g->getName() << " has no booking yet. "
                 << "Book a room first.\n";
            return;
        }
        cout << "  Stays of " << g->getName() << ":\n" << g->staysText()
             << "  Services:\n";
        for (int i = 0; i < NUM_SERVICES; i++)
            cout << "    " << i + 1 << ". " << pad(SERVICE_NAMES[i], 15)
                 << money(SERVICE_PRICES[i]) << "\n";
        int s = readInt("Choose service (1-" + to_string(NUM_SERVICES) + "): ",
                        1, NUM_SERVICES) - 1;
        int qty = readInt("Quantity (1-20): ", 1, 20);

        int date;
        while (true) {
            if (!askDate("Service date (DD-MM-YYYY, Enter to cancel): ", date))
                return;
            if (g->hasStayOn(date)) break;
            cout << "  The date must fall within one of the stays above.\n";
        }

        double amount = qty * SERVICE_PRICES[s];
        g->addCharge(SERVICE_NAMES[s] + " x" + to_string(qty) + " (" +
                     money(SERVICE_PRICES[s]) + " each)", amount, date);
        cout << "  Added: " << dateString(date) << " | " << SERVICE_NAMES[s]
             << " x" << qty << " | " << money(amount) << "\n"
             << "  New bill total: " << money(g->getTotalBill()) << "\n";
    }

    void menuPrintBill() const {
        string cnic;
        if (!askCnic("Guest CNIC (Enter to cancel): ", cnic)) return;
        Guest* g = cnicIndex.find(cnic);
        if (!g) { cout << "  No guest found with that CNIC.\n"; return; }
        if (!g->hasStays()) {
            cout << "  " << g->getName()
                 << " has no booking, so there is no bill.\n";
            return;
        }
        cout << "\n" << g->billStatement();
    }

    // 3 guests and 8 bookings (dates relative to today) showing key cases
    void loadSampleData() {
        if (sampleLoaded) { cout << "  Sample data is already loaded.\n"; return; }
        cout << "  This creates 3 sample guests and runs 8 booking requests,\n"
             << "  so some rooms will become booked.\n";
        if (!readYesNo("  Continue? (y/n): ")) { cout << "  Cancelled.\n"; return; }

        if (cnicIndex.find("3520112345671") || cnicIndex.find("6110198765432") ||
            cnicIndex.find("1730155512349")) {
            cout << "  A sample CNIC is already registered, "
                 << "so nothing was loaded.\n";
            return;
        }
        Guest* ali = registerGuest("Ali Khan", "3520112345671", "03001234567");
        Guest* sara = registerGuest("Sara Ahmed", "6110198765432", "03111234567");
        Guest* bilal = registerGuest("Bilal Raza", "1730155512349", "03211234567");
        sampleLoaded = true;
        int d = today();

        cout << "\n  Loaded 3 guests. Running 8 booking requests:\n";
        cout << "\n 1) Ali, Basic, tomorrow, 4 nights (lowest free room)\n";
        bookRoom(ali, 0, d + 1, 4);
        cout << "\n 2) Sara, Basic, in 3 days, 4 nights (101 busy, so next room)\n";
        bookRoom(sara, 0, d + 3, 4);
        cout << "\n 3) Bilal, Basic, in 5 days, 3 nights (101 free after Ali)\n";
        bookRoom(bilal, 0, d + 5, 3);
        cout << "\n 4) Ali, Business Class, in 10 days, 5 nights\n";
        bookRoom(ali, 1, d + 10, 5);
        cout << "\n 5) Sara, Premium Class, tomorrow, 3 nights\n";
        bookRoom(sara, 2, d + 1, 3);
        cout << "\n 6) Bilal, Premium Class, in 2 days, 2 nights (301 busy)\n";
        bookRoom(bilal, 2, d + 2, 2);
        cout << "\n 7) Ali, Premium Class, in 2 days, 1 night (all busy)\n";
        bookRoom(ali, 2, d + 2, 1);
        cout << "\n 8) Bilal, Premium Suite, in 20 days, 2 nights (room 401)\n";
        bookRoom(bilal, 3, d + 20, 2);
    }

public:
    HotelManager() : nextGuestID(1), nextBookingID(1), sampleLoaded(false) {
        addRoom(101, 0); addRoom(102, 0); addRoom(103, 0); addRoom(104, 0);
        addRoom(201, 1); addRoom(202, 1); addRoom(203, 1);
        addRoom(301, 2); addRoom(302, 2);
        addRoom(401, 3);
    }

    void run() {
        while (true) {
            cout << "\n===== HOTEL MANAGEMENT (DEMO) =====\n"
                 << "Today: " << dateString(today()) << " | Bookings open until "
                 << dateString(windowEnd()) << "\n"
                 << " 1. Room summary (booked and free dates)\n"
                 << " 2. Register guest\n"
                 << " 3. Book a room\n"
                 << " 4. Check if one room is free (interval tree)\n"
                 << " 5. Show free rooms for a date range\n"
                 << " 6. Find guest by CNIC (hash table)\n"
                 << " 7. Find guest by ID (AVL tree)\n"
                 << " 8. Show all guests sorted by ID\n"
                 << " 9. Add service charge to a bill\n"
                 << "10. Print guest bill\n"
                 << "11. Load sample guests and bookings\n"
                 << " 0. Exit\n";
            int choice = readInt("Choose: ", 0, 11);
            switch (choice) {
                case 1: showSummary(); break;
                case 2: menuRegister(); break;
                case 3: menuBook(); break;
                case 4: menuCheckRoom(); break;
                case 5: menuFreeRooms(); break;
                case 6: menuFindByCnic(); break;
                case 7: menuFindByID(); break;
                case 8: showGuests(); break;
                case 9: menuAddCharge(); break;
                case 10: menuPrintBill(); break;
                case 11: loadSampleData(); break;
                case 0: cout << "Goodbye.\n"; return;
            }
        }
    }
};

int main() {
    HotelManager hotel;
    hotel.run();
    return 0;
}
