#include "BettingStrategy.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
void expectNear(double actual, double expected, const std::string& message)
{
    if (std::fabs(actual - expected) > 1e-9) {
        std::cerr << "FAIL: " << message << ": expected " << expected
                  << ", got " << actual << '\n';
        std::exit(1);
    }
}

void testFlatbet()
{
    FlatBetStrategy strategy;
    strategy.reset(10.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "flatbet starts at base");
    strategy.recordResult(false, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "flatbet ignores loss");
    strategy.recordResult(true, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "flatbet ignores win");
}

void testMartingale()
{
    MartingaleStrategy strategy;
    strategy.reset(10.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "martingale starts at base");
    strategy.recordResult(false, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 20.0, "martingale doubles after loss");
    strategy.recordResult(false, 20.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 40.0, "martingale doubles repeatedly");
    strategy.recordResult(true, 40.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "martingale resets after win");
}

void testOscarsGrind()
{
    OscarsGrindStrategy strategy;
    strategy.reset(10.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "oscars starts at base");
    strategy.recordResult(false, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "oscars keeps bet after loss");
    strategy.recordResult(true, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "oscars caps bet to one-unit cycle goal");
    strategy.recordResult(true, 10.0, 1.0);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.0, "oscars resets after one-unit cycle profit");

    strategy.reset(10.0);
    strategy.recordResult(false, 10.0, 0.96);
    strategy.recordResult(true, 10.0, 0.96);
    expectNear(strategy.nextBet(1000.0, 0.0), 10.833333333333334,
        "oscars accounts for payout when capping to cycle goal");
}
}

int main()
{
    testFlatbet();
    testMartingale();
    testOscarsGrind();
    std::cout << "strategy tests passed\n";
    return 0;
}
