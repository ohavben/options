#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#  include <ql/auto_link.hpp>
#endif

#include <boost/config.hpp>

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
#include <ql/utilities/dataparsers.hpp>
#include <ql/models/shortrate/onefactormodels/vasicek.hpp>
#include <ql/time/date.hpp>

#include <string>
#include <ctime>
#include <iomanip>
#include <iostream>
#include "json.hpp"

//using namespace std;
using namespace QuantLib;
using json = nlohmann::json;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

    ThreadKey sessionId() { return {}; }

}
#endif

// setting option type: call or put.
QuantLib::Option::Type setOptionType(std::string type){
    if (type == "Call") return Option::Type (Option::Call);
    return Option::Type (Option::Put);
};

std::string calcuateEuropeanOption(std::string data){
    try {

        json JsonData = json::parse(data);

        Calendar calendar = TARGET();
        Date todaysDate = Date(DateParser::parseISO(JsonData["request"]["todaysDate"].get<std::string>()));
        Date settlementDate = Date(DateParser::parseISO(JsonData["request"]["settlementDate"].get<std::string>()));
        Settings::instance().evaluationDate() = todaysDate;
        Option::Type type(setOptionType(JsonData["request"]["optionType"].get<std::string>()));
        Real underlying = JsonData["request"]["underlying"];
        Real strike = JsonData["request"]["strike"];
        Spread dividendYield = JsonData["request"]["dividendYield"];
        Rate riskFreeRate = JsonData["request"]["riskFreeRate"];
        Volatility volatility = JsonData["request"]["volatility"];
        Date maturity(DateParser::parseISO(JsonData["request"]["maturityDate"].get<std::string>()));
        DayCounter dayCounter = Actual365Fixed();

        std::vector<Date> exerciseDates;
        for (Integer i=1; i<=4; i++)
            exerciseDates.push_back(settlementDate + 3*i*Months);

        ext::shared_ptr<Exercise> europeanExercise(
            new EuropeanExercise(maturity));

        Handle<Quote> underlyingH(
            ext::shared_ptr<Quote>(
                new SimpleQuote(
                    underlying
                )
            )
        );

        // bootstrap the yield/dividend/vol curves
        Handle<YieldTermStructure> flatTermStructure(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate,
                    riskFreeRate, 
                    dayCounter
                )
            )
        );

        Handle<YieldTermStructure> flatDividendTS(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate, 
                    dividendYield, 
                    dayCounter
                )
            )
        );

        Handle<BlackVolTermStructure> flatVolTS(
            ext::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(
                    settlementDate, 
                    calendar, 
                    volatility,
                    dayCounter
                )
            )
        );

        ext::shared_ptr<StrikedTypePayoff> payoff(
            new PlainVanillaPayoff(
                type, 
                strike
            )
        );

        ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
            new BlackScholesMertonProcess(
                underlyingH, 
                flatDividendTS,
                flatTermStructure, 
                flatVolTS
            )
        );

        VanillaOption europeanOption(payoff, europeanExercise);

        // Black-Scholes for European
        europeanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new AnalyticEuropeanEngine(bsmProcess)
            )
        );
        
        JsonData["response"]["Black-Scholes"]["NPV"] = europeanOption.NPV();
        JsonData["response"]["Black-Scholes"]["gamma"] = europeanOption.gamma();
        JsonData["response"]["Black-Scholes"]["delta"] = europeanOption.delta();
        JsonData["response"]["Black-Scholes"]["rho"] = europeanOption.rho();
        JsonData["response"]["Black-Scholes"]["vega"] = europeanOption.vega();
        JsonData["response"]["Black-Scholes"]["theta"] = europeanOption.theta();
        JsonData["response"]["Black-Scholes"]["thetaPerDay"] = europeanOption.thetaPerDay();
        
        return JsonData.dump();
    } 
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return e.what();
    }
    catch (...)
    {
        std::cerr << "unknown error" << std::endl;
        return "unknown error";
    }
};

std::string calcuateBermudanOption(std::string data){
    try {
        json JsonData = json::parse(data);

        Calendar calendar = TARGET();
        Date todaysDate = Date(DateParser::parseISO(JsonData["request"]["todaysDate"].get<std::string>()));
        Date settlementDate = Date(DateParser::parseISO(JsonData["request"]["settlementDate"].get<std::string>()));
        Settings::instance().evaluationDate() = todaysDate;
        Option::Type type(setOptionType(JsonData["request"]["optionType"].get<std::string>()));
        Real underlying = JsonData["request"]["underlying"];
        Real strike = JsonData["request"]["strike"];
        Spread dividendYield = JsonData["request"]["dividendYield"];
        Rate riskFreeRate = JsonData["request"]["riskFreeRate"];
        Volatility volatility = JsonData["request"]["volatility"];
        Date maturity(DateParser::parseISO(JsonData["request"]["maturityDate"].get<std::string>()));
        DayCounter dayCounter = Actual365Fixed();

        std::vector<Date> exerciseDates;
        for (Integer i=1; i<=4; i++)
            exerciseDates.push_back(settlementDate + 3*i*Months);

        ext::shared_ptr<Exercise> bermudanExercise(
            new BermudanExercise(exerciseDates)
        );

        Handle<Quote> underlyingH(
            ext::shared_ptr<Quote>(
                new SimpleQuote(underlying)
            )
        );

        // bootstrap the yield/dividend/vol curves
        Handle<YieldTermStructure> flatTermStructure(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate,
                    riskFreeRate, 
                    dayCounter
                )
            )
        );

        Handle<YieldTermStructure> flatDividendTS(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate, 
                    dividendYield, 
                    dayCounter
                )
            )
        );

        Handle<BlackVolTermStructure> flatVolTS(
            ext::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(
                    settlementDate, 
                    calendar, 
                    volatility,
                    dayCounter
                )
            )
        );

        ext::shared_ptr<StrikedTypePayoff> payoff(
            new PlainVanillaPayoff(
                type, 
                strike
            )
        );

        ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
            new BlackScholesMertonProcess(
                underlyingH, 
                flatDividendTS,
                flatTermStructure, 
                flatVolTS
            )
        );

        VanillaOption bermudanOption(payoff, bermudanExercise);

            // Finite differences
        Size timeSteps = JsonData["request"]["FiniteDifferencesTimeSteps"];
        ext::shared_ptr<PricingEngine> fdengine =
            ext::make_shared<FdBlackScholesVanillaEngine>(
                bsmProcess,
                timeSteps,
                timeSteps-1
            );
        bermudanOption.setPricingEngine(fdengine);

        JsonData["response"]["Finite-differences"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Finite-differences"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Finite-differences"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Finite-differences"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Finite-differences"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Finite-differences"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Finite-differences"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        // Binomial method: Jarrow-Rudd
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<JarrowRudd>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Jarrow-Rudd"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Binomial-Jarrow-Rudd"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Binomial-Jarrow-Rudd"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Binomial-Jarrow-Rudd"]["thetaPerDay"] = bermudanOption.thetaPerDay();


        //"Binomial Cox-Ross-Rubinstein";;
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<CoxRossRubinstein>(
                    bsmProcess,
                    timeSteps
                
                )
            )
        );

        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["thetaPerDay"] = bermudanOption.thetaPerDay();
        

        // Binomial method: Additive equiprobabilities
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<AdditiveEQPBinomialTree>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Additive-equiprobabilities"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Additive-equiprobabilities"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Additive-equiprobabilities"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Additive-equiprobabilities"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Additive-equiprobabilities"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Additive-equiprobabilities"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Additive-equiprobabilities"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        // Binomial method: Binomial Trigeorgis
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Trigeorgis>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Trigeorgis"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Binomial-Trigeorgis"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Binomial-Trigeorgis"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Binomial-Trigeorgis"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Binomial-Trigeorgis"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Binomial-Trigeorgis"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Binomial-Trigeorgis"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        // Binomial method: Binomial Tian
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Tian>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Heston-semi-analytic"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Heston-semi-analytic"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Heston-semi-analytic"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Heston-semi-analytic"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Heston-semi-analytic"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Heston-semi-analytic"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Heston-semi-analytic"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        // Binomial method: Binomial Leisen-Reimer
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<LeisenReimer>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Leisen-Reimer"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Binomial-Leisen-Reimer"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Binomial-Leisen-Reimer"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Binomial-Leisen-Reimer"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Binomial-Leisen-Reimer"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Binomial-Leisen-Reimer"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Binomial-Leisen-Reimer"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        // Binomial method: Binomial Joshi 
        bermudanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Joshi4>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Joshi"]["NPV"] = bermudanOption.NPV();
        JsonData["response"]["Binomial-Joshi"]["gamma"] = bermudanOption.gamma();
        JsonData["response"]["Binomial-Joshi"]["delta"] = bermudanOption.delta();
        //JsonData["response"]["Binomial-Joshi"]["rho"] = bermudanOption.rho();
        //JsonData["response"]["Binomial-Joshi"]["vega"] = bermudanOption.vega();
        JsonData["response"]["Binomial-Joshi"]["theta"] = bermudanOption.theta();
        //JsonData["response"]["Binomial-Joshi"]["thetaPerDay"] = bermudanOption.thetaPerDay();

        return JsonData.dump();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return e.what();
    }
    catch (...)
    {
        std::cerr << "unknown error" << std::endl;
        return "unknown error";
    }
};

std::string calcuateAmericanOption(std::string data){

    try {
        json JsonData = json::parse(data);
        
        Calendar calendar = TARGET();

        Date todaysDate = Date(DateParser::parseISO(JsonData["request"]["todaysDate"].get<std::string>()));
        Date settlementDate = Date(DateParser::parseISO(JsonData["request"]["settlementDate"].get<std::string>()));
        Settings::instance().evaluationDate() = todaysDate;
        Option::Type type(setOptionType(JsonData["request"]["optionType"].get<std::string>()));
        Real underlying = JsonData["request"]["underlying"];
        Real strike = JsonData["request"]["strike"];
        Spread dividendYield = JsonData["request"]["dividendYield"];
        Rate riskFreeRate = JsonData["request"]["riskFreeRate"];
        Volatility volatility = JsonData["request"]["volatility"];
        Date maturity(DateParser::parseISO(JsonData["request"]["maturityDate"].get<std::string>()));
        DayCounter dayCounter = Actual365Fixed();

        std::vector<Date> exerciseDates;
        for (Integer i=1; i<=4; i++)
            exerciseDates.push_back(settlementDate + 3*i*Months);

        ext::shared_ptr<Exercise> americanExercise(
            new AmericanExercise(
                settlementDate,
                maturity
            )
        );

        Handle<Quote> underlyingH(
            ext::shared_ptr<Quote>(
                new SimpleQuote(underlying)
            )
        );

        // bootstrap the yield/dividend/vol curves
        Handle<YieldTermStructure> flatTermStructure(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate,
                    riskFreeRate,
                    dayCounter
                )
            )
        );

        Handle<YieldTermStructure> flatDividendTS(
            ext::shared_ptr<YieldTermStructure>(
                new FlatForward(
                    settlementDate,
                    dividendYield,
                    dayCounter
                )
            )
        );

        Handle<BlackVolTermStructure> flatVolTS(
            ext::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(
                    settlementDate,
                    calendar,
                    volatility,
                    dayCounter
                )
            )
        );

        ext::shared_ptr<StrikedTypePayoff> payoff(
            new PlainVanillaPayoff(type, strike)
        );

        ext::shared_ptr<BlackScholesMertonProcess> bsmProcess(
            new BlackScholesMertonProcess(
                underlyingH,
                flatDividendTS,
                flatTermStructure,
                flatVolTS
            )
        );

        VanillaOption americanOption(payoff, americanExercise);

        // Finite differences
        Size timeSteps = JsonData["request"]["FiniteDifferencesTimeSteps"];

        ext::shared_ptr<PricingEngine> fdengine =
            ext::make_shared<FdBlackScholesVanillaEngine>(
                bsmProcess,
                timeSteps,
                timeSteps-1
            );

        americanOption.setPricingEngine(fdengine);
        JsonData["response"]["Finite-Differences"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Finite-Differences"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Finite-Differences"]["delta"] = americanOption.delta();
        JsonData["response"]["Finite-Differences"]["theta"] = americanOption.theta();
        //JsonData["response"]["Finite-Differences"]["rho"] = americanOption.rho();
        //JsonData["response"]["Finite-Differences"]["vega"] = americanOption.vega();

        // Binomial method: Jarrow-Rudd
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<JarrowRudd>(
                    bsmProcess,
                    timeSteps
                )
            )
        );
        JsonData["response"]["Binomial-Jarrow-Rudd"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Jarrow-Rudd"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Jarrow-Rudd"]["rho"] = americanOption.rho();
        //JsonData["response"]["Binomial-Jarrow-Rudd"]["vega"] = americanOption.vega();

        // Binomial Cox-Ross-Rubinstein;
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<CoxRossRubinstein>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["rho"] = americanOption.rho();
        //JsonData["response"]["Binomial-Cox-Ross-Rubinstein"]["vega"] = americanOption.vega();
        
        // Binomial method: Additive equiprobabilities
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<AdditiveEQPBinomialTree>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Additive-equiprobabilities"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Additive-equiprobabilities"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Additive-equiprobabilities"]["delta"] = americanOption.delta();
        JsonData["response"]["Additive-equiprobabilities"]["theta"] = americanOption.theta();
        //JsonData["response"]["Additive-equiprobabilities"]["rho"] = americanOption.rho();
        //JsonData["response"]["Additive-equiprobabilities"]["vega"] = americanOption.vega();
        
        
        // Binomial method: Binomial Trigeorgis
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Trigeorgis>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Trigeorgis"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Trigeorgis"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Trigeorgis"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Trigeorgis"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Trigeorgis"]["rho"] = americanOption.rho();
        //JsonData["response"]["Binomial-Trigeorgis"]["vega"] = americanOption.vega();

        // Binomial method: Binomial Tian
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Tian>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Tian"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Tian"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Tian"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Tian"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Tian"]["rho"] = americanOption.rho();
        //JsonData["response"]["Binomial-Tian"]["vega"] = americanOption.vega();
        
        // Binomial-Leisen-Reimer
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<LeisenReimer>(
                    bsmProcess,
                    timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Leisen-Reimer"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Leisen-Reimer"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Leisen-Reimer"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Leisen-Reimer"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Leisen-Reimer"]["rho"] = americanOption.rho();
        //JsonData["response"]["Binomial-Leisen-Reimer"]["vega"] = americanOption.vega();
        
        // Binomial method: Binomial Joshi
        americanOption.setPricingEngine(
            ext::shared_ptr<PricingEngine>(
                new BinomialVanillaEngine<Joshi4>(
                    bsmProcess,timeSteps
                )
            )
        );

        JsonData["response"]["Binomial-Joshi"]["NPV"] = americanOption.NPV();
        JsonData["response"]["Binomial-Joshi"]["delta"] = americanOption.delta();
        JsonData["response"]["Binomial-Joshi"]["gamma"] = americanOption.gamma();
        JsonData["response"]["Binomial-Joshi"]["theta"] = americanOption.theta();
        //JsonData["response"]["Binomial-Joshi"]["vega"] = americanOption.vega();
        //JsonData["response"]["Binomial-Joshi"]["rho"] = americanOption.rho();
        
        return JsonData.dump();
    } 
    catch(std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return e.what();
    }
    catch (...)
    {
        std::cerr << "unknown error" << std::endl;
        return "unknown error in pricer";
    }
};

int main(int, char* []) {

    try {

        json americanOptionJson = R"({
            "request": {
                "todaysDate":"1998-05-15",
                "settlementDate": "1998-05-17",
                "maturityDate": "1999-05-17",
                "optionType":"Put",
                "underlying": 36,
                "strike": 40,
                "dividendYield": 0,
                "riskFreeRate": 0.06,
                "volatility": 0.20,
                "FiniteDifferencesTimeSteps": 801
            },
            "response":{},
            "errors":{}
        })"_json;

        json europeanOptionJson = R"({
            "request": {
                "todaysDate":"1998-05-15",
                "settlementDate": "1998-05-17",
                "maturityDate": "1999-05-17",
                "optionType":"Put",
                "underlying": 36,
                "strike": 40,
                "dividendYield": 0,
                "riskFreeRate": 0.06,
                "volatility": 0.20
            },
            "response":{},
            "errors":{}
        })"_json;

        json bermudanOptionJson = R"({
            "request": {
                "todaysDate":"1998-05-15",
                "settlementDate": "1998-05-17",
                "maturityDate": "1999-05-17",
                "optionType":"Put",
                "underlying": 36,
                "strike": 40,
                "dividendYield": 0,
                "riskFreeRate": 0.06,
                "volatility": 0.20,
                "FiniteDifferencesTimeSteps": 801
            },
            "response":{},
            "errors":{}
        })"_json;

        std::cout << "American Option: " << calcuateAmericanOption(americanOptionJson.dump()) << std::endl;
        std::cout << "----------------------------------------------------------------------------------------" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << "Bermudan Option: " << calcuateBermudanOption(bermudanOptionJson.dump()) << std::endl;
        std::cout << "----------------------------------------------------------------------------------------" << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
        std::cout << "European Option: " << calcuateEuropeanOption(europeanOptionJson.dump()) << std::endl;
        std::cout << "----------------------------------------------------------------------------------------" << std::endl;
        return 0;

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
        return 1;
    }
}
