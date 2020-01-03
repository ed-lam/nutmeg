#ifndef PS_INSTANCEDATA_H
#define PS_INSTANCEDATA_H

#include "Nutmeg/Includes.h"
#include "Nutmeg/Matrix.h"

using namespace Nutmeg;

class InstanceData
{
  public:
    // Instance data
    Int T;
    Int M;
    Matrix<Int> cost;
    Matrix<Int> duration;
    Matrix<Int> resource;
    Vector<Int> release;
    Vector<Int> deadline;
    Vector<Int> capacity;

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