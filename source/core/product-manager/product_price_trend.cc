#include "product_price_trend.h"

#include <common/Utilities.h>
#include <log-manager/PriceHistory.h>
#include <am/range/AmIterator.h>
#include <libcassandra/util_functions.h>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <algorithm>

using namespace std;
using namespace libcassandra;
using izenelib::util::UString;

namespace sf1r
{

ProductPriceTrend::ProductPriceTrend(
        const string& collection_name,
        const string& data_dir,
        const vector<string>& group_prop_vec,
        const vector<uint32_t>& time_int_vec)
    : collection_name_(collection_name)
    , data_dir_(data_dir)
    , group_prop_vec_(group_prop_vec)
    , time_int_vec_(time_int_vec)
    , enable_tpc_(!group_prop_vec_.empty() && !time_int_vec_.empty())
{
}

ProductPriceTrend::~ProductPriceTrend()
{
    for (TPCStorage::iterator it = tpc_storage_.begin();
            it != tpc_storage_.end(); ++it)
    {
        for (uint32_t i = 0; i < time_int_vec_.size(); i++)
            delete it->second[i];
    }
}

bool ProductPriceTrend::Init()
{
    if (!PriceHistory::is_enabled) return false;

    for (vector<string>::const_iterator it = group_prop_vec_.begin();
            it != group_prop_vec_.end(); ++it)
    {
        vector<TPCBTree *>& prop_tpc = tpc_storage_[*it];
        for (uint32_t i = 0; i < time_int_vec_.size(); i++)
        {
            string db_path = data_dir_ + "/" + *it + "." + boost::lexical_cast<string>(time_int_vec_[i]) + ".tpc";
            prop_tpc.push_back(new TPCBTree(db_path));
            if (!prop_tpc.back()->open())
            {
                boost::filesystem::remove_all(db_path);
                prop_tpc.back()->open();
            }
        }
    }
    price_history_buffer_.reserve(10000);

    return true;
}

bool ProductPriceTrend::Insert(
        const string& docid,
        const ProductPrice& price,
        time_t timestamp)
{
    string key;
    ParseDocid_(key, docid);
    price_history_buffer_.push_back(PriceHistory(key));
    price_history_buffer_.back().insert(timestamp, price);

    if (IsBufferFull_())
    {
        return Flush();
    }

    return true;
}

bool ProductPriceTrend::Update(
        const string& docid,
        const ProductPrice& price,
        time_t timestamp,
        map<string, string>& group_prop_map)
{
    string key;
    ParseDocid_(key, docid);
    price_history_buffer_.push_back(PriceHistory());
    price_history_buffer_.back().resetKey(key);
    price_history_buffer_.back().insert(timestamp, price);

    if (enable_tpc_ && !group_prop_map.empty() && price.value.first > 0)
    {
        PropItemType& prop_item = prop_map_[key];
        prop_item.first = price.value.first;
        prop_item.second.swap(group_prop_map);
    }

    if (IsBufferFull_())
    {
        return Flush();
    }

    return true;
}

bool ProductPriceTrend::IsBufferFull_()
{
    return price_history_buffer_.size() >= 10000;
}

bool ProductPriceTrend::Flush()
{
    bool ret = true;
    time_t now = Utilities::createTimeStamp();

    if (!prop_map_.empty())
    {
        for (uint32_t i = 0; i < time_int_vec_.size(); i++)
        {
            if (!UpdateTPC_(i, now))
                ret = false;
        }

        prop_map_.clear();
    }

    if (!PriceHistory::updateMultiRow(price_history_buffer_))
        ret = false;

    price_history_buffer_.clear();

    return ret;
}

bool ProductPriceTrend::CronJob()
{
    if (enable_tpc_)
    {
        //TODO
    }
    return true;
}

bool ProductPriceTrend::UpdateTPC_(uint32_t time_int, time_t timestamp)
{
    vector<string> key_list;
    Utilities::getKeyList(key_list, prop_map_);
    if (timestamp == -1)
        timestamp = Utilities::createTimeStamp();
    timestamp -= 86400000000LL * time_int_vec_[time_int];

    vector<PriceHistory> row_list;
    if (!PriceHistory::getMultiSlice(row_list, key_list, serializeLong(timestamp), "", 1))
        return false;

    map<string, map<string, TPCQueue> > tpc_cache;
    for (uint32_t i = 0; i < row_list.size(); i++)
    {
        const PropItemType& prop_item = prop_map_.find(row_list[i].getDocId())->second;
        const pair<time_t, ProductPrice>& price_record = *(row_list[i].getPriceHistory().begin());
        const ProductPriceType& old_price = price_record.second.value.first;
        float price_cut = old_price == 0 ? 0 : 1 - prop_item.first / old_price;

        for (map<string, string>::const_iterator it = prop_item.second.begin();
                it != prop_item.second.end(); ++it)
        {
            TPCQueue& tpc_queue = tpc_cache[it->first][it->second];
            if (tpc_queue.empty())
            {
                tpc_queue.reserve(1001);
                tpc_storage_[it->first][time_int]->get(it->second, tpc_queue);
            }

            tpc_queue.push_back(make_pair(price_cut, string()));
            StripDocid_(tpc_queue.back().second, row_list[i].getDocId());
            push_heap(tpc_queue.begin(), tpc_queue.end(), rel_ops::operator> <TPCQueue::value_type>);

            if (tpc_queue.size() > 1000)
            {
                pop_heap(tpc_queue.begin(), tpc_queue.end(), rel_ops::operator> <TPCQueue::value_type>);
                tpc_queue.pop_back();
            }
        }
    }

    for (map<string, map<string, TPCQueue> >::const_iterator it = tpc_cache.begin();
            it != tpc_cache.end(); ++it)
    {
        TPCBTree* tpc_btree = tpc_storage_[it->first][time_int];
        for (map<string, TPCQueue>::const_iterator mit = it->second.begin();
                mit != it->second.end(); ++mit)
        {
            tpc_btree->update(mit->first, mit->second);
        }
        tpc_btree->flush();
    }

    return true;
}

bool ProductPriceTrend::GetMultiPriceHistory(
        PriceHistoryList& history_list,
        const vector<string>& docid_list,
        time_t from_tt,
        time_t to_tt,
        string& error_msg)
{
    vector<string> key_list;
    ParseDocidList_(key_list, docid_list);

    string from_str(from_tt == -1 ? "" : serializeLong(from_tt));
    string to_str(to_tt == -1 ? "" : serializeLong(to_tt));

    vector<PriceHistory> row_list;
    if (!PriceHistory::getMultiSlice(row_list, key_list, from_str, to_str))
    {
        error_msg = "Failed retrieving price histories from Cassandra";
        return false;
    }

    history_list.reserve(history_list.size() + row_list.size());
    for (vector<PriceHistory>::const_iterator it = row_list.begin();
            it != row_list.end(); ++it)
    {
        const PriceHistory::PriceHistoryType& price_history = it->getPriceHistory();
        PriceHistoryItem history_item;
        history_item.reserve(price_history.size());
        for (PriceHistory::PriceHistoryType::const_iterator hit = price_history.begin();
                hit != price_history.end(); ++hit)
        {
            history_item.push_back(make_pair(string(), hit->second.value));
            history_item.back().first = boost::posix_time::to_iso_string(
                    boost::posix_time::from_time_t(hit->first / 1000000 - timezone)
                    + boost::posix_time::microseconds(hit->first % 1000000));
        }
        history_list.push_back(make_pair(string(), PriceHistoryItem()));
        StripDocid_(history_list.back().first, it->getDocId());
        history_list.back().second.swap(history_item);
    }

    if (history_list.empty())
    {
        error_msg = "Have not got any valid docid";
        return false;
    }

    return true;
}

bool ProductPriceTrend::GetMultiPriceRange(
        PriceRangeList& range_list,
        const vector<string>& docid_list,
        time_t from_tt,
        time_t to_tt,
        string& error_msg)
{
    vector<string> key_list;
    ParseDocidList_(key_list, docid_list);

    string from_str(from_tt == -1 ? "" : serializeLong(from_tt));
    string to_str(to_tt == -1 ? "" : serializeLong(to_tt));

    vector<PriceHistory> row_list;
    if (!PriceHistory::getMultiSlice(row_list, key_list, from_str, to_str))
    {
        error_msg = "Failed retrieving price histories from Cassandra";
        return false;
    }

    range_list.reserve(range_list.size() + row_list.size());
    for (vector<PriceHistory>::const_iterator it = row_list.begin();
            it != row_list.end(); ++it)
    {
        const PriceHistory::PriceHistoryType& price_history = it->getPriceHistory();
        ProductPrice range_item;
        for (PriceHistory::PriceHistoryType::const_iterator hit = price_history.begin();
                hit != price_history.end(); ++hit)
        {
            range_item += hit->second;
        }
        range_list.push_back(make_pair(string(), range_item.value));
        StripDocid_(range_list.back().first, it->getDocId());
    }

    if (range_list.empty())
    {
        error_msg = "Have not got any valid docid";
        return false;
    }

    return true;
}

void ProductPriceTrend::ParseDocid_(string& dest, const string& src) const
{
    if (!collection_name_.empty())
    {
        dest.assign(collection_name_ + "_" + src);
    }
    else
    {
        dest.assign(src);
    }
}

void ProductPriceTrend::StripDocid_(string& dest, const string& src) const
{
    if (!collection_name_.empty() && src.length() > collection_name_.length() + 1)
    {
        dest.assign(src.substr(collection_name_.length() + 1));
    }
    else
    {
        dest.assign(src);
    }
}

void ProductPriceTrend::ParseDocidList_(vector<string>& dest, const vector<string>& src) const
{
    dest.reserve(dest.size() + src.size());
    for (uint32_t i = 0; i < src.size(); i++)
    {
        if (src[i].empty()) continue;
        dest.push_back(string());
        ParseDocid_(dest.back(), src[i]);
    }
}

void ProductPriceTrend::StripDocidList_(vector<string>& dest, const vector<string>& src) const
{
    dest.reserve(dest.size() + src.size());
    for (uint32_t i = 0; i < src.size(); i++)
    {
        if (src[i].empty()) continue;
        dest.push_back(string());
        StripDocid_(dest.back(), src[i]);
    }
}

bool ProductPriceTrend::GetTopPriceCutList(
        TPCQueue& tpc_queue,
        const string& prop_name,
        const string& prop_value,
        uint32_t days,
        uint32_t count,
        string& error_msg)
{
    if (!enable_tpc_)
    {
        error_msg = "Top price-cut list is not enabled for this collection.";
        return false;
    }

    TPCStorage::const_iterator tpc_it = tpc_storage_.find(prop_name);
    if (tpc_it == tpc_storage_.end())
    {
        error_msg = "Don't find data for this property name: " + prop_name;
        return false;
    }

    uint32_t time_int = 0;
    while (days > time_int_vec_[time_int++] && time_int < time_int_vec_.size() - 1);
    if (!tpc_it->second[time_int]->get(prop_value, tpc_queue))
    {
        error_msg = "Don't find price-cut record for this property value: " + prop_value;
        return false;
    }

    sort_heap(tpc_queue.begin(), tpc_queue.end(), rel_ops::operator><TPCQueue::value_type>);
    if (tpc_queue.size() > count)
        tpc_queue.resize(count);

    return true;
}

}
