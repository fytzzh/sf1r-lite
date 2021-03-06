/**
 * @file ZambeziScoreNormalizer.h
 * @brief normalize the zambezi score.
 */

#ifndef SF1R_ZAMBEZI_SCORE_NORMALIZER_H
#define SF1R_ZAMBEZI_SCORE_NORMALIZER_H

#include <common/inttypes.h>
#include <common/PropSharedLockSet.h>
#include <mining-manager/product-scorer/NumericExponentScorer.h>

#include <vector>

namespace sf1r
{
class MiningManager;

class ZambeziScoreNormalizer
{
public:
    ZambeziScoreNormalizer(MiningManager& miningManager);

    void normalizeScore(
        const std::vector<docid_t>& docids,
        const std::vector<float>& productScores,
        std::vector<float>& relevanceScores);

private:
    const NumericExponentScorer exponentScorer_;

    const float relevanceWeight_;
};

} // namespace sf1r

#endif // SF1R_ZAMBEZI_SCORE_NORMALIZER_H
