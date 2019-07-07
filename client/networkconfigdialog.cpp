/******************************************************************************
 * Copyright (C) 2017 Kitsune Ral <kitsune-ral@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "networkconfigdialog.h"

#include <networksettings.h>

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

QLabel* makeBuddyLabel(QString labelText, QWidget* field)
{
    auto label = new QLabel(labelText);
    label->setBuddy(field);
    return label;
}

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
    connect(proxyTypeGroup,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
            QOverload<int,bool>::of(&QButtonGroup::buttonToggled),
#else
            static_cast<void (QButtonGroup::*)(int,bool)>(&QButtonGroup::buttonToggled),
#endif
            this, &NetworkConfigDialog::maybeDisableControls);

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
