#include "Simulator.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <stdexcept>

namespace {
void validateConfig(const SimulationConfig& config)
{
    if (config.sessions <= 0 || config.rounds <= 0) {
        throw std::invalid_argument("players and rounds must be positive");
    }
    if (config.initialBankroll <= 0.0 || config.baseBet <= 0.0) {
        throw std::invalid_argument("bankroll and base bet must be positive");
    }
    if (config.payoutMultiplier < 0.0) {
        throw std::invalid_argument("payout multiplier must be non-negative");
    }
    if (config.probabilityRed < 0.0 || config.probabilityRed > 1.0) {
        throw std::invalid_argument("probability red must be in [0, 1]");
    }
}

double mean(const std::vector<double>& values)
{
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values[mid];
    }
    return (values[mid - 1] + values[mid]) / 2.0;
}

double stddev(const std::vector<double>& values, double average)
{
    if (values.size() < 2) {
        return 0.0;
    }

    double sumSquares = 0.0;
    for (double value : values) {
        const double diff = value - average;
        sumSquares += diff * diff;
    }
    return std::sqrt(sumSquares / static_cast<double>(values.size() - 1));
}
}

Simulator::Simulator(SimulationConfig config)
    : config_(config)
{
    validateConfig(config_);
}

SummaryStats Simulator::run(const BettingStrategy& prototype) const
{
    std::vector<double> profits;
    std::vector<double> drawdowns;
    std::vector<double> wagers;
    std::vector<double> roundsPlayed;
    std::vector<double> maxBets;
    int profitableSessions = 0;
    int bankruptSessions = 0;

    profits.reserve(config_.sessions);
    drawdowns.reserve(config_.sessions);
    wagers.reserve(config_.sessions);
    roundsPlayed.reserve(config_.sessions);
    maxBets.reserve(config_.sessions);

    for (int i = 0; i < config_.sessions; ++i) {
        std::mt19937 rng(config_.seed + static_cast<std::uint32_t>(i * 9973));
        auto strategy = prototype.clone();
        const SessionResult result = runSession(*strategy, rng);

        profits.push_back(result.profit);
        drawdowns.push_back(result.maxDrawdown);
        wagers.push_back(result.totalWagered);
        roundsPlayed.push_back(static_cast<double>(result.roundsPlayed));
        maxBets.push_back(result.maxBet);
        profitableSessions += result.profit > 0.0 ? 1 : 0;
        bankruptSessions += result.bankrupt ? 1 : 0;
    }

    const double averageProfit = mean(profits);
    SummaryStats summary;
    summary.strategyName = prototype.name();
    summary.sessions = config_.sessions;
    summary.meanProfit = averageProfit;
    summary.medianProfit = median(profits);
    summary.stddevProfit = stddev(profits, averageProfit);
    summary.profitRate = 100.0 * profitableSessions / static_cast<double>(config_.sessions);
    summary.bankruptcyRate = 100.0 * bankruptSessions / static_cast<double>(config_.sessions);
    summary.averageMaxDrawdown = mean(drawdowns);
    summary.averageTotalWagered = mean(wagers);
    summary.averageRoundsPlayed = mean(roundsPlayed);
    summary.averageMaxBet = mean(maxBets);
    return summary;
}

SessionResult Simulator::runSession(BettingStrategy& strategy, std::mt19937& rng) const
{
    XocDiaGame game(config_.probabilityRed);
    strategy.reset(config_.baseBet);

    SessionResult result;
    double bankroll = config_.initialBankroll;
    double peak = bankroll;

    for (int round = 0; round < config_.rounds; ++round) {
        if (bankroll <= 0.0) {
            result.bankrupt = true;
            break;
        }

        const double bet = strategy.nextBet(bankroll, config_.tableLimit);
        if (bet <= 0.0) {
            result.bankrupt = true;
            break;
        }

        const XocDiaOutcome outcome = game.shake(rng);
        const bool win = game.isWinningBet(config_.side, outcome, rng);
        if (win) {
            bankroll += bet * config_.payoutMultiplier;
            ++result.wins;
        } else {
            bankroll -= bet;
            ++result.losses;
        }

        result.totalWagered += bet;
        result.maxBet = std::max(result.maxBet, bet);
        ++result.roundsPlayed;

        peak = std::max(peak, bankroll);
        result.maxDrawdown = std::max(result.maxDrawdown, peak - bankroll);
        strategy.recordResult(win, bet, config_.payoutMultiplier);
    }

    result.finalBankroll = bankroll;
    result.profit = bankroll - config_.initialBankroll;
    result.bankrupt = result.bankrupt || bankroll <= 0.0;
    return result;
}

void printSummaryTable(const std::vector<SummaryStats>& summaries, std::ostream& out)
{
    out << std::fixed << std::setprecision(2);
    out << std::left << std::setw(18) << "Strategy"
        << std::right << std::setw(12) << "Mean"
        << std::setw(12) << "Median"
        << std::setw(12) << "StdDev"
        << std::setw(10) << "Profit%"
        << std::setw(10) << "Ruin%"
        << std::setw(12) << "AvgDD"
        << std::setw(14) << "AvgWager"
        << std::setw(12) << "AvgMaxBet"
        << '\n';

    out << std::string(112, '-') << '\n';

    for (const SummaryStats& summary : summaries) {
        out << std::left << std::setw(18) << summary.strategyName
            << std::right << std::setw(12) << summary.meanProfit
            << std::setw(12) << summary.medianProfit
            << std::setw(12) << summary.stddevProfit
            << std::setw(10) << summary.profitRate
            << std::setw(10) << summary.bankruptcyRate
            << std::setw(12) << summary.averageMaxDrawdown
            << std::setw(14) << summary.averageTotalWagered
            << std::setw(12) << summary.averageMaxBet
            << '\n';
    }
}

void writeCsv(const std::string& path, const std::vector<SummaryStats>& summaries)
{
    if (path.empty()) {
        return;
    }

    const std::filesystem::path csvPath(path);
    if (csvPath.has_parent_path()) {
        std::filesystem::create_directories(csvPath.parent_path());
    }

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot open CSV file: " + path);
    }

    out << "strategy,players,mean_profit,median_profit,stddev_profit,profit_rate_percent,"
        << "bankruptcy_rate_percent,average_max_drawdown,average_total_wagered,"
        << "average_rounds_played,average_max_bet\n";
    out << std::fixed << std::setprecision(6);
    for (const SummaryStats& summary : summaries) {
        out << '"' << summary.strategyName << '"' << ','
            << summary.sessions << ','
            << summary.meanProfit << ','
            << summary.medianProfit << ','
            << summary.stddevProfit << ','
            << summary.profitRate << ','
            << summary.bankruptcyRate << ','
            << summary.averageMaxDrawdown << ','
            << summary.averageTotalWagered << ','
            << summary.averageRoundsPlayed << ','
            << summary.averageMaxBet << '\n';
    }
}
