#ifndef CDCPLP_INSTANCEDATA_H
#define CDCPLP_INSTANCEDATA_H

#include "Nutmeg/Includes.h"
#include "Nutmeg/Matrix.h"

using namespace Nutmeg;

class InstanceData
{
  public:
    // Instance data
    Int P;
    Int C;
    Vector<Int> plant_capacity;
    Vector<Int> plant_cost;
    Vector<Int> client_demand;
    Matrix<Int> allocation_cost;

    Int vehicle_cost;
    Int max_distance;
    Matrix<Int> distance;

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