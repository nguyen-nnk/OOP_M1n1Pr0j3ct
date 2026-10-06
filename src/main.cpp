#include "BettingStrategy.h"
#include "Simulator.h"
#include "XocDiaGame.h"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <ncurses.h>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
struct ProgramOptions {
    SimulationConfig config;
    std::string strategy = "all";
    std::string csvPath;
    bool interactive = true;
    bool help = false;
};

enum class Language {
    English,
    Vietnamese
};

struct StepRound {
    int round = 0;
    int redCount = 0;
    bool win = false;
    double bet = 0.0;
    double profit = 0.0;
    double bankroll = 0.0;
};

struct StepSession {
    bool active = false;
    int round = 0;
    double bankroll = 0.0;
    double peak = 0.0;
    double maxDrawdown = 0.0;
    double totalWagered = 0.0;
    double maxBet = 0.0;
    int wins = 0;
    int losses = 0;
    bool finished = false;
    bool lastWin = false;
    int lastRedCount = -1;
    double lastBet = 0.0;
    double lastProfit = 0.0;
    std::vector<StepRound> history;
    std::string strategyName;
    std::unique_ptr<BettingStrategy> strategy;
    std::mt19937 rng;
};

struct RunRecord {
    int runId = 0;
    SummaryStats summary;
};

struct TuiState {
    ProgramOptions options;
    std::vector<SummaryStats> summaries;
    std::vector<RunRecord> runHistory;
    std::string message;
    std::string secretBuffer;
    int selectedField = 0;
    int sparkle = 0;
    bool showHelp = false;
    bool luckyMode = false;
    Language language = Language::English;
    int runCounter = 0;
    StepSession step;
};

enum ColorPair {
    PairTitle = 1,
    PairAccent = 2,
    PairPanel = 3,
    PairSelected = 4,
    PairGood = 5,
    PairBad = 6,
    PairDim = 7,
    PairGold = 8
};

void printHelp()
{
    std::cout
        << "Xoc dia OOP simulator\n"
        << "Usage:\n"
        << "  xocdia                         Open console menu\n"
        << "  xocdia --strategy all [options]\n\n"
        << "Options:\n"
        << "  --strategy flat|martingale|oscars|all\n"
        << "  --players N        alias: --sessions\n"
        << "  --rounds N\n"
        << "  --bankroll VALUE\n"
        << "  --base VALUE\n"
        << "  --payout VALUE\n"
        << "  --prob-red VALUE\n"
        << "  --side even|odd|random\n"
        << "  --seed N\n"
        << "  --limit VALUE        0 means no explicit table limit\n"
        << "  --csv PATH\n";
}

std::string formatDouble(double value, int precision = 2)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string requireValue(int& index, int argc, char* argv[])
{
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string("missing value for ") + argv[index]);
    }
    ++index;
    return argv[index];
}

ProgramOptions parseArgs(int argc, char* argv[])
{
    ProgramOptions options;
    if (argc > 1) {
        options.interactive = false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--strategy") {
            options.strategy = requireValue(i, argc, argv);
        } else if (arg == "--players" || arg == "--sessions") {
            options.config.sessions = std::stoi(requireValue(i, argc, argv));
        } else if (arg == "--rounds") {
            options.config.rounds = std::stoi(requireValue(i, argc, argv));
        } else if (arg == "--bankroll") {
            options.config.initialBankroll = std::stod(requireValue(i, argc, argv));
        } else if (arg == "--base") {
            options.config.baseBet = std::stod(requireValue(i, argc, argv));
        } else if (arg == "--payout") {
            options.config.payoutMultiplier = std::stod(requireValue(i, argc, argv));
        } else if (arg == "--prob-red") {
            options.config.probabilityRed = std::stod(requireValue(i, argc, argv));
        } else if (arg == "--side") {
            options.config.side = parseBetSide(requireValue(i, argc, argv));
        } else if (arg == "--seed") {
            options.config.seed = static_cast<std::uint32_t>(std::stoul(requireValue(i, argc, argv)));
        } else if (arg == "--limit") {
            options.config.tableLimit = std::stod(requireValue(i, argc, argv));
        } else if (arg == "--csv") {
            options.csvPath = requireValue(i, argc, argv);
        } else {
            throw std::invalid_argument("unknown option: " + arg);
        }
    }

    return options;
}

std::vector<SummaryStats> runSimulation(const ProgramOptions& options);

bool isVi(Language language)
{
    return language == Language::Vietnamese;
}

std::string text(Language language, const std::string& en, const std::string& vi)
{
    return isVi(language) ? vi : en;
}

std::string strategyLabel(const std::string& strategy, Language language)
{
    if (strategy == "flat") {
        return "Flatbet";
    }
    if (strategy == "martingale") {
        return "Martingale";
    }
    if (strategy == "oscars") {
        return "Oscar's Grind";
    }
    return text(language, "All strategies", "Tất cả chiến thuật");
}

std::string strategyDescription(const std::string& strategy, Language language)
{
    if (strategy == "flat") {
        return text(language,
            "Flatbet: always bets the same base amount; stable, simple, low drama.",
            "Flatbet: luôn cược cùng một mức cơ bản; ổn định, đơn giản, ít biến động.");
    }
    if (strategy == "martingale") {
        return text(language,
            "Martingale: doubles after each loss, then resets after a win; high recovery pressure.",
            "Martingale: thua thì gấp đôi, thắng thì về mức đầu; áp lực vốn rất cao.");
    }
    if (strategy == "oscars") {
        return text(language,
            "Oscar's Grind: increases one unit after wins and aims for one-unit cycle profit.",
            "Oscar's Grind: thắng thì tăng một đơn vị, nhắm lợi nhuận một đơn vị mỗi chu kỳ.");
    }
    return text(language,
        "All: runs Flatbet, Martingale, and Oscar's Grind side by side for comparison.",
        "Tất cả: chạy Flatbet, Martingale và Oscar's Grind cạnh nhau để so sánh.");
}

std::string runningExplanation(const std::string& strategyName, Language language)
{
    if (strategyName == "Flatbet") {
        return text(language,
            "No chase here: every preview bet stays at the base unit.",
            "Không rượt lệnh: mỗi lượt preview giữ mức cược cơ bản.");
    }
    if (strategyName == "Martingale") {
        return text(language,
            "Watch the risk: losses make the next bet grow fast.",
            "Chú ý rủi ro: chuỗi thua làm mức cược tăng rất nhanh.");
    }
    if (strategyName == "Oscar's Grind") {
        return text(language,
            "Slow grind: wins step the bet up toward a small target.",
            "Cày chậm: thắng thì tăng cược để nhắm mục tiêu nhỏ.");
    }
    return text(language,
        "Comparing all strategies with the same game settings.",
        "Đang so sánh tất cả chiến thuật trên cùng cấu hình.");
}

std::vector<std::string> fieldLabels(const ProgramOptions& options, Language language)
{
    return {
        text(language, "Strategy      : ", "Chiến thuật   : ") + strategyLabel(options.strategy, language),
        text(language, "Players       : ", "Người chơi    : ") + std::to_string(options.config.sessions),
        text(language, "Rounds        : ", "Số lượt       : ") + std::to_string(options.config.rounds),
        text(language, "Bankroll      : ", "Vốn ban đầu   : ") + formatDouble(options.config.initialBankroll),
        text(language, "Base bet      : ", "Cược cơ bản   : ") + formatDouble(options.config.baseBet),
        text(language, "Payout        : ", "Tỷ lệ trả     : ") + formatDouble(options.config.payoutMultiplier, 4),
        text(language, "Probability R : ", "Xác suất đỏ   : ") + formatDouble(options.config.probabilityRed, 4),
        text(language, "Side          : ", "Cửa đặt       : ") + betSideName(options.config.side),
        text(language, "Seed          : ", "Seed          : ") + std::to_string(options.config.seed),
        text(language, "Table limit   : ", "Giới hạn bàn  : ") + formatDouble(options.config.tableLimit)
    };
}

void initColors()
{
    if (!has_colors()) {
        return;
    }
    start_color();
    use_default_colors();
    init_pair(PairTitle, COLOR_CYAN, -1);
    init_pair(PairAccent, COLOR_MAGENTA, -1);
    init_pair(PairPanel, COLOR_BLUE, -1);
    init_pair(PairSelected, COLOR_BLACK, COLOR_CYAN);
    init_pair(PairGood, COLOR_GREEN, -1);
    init_pair(PairBad, COLOR_RED, -1);
    init_pair(PairDim, COLOR_WHITE, -1);
    init_pair(PairGold, COLOR_YELLOW, -1);
}

void attrOn(int pair, int attrs = 0)
{
    if (has_colors()) {
        attron(COLOR_PAIR(pair));
    }
    if (attrs != 0) {
        attron(attrs);
    }
}

void attrOff(int pair, int attrs = 0)
{
    if (attrs != 0) {
        attroff(attrs);
    }
    if (has_colors()) {
        attroff(COLOR_PAIR(pair));
    }
}

void drawBorderedBox(int y, int x, int h, int w, const std::string& title)
{
    for (int row = 1; row < h - 1; ++row) {
        mvhline(y + row, x + 1, ' ', w - 2);
    }

    attrOn(PairPanel, A_BOLD);
    mvaddch(y, x, '+');
    mvhline(y, x + 1, '-', w - 2);
    mvaddch(y, x + w - 1, '+');
    mvvline(y + 1, x, '|', h - 2);
    mvvline(y + 1, x + w - 1, '|', h - 2);
    mvaddch(y + h - 1, x, '+');
    mvhline(y + h - 1, x + 1, '-', w - 2);
    mvaddch(y + h - 1, x + w - 1, '+');
    attrOff(PairPanel, A_BOLD);

    if (!title.empty() && w > 6) {
        attrOn(PairTitle, A_BOLD);
        mvprintw(y, x + 3, " %.*s ", std::max(0, w - 8), title.c_str());
        attrOff(PairTitle, A_BOLD);
    }
}

void drawBanner(int width, int tick, bool luckyMode, Language language)
{
    const std::string title = "XOC DIA STRATEGY LAB";
    const std::string subtitle = luckyMode
        ? text(language, "LUCKY MODE: the bowl is suspiciously shiny",
               "CHẾ ĐỘ MAY MẮN: cái bát sáng bóng đáng nghi")
        : text(language, "Monte Carlo, OOP, and questionable bankroll decisions",
               "Monte Carlo, OOP, và các quyết định vốn đầy drama");
    const int x = std::max(0, (width - static_cast<int>(title.size())) / 2);
    attrOn(PairTitle, A_BOLD);
    mvprintw(1, x, "%s", title.c_str());
    attrOff(PairTitle, A_BOLD);

    attrOn(luckyMode ? PairGold : PairDim);
    mvprintw(2, std::max(0, (width - static_cast<int>(subtitle.size())) / 2), "%s", subtitle.c_str());
    attrOff(luckyMode ? PairGold : PairDim);

    const char* coinFrames[] = {"(oo)  (--)  (oo)  (--)", "(--)  (oo)  (--)  (oo)"};
    attrOn(PairGold, A_BOLD);
    mvprintw(4, std::max(0, (width - 24) / 2), "%s", coinFrames[tick % 2]);
    attrOff(PairGold, A_BOLD);
}

void drawConfigPanel(const TuiState& state, int y, int x, int h, int w)
{
    drawBorderedBox(y, x, h, w, text(state.language, "CONFIG", "CẤU HÌNH"));
    const auto labels = fieldLabels(state.options, state.language);
    for (int i = 0; i < static_cast<int>(labels.size()) && i < h - 3; ++i) {
        if (i == state.selectedField) {
            attrOn(PairSelected, A_BOLD);
        }
        mvprintw(y + 2 + i, x + 2, "%-*.*s", w - 4, w - 4, labels[i].c_str());
        if (i == state.selectedField) {
            attrOff(PairSelected, A_BOLD);
        }
    }
}

void drawTheoryPanel(const TuiState& state, int y, int x, int h, int w)
{
    const SimulationConfig& config = state.options.config;
    drawBorderedBox(y, x, h, w, text(state.language, "MODEL", "MÔ HÌNH"));
    XocDiaGame game(config.probabilityRed);
    const double edgePerUnit = 0.5 * (config.payoutMultiplier - 1.0);
    mvprintw(y + 2, x + 2, "P(even): %s", formatDouble(game.evenProbability(), 4).c_str());
    mvprintw(y + 3, x + 2, "P(odd) : %s", formatDouble(game.oddProbability(), 4).c_str());
    mvprintw(y + 4, x + 2, "EV/unit: %s", formatDouble(edgePerUnit, 4).c_str());
    mvprintw(y + 6, x + 2, "%s", text(state.language, "Keys:", "Phím:").c_str());
    mvprintw(y + 7, x + 2, "%s", text(state.language, "Up/Down select", "Lên/Xuống chọn").c_str());
    mvprintw(y + 8, x + 2, "%s", text(state.language, "Left/Right tweak", "Trái/Phải chỉnh").c_str());
    mvprintw(y + 9, x + 2, "%s", text(state.language, "Enter edit", "Enter nhập giá trị").c_str());
    mvprintw(y + 10, x + 2, "%s", text(state.language, "R run, D step, L language", "R chạy, D từng bước, L ngôn ngữ").c_str());
    attrOn(PairGold, A_BOLD);
    mvprintw(y + 12, x + 2, "%s",
             text(state.language, "Guide:", "Gợi ý:").c_str());
    attrOff(PairGold, A_BOLD);
    const std::string description = strategyDescription(state.options.strategy, state.language);
    mvprintw(y + 13, x + 2, "%-*.*s", w - 4, w - 4, description.c_str());
}

void drawResultsPanel(const TuiState& state, int y, int x, int h, int w)
{
    drawBorderedBox(y, x, h, w, text(state.language, "RESULTS", "KẾT QUẢ"));
    if (state.runHistory.empty()) {
        attrOn(PairDim);
        mvprintw(y + 2, x + 2, "%s",
                 text(state.language, "No run yet. Press R and shake the bowl.",
                      "Chưa chạy. Bấm R để xóc bát.").c_str());
        attrOff(PairDim);
        return;
    }

    mvprintw(y + 2, x + 2, "%4s %-16s %9s %9s %9s %7s %7s %9s",
             "Run", "Strategy", "Mean", "Median", "StdDev", "P%", "Ruin%", "AvgDD");
    mvhline(y + 3, x + 2, '-', std::min(w - 4, 84));

    const int visibleRows = std::max(0, h - 6);
    const int start = std::max(0, static_cast<int>(state.runHistory.size()) - visibleRows);
    for (int i = start; i < static_cast<int>(state.runHistory.size()); ++i) {
        const RunRecord& record = state.runHistory[i];
        const SummaryStats& s = record.summary;
        const int row = y + 4 + (i - start);
        const int color = s.bankruptcyRate > 20.0 ? PairBad : (s.meanProfit >= 0.0 ? PairGood : PairGold);
        attrOn(color);
        mvprintw(row, x + 2, "%4d %-16.16s %9.2f %9.2f %9.2f %7.2f %7.2f %9.2f",
                 record.runId, s.strategyName.c_str(), s.meanProfit, s.medianProfit,
                 s.stddevProfit, s.profitRate, s.bankruptcyRate,
                 s.averageMaxDrawdown);
        attrOff(color);
    }

    const int latestRun = state.runHistory.back().runId;
    auto latestBegin = std::find_if(state.runHistory.begin(), state.runHistory.end(),
        [latestRun](const RunRecord& record) { return record.runId == latestRun; });
    auto safest = std::min_element(latestBegin, state.runHistory.end(),
        [](const RunRecord& a, const RunRecord& b) {
            return a.summary.bankruptcyRate < b.summary.bankruptcyRate;
        });
    if (safest != state.runHistory.end()) {
        attrOn(PairAccent, A_BOLD);
        mvprintw(y + h - 3, x + 2, "%s: %s",
                 text(state.language, "Best strategy", "Chiến thuật tốt nhất").c_str(),
                 safest->summary.strategyName.c_str());
        mvprintw(y + h - 2, x + 2, "%s",
                 text(state.language, "C clears result history.",
                      "C xóa lịch sử kết quả.").c_str());
        attrOff(PairAccent, A_BOLD);
    }
}

std::string stepStrategyKey(const std::string& selected)
{
    return selected == "all" ? "flat" : selected;
}

std::string redPattern(int redCount)
{
    if (redCount < 0) {
        return "????";
    }

    std::string pattern;
    for (int i = 0; i < 4; ++i) {
        pattern += i < redCount ? 'R' : 'W';
    }
    return pattern;
}

void printClipped(int y, int x, int width, const std::string& value)
{
    if (width <= 0) {
        return;
    }
    mvprintw(y, x, "%-*.*s", width, width, value.c_str());
}

void drawStepOverlay(const TuiState& state)
{
    if (!state.step.active) {
        return;
    }

    const int h = std::min(LINES - 4, 24);
    const int w = std::min(COLS - 4, 100);
    const int y = std::max(1, (LINES - h) / 2);
    const int x = std::max(1, (COLS - w) / 2);
    drawBorderedBox(y, x, h, w, text(state.language, "STEP-BY-STEP SESSION", "PHIÊN MÔ PHỎNG TỪNG BƯỚC"));

    const StepSession& step = state.step;
    const int left = x + 3;
    const int right = x + w - 38;
    const int historyWidth = std::max(42, right - left - 2);
    const int infoWidth = std::max(28, x + w - right - 2);

    attrOn(PairTitle, A_BOLD);
    mvprintw(y + 2, left, "%s: %s",
             text(state.language, "Strategy", "Chiến thuật").c_str(),
             step.strategyName.c_str());
    attrOff(PairTitle, A_BOLD);

    mvprintw(y + 4, left, "%s: %d / %d",
             text(state.language, "Round", "Lượt").c_str(),
             step.round, state.options.config.rounds);
    mvprintw(y + 5, left, "%s: %s",
             text(state.language, "Bankroll", "Vốn").c_str(),
             formatDouble(step.bankroll).c_str());
    mvprintw(y + 6, left, "%s: %s",
             text(state.language, "Drawdown", "Sụt giảm").c_str(),
             formatDouble(step.maxDrawdown).c_str());
    mvprintw(y + 7, left, "%s: %s",
             text(state.language, "Total wagered", "Tổng đã cược").c_str(),
             formatDouble(step.totalWagered).c_str());
    mvprintw(y + 8, left, "%s: %d / %d",
             text(state.language, "Wins/Losses", "Thắng/Thua").c_str(),
             step.wins, step.losses);

    mvprintw(y + 4, right, "%s: %s",
             text(state.language, "Last bet", "Cược vừa đặt").c_str(),
             formatDouble(step.lastBet).c_str());
    mvprintw(y + 5, right, "%s: %s",
             text(state.language, "Outcome", "Kết quả").c_str(),
             redPattern(step.lastRedCount).c_str());
    mvprintw(y + 6, right, "%s: %s",
             text(state.language, "Even/Odd", "Chẵn/Lẻ").c_str(),
             step.lastRedCount < 0 ? "-" : (step.lastRedCount % 2 == 0 ? "even" : "odd"));

    const int resultColor = step.lastRedCount < 0 ? PairDim : (step.lastWin ? PairGood : PairBad);
    attrOn(resultColor, A_BOLD);
    mvprintw(y + 7, right, "%s: %s",
             text(state.language, "Last result", "Kết quả cuối").c_str(),
             step.lastRedCount < 0 ? "-" : (step.lastWin ? "WIN" : "LOSS"));
    mvprintw(y + 8, right, "%s: %s",
             text(state.language, "Last profit", "Lời/lỗ cuối").c_str(),
             formatDouble(step.lastProfit).c_str());
    attrOff(resultColor, A_BOLD);

    const int historyY = y + 10;
    mvprintw(historyY, left, "%s", text(state.language, "Round history", "Lịch sử lượt").c_str());
    mvprintw(historyY + 1, left, "%4s %-4s %7s %8s %9s %5s",
             "#", text(state.language, "Out", "KQ").c_str(),
             text(state.language, "Bet", "Cược").c_str(),
             text(state.language, "P/L", "L/lỗ").c_str(),
             text(state.language, "Bank", "Vốn").c_str(),
             "W/L");
    mvhline(historyY + 2, left, '-', std::min(historyWidth, 48));

    const int availableRows = std::max(0, h - 17);
    const int start = std::max(0, static_cast<int>(step.history.size()) - availableRows);
    for (int i = start; i < static_cast<int>(step.history.size()); ++i) {
        const StepRound& r = step.history[i];
        const int row = historyY + 3 + (i - start);
        attrOn(r.win ? PairGood : PairBad);
        mvprintw(row, left, "%4d %-4s %7.2f %8.2f %9.2f %5s",
                 r.round, redPattern(r.redCount).c_str(), r.bet, r.profit,
                 r.bankroll, r.win ? "WIN" : "LOSS");
        attrOff(r.win ? PairGood : PairBad);
    }

    if (step.finished) {
        attrOn(PairBad, A_BOLD);
        printClipped(y + 11, right, infoWidth,
            text(state.language, "Session finished. N resets the demo.", "Đã kết thúc phiên. N để reset."));
        attrOff(PairBad, A_BOLD);
    } else {
        printClipped(y + 11, right, infoWidth,
            text(state.language, "Space: next round", "Space: lượt tiếp"));
        printClipped(y + 12, right, infoWidth,
            text(state.language, "P: autoplay 10", "P: tự động 10"));
    }
    printClipped(y + 14, right, infoWidth, "N: reset");
    printClipped(y + 15, right, infoWidth, text(state.language, "C: clear history", "C: xóa lịch sử"));
    printClipped(y + 16, right, infoWidth, text(state.language, "X: close", "X: đóng"));
}

void drawHelpOverlay(Language language)
{
    const int h = 13;
    const int w = 58;
    const int y = std::max(1, (LINES - h) / 2);
    const int x = std::max(1, (COLS - w) / 2);
    drawBorderedBox(y, x, h, w, text(language, "HELP", "TRO GIUP"));
    mvprintw(y + 2, x + 3, "%s", text(language, "R: run simulation         D: step demo",
                                      "R: chạy mô phỏng         D: demo từng bước").c_str());
    mvprintw(y + 3, x + 3, "%s", text(language, "Enter: edit field      Ctrl+D: quit",
                                      "Enter: sửa giá trị     Ctrl+D: thoát").c_str());
    mvprintw(y + 4, x + 3, "%s", text(language, "Arrows: move/tweak        C: clear results",
                                      "Mũi tên: chọn/chỉnh      C: xóa kết quả").c_str());
    mvprintw(y + 5, x + 3, "%s", text(language, "Step: Space next, P auto, C clear, X close",
                                      "Step: Space tiếp, P auto, C xóa, X đóng").c_str());
    mvprintw(y + 6, x + 3, "%s", text(language, "Fun keys:", "Phim vui:").c_str());
    mvprintw(y + 7, x + 3, "%s", text(language, "  ! randomizes seed", "  ! đổi seed ngẫu nhiên").c_str());
    mvprintw(y + 8, x + 3, "%s", text(language, "  F/M/O jump strategies", "  F/M/O chọn nhanh chiến thuật").c_str());
    mvprintw(y + 9, x + 3, "%s", text(language, "  Try the classic 10-key arcade code",
                                      "  Thu ma arcade 10 phim kinh dien").c_str());
    mvprintw(y + 11, x + 3, "%s", text(language, "L toggles language. Press any key.", "L đổi ngôn ngữ. Bấm phím bất kỳ.").c_str());
}

void drawStatus(const TuiState& state)
{
    attrOn(PairAccent);
    mvhline(LINES - 2, 0, '-', COLS);
    mvprintw(LINES - 1, 1, "%.*s", COLS - 2, state.message.c_str());
    attrOff(PairAccent);
}

void renderTui(const TuiState& state)
{
    erase();
    if (LINES < 24 || COLS < 88) {
        mvprintw(1, 2, "Terminal too small. Need at least 88x24. Current: %dx%d", COLS, LINES);
        refresh();
        return;
    }

    drawBanner(COLS, state.sparkle, state.luckyMode, state.language);
    drawConfigPanel(state, 6, 2, 15, 42);
    drawTheoryPanel(state, 6, 46, 15, COLS - 48);
    drawResultsPanel(state, 22, 2, std::max(8, LINES - 25), COLS - 4);
    drawStatus(state);
    drawStepOverlay(state);
    if (state.showHelp) {
        drawHelpOverlay(state.language);
    }
    refresh();
}

std::string promptTui(const std::string& prompt, const std::string& current)
{
    timeout(-1);
    flushinp();
    echo();
    curs_set(1);
    attrOn(PairSelected);
    mvhline(LINES - 3, 0, ' ', COLS);
    mvprintw(LINES - 3, 1, "%s [%s]: ", prompt.c_str(), current.c_str());
    attrOff(PairSelected);

    char buffer[256] = {};
    getnstr(buffer, 255);
    noecho();
    curs_set(0);
    timeout(120);
    std::string value(buffer);
    return value.empty() ? current : value;
}

void cycleStrategy(std::string& strategy, int delta)
{
    std::vector<std::string> values = {"flat", "martingale", "oscars", "all"};
    auto it = std::find(values.begin(), values.end(), strategy);
    int index = it == values.end() ? 3 : static_cast<int>(it - values.begin());
    index = (index + delta + static_cast<int>(values.size())) % static_cast<int>(values.size());
    strategy = values[index];
}

void cycleSide(BetSide& side, int delta)
{
    std::vector<BetSide> values = {BetSide::Even, BetSide::Odd, BetSide::Random};
    auto it = std::find(values.begin(), values.end(), side);
    int index = it == values.end() ? 0 : static_cast<int>(it - values.begin());
    index = (index + delta + static_cast<int>(values.size())) % static_cast<int>(values.size());
    side = values[index];
}

void tweakSelected(TuiState& state, int delta)
{
    SimulationConfig& c = state.options.config;
    switch (state.selectedField) {
    case 0:
        cycleStrategy(state.options.strategy, delta);
        break;
    case 1:
        c.sessions = std::max(1, c.sessions + delta * 500);
        break;
    case 2:
        c.rounds = std::max(1, c.rounds + delta * 25);
        break;
    case 3:
        c.initialBankroll = std::max(1.0, c.initialBankroll + delta * 100.0);
        break;
    case 4:
        c.baseBet = std::max(1.0, c.baseBet + delta * 1.0);
        break;
    case 5:
        c.payoutMultiplier = std::max(0.0, c.payoutMultiplier + delta * 0.01);
        break;
    case 6:
        c.probabilityRed = std::clamp(c.probabilityRed + delta * 0.01, 0.0, 1.0);
        break;
    case 7:
        cycleSide(c.side, delta);
        break;
    case 8:
        c.seed = static_cast<std::uint32_t>(static_cast<int>(c.seed) + delta);
        break;
    case 9:
        c.tableLimit = std::max(0.0, c.tableLimit + delta * 10.0);
        break;
    default:
        break;
    }
}

void editSelected(TuiState& state)
{
    SimulationConfig& c = state.options.config;
    try {
        switch (state.selectedField) {
        case 0:
            cycleStrategy(state.options.strategy, 1);
            break;
        case 1:
            c.sessions = std::stoi(promptTui(text(state.language, "Players", "Người chơi"), std::to_string(c.sessions)));
            break;
        case 2:
            c.rounds = std::stoi(promptTui(text(state.language, "Rounds", "Số lượt"), std::to_string(c.rounds)));
            break;
        case 3:
            c.initialBankroll = std::stod(promptTui("Bankroll", formatDouble(c.initialBankroll)));
            break;
        case 4:
            c.baseBet = std::stod(promptTui("Base bet", formatDouble(c.baseBet)));
            break;
        case 5:
            c.payoutMultiplier = std::stod(promptTui("Payout", formatDouble(c.payoutMultiplier, 4)));
            break;
        case 6:
            c.probabilityRed = std::stod(promptTui("Probability red", formatDouble(c.probabilityRed, 4)));
            break;
        case 7:
            c.side = parseBetSide(promptTui("Side even|odd|random", betSideName(c.side)));
            break;
        case 8:
            c.seed = static_cast<std::uint32_t>(std::stoul(promptTui("Seed", std::to_string(c.seed))));
            break;
        case 9:
            c.tableLimit = std::stod(promptTui("Table limit", formatDouble(c.tableLimit)));
            break;
        }
        state.message = text(state.language,
            "Updated. Press R to rerun with the new settings.",
            "Đã cập nhật. Bấm R để chạy lại với cấu hình mới.");
    } catch (const std::exception& error) {
        state.message = text(state.language, "Edit rejected: ", "Không nhận giá trị: ") + error.what();
    }
}

std::string defaultCsvPath()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return "results/tui_" + std::to_string(now) + ".csv";
}

void drawRunAnimation(const TuiState& state, const std::vector<std::unique_ptr<BettingStrategy>>& strategies)
{
    const int h = 17;
    const int w = std::min(COLS - 4, 72);
    const int y = std::max(1, (LINES - h) / 2);
    const int x = std::max(1, (COLS - w) / 2);
    const char spinner[] = {'|', '/', '-', '\\'};
    const char* bowlFrames[] = {
        "     .-========-.      o   O   o",
        "   .-==  o  O  ==-.      o   O",
        "  (==  O   o   O  ==)  O   o   ",
        "   '-==__shake__==-'     *   * "
    };
    int previewWins = 0;
    int previewLosses = 0;
    std::vector<std::string> recentPreview;

    for (int frame = 0; frame <= 24; ++frame) {
        renderTui(state);
        drawBorderedBox(y, x, h, w, text(state.language, "RUNNING MONTE CARLO", "ĐANG CHẠY MONTE CARLO"));
        attrOn(frame % 2 == 0 ? PairGold : PairAccent, A_BOLD);
        mvprintw(y + 2, x + 3, "%s", bowlFrames[frame % 4]);
        attrOff(frame % 2 == 0 ? PairGold : PairAccent, A_BOLD);
        mvprintw(y + 3, x + 3, "%s %c",
                 text(state.language, "Shaking thousands of virtual bowls", "Đang xóc hàng ngàn bát ảo").c_str(),
                 spinner[frame % 4]);

        const int barWidth = std::max(10, w - 18);
        const int filled = frame * barWidth / 24;
        mvprintw(y + 5, x + 3, "[");
        for (int i = 0; i < barWidth; ++i) {
            if (i < filled && i >= std::max(0, filled - 3)) {
                attrOn(PairGold, A_BOLD);
                addch('#');
                attrOff(PairGold, A_BOLD);
            } else {
                addch(i < filled ? '#' : '.');
            }
        }
        printw("] %3d%%", frame * 100 / 24);

        const int strategyIndex = strategies.empty()
            ? 0
            : std::min(static_cast<int>(strategies.size()) - 1,
                frame * static_cast<int>(strategies.size()) / 25);
        const std::string strategyName = strategies.empty() ? "-" : strategies[strategyIndex]->name();
        mvprintw(y + 7, x + 3, "%s: %s",
                 text(state.language, "Current strategy", "Chiến thuật hiện tại").c_str(),
                 strategyName.c_str());
        mvprintw(y + 8, x + 3, "%s: %d x %d",
                 text(state.language, "Players x rounds", "Người chơi x số lượt").c_str(),
                 state.options.config.sessions, state.options.config.rounds);

        const int totalRounds = std::max(1, state.options.config.rounds);
        const int previewRound = std::min(totalRounds, 1 + (frame * totalRounds / 24));
        const bool previewWin = ((frame * 37 + static_cast<int>(state.options.config.seed)) % 100) < 50;
        if (frame > 0) {
            previewWin ? ++previewWins : ++previewLosses;
        }
        const std::string wlLabel = text(state.language, "W/L", "T/Th");
        const std::string wlValue = state.language == Language::English
            ? (previewWin ? "W" : "L")
            : (previewWin ? "T" : "Th");
        if (frame > 0) {
            recentPreview.push_back(wlValue);
            if (recentPreview.size() > 5) {
                recentPreview.erase(recentPreview.begin());
            }
        }
        std::ostringstream recentLine;
        for (std::size_t i = 0; i < recentPreview.size(); ++i) {
            if (i > 0) {
                recentLine << ' ';
            }
            recentLine << recentPreview[i];
        }
        attrOn(previewWin ? PairGood : PairBad, A_BOLD);
        mvprintw(y + 9, x + 3, "%s: %d   %s: %s",
                 text(state.language, "Preview round", "Lượt hiện tại").c_str(),
                 previewRound,
                 wlLabel.c_str(),
                 wlValue.c_str());
        attrOff(previewWin ? PairGood : PairBad, A_BOLD);

        mvprintw(y + 10, x + 3, "%s: %d   %s: %d",
                 text(state.language, "W count", "Số thắng").c_str(),
                 previewWins,
                 text(state.language, "L count", "Số thua").c_str(),
                 previewLosses);
        mvprintw(y + 11, x + 3, "%s: [%s]",
                 text(state.language, "Last 5", "5 lượt gần nhất").c_str(),
                 recentLine.str().c_str());
        const std::string explanation = text(state.language, "Coach: ", "Người dẫn: ")
            + runningExplanation(strategyName, state.language);
        attrOn(PairDim);
        mvprintw(y + 12, x + 3, "%-*.*s", w - 6, w - 6, explanation.c_str());
        attrOff(PairDim);

        mvprintw(y + 15, x + 3, "%s",
                 text(state.language, "This is a fast visual preview; results compute next.",
                      "Đây là animation tua nhanh; kết quả được tính ngay sau đó.").c_str());
        refresh();
        napms(110);
    }
}

void runTuiSimulation(TuiState& state)
{
    try {
        state.message = text(state.language, "Running simulation...", "Đang chạy mô phỏng...");
        const auto strategies = makeSelectedStrategies(state.options.strategy);
        drawRunAnimation(state, strategies);
        state.summaries = runSimulation(state.options);
        ++state.runCounter;
        for (const SummaryStats& summary : state.summaries) {
            state.runHistory.push_back(RunRecord{state.runCounter, summary});
        }
        state.message = text(state.language,
            "Done. Results were appended. S saves CSV, C clears history.",
            "Xong. Kết quả đã được thêm vào lịch sử. S lưu CSV, C xóa lịch sử.");
    } catch (const std::exception& error) {
        state.message = text(state.language, "Simulation failed: ", "Mô phỏng lỗi: ") + error.what();
    }
}

void saveTuiCsv(TuiState& state)
{
    if (state.summaries.empty()) {
        state.message = text(state.language, "Nothing to save yet. Press R first.", "Chưa có gì để lưu. Bấm R trước.");
        return;
    }

    const std::string path = defaultCsvPath();

    try {
        writeCsv(path, state.summaries);
        state.message = text(state.language, "CSV saved to ", "Đã lưu CSV vào ") + path;
    } catch (const std::exception& error) {
        state.message = text(state.language, "CSV save failed: ", "Lưu CSV lỗi: ") + error.what();
    }
}

void resetStepSession(TuiState& state)
{
    StepSession fresh;
    fresh.active = true;
    fresh.bankroll = state.options.config.initialBankroll;
    fresh.peak = fresh.bankroll;
    fresh.strategy = makeStrategy(stepStrategyKey(state.options.strategy));
    fresh.strategy->reset(state.options.config.baseBet);
    fresh.strategyName = fresh.strategy->name();
    fresh.rng.seed(state.options.config.seed);
    state.step = std::move(fresh);
    state.message = text(state.language,
        "Step demo ready. Space advances one animated round.",
        "Demo từng bước sẵn sàng. Space để chạy một lượt.");
}

void animateStepShake(const TuiState& state)
{
    static const char* frames[] = {
        "   .-~~~~-.     ",
        "  /  SHAKE \\    ",
        "  \\__    __/    ",
        "     |__|       ",
        "    /~~~~\\      ",
        "   / SHAKE\\     ",
        "   \\______/     "
    };

    const int h = 7;
    const int w = 22;
    const int y = std::max(1, LINES / 2 - 4);
    const int x = std::max(1, COLS / 2 - 11);
    for (int i = 0; i < 8; ++i) {
        attrOn(i % 2 == 0 ? PairGold : PairAccent, A_BOLD);
        for (int row = 0; row < h; ++row) {
            mvprintw(y + row, x + (i % 2), "%-*s", w, frames[(row + i) % h]);
        }
        attrOff(i % 2 == 0 ? PairGold : PairAccent, A_BOLD);
        mvprintw(y + h, x - 2, "%s",
                 text(state.language, "shaking...", "đang xóc...").c_str());
        refresh();
        napms(70);
    }
}

void stepOnce(TuiState& state, bool animate)
{
    if (!state.step.active) {
        resetStepSession(state);
    }
    if (state.step.finished) {
        state.message = text(state.language,
            "Step session is finished. Press N to reset.",
            "Phiên từng bước đã xong. Bấm N để reset.");
        return;
    }

    StepSession& step = state.step;
    const SimulationConfig& config = state.options.config;
    if (step.bankroll <= 0.0 || step.round >= config.rounds) {
        step.finished = true;
        return;
    }

    const double bet = step.strategy->nextBet(step.bankroll, config.tableLimit);
    if (bet <= 0.0) {
        step.finished = true;
        state.message = text(state.language,
            "No valid bet remains. Session stopped.",
            "Không còn mức cược hợp lệ. Dừng phiên.");
        return;
    }

    if (animate) {
        animateStepShake(state);
    }

    XocDiaGame game(config.probabilityRed);
    const XocDiaOutcome outcome = game.shake(step.rng);
    const bool win = game.isWinningBet(config.side, outcome, step.rng);
    const double profit = win ? bet * config.payoutMultiplier : -bet;

    step.bankroll += profit;
    step.totalWagered += bet;
    step.maxBet = std::max(step.maxBet, bet);
    step.peak = std::max(step.peak, step.bankroll);
    step.maxDrawdown = std::max(step.maxDrawdown, step.peak - step.bankroll);
    step.lastBet = bet;
    step.lastProfit = profit;
    step.lastWin = win;
    step.lastRedCount = outcome.redCount;
    ++step.round;
    step.history.push_back(StepRound{
        step.round,
        outcome.redCount,
        win,
        bet,
        profit,
        step.bankroll
    });
    if (win) {
        ++step.wins;
    } else {
        ++step.losses;
    }
    step.strategy->recordResult(win, bet, config.payoutMultiplier);

    if (step.bankroll <= 0.0 || step.round >= config.rounds) {
        step.finished = true;
    }

    state.message = (win
        ? text(state.language, "Round won. Space for next round.", "Thắng lượt này. Space để tiếp tục.")
        : text(state.language, "Round lost. Space for next round.", "Thua lượt này. Space để tiếp tục."));
}

void autoplayStep(TuiState& state)
{
    for (int i = 0; i < 10 && state.step.active && !state.step.finished; ++i) {
        stepOnce(state, true);
        renderTui(state);
        napms(120);
    }
}

void recordSecret(TuiState& state, int ch)
{
    if (ch == KEY_UP) {
        state.secretBuffer += "u";
    } else if (ch == KEY_DOWN) {
        state.secretBuffer += "d";
    } else if (ch == KEY_LEFT) {
        state.secretBuffer += "l";
    } else if (ch == KEY_RIGHT) {
        state.secretBuffer += "r";
    } else if (ch == 'b' || ch == 'B') {
        state.secretBuffer += "b";
    } else if (ch == 'a' || ch == 'A') {
        state.secretBuffer += "a";
    }

    if (state.secretBuffer.size() > 10) {
        state.secretBuffer.erase(0, state.secretBuffer.size() - 10);
    }
    if (state.secretBuffer == "uuddlrlrba") {
        state.luckyMode = !state.luckyMode;
        state.options.config.payoutMultiplier = state.luckyMode ? 1.0 : 0.96;
        state.message = state.luckyMode
            ? text(state.language, "Easter egg unlocked: fair payout mode. Still not financial advice.",
                   "Đã mở easter egg: payout công bằng. Vẫn không phải lời khuyên tài chính.")
            : text(state.language, "Lucky mode off. The house edge returns.",
                   "Tắt chế độ may mắn. Lợi thế nhà cái quay lại.");
        state.secretBuffer.clear();
    }
}

void runTui(ProgramOptions& options)
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(120);
    initColors();

    TuiState state;
    state.options = options;
    state.message = text(state.language,
        "Press R to run, Enter to edit, ? for help, L for VI.",
        "Bấm R để chạy, Enter để sửa, ? trợ giúp, L đổi EN.");

    bool running = true;
    while (running) {
        renderTui(state);
        const int ch = getch();
        if (ch == ERR) {
            ++state.sparkle;
            continue;
        }

        if (state.showHelp) {
            state.showHelp = false;
            continue;
        }

        if (state.step.active) {
            switch (ch) {
            case ' ':
                stepOnce(state, true);
                break;
            case 'p':
            case 'P':
                autoplayStep(state);
                break;
            case 'n':
            case 'N':
                resetStepSession(state);
                break;
            case 'c':
            case 'C':
                state.step.history.clear();
                state.message = text(state.language,
                    "Step history cleared. Session state was kept.",
                    "Đã xóa lịch sử lượt. Trạng thái phiên vẫn giữ nguyên.");
                break;
            case 'x':
            case 'X':
                state.step = StepSession{};
                state.message = text(state.language,
                    "Step demo closed.",
                    "Đã đóng demo từng bước.");
                break;
            case '?':
                state.showHelp = true;
                break;
            case 4:
                running = false;
                break;
            default:
                break;
            }
            ++state.sparkle;
            continue;
        }

        recordSecret(state, ch);
        const int fieldCount = static_cast<int>(fieldLabels(state.options, state.language).size());
        switch (ch) {
        case 4:
            running = false;
            break;
        case KEY_UP:
            state.selectedField = (state.selectedField - 1 + fieldCount) % fieldCount;
            break;
        case KEY_DOWN:
            state.selectedField = (state.selectedField + 1) % fieldCount;
            break;
        case KEY_LEFT:
            tweakSelected(state, -1);
            break;
        case KEY_RIGHT:
            tweakSelected(state, 1);
            break;
        case '\n':
        case KEY_ENTER:
            editSelected(state);
            break;
        case 'r':
        case 'R':
            runTuiSimulation(state);
            break;
        case 's':
        case 'S':
            saveTuiCsv(state);
            break;
        case 'c':
        case 'C':
            state.runHistory.clear();
            state.summaries.clear();
            state.message = text(state.language,
                "Result history cleared.",
                "Đã xóa lịch sử kết quả.");
            break;
        case 'd':
        case 'D':
            resetStepSession(state);
            break;
        case '?':
            state.showHelp = true;
            break;
        case 'l':
        case 'L':
            state.language = isVi(state.language) ? Language::English : Language::Vietnamese;
            state.message = text(state.language,
                "Language switched to English.",
                "Đã chuyển ngôn ngữ sang Tiếng Việt.");
            break;
        case '!':
            state.options.config.seed = static_cast<std::uint32_t>(
                std::chrono::system_clock::now().time_since_epoch().count());
            state.message = text(state.language,
                "Seed randomized. Chaos, but reproducible if you write it down.",
                "Đã đổi seed ngẫu nhiên. Hỗn loạn, nhưng ghi seed là lặp lại được.");
            break;
        case 'f':
        case 'F':
            state.options.strategy = "flat";
            state.message = text(state.language,
                "Flatbet selected. Calm, boring, defensible.",
                "Đã chọn Flatbet. Điềm tĩnh, hơi chán, nhưng dễ bảo vệ.");
            break;
        case 'm':
        case 'M':
            state.options.strategy = "martingale";
            state.message = text(state.language,
                "Martingale selected. Watch the max bet column.",
                "Đã chọn Martingale. Nhìn kỹ cột max bet.");
            break;
        case 'o':
        case 'O':
            state.options.strategy = "oscars";
            state.message = text(state.language,
                "Oscar's Grind selected. Slow grind, big state.",
                "Đã chọn Oscar's Grind. Tăng chậm, nhiều trạng thái.");
            break;
        default:
            break;
        }
        ++state.sparkle;
    }

    endwin();
    options = state.options;
}

std::vector<SummaryStats> runSimulation(const ProgramOptions& options)
{
    Simulator simulator(options.config);
    std::vector<SummaryStats> summaries;

    for (const auto& strategy : makeSelectedStrategies(options.strategy)) {
        summaries.push_back(simulator.run(*strategy));
    }

    return summaries;
}

void printConfig(const SimulationConfig& config)
{
    XocDiaGame game(config.probabilityRed);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nConfig: players=" << config.sessions
              << ", rounds=" << config.rounds
              << ", bankroll=" << config.initialBankroll
              << ", base=" << config.baseBet
              << ", payout=" << config.payoutMultiplier
              << ", prob_red=" << config.probabilityRed
              << ", side=" << betSideName(config.side)
              << ", seed=" << config.seed
              << ", limit=" << config.tableLimit << '\n';
    std::cout << "Theoretical P(even)=" << game.evenProbability()
              << ", P(odd)=" << game.oddProbability() << "\n\n";
}
}

int main(int argc, char* argv[])
{
    try {
        ProgramOptions options = parseArgs(argc, argv);
        if (options.help) {
            printHelp();
            return 0;
        }

        if (options.interactive) {
            runTui(options);
            return 0;
        }

        printConfig(options.config);
        std::vector<SummaryStats> summaries = runSimulation(options);
        printSummaryTable(summaries, std::cout);

        if (!options.csvPath.empty()) {
            writeCsv(options.csvPath, summaries);
            std::cout << "\nCSV written to " << options.csvPath << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
