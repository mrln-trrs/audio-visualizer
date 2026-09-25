#include "analysis/window_function.h"

#include <cmath>

namespace analysis {

AnalysisWindow MakePeriodicHann(int size) {
    constexpr double kPi = 3.14159265358979323846;
    AnalysisWindow w;
    w.coefficients.resize(size);
    double sum = 0.0;
    for (int n = 0; n < size; ++n) {
        const double v = 0.5 * (1.0 - std::cos(2.0 * kPi * n / size));
        w.coefficients[n] = static_cast<float>(v);
        sum += v;
    }
    w.normalization = static_cast<float>(2.0 / sum);
    return w;
}

} // namespace analysis
