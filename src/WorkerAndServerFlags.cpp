#include "WorkerAndServerFlags.h"

std::string awsim::WorkerAndServerFlags::to_string(ToWorkerFlags flag)
{
    switch(flag)
    {
        case ToWorkerFlags::WeakStopRequest:
            return "WEAK STOP REQUEST";
        case ToWorkerFlags::StopRequest:
            return "STOP REQUEST";
        case ToWorkerFlags::NumberOfClientsRequest:
            return "NUMBER OF CLIENTS REQUEST";
        default:
            return "UNKNOWN";
    }
}

std::string awsim::WorkerAndServerFlags::to_string(ToServerFlags flag)
{
    switch(flag)
    {
        case ToServerFlags::NumberOfClients:
            return "NUMBER OF CLIENTS";
        case ToServerFlags::WeakStopCompleted:
            return "WEAK STOP COMPLETED";
        case ToServerFlags::Crashed:
            return "CRASHED";
        default:
            return "UNKNOWN";
    }
}
