#ifndef TAG_TRACKER_H
#define TAG_TRACKER_H

#include "tag.h"

char * getTagItemString(int type, char * string);

void removeTagItemString(int type, char * string);

int getNumberOfTagItems(int type);

void printMemorySavedByTagTracker();

void sortTagTrackerInfo();

void resetVisitedFlagsInTagTracker(int type);
 
int wasVisitedInTagTracker(int type, char * str);

void visitInTagTracker(int type, char * str);

void printVisitedInTagTracker(FILE * fp, int type);

#endif
