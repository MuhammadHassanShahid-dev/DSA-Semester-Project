HOTEL MANAGEMENT WITH ROOM BOOKING (Project P21, Group G04) - CS 216L Data Structures Lab
Air University Islamabad, BSCS-III-B, Fall 2026. Instructor: Mr. Shakir Hussain

Group members
  Muhammad Hassan Shahid (2501371) - Group Leader
  Syed Ali Mudassar Kazmi (2501368)
  Abdul Wasay (2501326)

GitHub repository : https://github.com/RanaCoder804/DSA-Semester-Project
Demo video link   : (add here)

Compile (g++, C++17) - run from the project folder:
  g++ -std=c++17 -Iinclude src/Utils.cpp src/IntervalTree.cpp src/RoomHeap.cpp src/RoomList.cpp src/HousekeepingQueue.cpp src/Guest.cpp src/GuestAVL.cpp src/CnicHashTable.cpp src/PlainBST.cpp src/HotelManager.cpp src/Menus.cpp src/FileIO.cpp src/DataGen.cpp src/Benchmark.cpp src/main.cpp -o hotel

Dataset generator (writes data/hotel_data.txt with 1,500 guests):
  g++ -std=c++17 -Iinclude src/Utils.cpp src/IntervalTree.cpp src/RoomHeap.cpp src/RoomList.cpp src/HousekeepingQueue.cpp src/Guest.cpp src/GuestAVL.cpp src/CnicHashTable.cpp src/PlainBST.cpp src/HotelManager.cpp src/Menus.cpp src/FileIO.cpp src/DataGen.cpp src/Benchmark.cpp data/DataGenerator.cpp -o generator
  generator

Run:
  hotel
  Menu 6 (Data, Files and Analysis) -> 2 (Load from file) -> data/hotel_data.txt
