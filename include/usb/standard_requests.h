#ifndef STANDARD_REQUESTS_H
#define STANDARD_REQUESTS_H

#include "handler.h"
#include "descriptors.h"


class StandardRequests: public Handler
{
public:
    void init(Descriptors const& descriptors) {
        this->descriptors = &descriptors;
    }

    SetupResult on_ctrl_setup_stage() override;

private:
    SetupResult send_descriptor();

    Descriptors const* descriptors;
};

#endif // STANDARD_REQUESTS_H
