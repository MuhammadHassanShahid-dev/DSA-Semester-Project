
// Hotel Management with Room Booking
// DSA Lab Semester Project (P21)
// Language: C++17

// Project Description:
//
// This project is a Hotel Management System with Room Booking.
//
// Data Structures Used:
// 1. Interval Tree (AVL Balanced): Stores booked dates of each room.
// 2. Linked List: Manages rooms, guest stays, and guest bill lines.
// 3. Min-Heap: Finds the lowest available room number of each room type.
// 4. AVL Tree: Stores guests according to their guest ID.
// 5. Hash Table: Searches guests using their CNIC.
//
// Booking Rules:
// - Check-in and check-out are both at 12:00 PM.
// - Dates are represented as days since 1970-01-01.
// - Bookings are accepted from today up to one year ahead.
//
// Main Project Features:
// - Housekeeping queue
// - Extend and cancel bookings
// - Check-out and delete operations
// - Rehashing
// - Discounts
// - File saving and loading
// - Data generation and benchmarks

#include <iostream>
using namespace std;

int main()
{
    cout << "======================================" << endl;
    cout << "   HOTEL MANAGEMENT SYSTEM" << endl;
    cout << "       ROOM BOOKING PROJECT" << endl;
    cout << "======================================" << endl;

    cout << "Welcome to our Hotel Management System!" << endl;
    cout << "Our project is under development." << endl;

    return 0;
}
