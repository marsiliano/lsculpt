#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include <qdatetime.h>

AboutDialog::AboutDialog(const char *version, QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Change text box background color to same as parent (ie. transparent)
    QColor bg = this->palette().color(QPalette::Window);
    QPalette p = ui->textEdit->palette();
    p.setColor(QPalette::Base, bg);
    ui->textEdit->setPalette(p);

    ui->titleLabel->setText(QString("LSculpt %1").arg(version));

    QDateTime dt = QDateTime::fromString(QString("%1 %2").arg(__DATE__, __TIME__),
                                         QString("MMM dd yyyy hh:mm:ss"));
    QString aboutmessage = "";

    aboutmessage.append("Copyright 2006 - 2010 Bram Lambrecht. All rights reserved.");
    aboutmessage.append("\n\n");
    aboutmessage.append("LSculpt developed by Bram Lambrecht and Remi Gagne. ");
    aboutmessage.append("LEGO® is a registered trademark of the LEGO Group, which does not "
                        "sponsor, endorse, or authorize this software.");
    aboutmessage.append("\n\n");
    aboutmessage.append("http://lego.bldesign.org/LSculpt/");
    aboutmessage.append("\n\n");
    aboutmessage.append("Adapted by Massimiliano Pavesi (aka MaxCodeLaundry): updated to the "
                        "latest LDraw specification, ported to macOS, and migrated to Qt6.");

    ui->textEdit->setText(aboutmessage);
    ui->textBuild->setText(
        QString("Built on %1 at %2").arg(dt.toString("d MMM yyyy"), dt.toString("hh:mm:ss")));
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
