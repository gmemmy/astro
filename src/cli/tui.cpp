#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <span>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <filesystem>
#include <ctime>
#include <fstream>

#include "astro/core/chain.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"
#include "astro/core/block.hpp"
#include "astro/core/miner.hpp"
#include "astro/storage/block_store.hpp"

using namespace astro::core;

namespace tui {

static bool g_running = true;

struct TermiosGuard {
  termios orig{};
  bool ok{false};
  int orig_flags{-1};
  TermiosGuard() {
    if (tcgetattr(STDIN_FILENO, &orig) == 0) {
      termios raw = orig;
      raw.c_lflag &= ~(ICANON | ECHO);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) ok = true;
    }
    orig_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (orig_flags != -1) {
      fcntl(STDIN_FILENO, F_SETFL, orig_flags | O_NONBLOCK);
    }
  }
  ~TermiosGuard() {
    if (ok) tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    if (orig_flags != -1) {
      fcntl(STDIN_FILENO, F_SETFL, orig_flags);
    }
  }
};

inline void write_str(const char* s) { ::write(STDOUT_FILENO, s, std::strlen(s)); }
inline void write_str(const std::string& s) { ::write(STDOUT_FILENO, s.c_str(), s.size()); }

inline void clear()         { write_str("\x1b[2J"); }
inline void home()          { write_str("\x1b[H"); }
inline void hide_cursor()   { write_str("\x1b[?25l"); }
inline void show_cursor()   { write_str("\x1b[?25h"); }
inline void alt_screen_on() { write_str("\x1b[?1049h"); }
inline void alt_screen_off(){ write_str("\x1b[?1049l"); }
inline void reset()         { write_str("\x1b[0m"); }
inline void fg(int code)    { char buf[16]; std::snprintf(buf, sizeof(buf), "\x1b[%dm", code); write_str(buf); }
inline void move(int r,int c){ char buf[32]; std::snprintf(buf, sizeof(buf), "\x1b[%d;%dH", r, c); write_str(buf); }

struct ScreenGuard {
  ScreenGuard() { alt_screen_on(); hide_cursor(); }
  ~ScreenGuard() { show_cursor(); alt_screen_off(); }
};

inline void box(int r1,int c1,int r2,int c2){
  move(r1,c1); write_str("┌"); for(int c=c1+1;c<c2;c++) write_str("─"); write_str("┐");
  for(int r=r1+1;r<r2;r++){ move(r,c1); write_str("│"); move(r,c2); write_str("│"); }
  move(r2,c1); write_str("└"); for(int c=c1+1;c<c2;c++) write_str("─"); write_str("┘");
}

inline int read_key() {
  unsigned char ch;
  int n = ::read(STDIN_FILENO, &ch, 1);
  if (n != 1) return -1;
  if (ch == 0x1b) {
    while (true) {
      unsigned char d;
      int n2 = ::read(STDIN_FILENO, &d, 1);
      if (n2 != 1) break;
    }
    return 1000;
  }
  return ch;
}

inline void drain_input() {
  while (true) {
    int k = read_key();
    if (k == -1) break;
  }
}

struct FPS {
  std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
  double avg = 0.0;
  void tick() {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last).count();
    last = now;
    double inst = dt > 0 ? (1.0/dt) : 0.0;
    avg = (avg*0.9) + (inst*0.1);
  }
};

struct KeyDebounce {
  int last_key = -1;
  std::chrono::steady_clock::time_point last_time = std::chrono::steady_clock::now();
  int min_interval_ms = 200;
  bool allow(int key) {
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
    if (key == last_key && delta < min_interval_ms) return false;
    last_key = key;
    last_time = now;
    return true;
  }
};

struct Spinner {
  const char* frames = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
  size_t i = 0;
  char next() { char ch = frames[i]; i = (i + 1) % 10; return ch; }
};

}

static std::string short_hash(const Hash256& h, size_t keep = 10) {
  auto hex = to_hex(std::span<const uint8_t>(h.data(), h.size()));
  if (hex.size() <= keep) return hex;
  return hex.substr(0, keep) + "…";
}

struct LogLine { std::string text; int color = 37; };

struct MiningState {
  std::atomic<bool> mining{false};
  std::atomic<bool> cancel{false};
  std::atomic<bool> done{false};

  std::atomic<uint64_t> attempts{0};
  std::atomic<uint32_t> last_lz{0};
  std::atomic<double>   last_rate{0.0};

  // Snapshots for post-completion display
  std::atomic<uint64_t> snap_attempts{0};
  std::atomic<uint32_t> snap_lz{0};
  std::atomic<double>   snap_rate{0.0};
  std::atomic<bool>     has_recent_result{false};
  std::chrono::steady_clock::time_point last_done_time{};

  std::string last_hash_short;
  std::mutex  mu;

  Block mined_block;
  std::thread worker;
};

struct App {
  Chain chain;
  astro::storage::BlockStore store{std::filesystem::path("./data")};
  uint32_t ui_difficulty_bits = 16;
  std::vector<LogLine> log;
  size_t max_log = 200;

  size_t chain_scroll = 0;
  size_t log_scroll = 0;
  bool dirty = true;

  MiningState mining;

  std::string toast_text;
  int toast_color = 33;
  std::chrono::steady_clock::time_point toast_until{};

  void push_log(std::string s, int color=37) {
    // prefix HH:MM:SS
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char tb[16]; std::strftime(tb, sizeof(tb), "%H:%M:%S", &tm);
    log.push_back({std::string("[") + tb + "] " + std::move(s), color});
    if (log.size() > max_log) log.erase(log.begin(), log.begin()+ (log.size()-max_log));
    dirty = true;
  }

  void toast(std::string s, int color=33, double seconds=4.0) {
    toast_text = std::move(s);
    toast_color = color;
    toast_until = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(seconds*1000));
    dirty = true;
  }
};

static uint64_t now_sec() {
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

static bool do_genesis(App& app) {
  if (app.chain.height() > 0) {
    app.push_log("genesis already exists", 33);
    return false;
  }
  tui::drain_input();
  uint64_t unix_time = now_sec();
  Block genesis_block = make_genesis_block("Astro: Born from bytes.", unix_time);
  auto validation_result = app.chain.append_and_store(genesis_block, app.store);
  if (validation_result.is_valid) {
    app.push_log("genesis appended ✓", 32);
    app.toast("Genesis created", 32, 4.0);
    app.dirty = true;
    return true;
  } else {
    app.push_log("genesis append failed", 31);
    app.toast("Genesis failed", 31, 4.0);
    return false;
  }
}

static bool do_append_signed_block(App& app) {
  auto tip = app.chain.tip();
  if (!tip) { app.push_log("cannot append: chain empty (create genesis first)", 33); return false; }

  auto key_pair = generate_ec_keypair();
  Transaction transaction;
  transaction.version = 1;
  transaction.nonce   = 1 + (tip->transactions.empty() ? 0 : tip->transactions.back().nonce);
  transaction.amount  = 42;
  transaction.from_pub_pem = key_pair.pubkey_pem;
  transaction.to_label = "darth vader";
  transaction.sign(key_pair.privkey_pem);

  uint64_t unix_time = now_sec();
  Block new_block = app.chain.build_block_from_transactions({transaction}, unix_time);
  auto validation_result = app.chain.append_and_store(new_block, app.store);
  if (validation_result.is_valid) { app.push_log("block appended ✓", 32); app.toast("Block appended", 32, 4.0); return true; }
  app.push_log("append failed (validation error)", 31);
  app.toast("Append failed", 31, 4.0);
  return false;
}

static void do_inspect_tip(App& app) {
  auto tip = app.chain.tip();
  if (!tip) { app.push_log("no tip (empty chain)", 33); return; }
  auto header_hash = tip->header.hash();
  app.push_log("tip: h=" + short_hash(header_hash) + " txs=" + std::to_string(tip->transactions.size()), 36);
}

static void start_mining(App& app) {
  if (app.mining.mining.load()) {
    app.push_log("mining already in progress", 33);
    return;
  }
  auto tip = app.chain.tip();
  if (!tip) {
    app.push_log("cannot mine: chain empty (create genesis first)", 33);
    return;
  }
  app.mining.cancel.store(false);
  app.mining.done.store(false);
  app.mining.attempts.store(0);
  app.mining.last_lz.store(0);
  app.mining.last_rate.store(0.0);
  app.mining.snap_attempts.store(0);
  app.mining.snap_lz.store(0);
  app.mining.snap_rate.store(0.0);
  app.mining.has_recent_result.store(false);
  {
    std::lock_guard<std::mutex> lk(app.mining.mu);
    app.mining.last_hash_short.clear();
  }
  app.mining.mining.store(true);
  app.push_log(std::string("mining started (difficulty ") + std::to_string(app.ui_difficulty_bits) + " bits)", 36);
  app.toast("Mining started", 36, 3.0);

  // simple tx to include
  auto kp = generate_ec_keypair();
  Transaction tx;
  tx.version = 1;
  tx.nonce   = 1;
  tx.amount  = 1;
  tx.from_pub_pem = kp.pubkey_pem;
  tx.to_label = "miner-reward";
  tx.sign(kp.privkey_pem);
  std::vector<Transaction> txs{tx};

  uint32_t difficulty = app.ui_difficulty_bits;
  const Chain* chain_ptr = &app.chain;
  MiningState* ms = &app.mining;

  ms->worker = std::thread([chain_ptr, ms, difficulty, txs]() mutable {
    auto t0 = std::chrono::steady_clock::now();
    auto on_progress = [ms, t0](uint64_t attempts, uint32_t lz, const std::string& hash_hex) {
      ms->attempts.store(attempts);
      ms->last_lz.store(lz);
      double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      if (dt <= 0) dt = 1e-9;
      double rate = attempts / dt;
      ms->last_rate.store(rate);
      std::lock_guard<std::mutex> lk(ms->mu);
      ms->last_hash_short = hash_hex.size() > 10 ? (hash_hex.substr(0, 10) + "...") : hash_hex;
    };
    try {
      Block mined = mine_block(*chain_ptr, std::move(txs), difficulty, ms->cancel, on_progress, 50'000);
      {
        std::lock_guard<std::mutex> lk(ms->mu);
        ms->mined_block = mined;
        ms->snap_attempts.store(ms->attempts.load());
        ms->snap_lz.store(ms->last_lz.load());
        ms->snap_rate.store(ms->last_rate.load());
        ms->has_recent_result.store(true);
        ms->last_done_time = std::chrono::steady_clock::now();
      }
      ms->done.store(true);
    } catch (...) {
      // cancelled or error
    }
    ms->mining.store(false);
  });
}

static void stop_mining(App& app) {
  if (!app.mining.mining.load()) return;
  app.mining.cancel.store(true);
  app.push_log("mining cancel requested", 33);
}

static bool do_clear_store(App& app) {
  // Stop mining and join worker if running
  if (app.mining.mining.load()) {
    app.mining.cancel.store(true);
    if (app.mining.worker.joinable()) app.mining.worker.join();
    app.mining.mining.store(false);
  }
  // Truncate the log file
  try {
    std::ofstream trunc(app.store.log_path(), std::ios::binary | std::ios::trunc);
    if (!trunc) {
      app.push_log("clear store: open failed", 31);
      app.toast("Clear store failed (open)", 31, 4.0);
      return false;
    }
    trunc.close();
  } catch (...) {
    app.push_log("clear store: exception", 31);
    app.toast("Clear store exception", 31, 4.0);
    return false;
  }
  // Reset in-memory chain
  Chain new_chain(app.chain.config());
  app.chain = std::move(new_chain);
  app.log_scroll = 0;
  app.push_log("store cleared; chain reset", 33);
  app.toast("Store cleared", 33, 4.0);
  app.dirty = true;
  return true;
}

static void draw(App& app, int rows, int cols, tui::FPS& fps) {
  using namespace tui;
  auto human_rate = [](double rps) -> std::string {
    char buf[64];
    if (rps >= 1e9) { std::snprintf(buf, sizeof(buf), "%.2f GH/s", rps/1e9); }
    else if (rps >= 1e6) { std::snprintf(buf, sizeof(buf), "%.2f MH/s", rps/1e6); }
    else if (rps >= 1e3) { std::snprintf(buf, sizeof(buf), "%.2f KH/s", rps/1e3); }
    else { std::snprintf(buf, sizeof(buf), "%.0f H/s", rps); }
    return std::string(buf);
  };
  auto human_duration = [](double s) -> std::string {
    if (s < 120.0) {
      char b[32]; std::snprintf(b, sizeof(b), "%.1fs", s); return b;
    }
    int64_t total = (int64_t)std::llround(s);
    int64_t mins = total / 60, sec = total % 60;
    if (mins < 60) {
      char b[32]; std::snprintf(b, sizeof(b), "%lldm%02llds", (long long)mins, (long long)sec); return b;
    }
    int64_t hrs = mins / 60; mins = mins % 60;
    if (hrs < 48) {
      char b[32]; std::snprintf(b, sizeof(b), "%lldh%02lldm", (long long)hrs, (long long)mins); return b;
    }
    int64_t days = hrs / 24; hrs = hrs % 24;
    char b[32]; std::snprintf(b, sizeof(b), "%lldd%02lldh", (long long)days, (long long)hrs); return b;
  };
  auto draw_bar = [&](int r, int c, int w, double frac) {
    if (w <= 0) return;
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    int filled = (int)std::floor(frac * w);
    move(r, c); write_str("[");
    for (int i = 0; i < w; ++i) {
      if (i < filled) write_str("█"); else write_str(" ");
    }
    write_str("]");
  };
  clear(); home();

  // Terminal title
  {
    std::string title = std::string("Astro TUI — h:") + std::to_string(app.chain.height()) + (app.mining.mining.load() ? " — Mining" : " — Idle");
    std::string osc = std::string("\x1b]0;") + title + "\x07";
    write_str(osc);
  }

  int header_h = 3;
  int footer_h = 10;
  int body_h   = rows - header_h - footer_h - 2;
  if (body_h < 6) body_h = 6;

  box(1,1, header_h, cols);
  move(1, 3); fg(36); write_str(" ASTRO "); reset();
  // Header status line and toast
  // Title and compact status
  move(2, 3); write_str("C++ Blockchain · TUI");
  // Compact status: height, mining, rate
  move(2, cols-38);
  write_str("h="); fg(32); write_str(std::to_string(app.chain.height())); reset();
  write_str(" · "); write_str(app.mining.mining.load() ? "mining" : "idle");
  write_str(" · ");
  {
    double r = app.mining.last_rate.load();
    char rb[32];
    if (r >= 1e6) std::snprintf(rb, sizeof(rb), "%.1f MH/s", r/1e6);
    else if (r >= 1e3) std::snprintf(rb, sizeof(rb), "%.1f KH/s", r/1e3);
    else std::snprintf(rb, sizeof(rb), "%.0f H/s", r);
    fg(36); write_str(rb); reset();
  }
  // FPS at far right
  move(2, cols-20); fg(2); char fpsbuf[64]; std::snprintf(fpsbuf, sizeof(fpsbuf), "fps %.1f", fps.avg); write_str(fpsbuf); reset();
  // Toast (expires)
  if (app.toast_until.time_since_epoch().count() != 0) {
    if (std::chrono::steady_clock::now() < app.toast_until) {
      std::string t = app.toast_text;
      int maxw = cols - 10;
      if ((int)t.size() > maxw) t = t.substr(0, maxw);
      move(1, cols - (int)t.size() - 3);
      fg(app.toast_color); write_str(t.c_str()); reset();
    }
  }

  int left_w = cols * 2 / 3;
  int body_top = header_h + 1;
  int body_bot = body_top + body_h;

  box(body_top, 1, body_bot, left_w);
  move(body_top, 3); fg(36); write_str(" Chain "); reset();

  auto tip = app.chain.tip();
  move(body_top+1, 3);
  if (tip) {
    auto tip_hash = tip->header.hash();
    write_str("height ");
    fg(32); write_str(std::to_string(app.chain.height())); reset();
    write_str("  tip ");
    fg(36); write_str(short_hash(tip_hash)); reset();
    write_str("  merkle ");
    fg(36); write_str(short_hash(tip->header.merkle_root)); reset();
  } else {
    fg(33); write_str("empty chain — press "); fg(37); write_str("[G]enesis"); reset();
  }

  int list_top = body_top + 3;
  int list_rows = body_h - 4;
  size_t chain_height = app.chain.height();
  size_t start = (chain_height > (size_t)list_rows) ? (chain_height - (size_t)list_rows) : 0;
  size_t visible_row_index = 0;
  for (size_t i = start; i < chain_height; ++i) {
    const Block* b = app.chain.block_at(i);
    if (!b) break;
    auto block_hash = b->header.hash();
    move(list_top + (int)visible_row_index, 3);
    if (i+1==chain_height) fg(32);
    write_str("#");
    write_str(std::to_string(i));
    write_str(" h=");
    write_str(short_hash(block_hash));
    write_str(" txs=");
    write_str(std::to_string(b->transactions.size()));
    reset();
    visible_row_index++;
    if (visible_row_index >= (size_t)list_rows) break;
  }

  box(body_top, left_w+2, body_bot, cols);
  move(body_top, left_w+4); fg(36); write_str(" Actions "); reset();
  int actions_row = body_top + 2;
  auto put_action = [&](const char* key, const char* desc, int color=37){
    move(actions_row++, left_w+4);
    tui::fg(color); write_str("["); write_str(key); write_str("] "); reset(); write_str(desc);
  };
  put_action("G", "Create genesis");
  put_action("B", "Append signed block");
  // Dim Mine if no tip
  put_action("M", "Mine PoW block", tip ? 37 : 90);
  put_action("I", "Inspect tip");
  put_action("Q", "Quit");
  actions_row++;
  move(actions_row++, left_w+4); fg(2); write_str("OpenSSL "); reset(); write_str("EVP | secp256k1 | SHA-256");
  move(actions_row++, left_w+4); fg(2); write_str("Difficulty "); reset();
  move(actions_row++, left_w+6); write_str(std::to_string(app.ui_difficulty_bits)); write_str(" bits  [ [ - ] + ]");
  move(actions_row++, left_w+4); fg(2); write_str("Status  "); reset();
  move(actions_row++, left_w+6);
  if (tip) { fg(32); write_str("tip OK"); reset(); }
  else     { fg(33); write_str("awaiting genesis"); reset(); }
  actions_row++;
  put_action("X", "Clear store (truncate log)", 31);
 
  // Mining status
  actions_row++;
  move(actions_row++, left_w+4); fg(36); write_str(" Mining "); reset();
  bool is_mining = app.mining.mining.load();
  bool is_done   = app.mining.done.load();
  move(actions_row++, left_w+6);
  static tui::Spinner spin;
  if (!tip) {
    fg(33); write_str("waiting for genesis"); reset();
  } else if (!is_mining && !is_done) {
    // show recent mining result for a short linger window
    bool show_recent = app.mining.has_recent_result.load();
    bool within_window = false;
    if (show_recent) {
      auto nowt = std::chrono::steady_clock::now();
      within_window = (std::chrono::duration<double>(nowt - app.mining.last_done_time).count() < 6.0);
      if (!within_window) {
        app.mining.has_recent_result.store(false);
      }
    }
    if (show_recent && within_window) {
      uint64_t attempts = app.mining.snap_attempts.load();
      uint32_t lz       = app.mining.snap_lz.load();
      double rate       = app.mining.snap_rate.load();
      char buf[128];
      tui::fg(32);
      std::snprintf(buf, sizeof(buf), "last: attempts=%llu lz=%u rate=%.1f KH/s",
                    (unsigned long long)attempts, lz, rate/1000.0);
      write_str(buf);
      reset();
      move(actions_row++, left_w+6);
      write_str("hash ");
      std::string hash_short;
      {
        std::lock_guard<std::mutex> lk(app.mining.mu);
        hash_short = app.mining.last_hash_short;
      }
      fg(36); write_str(hash_short); reset();
    } else {
      fg(33); write_str("idle"); reset();
    }
  } else if (is_mining) {
    char s = spin.next();
    uint64_t attempts = app.mining.attempts.load();
    uint32_t lz       = app.mining.last_lz.load();
    double rate       = app.mining.last_rate.load();
    static double decayed_peak = 0.0;
    static auto last_peak_t = std::chrono::steady_clock::now();
    auto nowt = std::chrono::steady_clock::now();
    double dtp = std::chrono::duration<double>(nowt - last_peak_t).count();
    last_peak_t = nowt;
    double half_life = 5.0;
    double decay = std::exp(-dtp / half_life);
    decayed_peak = std::max(rate, decayed_peak * decay);
    std::string hash_short;
    {
      std::lock_guard<std::mutex> lk(app.mining.mu);
      hash_short = app.mining.last_hash_short;
    }
    tui::fg(33);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%c attempts=%llu lz=%u rate=%.1f KH/s",
                  s, (unsigned long long)attempts, lz, rate/1000.0);
    write_str(buf);
    reset();
    move(actions_row++, left_w+6);
    write_str("hash ");
    fg(36); write_str(hash_short); reset();
    // Throughput bar
    int bar_w = std::max(10, cols - (left_w+10) - 6);
    move(actions_row++, left_w+6);
    double frac = (decayed_peak > 0.0) ? (rate / decayed_peak) : 0.0;
    draw_bar(actions_row-1, left_w+6, std::min(30, bar_w), frac);
    write_str(" ");
    write_str(human_rate(rate));
    write_str(" (peak ");
    write_str(human_rate(decayed_peak));
    write_str(")");
    // Probabilistic ETA
    double R = std::max(1e-9, rate);
    double invp = std::pow(2.0, (double)app.ui_difficulty_bits);
    double t50 = std::log(2.0) * invp / R;
    double t90 = std::log(10.0) * invp / R;
    move(actions_row++, left_w+6);
    write_str("ETA t50=");
    fg(36); write_str(human_duration(t50)); reset();
    write_str("  t90=");
    fg(36); write_str(human_duration(t90)); reset();
    // Best-so-far LZ
    move(actions_row++, left_w+6);
    write_str("lz=");
    fg(36); write_str(std::to_string(app.mining.last_lz.load())); reset();
  } else if (is_done) {
    fg(32); write_str("block found (pending append)"); reset();
  }

  box(body_bot+1, 1, rows, cols);
  move(body_bot+1, 3); fg(36); write_str(" Log "); reset();
  // show scroll status
  move(body_bot+1, cols-20);
  size_t total_logs = app.log.size();
  size_t max_rows = (size_t)(rows - (body_bot+1) - 1);
  size_t top_index = (app.log_scroll >= total_logs) ? 0 : (total_logs - std::min(total_logs, app.log_scroll + max_rows));

  int log_rows = rows - (body_bot+1) - 1;
  int start_log = (int)top_index;
  int log_row_index = 0;
  // indicator x/y
  char scbuf[32]; std::snprintf(scbuf, sizeof(scbuf), "%zu/%zu", app.log_scroll, total_logs);
  fg(2); write_str(scbuf); reset();
  for (size_t i = (size_t)start_log; i < total_logs; ++i) {
    move(body_bot+1 + 1 + log_row_index, 3);
    tui::fg(app.log[i].color);
    tui::write_str(app.log[i].text);
    tui::reset();
    log_row_index++;
    if (log_row_index >= log_rows) break;
  }
  fflush(stdout);
}

static void on_sigint(int){ tui::g_running = false; }
static std::atomic<bool> g_resized{false};
static void on_sigwinch(int){ g_resized.store(true); }

int main() {
  std::signal(SIGINT, on_sigint);
  std::signal(SIGWINCH, on_sigwinch);
  if (!crypto_init()) {
    fprintf(stderr, "OpenSSL init failed\n");
    return 1;
  }
  tui::ScreenGuard screen;
  tui::TermiosGuard tty;

  App app;
  tui::FPS fps;
  app.chain.restore_from_store(app.store);
  if (app.chain.height() > 0) {
    app.push_log("restored chain from ./data", 36);
  }
  app.push_log("TUI started", 36);
  app.push_log("Press G to create genesis", 33);
  int rows = 36, cols = 120;
#ifdef TIOCGWINSZ
  winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row>0 && ws.ws_col>0) {
    rows = ws.ws_row; cols = ws.ws_col;
  }
#endif

  using clock = std::chrono::steady_clock;
  auto next = clock::now();
  tui::KeyDebounce debounce;
  auto last_draw = clock::now();
  const auto min_draw_interval = std::chrono::milliseconds(120);

  while (tui::g_running) {
    if (g_resized.load()) {
      g_resized.store(false);
#ifdef TIOCGWINSZ
      winsize ws{};
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row>0 && ws.ws_col>0) {
        rows = ws.ws_row; cols = ws.ws_col;
      }
#endif
      app.dirty = true;
    }
    // if mining completed, attempt to append and persist
    if (app.mining.done.load()) {
      Block mined;
      {
        std::lock_guard<std::mutex> lk(app.mining.mu);
        mined = app.mining.mined_block;
      }
      app.mining.done.store(false);
      app.mining.mining.store(false);
      // enforce difficulty for validation
      app.chain.set_difficulty_bits(app.ui_difficulty_bits);
      auto vr = app.chain.append_and_store(mined, app.store);
      if (vr.is_valid) {
        auto hh = mined.header.hash();
        app.push_log(std::string("[✅] mined block appended h=") + short_hash(hh), 32);
        app.toast("Mined block appended", 32, 5.0);
      } else {
        app.push_log("[x] mined block rejected (validation failed)", 31);
        app.toast("Mined block rejected", 31, 5.0);
      }
    }

    for (int k; (k = tui::read_key()) != -1; ) {
      if (!debounce.allow(k)) continue;
      switch (k) {
        case 'q': case 'Q': tui::g_running = false; tui::drain_input(); break;
        case 'g': case 'G': do_genesis(app); tui::drain_input(); break;
        case 'b': case 'B': do_append_signed_block(app); tui::drain_input(); break;
        case 'i': case 'I': do_inspect_tip(app); tui::drain_input(); break;
        case 'm': case 'M': start_mining(app); tui::drain_input(); break;
        case '[': if (app.ui_difficulty_bits > 0) { app.ui_difficulty_bits--; app.toast("Difficulty -", 36, 2.0); app.dirty = true; } break;
        case ']': if (app.ui_difficulty_bits < 32) { app.ui_difficulty_bits++; app.toast("Difficulty +", 36, 2.0); app.dirty = true; } break;
        case 'j': if (app.log_scroll + 1 < app.log.size()) { app.log_scroll++; app.dirty = true; } break;
        case 'k': if (app.log_scroll > 0) { app.log_scroll--; app.dirty = true; } break;
        case 'x': case 'X': do_clear_store(app); tui::drain_input(); break;
        default: break;
      }
    }

    auto now = clock::now();
    if (app.dirty || (now - last_draw) >= min_draw_interval) {
      draw(app, rows, cols, fps);
      fps.tick();
      app.dirty = false;
      last_draw = now;
    }

    next += std::chrono::milliseconds(33);
    std::this_thread::sleep_until(next);
  }

  stop_mining(app);
  if (app.mining.worker.joinable()) app.mining.worker.join();
  crypto_shutdown();
  return 0;
}