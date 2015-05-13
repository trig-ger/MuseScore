#include "Induction.h"
#include "Agent.h"
#include "Event.h"

#include <set>
#include <map>
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>

#include <QtGlobal>


namespace BeatTracker {
namespace {

/** The maximum difference in IOIs which are in the same cluster */
const double clusterWidth = 0.025;

/** The minimum IOI for inclusion in a cluster */
const double minIOI = 0.070;

/** The maximum IOI for inclusion in a cluster */
const double maxIOI = 2.500;

/** The minimum inter-beat interval (IBI), i.e. the maximum tempo
 *  hypothesis that can be returned.
 *  0.30 seconds == 200 BPM
 *  0.25 seconds == 240 BPM
 */
const double minIBI = 0.3;

/** The maximum inter-beat interval (IBI), i.e. the minimum tempo
 *  hypothesis that can be returned.
 *  1.00 seconds ==  60 BPM
 *  0.75 seconds ==  80 BPM
 *  0.60 seconds == 100 BPM
 */
const double maxIBI = 1.0;   // 60 BPM, was 0.75 => 80 BPM

/** The maximum number of tempo hypotheses to return */
const int topN = 10;

/** The size of the window to obtain local tempo */
const double TEMPO_WINDOW_SIZE = 5; // s


        // IOI = inter-onset interval
struct IoiCluster
{
    int ioiCount;
    int score;
};

bool addToClosestCluster(double ioi, std::map<double, IoiCluster> &clusters)
{
            // compare differences with IOIs of two closest clusters
            // and select the minumum difference
    double diff1 = std::numeric_limits<double>::max();
    double diff2 = std::numeric_limits<double>::max();

    const auto it = clusters.upper_bound(ioi);
    if (it != clusters.end())
        diff1 = std::fabs(ioi - it->first);
    if (it != clusters.begin())
        diff2 = std::fabs(ioi - std::prev(it)->first);

    auto found = clusters.end();
    if (diff1 < diff2 && diff1 < clusterWidth)
        found = it;
    else if (diff2 < diff1 && diff2 < clusterWidth)
        found = std::prev(it);

    if (found != clusters.end()) {
                // change ioi of the cluster; as it is a map key,
                //   we need to erase and insert the updated cluster
        const double avgIoi = (found->first * found->second.ioiCount + ioi)
                              / (found->second.ioiCount + 1);
        IoiCluster updCluster;
        updCluster.ioiCount = found->second.ioiCount + 1;
        const auto ins = clusters.insert({avgIoi, updCluster});
        if (!ins.second)  // same ioi already exists in the map
              ins.first->second = updCluster;
        if (ins.first != found)
              clusters.erase(found);

        return true;
    }

    return false;
}

std::map<double, IoiCluster> findClusters(
            size_t beg, size_t end, const std::vector<Event> &events)
{
    Q_ASSERT(beg < end && end <= events.size());

    std::map<double, IoiCluster> clusters;  // <average ioi, cluster>

    for (size_t i = beg; i < end - 1; ++i) {
          const Event &e1 = events[i];

          for (size_t j = i + 1; j < end; ++j) {
                const Event &e2 = events[j];

                Q_ASSERT(e2.time > e1.time);

                const double ioi = e2.time - e1.time;
                if (ioi < minIOI)		// skip short intervals
                      continue;
                if (ioi > maxIOI)		// IOI is too long
                      break;

                if (!addToClosestCluster(ioi, clusters)) {
                              // no suitable cluster; create a new one
                      IoiCluster cluster;
                      cluster.ioiCount = 1;
                      clusters.insert({ioi, cluster});
                      }
                }
          }

    return clusters;
}

void mergeSimilarClusters(std::map<double, IoiCluster> &clusters)
{
    if (clusters.size() <= 1)
        return;

    for (auto it = clusters.begin(); it != std::prev(clusters.end()); ) {
        const IoiCluster &c1 = it->second;
        const auto next = std::next(it);

        if (std::fabs(next->first - it->first) < clusterWidth) {
            const IoiCluster &c2 = next->second;

            if (next->first == it->first) {
                  it->second.ioiCount = c1.ioiCount + c2.ioiCount;
                  it = clusters.erase(next);
                  continue;
                  }
                  // change ioi of the cluster; as it is a map key,
                  //   we need to erase and insert the updated cluster
            const double avgIoi = (it->first * c1.ioiCount + next->first * c2.ioiCount)
                                  / (c1.ioiCount + c2.ioiCount);
            IoiCluster newCluster;
            newCluster.ioiCount = c1.ioiCount + c2.ioiCount;

            clusters.erase(it);
            clusters.erase(next);
            const auto ins = clusters.insert({avgIoi, newCluster});

            Q_ASSERT(ins.second);

            it = ins.first;
            continue;
        }
        ++it;
    }
}

void setInitialScores(std::map<double, IoiCluster> &clusters)
{
    for (auto &i: clusters)
          i.second.score = 10 * i.second.ioiCount;
}

struct DescScore {
    bool operator()(const std::map<double, IoiCluster>::const_iterator &l,
                    const std::map<double, IoiCluster>::const_iterator &r) const
    {
        return l->second.score > r->second.score;
    }
};

std::set<std::map<double, IoiCluster>::const_iterator, DescScore>
findBestNClusters(const std::map<double, IoiCluster> &clusters)
{
    // count of high-scoring clusters
    std::set<std::map<double, IoiCluster>::const_iterator, DescScore> bestN;

    for (auto it = clusters.begin(); it != clusters.end(); ++it) {
        const int score = it->second.score;

        if (bestN.size() < topN) {
            bestN.insert(it);
        }
        else if (score <= (*bestN.begin())->second.score
                 && score > (*std::prev(bestN.end()))->second.score) {
            bestN.insert(it);
            bestN.erase(std::prev(bestN.end()));
        }
    }

    return bestN;
}

// if cluster intervals are multiples of each other
//    then they are related by some degree
int findRelationDegree(double ioi1, double ioi2)
{
    const double maxAvg = std::max(ioi1, ioi2);
    const double minAvg = std::min(ioi1, ioi2);

    const int degree = std::nearbyint(maxAvg / minAvg);
    if (degree < 2 || degree > 8)
        return -1;

    const double diff = std::fabs(minAvg * degree - maxAvg);
    const double tol = (ioi1 > ioi2)
                      ? degree * clusterWidth : clusterWidth;
    if (diff >= tol)
        return -1;

    return degree;
}

void rankRelatedClusters(std::map<double, IoiCluster> &clusters)
{
    for (auto it1 = clusters.begin(); it1 != std::prev(clusters.end()); ++it1) {
        for (auto it2 = std::next(it1); it2 != clusters.end(); ++it2) {

            int degree = findRelationDegree(it1->first, it2->first);
            if (degree < 0)
                continue;

            degree = (degree >= 5) ? 1 : 6 - degree;

            it1->second.score += degree * it2->second.ioiCount;
            it2->second.score += degree * it1->second.ioiCount;
        }
    }
}

std::vector<double> findBeats(
        const std::set<std::map<double, IoiCluster>::const_iterator, DescScore> &bestClusters,
        const std::map<double, IoiCluster> &clusters)
{
    std::vector<double> beats;

    for (const auto &bestIt: bestClusters) {
                // Adjust it, using the size of super- and sub-intervals
        double newSum = bestIt->first * bestIt->second.score;
        int newCount = bestIt->second.ioiCount;
        int newWeight = bestIt->second.score;

        for (const auto &i: clusters) {
            if (i.first == bestIt->first)
                continue;

            double degree = findRelationDegree(bestIt->first, i.first);
            if (degree < 0)
                continue;

            if (bestIt->first < i.first)
                degree = 1.0 / degree;

            newSum += i.first * degree * i.second.score;
            newCount += i.second.ioiCount;
            newWeight += i.second.score;
        }

        double beat = newSum / newWeight;
                // Scale within range ... hope the grouping isn't ternary :(
        while (beat < minIBI)		// Maximum speed
            beat *= 2.0;
        while (beat > maxIBI)		// Minimum speed
            beat /= 2.0;

        if (beat >= minIBI)
            beats.push_back(beat);
    }

    std::sort(beats.begin(), beats.end());

    return beats;
}

} // namespace

BeatMap doBeatInduction(const std::vector<Event> &events)
{
    size_t eventsBeg = 0;
    for (size_t i = 0; i != events.size(); ++i) {
          if (events[i].time < TEMPO_WINDOW_SIZE / 2)
                eventsBeg = i;
          else
                break;
          }

    size_t eventsEnd = events.size();
    const double endTime = events[events.size() - 1].time;
    for (size_t i = events.size(); i > 0; --i) {
          if (endTime - events[i - 1].time < TEMPO_WINDOW_SIZE / 2)
                eventsEnd = i;
          else
                break;
          }

    BeatMap beatMap(eventsBeg, eventsEnd);

    for (size_t i = eventsBeg; i != eventsEnd; ++i) {
          const double centralTime = events[i].time;

          size_t beg = i;
          for (size_t j = i + 1; j > 0; --j) {
                if (centralTime - events[j - 1].time < TEMPO_WINDOW_SIZE / 2)
                      beg = j - 1;
                else
                      break;
                }

          size_t end = i;
          for (size_t j = i + 1; j < events.size(); ++j) {
                end = j;
                if (events[j].time - centralTime > TEMPO_WINDOW_SIZE / 2)
                      break;
                }

          auto clusters = findClusters(beg, end, events);
          if (clusters.empty())
                continue;

          mergeSimilarClusters(clusters);
          setInitialScores(clusters);
                  // find best clusters (as iterators) before changing cluster scores
          const auto bestClusters = findBestNClusters(clusters);
                  // no more cluster insertion/deletion from now
          rankRelatedClusters(clusters);

          std::vector<double> beats = findBeats(bestClusters, clusters);
          if (!beats.empty())
                beatMap.addBeats(i, std::move(beats));
     }

    return beatMap;
}

} // namespace BeatTracker
