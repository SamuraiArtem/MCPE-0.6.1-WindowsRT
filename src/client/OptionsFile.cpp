#include "OptionsFile.h"
#include <stdio.h>
#include <string.h>

OptionsFile::OptionsFile() {
#ifdef __APPLE__
	settingsPath = "./Documents/options.txt";
#elif defined(ANDROID)
	settingsPath = "options.txt";
#else
	settingsPath = "options.txt";
#endif
}

void OptionsFile::save(const StringVector& settings) {
	FILE* pFile = fopen(settingsPath.c_str(), "w");
	if(pFile != NULL) {
		for(StringVector::const_iterator it = settings.begin(); it != settings.end(); ++it) {
			fprintf(pFile, "%s\n", it->c_str());
		}
		fclose(pFile);
	}
}

StringVector OptionsFile::getOptionStrings() {
	StringVector returnVector;
	// Must be opened for READING. With "w" the options file was wiped
	// every time the settings were loaded.
	FILE* pFile = fopen(settingsPath.c_str(), "r");
	if(pFile != NULL) {
		char lineBuff[256];
		while(fgets(lineBuff, sizeof lineBuff, pFile)) {
			size_t len = strlen(lineBuff);
			while(len > 0 && (lineBuff[len-1] == '\n' || lineBuff[len-1] == '\r'))
				lineBuff[--len] = 0;
			if(len > 2)
				returnVector.push_back(std::string(lineBuff, len));
		}
		fclose(pFile);
	}
	return returnVector;
}
