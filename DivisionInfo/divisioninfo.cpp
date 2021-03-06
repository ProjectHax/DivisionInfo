#include "divisioninfo.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QTextStream>
#include <QStringList>

#include "Stream/stream_utility.h"
#include <boost/lexical_cast.hpp>
#include <sstream>

#if _WIN32
	#include "windows.h"
#endif

//Constructor
DivisionInfo::DivisionInfo(QWidget *parent, Qt::WFlags flags) : QWidget(parent, flags)
{
	//Sets up the user interface
	ui.setupUi(this);

	//Connect the menu bar items
	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(Open()));
	connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(Save()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(Exit()));
	connect(ui.actionExtract_PK2_Key, SIGNAL(triggered()), this, SLOT(ExtractPK2Key()));

	//Connect export menu items
	connect(ui.actionSV_T, SIGNAL(triggered()), this, SLOT(ExportSVT()));
	connect(ui.actionDIVISIONINFO_TXT, SIGNAL(triggered()), this, SLOT(ExportDivisionInfo()));
	connect(ui.actionGATEPORT_TXT, SIGNAL(triggered()), this, SLOT(ExportGatePort()));
	
	//Context menu
	connect(ui.lstDivisions, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
	connect(ui.lstGateways, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));

	//PK2 keys
	keys.push_back("169841");						//iSRO/SilkroadR/MySRO/vSRO
	keys.push_back("\x32\x30\x30\x39\xC4\xEA");		//ZSZC/SWSRO

	QString path = QCoreApplication::applicationFilePath();
	path = path.mid(0, path.lastIndexOf("/")) + "/keys.txt";

	QFile file(path);
	if(file.open(QIODevice::ReadOnly))
	{
		QTextStream in(&file);
		QString line = in.readLine();
		
		while(!line.isNull())
		{
			std::string temp(line.toAscii().data());

			//Split the line to see if it is a hex string
			QStringList bytes = line.split("\\x");

			if(bytes.size() > 1)
			{
				temp.clear();

				for(int x = 1; x < bytes.size(); ++x)
				{
					//Convert to hex
					uint32_t hex = 0;
					std::stringstream ss;
					ss << std::hex << bytes[x].toAscii().data();
					ss >> hex;

					//Append
					temp += (uint8_t)hex;
				}
			}

			//Add the key to the list
			keys.push_back(temp);

			//Next
			line = in.readLine();
		}
	}
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
			
			bool open = false;
			for(size_t x = 0; x < keys.size(); ++x)
			{
				pk2reader.SetDecryptionKey((char*)keys[x].c_str(), keys[x].size());

				//Attempt to open the PK2 file
				if(pk2reader.Open(std::string(path.toAscii().data()) + "/Media.pk2"))
				{
					open = true;
					break;
				}
			}

			if(open)
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

		/*char * keys[2] =
		{
			"169841",						//iSRO/SilkroadR/MySRO/vSRO
			"\x32\x30\x30\x39\xC4\xEA"		//ZSZC/SWSRO
		};*/

		bool open = false;
		for(size_t x = 0; x < keys.size(); ++x)
		{
			//Attempt to open the PK2 file
			if(pk2writer.Open(std::string(path.toAscii().data()) + "/Media.pk2", (void*)keys[x].c_str(), 6))
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
		StreamUtility version = CreateSVT();
		if(version.GetStreamSize())
		{
			//Import the file
			if(!pk2writer.ImportFile("SV.T", (void*)version.GetStreamPtr(), version.GetStreamSize()))
			{
				QMessageBox::critical(this, "Error", "There was a problem saving the version file (Media.pk2/SV.T)");
				error = true;
			}
		}

		//Save division info and gateway port
		StreamUtility division = CreateDivisionInfo();
		if(division.GetStreamSize())
		{
			//Import the file
			if(!pk2writer.ImportFile("DIVISIONINFO.TXT", (void*)division.GetStreamPtr(), division.GetStreamSize()))
			{
				QMessageBox::critical(this, "Error", "There was a problem saving the division info (Media.pk2/DIVISIONINFO.TXT)");
				error = true;
			}
		}

		StreamUtility gateport = CreateGatePort();
		if(gateport.GetStreamSize())
		{
			//Import the file
			if(!pk2writer.ImportFile("GATEPORT.TXT", (void*)gateport.GetStreamPtr(), gateport.GetStreamSize()))
			{
				QMessageBox::critical(this, "Error", "There was a problem saving the division info (Media.pk2/GATEPORT.TXT)");
				error = true;
			}
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
	try
	{
		PK2Entry entry = {0};

		//Division info
		pk2reader.GetEntry("DIVISIONINFO.TXT", entry);
		StreamUtility r(pk2reader.Extract(entry), entry.size);

#if _DEBUG
		printf("%s\n\n", DumpToString(r).c_str());
#endif

		//Locale and division count
		uint8_t locale = r.Read<uint8_t>();
		uint8_t divisons = r.Read<uint8_t>();

		//Set locale in GUI
		ui.Locale->setText(QString("%0").arg(locale));

		//Iterate divisions
		for(uint8_t x = 0; x < divisons; ++x)
		{
			uint32_t len = r.Read<uint32_t>();
			if(len >= 255)
				throw std::exception("Division name length is too large");

			//Division name
			std::string name = r.Read_Ascii(len + 1);
			name.resize(name.length() - 1);	//Remove null terminator

			//Delete null bytes (stupid people that didn't change the length)
			size_t nullcount = 0;
			for(size_t z = 0; z < name.length(); ++z)
			{
				if(name[z] == '\0')
					nullcount++;
			}
			name.resize(name.length() - nullcount);

			//Number of servers in this division
			uint8_t servers = r.Read<uint8_t>();

			std::vector<std::string> divisiontemp;
			for(uint8_t i = 0; i < servers; ++i)
			{
				//Gateway hostname or IP
				uint32_t len = r.Read<uint32_t>();
				if(len >= 255)
					throw std::exception("Hostname/IP length is too large");

				std::string gateway = r.Read_Ascii(len + 1);
				gateway.resize(gateway.length() - 1);	//Remove null terminator

				//Delete null bytes (stupid people that didn't change the length)
				nullcount = 0;
				for(size_t z = 0; z < gateway.length(); ++z)
				{
					if(gateway[z] == '\0')
						nullcount++;
				}
			
				gateway.resize(gateway.length() - nullcount);
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
	}
	catch(std::exception & e)
	{
		QMessageBox::critical(this, "Error", e.what());
		return false;
	}

	//Success
	return true;
}

//Create the version file (SV.T)
StreamUtility DivisionInfo::CreateSVT()
{
	StreamUtility w;
	std::string version = ui.Version->text().toAscii().data();

	if(version.empty() || version == "0")
	{
		QMessageBox::critical(this, "Error", "Version is empty or null");
		return w;
	}

	version.resize(8);

	Blowfish bf;
	bf.Initialize("SILKROADVERSION", 8);
	bf.Encode(&version[0], 4, &version[0], 8);

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

	return w;
}

//Creates division info (DIVISIONINFO.TXT)
StreamUtility DivisionInfo::CreateDivisionInfo()
{
	StreamUtility w;

	//Retrieve locale and port from the GUI
	std::string locale = ui.Locale->text().toAscii().data();

	//Locale error checking
	if(locale.empty() || locale == "0")
	{
		QMessageBox::critical(this, "Error", "Locale is empty or null");
		return w;
	}

	//Create DIVISIONINFO.TXT
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

	return w;
}

//Creates gateway port (GATEPORT.TXT)
StreamUtility DivisionInfo::CreateGatePort()
{
	StreamUtility w;
	std::string port = ui.Port->text().toAscii().data();

	//Port error checking
	if(port.empty() || port == "0")
	{
		QMessageBox::critical(this, "Error", "Port is empty or null");
		return w;
	}

	w.Write_Ascii(port);

	//Extra null bytes
	uint8_t extra = (8 - port.length());
	for(uint8_t x = 0; x < extra; ++x)
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

	return w;
}

//Context menu
void DivisionInfo::ContextMenu(const QPoint & pos)
{
	QMenu menu;
	menu.addAction("Copy");

	if(ui.lstDivisions->hasFocus())
	{
		//Find where the user clicked
		QPoint global = ui.lstDivisions->mapToGlobal(pos);
		QModelIndex t = ui.lstDivisions->indexAt(pos);
		
		//Show menu
		QAction* item = menu.exec(global);
		
		//Make sure an item was selected
		if(item)
		{
			QList<QListWidgetItem*> items = ui.lstDivisions->selectedItems();
			if(items.size())
			{
				//Copy text to clipboard
				QClipboard* clipboard = QApplication::clipboard();
				if(clipboard) clipboard->setText(items[0]->text());
			}
		}
	}
	else if(ui.lstGateways->hasFocus())
	{
		//Find where the user clicked
		QPoint global = ui.lstGateways->mapToGlobal(pos);
		QModelIndex t = ui.lstGateways->indexAt(pos);

		//Show menu
		QAction* item = menu.exec(global);

		//Make sure an item was selected
		if(item)
		{
			QList<QListWidgetItem*> items = ui.lstGateways->selectedItems();
			if(items.size())
			{
				//Copy text to clipboard
				QClipboard* clipboard = QApplication::clipboard();
				if(clipboard) clipboard->setText(items[0]->text());
			}
		}
	}
}

//Exports SV.T
void DivisionInfo::ExportSVT()
{
	StreamUtility w = CreateSVT();
	if(w.GetStreamSize())
	{
		//Ask the user where to save the file
		QString name = QFileDialog::getSaveFileName(this, "Export", QDir::currentPath(), "Text Document (*.txt)");
		if(!name.isEmpty())
		{
			//Open the file for writing
			QFile file(name);
			if(file.open(QIODevice::WriteOnly))
			{
				//Write the data and close the file
				file.write((const char*)w.GetStreamPtr(), w.GetStreamSize());
				file.flush();
				file.close();

				QMessageBox::information(this, "Export Success", QString("SV.T has been saved to:\n%0").arg(name));
			}
			else
			{
				QMessageBox::critical(this, "Export Error", QString("Could not open %0 for writing.").arg(name));
			}
		}
	}
}

//Exports DIVISIONINFO.TXT
void DivisionInfo::ExportDivisionInfo()
{
	StreamUtility w = CreateDivisionInfo();
	if(w.GetStreamSize())
	{
		//Ask the user where to save the file
		QString name = QFileDialog::getSaveFileName(this, "Export", QDir::currentPath(), "Text Document (*.txt)");
		if(!name.isEmpty())
		{
			//Open the file for writing
			QFile file(name);
			if(file.open(QIODevice::WriteOnly))
			{
				//Write the data and close the file
				file.write((const char*)w.GetStreamPtr(), w.GetStreamSize());
				file.flush();
				file.close();

				QMessageBox::information(this, "Export Success", QString("DIVISIONINFO.TXT has been saved to:\n%0").arg(name));
			}
			else
			{
				QMessageBox::critical(this, "Export Error", QString("Could not open %0 for writing.").arg(name));
			}
		}
	}
}

//Exports GATEPORT.TXT
void DivisionInfo::ExportGatePort()
{
	StreamUtility w = CreateGatePort();
	if(w.GetStreamSize())
	{
		//Ask the user where to save the file
		QString name = QFileDialog::getSaveFileName(this, "Export", QDir::currentPath(), "Text Document (*.txt)");
		if(!name.isEmpty())
		{
			//Open the file for writing
			QFile file(name);
			if(file.open(QIODevice::WriteOnly))
			{
				//Write the data and close the file
				file.write((const char*)w.GetStreamPtr(), w.GetStreamSize());
				file.flush();
				file.close();

				QMessageBox::information(this, "Export Success", QString("DIVISIONINFO.TXT has been saved to:\n%0").arg(name));
			}
			else
			{
				QMessageBox::critical(this, "Export Error", QString("Could not open %0 for writing.").arg(name));
			}
		}
	}
}

//Extracts the PK2 key from a Silkroad client
void DivisionInfo::ExtractPK2Key()
{
#if _WIN32
	QMessageBox::warning(this, "Warning", "Extracting the PK2 key/file names will only work with stock vSRO clients that are packed but have the same memory addresses.\n\nIf it does not work correctly the first time then you will need to run it again since the client needs to be loaded into memory before it can be read.");

	QString file = QFileDialog::getOpenFileName(this, "sro_client.exe", "", "Executable files (*.exe)");
	if(!file.isEmpty())
	{
		file = file.replace("/", "\\") + " 0 /22 0 0";

		STARTUPINFOA sInfo = {0};
		PROCESS_INFORMATION pInfo = {0};

		if(!CreateProcessA(0, file.toAscii().data(), 0, 0, FALSE, 0, 0, 0, &sInfo, &pInfo))
		{
			QMessageBox::critical(this, "Error", QString("Unable to start %0").arg(file));
		}
		else
		{
			//Wait for the client to load/uncompress
			for(int x = 0; x < 500; ++x)
			{
				QCoreApplication::processEvents();
				Sleep(10);
			}

			QString hex_key;
			char out[16] = {0};
			char key[16] = {0};
			char temp[8] = {0};

			//PK2 key
			ReadProcessMemory(pInfo.hProcess, (void*)0x0E19450, key, 8, NULL);

			for(uint8_t x = 0; x < 8; ++x)
			{
				sprintf_s(temp, "%.2X", key[x]);

				if(temp[0] == '0' && temp[1] == '0')
					break;

				hex_key += QString(temp) + " ";
			}

			QString str = QString("PK2 Key:\n[%0]\n[%1]\n\n").arg(key).arg(hex_key.mid(0, hex_key.length() - 1));

			//Media
			ReadProcessMemory(pInfo.hProcess, (void*)0x0E19458, out, 15, NULL);
			str += QString("Media.pk2 -> %0\n").arg(out);

			//Data
			ReadProcessMemory(pInfo.hProcess, (void*)0x0DD2D94, out, 15, NULL);
			str += QString("Data.pk2 -> %0\n").arg(out);			
			
			//Particles
			ReadProcessMemory(pInfo.hProcess, (void*)0x0DD2D78, out, 15, NULL);
			str += QString("Particles.pk2 -> %0\n").arg(out);
			
			//Music
			ReadProcessMemory(pInfo.hProcess, (void*)0x0DD2D88, out, 15, NULL);
			str += QString("Music.pk2 -> %0\n").arg(out);

			//Map
			ReadProcessMemory(pInfo.hProcess, (void*)0x0DD2DA0, out, 15, NULL);
			str += QString("Map.pk2 -> %0").arg(out);
			
			//Kill the process
			TerminateProcess(pInfo.hProcess, 0);
			CloseHandle(pInfo.hThread);
			CloseHandle(pInfo.hProcess);

			QMessageBox::information(this, "PK2 Key Extractor", str);
		}
	}
#endif
}