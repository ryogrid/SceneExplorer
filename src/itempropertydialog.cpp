#include "itempropertydialog.h"
#include "ui_itempropertydialog.h"

#include "helper.h"
#include "sql.h"


ItemPropertyDialog::ItemPropertyDialog(QWidget *parent) :
    QDialog(parent,GetDefaultDialogFlags()),
    ui(new Ui::ItemPropertyDialog)
{
    ui->setupUi(this);
    ui->sbOpenCount->setMinimum(0);
    ui->sbOpenCount->setMaximum(std::numeric_limits<int>::max());

    setReadOnly(*ui->lineID);
    setReadOnly(*ui->lineFile);
}

void ItemPropertyDialog::showEvent(QShowEvent *ev)
{
    QDialog::showEvent(ev);

    // Variable to Dialog
    updateData(true);
}
void ItemPropertyDialog::updateData(bool bUpdate)
{
    if(bUpdate)
    {
        // must be set
        Q_ASSERT(id_ != -1);


        ui->lineID->setText(QString::number(id_));
        ui->lineFile->setText(file_);
    }

    if(bUpdate)
        ui->sbOpenCount->setValue((int)openCount_);
    else
        openCount_ = ui->sbOpenCount->value();
}
ItemPropertyDialog::~ItemPropertyDialog()
{
    delete ui;
}

void ItemPropertyDialog::on_buttonBox_accepted()
{
    // Dialog to variable
    updateData(false);
}
