// rle32_to_vcd.cpp
//
// Generate a VCD from a run-length encoded 32-bit bus.
//
// Input (stdin): lines of "<value> <runlen>"
//   - <value>  : 32-bit value in hex (e.g. 0xDEADBEEF) or decimal
//   - <runlen> : number of samples the value is held (uint64)
// Blank lines and lines starting with '#' are ignored.
//
// Time model:
//   - Each sample advances time by dt (default dt=1).
//   - First run starts at t=0 and holds for runlen*dt.
//   - Each subsequent run starts at the cumulative boundary time.
//
// Usage:
//   ./rle32_to_vcd [out.vcd] [dt] [timescale]
// Examples:
//   echo "0x0 10\n0xFFFFFFFF 5\n0x12345678 20" | ./rle32_to_vcd out.vcd 1 1ns
//   ./rle32_to_vcd out.vcd 10 1ps < rle.txt
//
// Build:
//   g++ -std=c++17 -O2 -Wall -Wextra rle32_to_vcd.cpp -o rle32_to_vcd

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string now_as_string()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return os.str();
}

static std::string trim(std::string s)
{
    auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_ws(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_ws(static_cast<unsigned char>(s.back())))  s.pop_back();
    return s;
}

static bool parse_u64(const std::string& token, uint64_t& out)
{
    char* end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(token.c_str(), &end, 0);
    if (errno != 0 || end == token.c_str() || *end != '\0') return false;
    out = static_cast<uint64_t>(v);
    return true;
}

static bool parse_u32(const std::string& token, uint32_t& out)
{
    uint64_t tmp = 0;
    if (!parse_u64(token, tmp)) return false;
    out = static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
    return true;
}

static std::string bin32(uint32_t v)
{
    std::string s(32, '0');
    for (int i = 31; i >= 0; --i) {
        s[31 - i] = ((v >> i) & 1u) ? '1' : '0';
    }
    return s;
}

int main(int argc, char** argv)
{
    std::string out_path   = (argc >= 2) ? argv[1] : "out.vcd";
    uint64_t dt            = 1;
    std::string timescale  = "1ns";

    if (argc >= 3) {
        uint64_t tmp = 0;
        if (!parse_u64(argv[2], tmp) || tmp == 0) {
            std::cerr << "Invalid dt: " << argv[2] << " (must be positive integer)\n";
            return 2;
        }
        dt = tmp;
    }
    if (argc >= 4) {
        timescale = argv[3]; // e.g. 1ns, 10ps, 1fs (VCD timescale token)
    }

    std::ofstream out(out_path, std::ios::out | std::ios::trunc);
    if (!out) {
        std::cerr << "Cannot open output file: " << out_path << "\n";
        return 2;
    }

    // VCD identifier for the bus. Any short printable token is ok; keep it simple.
    const std::string id = "b"; // VCD "symbol" (identifier)
    const std::string bus_name = "bus32";

    // Header
    out << "$date\n  " << now_as_string() << "\n$end\n";
    out << "$version\n  rle32_to_vcd.cpp\n$end\n";
    out << "$timescale " << timescale << " $end\n";
    out << "$scope module top $end\n";
    out << "$var wire 32 " << id << " " << bus_name << " $end\n";
    out << "$upscope $end\n";
    out << "$enddefinitions $end\n\n";

    // Read first valid run to initialize.
    bool have_any = false;
    uint32_t cur_val = 0;
    uint64_t cur_run = 0;

    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream is(line);
        std::string vtok, rtok;
        if (!(is >> vtok >> rtok)) continue;

        uint32_t v = 0;
        uint64_t r = 0;
        if (!parse_u32(vtok, v) || !parse_u64(rtok, r)) {
            std::cerr << "Parse error in line: " << line << "\n";
            return 2;
        }
        if (r == 0) continue;

        cur_val = v;
        cur_run = r;
        have_any = true;
        break;
    }

    if (!have_any) {
        std::cerr << "No valid <value runlen> pairs found on stdin.\n";
        return 1;
    }

    // Initial value at t=0
    uint64_t t = 0;
    out << "#0\n";
    out << "b" << bin32(cur_val) << " " << id << "\n";

    // Advance through remaining runs
    t += cur_run * dt;

    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream is(line);
        std::string vtok, rtok;
        if (!(is >> vtok >> rtok)) continue;

        uint32_t v = 0;
        uint64_t r = 0;
        if (!parse_u32(vtok, v) || !parse_u64(rtok, r)) {
            std::cerr << "Parse error in line: " << line << "\n";
            return 2;
        }
        if (r == 0) continue;

        // Start of next run: emit new value at the boundary time.
        out << "#" << t << "\n";
        out << "b" << bin32(v) << " " << id << "\n";

        // Hold for r samples.
        t += r * dt;
    }

    // Optional: add a final timestamp marker (no value change).
    out << "#" << t << "\n";

    return 0;
}

