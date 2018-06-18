#include "aboutdialog.h"

#include <iostream>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QPixmap>

AboutDialog::AboutDialog(QWidget* parent)
    : Dialog(tr("About"), parent, Dialog::NoStatusLine, "", Dialog::NoExtraButtons)
{
    auto* layout = addLayout<QVBoxLayout>();

    QLabel* imageLabel = new QLabel();
    imageLabel->setPixmap(QPixmap(":/icon.png"));
    imageLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(imageLabel);

    QLabel* labelString = new QLabel("<h1>" + QApplication::applicationDisplayName() + " v"
                     + QApplication::applicationVersion() + "</h1>");
    labelString->setAlignment(Qt::AlignHCenter);
    layout->addWidget(labelString);

    QLabel* linkLabel = new QLabel("<a href=\"https://github.com/QMatrixClient/Quaternion\">Website</a>");
    linkLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(linkLabel);

    layout->addWidget(new QLabel("Quaternion Copyright (C) 2018 Kitsune Ral et. al"));

    if (!GIT_SHA1.empty()) {
        layout->addWidget(new QLabel(tr("Built from Git, commit SHA:")));
        layout->addWidget(new QLabel(GIT_SHA1.c_str()));
    }

    // Remove the default buttons
    buttons->clear();
    buttons->addButton(QDialogButtonBox::Close);
}

AboutDialog::~AboutDialog() = default;