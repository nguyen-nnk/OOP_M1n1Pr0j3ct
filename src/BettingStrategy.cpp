#include "BettingStrategy.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}

void BettingStrategy::reset(double baseBet)
{
    if (baseBet <= 0.0) {
        throw std::invalid_argument("base bet must be positive");
    }
    baseBet_ = baseBet;
}

double BettingStrategy::clampBet(double requested, double bankroll, double tableLimit)
{
    if (requested <= 0.0 || bankroll <= 0.0) {
        return 0.0;
    }

    double capped = std::min(requested, bankroll);
    if (tableLimit > 0.0) {
        capped = std::min(capped, tableLimit);
    }
    return capped;
}

std::string FlatBetStrategy::name() const
{
    return "Flatbet";
}

double FlatBetStrategy::nextBet(double currentBankroll, double tableLimit)
{
    return clampBet(baseBet_, currentBankroll, tableLimit);
}

void FlatBetStrategy::recordResult(bool, double, double)
{
}

std::unique_ptr<BettingStrategy> FlatBetStrategy::clone() const
{
    return std::make_unique<FlatBetStrategy>(*this);
}

void MartingaleStrategy::reset(double baseBet)
{
    BettingStrategy::reset(baseBet);
    currentBet_ = baseBet_;
}

std::string MartingaleStrategy::name() const
{
    return "Martingale";
}

double MartingaleStrategy::nextBet(double currentBankroll, double tableLimit)
{
    return clampBet(currentBet_, currentBankroll, tableLimit);
}

void MartingaleStrategy::recordResult(bool win, double amount, double)
{
    if (win) {
        currentBet_ = baseBet_;
    } else {
        currentBet_ = std::max(baseBet_, amount * 2.0);
    }
}

std::unique_ptr<BettingStrategy> MartingaleStrategy::clone() const
{
    return std::make_unique<MartingaleStrategy>(*this);
}

void OscarsGrindStrategy::reset(double baseBet)
{
    BettingStrategy::reset(baseBet);
    currentBet_ = baseBet_;
    cycleProfit_ = 0.0;
    payoutMultiplier_ = 1.0;
}

std::string OscarsGrindStrategy::name() const
{
    return "Oscar's Grind";
}

double OscarsGrindStrategy::nextBet(double currentBankroll, double tableLimit)
{
    const double remainingProfit = baseBet_ - cycleProfit_;
    const double capToGoal = remainingProfit > 0.0
        ? remainingProfit / std::max(0.000001, payoutMultiplier_)
        : baseBet_;
    return clampBet(std::min(currentBet_, capToGoal), currentBankroll, tableLimit);
}

void OscarsGrindStrategy::recordResult(bool win, double amount, double payoutMultiplier)
{
    payoutMultiplier_ = payoutMultiplier;
    if (win) {
        cycleProfit_ += amount * payoutMultiplier;
        if (cycleProfit_ >= baseBet_) {
            currentBet_ = baseBet_;
            cycleProfit_ = 0.0;
        } else {
            currentBet_ = amount + baseBet_;
        }
    } else {
        cycleProfit_ -= amount;
        currentBet_ = std::max(baseBet_, amount);
    }
}

std::unique_ptr<BettingStrategy> OscarsGrindStrategy::clone() const
{
    return std::make_unique<OscarsGrindStrategy>(*this);
}

std::unique_ptr<BettingStrategy> makeStrategy(const std::string& strategyName)
{
    const std::string name = lowerCopy(strategyName);
    if (name == "flat" || name == "flatbet" || name == "flat-bet") {
        return std::make_unique<FlatBetStrategy>();
    }
    if (name == "martingale") {
        return std::make_unique<MartingaleStrategy>();
    }
    if (name == "oscars" || name == "oscar" || name == "oscarsgrind"
        || name == "oscargrind" || name == "oscar's grind") {
        return std::make_unique<OscarsGrindStrategy>();
    }
    throw std::invalid_argument("unknown strategy: " + strategyName);
}

std::vector<std::unique_ptr<BettingStrategy>> makeSelectedStrategies(const std::string& strategyName)
{
    const std::string name = lowerCopy(strategyName);
    std::vector<std::unique_ptr<BettingStrategy>> strategies;
    if (name == "all") {
        strategies.push_back(std::make_unique<FlatBetStrategy>());
        strategies.push_back(std::make_unique<MartingaleStrategy>());
        strategies.push_back(std::make_unique<OscarsGrindStrategy>());
        return strategies;
    }

    strategies.push_back(makeStrategy(strategyName));
    return strategies;
}
