#pragma once

#ifndef DIVISIONINFO_H
#define DIVISIONINFO_H

#include <QtGui/QWidget>
#include "ui_divisioninfo.h"

#include "PK2/PK2Reader.h"
#include "PK2/PK2Writer.h"

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

public:

	//Constructor
	DivisionInfo(QWidget *parent = 0, Qt::WFlags flags = 0);

	//Destructor
	~DivisionInfo();

private slots:
	
	//Opens media.pk2
	void Open();

	//Adds a division
	void AddDivision();

	//Adds a gateway server
	void AddGateway();

	//Removes a division
	void RemoveDivision();

	//Removes a gateway server
	void RemoveGateway();
};

#endif