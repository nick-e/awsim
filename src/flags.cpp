#include "flags.h"

std::string awsim::ServerToWorkerFlagToString(ServerToWorkerFlags flag)
{
    switch(flag)
    {
        case ServerToWorkerFlags::WeakStopRequest:
            return "WEAK STOP REQUEST";
        case ServerToWorkerFlags::StopRequest:
            return "STOP REQUEST";
        case ServerToWorkerFlags::NumberOfClientsRequest:
            return "NUMBER OF CLIENTS REQUEST";
        default:
            return "UNKNOWN";
    }
}

std::string awsim::WorkerToServerFlagToString(WorkerToServerFlags flag)
{
    switch(flag)
    {
        case WorkerToServerFlags::NumberOfClients:
            return "NUMBER OF CLIENTS";
        case WorkerToServerFlags::WeakStopCompleted:
            return "WEAK STOP COMPLETED";
        case WorkerToServerFlags::Crashed:
            return "CRASHED";
        default:
            return "UNKNOWN";
    }
}
