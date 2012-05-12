#include "divisioninfo.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

//Constructor
DivisionInfo::DivisionInfo(QWidget *parent, Qt::WFlags flags) : QWidget(parent, flags)
{
	//Sets up the user interface
	ui.setupUi(this);
}

//Destructor
DivisionInfo::~DivisionInfo()
{

}

//Opens media.pk2
void DivisionInfo::Open()
{
	//File dialog
	QFileDialog dlg(this);
	dlg.setFileMode(QFileDialog::Directory);

	//Execute
	if(dlg.exec())
	{
		//Get selected folders
		QStringList folders = dlg.selectedFiles();
		if(!folders.empty())
		{
			path = folders[0];
			ui.SilkroadPath->setText(path);
		}
	}
}

//Adds a division
void DivisionInfo::AddDivision()
{

}

//Adds a gateway server
void DivisionInfo::AddGateway()
{

}

//Removes a division
void DivisionInfo::RemoveDivision()
{

}

//Removes a gateway server
void DivisionInfo::RemoveGateway()
{

}