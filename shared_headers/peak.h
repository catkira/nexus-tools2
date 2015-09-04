#ifndef PEAK_H_
#define PEAK_H_

#include <algorithm>
#include <vector>
#include <functional>

template<typename TContainer>
using Range = std::pair<typename TContainer::const_iterator, typename TContainer::const_iterator>;

template <typename TBedRecord>
struct SaveBed
{
    using BedRecord = TBedRecord;
    SaveBed(const std::string& filename) : bedFileOut()
    {
        if (!open(bedFileOut, (filename + ".bed").c_str()))
        {
            std::cerr << "ERROR: Could not open " << filename << " for writing.\n";
            return;
        }
    }
    void write(TBedRecord& record)
    {
        writeRecord(bedFileOut, record);
    }
    void writeHeader(const seqan::CharString& header)
    {
        seqan::write(bedFileOut.iter, header);
    }
    void close()
    {
        seqan::close(bedFileOut);
    }
    seqan::BedFileOut bedFileOut;
};

//struct SingleStrandPosition
//{
//    const int INVALID_ID = -1;
//    const int INVALID_POSITION = -1;
//
//    SingleStrandPosition() :
//        chromosomeID(INVALID_ID), position(INVALID_POSITION) {};
//
//    int chromosomeID;
//    int position;
//};

template <typename TEdgeDistribution>
struct PeakCandidate
{
    PeakCandidate() : score(0), width(0) {};
    void clear()
    {
        score = 0;
    }
    Range<TEdgeDistribution> range;
    typename TEdgeDistribution::const_iterator centerIt;
    unsigned int width;
    double score;
};

template <typename TEdgeDistribution>
double slidingWindowScore(const typename TEdgeDistribution::const_iterator centerIt, Range<TEdgeDistribution> range,
    const unsigned halfWindowWidth, const double ratioTolerance, Range<TEdgeDistribution>& windowRange)
{
    typename TEdgeDistribution::const_iterator runningIt = centerIt;
    windowRange.first = centerIt;
    double score = 0;
    double half1score = 0;
    int distance = 0;
    if (centerIt == range.first)
        return 0;
    bool ok = false;
    while (runningIt != range.first && (ok = calculateDistance(getKey(*runningIt), getKey(*centerIt), distance)) && distance <= static_cast<signed>(halfWindowWidth))
    {
        if (getKey(*runningIt).isReverseStrand())
            score -= getUniqueFrequency(*runningIt);
        else
            score += getUniqueFrequency(*runningIt);
        windowRange.first = runningIt--;
    }
    if (!ok) // score across chromosomes is not supported, return 0
        return 0;
    runningIt = std::next(centerIt, 1);
    if (runningIt == range.second) // if centerIt is last before chromosome end, return 0 
        return 0;
    half1score = score;
    windowRange.second = centerIt;
    while (runningIt != range.second && (ok = calculateDistance(getKey(*centerIt), getKey(*runningIt), distance)) && distance <= static_cast<signed>(halfWindowWidth))
    {
        if (getKey(*runningIt).isReverseStrand())
            score += getUniqueFrequency(*runningIt);
        else
            score -= getUniqueFrequency(*runningIt);
        windowRange.second = runningIt++;
    }
    if (ratioTolerance != 0)
    {
        double ratio = static_cast<double>(score - half1score) / static_cast<double>(half1score);
        if (ratio < (1 - ratioTolerance) || ratio >(1 + ratioTolerance))
        {
            //if (getPosition(getKey(*centerIt)) > 2456589 && getPosition(getKey(*centerIt)) < 2456631)
            //    std::cout << "ratio fail: " << ratio <<std::endl;
            return 0;
        }
    }
    if (!ok) // score across chromosomes is not supported
        return 0;
    return score;    // assume return value optimization
}

// range has to be within the same chromosome
template <typename TRange, typename TLambda, typename TPeakCandidate>
void plateauAdjustment(const TRange& range, TLambda& calcScore, TPeakCandidate& peakCandidate)
{
    // assert range.first.chromosome == range.second.chromosome
    unsigned int plateauCount = 0;
    TRange tempSlidingWindowRange;
    TRange plateauRange;
    //const auto strand = getStrand(peakCandidate.centerIt->first);
    auto prevIt = range.first;
    plateauRange = range;
    bool startDetected = false;
    // maybe better use find_if
    for (auto it = range.first; it != range.second; prevIt = it++)
    {
        //if (getStrand(it->first) != strand)
        //    continue;
        if (getKey(*range.first).getPosition() > 2456589 && getKey(*range.first).getPosition() < 2456650)
        {
            std::cout << "startPos: " << getKey(*range.first).getPosition()
                << " endPos: " << getKey(*range.second).getPosition()
                << " itPos: " << getKey(*it).getPosition()
                << " score: " << calcScore(it, tempSlidingWindowRange) << std::endl;
        }

        if (!startDetected && calcScore(it, tempSlidingWindowRange) > (peakCandidate.score * 0.9))
        {
            plateauRange.first = it;
            startDetected = true;
        }
        else if (startDetected && calcScore(it, tempSlidingWindowRange) < (peakCandidate.score * 0.9))
        {
            plateauRange.second = prevIt;
            break;
        }
    }
    // find element which is closest to the center of plateauRange
    const auto startPos = getKey(*plateauRange.first).getPosition();
    const auto endPos = getKey(*plateauRange.second).getPosition();
    const auto midPos = (startPos + endPos) / 2;
    auto minPair = std::make_pair(midPos - startPos, plateauRange.first);

    // can not use for_each here, because access to iterator is needed
    for (auto it = plateauRange.first; it != plateauRange.second; ++it)
    {
        if (abs(midPos - getKey(*it).getPosition()) < minPair.first)
            minPair.second = it;
    }

    if (startPos > 2456589 && startPos < 2456650)
    {
        std::cout << "startPos: " << startPos << " endPos: " << endPos << std::endl;
    }
    peakCandidate.width = endPos - startPos;
    peakCandidate.centerIt = minPair.second;
}

template <typename TEdgeDistribution, typename TCalcScore>
void collectForwardCandidates(const Range<TEdgeDistribution> range, TCalcScore calcScore,
    const double scoreLimit, const unsigned halfWindowWidth, typename std::vector<PeakCandidate<TEdgeDistribution>>& candidatePositions)
{
    double tempScore = 0;
    int checkAhead = 0;
    PeakCandidate<TEdgeDistribution> peakCandidate;
    Range<TEdgeDistribution> tempSlidingWindowRange;
    auto prevIt = range.first;
    for (typename TEdgeDistribution::const_iterator it = range.first; it != range.second; prevIt=it++)
    {
        //if (prevIt != it && getPosition(getKey(*prevIt)) == getPosition(getKey(*it)))   // check if the next object is on the same position, but only another strand
        //    continue;
        tempScore = calcScore(it, tempSlidingWindowRange);
        if (tempScore >= scoreLimit && peakCandidate.score == 0)    // scan until first match
        {
            checkAhead = halfWindowWidth;    // start checkAhead
            peakCandidate.score = tempScore;
            peakCandidate.range = tempSlidingWindowRange;
            peakCandidate.centerIt = it;
        }
        if(checkAhead > 0)  // after first match, checkAhead for better peak candidates
        {
            if (tempScore > peakCandidate.score)  // is the current candidate better
            {
                peakCandidate.score = tempScore;
                peakCandidate.range = tempSlidingWindowRange;
                peakCandidate.centerIt = it;
            }
            --checkAhead;
            continue;
        }
        if (peakCandidate.score > 0)    // checking finished
        {
            PeakCandidate<TEdgeDistribution> prevPeakCandidate = peakCandidate;
            plateauAdjustment(Range<TEdgeDistribution>(peakCandidate.range.first, peakCandidate.range.second), calcScore, peakCandidate);
            //if (peakCandidate.centerIt != prevPeakCandidate.centerIt)
            //{
            //    std::cout << "old center: " << getPosition(getKey(*prevPeakCandidate.centerIt))
            //        << " new center: " << getPosition(getKey(*peakCandidate.centerIt))
            //        << " width: " << peakCandidate.width << std::endl;
            //}
            if (getKey(*prevPeakCandidate.centerIt).getPosition() > 2456589 && getKey(*prevPeakCandidate.centerIt).getPosition() < 2456650)
            {
                std::cout << "\npeak score: " << peakCandidate.score << std::endl;
                std::cout << "pos before plateau adjustment: " << getKey(*prevPeakCandidate.centerIt).getPosition() << std::endl;
                std::cout << "pos after plateau adjustment: " << getKey(*peakCandidate.centerIt).getPosition() << std::endl;
                std::cout << " width: " << peakCandidate.width << std::endl;
            }
            candidatePositions.push_back(peakCandidate);
            it = peakCandidate.range.second;
            tempScore = 0;
            checkAhead = 0;
            peakCandidate.clear();
        }
    }
}

template <typename TEdgeDistribution, typename TWriter, typename TContext>
void forwardCandidatesToBed(const typename std::vector<PeakCandidate<TEdgeDistribution>>& candidatePositions, TWriter& writer, TContext& context)
{
    typename TWriter::BedRecord bedRecord;
    for (const auto& element : candidatePositions)
    {
        bedRecord.rID = element.centerIt->first.getRID();
        bedRecord.ref = contigNames(context)[bedRecord.rID];    // ADL
        bedRecord.beginPos = element.centerIt->first.getPosition();
        bedRecord.endPos = bedRecord.beginPos + 1;
        bedRecord.name = std::to_string(element.score); // abuse name as score parameter in BedGraph

        writer.write(bedRecord);
    }
}

template <typename TPeakCandidatesVector, typename TSelector, typename TBindingLengthDistribution>
void calculateBindingLengthDistribution(const TPeakCandidatesVector& peakCandidatesVector, TSelector selector, TBindingLengthDistribution& bindingLengthDistribution)
{
    for (const auto it : peakCandidatesVector)
        if (selector(it))
            ++bindingLengthDistribution[it.width];
}

template <typename TPeakCandidatesVector, typename TCalcScore, typename TScoreDistribution>
void calculateScoreDistribution(const TPeakCandidatesVector& peakCandidatesVector, TCalcScore calcScore, const int maxDistance, TScoreDistribution& scoreDistribution)
{
    int distance = 0;
    auto tempRange = peakCandidatesVector.begin()->range;
    for (const auto it : peakCandidatesVector)
        for (auto it2 = it.range.first; it2 != it.range.second; ++it2)
        {
            calculateDistance(getKey(*it2), getKey(*it.centerIt), distance);
            if (abs(distance) > maxDistance)
                continue;
            scoreDistribution[distance + maxDistance] += calcScore(it2, tempRange);
        }
}

template <typename TEdgeDistribution, typename TLengthDistribution>
void calculateQFragLengthDistribution(const TEdgeDistribution& edgeDistribution, TLengthDistribution& lengthDistribution, seqan::BamFileIn& bamFileIn)
{
    const auto maxDistance = lengthDistribution.size();
    seqan::BedRecord<seqan::Bed4> bedRecord;
                // I think this -1 is not neccessary, but its here to reproduce the data from the CHipNexus paper exactly
    SaveBed<seqan::BedRecord<seqan::Bed4>> saveBed("P:\\length20Frags");
    for (auto it = edgeDistribution.begin(); it != edgeDistribution.end(); ++it)
    {
        auto tempIt = std::next(it, 1);
        int distance = 0;
        if (getKey(*it).isReverseStrand())
            continue;
        bedRecord.rID = getKey(*it).getRID();
        bedRecord.ref = contigNames(bamFileIn.context)[bedRecord.rID];
        while (calculateDistance(getKey(*it), getKey(*tempIt), distance))
        {
            if (distance > maxDistance)
                break;
            if (getKey(*tempIt).isReverseStrand())
            {
                //lengthDistribution[distance] += getUniqueFrequency(*it) * getUniqueFrequency(*tempIt);
                if (distance == 20)
                {
                    bedRecord.beginPos = getKey(*it).getPosition();
                    bedRecord.endPos = getKey(*tempIt).getPosition();
                    bedRecord.name = std::to_string(getUniqueFrequency(*it) * getUniqueFrequency(*tempIt)); // abuse name as val parameter in BedGraph
                    saveBed.write(bedRecord);
                }

                lengthDistribution[distance] ++;
            }
            tempIt++;
        }
    }
}

template <typename TEdgeDistribution, typename TCalcScore, typename TScoreDistribution>
void calculateScoreDistribution2(const TEdgeDistribution& edgeDistribution, TCalcScore calcScore, const int maxDistance, TScoreDistribution& scoreDistribution)
{
    int distance = 0;
    auto tempRange = Range<TEdgeDistribution>(edgeDistribution.begin(), edgeDistribution.end());

    for (unsigned int width = 0; width < 50; ++width)
    {
        for (auto it = edgeDistribution.begin(); it != edgeDistribution.end(); ++it)
            scoreDistribution[width] += calcScore(it, tempRange, width);
    }
}


#endif  // #ifndef PEAK_H_
