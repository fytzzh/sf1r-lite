#include "GroupRep.h"
#include "NumericRangeGroupCounter.h"
#include <queue>

NS_FACETED_BEGIN

GroupRep::GroupRep()
{
}

GroupRep::~GroupRep()
{
}

bool GroupRep::empty() const
{
    return (stringGroupRep_.empty() && numericGroupRep_.empty() && numericRangeGroupRep_.empty());
}

void GroupRep::clear()
{
    stringGroupRep_.clear();
    numericGroupRep_.clear();
    numericRangeGroupRep_.clear();
}

void GroupRep::swap(GroupRep& rep)
{
    stringGroupRep_.swap(rep.stringGroupRep_);
    numericGroupRep_.swap(rep.numericGroupRep_);
    numericRangeGroupRep_.swap(rep.numericRangeGroupRep_);
}

bool GroupRep::operator==(const GroupRep& other) const
{
    if (stringGroupRep_.size() != other.stringGroupRep_.size()
        || numericGroupRep_.size() != other.numericGroupRep_.size()
        || numericRangeGroupRep_.size() != other.numericRangeGroupRep_.size())
    {
        return false;
    }

    StringGroupRep::const_iterator sit1 = stringGroupRep_.begin();
    StringGroupRep::const_iterator sit2 = other.stringGroupRep_.begin();
    while (sit1 != stringGroupRep_.end())
    {
        if (!(*sit1 == *sit2))
            return false;

        ++sit1;
        ++sit2;
    }

    NumericGroupRep::const_iterator nit1 = numericGroupRep_.begin();
    NumericGroupRep::const_iterator nit2 = other.numericGroupRep_.begin();
    while (nit1 != numericGroupRep_.end())
    {
        if (nit1->first != nit2->first || nit1->second.size() != nit2->second.size())
            return false;

        list<pair<double, unsigned int> >::const_iterator it1 = nit1->second.begin();
        list<pair<double, unsigned int> >::const_iterator it2 = nit2->second.begin();
        while (it1 != nit1->second.end())
        {
            if (it1->first != it2->first || it1->second != it2->second)
                return false;

            ++it1;
            ++it2;
        }
        ++nit1;
        ++nit2;
    }

    NumericRangeGroupRep::const_iterator rit1 = numericRangeGroupRep_.begin();
    NumericRangeGroupRep::const_iterator rit2 = other.numericRangeGroupRep_.begin();
    while (rit1 != numericRangeGroupRep_.end())
    {
        if (rit1->first != rit2->first || rit1->second.size() != rit2->second.size())
            return false;

        vector<unsigned int>::const_iterator it1 = rit1->second.begin();
        vector<unsigned int>::const_iterator it2 = rit2->second.begin();
        while (it1 != rit1->second.end())
        {
            if (*it1 != *it2)
                return false;

            ++it1;
            ++it2;
        }
        ++rit1;
        ++rit2;
    }

    return true;
}

void GroupRep::merge(const GroupRep& other)
{
    mergeStringGroup(other);
    mergeNumericGroup(other);
    mergeNumericRangeGroup(other);
}

void GroupRep::mergeStringGroup(const GroupRep& other)
{
    if (other.stringGroupRep_.empty())
        return;

    if (stringGroupRep_.empty())
    {
        stringGroupRep_.swap(const_cast<GroupRep &>(other).stringGroupRep_);
        return;
    }

    StringGroupRep::iterator it = stringGroupRep_.begin();
    StringGroupRep::const_iterator oit = other.stringGroupRep_.begin();

    recurseStringGroup(other, it, oit, 0);
}

void GroupRep::recurseStringGroup(
    const GroupRep& other,
    StringGroupRep::iterator &it,
    StringGroupRep::const_iterator &oit,
    uint8_t level
)
{
    while (it != stringGroupRep_.end() && it->level >= level
        && oit != other.stringGroupRep_.end() && oit->level >= level)
    {
        int comp = it->text.compare(oit->text);

        if (comp < 0)
        {
            while (++it != stringGroupRep_.end() && it->level > level);
            continue;
        }
        if (comp > 0)
        {
            stringGroupRep_.insert(it, *(oit++));
            while (oit != other.stringGroupRep_.end() && oit->level > level)
            {
                stringGroupRep_.insert(it, *(oit++));
            }
            continue;
        }
        (it++)->doc_count += (oit++)->doc_count;
        recurseStringGroup(other, it, oit, level + 1);
    }
    if (it != stringGroupRep_.end() && it->level >= level)
    {
        while (++it != stringGroupRep_.end() && it->level >= level);
    }
    else if (oit != other.stringGroupRep_.end() && oit->level >= level)
    {
        stringGroupRep_.insert(it, *(oit++));
        while (oit != other.stringGroupRep_.end() && oit->level >= level)
        {
            stringGroupRep_.insert(it, *(oit++));
        }
    }
}

void GroupRep::mergeNumericGroup(const GroupRep& other)
{
    if (other.numericGroupRep_.empty())
        return;

    if (numericGroupRep_.empty())
    {
        numericGroupRep_.swap(const_cast<GroupRep &>(other).numericGroupRep_);
        return;
    }

    NumericGroupRep::iterator it = numericGroupRep_.begin();
    NumericGroupRep::const_iterator oit = other.numericGroupRep_.begin();

    while (it != numericGroupRep_.end())
    {
        list<pair<double, unsigned int> >::iterator lit = it->second.begin();
        list<pair<double, unsigned int> >::const_iterator olit = oit->second.begin();

        while (true)
        {
            if (olit == oit->second.end())
                break;

            while (lit != it->second.end() && lit->first < olit->first)
            {
                ++lit;
            }

            if (lit == it->second.end())
            {
                it->second.insert(lit, olit, oit->second.end());
                break;
            }

            while (olit != oit->second.end() && lit->first > olit->first)
            {
                it->second.insert(lit, *(olit++));
            }

            while (lit != it->second.end() && olit != oit->second.end() && lit->first == olit->first)
            {
                (lit++)->second += (olit++)->second;
            }
        }

        ++it;
        ++oit;
    }
}

void GroupRep::mergeNumericRangeGroup(const GroupRep& other)
{
    if (other.numericRangeGroupRep_.empty())
        return;

    if (numericRangeGroupRep_.empty())
    {
        numericRangeGroupRep_.swap(const_cast<GroupRep &>(other).numericRangeGroupRep_);
        return;
    }

    NumericRangeGroupRep::iterator it = numericRangeGroupRep_.begin();
    NumericRangeGroupRep::const_iterator oit = other.numericRangeGroupRep_.begin();

    while (it != numericRangeGroupRep_.end())
    {
        if (oit->second.back() == 0)
        {
            ++it;
            ++oit;
            continue;
        }

        vector<unsigned int>::iterator level3 = it->second.begin();
        vector<unsigned int>::iterator level2 = level3 + 100 * LEVEL_1_OF_SEGMENT_TREE;
        vector<unsigned int>::iterator level1 = level2 + 10 * LEVEL_1_OF_SEGMENT_TREE;

        vector<unsigned int>::const_iterator olevel3 = oit->second.begin();
        vector<unsigned int>::const_iterator olevel2 = olevel3 + 100 * LEVEL_1_OF_SEGMENT_TREE;
        vector<unsigned int>::const_iterator olevel1 = olevel2 + 10 * LEVEL_1_OF_SEGMENT_TREE;

        it->second.back() += oit->second.back();
        for (int i = 0; i < LEVEL_1_OF_SEGMENT_TREE; i++)
        {
            if (*olevel1)
            {
                for (int j = 0; j < 10; j++)
                {
                    if (*olevel2)
                    {
                        for (int k = 0; k < 10; k++)
                            *(level3++) += *(olevel3++);

                        *level2 += *olevel2;
                    }
                    else
                    {
                        level3 += 10;
                        olevel3 += 10;
                    }

                    ++level2;
                    ++olevel2;
                }

                *level1 += *olevel1;
            }
            else
            {
                level2 += 10;
                level3 += 100;
                olevel2 += 10;
                olevel3 += 100;
            }

            ++level1;
            ++olevel1;
        }

        ++it;
        ++oit;
    }
}

void GroupRep::toOntologyRepItemList()
{
    for (GroupRep::NumericGroupRep::const_iterator it = numericGroupRep_.begin();
            it != numericGroupRep_.end(); ++it)
    {
        izenelib::util::UString propName(it->first, izenelib::util::UString::UTF_8);
        stringGroupRep_.push_back(faceted::OntologyRepItem(0, propName, 0, 0));
        faceted::OntologyRepItem& topItem = stringGroupRep_.back();
        unsigned int count = 0;
        izenelib::util::UString ustr;

        for (list<pair<double, unsigned int> >::const_iterator lit = it->second.begin();
                lit != it->second.end(); ++lit)
        {
            formatNumericToUStr(lit->first, ustr);
            stringGroupRep_.push_back(faceted::OntologyRepItem(1, ustr, 0, lit->second));
            count += lit->second;
        }
        topItem.doc_count = count;
    }
    numericGroupRep_.clear();

    NumericRangeGroupCounter::toOntologyRepItemList(*this);
    numericRangeGroupRep_.clear();
}

void GroupRep::ResizeTo(const std::map<std::string, int>& grouptop_for_props)
{
    toOntologyRepItemList();
    // first we need to decide the topk group doc_count, since 
    // the items is not sorted, we need traverse it first.
    // each property has a separate topk .
    typedef std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t> > GroupTopKQueueT;
    std::vector<GroupTopKQueueT> topk_doc_count_queue;
    std::string last_property;
    int grouptop = stringGroupRep_.size();
    for(std::list<OntologyRepItem>::iterator it = stringGroupRep_.begin();
        it != stringGroupRep_.end(); ++it)
    {
        if(it->level == 0)
        {
            if(topk_doc_count_queue.size() > 0)
                cout << "topk group doc_count in property : " << last_property << ", " << topk_doc_count_queue.back().top() << std::endl;
            // new property group.
            topk_doc_count_queue.push_back(GroupTopKQueueT());
            it->text.convertString(last_property, izenelib::util::UString::UTF_8);
            std::map<std::string, int>::const_iterator tmpit = grouptop_for_props.find(last_property);
            if(tmpit != grouptop_for_props.end())
            {
                grouptop = tmpit->second;
            }
            else
            {
                grouptop = stringGroupRep_.size();
            }
        }
        else if(it->level == 1)
        {
            topk_doc_count_queue.back().push(it->doc_count);
        }
        if(topk_doc_count_queue.back().size() > (size_t)grouptop)
        {
            topk_doc_count_queue.back().pop();
        }
    }
    if(topk_doc_count_queue.size() > 0 && topk_doc_count_queue.back().size() > 0)
        cout << "topk group doc_count in property : " << last_property << ", " << topk_doc_count_queue.back().top() << std::endl;

    std::list<OntologyRepItem>::iterator prop_erase_start = stringGroupRep_.begin();
    int prop_index = -1;
    bool need_remove_last = false;
    std::string last_removing_group;
    for(std::list<OntologyRepItem>::iterator it = stringGroupRep_.begin();
        it != stringGroupRep_.end(); ++it)
    {
        if (it->level == 0)
        {
            ++prop_index;
            if(need_remove_last)
            {
                it = stringGroupRep_.erase(prop_erase_start, it);
                need_remove_last = false;
            }
        }
        else if(it->level == 1)
        {
            if(need_remove_last)
            {
                it = stringGroupRep_.erase(prop_erase_start, it);
            }
            if(it->doc_count < topk_doc_count_queue[prop_index].top())
            {
                prop_erase_start = it;
                need_remove_last = true;
                it->text.convertString(last_removing_group, izenelib::util::UString::UTF_8);
            }
            else
            {
                need_remove_last = false;
            }
        }
    }
    if (need_remove_last)
    {
        stringGroupRep_.erase(prop_erase_start, stringGroupRep_.end());
    }
}

string GroupRep::ToString() const
{
    stringstream ss;
    for (StringGroupRep::const_iterator it = stringGroupRep_.begin();
            it != stringGroupRep_.end(); ++it)
    {
        OntologyRepItem item = *it;
        string str;
        item.text.convertString(str, izenelib::util::UString::UTF_8);
        ss << (int)item.level << " , " << item.id << " , " << str << " , " << item.doc_count << endl;
    }

    return ss.str();
}

void GroupRep::formatNumericToUStr(double value, izenelib::util::UString& ustr)
{
    ostringstream ss;
    ss << std::fixed << std::setprecision(2) << value;
    ustr.assign(ss.str(), izenelib::util::UString::UTF_8);
}

NS_FACETED_END
