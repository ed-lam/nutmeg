#ifndef RCPSP_WET_INSTANCEDATA_H
#define RCPSP_WET_INSTANCEDATA_H

#include "Nutmeg/Includes.h"
#include "Nutmeg/Matrix.h"

using namespace Nutmeg;

class InstanceData
{
  public:
    // Instance data
    Int time_horizon;
    Int num_resources;
    Vector<Int> resource_availability;
    Int num_jobs;
    Matrix<Int> job_consumption;
    Vector<Int> job_duration;
    Vector<Int> job_duedate;
    Vector<Int> job_earliness_cost;
    Vector<Int> job_tardiness_cost;
    Vector<Vector<Int>> job_successors;

    // Constructors and destructors
    InstanceData(const String& instance_file_path);
    ~InstanceData() = default;
    InstanceData() = delete;
    InstanceData(const InstanceData& instance_data) = delete;
    InstanceData(InstanceData&& instance_data) = delete;
    InstanceData operator=(const InstanceData& instance_data) = delete;
    InstanceData operator=(InstanceData&& instance_data) = delete;
};

#endif