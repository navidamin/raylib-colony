// inquiry.h
#ifndef INQUIRY_H
#define INQUIRY_H

#include <string>
#include <queue>
#include <map>
#include "resource_types.h"

class Inquiry {
public:
    ResourceType resource;
    float amount;
    std::string process;  // "building", "upgrading", "transport"
    EntityType& requester; // "Sect, Unit
    EntityType* requesterModule;
    std::string priority; // ""high" , "medium", "low"


    Inquiry(ResourceType res, float amt, std::string proc,
            Entity& req)
        : resource(res), amount(amt), process(proc),
          requester(req), requesterModule(nullptr){}

    Inquiry(ResourceType res, float amt, std::string proc,
            Entity& req, Entity& mod)
        : resource(res), amount(amt), process(proc),
          requester(req), requesterModule(&mod){}

    virtual ~Inquiry() = default;
};

class InquiryResponse {
public:
    bool isGranted;
    ResourceType resource;
    float amount;
    std::string process;  // "building", "upgrading", "transport"
    Entity& requester; // "Sect, Unit
    Entity* requesterModule;

    InquiryResponse(bool grant, ResourceType res, float amt,
                    std::string proc, Entity& req)
                : isGranted(grant), resource(res), amount(amt), process(proc),
                  requester(req), requesterModule(nullptr){}

    InquiryResponse(bool grant, ResourceType res, float amt,
                    std::string proc, Entity& req, Entity& mod)
                : isGranted(grant), resource(res), amount(amt), process(proc),
                  requester(req), requesterModule(&mod){}

    virtual ~InquiryResponse() = default;
};


#endif // INQUIRY_H


