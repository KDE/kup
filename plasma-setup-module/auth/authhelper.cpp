// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "authhelper.h"

#include <QMap>
#include <QProcess>
#include <QString>
#include <QVariantMap>

#include <grp.h>
#include <pwd.h>
#include <qdir.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

constexpr auto MIN_REGULAR_USER_UID = 1000;

KAUTH_HELPER_MAIN("org.kde.plasmasetup.restorebackup", AuthHelper)

AuthHelper::AuthHelper(QObject *pParent)
    : QObject(pParent)
{
}

ActionReply AuthHelper::restorebackup(const QVariantMap &pArgs)
{
    const QVariant lUsernameVariant = pArgs.value("username"_L1);
    const QVariant lBackupDirVariant = pArgs.value("backupDir"_L1);
    const QVariant lBackupNameVariant = pArgs.value("backupName"_L1);
    const QVariant lBackupRevisionVariant = pArgs.value("backupRevision"_L1);

    if (!lUsernameVariant.canConvert<QString>() || !lBackupDirVariant.canConvert<QString>() || !lBackupNameVariant.canConvert<QString>()
        || !lBackupRevisionVariant.canConvert<QString>()) {
        return makeErrorReply("Username or source URL argument is missing or invalid."_L1);
    }

    const QString lUsername = lUsernameVariant.toString().trimmed();
    const QString lBackupDir = lBackupDirVariant.toString().trimmed();
    const QString lBackupName = lBackupNameVariant.toString().trimmed();
    const QString lBackupRevision = lBackupRevisionVariant.toString().trimmed();

    struct passwd lPwd;
    struct passwd *lResult = nullptr;
    QByteArray lUsernameBytes = lUsername.toLocal8Bit();
    long lBufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (lBufsize <= 0) {
        lBufsize = 16384; // fallback size
    }
    QByteArray lBuf(static_cast<int>(lBufsize), 0);

    int lRet = getpwnam_r(lUsernameBytes.constData(), &lPwd, lBuf.data(), lBuf.size(), &lResult);

    if (lResult == nullptr) {
        if (lRet == 0) {
            return makeErrorReply("User does not exist: "_L1 + lUsername);
        } else {
            return makeErrorReply("System error while looking up user "_L1 + lUsername + ": error code "_L1 + QString::number(lRet));
        }
    }

    if (lPwd.pw_uid < MIN_REGULAR_USER_UID) {
        return makeErrorReply("Refusing to perform action for system user: "_L1 + lUsername);
    }

    const auto lUid = lPwd.pw_uid;
    const auto lGid = lPwd.pw_gid;

    // bup refuses to write into existing directories, so we write into a temporary folder
    // then move that with rsync to the actual home directory

    QProcess lRestoreProcess;
    lRestoreProcess.start("bup"_L1,
                          {
                              "--bup-dir"_L1,
                              lBackupDir,
                              "restore"_L1,
                              // the trailing slash is significant -- see https://bup.github.io/man/bup-restore.1.html
                              "%1/%2/home/%3/"_L1.arg(lBackupName, lBackupRevision, lUsername),
                              "--outdir"_L1,
                              "/home/%1/.plasma-setup-bup-restore-tmp"_L1.arg(lUsername),
                          });

    if (!lRestoreProcess.waitForStarted()) {
        return makeErrorReply("Failed to start bup: "_L1 + lRestoreProcess.errorString());
    }

    lRestoreProcess.waitForFinished(-1);
    if (lRestoreProcess.exitStatus() == QProcess::CrashExit || lRestoreProcess.exitCode() != 0) {
        return makeErrorReply("Bup restore failed: "_L1 + QString::fromUtf8(lRestoreProcess.readAllStandardError()));
    }

    QProcess lMoveProcess;
    lMoveProcess.start("rsync"_L1,
                       {
                           "--archive"_L1,
                           "--remove-source-files"_L1,
                           "/home/%1/.plasma-setup-bup-restore-tmp/"_L1.arg(lUsername),
                           "/home/%1/"_L1.arg(lUsername),
                       });

    if (!lMoveProcess.waitForStarted()) {
        return makeErrorReply("Failed to start rsync: "_L1 + lMoveProcess.errorString());
    }

    lMoveProcess.waitForFinished(-1);
    if (lMoveProcess.exitStatus() == QProcess::CrashExit || lMoveProcess.exitCode() != 0) {
        return makeErrorReply("Rsync move failed: "_L1 + QString::fromUtf8(lMoveProcess.readAllStandardError()));
    }

    // at this point, it will only have empty subdirectories all the way down
    // but the subdirectories are there, so we can't just use rmdir...
    QDir("/home/%1/.plasma-setup-bup-restore-tmp"_L1.arg(lUsername)).removeRecursively();

    // home permissions can become too permissive after this process, so reset them
    QFile::setPermissions("/home/%1"_L1.arg(lUsername), QFile::Permission::ReadOwner | QFile::Permission::WriteOwner | QFile::Permission::ExeOwner);
    ::chown("/home/%1"_L1.arg(lUsername).toUtf8().constData(), lUid, lGid);

    return ActionReply::SuccessReply();
}

ActionReply AuthHelper::makeErrorReply(const QString &pErrorDescription)
{
    ActionReply lReply = ActionReply::HelperErrorReply();
    lReply.setErrorDescription(pErrorDescription);
    return lReply;
}
