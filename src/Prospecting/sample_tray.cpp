#include "sample_tray.h"

SampleTray::SampleTray(int tier)
    : capacity(GetTrayCapacityForTier(tier))
{
}

bool SampleTray::AddSample(const Sample& sample)
{
    int effectiveCapacity = capacity + bonusSlots;
    if (effectiveCapacity > TRAY_MAX_CAPACITY)
        effectiveCapacity = TRAY_MAX_CAPACITY;

    if (static_cast<int>(samples.size()) >= effectiveCapacity)
        return false;

    samples.push_back(sample);
    samples.back().id = NextId();
    return true;
}

bool SampleTray::RemoveSample(int sampleId)
{
    for (auto it = samples.begin(); it != samples.end(); ++it)
    {
        if (it->id == sampleId)
        {
            samples.erase(it);
            return true;
        }
    }
    return false;
}

Sample* SampleTray::GetSampleById(int sampleId)
{
    for (auto& s : samples)
    {
        if (s.id == sampleId) return &s;
    }
    return nullptr;
}

const Sample* SampleTray::GetSampleById(int sampleId) const
{
    for (const auto& s : samples)
    {
        if (s.id == sampleId) return &s;
    }
    return nullptr;
}

Sample* SampleTray::GetSampleByIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(samples.size()))
        return nullptr;
    return &samples[index];
}

const Sample* SampleTray::GetSampleByIndex(int index) const
{
    if (index < 0 || index >= static_cast<int>(samples.size()))
        return nullptr;
    return &samples[index];
}

int SampleTray::GetCount() const
{
    return static_cast<int>(samples.size());
}

int SampleTray::GetCapacity() const
{
    int effectiveCapacity = capacity + bonusSlots;
    if (effectiveCapacity > TRAY_MAX_CAPACITY)
        return TRAY_MAX_CAPACITY;
    return effectiveCapacity;
}

bool SampleTray::IsFull() const
{
    return GetCount() >= GetCapacity();
}

bool SampleTray::IsEmpty() const
{
    return samples.empty();
}

void SampleTray::SetTier(int tier)
{
    capacity = GetTrayCapacityForTier(tier);
}

void SampleTray::AddBonusSlots(int slots)
{
    bonusSlots += slots;
}

const std::vector<Sample>& SampleTray::GetSamples() const
{
    return samples;
}

std::vector<Sample>& SampleTray::GetSamples()
{
    return samples;
}

int SampleTray::FindLowestValueSampleIndex() const
{
    if (samples.empty()) return -1;

    int lowestIdx = 0;
    float lowestValue = samples[0].richness;

    for (int i = 1; i < static_cast<int>(samples.size()); i++)
    {
        if (samples[i].richness < lowestValue)
        {
            lowestValue = samples[i].richness;
            lowestIdx = i;
        }
    }
    return lowestIdx;
}

int SampleTray::NextId()
{
    static int nextId = 1;
    return nextId++;
}
