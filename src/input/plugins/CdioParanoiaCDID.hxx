#pragma once

#include <string>

struct CdioUri {
	char device[64];
	int track;
};

class CDIODiscID
{
public:
	static std::string getCurrentCDId (std::string device);

private:
	static int cddb_sum(int n);
};


