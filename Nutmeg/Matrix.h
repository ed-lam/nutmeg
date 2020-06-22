#ifndef NUTMEG_MATRIX_H
#define NUTMEG_MATRIX_H

#include <valarray>

template<typename T>
class Matrix
{
  protected:
    size_t _rows{0};
    size_t _cols{0};
    std::valarray<T> _data{};

  public:
    // Constructors and destructors
    Matrix(const size_t rows, const size_t cols, const T value = {})
        : _rows(rows),
          _cols(cols),
          _data(value, rows * cols)
    {
    }
    Matrix(const size_t rows, const size_t cols, const T* const* matrix)
        : Matrix<T>(rows, cols)
    {
        for (size_t i = 0; i < _rows; ++i)
            for (size_t j = 0; j < _cols; ++j)
                _data[i * _cols + j] = matrix[i][j];
    }
    Matrix() = default;
    Matrix(const Matrix<T>& other) = default;
    Matrix(Matrix<T>&& other) = default;
    ~Matrix() = default;

    // Assignment
    Matrix<T>& operator=(const Matrix<T>& other) = default;
    Matrix<T>& operator=(Matrix<T>&& other) = default;
    Matrix<T>& operator=(const T& value)
    {
        _data = value;
        return *this;
    }

    // Modifiers
    void clear_and_resize(const size_t rows, const size_t cols, const T value = {})
    {
        _rows = rows;
        _cols = cols;
        _data.resize(0);
        _data.resize(rows * cols, value);
    }

    // Comparison
    inline bool operator==(const Matrix<T>& other) const
    {
        return _rows == other.rows() && _data == other._data;
    }

    // Getters
    inline size_t rows() const { return _rows; }
    inline size_t cols() const { return _cols; }
    inline const T operator()(const size_t i, const size_t j) const
    {
#ifndef NDEBUG
        if (i >= rows() || j >= cols())
        {
            printf("Accessing matrix element (%lu,%lu) is out of bounds\n", i, j);
            std::abort();
        }
#endif
        return _data[i * _cols + j];
    }

    // Setters
    inline T& operator()(const size_t i, const size_t j)
    {
#ifndef NDEBUG
        if (i >= rows() || j >= cols())
        {
            printf("Accessing matrix element (%lu,%lu) is out of bounds\n", i, j);
            std::abort();
        }
#endif
        return _data[i * _cols + j];
    }

    // Row iterators
    inline auto begin(const size_t row) { return (&_data[0]) + (row * cols()); }
    inline auto end(const size_t row) { return (&_data[0]) + ((row + 1) * cols()); }
    inline auto begin(const size_t row) const { return (&_data[0]) + (row * cols()); }
    inline auto end(const size_t row) const { return (&_data[0]) + ((row + 1) * cols()); }
    inline auto cbegin(const size_t row) const { return (&_data[0]) + (row * cols()); }
    inline auto cend(const size_t row) const { return (&_data[0]) + ((row + 1) * cols()); }

    // Print
    inline void print() const
    {
        print_internal(*this);
    }
    void write_to_file(const std::string& output_file_path) const
    {
        write_internal(*this, output_file_path);
    }

  private:
    // Internal print methods
    template<class U>
    static inline void print_internal(const Matrix<U>&)
    {
        fprintf(stderr, "No implementation to print matrix of arbitrary type\n");
    }
    static inline void print_internal(const Matrix<bool>& matrix)
    {
        printf("     ");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf(" %3lu", j);
        printf("\n");
        printf("    +");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf("----");
        printf("\n");

        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            printf("%3lu |", i);
            for (size_t j = 0; j < matrix.cols(); ++j)
            {
                if (matrix(i, j))
                    printf("   1");
                else
                    printf("   -");
            }
            printf("\n");
        }
        printf("\n");
    }
    static inline void print_internal(const Matrix<double>& matrix)
    {
        printf("     ");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf(" %8lu", j);
        printf("\n");
        printf("    +");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf("---------");
        printf("\n");

        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            printf("%3lu |", i);
            for (size_t j = 0; j < matrix.cols(); ++j)
                printf(" %8.1f", matrix(i, j));
            printf("\n");
        }
        printf("\n");
    }
    static inline void print_internal(const Matrix<long>& matrix)
    {
        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            for (size_t j = 0; j < matrix.cols(); ++j)
            {
                const T val = matrix(i, j);
                if (val == std::numeric_limits<T>::max())
                    printf("    max");
                else
                    printf(" %6ld", val);
            }
            printf("\n");
        }
        printf("\n");
    }
    static inline void print_internal(const Matrix<unsigned long>& matrix)
    {
        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            for (size_t j = 0; j < matrix.cols(); ++j)
            {
                const T val = matrix(i, j);
                if (val == std::numeric_limits<T>::max())
                    printf("    max");
                else
                    printf(" %6lu", val);
            }
            printf("\n");
        }
        printf("\n");
    }
    static inline void print_internal(const Matrix<int>& matrix)
    {
        printf("     ");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf(" %5lu", j);
        printf("\n");
        printf("    +");
        for (size_t j = 0; j < matrix.cols(); ++j)
            printf("------");
        printf("\n");

        for (size_t i = 0; i < matrix.rows(); ++i)
        {
            printf("%3lu |", i);
            for (size_t j = 0; j < matrix.cols(); ++j)
            {
                const T val = matrix(i, j);
                if (val == std::numeric_limits<T>::max())
                    printf("   max");
                else
                    printf(" %5d", val);
            }
            printf("\n");
        }
        printf("\n");
    }

    // Internal write methods
    template<class U>
    static inline void write_internal(const Matrix<U>&, const std::string&)
    {
        fprintf(stderr, "No implementation to write matrix of arbitrary type\n");
    }
    void write_internal(const Matrix<bool>& matrix, const std::string& output_file_path) const
    {
        // Open file.
        auto f = fopen(output_file_path.c_str(), "w");
        if (!f)
        {
            fprintf(stderr, "Cannot write matrix to file %s", output_file_path.c_str());
            std::abort();
        }

        // Write to file.
        for (size_t i = 0; i < rows(); ++i)
        {
            for (size_t j = 0; j < cols(); ++j)
                fprintf(f, " %9u,", matrix(i, j));
            fprintf(f, "\n");
        }

        // Close file.
        fclose(f);
    }
    void write_internal(const Matrix<double>& matrix, const std::string& output_file_path) const
    {
        // Open file.
        auto f = fopen(output_file_path.c_str(), "w");
        if (!f)
        {
            fprintf(stderr, "Cannot write matrix to file %s", output_file_path.c_str());
            std::abort();
        }

        // Write to file.
        for (size_t i = 0; i < rows(); ++i)
        {
            for (size_t j = 0; j < cols(); ++j)
                fprintf(f, " %9.3f,", matrix(i, j));
            fprintf(f, "\n");
        }

        // Close file.
        fclose(f);
    }
};

#endif
