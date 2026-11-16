// SparseCsr.cpp  –  THERMATRIX Phase 1
// Implementation of CSR matrix and SparseBuilder.

#include "SparseCsr.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// SpMV: y = A * x
// ---------------------------------------------------------------------------
void SparseCsr::spmv(const std::vector<double>& x,
                     std::vector<double>& y) const
{
    assert((int)x.size() == ncols);
    y.assign(nrows, 0.0);
    for (int i = 0; i < nrows; ++i) {
        double s = 0.0;
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p)
            s += values[p] * x[col_idx[p]];
        y[i] = s;
    }
}

// ---------------------------------------------------------------------------
// SpMV: y += alpha * A * x
// ---------------------------------------------------------------------------
void SparseCsr::spmv_add(double alpha,
                          const std::vector<double>& x,
                          std::vector<double>& y) const
{
    assert((int)x.size() == ncols);
    assert((int)y.size() == nrows);
    for (int i = 0; i < nrows; ++i) {
        double s = 0.0;
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p)
            s += values[p] * x[col_idx[p]];
        y[i] += alpha * s;
    }
}

// ---------------------------------------------------------------------------
// Memory estimate
// ---------------------------------------------------------------------------
std::size_t SparseCsr::memory_bytes() const noexcept {
    return sizeof(int)    * row_ptr.size()
         + sizeof(int)    * col_idx.size()
         + sizeof(double) * values.size()
         + sizeof(*this);
}

// ---------------------------------------------------------------------------
// Summary string
// ---------------------------------------------------------------------------
std::string SparseCsr::summary() const {
    std::ostringstream ss;
    ss << "SparseCsr " << nrows << "×" << ncols
       << "  nnz=" << nnz()
       << "  mem=" << memory_bytes() / 1024 << " KB";
    return ss.str();
}

// ---------------------------------------------------------------------------
// Structural symmetry check
// ---------------------------------------------------------------------------
bool SparseCsr::is_symmetric(double tol) const {
    if (nrows != ncols) return false;
    for (int i = 0; i < nrows; ++i) {
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p) {
            int j = col_idx[p];
            double v = values[p];
            // Find (j, i)
            double vt = get(j, i);
            if (std::abs(v - vt) > tol * (1.0 + std::abs(v))) return false;
        }
    }
    return true;
}

double check_symmetry(const SparseCsr& A) {
    if (A.nrows != A.ncols) return 1.0;
    double max_rel_diff = 0.0;
    for (int i = 0; i < A.nrows; ++i) {
        for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p) {
            int j = A.col_idx[p];
            double v_ij = A.values[p];
            double v_ji = A.get(j, i);
            double diff = std::abs(v_ij - v_ji);
            double denom = std::max(std::abs(v_ij), std::abs(v_ji));
            if (denom > 1e-15) {
                max_rel_diff = std::max(max_rel_diff, diff / denom);
            }
        }
    }
    return max_rel_diff;
}

bool check_positive_diagonal(const SparseCsr& A, double* min_diag) {
    auto diag = A.diagonal();
    if (diag.empty()) return false;
    double m = diag[0];
    for (double val : diag) {
        if (val <= 0.0) return false;
        if (val < m) m = val;
    }
    if (min_diag) *min_diag = m;
    return true;
}

// ---------------------------------------------------------------------------
// Extract diagonal
// ---------------------------------------------------------------------------
std::vector<double> SparseCsr::diagonal() const {
    std::vector<double> d(nrows, 0.0);
    for (int i = 0; i < nrows; ++i) {
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p) {
            if (col_idx[p] == i) {
                d[i] = values[p];
                break;
            }
        }
    }
    return d;
}

// ---------------------------------------------------------------------------
// Get entry (row, col) – linear scan per row (OK for verification use only)
// ---------------------------------------------------------------------------
double SparseCsr::get(int row, int col) const noexcept {
    if (row < 0 || row >= nrows) return 0.0;
    for (int p = row_ptr[row]; p < row_ptr[row + 1]; ++p) {
        if (col_idx[p] == col) return values[p];
    }
    return 0.0;
}

// ---------------------------------------------------------------------------
// SparseBuilder::add
// ---------------------------------------------------------------------------
void SparseBuilder::add(int row, int col, double val) {
    assert(row >= 0 && row < nrows_);
    assert(col >= 0 && col < ncols_);
    triplets_.push_back({row, col, val});
}

// ---------------------------------------------------------------------------
// SparseBuilder::add_block
// ---------------------------------------------------------------------------
void SparseBuilder::add_block(const std::vector<int>& local_dofs,
                               const std::vector<double>& ke)
{
    int n = static_cast<int>(local_dofs.size());
    assert((int)ke.size() == n * n);
    for (int i = 0; i < n; ++i) {
        int gi = local_dofs[i];
        for (int j = 0; j < n; ++j) {
            int gj = local_dofs[j];
            double v = ke[i * n + j];
            if (v != 0.0) triplets_.push_back({gi, gj, v});
        }
    }
}

// ---------------------------------------------------------------------------
// SparseBuilder::finalise
//   Sort triplets by (row, col), then sum duplicates into CSR.
// ---------------------------------------------------------------------------
SparseCsr SparseBuilder::finalise() {
    // Sort by row, then column
    std::sort(triplets_.begin(), triplets_.end(),
              [](const Triplet& a, const Triplet& b) {
                  if (a.row != b.row) return a.row < b.row;
                  return a.col < b.col;
              });

    SparseCsr mat;
    mat.nrows = nrows_;
    mat.ncols = ncols_;
    mat.row_ptr.assign(nrows_ + 1, 0);

    // Count non-zeros per row (after merging duplicates)
    // First pass: build merged col/val lists
    struct Entry { int col; double val; };
    std::vector<Entry> entries;
    entries.reserve(triplets_.size());

    for (std::size_t k = 0; k < triplets_.size(); ) {
        int r = triplets_[k].row;
        int c = triplets_[k].col;
        double v = 0.0;
        while (k < triplets_.size() && triplets_[k].row == r && triplets_[k].col == c) {
            v += triplets_[k].val;
            ++k;
        }
        entries.push_back({c, v});
        mat.row_ptr[r + 1]++;
    }

    // Prefix sum row_ptr
    for (int i = 0; i < nrows_; ++i)
        mat.row_ptr[i + 1] += mat.row_ptr[i];

    int nnz = static_cast<int>(entries.size());
    mat.col_idx.resize(nnz);
    mat.values.resize(nnz);
    for (int i = 0; i < nnz; ++i) {
        mat.col_idx[i] = entries[i].col;
        mat.values[i]  = entries[i].val;
    }

    return mat;
}

// ---------------------------------------------------------------------------
// SparseBuilder::clear
// ---------------------------------------------------------------------------
void SparseBuilder::clear() {
    triplets_.clear();
}

} // namespace t3d
} // namespace thermatrix
