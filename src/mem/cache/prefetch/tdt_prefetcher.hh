#ifndef __MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"
#include "params/TDTPrefetcherHashedSetAssociative.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{
    class Base;
}

struct TDTPrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

class TDTPrefetcherHashedSetAssociative : public SetAssociative
{
    protected:
        uint32_t extractSet(const Addr addr) const override;
        Addr extractTag(const Addr addr) const override;

    public:
        TDTPrefetcherHashedSetAssociative(
            const TDTPrefetcherHashedSetAssociativeParams &p)
        : SetAssociative(p)
        {}

        ~TDTPrefetcherHashedSetAssociative() = default;
};

class TDTPrefetcher : public Queued
{

  protected:

    const struct PCTableInfo
    {
        const int assoc;
        const int numEntries;

        BaseIndexingPolicy* const indexingPolicy;
        replacement_policy::Base* const replacementPolicy;

        PCTableInfo(int assoc, int num_entries,
            BaseIndexingPolicy* indexing_policy,
            replacement_policy::Base* repl_policy)
          : assoc(assoc), numEntries(num_entries),
            indexingPolicy(indexing_policy), replacementPolicy(repl_policy)
        {

        }
    } pcTableInfo;

    struct TDTEntry : public TaggedEntry
    {
        TDTEntry();

        Addr lastAddr = 0;

        void invalidate() override;

    };

    const int SCOREMAX = 3;
    const int ROUNDMAX = 3;
    const int BADSCORE = 3;

    // MK begin
    const int offsets [52] = {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32, 36, 40, 45,
    48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 128, 135,
    144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};

    typedef AssociativeSet<TDTEntry> PCTable;
    std::unordered_map<int, PCTable> pcTables;
    std::map<int, int> scoreBoard;

    PCTable* findTable(int context);

    PCTable* allocateNewContext(int context);

    void scoreBoardInit();
    int getBestOffset();

  public:
    TDTPrefetcher(const TDTPrefetcherParams &p);

    void calculatePrefetch(const PrefetchInfo &pf1,
                           std::vector<AddrPriority> &addresses) override;

};

} //namespace prefetch
} //namespace gem5

#endif //_MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__
