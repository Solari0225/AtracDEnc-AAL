#pragma once
#include "independent_lpc.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace independent_aal { namespace compression {

struct ChannelAnalysis {
    unsigned mode=0;
    std::array<int32_t,8> parameters{}; // mode 8: two 11-bit, then six 6-bit codes
    std::array<int32_t,8> coefficients{};
    std::array<double,9> candidate_cost{}; // Mean absolute wrapped residual; unused modes = infinity.
    std::vector<int32_t> residual;
    int lpc_status=2; // Levinson result: 0 complete, 1 early termination, 2 insufficient energy.
    bool lpc_coefficients_valid=false;
};

// Official analysis searches modes 0,1,2,3,8; initial eight-sample history is
// zero. Strictly smaller mean absolute residual wins; equal scores retain the
// earlier mode. AAL3 uses 1024 samples; AALP uses 2048 samples.
ChannelAnalysis analyze_channel(const int32_t* input,size_t count=1024,unsigned width=16,
                                const double* exact_window=nullptr);
std::array<int32_t,8> coefficients_from_parameters(const std::array<int32_t,8>& parameters);
std::vector<int32_t> inverse_channel(const int32_t* residual,size_t count,unsigned width,
                                    unsigned mode,const std::array<int32_t,8>& parameters={});

// Math APIs exposed for exact offline comparisons of each numerical stage.
void lpc_autocorrelation(const double* windowed,size_t count,double output[9]);
int lpc_levinson(const double correlations[9],double direct[8],double reflection[8],double& error);
std::array<int32_t,8> quantize_reflection(const double reflection[8]);

}} // namespace independent_aal::compression
