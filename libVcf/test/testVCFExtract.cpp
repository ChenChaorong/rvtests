#include "VCFUtil.h"
#include "VCFExtractor.h"

int main(){
    VCFExtractor ve("test.vcf");
    ve.setSiteFreqMin(0.001);
    // while
    // ve.extract();
    // const std::vector<double>& af = ve.getAF();
    // for (int i = 0; i < af.size(); i++) {
    //     fprintf(stdout, "i= %d, AF= %.5f\n", i, af[i]);
    // }
    
    ve.close();
    return 0;
};
