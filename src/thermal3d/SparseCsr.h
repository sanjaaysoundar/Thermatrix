#pragma once
// SparseCsr.h  –  THERMATRIX Phase 1
// Compressed Sparse Row (CSR) matrix for symmetric positive-definite systems.
// namespace thermatrix::t3d
//
// Design:
//   • Stored in standard CSR format: row_ptr, col_idx, values.
//   • Builder (SparseBuilder) accumulates (row,col,val) triplets, then
//     finalises by sorting and summing duplicate entries.
//   • SpMV: y = A * x  (double precision).
//   • The matrix may be rectangular but is used as symmetric/SPD here.
//   • Thread-safe read (SpMV can be OpenMP-parallelised).

#include <vector>
#include <cstddef>
#include <cstdint>
#include <string>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// CSR matrix
// ---------------------------------------------------------------------------
struct SparseCsr {
    int                  nrows { 0 };
    int                  ncols { 0 };
    std::vector<int>     row_ptr;    ///< size nrows+1
    std::vector<int>     col_idx;    ///< size nnz
    std::vector<double>  values;     ///< size nnz

    int nnz() const noexcept { return static_cast<int>(values.size()); }

    // y = A * x  (x.size() == ncols, y.size() == nrows)
    void spmv(const std::vector<double>& x, std::vector<double>& y) const;

    // y += alpha * A * x
    void spmv_add(double alpha,
                  const std::vector<double>& x,
                  std::vector<double>& y) const;

    // Memory footprint estimate in bytes
    std::size_t memory_bytes() const noexcept;

    // Human-readable summary string
    std::string summary() const;

    // Verify structural symmetry: for every (i,j) entry there is a (j,i)
    // Returns true if symmetric (within tol on values), false otherwise.
    bool is_symmetric(double tol = 1e-10) const;

    // Extract diagonal into a vector (length nrows)
    std::vector<double> diagonal() const;

    // Get entry (row, col); returns 0.0 if not stored
    double get(int row, int col) const noexcept;
};

// ---------------------------------------------------------------------------
// Builder: accumulate triplets, then call finalise()
// ---------------------------------------------------------------------------
class SparseBuilder {
public:
    explicit SparseBuilder(int nrows, int ncols)
        : nrows_(nrows), ncols_(ncols) {}

    // Add a scalar contribution: A[row][col] += val
    void add(int row, int col, double val);

    // Add a dense local matrix block (for FEM element stiffness)
    // local_dofs: global DOF indices of the element nodes
    // ke: local stiffness, stored row-major, size n×n
    void add_block(const std::vector<int>& local_dofs,
                   const std::vector<double>& ke);

    // Finalise: sort col_idx, sum duplicates, build SparseCsr
    SparseCsr finalise();

    void clear();

private:
    int nrows_, ncols_;

    struct Triplet {
        int    row;
        int    col;
        double val;
    };
    std::vector<Triplet> triplets_;
};

} // namespace t3d
} // namespace thermatrix
