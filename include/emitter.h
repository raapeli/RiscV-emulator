#include <string>
#include <unordered_set>

class Emitter {
public:
  Emitter();
  void writeFile();

private:
  std::string writeBuffer;
  std::unordered_set<std::string> labelsDeclared;
  std::unordered_set<std::string> labelsGotoed;
};
