#ifndef _PLINKOUTPUTFILE_H_
#define _PLINKOUTPUTFILE_H_

#include "VCFUtil.h"
#include "MathMatrix.h"
/****************************/
/*    Binary PLINK format   */
/****************************/
/*
 * Documentation 
 * http://pngu.mgh.harvard.edu/~purcell/plink/binary.shtml 
 * BED (binary PED):
 * BIM (extended MAP file): chromosome, SNP, cM, base-position, allele 1, allele 2
 *    e.g.
 *    1       snp1    0       1       G       A
 * FAM (first 6 columns of PED file)
 *      Family ID
        Individual ID
        Paternal ID
        Maternal ID
        Sex (1=male; 2=female; other=unknown)
        Phenotype
 *    e.g.
 *    1 1 0 0 1 0
 *    
 */
class PlinkOutputFile{
public:
    PlinkOutputFile(const char* fnPrefix) {
        std::string prefix = fnPrefix;
        this->fpBed = fopen( (prefix + ".bed").c_str(), "wb");
        this->fpBim = fopen( (prefix + ".bim").c_str(), "wt");
        this->fpFam = fopen( (prefix + ".fam").c_str(), "wt");
        if (!this->fpBed || !this->fpBim || !this->fpFam){
            REPORT("Cannot create binary PLINK file!");
            abort();
        }
        // write Bed header
        char c;
        // magic number
        c = 0x6c; // 0b01101100;
        fwrite(&c, sizeof(char), 1, this->fpBed);
        c = 0x1b; // 0b00011011;
        fwrite(&c, sizeof(char), 1, this->fpBed);
        // snp major mode
        c = 0x01; //0b00000001;
        fwrite(&c, sizeof(char), 1, this->fpBed);
    };
    ~PlinkOutputFile() {
        fclose(this->fpBed);
        fclose(this->fpBim);
        fclose(this->fpFam);
    };
    void writeHeader(const VCFHeader* h){
        std::vector<std::string> people; 
        h->getPeopleName(&people);
        this->writeFAM(people);
    };
    // @pos is from 0 to 3
    void setGenotype(unsigned char* c, const int pos, const int geno){
        (*c) |= (geno << (pos<<1));
    }

    void writeRecord(VCFRecord* r){
        // write BIM
        // printf("id= %s and its address id = %p\n", r->getID(), r->getID());
        this->writeBIM(r->getChrom(), r->getID(), 0, r->getPos(), r->getRef(), r->getAlt());
        // printf("id= %s and its address id = %p\n", r->getID(), r->getID());

        // write BED
        VCFPeople& people = r->getPeople();
        unsigned char c = 0;
        VCFIndividual* indv;
        int offset;
        for (int i = 0; i < people.size() ; i ++) {
            indv = people[i];
            offset = i & (4 - 1);
            if ((*indv)[0].isHaploid()) { // 0: index of GT
                int a1 = (*indv)[0].getAllele1();
                if (a1 == 0) 
                    setGenotype(&c, offset, HOM_REF);
                else if (a1 == 1)
                    setGenotype(&c, offset, HET);
                else
                    setGenotype(&c, offset, MISSING);
            } else {
                int a1 = (*indv)[0].getAllele1();
                int a2 = (*indv)[0].getAllele2();
                if (a1 == 0) {
                    if (a2 == 0) {
                        //homo ref: 0b00
                    } else if (a2 == 1) {
                        setGenotype(&c, offset, HET); // het: 0b01
                    } else {
                        setGenotype(&c, offset, MISSING); // missing 0b10
                    }
                } else if (a1 == 1) {
                    if (a2 == 0) {
                        setGenotype(&c, offset, HET); // het: 0b01
                    } else if (a2 == 1) {
                        setGenotype(&c, offset, HOM_ALT); // hom alt: 0b11
                    } else {
                        setGenotype(&c, offset, MISSING); // missing
                    }
                }
            }
            if ( offset == 3) { // 3: 4 - 1, so every 4 genotype we will flush 
                fwrite(&c, sizeof(char), 1, this->fpBed);
                c = 0;
            }
        };
        if (people.size() % 4 != 0 ) // remaining some bits
            fwrite(&c, sizeof(char), 1, this->fpBed);
    }
    void writeBIM(const char* chr, const char* id, int mapDist, int pos, const char* ref, const char* alt){
        // printf("In writeBIM(), id = %s and its address is id = %p \n", id, id);
        if (strlen(ref) > 1 || strlen(alt) > 1) {
            fprintf(stdout, "skip with ref = %s and alt = %s\n", ref, alt);
            return;
        }
        std::string chrom = chr;
        if (atoi(chr) > 0) {
            fputs(chr, this->fpBim);
            fputc('\t', this->fpBim);
        } else if (chrom == "X")
            fputs("23\t", this->fpBim);
        else if (chrom == "Y")
            fputs("24\t", this->fpBim);
        else if (chrom == "MT")
            fputs("25\t", this->fpBim);
        else {
            fprintf(stdout, "skip chrom %s\n", chr);
            return;
        }
        if (id && id[0] != '.')
            fprintf(this->fpBim, "%s\t", id);
        else
            fprintf(this->fpBim, "%s:%d\t", chrom.c_str(), pos);

        fprintf(this->fpBim, "0\t");
        fprintf(this->fpBim, "%d\t", pos);
        fprintf(this->fpBim, "%c\t", ref[0]);
        fprintf(this->fpBim, "%c\n", alt[0]);
    };
    void writeFAM(std::vector< std::string >& people){
        for (int i = 0; i < people.size(); i++) {
            fprintf(this->fpFam, "%s\t%s\t0\t0\t0\t-9\n", people[i].c_str(), people[i].c_str());
        };
    };
    // NOTE: m should be: marker x people
    void writeBED(Matrix* mat){ 
        int nPeople = mat->cols;
        int nMarker = mat->rows;
        unsigned char c = 0;
        int offset;
        for (int m = 0; m < nMarker; m++){
            for (int i = 0; i < nPeople ; i ++) {
                offset = i & (4 - 1);
                int geno = (int)((*mat)[m][i]);
                switch(geno){
                case HOM_REF:
                    setGenotype(&c, offset, HET); // het: 0b01                
                    break;
                case HET:
                    setGenotype(&c, offset, HET); // het: 0b01
                    break;
                case HOM_ALT:
                    setGenotype(&c, offset, HOM_ALT); // hom alt: 0b11
                    break;
                default:
                    setGenotype(&c, offset, MISSING); // missing
                    break;
                }
            }
            if ( offset == 3) { // 3: 4 - 1, so every 4 genotype we will flush 
                fwrite(&c, sizeof(char), 1, this->fpBed);
                c = 0;
            }
        };
        if (nPeople % 4 != 0 )
            fwrite(&c, sizeof(char), 1, this->fpBed);
    };
private:
    // we reverse the two bits as defined in PLINK format, 
    // so we can process 2-bit at a time.
    const static unsigned char HOM_REF = 0x0;     //0b00;
    const static unsigned char HET = 0x2;         //0b10;
    const static unsigned char HOM_ALT = 0x3;     //0b11;
    const static unsigned char MISSING = 0x1;     //0b01;


    FILE* fpBed;
    FILE* fpBim;
    FILE* fpFam;
}; //end PlinkOutputFile

#endif /* _PLINKOUTPUTFILE_H_ */