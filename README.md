# BountyChore
**Author:** Team BountyChore (Dikesh Shrestha, Sudip Bashyal, Aayush Marasini)
**Course:** CSC 307 Data Structures and Algorithm Analysis
**Semester:** Fall 2026

---

## 1. Prerequisites & Environment
* **Operating System:** Windows 10/11, Ubuntu 22.04, or macOS (any OS with a C++17 compiler)
* **Compiler:** GCC 9+, Clang 10+, or MSVC 2019+ (must support C++17: `std::filesystem`, `std::optional`)
* **Build System:** CMake 3.10+ (optional); a single `g++` command also works

### Required Libraries
None. The program uses only the C++ standard library. The max-heap, chained hash table and AVL tree are all implemented by the team.

---

## 2. Installation & Setup
1. Install a C++17 compiler (for example MinGW-w64/MSYS2 on Windows, `build-essential` on Ubuntu, Xcode tools on macOS).
2. (Optional) Install CMake from https://cmake.org/download/.
3. Copy or clone the project folder. Source files are in `src/`.

---

## 3. Build Instructions

**Using g++ (no CMake needed):**
```bash
mkdir bin
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o bin/BountyChore
```

**Using CMake:**
```bash
mkdir build
cd build
cmake ..
cmake --build .
```
The executable is written to `bin/`.

**Using Visual Studio (Developer Command Prompt):**
```bash
mkdir bin
cl /std:c++17 /EHsc /O2 src\main.cpp /Fe:bin\BountyChore.exe
```

---

## 4. Execution Guide
* **Executable Name:** `bin/BountyChore` (`bin\BountyChore.exe` on Windows)
* **Entry Point:** No command-line arguments. Run `./bin/BountyChore` from the project folder.
* **Assets:** None. Data is stored in CSV files inside a folder you choose (default `data`, created on first save). A ready-made example is in `sample_data/`: choose **Load**, then type `sample_data`.

### Menu overview
| Menu | What it does |
|------|--------------|
| 1. Add | Add Chore (ID, name, room, base points, daily growth, urgency) or Add Roommate |
| 2. View / Search | Top bounty, search a chore by ID, full bounty board, search a roommate, balance leaderboard + fairness report |
| 3. Modify | Edit a chore, add a tip from a roommate's balance, rename a roommate, advance one day |
| 4. Complete / Delete | Complete the top chore (credits the roommate) or delete a chore (refunds tips) |
| 5. Save | Writes `chores.csv`, `roommates.csv`, `tips.csv` to a folder |
| 6. Load | Reads and validates the three CSV files, then replaces the session |
| 7. Exit | Prompts to save if there are unsaved changes |

Press **Enter on an empty prompt** to cancel the command you are in.

### Bounty rules
* Live bounty = `(B + D*R) * U + T` (B base points, D days unclaimed, R daily growth, U urgency 1.0/1.5/2.0/2.5, T tips).
* Points are kept internally in half-point units, so 1.5x / 2.5x payouts are exact.
* Completing a chore credits the full bounty to the roommate's balance but only `(B + D*R) * U` to their earned work points (tips are transfers, not new work).
* Ties: higher bounty first, then the chore waiting longer, then the alphabetically smaller task ID.
* Bounties only grow when you choose **Advance one day**.

### Files
| File | Columns |
|------|---------|
| `chores.csv` | TaskID, Name, Room, BasePoints, DailyInflation, DaysUnclaimed, UrgencyMultiplier |
| `roommates.csv` | RoommateID, Name, EarnedWorkPoints, AvailableBalance, ChoresCompleted |
| `tips.csv` | TaskID, DonorID, Amount |

### Source layout
* `ChoreMaxHeap.h` - binary max-heap (primary data structure) with task-ID index
* `StringHashMap.h` - chained hash table (task ID -> heap index, roommate ID -> AVL key)
* `RoommateAVL.h` - AVL tree of roommates ordered by (balance, ID)
* `Storage.h` - CSV save/load with validation
* `main.cpp` - menu interface

---

## 5. Troubleshooting / Known Issues
* **"Load rejected"**: the message names the file and line with the problem (missing file, bad row, duplicate ID, unknown chore/roommate). The current session is left unchanged.
* **"Save failed"**: check that the folder is writable. The session stays marked unsaved.
* IDs may contain only letters, digits, `-` and `_` (max 32 characters). Names and rooms may contain commas.
* Roommates cannot be deleted (so tip donor references stay valid).
* There is no user authentication; picking a roommate ID only chooses whose balance changes.
