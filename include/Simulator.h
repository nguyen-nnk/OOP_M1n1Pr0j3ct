#pragma once

#include "BettingStrategy.h"
#include "XocDiaGame.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct SimulationConfig {
    int sessions = 5000;
    int rounds = 200;
    double initialBankroll = 1000.0;
    double baseBet = 10.0;
    double payoutMultiplier = 0.96;
    double probabilityRed = 0.5;
    double tableLimit = 0.0;
    BetSide side = BetSide::Even;
    std::uint32_t seed = 42;
};

struct SessionResult {
    double finalBankroll = 0.0;
    double profit = 0.0;
    double maxDrawdown = 0.0;
    double totalWagered = 0.0;
    double maxBet = 0.0;
    int roundsPlayed = 0;
    int wins = 0;
    int losses = 0;
    bool bankrupt = false;
};

struct SummaryStats {
    std::string strategyName;
    int sessions = 0;
    double meanProfit = 0.0;
    double medianProfit = 0.0;
    double stddevProfit = 0.0;
    double profitRate = 0.0;
    double bankruptcyRate = 0.0;
    double averageMaxDrawdown = 0.0;
    double averageTotalWagered = 0.0;
    double averageRoundsPlayed = 0.0;
    double averageMaxBet = 0.0;
};

class Simulator {
public:
    explicit Simulator(SimulationConfig config);

    SummaryStats run(const BettingStrategy& prototype) const;

private:
    SessionResult runSession(BettingStrategy& strategy, std::mt19937& rng) const;

    SimulationConfig config_;
};

void printSummaryTable(const std::vector<SummaryStats>& summaries, std::ostream& out);
void writeCsv(const std::string& path, const std::vector<SummaryStats>& summaries);
