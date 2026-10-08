#ifndef AUDIOEXTRACTIONSERVICE_H
#define AUDIOEXTRACTIONSERVICE_H

#include <QString>

class AudioExtractionService final
{
public:
    bool extract(const QString &inputFile, const QString &outputFile);
};

#endif // AUDIOEXTRACTIONSERVICE_H