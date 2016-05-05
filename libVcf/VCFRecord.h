#ifndef _VCFRECORD_H_
#define _VCFRECORD_H_

#include "CommonFunction.h"
#include "Exception.h"
#include "IO.h"
#include "OrderedMap.h"
#include "RangeList.h"
#include "Utils.h"
#include "VCFBuffer.h"
#include "VCFFunction.h"
#include "VCFHeader.h"
#include "VCFIndividual.h"
#include "VCFInfo.h"

typedef OrderedMap<int, VCFIndividual*> VCFPeople;

class VCFRecord {
 public:
  VCFRecord() { this->hasAccess = false; }

  /**
   * Parse will first make a copy then tokenized the copied one
   * @return 0: if success
   */
  int parse(const std::string vcfLine) {
    this->vcfInfo.reset();
    this->parsed = vcfLine.c_str();
    this->self.line = this->parsed.c_str();
    this->self.beg = 0;
    this->self.end = this->parsed.size();

    // go through VCF sites (first 9 columns)
    int ret;
    if ((ret = this->chrom.parseTill(this->parsed, 0, '\t'))) {
      fprintf(stderr, "Error when parsing CHROM [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->chrom.end] = '\0';

    if ((ret =
             (this->pos.parseTill(this->parsed, this->chrom.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing POS [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->pos.end] = '\0';

    if ((ret = (this->id.parseTill(this->parsed, this->pos.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing ID [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->id.end] = '\0';

    if ((ret = (this->ref.parseTill(this->parsed, this->id.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing REF [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->ref.end] = '\0';

    if ((ret = (this->alt.parseTill(this->parsed, this->ref.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing ALT [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->alt.end] = '\0';

    if ((ret = (this->qual.parseTill(this->parsed, this->alt.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing QUAL [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->qual.end] = '\0';

    if ((ret =
             (this->filt.parseTill(this->parsed, this->qual.end + 1, '\t')))) {
      fprintf(stderr, "Error when parsing FILTER [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->filt.end] = '\0';

    if ((ret =
             (this->info.parseTill(this->parsed, this->filt.end + 1, '\t')))) {
      // either parsing error, or we are dealing with site only VCFs
      if (ret < 0) {  // error happens
        fprintf(stderr, "Error when parsing INFO [ %s ]\n", vcfLine.c_str());
        return -1;
      }
    }
    this->parsed[this->info.end] = '\0';
    this->vcfInfo.parse(this->info);  // lazy parse inside VCFInfo
    if (ret > 0) {                    // for site-only VCFs, just return
      return 0;
    }

    if ((ret = (this->format.parseTill(this->parsed, this->info.end + 1,
                                       '\t')))) {
      fprintf(stderr, "Error when parsing FORMAT [ %s ]\n", vcfLine.c_str());
      return -1;
    }
    this->parsed[this->format.end] = '\0';

    // now comes each individual genotype
    unsigned int idx = 0;  // peopleIdx
    VCFIndividual* p;
    VCFValue indv;
    int beg = this->format.end + 1;
    while ((ret = indv.parseTill(this->parsed, beg, '\t')) == 0) {
      if (idx >= this->allIndv.size()) {
        fprintf(stderr,
                "Expected %d individual but already have %d individual\n",
                (int)this->allIndv.size(), idx);
        fprintf(stderr, "VCF header have LESS people than VCF content!\n");
        return -1;
      }

      this->parsed[indv.end] = '\0';
      p = this->allIndv[idx];
      p->parse(indv);

      beg = indv.end + 1;
      idx++;
    }

    // if ret == 1 menas reaches end of this->parsed
    if (ret != 1) {
      fprintf(stderr, "Parsing error in line: %s\n", this->self.toStr());
      return -1;
    } else {
      this->parsed[indv.end] = '\0';
      p = this->allIndv[idx];
      p->parse(indv);
      idx++;
    }

    if (idx > this->allIndv.size()) {
      fprintf(stderr, "Expected %d individual but already have %d individual\n",
              (int)this->allIndv.size(), idx);
      REPORT("VCF header have MORE people than VCF content!");
    } else if (idx < this->allIndv.size()) {
      fprintf(stderr, "Expected %d individual but only have %d individual\n",
              (int)this->allIndv.size(), idx);
      REPORT("VCF header have LESS people than VCF content!");
      return -1;
    }

    return 0;
  }
  void createIndividual(const std::string& line) {
    std::vector<std::string> sa;
    stringTokenize(line, '\t', &sa);
    if (sa.size() <= 9) {
      // FATAL("not enough people in the VCF (VCF does not contain genotype and
      // individuals?)");
      // let's support site only VCFs
      return;
    }
    for (unsigned int i = 9; i < sa.size(); i++) {
      int idx = i - 9;
      VCFIndividual* p = new VCFIndividual;
      this->allIndv[idx] = p;
      p->setName(sa[i]);
    }
  }
  void deleteIndividual() {
    for (unsigned int i = 0; i < this->allIndv.size(); i++) {
      if (this->allIndv[i]) delete this->allIndv[i];
      this->allIndv[i] = NULL;
    }
  }

  //////////////////////////////////////////////////////////////////////
  // Code related with include/exclude people
  void includePeople(const std::string& name) {
    if (name.size() == 0) return;

    // tokenize @param name by ','
    int beg = 0;
    size_t end = name.find(',');
    std::string s;
    while (end != std::string::npos) {
      s = name.substr(beg, end - beg);
      beg = end + 1;
      end = name.find(',', beg);
      this->includePeople(s);
    }
    s = name.substr(beg, end);

    bool included = false;
    for (unsigned int i = 0; i != this->allIndv.size(); i++) {
      VCFIndividual* p = this->allIndv[i];
      if (p->getName() == s) {
        p->include();
        included = true;
        fprintf(stderr, "Include sample [ %s ].\n", s.c_str());
      }
    }
    if (!included) {
      fprintf(stderr, "Failed to include sample [ %s ] - not in VCF file.\n",
              s.c_str());
    }
    this->hasAccess = false;
  }
  void includePeople(const std::vector<std::string>& v) {
    for (unsigned int i = 0; i < v.size(); i++) {
      this->includePeople(v[i]);
    }
    this->hasAccess = false;
  }
  void includePeopleFromFile(const char* fn) {
    if (!fn || strlen(fn) == 0) return;
    LineReader lr(fn);
    std::vector<std::string> fd;
    while (lr.readLineBySep(&fd, "\t ")) {
      for (unsigned int i = 0; i < fd.size(); i++) this->includePeople(fd[i]);
    }
    this->hasAccess = false;
  }
  void includeAllPeople() {
    for (unsigned int i = 0; i != this->allIndv.size(); i++) {
      VCFIndividual* p = this->allIndv[i];
      p->include();
    }
    this->hasAccess = false;
  }
  void excludePeople(const std::string& name) {
    if (name.empty()) return;
    for (size_t i = 0; i != this->allIndv.size(); i++) {
      VCFIndividual* p = this->allIndv[i];
      if (p->getName() == name) {
        p->exclude();
      }
    }
    this->hasAccess = false;
  }
  void excludePeople(const std::set<std::string>& name) {
    if (name.empty()) return;
    for (size_t i = 0; i != this->allIndv.size(); i++) {
      VCFIndividual* p = this->allIndv[i];
      if (name.count(p->getName())) {
        p->exclude();
      }
    }
    this->hasAccess = false;
  }
  void excludePeople(const std::vector<std::string>& v) {
    std::set<std::string> s;
    makeSet(v, &s);
    this->excludePeople(s);
  }
  void excludePeopleFromFile(const char* fn) {
    if (!fn || strlen(fn) == 0) return;
    LineReader lr(fn);
    std::vector<std::string> fd;
    std::set<std::string> toExclude;
    while (lr.readLineBySep(&fd, "\t ")) {
      for (unsigned int i = 0; i != fd.size(); i++) toExclude.insert(fd[i]);
    }
    this->excludePeople(toExclude);
  }
  void excludeAllPeople() {
    for (unsigned int i = 0; i != this->allIndv.size(); i++) {
      VCFIndividual* p = this->allIndv[i];
      p->exclude();
    }
    this->hasAccess = false;
  }
  VCFInfo& getVCFInfo() { return this->vcfInfo; }
  const VCFValue& getInfoTag(const char* tag, bool* exists) {
    return this->vcfInfo.getTag(tag, exists);
  }
  /**
   * Output this->self, it may contain '\0'
   */
  void output(FILE* fp) const {
    this->self.output(fp);
    fputc('\n', fp);
  }

 public:
  const char* getChrom() const { return this->chrom.toStr(); }
  const int getPos() const { return this->pos.toInt(); }
  const char* getPosStr() const { return this->pos.toStr(); }
  const char* getID() const { return this->id.toStr(); }
  const char* getRef() const { return this->ref.toStr(); }
  const char* getAlt() const { return this->alt.toStr(); }
  const char* getQual() const { return this->qual.toStr(); }
  const int getQualInt() const { return this->qual.toInt(); }
  const int getQualDouble() const { return this->qual.toDouble(); }
  const char* getFilt() const { return this->filt.toStr(); }
  const char* getInfo() const { return this->vcfInfo.toStr(); }
  const char* getFormat() const { return this->format.toStr(); }

  VCFPeople& getPeople() {
    if (!this->hasAccess) {
      this->selectedIndv.clear();
      for (unsigned int i = 0; i < this->allIndv.size(); i++) {
        if (allIndv[i]->isInUse()) {
          this->selectedIndv[this->selectedIndv.size()] = allIndv[i];
        }
      }
      this->hasAccess = true;
    }
    return this->selectedIndv;
  }
  /**
   * You want to know tag "GQ" where FORMAT column is "GT:GQ:PL"
   * then call getFormatIndx("GQ") will @return 1 (0-based index)
   * @return -1 when not found
   */
  int getFormatIndex(const char* s) {
    int b = this->format.beg;
    int e = this->format.end;
    int idx = 0;

    // locate first field
    while (b < e) {
      // check match
      bool match = true;
      for (int i = 0; s[i] != '\0'; i++) {
        if (this->format.line[b + i] != s[i]) {
          match = false;
          break;
        }
      }
      if (match) return idx;

      // skip to next field
      idx++;
      while (this->format.line[b++] != ':') {
        if (b >= e) {
          return -1;
        }
      }
    }
    return -1;
  }
  const VCFValue& getSelf() const { return this->self; }
  void getIncludedPeopleName(std::vector<std::string>* p) {
    VCFPeople& people = getPeople();
    p->clear();
    const int n = people.size();
    for (int i = 0; i < n; ++i) {
      p->push_back(people[i]->getName());
    }
  }

 private:
  VCFPeople allIndv;       // all individual
  VCFPeople selectedIndv;  // user-selected individual

  VCFValue chrom;
  VCFValue pos;
  VCFValue id;
  VCFValue ref;
  VCFValue alt;
  VCFValue qual;
  VCFValue filt;
  VCFValue info;
  VCFValue format;

  VCFInfo vcfInfo;

  // indicates if getPeople() has been called
  bool hasAccess;

  // store parsed results
  VCFBuffer parsed;
  VCFValue
      self;  // a self value points to itself, it contain parsed information

};  // VCFRecord

#endif /* _VCFRECORD_H_ */
