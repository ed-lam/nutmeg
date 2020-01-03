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

//static inline Vector<String> split_string(const String& input, const char delim)
//{
//    Vector<String> array_of_strings;
//    {
//        std::stringstream ss(input);
//        String token;
//        while (std::getline(ss, token, delim))
//            array_of_strings.push_back(token);
//    }
//    return array_of_strings;
//}

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

            if (name == "job_count")
            {
                T = std::atoi(val_str.c_str());
            }
            else if (name == "machine_count")
            {
                M = std::atoi(val_str.c_str());
            }
        }
    }

    // Allocate memory.
    cost.clear_and_resize(T, M);
    duration.clear_and_resize(T, M);
    resource.clear_and_resize(T, M);
    release.resize(T);
    deadline.resize(T);
    capacity.resize(M);

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

            if (name == "cost")
            {
                auto it = cost.begin(0);
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
            else if (name == "duration")
            {
                auto it = duration.begin(0);
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
            else if (name == "resource")
            {
                auto it = resource.begin(0);
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
            else if (name == "release")
            {
                auto it = release.begin();
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
            else if (name == "deadline")
            {
                auto it = deadline.begin();
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
            else if (name == "capacities")
            {
                auto it = capacity.begin();
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
