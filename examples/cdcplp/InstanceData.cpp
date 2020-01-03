#include "InstanceData.h"
#include <fstream>
#include <sstream>
#include <regex>

static inline Vector<String> read_file(const String& instance_file_path)
{
    Vector<String> lines;

    // Open the file.
    std::ifstream f(instance_file_path);
    if (!f)
    {
        err("Cannot open data file {}", instance_file_path);
    }

    // Read the file.
    {
        String line;
        while (std::getline(f, line))
            lines.push_back(line);
    }

    // Return.
    return lines;
}

InstanceData::InstanceData(const String& instance_file_path)
{
    // Read the file.
    Vector<String> lines = read_file(instance_file_path);

    // Read size.
    for (size_t l = 0; l < lines.size() && lines[l].length() > 0; l++)
    {
        const auto& line = lines[l];

        // (\w+)\s*=\s*(.+)\s*;
        std::regex r("(\\w+)\\s*=\\s*(.+)\\s*;");
        std::smatch sm;
        if (std::regex_search(line, sm, r))
        {
            const auto name = sm[1].str();
            const auto val_str = sm[2].str();

            if (name == "num_plants")
            {
                P = std::atoi(val_str.c_str());
            }
            else if (name == "num_clients")
            {
                C = std::atoi(val_str.c_str());
            }
            else if (name == "vehicle_cost")
            {
                vehicle_cost = std::atoi(val_str.c_str());
            }
            else if (name == "max_distance")
            {
                max_distance = std::atoi(val_str.c_str());
            }
        }
    }

    // Allocate memory.
    plant_capacity.resize(P);
    plant_cost.resize(P);
    client_demand.resize(C);
    allocation_cost.clear_and_resize(C, P);
    distance.clear_and_resize(C, P);

    // Read remaining data.
    for (size_t l = 0; l < lines.size() && lines[l].length() > 0; l++)
    {
        const auto& line = lines[l];

        // (\w+)\s*=\s*(.+)\s*;
        std::regex r("(\\w+)\\s*=\\s*(.+)\\s*;");
        std::smatch sm;
        if (std::regex_search(line, sm, r))
        {
            const auto name = sm[1].str();
            auto val_str = sm[2].str();

            if (name == "capacity")
            {
                auto it = plant_capacity.begin();
                std::regex r("(\\d+)");
                std::smatch sm;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        *it = std::atoi(sm[i].str().c_str());
                        ++it;
                    }
                    val_str = sm.suffix();
                }
            }
            else if (name == "open_cost")
            {
                auto it = plant_cost.begin();
                std::regex r("(\\d+)");
                std::smatch sm;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        *it = std::atoi(sm[i].str().c_str());
                        ++it;
                    }
                    val_str = sm.suffix();
                }
            }
            else if (name == "demand")
            {
                auto it = client_demand.begin();
                std::regex r("(\\d+)");
                std::smatch sm;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        *it = std::atoi(sm[i].str().c_str());
                        ++it;
                    }
                    val_str = sm.suffix();
                }
            }
            else if (name == "alloc_cost")
            {
                auto it = allocation_cost.begin(0);
                std::regex r("(\\d+)");
                std::smatch sm;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        *it = std::atoi(sm[i].str().c_str());
                        ++it;
                    }
                    val_str = sm.suffix();
                }
            }
            else if (name == "distance")
            {
                auto it = distance.begin(0);
                std::regex r("(\\d+)");
                std::smatch sm;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        *it = std::atoi(sm[i].str().c_str());
                        ++it;
                    }
                    val_str = sm.suffix();
                }
            }
        }
    }
}
