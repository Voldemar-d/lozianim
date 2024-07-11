#pragma once

#include <list>
#include <string>

// {steps, Bfrom, Bto, Cfrom, Cto}
using lstCoef = std::list<std::tuple<int, double, double, double, double>>;

class coefAnim {
public:
	coefAnim() {}
	auto& lst() const { return m_lst; }
	bool parse(const std::string& infile);
	auto total() const { return m_nTotal; }
	static void outfmt();
private:
	lstCoef m_lst;
	int m_nTotal = 0;
};