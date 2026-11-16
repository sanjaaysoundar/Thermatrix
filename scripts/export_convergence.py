#!/usr/bin/env python3
"""
export_convergence.py
Computes Richardson extrapolation and Grid Convergence Index (GCI)
strictly according to Celik et al. (2008) / ASME V&V 20-2009 standards.

Validity criteria:
1. Monotonic convergence: (phi_2 - phi_1) and (phi_3 - phi_2) must have the same sign.
2. Decreasing differences: |phi_2 - phi_1| < |phi_3 - phi_2|.
3. Apparent order p within physically meaningful range: p in [0.5, 6.0].

If any test fails, GCI is marked "not valid" with the exact mathematical reason.
"""
import math
import os
import pandas as pd

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
RESULTS_DIR = os.path.join(BASE_DIR, "results")
FINAL_RESULTS_DIR = os.path.join(BASE_DIR, "THERMATRIX_ASSIGNMENT_2", "Results")

# Mesh cell/node counts from actual runs
# Grid 1 = Fine, Grid 2 = Medium, Grid 3 = Coarse
# Refinement ratio r = (N_fine / N_coarse)^(1/3)
GRID_N = {
    'FEM': {'1_fine': 11799, '2_med': 5568, '3_coarse': 2844, 'finer': 18882},
    'FDM': {'1_fine': 487620, '2_med': 145080, '3_coarse': 20700},
    'FVM': {'1_fine': 36671, '2_med': 16134, '3_coarse': 8249}
}

# Fine Case A results from complexity_study.csv / r4_analysis
DATA = [
    # Solver, Quantity, Coarse (phi_3), Med (phi_2), Fine (phi_1), Finer (phi_0)
    ('FEM', 'T_max_C', 60.5102, 60.5810, 60.8302, 60.8134),
    ('FEM', 'T_avg_vw_C', 56.5685, 56.5692, 56.5700, 56.5704),
    ('FDM', 'T_max_C', 59.8088, 60.4034, 60.3607, None),
    ('FDM', 'T_avg_vw_C', 55.9367, 56.1549, 56.1581, None),
    ('FVM', 'T_max_C', 60.6765, 60.6601, 60.7770, None),
    ('FVM', 'T_avg_vw_C', 56.5819, 56.5818, 56.5791, None),
]

def analyze_gci(solver, qty, phi3, phi2, phi1, phi0):
    N1 = GRID_N[solver]['1_fine']
    N2 = GRID_N[solver]['2_med']
    N3 = GRID_N[solver]['3_coarse']
    
    r21 = (N1 / N2) ** (1.0 / 3.0)
    r32 = (N2 / N3) ** (1.0 / 3.0)
    
    e21 = phi1 - phi2
    e32 = phi2 - phi3
    
    # Check monotonicity
    if (e21 * e32) <= 0.0:
        monotonic = False
        valid = False
        reason = "Non-monotonic: oscillatory differences change sign"
        p_val = "N/A"
        gci21 = "not valid"
        extrap = "N/A"
    else:
        # Same sign
        if abs(e21) >= abs(e32):
            monotonic = True
            valid = False
            reason = "Divergent differences: |e21| >= |e32|"
            p_val = "N/A"
            gci21 = "not valid"
            extrap = "N/A"
        else:
            monotonic = True
            # Iterate for order p
            # p = ln|e32/e21| / ln(r21) + q
            p = abs(math.log(abs(e32 / e21))) / math.log(r21)
            for _ in range(50):
                term1 = (r21 ** p - 1.0)
                term2 = (r32 ** p - 1.0)
                if term1 <= 0 or term2 <= 0:
                    break
                q = math.log(term1 / term2)
                p_next = (math.log(abs(e32 / e21)) + q) / math.log(r21)
                if abs(p_next - p) < 1e-6:
                    p = p_next
                    break
                p = 0.5 * (p + p_next)
            
            p_val = f"{p:.4f}"
            if 0.5 <= p <= 6.0:
                valid = True
                reason = "Valid monotonic convergence"
                phi_ext = phi1 + (phi1 - phi2) / (r21**p - 1.0)
                extrap = f"{phi_ext:.4f}"
                gci = 1.25 * abs((phi1 - phi2) / phi1) / (r21**p - 1.0) * 100.0
                gci21 = f"{gci:.4f}%"
            else:
                valid = False
                reason = f"Apparent order p={p:.2f} outside valid range [0.5, 6.0]"
                gci21 = "not valid"
                phi_ext = phi1 + (phi1 - phi2) / (r21**p - 1.0)
                extrap = f"{phi_ext:.4f} (unverified)"

    return {
        'solver': solver,
        'quantity': qty,
        'phi_coarse': phi3,
        'phi_medium': phi2,
        'phi_fine': phi1,
        'phi_finer_crosscheck': phi0 if phi0 is not None else 'N/A',
        'r21': round(r21, 4),
        'r32': round(r32, 4),
        'e21': round(e21, 6),
        'e32': round(e32, 6),
        'monotonic': monotonic,
        'observed_order_p': p_val,
        'gci_fine_21': gci21,
        'extrapolated_value': extrap,
        'status': 'PASS' if valid else 'NOT_VALID',
        'validity_reason': reason
    }

def main():
    rows = []
    for s, q, p3, p2, p1, p0 in DATA:
        rows.append(analyze_gci(s, q, p3, p2, p1, p0))
    
    df = pd.DataFrame(rows)
    out_csv = os.path.join(RESULTS_DIR, "convergence.csv")
    df.to_csv(out_csv, index=False)
    
    final_csv = os.path.join(FINAL_RESULTS_DIR, "convergence.csv")
    df.to_csv(final_csv, index=False)
    
    print(f"[SUCCESS] Wrote convergence.csv to:\n  {out_csv}\n  {final_csv}\n")
    print(df[['solver', 'quantity', 'monotonic', 'observed_order_p', 'gci_fine_21', 'status', 'validity_reason']].to_string(index=False))

if __name__ == '__main__':
    main()
