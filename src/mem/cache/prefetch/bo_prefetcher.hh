#ifndef __MEM_CACHE_PREFETCH_BO_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_BO_PREFETCHER_HH__

#include <functional>
#include <queue>
#include <tuple>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/prefetch/associative_set.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/packet.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{
    class Base;
}

struct BOPrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

class BOPrefetcher : public Queued
{

  protected:
    int SCOREMAX;
    int ROUNDMAX;
    int BADSCORE;
    int DEGREE;
    static const int N_OFFSETS = 52;
    int N_RECENT_REQUESTS;
    bool PARALLEL;

    // MK begin
    static const int OFFSETS[N_OFFSETS];

    uint64_t bestOffset;
    uint64_t currentRound;
    uint64_t currentIndex;
    bool prefetching;

    int scoreBoard[N_OFFSETS] = { 0 };
    Addr* rrTable;

    std::queue<std::tuple<Addr, bool>> notifications;

    void scoreBoardInit();
    int getBestOffset();
    void notifyFill(const PacketPtr &pkt) override;

    void processNotifications();

    uint64_t getIndexRR(const Addr pc) const;

  public:
    BOPrefetcher(const BOPrefetcherParams &p);
    ~BOPrefetcher();

    void calculatePrefetch(const PrefetchInfo &pf1,
                           std::vector<AddrPriority> &addresses) override;

};

} //namespace prefetch
} //namespace gem5


#endif //_MEM_CACHE_PREFETCH_BO_PREFETCHER_HH__
