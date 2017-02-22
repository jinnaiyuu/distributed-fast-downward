#include "sym_enums.h" 

std::ostream & operator<<(std::ostream &os, const LinearPDBStrategy & st){
  switch(st){
  case LinearPDBStrategy::LEVEL: return os << "level";
  case LinearPDBStrategy::REVERSE:return os << "reverse";
  case LinearPDBStrategy::BINARY: return os << "binary_search";
  default:
    std::cerr << "Name of LinearPDBStrategy not known";
    exit(-1);
  }
}

std::ostream & operator<<(std::ostream &os, const RelaxDirStrategy & relaxDir){
  switch (relaxDir){
  case RelaxDirStrategy::FW: return os << "fw"; 
  case RelaxDirStrategy::BW: return os << "bw"; 
  case RelaxDirStrategy::BIDIR: return os << "bd"; 
  case RelaxDirStrategy::SWITCHBACK: return os << "sw"; 
  default:
    std::cerr << "Name of RelaxDirStrategy not known";
    exit(-1);
  }
}

std::ostream & operator<<(std::ostream &os, const Dir & dir){
  switch (dir){
  case Dir::FW: return os << "fw"; 
  case Dir::BW: return os << "bw"; 
  case Dir::BIDIR: return os << "bd"; 
  default:
    std::cerr << "Name of Dir not known";
    exit(-1);
  }
}

std::ostream & operator<<(std::ostream &os, const MutexType & m){
  switch(m){
  case MutexType::MUTEX_NOT: return os << "not";
  case MutexType::MUTEX_EDELETION: return os << "edeletion";
  case MutexType::MUTEX_AND: return os << "and";
  case MutexType::MUTEX_RESTRICT: return os << "restrict";
  case MutexType::MUTEX_NPAND: return os << "npand";
  case MutexType::MUTEX_CONSTRAIN: return os << "constrain";
  case MutexType::MUTEX_LICOMP: return os << "licompaction";    
  default:
    std::cerr << "Name of MutexType not known";
    exit(-1);
  }

}

std::ostream & operator<<(std::ostream &os, const AbsTRsStrategy & a){
  switch(a){
  case AbsTRsStrategy::TR_SHRINK: return os << "tr_shrink";
  case AbsTRsStrategy::IND_TR_SHRINK: return os << "ind_tr_shrink";
  case AbsTRsStrategy::REBUILD_TRS: return os << "rebuild_trs";
  case AbsTRsStrategy::SHRINK_AFTER_IMG: return os << "shrink_after_img";    
  default:
    std::cerr << "Name of AbsTRsStrategy not known";
    exit(-1);
  }
}

const std::vector<std::string> MutexTypeValues {
  "MUTEX_NOT", "MUTEX_AND", "MUTEX_RESTRICT",
  "MUTEX_NPAND", "MUTEX_CONSTRAIN", "MUTEX_LICOMP", "MUTEX_EDELETION"
};

const std::vector<std::string> LinearPDBStrategyValues {
  "LEVEL", "REVERSE", "BINARY"
};

const std::vector<std::string>   AbsTRsStrategyValues {
  "TR_SHRINK", "IND_TR_SHRINK", "REBUILD_TRS","SHRINK_AFTER_IMG"
};

const std::vector<std::string> RelaxDirStrategyValues {
  "FW", "BW", "BIDIR", "SWITCHBACK"
};

const std::vector<std::string> DirValues {
  "FW", "BW", "BIDIR"
};
