#ifndef SYM_ENUMS_H
#define SYM_ENUMS_H

#include <string> 
#include <iostream>
#include <vector>

//Auxiliar file to declare all enumerate values Each enumerate has:
//its definiton, ostream << operator and an array with representattive
//strings. The order of the values in the enumerate must correspond
//with the order in the xxxValues vector

enum class MutexType {MUTEX_NOT, MUTEX_AND, MUTEX_RESTRICT,
    MUTEX_NPAND, MUTEX_CONSTRAIN, MUTEX_LICOMP, MUTEX_EDELETION};
std::ostream & operator<<(std::ostream &os, const MutexType & m);
extern const std::vector<std::string> MutexTypeValues;

enum class AbsTRsStrategy{TR_SHRINK, IND_TR_SHRINK, REBUILD_TRS,
    SHRINK_AFTER_IMG};
std::ostream & operator<<(std::ostream &os, const AbsTRsStrategy & a);
extern const std::vector<std::string> AbsTRsStrategyValues;

enum class LinearPDBStrategy {LEVEL, REVERSE, BINARY};
std::ostream & operator<<(std::ostream &os, const LinearPDBStrategy & st);
extern const std::vector<std::string> LinearPDBStrategyValues;

//Types of strategies to choose in which direction search the abstract state spaces
enum class RelaxDirStrategy {FW, BW, BIDIR, SWITCHBACK};
std::ostream & operator<<(std::ostream &os, const RelaxDirStrategy & relaxDir);
extern const std::vector<std::string> RelaxDirStrategyValues; 

enum class Dir {FW, BW, BIDIR};
std::ostream & operator<<(std::ostream &os, const Dir & dir);
extern const std::vector<std::string> DirValues; 


#endif
