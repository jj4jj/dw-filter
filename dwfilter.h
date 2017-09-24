#ifndef _DW_FILTER_
#define _DW_FILTER_

#include <string>
#include <vector>

namespace dwf {
    static int  Clear();
    static int  Destory();
    static int  Remove(const std::string & strUTF8);
    static int  AddWords(const std::string & strUTF8, int iMatchLevel, bool bIsVocaulary = false);
    static int  MatchWords(std::vector<std::string> & strMatchedList, const std::string & strSentenseUTF8, bool bGlobal = false);
    static int  TotalWords();
};

#endif
