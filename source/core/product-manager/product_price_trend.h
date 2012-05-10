#ifndef SF1R_PRODUCTMANAGER_PRODUCTPRICETREND_H
#define SF1R_PRODUCTMANAGER_PRODUCTPRICETREND_H

#include "pm_def.h"
#include "pm_types.h"
#include "product_price.h"
#include <configuration-manager/CassandraStorageConfig.h>

#include <am/tc/BTree.h>

#include <boost/unordered_map.hpp>
#include <boost/pool/pool_alloc.hpp>

namespace sf1r
{

class PriceHistory;
class PriceHistoryRow;
class DocumentManager;

class ProductPriceTrend
{
    typedef izenelib::am::tc::BTree<std::string, TPCQueue> TPCBTree;
    typedef std::map<std::string, std::vector<TPCBTree *> > TPCStorage;

    typedef std::pair<ProductPriceType, std::map<std::string, std::string> > PropItemType;
    typedef boost::unordered_map<uint128_t, PropItemType, boost::hash<uint128_t>, std::equal_to<uint128_t>, boost::fast_pool_allocator<uint128_t> > PropMapType;

public:
    ProductPriceTrend(
            const boost::shared_ptr<DocumentManager>& document_manager,
            const CassandraStorageConfig& cassandraConfig,
            const std::string& data_dir,
            const std::vector<std::string>& group_prop_vec,
            const std::vector<uint32_t>& time_int_vec);

    ~ProductPriceTrend();

    bool Init();

    bool Flush();

    bool Insert(
            const uint128_t& docid,
            const ProductPrice& price,
            time_t timestamp);

    bool Update(
            const uint128_t& docid,
            const ProductPrice& price,
            time_t timestamp,
            std::map<std::string, std::string>& group_prop_map);

    bool GetMultiPriceHistory(
            PriceHistoryList& history_list,
            const std::vector<uint128_t>& docid_list,
            time_t from_tt,
            time_t to_tt,
            std::string& error_msg);

    bool GetMultiPriceRange(
            PriceRangeList& range_list,
            const std::vector<uint128_t>& docid_list,
            time_t from_tt,
            time_t to_tt,
            std::string& error_msg);

    bool GetTopPriceCutList(
            TPCQueue& tpc_queue,
            const std::string& prop_name,
            const std::string& prop_value,
            uint32_t days,
            uint32_t count,
            std::string& error_msg);

    bool MigratePriceHistory(
            const std::string& new_keyspace,
            uint32_t start,
            std::string& error_msg);

    bool CronJob();

private:
    bool IsBufferFull_() const;

    bool UpdateTPC_(uint32_t time_int, time_t timestamp);

private:
    boost::shared_ptr<DocumentManager> document_manager_;
    boost::shared_ptr<PriceHistory> price_history_;

    const CassandraStorageConfig cassandraConfig_;
    std::string data_dir_;

    std::vector<std::string> group_prop_vec_;
    std::vector<uint32_t> time_int_vec_;

    std::vector<PriceHistoryRow> price_history_buffer_;
    uint32_t buffer_size_;

    bool enable_tpc_;
    PropMapType prop_map_;
    TPCStorage tpc_storage_;
};

}

#endif
