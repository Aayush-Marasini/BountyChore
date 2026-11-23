// Storage.h - CSV persistence (chores.csv, roommates.csv, tips.csv).
// Load parses and validates everything into a scratch Household first; the live
// session is only replaced by the caller after a fully successful load.
#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Household.h"

namespace bc {

namespace fs = std::filesystem;

inline std::string csvEscape(const std::string& s) {
    bool quote = s.find_first_of(",\"") != std::string::npos ||
                 (!s.empty() && (std::isspace(static_cast<unsigned char>(s.front())) ||
                                 std::isspace(static_cast<unsigned char>(s.back()))));
    if (!quote) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    return out + "\"";
}

// Splits one CSV line (supports "quoted, fields" and "" escapes). False on bad quoting.
inline bool csvParseLine(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else inQuotes = false;
            } else cur += c;
        } else if (c == '"' && cur.empty()) {
            inQuotes = true;
        } else if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    if (inQuotes) return false;
    out.push_back(cur);
    return true;
}

struct CsvRow {
    int lineNo;
    std::vector<std::string> cells;
};

// Reads a CSV file, skipping the header row and blank lines.
inline bool readCsv(const fs::path& path, std::vector<CsvRow>& rows, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "Cannot open " + path.string() + " (missing or unreadable).";
        return false;
    }
    std::string line;
    int lineNo = 0;
    bool header = true;
    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (trim(line).empty()) continue;
        if (header) { header = false; continue; }
        CsvRow row;
        row.lineNo = lineNo;
        if (!csvParseLine(line, row.cells)) {
            err = path.filename().string() + " line " + std::to_string(lineNo) +
                  ": malformed quotes.";
            return false;
        }
        for (auto& c : row.cells) c = trim(c);
        rows.push_back(std::move(row));
    }
    return true;
}

inline std::string rowErr(const fs::path& p, const CsvRow& r, const std::string& msg) {
    return p.filename().string() + " line " + std::to_string(r.lineNo) + ": " + msg;
}

inline bool loadHousehold(const std::string& dir, Household& out, std::string& err) {
    const fs::path base(dir);
    const fs::path pChores = base / "chores.csv";
    const fs::path pRoom = base / "roommates.csv";
    const fs::path pTips = base / "tips.csv";
    const int64 kMax = 1000000000;

    std::vector<CsvRow> rChores, rRoom, rTips;
    if (!readCsv(pRoom, rRoom, err)) return false;
    if (!readCsv(pChores, rChores, err)) return false;
    if (!readCsv(pTips, rTips, err)) return false;

    Household next;

    for (const auto& r : rRoom) {
        if (r.cells.size() != 5) { err = rowErr(pRoom, r, "expected 5 columns."); return false; }
        Roommate m;
        m.id = r.cells[0];
        m.name = r.cells[1];
        if (!validId(m.id)) { err = rowErr(pRoom, r, "invalid roommate ID."); return false; }
        if (!validText(m.name)) { err = rowErr(pRoom, r, "invalid name."); return false; }
        if (!parseHalf(r.cells[2], m.earnedHalf) || !parseHalf(r.cells[3], m.balanceHalf) ||
            !parseWhole(r.cells[4], m.completed)) {
            err = rowErr(pRoom, r, "invalid point or count value.");
            return false;
        }
        if (!next.roommates.addRecord(m)) {
            err = rowErr(pRoom, r, "duplicate roommate ID '" + m.id + "'.");
            return false;
        }
    }

    std::vector<ChoreNode> chores;
    StringHashMap<int> seen;
    for (const auto& r : rChores) {
        if (r.cells.size() != 7) { err = rowErr(pChores, r, "expected 7 columns."); return false; }
        ChoreNode c;
        c.taskId = r.cells[0];
        c.name = r.cells[1];
        c.room = r.cells[2];
        if (!validId(c.taskId)) { err = rowErr(pChores, r, "invalid task ID."); return false; }
        if (!validText(c.name) || !validText(c.room)) {
            err = rowErr(pChores, r, "invalid name or room.");
            return false;
        }
        if (!parseWhole(r.cells[3], c.basePoints) || c.basePoints < 1 || c.basePoints > kMax) {
            err = rowErr(pChores, r, "base points must be a whole number 1-1000000000.");
            return false;
        }
        if (!parseWhole(r.cells[4], c.dailyGrowth) || c.dailyGrowth > kMax) {
            err = rowErr(pChores, r, "daily growth must be a whole number 0-1000000000.");
            return false;
        }
        if (!parseWhole(r.cells[5], c.daysUnclaimed) || c.daysUnclaimed > kMax) {
            err = rowErr(pChores, r, "days unclaimed must be a whole number >= 0.");
            return false;
        }
        if (!parseHalf(r.cells[6], c.urgencyTimes2) || c.urgencyTimes2 < 2 || c.urgencyTimes2 > 5) {
            err = rowErr(pChores, r, "urgency must be 1.0, 1.5, 2.0 or 2.5.");
            return false;
        }
        if (!seen.insert(c.taskId, 1)) {
            err = rowErr(pChores, r, "duplicate task ID '" + c.taskId + "'.");
            return false;
        }
        chores.push_back(c);
    }

    // Tips rebuild each chore's tip pot.
    StringHashMap<std::size_t> choreIdx;
    for (std::size_t i = 0; i < chores.size(); ++i) choreIdx.insert(chores[i].taskId, i);
    for (const auto& r : rTips) {
        if (r.cells.size() != 3) { err = rowErr(pTips, r, "expected 3 columns."); return false; }
        Tip t;
        t.taskId = r.cells[0];
        t.donorId = r.cells[1];
        if (!parseWhole(r.cells[2], t.amount) || t.amount < 1 || t.amount > kMax) {
            err = rowErr(pTips, r, "tip amount must be a whole number >= 1.");
            return false;
        }
        const std::size_t* ci = choreIdx.find(t.taskId);
        if (!ci) { err = rowErr(pTips, r, "unknown chore '" + t.taskId + "'."); return false; }
        if (!next.roommates.exists(t.donorId)) {
            err = rowErr(pTips, r, "unknown roommate '" + t.donorId + "'.");
            return false;
        }
        try { chores[*ci].tipPot = checkedAdd(chores[*ci].tipPot, t.amount); }
        catch (const std::overflow_error&) { err = rowErr(pTips, r, "tip total too large."); return false; }
        next.tips.push_back(t);
    }

    try {
        next.chores = ChoreMaxHeap(std::move(chores));
    } catch (const std::exception& e) {
        err = std::string("chores.csv: ") + e.what();
        return false;
    }
    out = std::move(next);
    return true;
}

// Serializes the full state. Files are written to temporary names first and only
// renamed into place once every file was written successfully.
inline bool saveHousehold(const std::string& dir, const Household& h, std::string& err) {
    const fs::path base(dir);
    std::error_code ec;
    fs::create_directories(base, ec);
    if (ec || !fs::is_directory(base)) {
        err = "Cannot create or use folder '" + dir + "'.";
        return false;
    }

    std::ostringstream chores, rooms, tips;
    chores << "TaskID,Name,Room,BasePoints,DailyInflation,DaysUnclaimed,UrgencyMultiplier\n";
    for (const auto& c : h.chores.snapshot())
        chores << csvEscape(c.taskId) << ',' << csvEscape(c.name) << ',' << csvEscape(c.room)
               << ',' << c.basePoints << ',' << c.dailyGrowth << ',' << c.daysUnclaimed << ','
               << formatHalf(c.urgencyTimes2) << '\n';
    rooms << "RoommateID,Name,EarnedWorkPoints,AvailableBalance,ChoresCompleted\n";
    for (const auto& r : h.roommates.all())
        rooms << csvEscape(r.id) << ',' << csvEscape(r.name) << ',' << formatHalf(r.earnedHalf)
              << ',' << formatHalf(r.balanceHalf) << ',' << r.completed << '\n';
    tips << "TaskID,DonorID,Amount\n";
    for (const auto& t : h.tips)
        tips << csvEscape(t.taskId) << ',' << csvEscape(t.donorId) << ',' << t.amount << '\n';

    struct Out { const char* name; std::string body; };
    std::vector<Out> files = {{"chores.csv", chores.str()},
                              {"roommates.csv", rooms.str()},
                              {"tips.csv", tips.str()}};

    for (const auto& f : files) {
        std::ofstream o(base / (std::string(f.name) + ".tmp"), std::ios::trunc | std::ios::binary);
        if (!o) { err = std::string("Cannot open ") + f.name + " for writing."; goto cleanup; }
        o << f.body;
        o.flush();
        if (!o) { err = std::string("Write failed for ") + f.name + "."; goto cleanup; }
    }
    for (const auto& f : files) {
        fs::path tmp = base / (std::string(f.name) + ".tmp");
        fs::path dst = base / f.name;
        std::error_code e2;
        fs::rename(tmp, dst, e2);
        if (e2) {
            fs::remove(dst, e2);
            e2.clear();
            fs::rename(tmp, dst, e2);
        }
        if (e2) { err = std::string("Cannot replace ") + f.name + ": " + e2.message(); goto cleanup; }
    }
    return true;

cleanup:
    for (const auto& f : files) fs::remove(base / (std::string(f.name) + ".tmp"), ec);
    return false;
}

} // namespace bc
