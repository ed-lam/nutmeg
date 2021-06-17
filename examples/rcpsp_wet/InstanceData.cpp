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

static inline Vector<String> split_string(const String& input, const char delim)
{
    Vector<String> array_of_strings;
    {
        std::stringstream ss(input);
        String token;
        while (std::getline(ss, token, delim))
            array_of_strings.push_back(token);
    }
    return array_of_strings;
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

            if (name == "n_tasks")
            {
                num_jobs = std::atoi(val_str.c_str());
            }
            else if (name == "n_res")
            {
                num_resources = std::atoi(val_str.c_str());
            }
            else if (name == "t_max")
            {
                time_horizon = std::atoi(val_str.c_str());
            }
        }
    }

    // Allocate memory.
    resource_availability.resize(num_resources);
    job_consumption.clear_and_resize(num_resources, num_jobs);
    job_duration.resize(num_jobs);
    job_duedate.resize(num_jobs);
    job_earliness_cost.resize(num_jobs);
    job_tardiness_cost.resize(num_jobs);
    job_successors.resize(num_jobs);

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

            if (name == "d")
            {
                auto it = job_duration.begin();
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
            else if (name == "rr")
            {
                auto it = job_consumption.begin(0);
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
            else if (name == "rc")
            {
                auto it = resource_availability.begin();
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
            else if (name == "suc")
            {
                int job = 0;
                size_t pos;
                String list;
                while ((pos = val_str.find("},{")) != String::npos)
                {
                    list = val_str.substr(0, pos);
//                    println("{{{}}}", list);
                    val_str.erase(0, pos + 3);

                    std::regex r("(\\d+)");
                    std::smatch sm;
                    while (std::regex_search(list, sm, r))
                    {
                        for (size_t i = 1; i < sm.size(); ++i)
                        {
                            job_successors[job].push_back(std::atoi(sm[i].str().c_str()));
                        }
                        list = sm.suffix();
                    }

                    ++job;
                }
                if ((pos = val_str.find("}")) != String::npos)
                {
                    list = val_str.substr(0, pos);
//                    println("{{{}}}", list);

                    std::regex r("(\\d+)");
                    std::smatch sm;
                    while (std::regex_search(list, sm, r))
                    {
                        for (size_t i = 1; i < sm.size(); ++i)
                        {
                            job_successors[job].push_back(std::atoi(sm[i].str().c_str()));
                        }
                        list = sm.suffix();
                    }
                }
            }
            else if (name == "deadline")
            {
                auto it1 = job_duedate.begin();
                auto it2 = job_earliness_cost.begin();
                auto it3 = job_tardiness_cost.begin();
                std::regex r("(\\d+)");
                std::smatch sm;

                int it_idx = 1;
                while (std::regex_search(val_str, sm, r))
                {
                    for (size_t i = 1; i < sm.size(); ++i)
                    {
                        if (it_idx == 1)
                        {
                            *it1 = std::atoi(sm[i].str().c_str());
                            ++it1;
                            it_idx = 2;
                        }
                        else if (it_idx == 2)
                        {
                            *it2 = std::atoi(sm[i].str().c_str());
                            ++it2;
                            it_idx = 3;
                        }
                        else
                        {
                            *it3 = std::atoi(sm[i].str().c_str());
                            ++it3;
                            it_idx = 1;
                        }
                    }
                    val_str = sm.suffix();
                }
            }
        }
    }

    // Offset index of jobs.
    for (auto& list : job_successors)
    {
        for (auto& j : list)
        {
            --j;
        }
    }
}
