#include "networkconfigdialog.h"

#include "networksettings.h"

#include <QtGui/QIntValidator>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QBoxLayout>

NetworkConfigDialog::NetworkConfigDialog(QWidget* parent)
    : QDialog(parent)
    , useProxyBox(new QGroupBox(tr("&Use proxy")))
    , proxyTypeGroup(new QButtonGroup)
    , proxyHostName(new QLineEdit)
    , proxyPort(new QSpinBox)
{
    setModal(false);
    setWindowTitle("Network proxy settings");

    // Create and configure all the controls

    useProxyBox->setCheckable(true);
    useProxyBox->setChecked(false);

    proxyTypeGroup->addButton(new QRadioButton(tr("&HTTP proxy")),
                              QNetworkProxy::HttpProxy);
    proxyTypeGroup->addButton(new QRadioButton(tr("&SOCKS5 proxy")),
                              QNetworkProxy::Socks5Proxy);

    auto hostLabel = new QLabel(tr("Host &name"));
    hostLabel->setBuddy(proxyHostName.data());
    proxyPort->setRange(0, 65535);
    proxyPort->setSpecialValueText(QStringLiteral(" "));
    auto portLabel = new QLabel(tr("&Port"));
    portLabel->setBuddy(proxyPort.data());

    auto pushButtons = new QDialogButtonBox(QDialogButtonBox::Ok|
                                            QDialogButtonBox::Cancel);
    connect( pushButtons, &QDialogButtonBox::accepted, this, [this]
    {
        applySettings();
        accept();
    });
    connect( pushButtons, &QDialogButtonBox::rejected, this, &QDialog::reject );

    // Now laying all this out

    auto proxyTypeLayout = new QHBoxLayout;
    for (auto btn: proxyTypeGroup->buttons())
        proxyTypeLayout->addWidget(btn);

    auto hostPortLayout = new QHBoxLayout;
    for (auto l: { hostLabel, portLabel })
    {
        hostPortLayout->addWidget(l);
        hostPortLayout->addWidget(l->buddy());
    }

    auto proxySettingsLayout = new QVBoxLayout;
    proxySettingsLayout->addLayout(proxyTypeLayout);
    proxySettingsLayout->addLayout(hostPortLayout);
    useProxyBox->setLayout(proxySettingsLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(useProxyBox.data());
    mainLayout->addWidget(pushButtons);
    setLayout(mainLayout);

}

NetworkConfigDialog::~NetworkConfigDialog() = default;

void NetworkConfigDialog::applySettings()
{
    QMatrixClient::NetworkSettings networkSettings;

    auto proxyType = useProxyBox->isChecked() ?
                QNetworkProxy::ProxyType(proxyTypeGroup->checkedId()) :
                QNetworkProxy::NoProxy;
    networkSettings.setProxyType(proxyType);
    networkSettings.setProxyHostName(proxyHostName->text());
    networkSettings.setProxyPort(proxyPort->value());
    QNetworkProxy::setApplicationProxy(
        { proxyType, proxyHostName->text(), quint16(proxyPort->value()) });
}

void NetworkConfigDialog::loadSettings()
{
    QMatrixClient::NetworkSettings networkSettings;
    auto proxyType = networkSettings.proxyType();
    if (proxyType == QNetworkProxy::NoProxy)
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

void NetworkConfigDialog::reactivate()
{
    if (!isVisible())
    {
        loadSettings();
        show();
    }
    raise();
    activateWindow();
}
