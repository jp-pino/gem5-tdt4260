#include "mem/cache/prefetch/tdt_prefetcher.hh"

#include <iomanip>

#include "debug/TDTSimpleCache.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "params/TDTPrefetcher.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

std::ostream & operator<<(std::ostream &os, const std::unordered_map<Addr, uint16_t, RecentRequestsHash> &m)
{
    os << "[";
    for (const auto &p : m)
    {
        os << "0x" << std::setw(2) << std::setfill('0') << std::hex << p.first << ": ";
        os << std::dec << p.second << ' ';
    }
    os << "]";
    return os;
}

const int TDTPrefetcher::OFFSETS[N_OFFSETS] = {
    1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32, 36, 40, 45,
    48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 128, 135,
    144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};

TDTPrefetcher::TDTEntry::TDTEntry()
    : TaggedEntry()
{
    invalidate();
}

void
TDTPrefetcher::TDTEntry::invalidate()
{
    TaggedEntry::invalidate();
}

TDTPrefetcher::TDTPrefetcher(const TDTPrefetcherParams &params)
    : Queued(params),
      bestOffset(0),
      prefetching(false),
      currentRound(0),
      pcTableInfo(params.table_assoc, params.table_entries,
                  params.table_indexing_policy,
                  params.table_replacement_policy)
    {
        scoreBoardInit();
        rrTable.clear();
    }

TDTPrefetcher::PCTable*
TDTPrefetcher::findTable(int context)
{
    auto it = pcTables.find(context);
    if (it != pcTables.end())
        return &it->second;

    return allocateNewContext(context);
}

TDTPrefetcher::PCTable*
TDTPrefetcher::allocateNewContext(int context)
{
    auto insertion_result = pcTables.insert(
    std::make_pair(context,
        PCTable(pcTableInfo.assoc, pcTableInfo.numEntries,
        pcTableInfo.indexingPolicy, pcTableInfo.replacementPolicy,
        TDTEntry())));

    return &(insertion_result.first->second);
}

void
TDTPrefetcher::calculatePrefetch(const PrefetchInfo &pfi,
                                 std::vector<AddrPriority> &addresses)
{
    Addr access_addr = pfi.getAddr();
    Addr access_pc = pfi.getPC();
    int context = 0;

    // Reset all scores to 0 at the start of the learning round
    if (currentRound == 0) {
        scoreBoardInit();
    }

    // Increment scores
    for (int i = 0; i < N_OFFSETS; i++) {
        if (rrTable[access_addr - OFFSETS[i]] == ((access_addr >> 8) & 0xFFF)) {
            scoreBoard[i]++;

            // Early stop
            if (scoreBoard[i] >= SCOREMAX) {
                currentRound = 0;
                bestOffset = scoreBoard[i];
                break;
            }
        }
    }

    currentRound++;
    if (currentRound >= ROUNDMAX) {
        currentRound = 0;
        bestOffset = getBestOffset();
        prefetching = bestOffset > BADSCORE;
    }

    // Next line prefetching
    if (prefetching) {
        DPRINTF(TDTSimpleCache, "Prefetching address: 0x%08x\n", (access_addr + bestOffset));
        addresses.push_back(AddrPriority(access_addr + bestOffset, 0));
    }


    // Get matching storage of entries
    // Context is 0 due to single-threaded application
    PCTable* pcTable = findTable(context);

    // Get matching entry from PC
    TDTEntry *entry = pcTable->findEntry(access_pc, false);

    // Check if you have entry
    if (entry != nullptr) {
        // There is an entry
    } else {
        // No entry
    }

    // *Add* new entry
    // All entries exist, you must replace previous with new data
    // Find replacement victim, update info
    TDTEntry* victim = pcTable->findVictim(access_pc);
    victim->lastAddr = access_addr;
    pcTable->insertEntry(access_pc, false, victim);
}

void TDTPrefetcher::notifyFill(const PacketPtr &pkt) {
    std::stringstream ss;
    bool prefetched = hasBeenPrefetched(pkt->getAddr(), false);

    if (prefetched) {
        rrTable[pkt->getAddr()] = ((pkt->getAddr() - bestOffset) >> 8) & 0xFFF;
    }

    if (!prefetching) {
        rrTable[pkt->getAddr()] = (pkt->getAddr() >> 8) & 0xFFF;
    }

    ss << "RecentRequestsTable: [" << rrTable << "]";
    DPRINTF(TDTSimpleCache, "Cache filled (prefetched: %d) (address: 0x%08x) (%s)\n",
        prefetched, pkt->getAddr(), ss.str().c_str());
}

void
TDTPrefetcher::scoreBoardInit() {
    DPRINTF(TDTSimpleCache, "Initializing scoreBoard\n");
    for (int i = 0; i < N_OFFSETS; i++) {
        scoreBoard[i] = 0;
    }
}

int
TDTPrefetcher::getBestOffset() {
    int max = 0, index = 0;
    for (int i = 0; i < N_OFFSETS; i++) {
        int score = scoreBoard[i];
        if (max < score) {
            max = score;
            index = i;
        }
    }
    DPRINTF(TDTSimpleCache, "Get Best Offset (index: %d offset: %d)\n", index, OFFSETS[index]);
    return OFFSETS[index];
}

uint32_t
TDTPrefetcherHashedSetAssociative::extractSet(const Addr pc) const
{
    const Addr hash1 = pc >> 1;
    const Addr hash2 = hash1 >> tagShift;
    return (hash1 ^ hash2) & setMask;
}

Addr
TDTPrefetcherHashedSetAssociative::extractTag(const Addr addr) const
{
    return addr;
}

}
}
