#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <exception>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include "models/Person.h"
#include "models/CoachClass.h"
#include "models/Berth.h"
#include "models/Train.h"
#include "models/Inventory.h"
#include "models/Payment.h"
#include "models/Exceptions.h"
#include "models/RailwaySystem.h"

using namespace std;

// ============================================================================
//  QR code encoder (ISO/IEC 18004, byte mode, versions 1-40, ECC L/M/Q/H).
//  Self-contained: no libraries, no internet.  Output is an SVG string.
// ============================================================================
namespace qrgen {

static const int ECC_PER_BLOCK[4][41] = {
    {-1,7,10,15,20,26,18,20,24,30,18,20,24,26,30,22,24,28,30,28,28,28,28,30,30,26,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30},
    {-1,10,16,26,18,24,16,18,22,22,26,30,22,22,24,24,28,28,26,26,26,26,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28},
    {-1,13,22,18,26,18,24,18,22,20,24,28,26,24,20,30,24,28,28,26,30,28,30,30,30,30,28,30,30,30,30,30,30,30,30,30,30,30,30,30,30},
    {-1,17,28,22,16,22,28,26,26,24,28,24,28,22,24,24,30,28,28,26,28,30,24,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30}};
static const int NUM_BLOCKS[4][41] = {
    {-1,1,1,1,1,1,2,2,2,2,4,4,4,4,4,6,6,6,6,7,8,8,9,9,10,12,12,12,13,14,15,16,17,18,19,19,20,21,22,24,25},
    {-1,1,1,1,2,2,4,4,4,5,5,5,8,9,9,10,10,11,13,14,16,17,17,18,20,21,23,25,26,28,29,31,33,35,37,38,40,43,45,47,49},
    {-1,1,1,2,2,4,4,6,6,8,8,8,10,12,16,12,17,16,18,21,20,23,23,25,27,29,34,34,35,38,40,43,45,48,51,53,56,59,62,65,68},
    {-1,1,1,2,4,4,4,5,6,8,8,11,11,16,16,18,16,19,21,25,25,25,34,30,32,35,37,40,42,45,48,51,54,57,60,63,66,70,74,77,81}};
static const int FORMAT_BITS[4] = {1, 0, 3, 2};   // L, M, Q, H

static int rawDataModules(int ver){
    int r = (16*ver + 128)*ver + 64;
    if(ver >= 2){
        int na = ver/7 + 2;
        r -= (25*na - 10)*na - 55;
        if(ver >= 7) r -= 36;
    }
    return r;
}
static int dataCodewords(int ver, int ecl){
    return rawDataModules(ver)/8 - ECC_PER_BLOCK[ecl][ver]*NUM_BLOCKS[ecl][ver];
}

// Reed-Solomon over GF(256), polynomial 0x11D
static uint8_t gfMul(uint8_t x, uint8_t y){
    int z = 0;
    for(int i = 7; i >= 0; i--){ z = (z << 1) ^ ((z >> 7) * 0x11D); z ^= ((y >> i) & 1) * x; }
    return (uint8_t)z;
}
static vector<uint8_t> rsDivisor(int degree){
    vector<uint8_t> r(degree, 0); r[degree-1] = 1;
    uint8_t root = 1;
    for(int i = 0; i < degree; i++){
        for(int j = 0; j < degree; j++){
            r[j] = gfMul(r[j], root);
            if(j + 1 < degree) r[j] ^= r[j+1];
        }
        root = gfMul(root, 0x02);
    }
    return r;
}
static vector<uint8_t> rsRemainder(const vector<uint8_t>& data, const vector<uint8_t>& div){
    vector<uint8_t> res(div.size(), 0);
    for(uint8_t b : data){
        uint8_t f = b ^ res[0];
        res.erase(res.begin()); res.push_back(0);
        for(size_t i = 0; i < div.size(); i++) res[i] ^= gfMul(div[i], f);
    }
    return res;
}

struct Matrix {
    int size;
    vector<vector<char>> mod, fn;      // module colour, is-function-pattern
    explicit Matrix(int ver) : size(ver*4 + 17), mod(size, vector<char>(size, 0)), fn(size, vector<char>(size, 0)) {}
    void setFn(int x, int y, bool dark){ mod[y][x] = dark; fn[y][x] = 1; }
};

static void drawFinder(Matrix& m, int cx, int cy){
    for(int dy = -4; dy <= 4; dy++) for(int dx = -4; dx <= 4; dx++){
        int d = max(abs(dx), abs(dy)), x = cx + dx, y = cy + dy;
        if(x >= 0 && x < m.size && y >= 0 && y < m.size) m.setFn(x, y, d != 2 && d != 4);
    }
}
static void drawAlign(Matrix& m, int cx, int cy){
    for(int dy = -2; dy <= 2; dy++) for(int dx = -2; dx <= 2; dx++)
        m.setFn(cx + dx, cy + dy, max(abs(dx), abs(dy)) != 1);
}
static void drawFormat(Matrix& m, int ecl, int mask){
    int data = FORMAT_BITS[ecl] << 3 | mask, rem = data;
    for(int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = (data << 10 | rem) ^ 0x5412;
    auto bit = [&](int i){ return ((bits >> i) & 1) != 0; };
    for(int i = 0; i <= 5; i++) m.setFn(8, i, bit(i));
    m.setFn(8, 7, bit(6)); m.setFn(8, 8, bit(7)); m.setFn(7, 8, bit(8));
    for(int i = 9; i < 15; i++) m.setFn(14 - i, 8, bit(i));
    for(int i = 0; i < 8; i++) m.setFn(m.size - 1 - i, 8, bit(i));
    for(int i = 8; i < 15; i++) m.setFn(8, m.size - 15 + i, bit(i));
    m.setFn(8, m.size - 8, true);
}
static void drawFunctionPatterns(Matrix& m, int ver, int ecl){
    for(int i = 0; i < m.size; i++){ m.setFn(6, i, i % 2 == 0); m.setFn(i, 6, i % 2 == 0); }
    drawFinder(m, 3, 3); drawFinder(m, m.size - 4, 3); drawFinder(m, 3, m.size - 4);
    if(ver >= 2){
        int na = ver/7 + 2;
        int step = (ver == 32) ? 26 : (ver*4 + na*2 + 1) / (na*2 - 2) * 2;
        vector<int> pos;
        for(int i = 0, p = m.size - 7; i < na - 1; i++, p -= step) pos.insert(pos.begin(), p);
        pos.insert(pos.begin(), 6);
        for(int i = 0; i < na; i++) for(int j = 0; j < na; j++)
            if(!((i == 0 && j == 0) || (i == 0 && j == na-1) || (i == na-1 && j == 0)))
                drawAlign(m, pos[i], pos[j]);
    }
    drawFormat(m, ecl, 0);
    if(ver >= 7){
        int rem = ver;
        for(int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        long bits = (long)ver << 12 | rem;
        for(int i = 0; i < 18; i++){
            bool c = ((bits >> i) & 1) != 0;
            int a = m.size - 11 + i % 3, b = i / 3;
            m.setFn(a, b, c); m.setFn(b, a, c);
        }
    }
}
static void placeData(Matrix& m, const vector<uint8_t>& cw){
    size_t i = 0;
    for(int right = m.size - 1; right >= 1; right -= 2){
        if(right == 6) right = 5;
        for(int vert = 0; vert < m.size; vert++) for(int j = 0; j < 2; j++){
            int x = right - j;
            bool up = ((right + 1) & 2) == 0;
            int y = up ? m.size - 1 - vert : vert;
            if(!m.fn[y][x] && i < cw.size()*8){ m.mod[y][x] = ((cw[i >> 3] >> (7 - (i & 7))) & 1) != 0; i++; }
        }
    }
}
static void applyMask(Matrix& m, int mask){
    for(int y = 0; y < m.size; y++) for(int x = 0; x < m.size; x++){
        bool inv = false;
        switch(mask){
            case 0: inv = (x + y) % 2 == 0; break;
            case 1: inv = y % 2 == 0; break;
            case 2: inv = x % 3 == 0; break;
            case 3: inv = (x + y) % 3 == 0; break;
            case 4: inv = (x/3 + y/2) % 2 == 0; break;
            case 5: inv = x*y % 2 + x*y % 3 == 0; break;
            case 6: inv = (x*y % 2 + x*y % 3) % 2 == 0; break;
            default: inv = ((x + y) % 2 + x*y % 3) % 2 == 0; break;
        }
        if(inv && !m.fn[y][x]) m.mod[y][x] ^= 1;
    }
}
static int penalty(const Matrix& m){
    int n = m.size, score = 0, dark = 0;
    for(int pass = 0; pass < 2; pass++){                       // rows then columns
        for(int a = 0; a < n; a++){
            int run = 1;
            for(int b = 0; b < n; b++){
                char c = pass ? m.mod[b][a] : m.mod[a][b];
                if(c) dark++;
                if(b > 0){
                    char p = pass ? m.mod[b-1][a] : m.mod[a][b-1];
                    if(c == p){ run++; if(run == 5) score += 3; else if(run > 5) score++; }
                    else run = 1;
                }
            }
        }
    }
    for(int y = 0; y + 1 < n; y++) for(int x = 0; x + 1 < n; x++){
        char c = m.mod[y][x];
        if(c == m.mod[y][x+1] && c == m.mod[y+1][x] && c == m.mod[y+1][x+1]) score += 3;
    }
    static const int pat[11] = {1,0,1,1,1,0,1,0,0,0,0};
    for(int pass = 0; pass < 2; pass++) for(int a = 0; a < n; a++) for(int b = 0; b + 11 <= n; b++){
        bool f1 = true, f2 = true;
        for(int k = 0; k < 11; k++){
            int v = pass ? m.mod[b+k][a] : m.mod[a][b+k];
            if(v != pat[k]) f1 = false;
            if(v != pat[10-k]) f2 = false;
        }
        if(f1) score += 40;
        if(f2) score += 40;
    }
    int total = n*n, k = (abs(dark*20 - total*10) + total - 1) / total - 1;
    return score + max(k, 0) * 10;
}

// Returns true on success; `out` becomes the module matrix (true = dark).
static bool encode(const string& text, int ecl, vector<vector<char>>& out){
    int ver = 0, capBits = 0;
    for(int v = 1; v <= 40; v++){
        int cc = v < 10 ? 8 : 16;
        int need = 4 + cc + 8*(int)text.size();
        if(need <= dataCodewords(v, ecl)*8){ ver = v; capBits = dataCodewords(v, ecl)*8; break; }
    }
    if(!ver) return false;

    vector<bool> bits;
    auto put = [&](unsigned val, int len){ for(int i = len-1; i >= 0; i--) bits.push_back((val >> i) & 1); };
    put(4, 4);
    put((unsigned)text.size(), ver < 10 ? 8 : 16);
    for(unsigned char c : text) put(c, 8);
    put(0, min(4, capBits - (int)bits.size()));
    while(bits.size() % 8) bits.push_back(false);
    for(uint8_t pad = 0xEC; (int)bits.size() < capBits; pad ^= 0xEC ^ 0x11) put(pad, 8);
    vector<uint8_t> data(bits.size()/8, 0);
    for(size_t i = 0; i < bits.size(); i++) data[i >> 3] |= (bits[i] ? 1 : 0) << (7 - (i & 7));

    int nb = NUM_BLOCKS[ecl][ver], bel = ECC_PER_BLOCK[ecl][ver], raw = rawDataModules(ver)/8;
    int nShort = nb - raw % nb, shortLen = raw / nb;
    vector<vector<uint8_t>> blocks;
    vector<uint8_t> div = rsDivisor(bel);
    for(int i = 0, k = 0; i < nb; i++){
        int dl = shortLen - bel + (i < nShort ? 0 : 1);
        vector<uint8_t> dat(data.begin() + k, data.begin() + k + dl); k += dl;
        vector<uint8_t> ec = rsRemainder(dat, div);
        if(i < nShort) dat.push_back(0);
        dat.insert(dat.end(), ec.begin(), ec.end());
        blocks.push_back(dat);
    }
    vector<uint8_t> cw;
    for(size_t i = 0; i < blocks[0].size(); i++) for(int j = 0; j < nb; j++)
        if((int)i != shortLen - bel || j >= nShort) cw.push_back(blocks[j][i]);

    Matrix m(ver);
    drawFunctionPatterns(m, ver, ecl);
    placeData(m, cw);
    int best = 0, bestPen = 1 << 30;
    for(int mask = 0; mask < 8; mask++){
        applyMask(m, mask); drawFormat(m, ecl, mask);
        int p = penalty(m);
        if(p < bestPen){ bestPen = p; best = mask; }
        applyMask(m, mask);                                    // undo
    }
    applyMask(m, best); drawFormat(m, ecl, best);
    out = m.mod;
    return true;
}

// SVG with a 4-module quiet zone, black on white, crisp edges.
static string toSvg(const vector<vector<char>>& mod, int border = 4, int px = 10){
    int n = (int)mod.size(), full = n + border*2;
    string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + to_string(full) + " " + to_string(full) +
               "\" width=\"" + to_string(full*px) + "\" height=\"" + to_string(full*px) +
               "\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"#fff\"/><path fill=\"#000\" d=\"";
    for(int y = 0; y < n; y++){
        for(int x = 0; x < n; ){
            if(!mod[y][x]){ x++; continue; }
            int x0 = x; while(x < n && mod[y][x]) x++;
            s += "M" + to_string(x0 + border) + "," + to_string(y + border) + "h" + to_string(x - x0) + "v1h-" + to_string(x - x0) + "z";
        }
    }
    return s + "\"/></svg>";
}

} // namespace qrgen

// ============================================================================
//  Small helpers shared by the HTTP handlers
// ============================================================================
static string jsonEscape(const string& s){
    string r; r.reserve(s.size() + 8);
    for(unsigned char c : s){
        switch(c){
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if(c < 0x20){ char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; }
                else r += (char)c;
        }
    }
    return r;
}
static string jstr(const string& s){ return "\"" + jsonEscape(s) + "\""; }

// A read-only view of one Stop. The sample data builds every Stop with the 7
// brace-initialisers { code, name, arrival, departure, haltMinutes, dayOffset, km },
// so a structured binding gives us the fields in that same order.
struct StopInfo { string code, name, arr, dep; int halt, day, km; };
template<class S> static StopInfo stopInfo(const S& s){
    const auto& [c, n, a, d, h, o, k] = s;
    return StopInfo{c, n, a, d, (int)h, (int)o, (int)k};
}
static int toMinutes(const string& hhmm){
    if(hhmm.size() < 5 || !isdigit((unsigned char)hhmm[0])) return -1;     // "--"
    return atoi(hhmm.substr(0, 2).c_str())*60 + atoi(hhmm.substr(3, 2).c_str());
}
static string fmtDuration(int mins){
    if(mins < 0) mins = 0;
    char b[24]; snprintf(b, sizeof b, "%02dh %02dm", mins/60, mins%60);
    return b;
}
static string classLabel(const string& code){
    if(code == "SL") return "Sleeper";
    if(code == "3A") return "AC 3 Tier";
    if(code == "2A") return "AC 2 Tier";
    if(code == "1A") return "AC First";
    if(code == "CC") return "Chair Car";
    if(code == "EC") return "Executive Chair";
    if(code == "3E") return "AC 3 Economy";
    return code;
}
static int classAvailability(const string& code){
    if(code == "SL") return 60;
    if(code == "3A") return 30;
    if(code == "2A") return 18;
    if(code == "1A") return 11;
    if(code == "CC") return 40;
    if(code == "EC") return 12;
    return 25;
}
// minutes from departure at stop `fi` to arrival at stop `ti` (handles overnight legs)
static int segmentMinutes(const StopInfo& a, const StopInfo& b){
    int dep = a.day*1440 + max(toMinutes(a.dep), 0);
    int arr = b.day*1440 + max(toMinutes(b.arr), 0);
    if(arr < dep) arr += 1440;
    return arr - dep;
}
// {"3A":1560,"2A":2210,...} for one boarding/deboarding pair on one train
static string fareObject(Train* t, int dist){
    string o = "{"; bool first = true;
    for(auto c : t->coachClasses){
        if(!first) o += ",";
        first = false;
        o += jstr(c->code) + ":" + to_string((int)RailwaySystem::get().calculateFare(t, c, dist, false));
    }
    return o + "}";
}
static string trainHeaderJson(Train* t){
    string o = "\"number\":" + jstr(t->no) + ",\"name\":" + jstr(t->name) + ",\"type\":" + jstr(t->getType());
    return o;
}

string addDays(string date, int days){
    struct tm tm1 = {};
    istringstream ss(date);
    ss >> get_time(&tm1,"%Y-%m-%d");
    time_t tp = mktime(&tm1);
    tp += days*86400;
    struct tm *tm2 = localtime(&tp);
    char buf[11];
    strftime(buf,sizeof(buf),"%Y-%m-%d",tm2);
    return string(buf);
}

int RailwaySystem::weekdayFromDate(const string& date){
    struct tm tm1 = {};
    istringstream ss(date);
    ss >> get_time(&tm1,"%Y-%m-%d");
    mktime(&tm1);
    return tm1.tm_wday;
}

string RailwaySystem::getOriginDate(const string& boardingDate, int dayOffset){
    return addDays(boardingDate, -dayOffset);
}

double RailwaySystem::calculateFare(Train* train, CoachClass* cls, int distance, bool isTatkal){
    double base = distance * cls->ratePerKm * train->getTariffMultiplier();
    double fare = base + cls->getReservationCharge() + train->getSuperfastCharge();
    if(isTatkal) fare += (cls->isAC()?0.3*base:0.2*base);
    if(cls->isAC()) fare *= 1.05;
    fare += 15;
    return std::round(fare);
}

vector<SearchResult> RailwaySystem::search(const string& from, const string& to, const string& boardingDate, const string& coachCode, const string& quota){
    vector<SearchResult> res;
    for(auto &tptr: trains){
        Train* train = tptr.get();
        int fi = train->getStopIndex(from);
        int ti = train->getStopIndex(to);
        if(fi==-1 || ti==-1 || fi>=ti) continue;
        int dayOffset = train->route[fi].dayOffset;
        string originDate = getOriginDate(boardingDate, dayOffset);
        int wday = weekdayFromDate(originDate);
        if(!train->runsOn(wday)) continue;
        int dist = train->route[ti].km - train->route[fi].km;
        CoachClass* cls = nullptr;
        for(auto c: train->coachClasses) if(c->code==coachCode){ cls=c; break; }
        if(!cls) continue;
        int available = 50;
        bool isTatkal = (quota=="TATKAL");
        double fare = calculateFare(train, cls, dist, isTatkal);
        res.push_back({train, fi, ti, dist, available, fare, originDate});
    }
    sort(res.begin(), res.end(), [](auto &a, auto &b){ return a.fare < b.fare; });
    return res;
}

void RailwaySystem::initSampleData(){
    auto t1=make_unique<Rajdhani>(); t1->no="12001"; t1->name="Ranchi Rajdhani Express"; t1->route={{"NDLS","New Delhi","--","06:00",0,0,0},{"CNB","Kanpur Central","06:45","06:50",5,0,440},{"PRYJ","Prayagraj","08:30","08:35",5,0,630},{"DDU","Pt Deen Dayal","10:30","10:40",10,0,780},{"GAYA","Gaya","12:30","12:35",5,0,990},{"HTE","Ranchi","00:15","--",0,1,1350}}; t1->coachClasses={new AC1(), new AC2(), new AC3()}; addTrain(move(t1));
    auto t2=make_unique<Rajdhani>(); t2->no="12002"; t2->name="New Delhi - Ranchi Rajdhani"; t2->route={{"HTE","Ranchi","--","16:30",0,0,0},{"GAYA","Gaya","20:00","20:05",5,0,360},{"DDU","Pt Deen Dayal","22:00","22:10",10,0,570},{"PRYJ","Prayagraj","00:00","00:05",5,1,720},{"CNB","Kanpur","02:00","02:05",5,1,910},{"NDLS","New Delhi","10:45","--",0,1,1350}}; t2->coachClasses={new AC1(),new AC2(),new AC3()}; addTrain(move(t2));
    auto t3=make_unique<Rajdhani>(); t3->no="12003"; t3->name="Mumbai Rajdhani Express"; t3->route={{"NDLS","New Delhi","--","07:00",0,0,0},{"KOTA","Kota","11:30","11:35",5,0,460},{"BRC","Vadodara","15:30","15:35",5,0,920},{"CSMT","Mumbai CSMT","23:00","--",0,0,1380}}; t3->coachClasses={new AC1(),new AC2(),new AC3()}; addTrain(move(t3));
    auto t4=make_unique<Rajdhani>(); t4->no="12004"; t4->name="Delhi CSMT Express"; t4->route={{"CSMT","Mumbai CSMT","--","15:45",0,0,0},{"BRC","Vadodara","19:45","19:50",5,0,460},{"KOTA","Kota","23:45","23:50",5,0,920},{"NDLS","New Delhi","07:45","--",0,1,1380}}; t4->coachClasses={new AC1(),new AC2(),new AC3()}; addTrain(move(t4));
    auto t5=make_unique<Rajdhani>(); t5->no="12005"; t5->name="Howrah Rajdhani Express"; t5->route={{"HWH","Howrah","--","14:50",0,0,0},{"DHN","Dhanbad","18:00","18:05",5,0,270},{"GAYA","Gaya","20:00","20:05",5,0,450},{"DDU","Pt Deen Dayal","22:00","22:10",10,0,630},{"CNB","Kanpur","00:30","00:35",5,1,1000},{"NDLS","New Delhi","10:50","--",0,1,1450}}; t5->coachClasses={new AC1(),new AC2(),new AC3()}; addTrain(move(t5));
    auto t6=make_unique<Superfast>(); t6->no="12006"; t6->name="Kolkata Mail Express"; t6->route={{"NDLS","New Delhi","--","22:15",0,0,0},{"CNB","Kanpur","04:00","04:10",10,1,440},{"DDU","Pt Deen Dayal","08:00","08:10",10,1,780},{"GAYA","Gaya","10:00","10:05",5,1,990},{"DHN","Dhanbad","12:00","12:05",5,1,1170},{"HWH","Howrah","18:15","--",0,1,1450}}; t6->coachClasses={new Sleeper(), new AC3(), new AC2(), new ChairCar()}; addTrain(move(t6));
    auto t7=make_unique<Duronto>(); t7->no="12801"; t7->name="CSMT-HWH Duronto Express"; t7->route={{"CSMT","Mumbai CSMT","--","05:30",0,0,0},{"NGP","Nagpur","13:30","13:35",5,0,840},{"R","Raipur","17:30","17:35",5,0,1160},{"TATA","Tatanagar","21:30","21:35",5,0,1680},{"HWH","Howrah","07:30","--",0,1,1960}}; t7->coachClasses={new Sleeper(), new AC3(), new AC2()}; addTrain(move(t7));
    auto t8=make_unique<Duronto>(); t8->no="12802"; t8->name="HWH-CSMT Duronto Express"; t8->route={{"HWH","Howrah","--","08:15",0,0,0},{"TATA","Tatanagar","11:15","11:20",5,0,250},{"R","Raipur","15:20","15:25",5,0,770},{"NGP","Nagpur","19:20","19:25",5,0,1090},{"CSMT","Mumbai CSMT","10:15","--",0,1,1960}}; t8->coachClasses={new Sleeper(), new AC3(), new AC2()}; addTrain(move(t8));
    auto t9=make_unique<Superfast>(); t9->no="12269"; t9->name="Kerala Sampark Kranti"; t9->route={{"ALLP","Alappuzha","--","09:20",0,0,0},{"ERS","Ernakulam","10:20","10:25",5,0,57},{"PGT","Palakkad","12:40","12:45",5,0,180},{"CBE","Coimbatore","13:50","13:55",5,0,230},{"MAS","Chennai","20:30","20:50",20,0,700},{"BZA","Vijayawada","02:30","02:40",10,1,1120},{"NGP","Nagpur","10:30","10:35",5,1,1680},{"R","Raipur","14:30","14:35",5,1,2000},{"ROU","Rourkela","18:30","18:35",5,1,2300},{"HTE","Ranchi","03:20","--",0,2,2600}}; t9->coachClasses={new Sleeper(), new AC3(), new AC2()}; addTrain(move(t9));
    auto t10=make_unique<Superfast>(); t10->no="12270"; t10->name="Jharkhand Kerala Express"; t10->route={{"HTE","Ranchi","--","12:00",0,0,0},{"ROU","Rourkela","15:30","15:35",5,0,300},{"R","Raipur","19:30","19:35",5,0,600},{"NGP","Nagpur","23:30","23:35",5,0,920},{"BZA","Vijayawada","07:30","07:40",10,1,1480},{"MAS","Chennai","13:30","13:50",20,1,1900},{"CBE","Coimbatore","20:30","20:35",5,1,2370},{"PGT","Palakkad","21:40","21:45",5,1,2420},{"ERS","Ernakulam","00:00","00:05",5,2,2543},{"ALLP","Alappuzha","06:00","--",0,2,2600}}; t10->coachClasses={new Sleeper(), new AC3(), new AC2()}; addTrain(move(t10));
    auto t11=make_unique<Shatabdi>(); t11->no="12301"; t11->name="Ranchi-Howrah Shatabdi"; t11->route={{"HTE","Ranchi","--","06:15",0,0,0},{"MURI","Muri","07:15","07:17",2,0,70},{"BKS","Bokaro","08:00","08:02",2,0,120},{"DHN","Dhanbad","09:00","09:05",5,0,170},{"ASN","Asansol","10:00","10:02",2,0,230},{"HWH","Howrah","13:15","--",0,0,410}}; t11->coachClasses={new ChairCar(), new ExecChair()}; addTrain(move(t11));
    auto t12=make_unique<Shatabdi>(); t12->no="12302"; t12->name="Howrah-Ranchi Shatabdi"; t12->route={{"HWH","Howrah","--","16:00",0,0,0},{"ASN","Asansol","18:15","18:17",2,0,180},{"DHN","Dhanbad","19:15","19:20",5,0,270},{"BKS","Bokaro","20:15","20:17",2,0,320},{"MURI","Muri","21:00","21:02",2,0,370},{"HTE","Ranchi","23:00","--",0,0,410}}; t12->coachClasses={new ChairCar(), new ExecChair()}; addTrain(move(t12));
}

static string readFile(const string& path){
    ifstream f(path, ios::binary);
    if(!f) return "";
    stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static string urlDecode(const string& s){
    string r;
    for(size_t i = 0; i < s.size(); i++){
        if(s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])){
            r += (char)strtol(s.substr(i+1, 2).c_str(), nullptr, 16);
            i += 2;
        } else if(s[i] == '+') r += ' ';
        else r += s[i];
    }
    return r;
}
static map<string,string> parseQuery(const string& q){
    map<string,string> m;
    stringstream ss(q);
    string kv;
    while(getline(ss, kv, '&')){
        auto p = kv.find('=');
        if(p != string::npos) m[urlDecode(kv.substr(0, p))] = urlDecode(kv.substr(p + 1));
    }
    return m;
}
static string upper(string s){ transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)toupper(c); }); return s; }

struct HttpResp { int status = 200; string type = "text/html; charset=utf-8"; string body; };

static string mimeFor(const string& p){
    auto ends = [&](const char* e){ size_t n = strlen(e); return p.size() >= n && p.compare(p.size() - n, n, e) == 0; };
    if(ends(".html")) return "text/html; charset=utf-8";
    if(ends(".js"))   return "application/javascript; charset=utf-8";
    if(ends(".css"))  return "text/css; charset=utf-8";
    if(ends(".json")) return "application/json; charset=utf-8";
    if(ends(".svg"))  return "image/svg+xml";
    if(ends(".png"))  return "image/png";
    if(ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if(ends(".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

// ---------------------------------------------------------------- API: trains
// Every train with its stops, and the fare for EVERY pair of its stops, so the
// page can offer one train for any combination of boarding / deboarding station.
static string apiTrainsFull(){
    auto& rs = RailwaySystem::get();
    string o = "[";
    for(size_t ti = 0; ti < rs.trains.size(); ti++){
        Train* t = rs.trains[ti].get();
        vector<StopInfo> st;
        for(auto& s : t->route) st.push_back(stopInfo(s));
        if(st.size() < 2) continue;
        if(o.size() > 1) o += ",";
        int total = st.back().km - st.front().km;

        o += "{" + trainHeaderJson(t) + ",\"route\":[";
        for(size_t i = 0; i < st.size(); i++) o += (i ? "," : "") + jstr(st[i].code);
        o += "],\"routeFull\":{";
        for(size_t i = 0; i < st.size(); i++) o += (i ? "," : "") + jstr(st[i].code) + ":" + jstr(st[i].name);
        o += "},\"stops\":[";
        for(size_t i = 0; i < st.size(); i++){
            o += string(i ? "," : "") + "{\"code\":" + jstr(st[i].code) + ",\"name\":" + jstr(st[i].name) +
                 ",\"arr\":" + jstr(st[i].arr) + ",\"dep\":" + jstr(st[i].dep) +
                 ",\"day\":" + to_string(st[i].day) + ",\"km\":" + to_string(st[i].km) + "}";
        }
        o += "],\"depTime\":" + jstr(st.front().dep) + ",\"duration\":" + jstr(fmtDuration(segmentMinutes(st.front(), st.back()))) +
             ",\"km\":" + to_string(total) + ",\"classes\":[";
        bool first = true;
        for(auto c : t->coachClasses){
            if(!first) o += ",";
        first = false;
            o += "{\"code\":" + jstr(c->code) + ",\"label\":" + jstr(classLabel(c->code)) +
                 ",\"price\":" + to_string((int)rs.calculateFare(t, c, total, false)) +
                 ",\"avail\":" + to_string(classAvailability(c->code)) + "}";
        }
        o += "],\"fares\":{";
        first = true;
        for(size_t i = 0; i < st.size(); i++) for(size_t j = i + 1; j < st.size(); j++){
            if(!first) o += ",";
        first = false;
            o += jstr(st[i].code + ">" + st[j].code) + ":" + fareObject(t, st[j].km - st[i].km);
        }
        o += "}}";
    }
    return o + "]";
}

// ------------------------------------------------------------- API: segments
// All station combinations of ONE train: every (from, to) with from before to.
static HttpResp apiSegments(const string& trainNo){
    HttpResp r; r.type = "application/json; charset=utf-8";
    Train* t = nullptr;
    for(auto& p : RailwaySystem::get().trains) if(p->no == trainNo){ t = p.get(); break; }
    if(!t){ r.status = 404; r.body = "{\"error\":\"unknown train\"}"; return r; }
    vector<StopInfo> st;
    for(auto& s : t->route) st.push_back(stopInfo(s));
    r.body = "{" + trainHeaderJson(t) + ",\"segments\":[";
    bool first = true;
    for(size_t i = 0; i < st.size(); i++) for(size_t j = i + 1; j < st.size(); j++){
        if(!first) r.body += ",";
        first = false;
        r.body += "{\"from\":" + jstr(st[i].code) + ",\"to\":" + jstr(st[j].code) +
                  ",\"fromName\":" + jstr(st[i].name) + ",\"toName\":" + jstr(st[j].name) +
                  ",\"km\":" + to_string(st[j].km - st[i].km) + ",\"dep\":" + jstr(st[i].dep) + ",\"arr\":" + jstr(st[j].arr) +
                  ",\"duration\":" + jstr(fmtDuration(segmentMinutes(st[i], st[j]))) +
                  ",\"fares\":" + fareObject(t, st[j].km - st[i].km) + "}";
    }
    r.body += "]}";
    return r;
}

static string apiStations(){
    map<string,string> m;
    for(auto& p : RailwaySystem::get().trains) for(auto& s : p->route){ StopInfo si = stopInfo(s); if(!m.count(si.code)) m[si.code] = si.name; }
    string o = "["; bool first = true;
    for(auto& kv : m){ if(!first) o += ",";
        first = false; o += "{\"code\":" + jstr(kv.first) + ",\"name\":" + jstr(kv.second) + "}"; }
    return o + "]";
}

static string apiSearch(map<string,string>& params){
    string from = upper(params["from"]), to = upper(params["to"]), date = params["date"], cls = params["class"], quota = upper(params["quota"]);
    if(from.empty()) from = "NDLS";
    if(to.empty())   to = "HTE";
    if(date.empty()) date = "2026-10-21";
    if(cls.empty())  cls = "3A";
    if(quota.empty()) quota = "GENERAL";
    auto results = RailwaySystem::get().search(from, to, date, cls, quota);
    string o = "[";
    for(size_t i = 0; i < results.size(); i++){
        auto& r = results[i];
        StopInfo a = stopInfo(r.train->route[r.fromIdx]), b = stopInfo(r.train->route[r.toIdx]);
        if(i) o += ",";
        o += "{\"no\":" + jstr(r.train->no) + ",\"name\":" + jstr(r.train->name) + ",\"type\":" + jstr(r.train->getType()) +
             ",\"from\":" + jstr(from) + ",\"to\":" + jstr(to) +
             ",\"fromName\":" + jstr(a.name) + ",\"toName\":" + jstr(b.name) +
             ",\"dep\":" + jstr(a.dep) + ",\"arr\":" + jstr(b.arr) + ",\"duration\":" + jstr(fmtDuration(segmentMinutes(a, b))) +
             ",\"fromIdx\":" + to_string(r.fromIdx) + ",\"toIdx\":" + to_string(r.toIdx) +
             ",\"distance\":" + to_string(r.distance) + ",\"available\":" + to_string(r.available) +
             ",\"fare\":" + to_string((int)r.fare) + ",\"originDate\":" + jstr(r.originDate) + "}";
    }
    return o + "]";
}

static HttpResp handleRequest(const string& method, const string& path, const string& query){
    HttpResp r;
    auto params = parseQuery(query);
    auto json = [&](string body){ r.type = "application/json; charset=utf-8"; r.body = move(body); };

    if(method != "GET" && method != "HEAD"){ r.status = 405; json("{\"error\":\"method not allowed\"}"); return r; }

    if(path == "/" || path == "/web" || path == "/web/" || path == "/web/index.html"){
        r.body = readFile("web/index.html");
        if(r.body.empty()) r.body = readFile("./web/index.html");
        if(r.body.empty()) r.body = "<h1>SRM Railway Running - C++ Backend Active</h1><p>web/index.html not found next to the executable.</p>";
    }
    else if(path == "/api/qr"){
        // /api/qr?text=...&ecc=M   ->  SVG QR code generated here, in C++
        string text = params["text"], e = upper(params["ecc"]);
        int ecl = (e == "L") ? 0 : (e == "Q") ? 2 : (e == "H") ? 3 : 1;
        vector<vector<char>> mods;
        if(text.empty() || text.size() > 1200 || !qrgen::encode(text, ecl, mods)){
            r.status = 400; json("{\"error\":\"text missing or too long for a QR code\"}");
        } else {
            r.type = "image/svg+xml"; r.body = qrgen::toSvg(mods);
        }
    }
    else if(path == "/api/trains_full") json(apiTrainsFull());
    else if(path == "/api/segments")    return apiSegments(params["train"]);
    else if(path == "/api/stations")    json(apiStations());
    else if(path == "/api/search")      json(apiSearch(params));
    else if(path == "/api/trains"){
        auto& rs = RailwaySystem::get();
        string o = "[";
        for(size_t i = 0; i < rs.trains.size(); i++){
            auto* t = rs.trains[i].get();
            o += string(i ? "," : "") + "{\"no\":" + jstr(t->no) + ",\"name\":" + jstr(t->name) + "}";
        }
        json(o + "]");
    }
    else if(path.rfind("/web/", 0) == 0 && path.find("..") == string::npos){
        r.body = readFile(path.substr(1));
        if(r.body.empty()){ r.status = 404; json("{\"error\":\"not found\"}"); }
        else r.type = mimeFor(path);
    }
    else { r.status = 404; json("{\"error\":\"not found\"}"); }
    return r;
}

static const char* statusText(int s){
    switch(s){ case 200: return "OK"; case 204: return "No Content"; case 400: return "Bad Request";
               case 404: return "Not Found"; case 405: return "Method Not Allowed"; default: return "Error"; }
}

int main(){
    RailwaySystem::get().initSampleData();
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    int server_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8000);
    if(bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        cerr << "Cannot bind port 8000 - is another copy of the server already running?" << endl;
        return 1;
    }
    listen(server_fd, 16);
    cout << "SRM Railway C++ Backend running on http://0.0.0.0:8000" << endl;
    cout << "Open http://localhost:8000/web/index.html" << endl;

    while(true){
        int client = (int)accept(server_fd, nullptr, nullptr);
        if(client < 0) continue;

        // Browsers open speculative connections that never send anything; a read
        // timeout keeps one idle socket from freezing this single-threaded server.
#ifdef _WIN32
        DWORD tv = 3000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
        timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
        string req;
        char buffer[4096];
        while(req.find("\r\n\r\n") == string::npos && req.size() < 16384){
            int n = (int)recv(client, buffer, sizeof(buffer), 0);
            if(n <= 0) break;
            req.append(buffer, n);
        }

        HttpResp resp;
        size_t sp1 = req.find(' '), sp2 = (sp1 == string::npos) ? string::npos : req.find(' ', sp1 + 1);
        if(sp1 == string::npos || sp2 == string::npos){
            resp.status = 400; resp.type = "application/json"; resp.body = "{\"error\":\"bad request\"}";
        } else {
            string method = req.substr(0, sp1);
            string path = req.substr(sp1 + 1, sp2 - sp1 - 1), query;
            auto qm = path.find('?');
            if(qm != string::npos){ query = path.substr(qm + 1); path = path.substr(0, qm); }
            if(method == "OPTIONS"){ resp.status = 204; resp.type = "text/plain"; }
            else {
                try { resp = handleRequest(method, path, query); }
                catch(const std::exception& e){ resp.status = 500; resp.type = "application/json"; resp.body = string("{\"error\":") + jstr(e.what()) + "}"; }
            }
            if(method == "HEAD") resp.body.clear();
        }

        string http = "HTTP/1.1 " + to_string(resp.status) + " " + statusText(resp.status) + "\r\n";
        http += "Content-Type: " + resp.type + "\r\n";
        http += "Access-Control-Allow-Origin: *\r\n";
        http += "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n";
        http += "Cache-Control: no-store\r\n";
        http += "Connection: close\r\n";
        http += "Content-Length: " + to_string(resp.body.size()) + "\r\n\r\n";
        http += resp.body;

        size_t sent = 0;
        while(sent < http.size()){
            int n = (int)send(client, http.c_str() + sent, (int)(http.size() - sent), 0);
            if(n <= 0) break;
            sent += n;
        }
#ifdef _WIN32
        closesocket(client);
#else
        close(client);
#endif
    }
    return 0;
}
