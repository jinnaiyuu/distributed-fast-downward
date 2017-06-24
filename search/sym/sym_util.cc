#include "sym_util.h"
//#include <stdio.h>
#include <cstdio>
using namespace std;

bool domain_has_cond_effects() {
	for (int i = 0; i < g_operators.size(); i++) {
		const vector<PrePost> &pre_post = g_operators[i].get_pre_post();
		for (int j = 0; j < pre_post.size(); j++) {
			const vector<Prevail> &cond = pre_post[j].cond;
			if (!cond.empty()) {
				// Accept conditions that are redundant, but nothing else.
				// In a better world, these would never be included in the
				// input in the first place.
				int var = pre_post[j].var;
				int pre = pre_post[j].pre;
				int post = pre_post[j].post;
				if (pre == -1 && cond.size() == 1 && cond[0].var == var
						&& cond[0].prev != post && g_variable_domain[var] == 2)
					continue;
				return true;
			}
		}
	}
	return false;
}

bool domain_has_axioms() {
	return (!g_axioms.empty());
}

//bool are_mutex(const pair<int, int> &a, const pair<int, int> &b) {
//    if (a.first == b.first) // same variable: mutex iff different value
//        return a.second != b.second;
//    return bool(g_inconsistent_facts[a.first][a.second].count(b));
//}

SymTransition mergeTR(SymTransition tr, const SymTransition & tr2,
		int maxSize) {
	tr.merge(tr2, maxSize);
	return tr;
}
BDD mergeAndBDD(const BDD & bdd, const BDD & bdd2, int maxSize) {
	return bdd.And(bdd2, maxSize);
}
BDD mergeOrBDD(const BDD & bdd, const BDD & bdd2, int maxSize) {
	return bdd.Or(bdd2, maxSize);
}

// Use it for reading file into char array.
// Pretty sure it is suboptimal, but for now let's use this simple implementation.
void file_to_char_array(const std::string filename, char* &array, int& size) {
	FILE * file = fopen(filename.c_str(), "r+");
	if (file == NULL)
		return;
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	char* in = new char[size];
	int bytes_read = fread(in, sizeof(char), size, file);
	printf("filetochararray: %d -> %d\n", size, bytes_read);
	fclose(file);
	array = in;
}

void file_to_char_vector(const std::string filename, vector<char>& buffer) {
	FILE * file = fopen(filename.c_str(), "r+");
	if (file == NULL)
		return;

	// 1. read file length
	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fclose(file);
	buffer.resize(size);

	// 2. read into
	file = fopen(filename.c_str(), "r+");
	int bytes_read = fread(buffer.data(), sizeof(char), size, file);
	printf("file to char vector: %d -> %d\n", size, bytes_read);
	fclose(file);

}

void char_vector_to_file(const std::string filename, vector<char>& buffer) {
	FILE* file = fopen(filename.c_str(), "w+");
	int bytes_written = fwrite(buffer.data(), sizeof(unsigned char),
			buffer.size(), file);
	printf("charvectortofile: %d -> %d\n", (int) buffer.size(), bytes_written);
	fclose(file);
	return;
}
