#include "InstanceData.h"
#include <fstream>
#include <sstream>

static String trim(String& s)
{
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](const char c)
            {
                return !std::isspace(c);
            }));

    s.erase(std::find_if(s.rbegin(), s.rend(), [](const char c)
            {
                return !std::isspace(c);
            }).base(),
            s.end());

    return s;
}

static inline String fix_instance_name(const String& str)
{
    String instance_name(str);
    trim(instance_name);
    std::replace(instance_name.begin(), instance_name.end(), ' ', '_');
    return instance_name;
}

static inline Vector<String> read_file(const String& instance_file_path)
{
    Vector<String> lines;

    // Open the file.
    std::ifstream f(instance_file_path);
    if (!f)
    {
        err("Cannot open instance file {}", instance_file_path);
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
    size_t i = 0;

    // Read parameters.
    Time T = 0;
    {
        // Go to the first line with data.
        while (i < lines.size() && lines[i].length() == 0)
            ++i;
        if (i == lines.size())
        {
            err("Data file {} is empty", instance_file_path);
        }

        // Read parameters.
        for (; i < lines.size() && lines[i].length() > 0; i++)
        {
            // Split the string into parameter name and value.
            Vector<String> str = split_string(lines[i], ':');
            if (str.size() != 2)
            {
                err("Data file {} has invalid parameters section (multiple colons)", instance_file_path);
            }

            // Read the parameter.
            if (str[0] == "DataName" || str[0] == "InstanceName")
            {
                instance_name = fix_instance_name(str[1]);
            }
            else if (str[0] == "T")
            {
                T = atoi(str[1].c_str());
            }
            else if (str[0] == "C")
            {
                C = atoi(str[1].c_str());
            }
            else if (str[0] == "Q")
            {
                Q = atoi(str[1].c_str());
            }
            else if (str[0] != "R" && str[0] != "ResourceType")
            {
                err("Data file {} has unknown parameter {}", instance_file_path, str[0]);
            }
        }
    }

    // Read locations.
    {
        // Go to the next line with data.
        while (lines[i].length() == 0)
            i++;

        // Read locations table heading row.
        size_t l_col = 0;
        size_t x_col = 0;
        size_t y_col = 0;
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(3);
            for (size_t j = 0; ss.good() && j < 3; j++)
                ss >> vals[j];

            {
                const auto index = find(vals.cbegin(), vals.cend(), "L");
                if (index == vals.cend())
                {
                    err("Cannot find L column in instance file");
                }
                l_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "X");
                if (index == vals.cend())
                {
                    err("Cannot find X column in instance file");
                }
                x_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "Y");
                if (index == vals.cend())
                {
                    err("Cannot find Y column in instance file");
                }
                y_col = index - vals.cbegin();
            }
        }

        // Read locations table.
        for (i++; i < lines.size() && lines[i].length() > 0; i++)
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(3);
            for (size_t j = 0; ss.good() && j < 3; j++)
                ss >> vals[j];

            loc_name.push_back(move(vals[l_col]));
            loc_x.push_back(atoi(vals[x_col].c_str()));
            loc_y.push_back(atoi(vals[y_col].c_str()));
        }
    }
    L = static_cast<LocationNumber>(loc_name.size());

    // Read requests.
    {
        r.emplace_back("R-S");
        l.emplace_back(0);
        a.emplace_back(0);
        b.emplace_back(0);
        s.emplace_back(0);
        q.emplace_back(0);
    }
    {
        // Find next line with data.
        while (lines[i].length() == 0)
            i++;

        // Read requests table heading row.
        size_t r_col = 0;
        size_t l_col = 0;
        size_t a_col = 0;
        size_t b_col = 0;
        size_t s_col = 0;
        size_t q_col = 0;
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(6);
            for (long j = 0; ss.good() && j < 6; j++)
                ss >> vals[j];

            {
                const auto index = find(vals.cbegin(), vals.cend(), "R");
                if (index == vals.cend())
                {
                    err("Cannot find R column in instance file");
                }
                r_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "L");
                if (index == vals.cend())
                {
                    err("Cannot find L column in instance file");
                }
                l_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "A");
                if (index == vals.cend())
                {
                    err("Cannot find A column in instance file");
                }
                a_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "B");
                if (index == vals.cend())
                {
                    err("Cannot find B column in instance file");
                }
                b_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "S");
                if (index == vals.cend())
                {
                    err("Cannot find S column in instance file");
                }
                s_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "Q");
                if (index == vals.cend())
                {
                    err("Cannot find Q column in instance file");
                }
                q_col = index - vals.cbegin();
            }
        }

        // Read requests table.
        for (i++; i < lines.size() && lines[i].length() > 0; i++)
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(6);
            for (long j = 0; ss.good() && j < 6; j++)
                ss >> vals[j];

            r.push_back(move(vals[r_col]));
            a.push_back(atoi(vals[a_col].c_str()));
            b.push_back(atoi(vals[b_col].c_str()));
            s.push_back(atoi(vals[s_col].c_str()));
            q.push_back(atoi(vals[q_col].c_str()));

            const auto index = find(loc_name.cbegin(), loc_name.cend(), vals[l_col]);
            if (index == loc_name.cend())
            {
                err("Request {} has invalid location name {}", r.back(), vals[l_col]);

            }
            l.push_back(static_cast<LocationNumber>(index - loc_name.cbegin()));
        }
    }

    // Duplicate depot node.
    {
        r.emplace_back("R-E");
        l.emplace_back(0);
        a.emplace_back(0);
        b.emplace_back(T);
        s.emplace_back(0);
        q.emplace_back(0);
    }

    // Get request counts.
    N = static_cast<Request>(r.size());
    R = N - 2;

    // Create cost matrix.
    cost_matrix = Matrix<Cost>(N, N);
    for (Request i = 0; i < N; i++)
        for (Request j = 0; j < N; j++)
        {
            const Float d_x = loc_x[l[i]] - loc_x[l[j]];
            const Float d_y = loc_y[l[i]] - loc_y[l[j]];
            cost_matrix(i, j) = ceil(sqrt(d_x * d_x + d_y * d_y));
        }

    // Create time matrix.
    time_matrix = Matrix<Time>(N, N);
    for (Request i = 0; i < N; i++)
        for (Request j = 0; j < N; j++)
        {
            time_matrix(i, j) = cost_matrix(i, j);
        }

    // Check self-cycles are zero.
    for (Request i = 0; i < N; i++)
    {
        if (cost_matrix(i, i) != 0)
        {
            err("Arc ({},{}) has positive cost {:.2f}", i, i, cost_matrix(i, i));
        }
        if (time_matrix(i, i) != 0)
        {
            err("Arc ({},{}) has positive cost {:.2f}", i, i, cost_matrix(i, i));
        }
    }

    // Create service+travel time matrix.
    service_plus_travel_time_matrix = time_matrix;
    for (Request i = 0; i < N; i++)
        for (Request j = 0; j < N; j++)
        {
            service_plus_travel_time_matrix(i, j) += s[i];
        }

    // Tighten time windows.
    for (Request i = 1; i <= R; ++i)
    {
        a[i] = std::max(static_cast<Time>(cost_matrix(0, i)), a[i]);
        b[i] = std::min(b[R + 1] - static_cast<Time>(cost_matrix(i, R + 1)) - s[i], b[i]);
    }
}

void InstanceData::print() const
{
    println("Instance name: {}", instance_name);
    println("R: {}", R);
    println("L: {}", L);
    println("C: {}", C);
    println("");

    println("Cost matrix:");
    cost_matrix.print();
    println("Time matrix:");
    time_matrix.print();

    println("{:>10s}{:>10s}{:>10s}{:>10s}{:>10s}{:>10s}", "R", "L", "A", "B", "S", "Q");
    for (Request i = 0; i < N; ++i)
    {
        println("{:>10s}{:>10s}{:>10d}{:>10d}{:>10d}{:>10d}", r[i], loc_name[l[i]], a[i], b[i], s[i], abs(q[i]));
    }
    println("");
}