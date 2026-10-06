#pragma once

#include <random>
#include <string>

enum class BetSide {
    Even,
    Odd,
    Random
};

struct XocDiaOutcome {
    int redCount = 0;

    bool isEven() const;
};

class XocDiaGame {
public:
    explicit XocDiaGame(double probabilityRed = 0.5);

    XocDiaOutcome shake(std::mt19937& rng) const;
    bool isWinningBet(BetSide side, const XocDiaOutcome& outcome, std::mt19937& rng) const;
    double evenProbability() const;
    double oddProbability() const;
    double probabilityRed() const;

private:
    double probabilityRed_ = 0.5;
};

BetSide parseBetSide(const std::string& value);
std::string betSideName(BetSide side);
