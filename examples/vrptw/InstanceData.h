#ifndef VRPTW_INSTANCEDATA_H
#define VRPTW_INSTANCEDATA_H

#include "Includes.h"
#include "Nutmeg/Matrix.h"

class InstanceData
{
  public:
    // Instance data
    String data_name;
    Load Q;
    Request N;
    Request R;
    Request end;
    Vector<String> r;
    Vector<Position> pos_x;
    Vector<Position> pos_y;
    Vector<Load> q;
    Vector<Time> a;
    Vector<Time> b;
    Vector<Time> s;
    Matrix<Cost> cost_matrix;
    Matrix<Time> time_matrix;

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