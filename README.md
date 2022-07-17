# options-
Options pricing calculation with quantlib. 

if you want to test without emscripten, run with:


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

