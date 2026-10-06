#include "XocDiaGame.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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

bool XocDiaOutcome::isEven() const
{
    return redCount % 2 == 0;
}

XocDiaGame::XocDiaGame(double probabilityRed)
    : probabilityRed_(probabilityRed)
{
    if (probabilityRed < 0.0 || probabilityRed > 1.0) {
        throw std::invalid_argument("probability red must be in [0, 1]");
    }
}

XocDiaOutcome XocDiaGame::shake(std::mt19937& rng) const
{
    std::bernoulli_distribution coin(probabilityRed_);
    int redCount = 0;
    for (int i = 0; i < 4; ++i) {
        if (coin(rng)) {
            ++redCount;
        }
    }
    return XocDiaOutcome{redCount};
}

bool XocDiaGame::isWinningBet(BetSide side, const XocDiaOutcome& outcome, std::mt19937& rng) const
{
    if (side == BetSide::Random) {
        std::bernoulli_distribution chooseEven(0.5);
        side = chooseEven(rng) ? BetSide::Even : BetSide::Odd;
    }
    return (side == BetSide::Even) == outcome.isEven();
}

double XocDiaGame::evenProbability() const
{
    const double theta = probabilityRed_;
    const double q = 1.0 - theta;
    return std::pow(q, 4) + 6.0 * theta * theta * q * q + std::pow(theta, 4);
}

double XocDiaGame::oddProbability() const
{
    return 1.0 - evenProbability();
}

double XocDiaGame::probabilityRed() const
{
    return probabilityRed_;
}

BetSide parseBetSide(const std::string& value)
{
    const std::string side = lowerCopy(value);
    if (side == "even" || side == "chan") {
        return BetSide::Even;
    }
    if (side == "odd" || side == "le") {
        return BetSide::Odd;
    }
    if (side == "random" || side == "rand") {
        return BetSide::Random;
    }
    throw std::invalid_argument("unknown bet side: " + value);
}

std::string betSideName(BetSide side)
{
    switch (side) {
    case BetSide::Even:
        return "even";
    case BetSide::Odd:
        return "odd";
    case BetSide::Random:
        return "random";
    }
    return "unknown";
}
