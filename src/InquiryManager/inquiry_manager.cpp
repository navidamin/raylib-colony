#include "inquiry_manager.h"

InquiryManager::InquiryManager()
{
    // Constructor
}

InquiryResponse InquiryManager::requestInquiry(Inquiry& inquiry){

    bool isGranted = availableResources[inquiry->resource] >= (inquiry->amount + resourceDemandInProcess[inquiry->resource]);
    InquiryResponse response(isGranted, inquiry.resource, inquiry.amount,
                             inquiry.process, inquiry.requester, inquiry.requesterModule);
    if (isGranted) {
        saveInquiry(response);
        resourceDemandInProcess[inquiry.resource] += inquiry.amount;

    } else {
        std::cout << "An inquiry for " << inquiry.amount << "of " << inquiry.resource
                  << "was rejected due to insufficient storage!" << std:: cout;
    }

    return response;
}

void saveInquiry(InquiryResponse response) {
    acceptedlInquiries.push(response);
}
