#pragma once

#ifndef DIVISIONINFO_H
#define DIVISIONINFO_H

#include <QtGui/QWidget>
#include "ui_divisioninfo.h"

#include "PK2/PK2Reader.h"
#include "PK2/PK2Writer.h"
#include "Stream/stream_utility.h"

#include <map>
#include <string>
#include <vector>

class DivisionInfo : public QWidget
{
	Q_OBJECT

private:

	//User interface
	Ui::DivisionInfoClass ui;

	//Path to media.pk2
	QString path;
	
	//PK2 API's
	PK2Reader pk2reader;
	PK2Writer pk2writer;

	//Closes an open PK2 file
	void Close();

	//Parses Silkroad version
	bool LoadVersion();

	//Parses division info
	bool LoadDivisionInfo();

	//Create the version file (SV.T)
	StreamUtility CreateSVT();

	//Creates division info (DIVISIONINFO.TXT)
	StreamUtility CreateDivisionInfo();

	//Creates gateway port (GATEPORT.TXT)
	StreamUtility CreateGatePort();

	//PK2 keys
	std::vector<std::string> keys;

	std::map<std::string, std::vector<std::string> > Divisions;

public:

	//Constructor
	DivisionInfo(QWidget *parent = 0, Qt::WFlags flags = 0);

	//Destructor
	~DivisionInfo();

private slots:
	
	//Opens media.pk2
	void Open();

	//Saves changes
	void Save();

	//Adds a division
	void AddDivision();

	//Adds a gateway server
	void AddGateway();

	//Removes a division
	void RemoveDivision();

	//Removes a gateway server
	void RemoveGateway();

	//Division changed
	void DivisionChanged();

	//Exits the program
	void Exit() { Close(); close(); }

	//Exports SV.T
	void ExportSVT();

	//Exports DIVISIONINFO.TXT
	void ExportDivisionInfo();

	//Exports GATEPORT.TXT
	void ExportGatePort();

	//Extracts the PK2 key from a Silkroad client
	void ExtractPK2Key();

	//Context menu
	void ContextMenu(const QPoint & pos);
};

#endif