/*
 * SPDX-FileCopyrightText: 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "networkconfigdialog.h"

#include <Quotient/networksettings.h>

#include <QtGui/QIntValidator>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGridLayout>

NetworkConfigDialog::NetworkConfigDialog(QWidget* parent)
    : Dialog(tr("Network proxy settings"), parent)
    , useProxyBox(new QGroupBox(tr("&Override system defaults"), this))
    , proxyTypeGroup(new QButtonGroup(this))
    , proxyHostName(new QLineEdit(this))
    , proxyPort(new QSpinBox(this))
    , proxyUserName(new QLineEdit(this))
{
    // Create and configure all the controls

    useProxyBox->setCheckable(true);
    useProxyBox->setChecked(false);
    connect(useProxyBox, &QGroupBox::toggled,
            this, &NetworkConfigDialog::maybeDisableControls);

    auto noProxyButton = new QRadioButton(tr("&No proxy"));
    noProxyButton->setChecked(true);
    proxyTypeGroup->addButton(noProxyButton,
                              QNetworkProxy::NoProxy);
    proxyTypeGroup->addButton(new QRadioButton(tr("&HTTP(S) proxy")),
                              QNetworkProxy::HttpProxy);
    proxyTypeGroup->addButton(new QRadioButton(tr("&SOCKS5 proxy")),
                              QNetworkProxy::Socks5Proxy);
    connect(proxyTypeGroup, &QButtonGroup::idToggled, this,
            &NetworkConfigDialog::maybeDisableControls);

    maybeDisableControls();

    auto hostLabel = makeBuddyLabel(tr("Host"), proxyHostName);
    auto portLabel = makeBuddyLabel(tr("Port"), proxyPort);
    auto userLabel = makeBuddyLabel(tr("User name"), proxyUserName);

    proxyPort->setRange(0, 65535);
    proxyPort->setSpecialValueText(QStringLiteral(" "));

    // Now laying all this out

    auto proxyTypeLayout = new QGridLayout;
    auto radios = proxyTypeGroup->buttons();
    proxyTypeLayout->addWidget(radios[0], 0, 0);
    for (int i = 2; i <= radios.size(); ++i) // Consider i as 1-based index
        proxyTypeLayout->addWidget(radios[i - 1], i / 2, i % 2);

    auto hostPortLayout = new QHBoxLayout;
    for (auto l: { hostLabel, portLabel })
    {
        hostPortLayout->addWidget(l);
        hostPortLayout->addWidget(l->buddy());
    }
    auto userNameLayout = new QHBoxLayout;
    userNameLayout->addWidget(userLabel);
    userNameLayout->addWidget(userLabel->buddy());

    auto proxySettingsLayout = new QVBoxLayout(useProxyBox);
    proxySettingsLayout->addLayout(proxyTypeLayout);
    proxySettingsLayout->addLayout(hostPortLayout);
    proxySettingsLayout->addLayout(userNameLayout);

    addWidget(useProxyBox);
}

NetworkConfigDialog::~NetworkConfigDialog() = default;

void NetworkConfigDialog::maybeDisableControls()
{
    if (useProxyBox->isChecked())
    {
        bool disable = proxyTypeGroup->checkedId() == -1 ||
            proxyTypeGroup->checkedId() == QNetworkProxy::NoProxy;
        proxyHostName->setDisabled(disable);
        proxyPort->setDisabled(disable);
        proxyUserName->setDisabled(disable);
    }
}

void NetworkConfigDialog::apply()
{
    Quotient::NetworkSettings networkSettings;

    auto proxyType = useProxyBox->isChecked() ?
                QNetworkProxy::ProxyType(proxyTypeGroup->checkedId()) :
                QNetworkProxy::DefaultProxy;
    networkSettings.setProxyType(proxyType);
    networkSettings.setProxyHostName(proxyHostName->text());
    networkSettings.setProxyPort(quint16(proxyPort->value()));
    networkSettings.setupApplicationProxy();
    // Should we do something for authentication at all?..
    accept();
}

void NetworkConfigDialog::load()
{
    Quotient::NetworkSettings networkSettings;
    auto proxyType = networkSettings.proxyType();
    if (proxyType == QNetworkProxy::DefaultProxy)
    {
        useProxyBox->setChecked(false);
    } else {
        useProxyBox->setChecked(true);
        if (auto b = proxyTypeGroup->button(proxyType))
            b->setChecked(true);
    }
    proxyHostName->setText(networkSettings.proxyHostName());
    auto port = networkSettings.proxyPort();
    if (port > 0)
        proxyPort->setValue(port);
}
