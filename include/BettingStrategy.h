#pragma once

#include <memory>
#include <string>
#include <vector>

class BettingStrategy {
public:
    virtual ~BettingStrategy() = default;

    virtual std::string name() const = 0;

    virtual void reset(double baseBet);
    virtual double nextBet(double currentBankroll, double tableLimit) = 0;
    virtual void recordResult(bool win, double amount, double payoutMultiplier) = 0;
    virtual std::unique_ptr<BettingStrategy> clone() const = 0;

protected:
    static double clampBet(double requested, double bankroll, double tableLimit);

    double baseBet_ = 1.0;
};

class FlatBetStrategy final : public BettingStrategy {
public:
    std::string name() const override;
    double nextBet(double currentBankroll, double tableLimit) override;
    void recordResult(bool win, double amount, double payoutMultiplier) override;
    std::unique_ptr<BettingStrategy> clone() const override;
};

class MartingaleStrategy final : public BettingStrategy {
public:
    void reset(double baseBet) override;
    std::string name() const override;
    double nextBet(double currentBankroll, double tableLimit) override;
    void recordResult(bool win, double amount, double payoutMultiplier) override;
    std::unique_ptr<BettingStrategy> clone() const override;

private:
    double currentBet_ = 1.0;
};

class OscarsGrindStrategy final : public BettingStrategy {
public:
    void reset(double baseBet) override;
    std::string name() const override;
    double nextBet(double currentBankroll, double tableLimit) override;
    void recordResult(bool win, double amount, double payoutMultiplier) override;
    std::unique_ptr<BettingStrategy> clone() const override;

private:
    double currentBet_ = 1.0;
    double cycleProfit_ = 0.0;
    double payoutMultiplier_ = 1.0;
};

std::unique_ptr<BettingStrategy> makeStrategy(const std::string& strategyName);
std::vector<std::unique_ptr<BettingStrategy>> makeSelectedStrategies(const std::string& strategyName);
