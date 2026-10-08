#ifndef TST_MEDIALOCATOR_H
#define TST_MEDIALOCATOR_H

#include <QObject>

class MediaLocatorTest : public QObject
{
    Q_OBJECT

private slots:
    void networkStreamsAreDetected();
    void supportedMediaFilesAreDetected();
    void emptyLocatorIsRejected();
    void networkStreamBypassesFormatRequirement();
    void unsupportedFormatIsRejectedBeforeExistenceCheck();
    void unsupportedFormatIsAcceptedWhenNotRequired();
    void missingFileIsReported();
    void existingFileIsNormalizedToAbsolutePath();
};

#endif // TST_MEDIALOCATOR_H
