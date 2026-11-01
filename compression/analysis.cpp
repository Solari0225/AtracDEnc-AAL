#include "analysis.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace independent_aal { namespace compression {
namespace {
#include "analysis_window.inc"
#include "analysis_window_plus.inc"

// Every operation rounds to binary64, and product/addition are separate.
// This deliberately prevents target-dependent FMA contraction. The actual
// reference x87 control word must be verified before claiming whole-analysis
// equivalence; stage oracle vectors retain that precision distinction.
double multiply(double a,double b){volatile double x=a*b;return x;}
double add(double a,double b){volatile double x=a+b;return x;}
double subtract(double a,double b){volatile double x=a-b;return x;}
double divide(double a,double b){volatile double x=a/b;return x;}

double score(const int32_t* input,size_t count) {
    double sum=0;
    for(size_t i=0;i<count;++i)sum=add(sum,std::abs(double(input[i])));
    return multiply(sum,divide(1.0,double(count)));
}
}

void lpc_autocorrelation(const double* windowed,size_t count,double output[9]) {
    for(size_t lag=0;lag<=8;++lag) {
        const size_t length=count-lag;
        if((lag&1)==0) {
            double even=0,odd=0;
            for(size_t i=0;i<length;i+=2) {
                even=add(even,multiply(windowed[i],windowed[i+lag]));
                odd=add(odd,multiply(windowed[i+1],windowed[i+lag+1]));
            }
            output[lag]=add(even,odd);
        } else {
            double sum=0;
            for(size_t i=0;i<length;++i)sum=add(sum,multiply(windowed[i],windowed[i+lag]));
            output[lag]=sum;
        }
    }
}

int lpc_levinson(const double correlations[9],double direct[8],double reflection[8],double& error) {
    std::fill(direct,direct+8,0.0);
    std::fill(reflection,reflection+8,0.0);
    error=correlations[0];
    if(correlations[0]<1.0)return 2;
    double previous[8]={};
    for(unsigned i=0;i<8;++i) {
        double sum=0;
        for(unsigned j=0;j<i;++j)sum=add(sum,multiply(previous[j],correlations[i-j]));
        const double k=divide(subtract(correlations[i+1],sum),error);
        reflection[i]=k;
        if(!(std::abs(k)<1.0) || !(multiply(correlations[0],1e-7)<error) || error<0.1) {
            std::fill(direct+i,direct+8,0.0);
            std::fill(reflection+i,reflection+8,0.0);
            return 1;
        }
        direct[i]=k;
        for(unsigned j=0;j<i;++j)direct[j]=subtract(previous[j],multiply(previous[i-j-1],k));
        std::copy(direct,direct+i+1,previous);
        error=multiply(subtract(1.0,multiply(k,k)),error);
        if(i>0 && !(std::abs(k)<0.98)) {
            std::fill(direct+i+1,direct+8,0.0);
            std::fill(reflection+i+1,reflection+8,0.0);
            return 1;
        }
    }
    return 0;
}

std::array<int32_t,8> quantize_reflection(const double reflection[8]) {
    std::array<int32_t,8> result{};
    for(unsigned i=0;i<8;++i) {
        if(!std::isfinite(reflection[i]) || std::abs(reflection[i])>1.0)
            throw std::runtime_error("Invalid LPC reflection coefficient.");
        const double value=i<2?multiply(reflection[i],1024.0):
            multiply(std::asin(reflection[i]),20.371832715762604);
        result[i]=int32_t(std::floor(value))+(i<2?1024:32);
    }
    return result;
}

std::array<int32_t,8> coefficients_from_parameters(const std::array<int32_t,8>& parameters) {
    std::array<int32_t,8> reflection{},direct{};
    if(aal_lpc_reflection_from_codes(parameters.data(),reflection.data(),8) ||
       aal_lpc_direct_from_reflection(reflection.data(),direct.data(),8))
        throw std::runtime_error("Invalid LPC coefficient parameters.");
    return direct;
}

ChannelAnalysis analyze_channel(const int32_t* input,size_t count,unsigned width,const double* exact_window) {
    if(!input || (count!=1024 && count!=2048) || width!=16)
        throw std::invalid_argument("LPC analysis requires 1024 or 2048 signed 16-bit samples.");
    if(!exact_window) {
        exact_window=count==1024?kLpcWindow:kLpcWindowPlus;
    }
    for(size_t i=0;i<count;++i)if(input[i]<-32768 || input[i]>32767)
        throw std::range_error("LPC input exceeds the signed sample width.");
    ChannelAnalysis result;
    result.candidate_cost.fill(std::numeric_limits<double>::infinity());
    result.candidate_cost[0]=score(input,count);
    result.residual.assign(input,input+count);
    double best=result.candidate_cost[0];
    const int32_t history[8]={};
    std::vector<int32_t> candidate(count);
    for(unsigned mode=1;mode<=3;++mode) {
        if(aal_fixed_prediction(input,candidate.data(),history,mode,unsigned(count),width,0))
            throw std::runtime_error("Fixed prediction failed.");
        const double cost=score(candidate.data(),count);
        result.candidate_cost[mode]=cost;
        if(cost<best){best=cost;result.mode=mode;result.residual=candidate;}
    }
    std::vector<double> windowed(count);double correlations[9],reflection[8],direct[8],error;
    for(size_t i=0;i<count;++i)windowed[i]=multiply(double(input[i]),exact_window[i]);
    lpc_autocorrelation(windowed.data(),count,correlations);
    result.lpc_status=lpc_levinson(correlations,direct,reflection,error);
    if(result.lpc_status!=2) {
        result.parameters=quantize_reflection(reflection);
        std::array<int32_t,8> quantized_reflection{};
        if(aal_lpc_reflection_from_codes(result.parameters.data(),quantized_reflection.data(),8))
            throw std::runtime_error("LPC parameter quantization failed.");
        result.lpc_coefficients_valid=aal_lpc_direct_from_reflection(quantized_reflection.data(),result.coefficients.data(),8)==0;
        if(result.lpc_coefficients_valid) {
            if(aal_lpc_forward(input,candidate.data(),result.coefficients.data(),history,8,unsigned(count),width))
                throw std::runtime_error("LPC prediction failed.");
            const double cost=score(candidate.data(),count);
            result.candidate_cost[8]=cost;
            if(cost<best){result.mode=8;result.residual=candidate;}
        } else {
            const double zero[8]={};
            result.parameters=quantize_reflection(zero);
            result.coefficients.fill(0);
        }
    }
    return result;
}

std::vector<int32_t> inverse_channel(const int32_t* residual,size_t count,unsigned width,
                                    unsigned mode,const std::array<int32_t,8>& parameters) {
    if(!residual||count>2048||width<1||width>32||mode>8)
        throw std::invalid_argument("Invalid inverse prediction parameters.");
    std::vector<int32_t> output(count);
    const int32_t history[8]={};
    int status=0;
    if(mode==0)std::copy(residual,residual+count,output.begin());
    else if(mode<=7)status=aal_fixed_prediction(residual,output.data(),history,mode,unsigned(count),width,1);
    else {
        const auto coefficients=coefficients_from_parameters(parameters);
        status=aal_lpc_inverse(residual,output.data(),coefficients.data(),history,8,unsigned(count),width);
    }
    if(status)throw std::runtime_error("Inverse prediction failed.");
    return output;
}

}} // namespace independent_aal::compression
