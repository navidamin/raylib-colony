#pragma once

#include <vector>
#include "prospecting_types.h"

class SampleTray
{
public:
    SampleTray(int tier = 0);

    bool AddSample(const Sample& sample);
    bool RemoveSample(int sampleId);
    Sample* GetSampleById(int sampleId);
    const Sample* GetSampleById(int sampleId) const;
    Sample* GetSampleByIndex(int index);
    const Sample* GetSampleByIndex(int index) const;

    int GetCount() const;
    int GetCapacity() const;
    bool IsFull() const;
    bool IsEmpty() const;

    void SetTier(int tier);
    void AddBonusSlots(int slots);

    const std::vector<Sample>& GetSamples() const;
    std::vector<Sample>& GetSamples();

    int FindLowestValueSampleIndex() const;

private:
    std::vector<Sample> samples;
    int capacity;
    int bonusSlots = 0;

    int NextId();
};
