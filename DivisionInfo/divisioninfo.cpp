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
	//Close the PK2 first
	Close();

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
			path = folders[0] + "/Media.pk2";
			ui.SilkroadPath->setText(path);

			if(pk2reader.Open(path.toAscii().data()))
			{

			}
			else
			{
				QMessageBox::critical(this, "Error", pk2reader.GetError().c_str());
			}
		}
	}
}

//Closes an open PK2 file
void DivisionInfo::Close()
{
	pk2reader.Close();
	pk2writer.Close();
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