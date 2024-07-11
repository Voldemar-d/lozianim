#include "coefanim.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

#define MIN_IN_FILE_SIZE 5
#define MAX_IN_FILE_SIZE (1024*1024)

bool coefAnim::parse(const std::string& infile) {
	if (!fs::exists(infile)) {
		std::cout << "Specified input file doesn't exist:\n" << infile << '\n';
		return false;
	}
	const auto fsz = fs::file_size(infile);
	if (fsz < MIN_IN_FILE_SIZE) {
		std::cout << "Specified input file is too small:\n" << infile << '\n';
		return false;
	}
	if (fsz > MAX_IN_FILE_SIZE) {
		std::cout << "Specified input file is too large:\n" << infile << '\n';
		return false;
	}
	std::ifstream infs; infs.open(infile, std::ofstream::binary);
	if (infs.is_open()) {
		// copies all data into buffer
		std::vector<char> buffer(std::istreambuf_iterator<char>(infs), {});
		infs.close(); buffer.push_back(0);
		std::string_view instr(buffer.data());
		const std::string crlfsp("\r\n \t"), crlf("\r\n"), cdec("0123456789");
		const auto start = instr.find_first_not_of(crlfsp), len = instr.length();
		auto retErr = [&infile]() {
			std::cout << "Specified input file has invalid format:\n" << infile << "\n";
			outfmt();
			return false;
		};
		if (std::string::npos == start)
			return retErr();
		auto offs = instr.find_first_of(cdec, start);
		if (offs != start)
			return retErr();
		size_t next, i, j;
		std::string sval[5];
		constexpr auto vcnt = std::size(sval) - 1;
		char ch; bool bPrevSpc;
		auto isNum = [](char ch) {
			return (ch >= '0' && ch <= '9' || ch == '.' || ch == '-');
		};
		auto isSpc = [](char ch) {
			return (ch == ' ' || ch == '\t');
		};
		for (;;) {
			// find next line
			next = instr.find_first_of(crlf, offs);
			if (std::string::npos == next)
				next = len;
			j = 0; for (auto& str : sval) str.clear();
			bPrevSpc = false;
			for (i = offs; i < next; i++) {
				ch = instr[i];
				if (isNum(ch)) {
					sval[j] += ch;
					bPrevSpc = false;
				}
				else if (isSpc(ch)) {
					if (!bPrevSpc) {
						if (0 == j && atoi(sval[j].c_str()) < 1) // number of steps must be positive integer
							return retErr();
						j++;
						if (j > vcnt)
							break;
					}
					bPrevSpc = true;
				}
				else // invalid character
					return retErr();
			}
			if (j < vcnt) // every line must contain 5 numbers
				return retErr();
			m_lst.emplace_back(atoi(sval[0].c_str()), atof(sval[1].c_str()), atof(sval[2].c_str()), atof(sval[3].c_str()), atof(sval[4].c_str()));
			m_nTotal += std::get<0>(m_lst.back());
			// switch to next line
			offs = instr.find_first_not_of(crlfsp, next);
			if (std::string::npos == offs)
				break;
			// find begin of number
			next = instr.find_first_of(cdec, offs);
			if (next != offs)
				return retErr();
		}
		return !m_lst.empty();
	}
	return false;
}

void coefAnim::outfmt()
{
	std::cout << "\nEvery line in coefficient animation text file must consist of:\n";
	std::cout << "{steps} {coef1} {coef1to} {coef2} {coef2to}\n\n";
	std::cout << "Example (B: -1.0 => -1.0, C: 0.5 => 1.0 in 100 steps):\n";
	std::cout << "100 -1.0 -1.0 0.5 1\n";
}
