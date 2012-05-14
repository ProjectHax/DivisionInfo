#include "divisioninfo.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

#include "Stream/stream_utility.h"
#include <boost/lexical_cast.hpp>

//Constructor
DivisionInfo::DivisionInfo(QWidget *parent, Qt::WFlags flags) : QWidget(parent, flags)
{
	//Sets up the user interface
	ui.setupUi(this);

	//Connect the menu bar items
	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(Open()));
	connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(Save()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(Exit()));
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
			path = folders[0];
			ui.SilkroadPath->setText(path);

			if(pk2reader.Open(std::string(path.toAscii().data()) + "/Media.pk2"))
			{
				//Load version (SV.T)
				if(!LoadVersion())
				{
					QMessageBox::critical(this, "Error", "Unable to parse Media.pk2/SV.T");
					Close();
					return;
				}

				//Load divisions (DIVISIONINFO.TXT and GATEPORT.TXT)
				if(!LoadDivisionInfo())
				{
					QMessageBox::critical(this, "Error", "Unable to parse Media.pk2/DIVISIONINFO.TXT");
					Close();
					return;
				}

				//Success
				ui.Locale->setEnabled(true);
				ui.Version->setEnabled(true);
				ui.Port->setEnabled(true);

				//Select the first division
				if(Divisions.size())
					ui.lstDivisions->setCurrentRow(0);

				//PK2 reader is no longer needed
				pk2reader.Close();
			}
			else
			{
				//Error occurred
				QMessageBox::critical(this, "Error", pk2reader.GetError().c_str());
				Close();
			}
		}
	}
}

//Closes an open PK2 file
void DivisionInfo::Close()
{
	//Reset classes
	pk2reader.Close();
	pk2writer.Close();
	path.clear();

	//GUI defaults
	ui.Locale->setEnabled(false);
	ui.Version->setEnabled(false);
	ui.Port->setEnabled(false);
	ui.Locale->setText("");
	ui.Version->setText("");
	ui.Port->setText("");
	ui.lstDivisions->clear();
	ui.lstGateways->clear();
	ui.SilkroadPath->setText("");

	//Clear internal data
	Divisions.clear();
}

//Saves changes
void DivisionInfo::Save()
{
	//Make sure there is a valid path
	if(!path.isEmpty())
	{
		bool error = false;

		if(!pk2writer.Initialize(std::string(path.toAscii().data()) + "/GFXFileManager.dll"))
		{
			QMessageBox::critical(this, "Error", "Failed to initialize GFXFileManager.dll");
			return;
		}

		char * keys[2] =
		{
			"169841",						//iSRO/SilkroadR/MySRO/vSRO
			"\x32\x30\x30\x39\xC4\xEA"		//ZSZC/SWSRO
		};

		bool open = false;
		for(uint8_t x = 0; x < 2; ++x)
		{
			if(pk2writer.Open(std::string(path.toAscii().data()) + "/Media.pk2", keys[x], 6))
			{
				open = true;
				break;
			}
		}

		if(!open)
		{
			QMessageBox::critical(this, "Error", pk2writer.GetError());
			pk2writer.Deinitialize();
			return;
		}

		//Save Silkroad version
		if(!SaveVersion())
		{
			QMessageBox::critical(this, "Error", "There was a problem saving the version file (Media.pk2/SV.T)");
			error = true;
		}

		//Save division info and gateway port
		if(!SaveDivisionInfo())
		{
			QMessageBox::critical(this, "Error", "There was a problem saving the division info (Media.pk2/DIVISIONINFO.TXT or Media.pk2/GATEPORT.TXT)");
			error = true;
		}
		
		//PK2 writer cleanup
		pk2writer.Close();
		pk2writer.Deinitialize();

		if(!error)
			QMessageBox::information(this, "Success", "The PK2 modifications have been saved!");
	}
	else
	{
		QMessageBox::warning(this, "Error", "No Media.pk2 file has been opened");
	}
}

//Adds a division
void DivisionInfo::AddDivision()
{
	//Make sure there is a valid path
	if(!path.isEmpty())
	{
		//Create an input dialog
		QInputDialog dlg(this);
		dlg.setLabelText("Enter a new division name:");

		//Make sure the option clicked was 'OK'
		if(dlg.exec())
		{
			//Get the value from the text box
			QString value = dlg.textValue();
			
			//Not empty
			if(!value.isEmpty())
			{
				if(Divisions.find(value.toAscii().data()) == Divisions.end())
				{
					//Add to GUI
					ui.lstDivisions->addItem(value);

					//Initialize division with an empty vector
					Divisions[value.toAscii().data()] = std::vector<std::string>();
				}
				else
					QMessageBox::critical(this, "Error", "The division name you have entered already exists");
			}
		}
	}
}

//Adds a gateway server
void DivisionInfo::AddGateway()
{
	QListWidgetItem* item = ui.lstDivisions->currentItem();

	//Make sure there is a valid path
	if(!path.isEmpty() && item && !item->text().isEmpty())
	{
		//Create an input dialog
		QInputDialog dlg(this);
		dlg.setLabelText("Enter a new gateway server IP/hostname:");

		//Make sure the option clicked was 'OK'
		if(dlg.exec())
		{
			//Get the value from the text box
			QString value = dlg.textValue();
			
			//Not empty
			if(!value.isEmpty())
			{
				//Locate the division
				std::map<std::string, std::vector<std::string> >::iterator itr = Divisions.find(item->text().toAscii().data());
				if(itr != Divisions.end())
				{
					//See if the gateway name already exists
					bool found = false;
					std::vector<std::string> & temp = itr->second;
					for(size_t x = 0; x < temp.size(); ++x)
					{
						if(temp[x] == std::string(value.toAscii().data()))
						{
							found = true;
							break;
						}
					}

					//Gateway server already exists
					if(found)
					{
						QMessageBox::critical(this, "Error", "The gateway server you have entered already exists");
					}
					else
					{
						//Add the gateway server to the GUI
						ui.lstGateways->addItem(value);

						//Add the gateway server to the internal list
						itr->second.push_back(value.toAscii().data());
					}
				}
				else
					QMessageBox::critical(this, "Error", "Unable to locate selected division");
			}
		}
	}
}

//Removes a division
void DivisionInfo::RemoveDivision()
{
	QListWidgetItem* item = ui.lstDivisions->currentItem();

	//Make sure there is a valid path
	if(!path.isEmpty() && item)
	{
		//Remove the division
		std::map<std::string, std::vector<std::string> >::iterator itr = Divisions.find(item->text().toAscii().data());
		if(itr != Divisions.end())
			Divisions.erase(itr);
		delete item;

		//Clear gateway list
		ui.lstGateways->clear();

		//Reload gateway servers
		DivisionChanged();
	}
}

//Removes a gateway server
void DivisionInfo::RemoveGateway()
{
	QListWidgetItem* div = ui.lstDivisions->currentItem();
	QListWidgetItem* item = ui.lstGateways->currentItem();

	//Make sure there is a valid path
	if(!path.isEmpty() && div && item)
	{
		std::map<std::string, std::vector<std::string> >::iterator itr = Divisions.find(div->text().toAscii().data());
		if(itr != Divisions.end())
		{
			std::vector<std::string> & temp = itr->second;
			for(size_t x = 0; x < temp.size(); ++x)
			{
				if(temp[x] == std::string(item->text().toAscii().data()))
				{
					//Remove item
					temp.erase(temp.begin() + x);
					delete item;
					break;
				}
			}

			//Reload gateway servers
			DivisionChanged();
		}
	}
}

//Division changed
void DivisionInfo::DivisionChanged()
{
	QListWidgetItem* item = ui.lstDivisions->currentItem();

	//Make sure an item was actually selected
	if(item)
	{
		//Clear gateway list
		ui.lstGateways->clear();

		//Locate the selected item in the map
		std::map<std::string, std::vector<std::string> >::iterator itr = Divisions.find(item->text().toAscii().data());
		if(itr != Divisions.end())
		{
			//Iterate gateway servers
			std::vector<std::string> & temp = itr->second;
			for(size_t x = 0; x < temp.size(); ++x)
			{
				ui.lstGateways->addItem(temp[x].c_str());
			}
		}
	}
}

//Parses Silkroad version
bool DivisionInfo::LoadVersion()
{
	PK2Entry entry = {0};

	//Find and extract the version file
	pk2reader.GetEntry("SV.T", entry);
	const char* svt = pk2reader.Extract(entry);

	//Blowfish key
	Blowfish bf;
	bf.Initialize("SILKROADVERSION", 8);

	//Number of bytes to decode
	uint32_t size = *(uint32_t*)svt;

	if(size > 8)
		return false;

	std::string version;
	version.resize(size);

	//Decode the version number
	bf.Decode(svt + 4, size, &version[0], size);

	//Update window
	ui.Version->setText(version.c_str());

	//Success
	return true;
}

//Parses division info
bool DivisionInfo::LoadDivisionInfo()
{
	PK2Entry entry = {0};

	//Division info
	pk2reader.GetEntry("DIVISIONINFO.TXT", entry);
	StreamUtility r(pk2reader.Extract(entry), entry.size);

	//Locale and division count
	uint8_t locale = r.Read<uint8_t>();
	uint8_t divisons = r.Read<uint8_t>();

	//Set locale in GUI
	ui.Locale->setText(QString("%0").arg(locale));

	//Iterate divisions
	for(uint8_t x = 0; x < divisons; ++x)
	{
		//Division name
		std::string name = r.Read_Ascii(r.Read<uint32_t>() + 1);
		name.resize(name.length() - 1);	//Remove null terminator

		//Number of servers in this division
		uint8_t servers = r.Read<uint8_t>();

		std::vector<std::string> divisiontemp;
		for(uint8_t i = 0; i < servers; ++i)
		{
			//Gateway hostname or IP
			std::string gateway = r.Read_Ascii(r.Read<uint32_t>() + 1);
			gateway.resize(gateway.length() - 1);	//Remove null terminator

			divisiontemp.push_back(gateway);
		}

		//Save the gateway servers
		Divisions[name] = divisiontemp;

		//Add the division to the list
		ui.lstDivisions->addItem(name.c_str());
	}

	//Port
	memset(&entry, 0, sizeof(PK2Entry));
	pk2reader.GetEntry("GATEPORT.TXT", entry);
	QString temp(pk2reader.Extract(entry));
	ui.Port->setText(temp);

	//Success
	return true;
}

//Creates and saves a new version file (SV.T)
bool DivisionInfo::SaveVersion()
{
	std::string version = ui.Version->text().toAscii().data();
	version.resize(8);

	Blowfish bf;
	bf.Initialize("SILKROADVERSION", 8);
	bf.Encode(&version[0], 4, &version[0], 8);

	StreamUtility w;
	w.Write<uint32_t>(8);				//Blowfish output size
	w.Write<char>(&version[0], 8);		//Version
	
	//Calculate how much padding should be appended
	uint16_t size = w.GetStreamSize();

	//Extra padding
	for(uint16_t x = 0; x < 1024 - size; ++x)
		w.Write<uint8_t>(0);

	w.SeekRead(0, Seek_Set);

#if _DEBUG
	printf("SV.T\n");
	while((w.GetReadStreamSize() - w.GetReadIndex()) > 0)
	{
		printf("%.2X ", w.Read<uint8_t>());
	}
	printf("\n\n");
	w.SeekRead(0, Seek_Set);
#endif

	return pk2writer.ImportFile("SV.T", (void*)w.GetStreamPtr(), w.GetStreamSize());
}

//Creates and saves new division info (DIVISIONINFO.TXT and GATEPORT.TXT)
bool DivisionInfo::SaveDivisionInfo()
{
	//Retrieve locale and port from the GUI
	std::string locale = ui.Locale->text().toAscii().data();
	std::string port = ui.Port->text().toAscii().data();

	//Locale error checking
	if(locale.empty() || locale == "0")
	{
		QMessageBox::critical(this, "Error", "Locale is empty or is null");
		return false;
	}

	//Port error checking
	if(port.empty() || port == "0")
	{
		QMessageBox::critical(this, "Error", "Port is empty or is null");
		return false;
	}

	//Create DIVISIONINFO.TXT
	StreamUtility w;
	w.Write<uint8_t>(boost::lexical_cast<int>(locale));			//Locale
	w.Write<uint8_t>(Divisions.size());							//Number of divisions

	for(std::map<std::string, std::vector<std::string> >::iterator itr = Divisions.begin(); itr != Divisions.end(); ++itr)
	{
		w.Write<uint32_t>(itr->first.length());					//Division name length
		w.Write_Ascii(itr->first);								//Division name
		w.Write<uint8_t>(0);									//Null terminator
		
		std::vector<std::string> & temp = itr->second;
		w.Write<uint8_t>(temp.size());							//Number of gateway servers in this division

		for(size_t x = 0; x < temp.size(); ++x)
		{
			w.Write<uint32_t>(temp[x].length());				//Gateway name length
			w.Write_Ascii(temp[x]);								//Gateway server
			w.Write<uint8_t>(0);								//Null terminator
		}
	}

	w.SeekRead(0, Seek_Set);

#if _DEBUG
	printf("DIVISIONINFO.TXT\n");
	while((w.GetReadStreamSize() - w.GetReadIndex()) > 0)
	{
		printf("%.2X ", w.Read<uint8_t>());
	}
	printf("\n\n");
	w.SeekRead(0, Seek_Set);
#endif

	if(!pk2writer.ImportFile("DIVISIONINFO.TXT", (void*)w.GetStreamPtr(), w.GetStreamSize()))
		return false;

	//Port
	w.Clear();
	w.Write_Ascii(port);

	//Extra null bytes
	for(uint8_t x = 0; x < 3; ++x)
		w.Write<uint8_t>(0);

	w.SeekRead(0, Seek_Set);

#if _DEBUG
	printf("GATEPORT.TXT\n");
	while((w.GetReadStreamSize() - w.GetReadIndex()) > 0)
	{
		printf("%.2X ", w.Read<uint8_t>());
	}
	printf("\n\n");
	w.SeekRead(0, Seek_Set);
#endif

	return pk2writer.ImportFile("GATEPORT.TXT", (void*)w.GetStreamPtr(), w.GetStreamSize());
}