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

static inline String fix_data_name(const String& str)
{
    String data_name(str);
    trim(data_name);
    std::replace(data_name.begin(), data_name.end(), ' ', '_');
    return data_name;
}

static inline Vector<String> read_file(const String& instance_file_path)
{
    Vector<String> lines;

    // Open the file.
    std::ifstream f(instance_file_path);
    if (!f)
    {
        err("Cannot open data file %s", instance_file_path.c_str());
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
    {
        // Go to the first line with data.
        while (lines[i].length() == 0)
            ++i;

        // Read parameters.
        for (; i < lines.size() && lines[i].length() > 0; i++)
        {
            // Split the string into parameter name and value.
            Vector<String> str = split_string(lines[i], ':');
            if (str.size() != 2)
            {
                err("Data file {} has invalid parameters section (multiple colons)",
                    instance_file_path);
            }

            // Read the parameter.
            if (str[0] == "DataName" || str[0] == "InstanceName")
            {
                data_name = fix_data_name(str[1]);
            }
            else if (str[0] == "Q")
            {
                Q = atoi(str[1].c_str());
            }
            else
            {
                err("Data file {} has unknown parameter {}",
                    instance_file_path,
                    str[0]);
            }
        }
    }

    // Read requests.
    {
        // Go to the next line with data.
        while (lines[i].length() == 0)
            ++i;

        // Read requests table heading row.
        size_t r_col = 0;
        size_t x_col = 0;
        size_t y_col = 0;
        size_t q_col = 0;
        size_t a_col = 0;
        size_t b_col = 0;
        size_t s_col = 0;
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(7);
            for (size_t j = 0; ss.good() && j < 7; j++)
                ss >> vals[j];

            {
                const auto index = find(vals.cbegin(), vals.cend(), "R");
                if (index == vals.cend())
                {
                    err("Cannot find R column in data file");
                }
                r_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "X");
                if (index == vals.cend())
                {
                    err("Cannot find X column in data file");
                }
                x_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "Y");
                if (index == vals.cend())
                {
                    err("Cannot find Y column in data file");
                }
                y_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "Q");
                if (index == vals.cend())
                {
                    err("Cannot find Q column in data file");
                }
                q_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "A");
                if (index == vals.cend())
                {
                    err("Cannot find A column in data file");
                }
                a_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "B");
                if (index == vals.cend())
                {
                    err("Cannot find B column in data file");
                }
                b_col = index - vals.cbegin();
            }
            {
                const auto index = find(vals.cbegin(), vals.cend(), "S");
                if (index == vals.cend())
                {
                    err("Cannot find S column in data file");
                }
                s_col = index - vals.cbegin();
            }
        }

        // Read requests table.
        for (++i; i < lines.size() && lines[i].length() > 0; i++)
        {
            std::stringstream ss(lines[i]);

            Vector<String> vals(7);
            for (size_t j = 0; ss.good() && j < 7; j++)
                ss >> vals[j];

            r.push_back(std::move(vals[r_col]));
            pos_x.push_back(std::atoi(vals[x_col].c_str()));
            pos_y.push_back(std::atoi(vals[y_col].c_str()));
            q.push_back(std::atoi(vals[q_col].c_str()));
            a.push_back(std::atoi(vals[a_col].c_str()));
            b.push_back(std::atoi(vals[b_col].c_str()));
            s.push_back(std::atoi(vals[s_col].c_str()));
        }
    }

    // Check data.
    if (data_name.length() == 0)
    {
        err("Data file {} contains no data name", instance_file_path);
    }
    if (Q == 0)
    {
        err("Vehicles have zero capacity (Q == 0)");
    }

    // Duplicate depot node.
    r.push_back(r.front());
    pos_x.push_back(pos_x.front());
    pos_y.push_back(pos_y.front());
    q.push_back(q.front());
    a.push_back(a.front());
    b.push_back(b.front());
    s.push_back(s.front());
    N = static_cast<Request>(r.size());
    R = N - 2;
    end = R + 1;

    // Calculate cost matrix and time matrix.
    b[0] = 0;
    {
        // Transform data.
        for (Request i = 0; i <= end; i++)
        {
            pos_x[i] *= 10;
            pos_y[i] *= 10;
            a[i] *= 10;
            b[i] *= 10;
            s[i] *= 10;
        }

        // Create cost matrix.
        cost_matrix = Matrix<Cost>(N, N);
        for (Request i = 0; i < N; i++)
            for (Request j = 0; j < N; j++)
            {
                const Float d_x = pos_x[i] - pos_x[j];
                const Float d_y = pos_y[i] - pos_y[j];
                cost_matrix(i, j) = static_cast<Cost>(floor(sqrt(d_x * d_x + d_y * d_y)));
            }

        // Create travel time matrix.
        time_matrix = cost_matrix;
        for (Request i = 0; i < N; i++)
            for (Request j = 0; j < N; j++)
                time_matrix(i, j) += s[i];

        // Verify that all customers have positive service time.
        for (Request i = 1; i <= R; i++)
        {
            release_assert(s[i] > 0, "Request %d has zero service time", i);
        }
    }

    // Reset self-cycles to zero.
    for (Request i = 0; i <= end; i++)
    {
        cost_matrix(i, i) = 0;
        time_matrix(i, i) = 0;
    }

    // Tighten time windows.
    for (Request i = 1; i <= R; ++i)
    {
        a[i] = std::max(a[i], time_matrix(0, i));
        b[i] = std::min(b[i], b[R + 1] - time_matrix(i, R + 1));
    }
}

void InstanceData::print() const
{
    println("Data name: {}", data_name);
    println("Q: {}", Q);
    println("R: {}", R);
    println("");

    println("         R         X         Y         Q         A         B         S");
    println("----------------------------------------------------------------------");

    for (size_t i = 0, size = r.size(); i < size; i++)
    {
        println(" {:>9s} {:9.0f} {:9.0f} {:9.0f} {:9.0f} {:9.0f} {:9.0f}",
                r[i],
                static_cast<Float>(pos_x[i]) / 10.0,
                static_cast<Float>(pos_y[i]) / 10.0,
                static_cast<Float>(q[i]),
                static_cast<Float>(a[i]) / 10.0,
                static_cast<Float>(b[i]) / 10.0,
                static_cast<Float>(s[i]) / 10.0);
    }

    println("----------------------------------------------------------------------\n");
}
