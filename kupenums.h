#ifndef KUP_ENUMS_H
#define KUP_ENUMS_H

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

namespace Kup
{
Q_NAMESPACE
QML_NAMED_ELEMENT(Enums)

enum BackupType {
    BupType = 0,
    RsyncType,
};
Q_ENUM_NS(BackupType)

enum ScheduleType {
    MANUAL = 0,
    INTERVAL,
    USAGE,
};
Q_ENUM_NS(ScheduleType)

enum Status {
    GOOD,
    MEDIUM,
    BAD,
    NO_STATUS,
};
Q_ENUM_NS(Status)

enum ExecutorState {
    NOT_AVAILABLE,
    WAITING_FOR_FIRST_BACKUP,
    WAITING_FOR_BACKUP_AGAIN,
    BACKUP_RUNNING,
    WAITING_FOR_MANUAL_BACKUP,
    INTEGRITY_TESTING,
    REPAIRING,
};
Q_ENUM_NS(ExecutorState)
}

#endif // KUP_ENUMS_H
