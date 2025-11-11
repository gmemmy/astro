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
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "astro/core/chain.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"
#include "astro/core/block.hpp"

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

} // namespace tui

static std::string short_hash(const Hash256& h, size_t keep = 10) {
  auto hex = to_hex(std::span<const uint8_t>(h.data(), h.size()));
  if (hex.size() <= keep) return hex;
  return hex.substr(0, keep) + "…";
}

struct LogLine { std::string text; int color = 37; };

struct App {
  Chain chain;
  std::vector<LogLine> log;
  size_t max_log = 200;

  size_t chain_scroll = 0;

  void push_log(std::string s, int color=37) {
    log.push_back({std::move(s), color});
    if (log.size() > max_log) log.erase(log.begin(), log.begin()+ (log.size()-max_log));
  }
};

static bool do_genesis(App& app) {
  if (app.chain.height() > 0) {
    app.push_log("genesis already exists", 33);
    return false;
  }
  tui::drain_input();
  uint64_t unix_time = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  Block genesis_block = make_genesis_block("Astro: Born from bytes.", unix_time);
  auto validation_result = app.chain.append_block(genesis_block);
  if (validation_result.is_valid) {
    app.push_log("genesis appended ✓", 32);
    return true;
  } else {
    app.push_log("genesis append failed", 31);
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

  uint64_t unix_time = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  Block new_block = app.chain.build_block_from_transactions({transaction}, unix_time);
  auto validation_result = app.chain.append_block(new_block);
  if (validation_result.is_valid) { app.push_log("block appended ✓", 32); return true; }
  app.push_log("append failed (validation error)", 31);
  return false;
}

static void do_inspect_tip(App& app) {
  auto tip = app.chain.tip();
  if (!tip) { app.push_log("no tip (empty chain)", 33); return; }
  auto header_hash = tip->header.hash();
  app.push_log("tip: h=" + short_hash(header_hash) + " txs=" + std::to_string(tip->transactions.size()), 36);
}


static void draw(App& app, int rows, int cols, tui::FPS& fps) {
  using namespace tui;
  clear(); home();

  int header_h = 3;
  int footer_h = 10;
  int body_h   = rows - header_h - footer_h - 2;
  if (body_h < 6) body_h = 6;

  box(1,1, header_h, cols);
  move(1, 3); fg(36); write_str(" ASTRO "); reset();
  move(2, 3); write_str("C++ Blockchain · TUI");
  move(2, cols-20); fg(2); char fpsbuf[64]; std::snprintf(fpsbuf, sizeof(fpsbuf), "fps %.1f", fps.avg); write_str(fpsbuf); reset();

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
  put_action("I", "Inspect tip");
  put_action("Q", "Quit");
  actions_row++;
  move(actions_row++, left_w+4); fg(2); write_str("OpenSSL "); reset(); write_str("EVP | secp256k1 | SHA-256");
  move(actions_row++, left_w+4); fg(2); write_str("Status  "); reset();
  move(actions_row++, left_w+6);
  if (tip) { fg(32); write_str("tip OK"); reset(); }
  else     { fg(33); write_str("awaiting genesis"); reset(); }

  box(body_bot+1, 1, rows, cols);
  move(body_bot+1, 3); fg(36); write_str(" Log "); reset();

  int log_rows = rows - (body_bot+1) - 1;
  int start_log = (app.log.size() > (size_t)log_rows) ? (app.log.size() - (size_t)log_rows) : 0;
  int log_row_index = 0;
  for (size_t i = start_log; i < app.log.size(); ++i) {
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

int main() {
  std::signal(SIGINT, on_sigint);
  if (!crypto_init()) {
    fprintf(stderr, "OpenSSL init failed\n");
    return 1;
  }
  tui::ScreenGuard screen;
  tui::TermiosGuard tty;

  App app;
  tui::FPS fps;
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

  while (tui::g_running) {
    for (int k; (k = tui::read_key()) != -1; ) {
      if (!debounce.allow(k)) continue;
      switch (k) {
        case 'q': case 'Q': tui::g_running = false; tui::drain_input(); break;
        case 'g': case 'G': do_genesis(app); tui::drain_input(); break;
        case 'b': case 'B': do_append_signed_block(app); tui::drain_input(); break;
        case 'i': case 'I': do_inspect_tip(app); tui::drain_input(); break;
        default: break;
      }
    }

    draw(app, rows, cols, fps);
    fps.tick();

    next += std::chrono::milliseconds(33);
    std::this_thread::sleep_until(next);
  }

  crypto_shutdown();
  return 0;
}