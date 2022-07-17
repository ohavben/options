// #include <emscripten/bind.h>
// #include <malloc.h>
#include <iostream>
#include <iomanip>
#include <string>
#include "ql/qldefines.hpp"
#include <boost/config.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/binomialengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/pricingengines/vanilla/baroneadesiwhaleyengine.hpp>
#include <ql/pricingengines/vanilla/bjerksundstenslandengine.hpp>
#include <ql/pricingengines/vanilla/batesengine.hpp>
#include <ql/pricingengines/vanilla/integralengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/mceuropeanengine.hpp>
#include <ql/pricingengines/vanilla/mcamericanengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanvasicekengine.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/models/shortrate/onefactormodels/vasicek.hpp>
#include <ql/time/date.hpp>
#include <ql/utilities/dataparsers.hpp>
#include "json.hpp"

using namespace std;
//using namespace emscripten;
using namespace QuantLib;
using json = nlohmann::json;

namespace
{
  struct optionParameters {
    Date todaysDate;
    QuantLib::Option::Type type;
    double strike;
    Date settlementDate;
    Date maturityDate;
    double underlying;
    double optionPrice;
    double dividendYield;
    double riskFreeRate;
    double timeSteps;
    json request;
  };

  std::string calcuateEuropeanOption(optionParameters &oP){

    Calendar calendar = TARGET();
    DayCounter dayCounter = Actual365Fixed();
    Settings::instance().evaluationDate() = oP.todaysDate;
    Volatility impliedVolatility(oP.optionPrice);

    oP.request["ImpliedVolatility"] = impliedVolatility;

    std::vector<Date> exerciseDates;
    for (Integer i = 1; i <= 4; i++)
      exerciseDates.push_back(oP.settlementDate + 3 * i * Months);

    ext::shared_ptr<Exercise> europeanExercise(
      new EuropeanExercise(oP.maturityDate));

    Handle<Quote> underlyingH(
      ext::shared_ptr<Quote>(
        new SimpleQuote(
          oP.underlying)));

    Handle<YieldTermStructure> flatTermStructure(
      ext::shared_ptr<YieldTermStructure>(
        new FlatForward(
          oP.settlementDate,
          oP.riskFreeRate,
          dayCounter)));

    Handle<YieldTermStructure> flatDividendTS(
      ext::shared_ptr<YieldTermStructure>(
        new FlatForward(
          oP.settlementDate,
          oP.dividendYield,
          dayCounter)));

    Handle<BlackVolTermStructure> flatVolTS(
      ext::shared_ptr<BlackVolTermStructure>(
        new BlackConstantVol(
          oP.settlementDate,
          calendar,
          impliedVolatility, 
          dayCounter)));

    ext::shared_ptr<StrikedTypePayoff> payoff(
      new PlainVanillaPayoff(
        oP.type,
        oP.strike));

    ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
      new BlackScholesMertonProcess(
        underlyingH,
        flatDividendTS,
        flatTermStructure,
        flatVolTS));

    VanillaOption europeanOption(payoff, europeanExercise);

    europeanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new AnalyticEuropeanEngine(bsmProcess)));

    oP.request["NPV"]["Black-Scholes"] = europeanOption.NPV();
    oP.request["gamma"]["Black-Scholes"] = europeanOption.gamma();
    oP.request["delta"]["Black-Scholes"] = europeanOption.delta();
    oP.request["rho"]["Black-Scholes"] = europeanOption.rho();
    oP.request["vega"]["Black-Scholes"] = europeanOption.vega();
    oP.request["theta"]["Black-Scholes"] = europeanOption.theta();
    oP.request["thetaPerDay"]["Black-Scholes"] = europeanOption.thetaPerDay();

    return oP.request.dump();
  };

  std::string calcuateAmericanOption(optionParameters &oP){

    Calendar calendar = TARGET();
    DayCounter dayCounter = Actual365Fixed();
    Settings::instance().evaluationDate() = oP.todaysDate;
    Volatility impliedVolatility(oP.optionPrice);

    oP.request["ImpliedVolatility"] = impliedVolatility;

    std::vector<Date> exerciseDates;
    for (Integer i = 1; i <= 4; i++)
      exerciseDates.push_back(oP.settlementDate + 3 * i * Months);

    ext::shared_ptr<Exercise> americanExercise(
      new AmericanExercise(
        oP.settlementDate,
        oP.maturityDate));

    Handle<Quote> underlyingH(
      ext::shared_ptr<Quote>(
        new SimpleQuote(oP.underlying)));

    Handle<YieldTermStructure> flatTermStructure(
      ext::shared_ptr<YieldTermStructure>(
        new FlatForward(
          oP.settlementDate,
          oP.riskFreeRate,
          dayCounter)));

    Handle<YieldTermStructure> flatDividendTS(
      ext::shared_ptr<YieldTermStructure>(
        new FlatForward(
          oP.settlementDate,
          oP.dividendYield,
          dayCounter)));

    Handle<BlackVolTermStructure> flatVolTS(
      ext::shared_ptr<BlackVolTermStructure>(
        new BlackConstantVol(
          oP.settlementDate,
          calendar,
          impliedVolatility,
          dayCounter)));

    ext::shared_ptr<StrikedTypePayoff> payoff(
      new PlainVanillaPayoff(oP.type, oP.strike));

    ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
      new BlackScholesMertonProcess(
        underlyingH,
        flatDividendTS,
        flatTermStructure,
        flatVolTS));

    VanillaOption americanOption(payoff, americanExercise);

    ext::shared_ptr<PricingEngine> fdengine =
      ext::make_shared<FdBlackScholesVanillaEngine>(
        bsmProcess,
        oP.timeSteps,
        oP.timeSteps - 1);

    americanOption.setPricingEngine(fdengine);
    oP.request["NPV"]["Finite-Differences"] = americanOption.NPV();
    oP.request["gamma"]["Finite-Differences"] = americanOption.gamma();
    oP.request["delta"]["Finite-Differences"] = americanOption.delta();
    oP.request["theta"]["Finite-Differences"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<JarrowRudd>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Jarrow-Rudd"] = americanOption.NPV();
    oP.request["gamma"]["Binomial-Jarrow-Rudd"] = americanOption.gamma();
    oP.request["delta"]["Binomial-Jarrow-Rudd"] = americanOption.delta();
    oP.request["theta"]["Binomial-Jarrow-Rudd"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<CoxRossRubinstein>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Cox-Ross-Rubinstein"] = americanOption.NPV();
    oP.request["gamma"]["Binomial-Cox-Ross-Rubinstein"] = americanOption.gamma();
    oP.request["delta"]["Binomial-Cox-Ross-Rubinstein"] = americanOption.delta();
    oP.request["theta"]["Binomial-Cox-Ross-Rubinstein"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<AdditiveEQPBinomialTree>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Additive-equiprobabilities"] = americanOption.NPV();
    oP.request["gamma"]["Additive-equiprobabilities"] = americanOption.gamma();
    oP.request["delta"]["Additive-equiprobabilities"] = americanOption.delta();
    oP.request["theta"]["Additive-equiprobabilities"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Trigeorgis>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Trigeorgis"] = americanOption.NPV();
    oP.request["gamma"] ["Binomial-Trigeorgis"]= americanOption.gamma();
    oP.request["delta"]["Binomial-Trigeorgis"] = americanOption.delta();
    oP.request["theta"]["Binomial-Trigeorgis"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Tian>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"] ["Binomial-Tian"]= americanOption.NPV();
    oP.request["gamma"]["Binomial-Tian"] = americanOption.gamma();
    oP.request["delta"]["Binomial-Tian"] = americanOption.delta();
    oP.request["theta"]["Binomial-Tian"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<LeisenReimer>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Leisen-Reimer"] = americanOption.NPV();
    oP.request["gamma"]["Binomial-Leisen-Reimer"] = americanOption.gamma();
    oP.request["delta"]["Binomial-Leisen-Reimer"] = americanOption.delta();
    oP.request["theta"]["Binomial-Leisen-Reimer"] = americanOption.theta();

    americanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Joshi4>(
            bsmProcess, oP.timeSteps)));

    oP.request["NPV"]["Binomial-Joshi"] = americanOption.NPV();
    oP.request["delta"]["Binomial-Joshi"] = americanOption.delta();
    oP.request["gamma"]["Binomial-Joshi"] = americanOption.gamma();
    oP.request["theta"]["Binomial-Joshi"] = americanOption.theta();

    return oP.request.dump();
  };

  std::string calcuateBermudanOption(optionParameters &oP){
    
    Calendar calendar = TARGET();
    DayCounter dayCounter = Actual365Fixed();
    Settings::instance().evaluationDate() = oP.todaysDate;
    Volatility impliedVolatility(oP.optionPrice);

    oP.request["ImpliedVolatility"] = impliedVolatility;

    std::vector<Date> exerciseDates;
    for (Integer i = 1; i <= 4; i++)
      exerciseDates.push_back(oP.settlementDate + 3 * i * Months);

    ext::shared_ptr<Exercise> bermudanExercise(
      new BermudanExercise(exerciseDates));

    Handle<Quote> underlyingH(
      ext::shared_ptr<Quote>(
        new SimpleQuote(oP.underlying)));

    Handle<YieldTermStructure> flatTermStructure(
    ext::shared_ptr<YieldTermStructure>(
      new FlatForward(
        oP.settlementDate,
        oP.riskFreeRate,
        dayCounter)));

    Handle<YieldTermStructure> flatDividendTS(
      ext::shared_ptr<YieldTermStructure>(
        new FlatForward(
          oP.settlementDate,
          oP.dividendYield,
          dayCounter)));

    Handle<BlackVolTermStructure> flatVolTS(
      ext::shared_ptr<BlackVolTermStructure>(
        new BlackConstantVol(
          oP.settlementDate,
          calendar,
          impliedVolatility,
          dayCounter)));

    ext::shared_ptr<StrikedTypePayoff> payoff(
      new PlainVanillaPayoff(
        oP.type,
        oP.strike));

    ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
      new BlackScholesMertonProcess(
        underlyingH,
        flatDividendTS,
        flatTermStructure,
        flatVolTS));

    VanillaOption bermudanOption(payoff, bermudanExercise);

    ext::shared_ptr<PricingEngine> fdengine =
      ext::make_shared<FdBlackScholesVanillaEngine>(
        bsmProcess,
        oP.timeSteps,
        oP.timeSteps - 1);
    bermudanOption.setPricingEngine(fdengine);

    oP.request["NPV"]["Finite-differences"] = bermudanOption.NPV();
    oP.request["gamma"]["Finite-differences"] = bermudanOption.gamma();
    oP.request["delta"]["Finite-differences"] = bermudanOption.delta();
    oP.request["theta"]["Finite-differences"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<JarrowRudd>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Jarrow-Rudd"] = bermudanOption.NPV();
    oP.request["gamma"]["Binomial-Jarrow-Rudd"] = bermudanOption.gamma();
    oP.request["delta"]["Binomial-Jarrow-Rudd"] = bermudanOption.delta();
    oP.request["theta"]["Binomial-Jarrow-Rudd"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<CoxRossRubinstein>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Cox-Ross-Rubinstein"] = bermudanOption.NPV();
    oP.request["gamma"]["Binomial-Cox-Ross-Rubinstein"] = bermudanOption.gamma();
    oP.request["delta"]["Binomial-Cox-Ross-Rubinstein"] = bermudanOption.delta();
    oP.request["theta"]["Binomial-Cox-Ross-Rubinstein"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<AdditiveEQPBinomialTree>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Additive-equiprobabilities"] = bermudanOption.NPV();
    oP.request["gamma"]["Additive-equiprobabilities"] = bermudanOption.gamma();
    oP.request["delta"]["Additive-equiprobabilities"] = bermudanOption.delta();
    oP.request["theta"]["Additive-equiprobabilities"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Trigeorgis>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Trigeorgis"] = bermudanOption.NPV();
    oP.request["gamma"]["Binomial-Trigeorgis"]= bermudanOption.gamma();
    oP.request["delta"]["Binomial-Trigeorgis"] = bermudanOption.delta();
    oP.request["theta"]["Binomial-Trigeorgis"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Tian>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Heston-semi-analytic"] = bermudanOption.NPV();
    oP.request["gamma"]["Heston-semi-analytic"] = bermudanOption.gamma();
    oP.request["delta"]["Heston-semi-analytic"] = bermudanOption.delta();
    oP.request["theta"]["Heston-semi-analytic"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<LeisenReimer>(
        bsmProcess,
        oP.timeSteps)));

    oP.request["NPV"]["Binomial-Leisen-Reimer"] = bermudanOption.NPV();
    oP.request["gamma"]["Binomial-Leisen-Reimer"] = bermudanOption.gamma();
    oP.request["delta"]["Binomial-Leisen-Reimer"] = bermudanOption.delta();
    oP.request["theta"]["Binomial-Leisen-Reimer"] = bermudanOption.theta();

    bermudanOption.setPricingEngine(
      ext::shared_ptr<PricingEngine>(
        new BinomialVanillaEngine<Joshi4>(
          bsmProcess,
          oP.timeSteps)));

    oP.request["NPV"]["Binomial-Joshi"] = bermudanOption.NPV();
    oP.request["gamma"]["Binomial-Joshi"] = bermudanOption.gamma();
    oP.request["delta"]["Binomial-Joshi"] = bermudanOption.delta();
    oP.request["theta"]["Binomial-Joshi"] = bermudanOption.theta();

    return oP.request.dump();
  };

  std::string calcuateOption(std::string data) {
    try {
      json request = json::parse(data);

      optionParameters oP;

      oP.todaysDate = Date(DateParser::parseISO(request["todaysDate"].get<std::string>()));
      oP.settlementDate = Date(DateParser::parseISO(request["settlementDate"].get<std::string>()));
      oP.type = Option::Type(request["optionType"].get<int>()); 
      oP.underlying = request["underlying"];
      oP.strike = request["strike"];
      oP.dividendYield = request["dividendYield"];
      oP.riskFreeRate = request["riskFreeRate"];
      oP.maturityDate = DateParser::parseISO(request["maturityDate"].get<std::string>());
      oP.optionPrice = request["optionPrice"];
      oP.timeSteps = 801;
      oP.request = request;

      switch(request["executionStyle"].get<int>()) 
      {
        case 0: return calcuateEuropeanOption(oP);
        case 1: return calcuateAmericanOption(oP);  
        case 2: return calcuateBermudanOption(oP);
        default: throw("must submit excerise style");
      };
    }

    catch (std::exception &e) { return e.what(); }
    catch (...) { return "unknown error"; }
  };

  //EMSCRIPTEN_BINDINGS(quantlib) { emscripten::function("calcuateOption", &calcuateOption); }
}

int main(int, char* []) {
 
    try {
  
      json j_data = R"({
        "executionStyle":0,
        "todaysDate":"1998-05-15",
        "settlementDate": "1998-05-17",
        "maturityDate": "1999-05-17",
        "optionType":-1,
        "underlying": 36,
        "strike": 40,
        "dividendYield": 0,
        "riskFreeRate": 0.06,
        "optionPrice": 4.00
      })"_json;

      std::string result = calcuateOption(j_data.dump());

      std::cout << "Result: "  << result << std::endl;
        
      return 0;
 
    } catch (exception& e) {
        cerr << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "unknown error" << endl;
        return 1;
    }
}
