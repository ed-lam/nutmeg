#ifndef VRPLC_INSTANCEDATA_H
#define VRPLC_INSTANCEDATA_H

#include "Nutmeg/Includes.h"
#include "Nutmeg/Matrix.h"

using namespace Nutmeg;

using Request          = Int;
using Cost             = Int;
using Time             = Int;
using Load             = Int;
using LocationNumber   = Int;
using LocationPosition = Int;
using ResourceCount    = Int;

class InstanceData
{
  public:
    // Instance data
    String instance_name;
    Load Q;
    LocationNumber L;
    ResourceCount C;
    Vector<String> loc_name;
    Vector<LocationPosition> loc_x;
    Vector<LocationPosition> loc_y;
    Request N;
    Request R;
    Matrix<Cost> cost_matrix;
    Matrix<Time> time_matrix;
    Matrix<Time> service_plus_travel_time_matrix;
    Vector<String> r;
    Vector<LocationNumber> l;
    Vector<Time> a;
    Vector<Time> b;
    Vector<Time> s;
    Vector<Load> q;

    // Constructors and destructors
    InstanceData(const String& instance_file_path);
    ~InstanceData() = default;
    InstanceData() = delete;
    InstanceData(const InstanceData& instance_data) = delete;
    InstanceData(InstanceData&& instance_data) = delete;
    InstanceData operator=(const InstanceData& instance_data) = delete;
    InstanceData operator=(InstanceData&& instance_data) = delete;

    // Print
    void print() const;
};

#endif