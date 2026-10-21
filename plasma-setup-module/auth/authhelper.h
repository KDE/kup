// SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
//
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>

using namespace KAuth;

class AuthHelper : public QObject
{
    Q_OBJECT
public:
    AuthHelper(QObject *pParent = nullptr);

public Q_SLOTS:
    ActionReply restorebackup(const QVariantMap &pArgs);

private:
    ActionReply makeErrorReply(const QString &pErrorDescription);
};
