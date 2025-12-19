#ifndef INQUIRYMANAGER_H
#define INQUIRYMANAGER_H

#include <string>
#include <queue>
#include <map>
#include "resource_types.h"
#include "inquiry.h"

template <typename EntityType>
class InquiryManager
{
public:
    InquiryManager();

    ~InquiryManager() = default;

    std::queue<Inquiry<EntityType>&> acceptedlInquiries;
    std::map<ResourceType, float>& availableResources;
    std::map<ResourceType, float>& resourceDemandInProcess;
    //void processInquiry(Inquiry& inquiry);
    //void generateResponse(Inquiry& inquiry, bool isAvailable);
    InquiryResponse<EntityType> requestInquiry(Inquiry<EntityType>& inquiry);
    InquiryResponse<EntityType> requestInquiry(Inquiry<EntityType>& inquiry);

};

#endif // INQUIRYMANAGER_H
