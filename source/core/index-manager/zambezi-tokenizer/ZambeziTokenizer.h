#ifndef SF1R_COMMON_ZAMBEZI_TOKENIZER_H_
#define SF1R_COMMON_ZAMBEZI_TOKENIZER_H_

#include <util/singleton.h>
#include <am/succinct/ux-trie/uxTrie.hpp>
#include <common/type_defs.h>
#include <string>
#include <list>
#include <map>
#include <vector>

namespace cma
{
class Analyzer;
class Knowledge;
}

namespace sf1r
{

class ZambeziTokenizer
{
public:
    enum TokenizerType
    {
        CMA_MAXPRE, ///type 100 of icma;
        CMA_FMINCOVER ///type 5 of icma
    };

    ZambeziTokenizer();

    ~ZambeziTokenizer();

    void setItemUnique(bool isUnique);

    bool getTokenResults(
            const std::string& pattern,
            std::vector<std::pair<std::string, int> >& token_results);
    
    void initWithCMA_(
            TokenizerType type,
            const std::string& dict_path);

private:
    bool getTokenResultsByCMA_(
            const std::string& pattern,
            std::vector<std::pair<std::string, int> >& token_results);

private:
    bool isInit_;
    bool isItemUnique_;
    TokenizerType type_;

    cma::Analyzer* analyzer_;
    cma::Knowledge* knowledge_;
};

}

#endif
