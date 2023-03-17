#include "mem/cache/prefetch/bo_prefetcher.hh"

#include <iomanip>

#include "debug/TDTSimpleCache.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "params/BOPrefetcher.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

const int BOPrefetcher::OFFSETS[N_OFFSETS] = {
    1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32, 36, 40, 45,
    48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 128, 135,
    144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};

BOPrefetcher::BOPrefetcher(const BOPrefetcherParams &params)
    : Queued(params)
{
    bestOffset = 0;
    prefetching = false;
    currentRound = 0;
    currentIndex = 0;

    SCOREMAX = params.scoremax;
    ROUNDMAX = params.roundmax;
    BADSCORE = params.badscore;
    DEGREE = params.degree;
    PARALLEL = params.parallel;

    N_RECENT_REQUESTS = (1 << params.n_bits_recent_requests);

    DPRINTF(TDTSimpleCache, "SCOREMAX: %d, ROUNDMAX: %d, BADSCORE: %d, N_RECENT_REQUESTS: %d, DEGREE: %d\n",
        SCOREMAX, ROUNDMAX, BADSCORE, N_RECENT_REQUESTS, DEGREE);

    rrTable = new Addr[N_RECENT_REQUESTS];
    scoreBoardInit();
    for (int i = 0; i < N_RECENT_REQUESTS; i++) {
        rrTable[i] = 0;
    }
}

BOPrefetcher::~BOPrefetcher() {
    delete rrTable;
}

void
BOPrefetcher::processNotifications() {
    std::stringstream ss;

    while (!notifications.empty()) {
        // Get notification
        auto notification = notifications.front();

        // Extract packet information
        Addr pktAddress = std::get<0>(notification);
        bool pktSecure = std::get<1>(notification);

        // Remove from queue
        notifications.pop();

        // Update RRTable algorithm
        bool prefetched = hasBeenPrefetched(pktAddress, pktSecure);
        uint64_t index, address;
        int choice = 0;

        if (!prefetching) {
            choice = 1;
            address = pktAddress;
            index = getIndexRR(address);
            rrTable[index] = (address >> 8) & 0x0FFF;
        } else if (prefetched && samePage((pktAddress - OFFSETS[bestOffset] * blkSize), pktAddress)) {
            choice = 2;
            address = pktAddress - OFFSETS[bestOffset] * blkSize;
            index = getIndexRR(address);
            rrTable[index] = (address >> 8) & 0x0FFF;
        }

        if (choice != 0) {
            ss << "RecentRequestsTable: [";
            for (uint64_t i = 0; i < N_RECENT_REQUESTS; i++) {
                if (rrTable[i] > 0) {
                    ss << "(" << i << ": ";
                    ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << std::dec << rrTable[i] << ") ";
                }
            }
            ss << "]";

            DPRINTF(TDTSimpleCache, "Cache filled (prefetched: %d) (choice: %d) (address: 0x%08x) (index: %d) (%s)\n",
                prefetched, choice, pktAddress, index, ss.str().c_str());
        } else {
            DPRINTF(TDTSimpleCache, "Ignored Fill. RRTable not updated (prefetched: %d) (address: 0x%08x)\n",
                prefetched, pktAddress);
        }
    }

}

void
BOPrefetcher::calculatePrefetch(const PrefetchInfo &pfi,
                                 std::vector<AddrPriority> &addresses)
{
    std::stringstream ss;
    Addr access_addr = pfi.getAddr();
    bool stop = false;

    // Process all notifications
    processNotifications();

    // Reset all scores to 0 at the start of the learning round
    if (currentRound == 0 && (currentIndex == 0 || PARALLEL)) {
        scoreBoardInit();
    }


    // Increment scores
    do {
        if (rrTable[getIndexRR(access_addr - OFFSETS[currentIndex] * blkSize)] == ((access_addr >> 8) & 0x0FFF)) {
            scoreBoard[currentIndex]++;

            // Early stop
            if (scoreBoard[currentIndex] >= SCOREMAX) {
                stop = true;
                bestOffset = currentIndex;
                prefetching = true;
                break;
            }
        }
        // Increment index
        currentIndex++;
    } while (PARALLEL && (currentIndex < N_OFFSETS));



    ss << "Scores Table: [";
    for (uint64_t i = 0; i < N_OFFSETS; i++) {
        if (scoreBoard[i] > 0) {
            ss << "(" << i << " -> " << OFFSETS[i] << ": ";
            ss << scoreBoard[i] << ") ";
        }
    }
    ss << "]";

    DPRINTF(TDTSimpleCache, "Scores updated (round: %d - index: %d) (address: 0x%08x) (%s)\n",
        currentRound, currentIndex, access_addr, ss.str().c_str());



    // Increment round
    if (currentIndex >= N_OFFSETS || PARALLEL) {
        currentIndex = 0;
        currentRound++;
    }

    if (stop || (currentRound > ROUNDMAX)) {
        currentRound = 0;
        currentIndex = 0;
        bestOffset = getBestOffset();
        prefetching = scoreBoard[bestOffset] > BADSCORE;
    }

    // Prefetch
    if (prefetching) {
        DPRINTF(TDTSimpleCache, "Prefetching address: 0x%08x\n", (access_addr + OFFSETS[bestOffset] * blkSize));
        addresses.push_back(AddrPriority(access_addr + OFFSETS[bestOffset] * blkSize, 0));
    } else {
        // Default back to next line if prefetching is off?
        // addresses.push_back(AddrPriority(access_addr + blkSize, 0));
    }
}

void BOPrefetcher::notifyFill(const PacketPtr &pkt) {
    // Check queue
    processNotifications();

    // Add notification to queue
    notifications.push(std::tuple<Addr, bool>(pkt->getAddr(), pkt->isSecure()));
}

void
BOPrefetcher::scoreBoardInit() {
    DPRINTF(TDTSimpleCache, "Initializing scoreBoard\n");
    for (int i = 0; i < N_OFFSETS; i++) {
        scoreBoard[i] = 0;
    }
}

int
BOPrefetcher::getBestOffset() {
    int max = 0, index = 0;
    for (int i = 0; i < N_OFFSETS; i++) {
        int score = scoreBoard[i];
        if (max < score) {
            max = score;
            index = i;
        }
    }
    DPRINTF(TDTSimpleCache, "Get Best Offset (index: %d offset: %d)\n", index, OFFSETS[index]);
    return index;
}

uint64_t
BOPrefetcher::getIndexRR(const Addr addr) const
{
    return (addr & (N_RECENT_REQUESTS - 1)) ^ ((addr >> 8) & (N_RECENT_REQUESTS - 1));
}
}
}
